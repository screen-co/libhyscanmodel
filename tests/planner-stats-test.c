#include "waypoint.h"
#include <hyscan-cached.h>
#include <hyscan-planner-stats.h>
#include <hyscan-data-writer.h>

#define PROJECT_NAME  "planner-stats-project"
#define SENSOR_NAME   "gnss-nmea"
#define TEST_TIMEOUT  30000

static gchar *project_name;
static GMainLoop *loop;

static void *db;
static HyScanPlannerStats *stats;
static HyScanObjectModel *model;

static gchar *test_zone_id;
static gchar *test_track1_id;
static gchar *test_track2_id;

static HyScanTrackPlan plan[] =
{
  { .start = { 50.0, 30.0 }, .end = { 50.008, 30.008 }, .speed = 1.0 },
  { .start = { 50.1, 30.1 }, .end = { 50.108, 30.108 }, .speed = 1.5 },
};

static gboolean   add_zone         (gpointer     user_data);
static void       add_track_record (const gchar *plan_id,
                                    gdouble      start_progress,
                                    gdouble      end_progress);

/* Функция фейлит тест, если он слишком затянулся. */
static gboolean
fail_test (gpointer user_data)
{
  g_error ("Test failed by timeout");

  return G_SOURCE_REMOVE;
}

/* Функция создаёт проект. */
static gchar *
create_project (void)
{
  GDateTime *date_time;
  gchar *project;
  HyScanDataWriter *writer;

  /* Создаём проект. */
  date_time = g_date_time_new_now_local ();
  project = g_strdup_printf (PROJECT_NAME"-%ld", g_date_time_to_unix (date_time));

  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);
  hyscan_data_writer_start (writer, project, project, HYSCAN_TRACK_SURVEY, NULL, -1);
  hyscan_data_writer_stop (writer);

  g_object_unref (writer);
  g_date_time_unref (date_time);

  return project;
}

/* Функция записывает в базу данных галс, который покрывает track_plan от
 * start_progress до end_progress. */
static void
add_track_record (const gchar *plan_id,
                  gdouble      start_progress,
                  gdouble      end_progress)
{
  HyScanGeo *geo;
  WayPoint *waypoints;
  HyScanPlannerTrack *plan_track;
  HyScanTrackInfo *track_info;
  HyScanTrackPlan *plan_params;
  HyScanGeoPoint start;
  gdouble course;
  gdouble length;
  gchar *track_name;
  gint start_idx, end_idx;
  gint32 project_id;

  plan_track = (HyScanPlannerTrack *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (model), HYSCAN_TYPE_PLANNER_TRACK, plan_id);
  plan_params = &plan_track->plan;

  /* Создаем путевые точки и пишем их в БД. */
  start = plan_params->start;
  geo = hyscan_planner_track_geo (plan_params, &course);
  length = hyscan_planner_track_length (plan_params);
  start_idx = length / plan_params->speed * start_progress;
  end_idx = length / plan_params->speed * end_progress;
  waypoints = waypoint_generate (start_idx, end_idx, plan_params->speed, start, course, 0, 0);

  track_name = waypoint_write (db, project_name, SENSOR_NAME, waypoints, 0, end_idx - start_idx, plan_params);
  g_free (waypoints);

  project_id = hyscan_db_project_open (db, project_name);
  track_info = hyscan_db_info_get_track_info (db, project_id, track_name);
  hyscan_db_close (db, project_id);

  /* Добавляем к плановому галсу запись. */
  hyscan_planner_track_record_append (plan_track, track_info->id);
  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (model), plan_id, (const HyScanObject *) plan_track);

  hyscan_db_info_track_info_free (track_info);
  hyscan_planner_track_free (plan_track);
  g_free (track_name);
  g_object_unref (geo);
}

/* Функция проверяет прогресс выполнения. */
static void
check_progress_5 (void)
{
  GHashTable *zones;
  GHashTableIter iter;
  HyScanPlannerStatsZone *stats_zone;
  HyScanPlannerStatsTrack *stats_track2;

  g_message ("Expected progress: track #2 - 80%%");

  zones = hyscan_planner_stats_get (stats);
  if (g_hash_table_size (zones) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, zones);
  if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &stats_zone))
    goto exit;

  stats_track2 = g_hash_table_lookup (stats_zone->tracks, test_track2_id);
  g_message ("Progress %f", stats_track2->progress);
  if (stats_track2->progress < 0.7)
    goto exit;

  /* Покрытие галса теперь стало 80%. */
  g_assert_cmpfloat (ABS (stats_track2->progress - 0.8), <, 0.05);

  g_signal_handlers_disconnect_by_func (stats, G_CALLBACK (check_progress_5), NULL);
  g_main_loop_quit (loop);

exit:
  g_hash_table_destroy (zones);
}

/* Функция записывает галс, так что плановый галс должен стать полностью покрытым. */
static gboolean
add_record_4 (void)
{
  g_message ("Add track #2 record: [85%%, 95%%]");
  add_track_record (test_track2_id, 0.85, 0.95);
  g_signal_connect (stats, "changed", G_CALLBACK (check_progress_5), NULL);

  return G_SOURCE_REMOVE;
}

static void
check_progress_4 (void)
{
  GHashTable *zones;
  GHashTableIter iter;
  HyScanPlannerStatsZone *stats_zone;
  HyScanPlannerStatsTrack *stats_track2;

  g_message ("Expected progress: track #2 - 70%%");

  zones = hyscan_planner_stats_get (stats);
  if (g_hash_table_size (zones) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, zones);
  if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &stats_zone))
    goto exit;

  stats_track2 = g_hash_table_lookup (stats_zone->tracks, test_track2_id);
  g_message ("Progress %f", stats_track2->progress);
  if (stats_track2->progress < 0.6)
    goto exit;

  /* Покрытие галса теперь стало 70%. */
  g_assert_cmpfloat (ABS (stats_track2->progress - 0.7), <, 0.05);

  g_signal_handlers_disconnect_by_func (stats, G_CALLBACK (check_progress_4), NULL);
  g_idle_add ((GSourceFunc) add_record_4, NULL);

exit:
  g_hash_table_destroy (zones);
}

/* Функция записывает галс, так что плановый галс должен стать полностью покрытым. */
static gboolean
add_record_3 (void)
{
  g_message ("Add track #2 record: [40%%, 70%%]");
  add_track_record (test_track2_id, 0.4, 0.7);
  g_signal_connect (stats, "changed", G_CALLBACK (check_progress_4), NULL);

  return G_SOURCE_REMOVE;
}

/* Функция проверяет прогресс выполнения. */
static void
check_progress_3 (void)
{
  GHashTable *zones;
  GHashTableIter iter;
  HyScanPlannerStatsZone *stats_zone;
  HyScanPlannerStatsTrack *stats_track1, *stats_track2;

  g_message ("Expected progress: track #1 - 100%%, track #2 - 50%%, overall - 75%%");

  zones = hyscan_planner_stats_get (stats);
  if (g_hash_table_size (zones) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, zones);
  if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &stats_zone))
    goto exit;

  stats_track1 = g_hash_table_lookup (stats_zone->tracks, test_track1_id);
  stats_track2 = g_hash_table_lookup (stats_zone->tracks, test_track2_id);
  if (stats_track1 == NULL || stats_track1->progress == 0.0 ||
      stats_track2 == NULL || stats_track2->progress == 0.0)
  {
    goto exit;
  }

  /* Покрытие одного галса - 100%, другого - 50%; в сумме - 75%. */
  g_assert_cmpfloat (ABS (stats_track1->progress - 1.00), <, 0.05);
  g_assert_cmpfloat (ABS (stats_track2->progress - 0.50), <, 0.05);
  g_assert_cmpfloat (ABS (stats_zone->progress   - 0.75), <, 0.05);

  g_signal_handlers_disconnect_by_func (stats, G_CALLBACK (check_progress_3), NULL);
  g_idle_add ((GSourceFunc) add_record_3, NULL);

exit:
  g_hash_table_destroy (zones);
}

/* Функция записывает галс, так что плановый галс #2 должен стать покрытым на 50%. */
static gboolean
add_record_2 (void)
{
  g_message ("Add track #2 record: [0%%, 50%%]");

  add_track_record (test_track2_id, 0.0, 0.5);
  g_signal_connect (stats, "changed", G_CALLBACK (check_progress_3), NULL);

  return G_SOURCE_REMOVE;
}

static void
check_progress_2 (void)
{
  GHashTable *zones;
  GHashTableIter iter;
  HyScanPlannerStatsZone *stats_zone;
  HyScanPlannerStatsTrack *stats_track;
  gchar *track_id;

  g_message ("Expected progress: track #1 - 100%%, track #2 - 0%%, overall - 50%%");

  zones = hyscan_planner_stats_get (stats);
  if (g_hash_table_size (zones) != 1)
    return;

  g_hash_table_iter_init (&iter, zones);
  if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &stats_zone))
    return;

  if (g_hash_table_size (stats_zone->tracks) != 2)
    return;

  g_hash_table_iter_init (&iter, stats_zone->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &track_id, (gpointer *) &stats_track))
    {
      if (g_strcmp0 (track_id, test_track1_id) == 0)
        g_assert_cmpfloat (stats_track->progress, ==, 1.0);
      else
        {
          test_track2_id = g_strdup (track_id);
          g_assert_cmpfloat (stats_track->progress, ==, 0.0);
        }
    }

  g_assert_cmpfloat (stats_zone->progress - 0.5, <, 1e-3);
  g_signal_handlers_disconnect_by_func (stats, G_CALLBACK (check_progress_2), NULL);

  g_idle_add ((GSourceFunc) add_record_2, NULL);

  g_hash_table_destroy (zones);
}

/* Функция дополняет схему галсов еще одним плановым галсом #2. */
static gboolean
add_track_2 (void)
{
  HyScanPlannerTrack track = { .type = HYSCAN_TYPE_PLANNER_TRACK };

  g_message ("Add track #2");

  track.zone_id = test_zone_id;
  track.plan = plan[1];
  hyscan_object_store_add (HYSCAN_OBJECT_STORE (model), (const HyScanObject *) &track, NULL);

  g_signal_connect (stats, "changed", G_CALLBACK (check_progress_2), NULL);

  return G_SOURCE_REMOVE;
}

/* Функция проверяет, что в схеме галсов одна зона и один галс, а прогресс выполнения  - 100%. */
static void
check_progress_1 (void)
{
  GHashTable *zones;
  GHashTableIter iter;
  HyScanPlannerStatsZone *stats_zone;
  HyScanPlannerStatsTrack *stats_track;
  gchar *key;

  g_message ("Expected progress: track #1 - 100%%, track #2 - 0%%, overall - 50%%");

  zones = hyscan_planner_stats_get (stats);
  if (g_hash_table_size (zones) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, zones);
  if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &stats_zone))
    goto exit;

  if (g_hash_table_size (stats_zone->tracks) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, stats_zone->tracks);
  if (!g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &stats_track))
    goto exit;

  g_message ("Progress: %f (%f)", stats_track->progress, stats_zone->progress);
  if (stats_track->progress == 0.0 || stats_zone->progress == 0.0)
    goto exit;

  g_assert_cmpfloat (stats_track->progress, ==, 1.0);
  g_assert_cmpfloat (stats_zone->progress, ==, 1.0);

  g_signal_handlers_disconnect_by_func (stats, G_CALLBACK (check_progress_1), NULL);
  g_idle_add ((GSourceFunc) add_track_2, NULL);

exit:
  g_hash_table_destroy (zones);
}

/* Функция записывает галс, так что плановый галс #1 должен стать полностью покрытым. */
static gboolean
add_record (void)
{
  g_message ("Add track #1 record: [-10%%, 110%%]");

  add_track_record (test_track1_id, -.1, 1.1);
  g_signal_connect (stats, "changed", G_CALLBACK (check_progress_1), NULL);

  return G_SOURCE_REMOVE;
}

/* Функция проверяет, что в схеме галсов одна зона и один галс, а прогресс выполнения - 0%. */
static void
check_progress_0 (void)
{
  GHashTable *zones;
  GHashTableIter iter;
  HyScanPlannerStatsZone *stats_zone;
  HyScanPlannerStatsZone *stats_track;
  gchar *key;

  g_message ("Expected progress: track #1 - 0%%, overall - 0%%");

  zones = hyscan_planner_stats_get (stats);
  if (g_hash_table_size (zones) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, zones);
  if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) &stats_zone))
    goto exit;

  if (g_hash_table_size (stats_zone->tracks) != 1)
    goto exit;

  g_hash_table_iter_init (&iter, stats_zone->tracks);
  if (!g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &stats_track))
    goto exit;

  test_track1_id = g_strdup (key);

  g_assert_cmpfloat (stats_track->progress, ==, 0.0);
  g_assert_cmpfloat (stats_zone->progress, ==, 0.0);

  g_signal_handlers_disconnect_by_func (stats, G_CALLBACK (check_progress_0), NULL);

  g_idle_add ((GSourceFunc) add_record, NULL);

exit:
  g_hash_table_destroy (zones);
}

/* Функция добавляет в зону первый галс. */
static void
add_track_1 (void)
{
  GHashTable *objects;
  GHashTableIter iter;
  gchar *key;
  HyScanPlannerTrack track = { .type = HYSCAN_TYPE_PLANNER_TRACK };

  objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (model), HYSCAN_TYPE_PLANNER_TRACK);
  g_hash_table_iter_init (&iter, objects);

  if (!g_hash_table_iter_next (&iter, (gpointer *) &key, NULL))
    return;

  test_zone_id = g_strdup (key);

  g_message ("Add track #1 into zone <%s>", test_zone_id);

  track.zone_id = test_zone_id;
  track.plan = plan[0];
  hyscan_object_store_add (HYSCAN_OBJECT_STORE (model), (const HyScanObject *) &track, NULL);

  g_signal_handlers_disconnect_by_func (model, G_CALLBACK (add_track_1), NULL);
  g_signal_connect (stats, "changed", G_CALLBACK (check_progress_0), NULL);

  g_hash_table_destroy (objects);
}

static gboolean
add_zone (gpointer user_data)
{
  HyScanPlannerZone zone = { .type = HYSCAN_TYPE_PLANNER_ZONE };

  /* Добавляем зону, галсы, записи галсов. */
  g_message ("Add zone");
  hyscan_object_store_add (HYSCAN_OBJECT_STORE (model), (const HyScanObject *) &zone, NULL);
  g_signal_connect (model, "changed", G_CALLBACK (add_track_1), NULL);

  return G_SOURCE_REMOVE;
}

int
main (int    argc,
      char **argv)
{
  HyScanTrackStats *track_stats;
  HyScanDBInfo *db_info;
  HyScanCache *cache;

  gchar *db_uri;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return -1;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't open db: %s", db_uri);

  /* Проводим тесты. */
  project_name = create_project ();

  model = HYSCAN_OBJECT_MODEL (hyscan_planner_model_new ());
  cache = HYSCAN_CACHE (hyscan_cached_new (100));
  db_info = hyscan_db_info_new (db);
  track_stats = hyscan_track_stats_new (db_info, HYSCAN_PLANNER_MODEL (model), cache);
  stats = hyscan_planner_stats_new (track_stats, HYSCAN_PLANNER_MODEL (model));

  hyscan_object_model_set_project (model, db, project_name);
  hyscan_db_info_set_project (db_info, project_name);

  loop = g_main_loop_new (NULL, TRUE);

  g_idle_add ((GSourceFunc) add_zone, model);
  g_timeout_add (TEST_TIMEOUT, fail_test, NULL);

  g_main_loop_run (loop);

  /* Удаляем после себя проект. */
  hyscan_db_project_remove (db, project_name);

  g_print ("Test done successfully!\n");

  g_object_unref (track_stats);
  g_object_unref (db_info);
  g_object_unref (cache);
  g_object_unref (model);
  g_object_unref (stats);

  g_free (project_name);
  g_object_unref (db);
  g_free (db_uri);

  return 0;
}
