/* hyscan-sonar-model.h
 *
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

#ifndef __HYSCAN_SONAR_MODEL_H__
#define __HYSCAN_SONAR_MODEL_H__

#include <hyscan-control.h>
#include "hyscan-sonar-state.h"
#include "hyscan-sensor-state.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_MODEL             (hyscan_sonar_model_get_type ())
#define HYSCAN_SONAR_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_MODEL, HyScanSonarModel))
#define HYSCAN_IS_SONAR_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_MODEL))
#define HYSCAN_SONAR_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_MODEL, HyScanSonarModelClass))
#define HYSCAN_IS_SONAR_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_MODEL))
#define HYSCAN_SONAR_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_MODEL, HyScanSonarModelClass))

typedef struct _HyScanSonarModel HyScanSonarModel;
typedef struct _HyScanSonarModelPrivate HyScanSonarModelPrivate;
typedef struct _HyScanSonarModelClass HyScanSonarModelClass;

struct _HyScanSonarModel
{
  GObject parent_instance;

  HyScanSonarModelPrivate *priv;
};

struct _HyScanSonarModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                     hyscan_sonar_model_get_type          (void);

HYSCAN_API
HyScanSonarModel *        hyscan_sonar_model_new               (HyScanControl *control);

HYSCAN_API
void                      hyscan_sonar_model_set_sync_timeout (HyScanSonarModel *model,
                                                               guint             msec);

HYSCAN_API
guint                     hyscan_sonar_model_get_sync_timeout (HyScanSonarModel *model);

G_END_DECLS

#endif /* __HYSCAN_SONAR_MODEL_H__ */
