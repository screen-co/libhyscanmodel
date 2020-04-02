/* hyscan-sonar-state.h
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
 * SECTION: hyscan-sonar-state
 * @Title HyScanSonarState
 * @Short_description
 *
 */
#include "hyscan-sonar-state.h"

#define HYSCAN_SONAR_STATE_FUNC(retval, func, ...)                             \
HyScanSonarStateInterface *iface;                                              \
g_return_val_if_fail (HYSCAN_IS_SONAR_STATE (sonar), retval);                  \
iface = HYSCAN_SONAR_STATE_GET_IFACE (sonar);                                  \
if (iface->func != NULL)                                                       \
  return (* iface->func) (sonar, __VA_ARGS__);                                 \
return retval;

G_DEFINE_INTERFACE (HyScanSonarState, hyscan_sonar_state, G_TYPE_OBJECT);

static void
hyscan_sonar_state_default_init (HyScanSonarStateInterface *iface)
{
  g_signal_new ("start-stop", HYSCAN_TYPE_SONAR_STATE,
                G_SIGNAL_RUN_LAST, 0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}


HyScanSonarReceiverModeType
hyscan_sonar_state_receiver_get_mode (HyScanSonarState *sonar,
                                      HyScanSourceType  source)
{
  HYSCAN_SONAR_STATE_FUNC (HYSCAN_SONAR_RECEIVER_MODE_NONE, receiver_get_mode, source)
}


gboolean
hyscan_sonar_state_receiver_get_time (HyScanSonarState *sonar,
                                      HyScanSourceType  source,
                                      gdouble          *receive_time,
                                      gdouble          *wait_time)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, receiver_get_time, source, receive_time, wait_time)
}

gboolean
hyscan_sonar_state_receiver_get_disabled (HyScanSonarState *sonar,
                                          HyScanSourceType  source)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, receiver_get_disabled, source);
}

gboolean
hyscan_sonar_state_generator_get_preset (HyScanSonarState *sonar,
                                         HyScanSourceType  source,
                                         gint64           *preset)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, generator_get_preset, source, preset);
}


gboolean
hyscan_sonar_state_generator_get_disabled (HyScanSonarState *sonar,
                                           HyScanSourceType  source)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, generator_get_disabled, source);
}

HyScanSonarTVGModeType
hyscan_sonar_state_tvg_get_mode (HyScanSonarState *sonar,
                                 HyScanSourceType  source)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, tvg_get_mode, source);
}

gboolean
hyscan_sonar_state_tvg_get_auto (HyScanSonarState *sonar,
                                 HyScanSourceType  source,
                                 gdouble          *level,
                                 gdouble          *sensitivity)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, tvg_get_auto, source, level, sensitivity);
}

gboolean
hyscan_sonar_state_tvg_get_constant (HyScanSonarState *sonar,
                                     HyScanSourceType  source,
                                     gdouble          *gain)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, tvg_get_constant, source, gain);
}

gboolean
hyscan_sonar_state_tvg_get_linear_db (HyScanSonarState *sonar,
                                      HyScanSourceType  source,
                                      gdouble          *gain0,
                                      gdouble          *gain_step)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, tvg_get_linear_db, source, gain0, gain_step);
}

gboolean
hyscan_sonar_state_tvg_get_logarithmic (HyScanSonarState *sonar,
                                        HyScanSourceType  source,
                                        gdouble          *gain0,
                                        gdouble          *beta,
                                        gdouble          *alpha)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, tvg_get_logarithmic, source, gain0, beta, alpha);
}

gboolean
hyscan_sonar_state_tvg_get_disabled (HyScanSonarState *sonar,
                                     HyScanSourceType  source)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, tvg_get_disabled, source);
}

gboolean
hyscan_sonar_state_get_start (HyScanSonarState  *sonar,
                              gchar            **project_name,
                              gchar            **track_name,
                              HyScanTrackType   *track_type,
                              HyScanTrackPlan  **plan)
{
  HYSCAN_SONAR_STATE_FUNC (FALSE, get_start, project_name, track_name, track_type, plan);
}
