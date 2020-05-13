#include <hyscan-planner-model.h>
#include <gio/gio.h>

#define PROJECT_NAME "planner-model-project"

static HyScanPlannerTrack test_tracks[7];
static HyScanPlannerOrigin test_origin = { .type = HYSCAN_PLANNER_ORIGIN, .origin = {.lat = 40, .lon = 50}, .ox = 60 };
static GMainLoop *loop;

/* Функция создает плановый галс. */
static void
make_track (HyScanPlannerTrack *track,
            const gchar        *zone_id,
            const gchar        *name,
            gdouble             lat1,
            gdouble             lon1,
            gdouble             lat2,
            gdouble             lon2)
{
  track->type = HYSCAN_PLANNER_TRACK;
  track->zone_id = (gchar *) zone_id;
  track->zone_id = (gchar *) zone_id;
  track->name = (gchar *) name;
  track->plan.start.lat = 10.0 + lat1 * 1e-4;
  track->plan.start.lon = 20.0 + lon1 * 1e-4;
  track->plan.end.lat = 10.0 + lat2 * 1e-4;
  track->plan.end.lon = 20.0 + lon2 * 1e-4;
}

/* Функция проверяет точку отсчета (что её нет). */
static void
check_no_origin (HyScanPlannerModel *model)
{
  HyScanPlannerOrigin *ref_point;

  g_message ("Checking no origin...");

  ref_point = hyscan_planner_model_get_origin (model);
  g_assert_null (ref_point);

  g_main_loop_quit (loop);
}

/* Функция проверяет точку отсчета (что она есть). */
static void
check_origin (HyScanPlannerModel *model)
{
  HyScanPlannerOrigin *ref_point;

  g_message ("Checking origin...");

  ref_point = hyscan_planner_model_get_origin (model);
  g_assert_nonnull (ref_point);

  g_assert_cmpfloat (ABS (ref_point->origin.lat - test_origin.origin.lat), <, 1e-4);
  g_assert_cmpfloat (ABS (ref_point->origin.lon - test_origin.origin.lon), <, 1e-4);
  g_assert_cmpfloat (ABS (ref_point->ox - test_origin.ox), <, 1e-4);

  hyscan_planner_origin_free (ref_point);

  g_signal_connect (model, "changed", G_CALLBACK (check_no_origin), NULL);
  g_signal_handlers_disconnect_by_func (model, check_origin, NULL);

  hyscan_planner_model_set_origin (model, NULL);
}

/* Функция устанавливает точку отсчета. */
static gboolean
assign_origin (HyScanPlannerModel *model)
{
  g_message ("Assigning origin...");

  g_signal_connect (model, "changed", G_CALLBACK (check_origin), NULL);

  g_assert_null (hyscan_planner_model_get_origin (model));
  hyscan_planner_model_set_origin (model, &test_origin);

  return G_SOURCE_REMOVE;
}

/* Функция проверяет нумерацию галсов. */
static void
check_number (HyScanPlannerModel *model)
{
  GHashTable *tracks;
  GHashTableIter iter;
  gchar *id;
  HyScanPlannerTrack *track;
  guint hits = 0;

  g_message ("Checking number...");

  tracks = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (model));
  if (g_hash_table_size (tracks) != G_N_ELEMENTS (test_tracks))
    goto exit;

  g_hash_table_iter_init (&iter, tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &track))
    {
      /* Проверяем, что галсы "1" - "3" пронумеровались и имеют правильные номера. */
      if ((g_strcmp0 (track->name, "1") == 0 && track->number == 1) ||
          (g_strcmp0 (track->name, "2") == 0 && track->number == 2) ||
          (g_strcmp0 (track->name, "3") == 0 && track->number == 3))
        {
          hits++;
          continue;
        }

      /* В любом другом случае галс будет без номера. */
      if (track->number != 0)
        g_error ("Track <%s> must not have number %d", track->name, track->number);
    }

  /* Если вдруг в БД еще не все обновилось, тогда ждем следующего сигнала "changed". */
  if (hits != 3)
    goto exit;

  g_signal_handlers_disconnect_by_func (model, check_number, NULL);
  g_idle_add ((GSourceFunc) assign_origin, model);

exit:
  g_hash_table_unref (tracks);
}

/* Функция нумерует плановые галсы. */
static void
assign_number (HyScanPlannerModel *model)
{
  GHashTable *tracks;
  GHashTableIter iter;
  gchar *id, *track0_id = NULL;
  HyScanPlannerTrack *track;

  g_message ("Assigning number...");

  tracks = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (model));
  if (g_hash_table_size (tracks) != G_N_ELEMENTS (test_tracks))
    goto exit;

  g_hash_table_iter_init (&iter, tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &track) && track0_id == NULL)
    {
      if (g_strcmp0 (track->name, "1") == 0)
        track0_id = id;
    }

  if (track0_id == NULL)
    goto exit;

  hyscan_planner_model_assign_number (model, track0_id);

  /* Переходим к следующему шагу. */
  g_signal_handlers_disconnect_by_func (model, assign_number, NULL);
  g_signal_connect (model, "changed", G_CALLBACK (check_number), NULL);

exit:
  g_hash_table_unref (tracks);
}

/* Функция заполняет БД плановыми галсами. */
static gboolean
append_tracks (HyScanPlannerModel *model)
{
  guint i;

  g_message ("Appending tracks...");

  g_signal_connect (model, "changed", G_CALLBACK (assign_number), NULL);

  make_track (&test_tracks[0], "zone1", "1", 0.0, 0.0, 0.0, 1.0);
  make_track (&test_tracks[1], "zone1", "2", 1.0, 0.0, 1.0, 1.0);
  make_track (&test_tracks[2], "zone1", "3", 2.0, 0.0, 2.0, 1.0);

  make_track (&test_tracks[3], "zone2", "4", 4.0, 0.0, 4.0, 1.0);
  make_track (&test_tracks[4], "zone2", "5", 5.0, 0.0, 5.0, 1.0);
  make_track (&test_tracks[5], "zone2", "6", 6.0, 0.0, 6.0, 1.0);

  make_track (&test_tracks[6], NULL, NULL, 6.0, 0.0, 6.0, 1.0);

  for (i = 0; i < G_N_ELEMENTS (test_tracks); i++)
    hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (model), (const HyScanObject *) &test_tracks[i]);

  return G_SOURCE_REMOVE;
}

/* Функция фейлит тест, если он слишком затянулся. */
static gboolean
fail_test (gpointer user_data)
{
  g_error ("Test failed by timeout");

  return G_SOURCE_REMOVE;
}

/* Функция создаёт проект. */
static gchar *
create_project (HyScanDB *db)
{
  GBytes *project_schema;
  GDateTime *date_time;
  gint32 project_id;
  gchar *project_name;

  project_schema = g_resources_lookup_data ("/org/hyscan/schemas/project-schema.xml",
                                            G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  g_assert (project_schema != NULL);

  /* Создаём проект. */
  date_time = g_date_time_new_now_local ();
  project_name = g_strdup_printf (PROJECT_NAME"-%ld", g_date_time_to_unix (date_time));
  project_id = hyscan_db_project_create (db, project_name, g_bytes_get_data (project_schema, NULL));
  g_assert (project_id > 0);

  g_bytes_unref (project_schema);
  g_date_time_unref (date_time);
  hyscan_db_close (db, project_id);

  return project_name;
}

int
main (int    argc,
      char **argv)
{
  HyScanPlannerModel *model;
  HyScanDB *db;

  gchar *db_uri;
  gchar *project_name;

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
  project_name = create_project (db);

  model = hyscan_planner_model_new ();
  hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (model), db, project_name);

  loop = g_main_loop_new (NULL, TRUE);

  g_idle_add ((GSourceFunc) append_tracks, model);
  g_timeout_add (10000, fail_test, NULL);

  g_main_loop_run (loop);
  g_object_unref (model);

  /* Удаляем после себя проект. */
  hyscan_db_project_remove (db, project_name);

  g_print ("Test done successfully!\n");

  g_free (project_name);
  g_object_unref (db);
  g_free (db_uri);

  return 0;
}
