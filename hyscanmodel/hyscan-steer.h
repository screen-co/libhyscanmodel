/* hyscan-steer.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanModel library.
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

#ifndef __HYSCAN_STEER_H__
#define __HYSCAN_STEER_H__

#include <hyscan-nav-state.h>
#include <hyscan-planner-selection.h>
#include <hyscan-sonar-recorder.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_STEER             (hyscan_steer_get_type ())
#define HYSCAN_STEER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_STEER, HyScanSteer))
#define HYSCAN_IS_STEER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_STEER))
#define HYSCAN_STEER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_STEER, HyScanSteerClass))
#define HYSCAN_IS_STEER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_STEER))
#define HYSCAN_STEER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_STEER, HyScanSteerClass))

typedef struct _HyScanSteer HyScanSteer;
typedef struct _HyScanSteerPrivate HyScanSteerPrivate;
typedef struct _HyScanSteerClass HyScanSteerClass;
typedef struct _HyScanSteerPoint HyScanSteerPoint;

/**
 * HyScanSteerPoint:
 * @nav_data: исходные навигационные данные о положении и скорости судна
 * @position: положение судна в системе координат активного плана галса
 * @track: проекция положения судна на галс
 * @d_y: отклонение положения антенны по расстоянию, м
 * @d_angle: отклонение курса судна по углу, рад
 * @time_left: отклонение судна по скорости, м/с
 *
 */
struct _HyScanSteerPoint
{
  HyScanNavStateData   nav_data;
  HyScanGeoCartesian2D position; 
  HyScanGeoCartesian2D track;
  gdouble              d_y;
  gdouble              d_angle;
  gdouble              time_left;
};

struct _HyScanSteer
{
  GObject parent_instance;

  HyScanSteerPrivate *priv;
};

struct _HyScanSteerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                      hyscan_steer_get_type          (void);

HYSCAN_API
HyScanSteer *              hyscan_steer_new               (HyScanNavState            *nav_state,
                                                           HyScanPlannerSelection    *selection,
                                                           HyScanSonarRecorder       *recorder);

HYSCAN_API
const HyScanPlannerTrack * hyscan_steer_get_track         (HyScanSteer               *steer,
                                                           gchar                    **track_id);

HYSCAN_API
gboolean                   hyscan_steer_get_track_topo    (HyScanSteer               *steer,
                                                           HyScanGeoCartesian2D      *start,
                                                           HyScanGeoCartesian2D      *end);

HYSCAN_API
gboolean                   hyscan_steer_get_track_info    (HyScanSteer               *steer,
                                                           gdouble                   *azimuth,
                                                           gdouble                   *length);

HYSCAN_API
void                       hyscan_steer_sensor_set_offset (HyScanSteer               *steer,
                                                           const HyScanAntennaOffset *offset);

HYSCAN_API
void                       hyscan_steer_set_threshold     (HyScanSteer                *steer,
                                                           gdouble                     threshold);

HYSCAN_API
gdouble                    hyscan_steer_get_threshold     (HyScanSteer                *steer);

HYSCAN_API
void                       hyscan_steer_set_autostart     (HyScanSteer               *steer,
                                                           gboolean                   autostart);

HYSCAN_API
gboolean                   hyscan_steer_get_autostart     (HyScanSteer               *steer);

HYSCAN_API
void                       hyscan_steer_set_autoselect    (HyScanSteer               *steer,
                                                           gboolean                   autoselect);

HYSCAN_API
gboolean                   hyscan_steer_get_autoselect    (HyScanSteer               *steer);

HYSCAN_API
void                       hyscan_steer_calc_point        (HyScanSteerPoint          *point,
                                                           HyScanSteer               *steer);

HYSCAN_API
HyScanSteerPoint *         hyscan_steer_point_copy        (const HyScanSteerPoint    *point);

HYSCAN_API
void                       hyscan_steer_point_free        (HyScanSteerPoint          *point);

G_END_DECLS

#endif /* __HYSCAN_STEER_H__ */
