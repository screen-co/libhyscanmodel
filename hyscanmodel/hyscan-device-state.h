/* hyscan-device-state.h
 *
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

#ifndef __HYSCAN_DEVICE_STATE_H__
#define __HYSCAN_DEVICE_STATE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEVICE_STATE            (hyscan_device_state_get_type ())
#define HYSCAN_DEVICE_STATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEVICE_STATE, HyScanDeviceState))
#define HYSCAN_IS_DEVICE_STATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEVICE_STATE))
#define HYSCAN_DEVICE_STATE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_DEVICE_STATE, HyScanDeviceStateInterface))

typedef struct _HyScanDeviceState HyScanDeviceState;
typedef struct _HyScanDeviceStateInterface HyScanDeviceStateInterface;

struct _HyScanDeviceStateInterface
{
  GTypeInterface       g_iface;

  GList *              (*get_sound_velocity)                (HyScanDeviceState         *device_state);

  gboolean             (*get_disconnected)                  (HyScanDeviceState         *device_state);
};

HYSCAN_API
GType               hyscan_device_state_get_type            (void);

HYSCAN_API
GList *             hyscan_device_state_get_sound_velocity  (HyScanDeviceState         *device_state);

HYSCAN_API
gboolean            hyscan_device_state_get_disconnected    (HyScanDeviceState         *device_state);

G_END_DECLS

#endif /* __HYSCAN_DEVICE_STATE_H__ */
