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
#ifndef __HYSCAN_SONAR_STATE_H__
#define __HYSCAN_SONAR_STATE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-sonar-info.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_STATE            (hyscan_sonar_state_get_type ())
#define HYSCAN_SONAR_STATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_STATE, HyScanSonarState))
#define HYSCAN_IS_SONAR_STATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_STATE))
#define HYSCAN_SONAR_STATE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SONAR_STATE, HyScanSonarStateInterface))

typedef struct _HyScanSonarState HyScanSonarState;
typedef struct _HyScanSonarStateInterface HyScanSonarStateInterface;

struct _HyScanSonarStateInterface
{
  GTypeInterface                g_iface;

  HyScanSonarReceiverModeType (*receiver_get_mode)             (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source);
  gboolean                    (*receiver_get_time)             (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                        *receive_time,
                                                                gdouble                        *wait_time);
  gboolean                    (*receiver_get_disabled)         (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source);
  gboolean                    (*generator_get_preset)          (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source,
                                                                gint64                         *preset);
  gboolean                    (*generator_get_disabled)        (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source);
  HyScanSonarTVGModeType      (*tvg_get_mode)                  (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source);
  gboolean                    (*tvg_get_auto)                  (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                        *level,
                                                                gdouble                        *sensitivity);
  gboolean                    (*tvg_get_constant)              (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                        *gain);
  gboolean                    (*tvg_get_linear_db)             (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                        *gain0,
                                                                gdouble                        *gain_step);
  gboolean                    (*tvg_get_logarithmic)           (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                        *gain0,
                                                                gdouble                        *beta,
                                                                gdouble                        *alpha);
  gboolean                    (*tvg_get_disabled)              (HyScanSonarState               *sonar,
                                                                HyScanSourceType                source);
  gboolean                    (*get_start)                     (HyScanSonarState               *state,
                                                                gchar                         **project_name,
                                                                gchar                         **track_name,
                                                                HyScanTrackType                *track_type,
                                                                HyScanTrackPlan               **plan);
};

HYSCAN_API
GType                       hyscan_sonar_state_get_type                (void);

HYSCAN_API
HyScanSonarReceiverModeType hyscan_sonar_state_receiver_get_mode       (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source);

HYSCAN_API
gboolean                    hyscan_sonar_state_receiver_get_time       (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source,
                                                                        gdouble                        *receive_time,
                                                                        gdouble                        *wait_time);

HYSCAN_API
gboolean                    hyscan_sonar_state_receiver_get_disabled   (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source);

HYSCAN_API
gboolean                    hyscan_sonar_state_generator_get_preset    (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source,
                                                                        gint64                         *preset);

HYSCAN_API
gboolean                    hyscan_sonar_state_generator_get_disabled  (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source);

HYSCAN_API
HyScanSonarTVGModeType      hyscan_sonar_state_tvg_get_mode            (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source);

HYSCAN_API
gboolean                    hyscan_sonar_state_tvg_get_auto            (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source,
                                                                        gdouble                        *level,
                                                                        gdouble                        *sensitivity);

HYSCAN_API
gboolean                    hyscan_sonar_state_tvg_get_constant        (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source,
                                                                        gdouble                        *gain);

HYSCAN_API
gboolean                    hyscan_sonar_state_tvg_get_linear_db       (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source,
                                                                        gdouble                        *gain0,
                                                                        gdouble                        *gain_step);

HYSCAN_API
gboolean                    hyscan_sonar_state_tvg_get_logarithmic     (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source,
                                                                        gdouble                        *gain0,
                                                                        gdouble                        *beta,
                                                                        gdouble                        *alpha);

HYSCAN_API
gboolean                    hyscan_sonar_state_tvg_get_disabled        (HyScanSonarState               *sonar,
                                                                        HyScanSourceType                source);

HYSCAN_API
gboolean                    hyscan_sonar_state_get_start               (HyScanSonarState               *state,
                                                                        gchar                         **project_name,
                                                                        gchar                         **track_name,
                                                                        HyScanTrackType                *track_type,
                                                                        HyScanTrackPlan               **plan);

G_END_DECLS

#endif /* __HYSCAN_SONAR_STATE_H__ */
