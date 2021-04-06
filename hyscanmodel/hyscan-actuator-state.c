/* hyscan-actuator-state.h
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
 * SECTION: hyscan-actuator-state
 * @Title HyScanActuatorState
 * @Short_description состояние актуатора
 *
 * Интерфейс #HyScanActuatorState определяет функции для получения статуса
 * актуатора #HyScanActuator.
 */
#include "hyscan-actuator-state.h"

G_DEFINE_INTERFACE (HyScanActuatorState, hyscan_actuator_state, G_TYPE_OBJECT);

static void
hyscan_actuator_state_default_init (HyScanActuatorStateInterface *iface)
{
}

HyScanActuatorModeType
hyscan_actuator_state_get_mode (HyScanActuatorState *actuator,
                                const gchar         *name)
{
  HyScanActuatorStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_ACTUATOR_STATE (actuator), HYSCAN_ACTUATOR_MODE_NONE);

  iface = HYSCAN_ACTUATOR_STATE_GET_IFACE (actuator);
  if (iface->get_mode != NULL)
    return (* iface->get_mode) (actuator, name);

  return HYSCAN_ACTUATOR_MODE_NONE;
}

gboolean
hyscan_actuator_state_get_disable (HyScanActuatorState *actuator,
                                   const gchar         *name)
{
  HyScanActuatorStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_ACTUATOR_STATE (actuator), FALSE);

  iface = HYSCAN_ACTUATOR_STATE_GET_IFACE (actuator);
  if (iface->get_disable != NULL)
    return (* iface->get_disable) (actuator, name);

  return FALSE;
}

gboolean
hyscan_actuator_state_get_scan (HyScanActuatorState *actuator,
                                const gchar         *name,
                                gdouble             *from,
                                gdouble             *to,
                                gdouble             *speed)
{
  HyScanActuatorStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_ACTUATOR_STATE (actuator), FALSE);

  iface = HYSCAN_ACTUATOR_STATE_GET_IFACE (actuator);
  if (iface->get_scan != NULL)
    return (* iface->get_scan) (actuator, name, from, to, speed);

  return FALSE;
}

gboolean
hyscan_actuator_state_get_manual (HyScanActuatorState *actuator,
                                  const gchar         *name,
                                  gdouble             *angle)
{
  HyScanActuatorStateInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_ACTUATOR_STATE (actuator), FALSE);

  iface = HYSCAN_ACTUATOR_STATE_GET_IFACE (actuator);
  if (iface->get_manual != NULL)
    return (* iface->get_manual) (actuator, name, angle);

  return FALSE;
}

