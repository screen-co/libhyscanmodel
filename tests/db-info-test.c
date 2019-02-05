/* db-info-test.c
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanModel имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanModel на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

#include <hyscan-data-writer.h>
#include <hyscan-db-info.h>

#include <libxml/parser.h>
#include <string.h>

#define USEC_PER_DAY  86400000000L
#define SONAR_INFO    "<schemalist><schema id=\"info\"><key id=\"id\" name=\"ID\" type=\"integer\"/></schema></schemalist>"
#define SOURCE_DATA   "0123456789"

gdouble timeout = 3.0;
guint   n_projects = 8;
guint   n_tracks = 8;

GTimer *timer = NULL;

HyScanDataWriter *writer;
HyScanDBInfo *db_info;
HyScanDB *db;

guint cur_step = 1;
guint sub_step = 1;
gint64 start_time = 0;

gint sources[] = { HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                   HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI, HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,
                   HYSCAN_SOURCE_ECHOSOUNDER, HYSCAN_SOURCE_PROFILER,
                   HYSCAN_SOURCE_LOOK_AROUND_STARBOARD, HYSCAN_SOURCE_LOOK_AROUND_PORT,
                   HYSCAN_SOURCE_FORWARD_LOOK, HYSCAN_SOURCE_SAS, HYSCAN_SOURCE_NMEA };

/* Функция создаёт канал данных. */
void
create_source (HyScanDataWriter *writer,
               HyScanSourceType  source)
{
  HyScanAcousticDataInfo info;
  HyScanBuffer *data;

  memset (&info, 0, sizeof (info));
  info.data_type = HYSCAN_DATA_BLOB;
  info.data_rate = 1.0;

  data = hyscan_buffer_new ();
  hyscan_buffer_wrap_data (data, HYSCAN_DATA_BLOB, SOURCE_DATA, sizeof (SOURCE_DATA));

  if (hyscan_source_is_sensor (source))
    {
      hyscan_data_writer_sensor_add_data (writer, "sensor", source, 1, 0, data);
    }
  else
    {
      hyscan_data_writer_acoustic_add_data (writer, source, 1, FALSE, 0, &info, data);
    }
}

/* Функция создаёт проекты, галсы и каналы данных для теста. */
gboolean
db_info_test (gpointer data)
{
  GMainLoop *loop = data;
  gchar *project_name;
  gchar *track_name;
  gint64 ctime;
  gint64 mtime;
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
  ctime = start_time + ((cur_step * n_tracks + sub_step) * USEC_PER_DAY);

  /* Создаём галс и каналы данных в нём. */
  hyscan_data_writer_set_operator_name (writer, project_name);
  if (!hyscan_data_writer_start (writer, project_name, track_name, HYSCAN_TRACK_CALIBRATION + (sub_step % 3), ctime))
    g_error ("can't create track %s.%s", project_name, track_name);

  /* Время изменения проекта. */




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

      /* Проверяем что информация о проекте считана правильно. */
      if (project_info->error)
        g_error ("%s: info error", project_info->name);

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
      guint n_sources;

      track_name = g_strdup_printf ("track-%d", i);
      track_info = g_hash_table_lookup (tracks, track_name);
      if ((track_info == NULL) || (g_strcmp0 (track_info->name, track_name) != 0))
        g_error ("%s: list error", track_name);
      g_free (track_name);

      /* Проверяем что информация о галсе считана правильно. */
      if (track_info->error)
        g_error ("%s: info error", track_info->name);

       /* Проверка флага активности записи. */
      if (track_info->active && (i != n_tracks))
        g_error ("%s: activity error", track_info->name);

      /* Ждём, если еще не все каналы данных обнаружены. */
      for (j = 0, n_sources = 0; j < HYSCAN_SOURCE_LAST; j++)
        n_sources += (track_info->sources[j] ? 1 : 0);
      if (n_sources != i)
        goto exit;

      /* Проверка имени оператора. */
      if (g_strcmp0 (track_info->operator_name, project_name) != 0)
        g_error ("%s: operator name error", track_info->name);

      /* Проверка информации о гидролокаторе. */
      if (!hyscan_data_schema_has_key (track_info->sonar_info, "/id"))
        g_error ("%s: sonar info error", track_info->name);

      /* Проверка списка источников данных. */
      for (j = 0; j < i; j++)
        {
          if (!track_info->sources[sources[j]])
            g_error ("%s: %s source error", track_info->name, hyscan_channel_get_name_by_types (sources[j], FALSE, 1));
        }
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

  /* Дата и время запуска. */
  start_time = g_get_real_time ();
  start_time /= USEC_PER_DAY;
  start_time *= USEC_PER_DAY;

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Объект записи данных. */
  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);
  hyscan_data_writer_set_sonar_info (writer, SONAR_INFO);

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

  return 0;
}
