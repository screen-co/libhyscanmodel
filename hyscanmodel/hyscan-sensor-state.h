/* hyscan-sensor-state.h
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
#ifndef __HYSCAN_SENSOR_STATE_H__
#define __HYSCAN_SENSOR_STATE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SENSOR_STATE            (hyscan_sensor_state_get_type ())
#define HYSCAN_SENSOR_STATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SENSOR_STATE, HyScanSensorState))
#define HYSCAN_IS_SENSOR_STATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SENSOR_STATE))
#define HYSCAN_SENSOR_STATE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SENSOR_STATE, HyScanSensorStateInterface))

typedef struct _HyScanSensorState HyScanSensorState;
typedef struct _HyScanSensorStateInterface HyScanSensorStateInterface;

struct _HyScanSensorStateInterface
{
  GTypeInterface       g_iface;

  gboolean            (*get_enabled)                  (HyScanSensorState *state,
                                                       const gchar       *name);
};

HYSCAN_API
GType               hyscan_sensor_state_get_type      (void);

HYSCAN_API
gboolean            hyscan_sensor_state_get_enabled   (HyScanSensorState *state,
                                                       const gchar       *name);

G_END_DECLS

#endif /* __HYSCAN_SENSOR_STATE_H__ */
