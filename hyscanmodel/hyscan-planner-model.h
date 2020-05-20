/* hyscan-planner-model.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_PLANNER_MODEL_H__
#define __HYSCAN_PLANNER_MODEL_H__

#include <hyscan-object-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER_MODEL             (hyscan_planner_model_get_type ())
#define HYSCAN_PLANNER_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER_MODEL, HyScanPlannerModel))
#define HYSCAN_IS_PLANNER_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER_MODEL))
#define HYSCAN_PLANNER_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER_MODEL, HyScanPlannerModelClass))
#define HYSCAN_IS_PLANNER_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER_MODEL))
#define HYSCAN_PLANNER_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER_MODEL, HyScanPlannerModelClass))

typedef struct _HyScanPlannerModel HyScanPlannerModel;
typedef struct _HyScanPlannerModelPrivate HyScanPlannerModelPrivate;
typedef struct _HyScanPlannerModelClass HyScanPlannerModelClass;

struct _HyScanPlannerModel
{
  HyScanObjectModel parent_instance;

  HyScanPlannerModelPrivate *priv;
};

struct _HyScanPlannerModelClass
{
  HyScanObjectModelClass parent_class;
};

HYSCAN_API
GType                  hyscan_planner_model_get_type         (void);

HYSCAN_API
HyScanPlannerModel *   hyscan_planner_model_new              (void);

HYSCAN_API
HyScanGeo *            hyscan_planner_model_get_geo          (HyScanPlannerModel       *pmodel);

HYSCAN_API
void                   hyscan_planner_model_set_origin       (HyScanPlannerModel       *pmodel,
                                                              const HyScanPlannerOrigin*origin);

HYSCAN_API
HyScanPlannerOrigin *  hyscan_planner_model_get_origin       (HyScanPlannerModel       *pmodel);

HYSCAN_API
void                   hyscan_planner_model_assign_number    (HyScanPlannerModel       *pmodel,
                                                              const gchar              *track0_id);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_MODEL_H__ */
