/* hyscan-async.h
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

#ifndef __HYSCAN_ASYNC_H__
#define __HYSCAN_ASYNC_H__

#include <glib-object.h>
#include <hyscan-api.h>

/**
 * HyScanAsyncCommand:
 * @object: объект
 * @data: данные
 *
 * функция для выполнения в фоновом потоке.
 */
typedef gpointer (*HyScanAsyncCommand) (gpointer object, gpointer data);
/**
 * HyScanAsyncResult:
 * @data: данные
 * @result: результат выполнения #HyScanAsyncCommand
 * @object: объект
 * функция для вызова из главного потока.
 */
typedef gpointer (*HyScanAsyncResult)  (gpointer data, gpointer result, gpointer object);

G_BEGIN_DECLS

#define HYSCAN_TYPE_ASYNC             (hyscan_async_get_type ())
#define HYSCAN_ASYNC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_ASYNC, HyScanAsync))
#define HYSCAN_IS_ASYNC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_ASYNC))
#define HYSCAN_ASYNC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_ASYNC, HyScanAsyncClass))
#define HYSCAN_IS_ASYNC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_ASYNC))
#define HYSCAN_ASYNC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_ASYNC, HyScanAsyncClass))

typedef struct _HyScanAsync HyScanAsync;
typedef struct _HyScanAsyncPrivate HyScanAsyncPrivate;
typedef struct _HyScanAsyncClass HyScanAsyncClass;

struct _HyScanAsync
{
  GObject parent_instance;

  HyScanAsyncPrivate *priv;
};

struct _HyScanAsyncClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType        hyscan_async_get_type      (void);

HYSCAN_API
HyScanAsync *hyscan_async_new           (void);


HYSCAN_API
gboolean     hyscan_async_append  (HyScanAsync        *async,
                                   HyScanAsyncCommand  command,
                                   gpointer            object,
                                   gpointer            data,
                                   HyScanAsyncResult   callback,
                                   gpointer            callback_data);

G_END_DECLS

#endif /* __HYSCAN_ASYNC_H__ */
