#include "waypoint.h"
#include <hyscan-nmea-helper.h>
#include <hyscan-db.h>
#include <hyscan-track-stats.h>
#include <hyscan-cached.h>
#include <string.h>

#define SENSOR_NAME          "nmea"

#define FLOAT_EQUAL(a,b,f,e) G_STMT_START {                                                           \
                               if (ABS((a->f) - (b->f)) > (e))                                        \
                                 g_error ("Value of " #f " is wrong: %f == %f (allowed error %f)",    \
                                          (a->f), (b->f), (e));                                       \
                             } G_STMT_END

enum
{
  DONE_0,
  DONE_PART,
  DONE_100,
};

typedef struct
{
  const gchar          *title;          /* Название теста. */
  WayPoint             *points;         /* Путевые точки. */
  guint                 n_points;       /* Количестов элементов массива n_points. */
  gchar                *track_name;     /* Название галса. */
  HyScanTrackStatsInfo  expect;         /* Ожидаемые значения стаистики. */
  gboolean              expect_planned; /* Ожидается наличие плана. */
  guint                 done_status;    /* Ожидаемый статус прохождения плана. */
} TestData;

static HyScanDB *db;
static GMainLoop *loop;

static gchar *db_uri;
static gchar *project_name = "planner-stats-test";
static gint timeout = 5000;

static struct
{
  HyScanGeoPoint coord;
  gdouble        course;
} origin[] =
{
  { { 30.51,  40.11}, 44 },
  { { 50.44, -35.13}, 180 },
  { {-20.12,  39.92}, 330 },
};

static void
test_data (TestData   *data,
           GHashTable *tracks)
{
  GHashTableIter iter;
  HyScanTrackStatsInfo *info;
  HyScanTrackStatsInfo *expect = &data->expect;

  g_message ("Processing test \"%s\"...", data->title);

  g_hash_table_iter_init (&iter, tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &info))
    {
      if (g_strcmp0 (info->info->name, data->track_name) == 0)
        {
          /* Сравниваем полученную статистику и ожидаемую. */
          if (data->expect_planned)
            g_assert_nonnull (info->plan);
          else
            g_assert_null (info->plan);

          FLOAT_EQUAL (info, expect, x_length,     expect->velocity * 2.0);
          FLOAT_EQUAL (info, expect, x_min,        expect->velocity * 2.0);
          FLOAT_EQUAL (info, expect, x_max,        expect->velocity * 2.0);
          FLOAT_EQUAL (info, expect, angle,        0.1);
          FLOAT_EQUAL (info, expect, angle_var,    1e-3);
          FLOAT_EQUAL (info, expect, velocity,     0.1);
          FLOAT_EQUAL (info, expect, velocity_var, 1e-3);

          switch (data->done_status)
            {
              case DONE_0:
                g_assert_cmpfloat (info->progress, ==, 0.0);
                break;

              case DONE_100:
                g_assert_cmpfloat (info->progress, ==, 1.0);
                break;

              case DONE_PART:
              default:
                FLOAT_EQUAL (info, expect, progress, 1e-3);
                break;
            }

          return;
        }
    }

  g_error ("Track %s not found in stats", data->track_name);
}

static void
track_stats_changed (HyScanTrackStats *stats,
                     GList            *list)
{
  GHashTable *tracks;
  GList *link;

  tracks = hyscan_track_stats_get (stats);
  g_message ("Found %d tracks", g_hash_table_size (tracks));

  for (link = list; link != NULL; link = link->next)
    test_data (link->data, tracks);

  g_hash_table_unref (tracks);
  g_object_unref (stats);

  /* Тест пройден, завершаем работу. */
  g_main_loop_quit (loop);
}

gboolean
test_failed_timeout (gpointer used_data)
{
  g_error ("Test failed by timeout: %d ms", GPOINTER_TO_INT (used_data));

  return G_SOURCE_REMOVE;
}

gboolean
test_start (GList *data_list)
{
  HyScanCache *cache;
  HyScanDBInfo *db_info;
  HyScanTrackStats *stats;

  cache = HYSCAN_CACHE (hyscan_cached_new (100));

  db_info = hyscan_db_info_new (db);

  stats = hyscan_track_stats_new (db_info, NULL, cache);
  g_signal_connect (stats, "changed", G_CALLBACK (track_stats_changed), data_list);

  hyscan_db_info_set_project (db_info, project_name);
  g_object_unref (cache);
  g_object_unref (db_info);

  return G_SOURCE_REMOVE;
}

static void
test_data_free (TestData *data)
{
  g_free (data->track_name);
  g_free (data->points);
  g_free (data);
}

int
main (int    argc,
      char **argv)
{
  GList *data_list = NULL;
  TestData *data;
  HyScanTrackPlan plan;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "timeout", 'i', 0, G_OPTION_ARG_INT, &timeout, "Test timeout, ms", NULL },
        { "project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_description (context, "Test HyScanTrackStats class");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Failed to connect to the database");

  /* Генерируем тестовые данные и пишем их в БД. */
  data = g_new0 (TestData, 1);
  data->title = "Track 1 (Std. Dev. = 0)";
  data->points = waypoint_generate (0, 99, 1.1, origin[0].coord, origin[0].course, 0, 0);
  data->done_status = DONE_0;
  data->track_name = waypoint_write (db, project_name, SENSOR_NAME, data->points, 0, 99, NULL);

  data->expect.progress = 0.0;
  data->expect_planned = FALSE;
  data->expect.velocity = 1.1;
  data->expect.x_min = 0.0;
  data->expect.x_max = 99.0 * 1.1;
  data->expect.x_length = 99.0 * 1.1;
  data->expect.velocity_var = 0.0;
  data->expect.angle = origin[0].course;
  data->expect.angle_var = 0.0;

  data_list = g_list_append (data_list, data);

  data = g_new0 (TestData, 1);
  data->title = "Track 2 (Std. Dev. > 0)";
  data->points = waypoint_generate (0, 99, 1.1, origin[1].coord, origin[1].course, 1.7, 2.3);
  data->done_status = DONE_0;
  data->track_name = waypoint_write (db, project_name, SENSOR_NAME, data->points, 0, 99, NULL);

  data->expect.progress = 0.0;
  data->expect_planned = FALSE;
  data->expect.velocity = 1.1;
  data->expect.x_min = 0.0;
  data->expect.x_max = 99.0 * 1.1;
  data->expect.x_length = 99.0 * 1.1;
  data->expect.velocity_var = 1.7;
  data->expect.angle = origin[1].course;
  data->expect.angle_var = 2.3;

  data_list = g_list_append (data_list, data);

  /* Плановый галс, покрытый по всей длине. */
  data = g_new0 (TestData, 1);
  data->title = "Plan track, 100% done";
  data->done_status = DONE_100;
  data->points = waypoint_generate (0, 99, 1.0, origin[2].coord, origin[2].course, 0.9, 1.2);
  plan.start = data->points[1].geod;
  plan.end = data->points[98].geod;
  plan.velocity = 1.0;
  data->track_name = waypoint_write (db, project_name, SENSOR_NAME, data->points, 0, 99, &plan);

  data->expect.progress = 1.0;
  data->expect_planned = TRUE;
  data->expect.velocity = 1.0;
  data->expect.x_min = 0.0;
  data->expect.x_max = 97.0;
  data->expect.x_length = 97.0;
  data->expect.velocity_var = 0.9;
  data->expect.angle = origin[2].course;
  data->expect.angle_var = 1.2;

  data_list = g_list_append (data_list, data);

  /* Плановый галс, покрытый на половину. */
  data = g_new0 (TestData, 1);
  data->title = "Plan track, 50% done";
  data->done_status = DONE_PART;
  data->points = waypoint_generate (0, 99, 2.0, origin[2].coord, origin[2].course, 0.7, 1.4);
  plan.start = data->points[0].geod;
  plan.end = data->points[99].geod;
  plan.velocity = 1.0;
  data->track_name = waypoint_write (db, project_name, SENSOR_NAME, data->points, 0, 49, &plan);

  data->expect.progress = 0.4949; /* 98 / 198 */
  data->expect_planned = TRUE;
  data->expect.velocity = 2.0;
  data->expect.x_min = 0.0;
  data->expect.x_max = 98.0;
  data->expect.x_length = 98.0;
  data->expect.velocity_var = 0.7;
  data->expect.angle = origin[2].course;
  data->expect.angle_var = 1.4;

  data_list = g_list_append (data_list, data);

  /* Плановый галс, c покрытием 0. */
  data = g_new0 (TestData, 1);
  data->title = "Plan track, 0% done (before start)";
  data->done_status = DONE_0;
  data->points = waypoint_generate (0, 99, 2.0, origin[2].coord, origin[2].course, 0.7, 1.4);
  plan.start = data->points[50].geod;
  plan.end = data->points[99].geod;
  plan.velocity = 1.0;
  data->track_name = waypoint_write (db, project_name, SENSOR_NAME, data->points, 0, 49, &plan);

  data->expect_planned = TRUE;
  data->expect.progress = 0.0;
  data->expect.velocity = 0.0;
  data->expect.x_min = 0.0;
  data->expect.x_max = 0.0;
  data->expect.x_length = 0.0;
  data->expect.velocity_var = 0.0;
  data->expect.angle = 0.0;
  data->expect.angle_var = 0.0;

  data_list = g_list_append (data_list, data);

  /* Плановый галс, c покрытием 0. */
  data = g_new0 (TestData, 1);
  data->title = "Plan track, 0% done (after end)";
  data->done_status = DONE_0;
  data->points = waypoint_generate (0, 99, 2.0, origin[2].coord, origin[2].course, 0.7, 1.4);
  plan.start = data->points[0].geod;
  plan.end = data->points[49].geod;
  plan.velocity = 1.0;
  data->track_name = waypoint_write (db, project_name, SENSOR_NAME, data->points, 50, 99, &plan);

  data->expect_planned = TRUE;
  data->expect.progress = 0.0;
  data->expect.velocity = 0.0;
  data->expect.x_min = 0.0;
  data->expect.x_max = 0.0;
  data->expect.x_length = 0.0;
  data->expect.velocity_var = 0.0;
  data->expect.angle = 0.0;
  data->expect.angle_var = 0.0;

  data_list = g_list_append (data_list, data);

  loop = g_main_loop_new (NULL, TRUE);

  /* Инициализируем модели во время работы main loop. */
  g_idle_add ((GSourceFunc) test_start, data_list);

  /* Завершаем тест с ошибкой, если он затянулся. */
  g_timeout_add (timeout, test_failed_timeout, GINT_TO_POINTER (timeout));

  g_main_loop_run (loop);

  g_message ("Test done successfully!");

  /* Удаляем после себя проект. */
  hyscan_db_project_remove (db, project_name);

  g_list_free_full (data_list, (GDestroyNotify) test_data_free);
  g_main_loop_unref (loop);
  g_object_unref (db);

  return 0;
}
