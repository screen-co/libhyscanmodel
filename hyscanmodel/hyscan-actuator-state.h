/* hyscan-actuator-state.h
 *
 * Copyright 2021 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
#ifndef __HYSCAN_ACTUATOR_STATE_H__
#define __HYSCAN_ACTUATOR_STATE_H__

#include <hyscan-actuator.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_ACTUATOR_STATE            (hyscan_actuator_state_get_type ())
#define HYSCAN_ACTUATOR_STATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_ACTUATOR_STATE, HyScanActuatorState))
#define HYSCAN_IS_ACTUATOR_STATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_ACTUATOR_STATE))
#define HYSCAN_ACTUATOR_STATE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_ACTUATOR_STATE, HyScanActuatorStateInterface))

typedef struct _HyScanActuatorState HyScanActuatorState;
typedef struct _HyScanActuatorStateInterface HyScanActuatorStateInterface;

struct _HyScanActuatorStateInterface
{
  GTypeInterface       g_iface;

  HyScanActuatorModeType (*get_mode)           (HyScanActuatorState  *actuator,
                                                const gchar           *name);

  gboolean               (*get_disable)        (HyScanActuatorState   *actuator,
                                                const gchar           *name);

  gboolean               (*get_scan)           (HyScanActuatorState   *actuator,
                                                const gchar           *name,
                                                gdouble               *from,
                                                gdouble               *to,
                                                gdouble               *speed);

  gboolean               (*get_manual)         (HyScanActuatorState   *actuator,
                                                const gchar           *name,
                                                gdouble               *angle);
};

HYSCAN_API
GType                  hyscan_actuator_state_get_type                  (void);

HYSCAN_API
HyScanActuatorModeType hyscan_actuator_state_get_mode    (HyScanActuatorState *actuator,
                                                          const gchar         *name);

HYSCAN_API
gboolean               hyscan_actuator_state_get_disable (HyScanActuatorState *actuator,
                                                          const gchar         *name);

HYSCAN_API
gboolean               hyscan_actuator_state_get_scan    (HyScanActuatorState *actuator,
                                                          const gchar         *name,
                                                          gdouble             *from,
                                                          gdouble             *to,
                                                          gdouble             *speed);

HYSCAN_API
gboolean               hyscan_actuator_state_get_manual  (HyScanActuatorState *actuator,
                                                          const gchar         *name,
                                                          gdouble             *angle);
G_END_DECLS

#endif /* __HYSCAN_ACTUATOR_STATE_H__ */
