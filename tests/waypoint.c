#include "waypoint.h"
#include <hyscan-data-writer.h>
#include <hyscan-nmea-helper.h>
#include <string.h>

/* Функция записывает путевые точки в NMEA-канал в виде RMC-строк. */
gchar *
waypoint_write (HyScanDB        *db,
                const gchar     *project_name,
                const gchar     *sensor_name,
                WayPoint        *points,
                guint            start,
                guint            end,
                HyScanTrackPlan *plan)
{
  HyScanDataWriter *writer;
  HyScanBuffer *buffer;
  guint i;
  gchar *track_name;

  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);
  track_name = g_strdup_printf ("%ld", g_get_real_time ());
  if (!hyscan_data_writer_start (writer, project_name, track_name, HYSCAN_TRACK_SURVEY, plan, -1))
    g_error ("Could not start data writer");

  buffer = hyscan_buffer_new ();

  for (i = start; i <= end; i++)
    {
      gchar *sentence;

      sentence = hyscan_nmea_helper_make_rmc (points[i].geod, points[i].course, points[i].velocity, points[i].time);
      hyscan_buffer_wrap (buffer, HYSCAN_DATA_STRING, sentence, strlen (sentence));
      if (!hyscan_data_writer_sensor_add_data (writer, sensor_name, HYSCAN_SOURCE_NMEA, 1, points[i].time, buffer))
        g_error ("Failed to add data");

      g_free (sentence);
    }

  hyscan_data_writer_stop (writer);
  g_object_unref (writer);
  g_object_unref (buffer);

  g_message ("Track %s has been written", track_name);

  return track_name;
}

/* Функция генерирует тестовые данные: движение со скоростю velocity
 * по направлению course и соответствующими дисперсиями dv и dtrack. */
WayPoint *
waypoint_generate (gint              start_idx,
                   gint              end_idx,
                   gdouble           velocity,
                   HyScanGeoPoint    start,
                   gboolean          course,
                   gdouble           dv,
                   gdouble           dtrack)
{
  WayPoint *points;
  HyScanGeoGeodetic origin;

  HyScanGeo *geo;
  gint i, n_points;

  n_points = end_idx - start_idx + 1;
  points = g_new (WayPoint, n_points);

  origin.lat = start.lat;
  origin.lon = start.lon;
  origin.h = course;
  geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
  for (i = 0; i < n_points; i++)
    {
      points[i].time = i * G_TIME_SPAN_SECOND;
      points[i].c2d.x = velocity * (i + start_idx);
      points[i].c2d.y = 0;

      hyscan_geo_topoXY2geo0 (geo, &points[i].geod, points[i].c2d);

      /* В курс и скорость добавляем "случайные" ошибки. */
      points[i].course = course + (i % 2 ? -1 : 1) * dtrack;
      points[i].velocity = velocity + (i % 2 ? -1 : 1) * dv;
    }

  g_object_unref (geo);

  return points;
}
