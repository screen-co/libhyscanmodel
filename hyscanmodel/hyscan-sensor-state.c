/* hyscan-sensor-state.h
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

/**
 * SECTION: hyscan-@class-name@
 * @Title HyScan@Class@
 * @Short_description
 *
 */
#include "hyscan-sensor-state.h"

G_DEFINE_INTERFACE (HyScanSensorState, hyscan_sensor_state, G_TYPE_OBJECT);

static void
hyscan_sensor_state_default_init (HyScanSensorStateInterface *iface)
{
}

gboolean
hyscan_sensor_state_get_enabled (HyScanSensorState *state,
                                 const gchar       *name)
{
  HyScanSensorStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_STATE (state), FALSE);

  iface = HYSCAN_SENSOR_STATE_GET_IFACE (state);
  if (iface->get_enabled != NULL)
    return (* iface->get_enabled) (state, name);

  return FALSE;
}