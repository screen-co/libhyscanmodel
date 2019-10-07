/* hyscan-async.c
 *
 * Copyright 2017 Screen LLC, Vladimir Maximov <vmakxs@gmail.com>
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanModel.
 *
 * HyScanModel is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanModel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanModel имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanModel на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-async
 * @Title HyScanAsync
 * @Short_description выполнение запросов в фоне
 *
 * Класс необходимо заменить на GTask.
 */

#include <memory.h>
#include "hyscan-async.h"

#define HYSCAN_ASYNC_COND_TIMEOUT (100 * G_TIME_SPAN_MILLISECOND)
#define HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS (100)

/* Запрос - это команда плюс объект плюс данные. */
typedef struct
{
  HyScanAsyncCommand  command;       /* Команда. */

  gpointer            object;        /* Объект. */
  gpointer            data;          /* Данные. */

  HyScanAsyncResult   callback;      /* Коллбек. */
  gpointer            callback_data; /* Первый пар-р коллбека. */

  gpointer            result;        /* Результат выполнения. */
} HyScanQuery;

struct _HyScanAsyncPrivate
{
  GList     *waiting;        /* Очередь запросов. */
  GList     *ready;          /* Выполненные запросы. */

  GThread   *sender;         /* Поток выполнения запросов. */
  gint       shutdown;       /* Флаг останова потока выполнения запросов. */
  GMutex     mutex;          /* Мьютекс, для установки запроса на выполнение. */
  GCond      cond;           /* Условие приостановки потока выполнения запросов. */

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
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->constructed = hyscan_async_object_constructed;
  oclass->finalize = hyscan_async_object_finalize;
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

      if (query->callback != NULL)
        (*query->callback) (query->callback_data, query->result, query->object);
    }

  g_list_free_full (ready, (GDestroyNotify) hyscan_async_query_free);

  return G_SOURCE_CONTINUE;
}

/* Освобождает память, распределенную под запрос. */
static void
hyscan_async_query_free (HyScanQuery *query)
{
  if (query != NULL)
    g_free (query);
}

/**
 * hyscan_async_new:
 *
 * Создаёт объект HyScanAsync.
 *
 * Returns: (transfer full): #HyScanAsync
 */
HyScanAsync *
hyscan_async_new (void)
{
  return g_object_new (HYSCAN_TYPE_ASYNC, NULL);
}

/**
 * hyscan_async_append:
 * @async: #HyScanAsync
 * @command: #HyScanAsyncCommand
 * @object: первый параметр, передаваемый в HyScanAsyncCommand
 * @data: второй параметр, передаваемый в HyScanAsyncCommand
 * @callback: коллбек, вызываемый после выполнения задания
 * @callback_data: первый параметр коллбека
 *
 * Добавляет запрос в список. Функция ничего не копирует, требуется самостоятельно
 * освобождать данные и гарантировать их доступность.
 * Коллбэк будет вызыван в главном потоке.
 *
 * Returns: %TRUE, если удалось добавить запрос, %FALSE, если произошла ошибка.
 */
gboolean
hyscan_async_append (HyScanAsync        *async,
                     HyScanAsyncCommand  command,
                     gpointer            object,
                     gpointer            data,
                     HyScanAsyncResult   callback,
                     gpointer            callback_data)
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
  query->data = data;
  query->callback = callback;
  query->callback_data = callback_data;

  /* Добавляем. */
  g_mutex_lock (&priv->mutex);
  async->priv->waiting = g_list_append (async->priv->waiting, (gpointer) query);
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}
