/*
 * \file hyscan-async.c
 *
 * \brief Исходный файл класса HyScanAsync, выполняющего список запросов в отдельном потоке.
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2019
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <memory.h>
#include "hyscan-async.h"

#define HYSCAN_ASYNC_COND_TIMEOUT (100 * G_TIME_SPAN_MILLISECOND)
#define HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS (100)

/* Запрос - это команда плюс объект плюс данные. */
typedef struct
{
  HyScanAsyncCommand command; /* Команда. */
  gpointer           object;  /* Объект. */
  gpointer           data;    /* Данные. */
  GQuark             detail;  /* Detail для эмиссии сигнала. */
  gpointer           result;  /* Результат выполнения. */
} HyScanQuery;

enum
{
  SIGNAL_READY,
  SIGNAL_LAST
};

/* Идентификаторы сигналов. */
guint hyscan_async_signals[SIGNAL_LAST] = { 0 };

struct _HyScanAsyncPrivate
{
  GList     *waiting;             /* Очередь запросов. */
  GList     *ready;               /* Выполненные запросы. */

  GThread   *sender;              /* Поток выполнения запросов. */
  gint       shutdown;            /* Флаг останова потока выполнения запросов. */
  GMutex     mutex;               /* Мьютекс, для установки запроса на выполнение. */
  GCond      cond;                /* Условие приостановки потока выполнения запросов. */

  guint      result_tag;     /* Идентификатор таймера. */
};

static void     hyscan_async_object_constructed (GObject        *object);
static void     hyscan_async_object_finalize    (GObject        *object);

static void     hyscan_async_shutdown           (HyScanAsync    *async);
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

  hyscan_async_signals[SIGNAL_READY] =
      g_signal_new ("ready", HYSCAN_TYPE_ASYNC,
                    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE,
                    1, G_TYPE_POINTER);
}

static void
hyscan_async_init (HyScanAsync *async)
{
  HyScanAsyncPrivate *priv = hyscan_async_get_instance_private (async);

  priv->sender = NULL;
  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);

  async->priv = priv;
}

static void
hyscan_async_object_constructed (GObject *object)
{
  HyScanAsync *async = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = async->priv;

  G_OBJECT_CLASS (hyscan_async_parent_class)->constructed (object);

  /* Поток, в котором выполняются запросы. */
  priv->shutdown = FALSE;
  priv->sender = g_thread_new ("hyscan-async", hyscan_async_thread_func, async);

  priv->result_tag = g_timeout_add (HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS,
                                    hyscan_async_result_func, async);
}

static void
hyscan_async_object_finalize (GObject *object)
{
  HyScanAsync *async = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = async->priv;

  hyscan_async_shutdown (async);

  if (priv->result_tag)
    g_source_remove (priv->result_tag);

  g_mutex_clear (&priv->mutex);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (hyscan_async_parent_class)->finalize (object);
}

/* Завершает поток отправки и выключает таймер опроса статуса. */
static void
hyscan_async_shutdown (HyScanAsync *async)
{
  HyScanAsyncPrivate *priv = async->priv;

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_cond_signal (&priv->cond);
  g_thread_join (priv->sender);
}

/* Функция выполняет запросы. */
static gpointer
hyscan_async_thread_func (gpointer object)
{
  HyScanAsync *async = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = async->priv;
  GList *waiting, *link;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      g_mutex_lock (&priv->mutex);

      if (priv->waiting == NULL)
        {
          gint64 end_time = g_get_monotonic_time () + HYSCAN_ASYNC_COND_TIMEOUT;
          g_cond_wait_until (&priv->cond, &priv->mutex, end_time);
        }

      /* Забираем список запросов. */
      waiting = priv->waiting;
      priv->waiting = NULL;
      g_mutex_unlock (&priv->mutex);

      /* Выполняю запросы. */
      while (waiting != NULL)
        {
          HyScanQuery *query = (HyScanQuery *) waiting->data;

          query->result = (*query->command) (query->object, query->data);

          /* Переносим запрос в список выполненных. */
          link = waiting;
          waiting = g_list_remove_link (waiting, link);

          g_mutex_lock (&priv->mutex);
          priv->ready = g_list_concat (priv->ready, link);
          g_mutex_unlock (&priv->mutex);
        }
    }

  return NULL;
}

/* Функция проверяет результат выполнения запросов. */
static gboolean
hyscan_async_result_func (gpointer object)
{
  HyScanAsync *async = HYSCAN_ASYNC (object);
  HyScanAsyncPrivate *priv = async->priv;
  GList *ready, *link;

  if (!g_mutex_trylock (&priv->mutex))
    return G_SOURCE_CONTINUE;

  /* Забираем список выполненных запросов. */
  ready = priv->ready;
  priv->ready = NULL;
  g_mutex_unlock (&priv->mutex);

  /* Выполняю запросы. */
  for (link = ready; link != NULL; link = link->next)
    {
      HyScanQuery *query = (HyScanQuery *) ready->data;

      g_signal_emit (async, hyscan_async_signals[SIGNAL_READY],
                     query->detail, query->result, NULL);
    }

  g_list_free_full (ready, (GDestroyNotify) hyscan_async_query_free);

  return G_SOURCE_CONTINUE;
}

/* Освобождает память, распределенную под запрос. */
static void
hyscan_async_query_free (HyScanQuery *query)
{
  if (query == NULL)
    return;

  g_free (query->data);
  g_free (query);
}

/* Создаёт объект HyScanAsync. */
HyScanAsync *
hyscan_async_new (void)
{
  return g_object_new (HYSCAN_TYPE_ASYNC, NULL);
}

/* Добавляет запрос в список. */
gboolean
hyscan_async_append (HyScanAsync        *async,
                     const gchar        *detail,
                     HyScanAsyncCommand  command,
                     gpointer            object,
                     gconstpointer       data,
                     gsize               data_size)
{
  HyScanAsyncPrivate *priv;
  HyScanQuery *query;

  g_return_val_if_fail (HYSCAN_IS_ASYNC (async), FALSE);
  priv = async->priv;

  if (command == NULL)
    return FALSE;

  query = g_new0 (HyScanQuery, 1);
  query->command = command;
  query->object = object;

  if (detail != NULL)
    query->detail = g_quark_from_static_string (detail);

  if (data != NULL && data_size != 0)
    query->data = g_memdup (data, data_size);

  /* Добавляем. */
  g_mutex_lock (&priv->mutex);
  async->priv->waiting = g_list_append (async->priv->waiting, (gpointer) query);
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}
