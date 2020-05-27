/* hyscan-object-model.h
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanModel library.
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

#ifndef __HYSCAN_OBJECT_MODEL_H__
#define __HYSCAN_OBJECT_MODEL_H__

#include <hyscan-object-data.h>
#include <hyscan-object-data-geomark.h>
#include <hyscan-object-data-wfmark.h>
#include <hyscan-object-data-planner.h>
#include <hyscan-mark.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_MODEL             (hyscan_object_model_get_type ())
#define HYSCAN_OBJECT_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_MODEL, HyScanObjectModel))
#define HYSCAN_IS_OBJECT_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_MODEL))
#define HYSCAN_OBJECT_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_MODEL, HyScanObjectModelClass))
#define HYSCAN_IS_OBJECT_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_MODEL))
#define HYSCAN_OBJECT_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_MODEL, HyScanObjectModelClass))

typedef struct _HyScanObjectModel HyScanObjectModel;
typedef struct _HyScanObjectModelPrivate HyScanObjectModelPrivate;
typedef struct _HyScanObjectModelClass HyScanObjectModelClass;

struct _HyScanObjectModel
{
  GObject parent_instance;

  HyScanObjectModelPrivate *priv;
};

struct _HyScanObjectModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_object_model_get_type          (void);

HYSCAN_API
HyScanObjectModel *     hyscan_object_model_new               (GType                      data_type);

HYSCAN_API
void                    hyscan_object_model_set_project       (HyScanObjectModel         *model,
                                                               HyScanDB                  *db,
                                                               const gchar               *project);

HYSCAN_API
void                    hyscan_object_model_refresh           (HyScanObjectModel         *model);

HYSCAN_API
void                    hyscan_object_model_add_object        (HyScanObjectModel         *model,
                                                               const HyScanObject        *object);

HYSCAN_API
void                    hyscan_object_model_modify_object     (HyScanObjectModel         *model,
                                                               const gchar               *id,
                                                               const HyScanObject        *object);

HYSCAN_API
void                    hyscan_object_model_remove_object     (HyScanObjectModel         *model,
                                                               const gchar               *id);

HYSCAN_API
GHashTable *            hyscan_object_model_get               (HyScanObjectModel         *model);

HYSCAN_API
HyScanObject *          hyscan_object_model_get_by_id         (HyScanObjectModel         *model,
                                                               const gchar               *id);

HYSCAN_API
GHashTable *            hyscan_object_model_copy              (GHashTable                *objects);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_MODEL_H__ */
