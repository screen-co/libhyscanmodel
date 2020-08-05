/* hyscan-control-model.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_CONTROL_MODEL_H__
#define __HYSCAN_CONTROL_MODEL_H__

#include <hyscan-control.h>
#include "hyscan-sonar-state.h"
#include "hyscan-sensor-state.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_CONTROL_MODEL             (hyscan_control_model_get_type ())
#define HYSCAN_CONTROL_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CONTROL_MODEL, HyScanControlModel))
#define HYSCAN_IS_CONTROL_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CONTROL_MODEL))
#define HYSCAN_CONTROL_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CONTROL_MODEL, HyScanControlModelClass))
#define HYSCAN_IS_CONTROL_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CONTROL_MODEL))
#define HYSCAN_CONTROL_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CONTROL_MODEL, HyScanControlModelClass))

typedef struct _HyScanControlModel HyScanControlModel;
typedef struct _HyScanControlModelPrivate HyScanControlModelPrivate;
typedef struct _HyScanControlModelClass HyScanControlModelClass;

struct _HyScanControlModel
{
  GObject parent_instance;

  HyScanControlModelPrivate *priv;
};

struct _HyScanControlModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                     hyscan_control_model_get_type         (void);

HYSCAN_API
HyScanControlModel *      hyscan_control_model_new              (HyScanControl      *control);

HYSCAN_API
HyScanControl *           hyscan_control_model_get_control      (HyScanControlModel *model);

HYSCAN_API
void                      hyscan_control_model_set_sync_timeout (HyScanControlModel *model,
                                                                 guint               msec);

HYSCAN_API
guint                     hyscan_control_model_get_sync_timeout (HyScanControlModel *model);

G_END_DECLS

#endif /* __HYSCAN_CONTROL_MODEL_H__ */
