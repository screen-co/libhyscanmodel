
#include <hyscan-data-writer.h>
#include <hyscan-db-info.h>

#include <libxml/parser.h>
#include <string.h>

#define SOURCE_DATA "0123456789"

gdouble timeout = 3.0;
guint   n_projects = 8;
guint   n_tracks = 8;

GTimer *timer = NULL;

HyScanDataWriter *writer;
HyScanDBInfo *db_info;
HyScanDB *db;

guint cur_step = 1;
guint sub_step = 1;

gint sources[] = { HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                   HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI, HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,
                   HYSCAN_SOURCE_ECHOSOUNDER, HYSCAN_SOURCE_PROFILER,
                   HYSCAN_SOURCE_LOOK_AROUND_STARBOARD, HYSCAN_SOURCE_LOOK_AROUND_PORT,
                   HYSCAN_SOURCE_FORWARD_LOOK, HYSCAN_SOURCE_SAS, HYSCAN_SOURCE_SAS_V2,
                   HYSCAN_SOURCE_NMEA_ANY, HYSCAN_SOURCE_NMEA_GGA,
                   HYSCAN_SOURCE_NMEA_RMC, HYSCAN_SOURCE_NMEA_DPT };

/* Функция создаёт канал данных. */
void
create_source (HyScanDataWriter *writer,
               HyScanSourceType  source)
{
  HyScanDataWriterData data;
  HyScanRawDataInfo raw_info;
  HyScanAcousticDataInfo acoustic_info;

  data.time = 0;
  data.size = sizeof (SOURCE_DATA);
  data.data = SOURCE_DATA;

  memset (&raw_info, 0, sizeof (raw_info));
  raw_info.data.type = HYSCAN_DATA_BLOB;
  raw_info.data.rate = 1.0;

  memset (&acoustic_info, 0, sizeof (acoustic_info));
  acoustic_info.data.type = HYSCAN_DATA_BLOB;
  acoustic_info.data.rate = 1.0;

  if (hyscan_source_is_sensor (source))
    {
      hyscan_data_writer_sensor_add_data (writer, "sensor", source, 1, &data);
    }
  else
    {
      if (source % 2)
        hyscan_data_writer_raw_add_data (writer, source, 1, &raw_info, &data);
      else
        hyscan_data_writer_acoustic_add_data (writer, source, &acoustic_info, &data);
    }
}

/* Функция проверяет канал данных. */
void
check_source (HyScanTrackInfo  *track_info,
              HyScanSourceType  source,
              gboolean          active)
{
  HyScanSourceInfo *source_info;

  source_info = g_hash_table_lookup (track_info->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    g_error ("%s: %s source error", track_info->name, hyscan_channel_get_name_by_types (source, FALSE, 1));

  if (source_info->active != active)
    g_error ("%s: %s activity error", track_info->name, hyscan_channel_get_name_by_types (source, FALSE, 1));

  if (hyscan_source_is_sensor (source))
    {
      if (!source_info->raw || source_info->computed)
        g_error ("%s: %s sensor type error", track_info->name, hyscan_channel_get_name_by_types (source, FALSE, 1));
    }
  else
    {
      if (source % 2)
        {
          if (!source_info->raw)
            g_error ("%s: %s raw type error", track_info->name, hyscan_channel_get_name_by_types (source, FALSE, 1));
        }
      else
        {
          if (!source_info->computed)
            g_error ("%s: %s computed type error", track_info->name, hyscan_channel_get_name_by_types (source, FALSE, 1));
        }
    }
}

/* Функция создаёт проекты, галсы и каналы данных для теста. */
gboolean
db_info_test (gpointer data)
{
  GMainLoop *loop = data;
  gchar *project_name;
  gchar *track_name;
  guint i;

  /* Завершаем работу, если прошли все этапы теста. */
  if (cur_step > n_projects)
    {
      g_main_loop_quit (loop);
      return FALSE;
    }

  /* На каждый этап отводится timeout секунд. */
  if (g_timer_elapsed (timer, NULL) > timeout)
    g_error ("step %d timeout", cur_step);

  /* Ждём завершения текущего этапа. */
  if (sub_step > n_tracks)
    return TRUE;

  project_name = g_strdup_printf ("project-%d", cur_step);
  track_name = g_strdup_printf ("track-%d", sub_step);

  /* Создаём галс и каналы данных в нём. */
  hyscan_data_writer_set_project (writer, project_name);
  hyscan_data_writer_set_operator_name (writer, project_name);

  if (!hyscan_data_writer_start (writer, track_name, HYSCAN_TRACK_CALIBRATION + (sub_step % 3)))
    g_error ("can't create track %s.%s", project_name, track_name);

  for (i = 0; i < sub_step; i++)
    create_source (writer, sources[i]);

  g_free (project_name);
  g_free (track_name);

  sub_step += 1;

  g_timer_start (timer);

  return TRUE;
}

/* Функция проверяем список проектов. */
void
projects_changed (HyScanDBInfo *db_info)
{
  GHashTable *projects;
  guint i;

  /* Проверяем число проектов. */
  projects = hyscan_db_info_get_projects (db_info);
  if (cur_step != g_hash_table_size (projects))
    goto exit;

  /* Проверяем текущий список проектов. */
  for (i = 1; i <= cur_step; i++)
    {
      HyScanProjectInfo *project_info;
      gchar *project_name;

      project_name = g_strdup_printf ("project-%d", i);
      project_info = g_hash_table_lookup (projects, project_name);
      if ((project_info == NULL) || (g_strcmp0 (project_info->name, project_name) != 0))
        g_error ("%s: list error", project_name);

      if (cur_step == i)
        {
          g_message ("check %s", project_name);
          hyscan_db_info_set_project (db_info, project_name);
        }

      g_free (project_name);
    }

exit:
  g_hash_table_unref (projects);
}

/* Функция проверяет список галсов и каналов данных. */
void
tracks_changed (HyScanDBInfo *info)
{
  gchar *cur_project_name;
  gchar *project_name;
  GHashTable *tracks;
  guint i, j;

  tracks = hyscan_db_info_get_tracks (info);
  cur_project_name = g_strdup_printf ("project-%d", cur_step);
  project_name = hyscan_db_info_get_project (info);

  /* Проверяем название текущего проекта. */
  if (g_strcmp0 (project_name, cur_project_name) != 0)
    goto exit;

  /* Ждём пока будут созданы все галсы. */
  if (n_tracks != g_hash_table_size (tracks))
    goto exit;

  /* Проверяем текущий список галсов. */
  for (i = 1; i <= n_tracks; i++)
    {
      HyScanTrackInfo *track_info;
      gchar *track_name;

      track_name = g_strdup_printf ("track-%d", i);
      track_info = g_hash_table_lookup (tracks, track_name);
      if ((track_info == NULL) || (g_strcmp0 (track_info->name, track_name) != 0))
        g_error ("%s: list error", track_name);

       /* Проверка флага активности записи. */
      if (track_info->active && (i != n_tracks))
        g_error ("%s: activity error", track_name);

      g_free (track_name);

      /* Ждём, если еще не все каналы данных обнаружены. */
      if (i != g_hash_table_size (track_info->sources))
        goto exit;

      /* Проверка списка источников данных. */
      for (j = 0; j < i; j++)
        check_source (track_info, sources[j], (i == n_tracks));
    }

  /* Следующий шаг теста. */
  cur_step += 1;
  sub_step = 1;

exit:
  g_hash_table_unref (tracks);
  g_free (cur_project_name);
  g_free (project_name);
}

int
main (int    argc,
      char **argv)
{
  GMainLoop *loop;
  gchar *db_uri;
  guint i;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "timeout", 'i', 0, G_OPTION_ARG_DOUBLE, &timeout, "Step timeout, s", NULL },
        { "projects", 'p', 0, G_OPTION_ARG_INT, &n_projects, "Number of projects", NULL },
        { "tracks", 't', 0, G_OPTION_ARG_INT, &n_tracks, "Number of tracks", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
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

  if (n_tracks < 1)
    n_tracks = 1;
  if (n_tracks > (sizeof (sources) / sizeof (gint)))
    n_tracks = (sizeof (sources) / sizeof (gint));

  /* Таймер операций. */
  timer = g_timer_new ();

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Объект записи данных. */
  writer = hyscan_data_writer_new (db);

  /* Информация о базе данных. */
  db_info = hyscan_db_info_new (db);

  /* Настройка Mainloop. */
  loop = g_main_loop_new (NULL, TRUE);

  g_timeout_add (100, db_info_test, loop);
  g_signal_connect (db_info, "projects-changed", G_CALLBACK (projects_changed), NULL);
  g_signal_connect (db_info, "tracks-changed", G_CALLBACK (tracks_changed), NULL);

  g_main_loop_run (loop);

  g_message ("All done");

  for (i = 1; i <= n_projects; i++)
    {
      gchar *project_name = g_strdup_printf ("project-%d", i);
      hyscan_db_project_remove (db, project_name);
      g_free (project_name);
    }

  g_timer_destroy (timer);
  g_object_unref (writer);
  g_object_unref (db_info);
  g_object_unref (db);
  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}
