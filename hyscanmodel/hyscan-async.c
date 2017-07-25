/*
 * \file hyscan-async.c
 * \brief Исходный файл класса HyScanAsync, выполняющего список запросов в отдельном потоке.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 */

#include <memory.h>
#include "hyscan-async.h"

#define HYSCAN_ASYNC_IDLE                       (0)
#define HYSCAN_ASYNC_BUSY                       (1)

#define HYSCAN_ASYNC_CONTINUE                   (0)
#define HYSCAN_ASYNC_SHUTDOWN                   (1)

#define HYSCAN_ASYNC_THREAD_NAME                "hyscan-async-thread"

#define HYSCAN_ASYNC_COND_WAIT_TIMEOUT          (100 * G_TIME_SPAN_MILLISECOND)
#define HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS    (100)

/* Запрос - это команда плюс объект плюс данные. */
typedef struct
{
  HyScanAsyncCommand command; /* Команда. */
  gpointer           object;  /* Объект. */
  gpointer           data;    /* Данные. */
} HyScanQuery;

enum
{
  SIGNAL_STARTED,
  SIGNAL_COMPLETED,
  SIGNAL_LAST
};

/* Идентификаторы сигналов. */
guint hyscan_async_signals[SIGNAL_LAST] = { 0 };

struct _HyScanAsyncPrivate
{
  GList     *queries;             /* Очередь запросов. */

  GThread   *sender;              /* Поток выполнения запросов. */
  GMutex     mutex;               /* Мьютекс, для установки запроса на выполнение. */
  GCond      cond;                /* Условие приостановки потока выполнения запросов. */

  gint       busy;                /* Флаг, запрещающий выполнение новых запросов, пока не закончится выполнение текущих. */
  gint       shutdown;            /* Флаг останова потока выполнения запросов. */

  guint      timer_source_id;     /* Идентификатор таймера. */
  gboolean   queries_ready;       /* Флаг готовности запросов к выполнению. */
  gboolean   queries_completed;   /* Флаг завершения выполнения запросов. */
  gboolean   queries_result;      /* Результат выполнения запроса (TRUE - успех, FALSE - ошибка). */
};

static void     hyscan_async_object_constructed (GObject        *object);
static void     hyscan_async_object_finalize    (GObject        *object);

static gboolean hyscan_async_is_busy            (HyScanAsync    *self);
static gboolean hyscan_async_is_idle            (HyScanAsync    *self);
static void     hyscan_async_set_busy           (HyScanAsync    *self);
static void     hyscan_async_set_idle           (HyScanAsync    *self);
static void     hyscan_async_shutdown           (HyScanAsync    *self);
static gpointer hyscan_async_thread_func        (gpointer        object);
static gboolean hyscan_async_result_func        (gpointer        object);

static void     hyscan_async_query_free         (HyScanQuery    *query);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanAsync, hyscan_async, G_TYPE_OBJECT)

static void
hyscan_async_class_init (HyScanAsyncClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->constructed = hyscan_async_object_constructed;
  obj_class->finalize = hyscan_async_object_finalize;

  hyscan_async_signals[SIGNAL_STARTED] =
      g_signal_new ("started", HYSCAN_TYPE_ASYNC,
                    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  hyscan_async_signals[SIGNAL_COMPLETED] =
      g_signal_new ("completed", HYSCAN_TYPE_ASYNC,
                    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                    g_cclosure_marshal_VOID__BOOLEAN,
                    G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
hyscan_async_init (HyScanAsync *self)
{
  HyScanAsyncPrivate *priv = hyscan_async_get_instance_private (self);

  priv->sender = NULL;
  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);

  priv->busy = HYSCAN_ASYNC_IDLE;
  priv->queries_ready = FALSE;
  priv->queries_completed = FALSE;
  priv->queries = NULL;

  self->priv = priv;
}

static void
hyscan_async_object_constructed (GObject *object)
{
  HyScanAsync *async = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = async->priv;

  G_OBJECT_CLASS (hyscan_async_parent_class)->constructed (object);

  /* Поток, в котором выполняются запросы. */
  priv->shutdown = HYSCAN_ASYNC_CONTINUE;
  priv->sender = g_thread_new (HYSCAN_ASYNC_THREAD_NAME, hyscan_async_thread_func, async);
}

static void
hyscan_async_object_finalize (GObject *object)
{
  HyScanAsync *self = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = self->priv;

  hyscan_async_shutdown (self);

  if (priv->timer_source_id)
    g_source_remove (priv->timer_source_id);
  
  g_mutex_clear (&priv->mutex);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (hyscan_async_parent_class)->finalize (object);
}

/* Проверяет, что запросы выполняются, т.е. поток ЗАНЯТ выполнением запросов. */
static gboolean
hyscan_async_is_busy (HyScanAsync *self)
{
  return self->priv->busy == HYSCAN_ASYNC_BUSY;
}

/* Проверяет, что запросы не выполняются, т.е. поток ОЖИДАЕТ выполнения запросов. */
static gboolean
hyscan_async_is_idle (HyScanAsync *self)
{
  return self->priv->busy == HYSCAN_ASYNC_IDLE;
}

/* Перевод в режим выполнения запроса. */
static void
hyscan_async_set_busy (HyScanAsync *self)
{
  if (hyscan_async_is_idle (self))
    {
      self->priv->busy = HYSCAN_ASYNC_BUSY;
      g_signal_emit (self, hyscan_async_signals[SIGNAL_STARTED], 0);
    }
}

/* Перевод в режим ожидания запроса. */
static void
hyscan_async_set_idle (HyScanAsync *self)
{
  if (hyscan_async_is_busy (self))
    {
      self->priv->busy = HYSCAN_ASYNC_IDLE;
      g_signal_emit (self, hyscan_async_signals[SIGNAL_COMPLETED], 0,
                     self->priv->queries_result);
    }
}

/* Завершает поток отправки и выключает таймер опроса статуса. */
static void
hyscan_async_shutdown (HyScanAsync *self)
{
  HyScanAsyncPrivate *priv = self->priv;

  g_atomic_int_set (&priv->shutdown, HYSCAN_ASYNC_SHUTDOWN);
  g_cond_signal (&priv->cond);
  g_thread_join (priv->sender);
}

/* Функция выполняет запросы. */
static gpointer
hyscan_async_thread_func (gpointer object)
{
  HyScanAsync *self;
  HyScanAsyncPrivate *priv;

  self = HYSCAN_ASYNC (object);
  priv = self->priv;

  while (g_atomic_int_get (&priv->shutdown) == HYSCAN_ASYNC_CONTINUE)
    {
      g_mutex_lock (&priv->mutex);

      if (priv->queries_completed || !priv->queries_ready)
        {
          gint64 end_time = g_get_monotonic_time () + HYSCAN_ASYNC_COND_WAIT_TIMEOUT;
          g_cond_wait_until (&priv->cond, &priv->mutex, end_time);
        }
      else
        {
          GList *query_item = priv->queries;
          priv->queries_result = TRUE;

          do
            {
              HyScanQuery *query = (HyScanQuery *) query_item->data;
              /* Если одна из команд завершилась с ошибкой, остальные команды не выполняются. */
              if (!(*query->command) (query->object, query->data))
                {
                  priv->queries_result = FALSE;
                  break;
                }
            }
          while (NULL != (query_item = g_list_next (query_item)));

          priv->queries_completed = TRUE;
        }

      g_mutex_unlock (&priv->mutex);
    }

  return NULL;
}

/* Функция проверяет результат выполнения запросов. */
static gboolean
hyscan_async_result_func (gpointer object)
{
  HyScanAsync *self = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = self->priv;

  if (g_mutex_trylock (&priv->mutex))
    {
      if (priv->queries_completed)
        {
          g_list_free_full (priv->queries, (GDestroyNotify) hyscan_async_query_free);
          priv->queries = NULL;
          priv->queries_ready = FALSE;
          priv->queries_completed = FALSE;

          g_mutex_unlock (&priv->mutex);

          hyscan_async_set_idle (self);

          priv->timer_source_id = 0;

          return G_SOURCE_REMOVE;
        }

        g_mutex_unlock (&priv->mutex);
    }

  return G_SOURCE_CONTINUE;
}

/* Освобождает память, распределенную под запрос. */
static void
hyscan_async_query_free (HyScanQuery *query)
{
  if (query != NULL)
    {
      g_free (query->data);
      g_free (query);
    }
}

/* Добавляет запрос в список. */
gboolean
hyscan_async_append_query (HyScanAsync        *async,
                           HyScanAsyncCommand  command,
                           gpointer            object,
                           gconstpointer       data,
                           gsize               data_size)
{
  HyScanQuery *query;

  g_return_val_if_fail (HYSCAN_IS_ASYNC (async), FALSE);

  if (hyscan_async_is_busy (async) || (command == NULL))
    return FALSE;

  query = g_new0 (HyScanQuery, 1);
  query->command = command;
  query->object = object;

  if (data != NULL && data_size)
    {
      query->data = g_malloc0 (data_size);
      memcpy (query->data, data, data_size);
    }

  async->priv->queries = g_list_append (async->priv->queries, (gpointer) query);

  return TRUE;
}

/* Запускает выполнение списка запросов в отдельном потоке. */
gboolean
hyscan_async_execute (HyScanAsync *async)
{
  HyScanAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_ASYNC (async), FALSE);

  priv = async->priv;

  if (hyscan_async_is_busy (async) || (priv->queries == NULL))
    return FALSE;

  hyscan_async_set_busy (async);

  g_mutex_lock (&priv->mutex);

  priv->queries_ready = TRUE;
  priv->queries_completed = FALSE;

  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  /* Проверка результата выполнения запросов по таймауту. */
  priv->timer_source_id = g_timeout_add (HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS,
                                         hyscan_async_result_func, async);

  return TRUE;
}

/* Создаёт объект HyScanAsync. */
HyScanAsync *
hyscan_async_new (void)
{
  return g_object_new (HYSCAN_TYPE_ASYNC, NULL);
}
