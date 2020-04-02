#ifndef __WAYPOINT_H__
#define __WAYPOINT_H__

#include <hyscan-planner.h>

typedef struct
{
  gint64                     time;          /* Время фиксации положения. */
  HyScanGeoCartesian2D       c2d;           /* Декартовы координаты точки. */
  HyScanGeoGeodetic          geod;          /* Геогрфические координаты точки. */
  gdouble                    velocity;      /* Скорость движения, м/c. */
} WayPoint;

gchar *           waypoint_write              (HyScanDB         *db,
                                               const gchar      *project_name,
                                               const gchar      *sensor_name,
                                               WayPoint         *points,
                                               guint             start,
                                               guint             end,
                                               HyScanTrackPlan  *plan);

WayPoint *        waypoint_generate           (gint              start_idx,
                                               gint              end_idx,
                                               gdouble           velocity,
                                               HyScanGeoGeodetic start,
                                               gdouble           dv,
                                               gdouble           dtrack);

#endif /* __WAYPOINT_H__ */
