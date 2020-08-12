/* hyscan-device-state.c
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

/**
 * SECTION: hyscan-device-state
 * @Title HyScanDeviceState
 * @Short_description состояние устройства
 *
 * Интерфейс #HyScanDeviceState определяет функции для получения статуса устройства
 * #HyScanDevice.
 *
 */
#include "hyscan-device-state.h"

G_DEFINE_INTERFACE (HyScanDeviceState, hyscan_device_state, G_TYPE_OBJECT)

static void
hyscan_device_state_default_init (HyScanDeviceStateInterface *iface)
{
}

/**
 * hyscan_device_state_get_sound_velocity:
 * @device_state: указатель на #HyScanDeviceState
 *
 * Функция возвращает таблицу профиля скорости звука.
 *
 * Returns: (element-type: HyScanSoundVelocity) (transfer full): таблица профиля звука, для удаления g_list_free_full().
 */
GList *
hyscan_device_state_get_sound_velocity (HyScanDeviceState *device_state)
{
  HyScanDeviceStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEVICE_STATE (device_state), NULL);

  iface = HYSCAN_DEVICE_STATE_GET_IFACE (device_state);
  if (iface->get_sound_velocity != NULL)
    return (* iface->get_sound_velocity) (device_state);

  return FALSE;
}

/**
 * hyscan_device_state_get_disconnected:
 * @device_state: указатель на #HyScanDeviceState
 *
 * Функция возвращает признак того, что устройство было отключено.
 *
 * Returns: %TRUE если устройство было отключено, иначе %FALSE.
 */
gboolean
hyscan_device_state_get_disconnected (HyScanDeviceState *device_state)
{
  HyScanDeviceStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEVICE_STATE (device_state), FALSE);

  iface = HYSCAN_DEVICE_STATE_GET_IFACE (device_state);
  if (iface->get_disconnected != NULL)
    return (* iface->get_disconnected) (device_state);

  return FALSE;
}
