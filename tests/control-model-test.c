/* control-model-test.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#include <hyscan-control-model.h>
#include <hyscan-dummy-device.h>

#define TEST_SOURCE HYSCAN_SOURCE_SIDE_SCAN_PORT           /* Источник данных. */
#define SYNC_PERIOD (100 * G_TIME_SPAN_MILLISECOND / 1000) /* Период синхронизации параметров ГЛ. */
#define WAIT_PERIOD (SYNC_PERIOD * 4)                      /* Период ожидания синхронизации ГЛ. */
#define FAIL_TIMEOUT (3 * G_TIME_SPAN_SECOND)              /* Время ожидания до завершения по таймауту. */

/* Параметры старта локатора. */
#define PROJECT_NAME   "test-project"
#define TRACK_SUFFIX   "-test-suffix"
#define TRACK_NAME     "001"
#define TRACK_TYPE     HYSCAN_TRACK_SURVEY
static HyScanTrackPlan track_plan = { .start = {1, 2}, .end = {3, 4}, .speed = 5 };

static HyScanControlModel *sonar_model;
static HyScanControl *control;
static HyScanDummyDevice *dummy_device;
static gulong start_stop_tag;
static guint timeout_tag;
static GMainLoop *loop;

static gboolean test_sonar                 (gpointer               data);
static gboolean test_sensor                (gpointer               data);
static gboolean test_start                 (gpointer               data);
static gboolean test_param                 (gpointer               data);
static gboolean test_atvg_sync             (gpointer               data);
static gboolean test_sync_failed           (gpointer               data);
static gboolean test_sync_happened         (gpointer               data);
static void     test_stopped               (void);
static void     test_started               (void);
static void     test_control_model_started (void);
static gboolean track_plan_equal           (const HyScanTrackPlan *plan1,
                                            const HyScanTrackPlan *plan2);
static gboolean fail_by_timeout            (gpointer               data);
static void     check_params               (HyScanParam           *param,
                                            HyScanParamList       *expect_list);


/* Сравнивает значения двух HyScanParam. */
static void
check_params (HyScanParam     *param,
              HyScanParamList *expect_list)
{
  HyScanParamList *read_plist;
  const gchar *const *params;
  gint i;

  read_plist = hyscan_param_list_new ();

  /* Создаём список параметров без значений и считываем в него. */
  params = hyscan_param_list_params (expect_list);
  for (i = 0; params[i] != NULL; i++)
    hyscan_param_list_add (read_plist, params[i]);

  if (!hyscan_param_get (HYSCAN_PARAM (param), read_plist))
    g_error ("Failed to get params");

  /* Проверяем все параметры. */
  for (i = 0; params[i] != NULL; i++)
    {
      gint expected, real;

      expected = hyscan_param_list_get_integer (expect_list, params[i]);
      real = hyscan_param_list_get_integer (read_plist, params[i]);
      if (real != expected)
        g_error ("Param %s has not been set correctly (%d != %d)", params[i], real, expected);
    }

  g_object_unref (read_plist);
}

/* Тест интерфейса HyScanParam. */
static gboolean
test_param (gpointer data)
{
  HyScanParamList *after, *before;
  gchar *param_key, *system_key;
  const gchar *prefix;

  prefix = hyscan_dummy_device_get_id (dummy_device);
  system_key = g_strdup_printf ("/system/%s/id", prefix);
  param_key = g_strdup_printf ("/params/%s/id", prefix);

  /* Старые значения параметров. */
  before = hyscan_param_list_new ();
  hyscan_param_list_add (before, system_key);
  hyscan_param_list_add (before, param_key);
  if (!hyscan_param_get (HYSCAN_PARAM (dummy_device), before))
    g_error ("Failed to read params before set");

  /* Новые значения параметров. */
  after = hyscan_param_list_new ();
  hyscan_param_list_set_integer (after, param_key, 2);
  hyscan_param_list_set_integer (after, system_key, 3);
  if (!hyscan_param_set (HYSCAN_PARAM (sonar_model), after))
    g_error ("Failed to set params");

  /* Проверяем, что параметры применились. */
  g_message ("Test param values after set");
  check_params (HYSCAN_PARAM (dummy_device), after);

  g_free (system_key);
  g_free (param_key);
  g_object_unref (after);
  g_object_unref (before);

  g_idle_add ((GSourceFunc) test_start, NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
test_sensor (gpointer data)
{
  HyScanSensor *sensor = HYSCAN_SENSOR (sonar_model);
  HyScanSensorState *state = HYSCAN_SENSOR_STATE (sonar_model);
  const gchar *const *sensors;
  gint i;

  sensors = hyscan_control_sensors_list (control);
  if (sensors == NULL || sensors[0] == NULL)
    g_error ("No sensors found");

  for (i = 0; sensors[i] != NULL; i++)
    {
      g_message ("Test sensor %s", sensors[i]);

      if (hyscan_sensor_state_get_enabled (state, sensors[i]))
        g_error ("Sensor %s state must be FALSE", sensors[i]);

      if (!hyscan_sensor_set_enable (sensor, sensors[i], TRUE))
        g_error ("Failed to enable sensor %s", sensors[i]);

      if (!hyscan_dummy_device_check_sensor_enable (dummy_device, sensors[i]))
        g_error ("Enable sensor %s check failed", sensors[i]);

      if (!hyscan_sensor_state_get_enabled (state, sensors[i]))
        g_error ("Sensor %s state must be TRUE", sensors[i]);
    }

  g_idle_add ((GSourceFunc) test_sonar, NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
test_sync_failed (gpointer data)
{
  /* Проверяем, что состояние ещё не синхронизировано. */
  if (hyscan_dummy_device_check_sync (dummy_device))
    g_error ("Sync has happened too early");

  g_timeout_add (WAIT_PERIOD, (GSourceFunc) test_sync_happened, NULL);

  return G_SOURCE_REMOVE;
}

/* Шаги проверки локатора и его состояния. */
#define TEST_SONAR(pm, set, check, get, test)       \
  G_STMT_START {                                    \
    g_message ("Test sonar param: %s", pm);         \
    if (!(set))                                     \
      g_error ("Failed to set %s", (pm));           \
    if (!(check))                                   \
      g_error ("Failed to check %s", (pm));         \
    if (!(get))                                     \
      g_error ("Failed to get state %s", (pm));     \
    if (!(test))                                    \
      g_error ("State of %s is incorrect", (pm));   \
  } G_STMT_END

static gboolean
test_sync_happened (gpointer data)
{
  HyScanSonar *sonar = HYSCAN_SONAR (sonar_model);
  HyScanSonarState *state = HYSCAN_SONAR_STATE (sonar_model);
  HyScanAntennaOffset offset = { 1, 2, 3, 4, 5, 6 };
  gdouble val1, val2;
  gint64 ival1;

  /* Теперь должно быть синхронизировано. */
  if (!hyscan_dummy_device_check_sync (dummy_device))
    g_error ("Background sync has not happened");

  /* Проверяем остальные параметры. */
  TEST_SONAR ("antenna offset",
              hyscan_sonar_antenna_set_offset (sonar, TEST_SOURCE, &offset),
              hyscan_dummy_device_check_antenna_offset (dummy_device, &offset),
              TRUE,
              TRUE);

  TEST_SONAR ("receiver time",
              hyscan_sonar_receiver_set_time (sonar, TEST_SOURCE, 1.0, 2.0),
              hyscan_dummy_device_check_receiver_time (dummy_device, 1.0, 2.0),
              hyscan_sonar_state_receiver_get_time (state, TEST_SOURCE, &val1, &val2),
              hyscan_sonar_state_receiver_get_mode (state, TEST_SOURCE) == HYSCAN_SONAR_RECEIVER_MODE_MANUAL &&
              val1 == 1.0 && val2 == 2.0);

  TEST_SONAR ("receiver auto",
              hyscan_sonar_receiver_set_auto (sonar, TEST_SOURCE),
              hyscan_dummy_device_check_receiver_auto (dummy_device),
              TRUE,
              hyscan_sonar_state_receiver_get_mode (state, TEST_SOURCE) == HYSCAN_SONAR_RECEIVER_MODE_AUTO);

  TEST_SONAR ("receiver disable",
              hyscan_sonar_receiver_disable (sonar, TEST_SOURCE),
              hyscan_dummy_device_check_receiver_disable (dummy_device),
              TRUE,
              hyscan_sonar_state_receiver_get_disabled (state, TEST_SOURCE));

  TEST_SONAR ("generator preset",
              hyscan_sonar_generator_set_preset (sonar, TEST_SOURCE, 3),
              hyscan_dummy_device_check_generator_preset (dummy_device, 3),
              hyscan_sonar_state_generator_get_preset (state, TEST_SOURCE, &ival1),
              ival1 == 3);

  TEST_SONAR ("generator disable",
              hyscan_sonar_generator_disable (sonar, TEST_SOURCE),
              hyscan_dummy_device_check_generator_disable (dummy_device),
              TRUE,
              hyscan_sonar_state_generator_get_disabled (state, TEST_SOURCE));

  g_timeout_add (WAIT_PERIOD, (GSourceFunc) test_atvg_sync, NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
test_atvg_sync (gpointer data)
{
  HyScanSonar *sonar = HYSCAN_SONAR (sonar_model);
  static guint iteration = 0;

  gdouble level[] = { 0.1, 0.1, 0.3 };
  gdouble sensitivity[] = { 0.1, 0.2, 0.2 };

  if (iteration > 0 && !hyscan_dummy_device_check_sync (dummy_device))
    g_error ("Background sync has not happened");

  iteration++;
  if (iteration < G_N_ELEMENTS (level))
    {
      hyscan_sonar_tvg_set_auto (sonar, TEST_SOURCE, level[iteration], sensitivity[iteration]);
      return G_SOURCE_CONTINUE;
    }

  g_idle_add ((GSourceFunc) test_param, NULL);
  return G_SOURCE_REMOVE;
}

static gboolean
test_sonar (gpointer data)
{
  HyScanSonar *sonar = HYSCAN_SONAR (sonar_model);
  HyScanSonarState *state = HYSCAN_SONAR_STATE (sonar_model);
  gdouble val1, val2, val3;

  TEST_SONAR ("tvg constant",
              hyscan_sonar_tvg_set_constant (sonar, TEST_SOURCE, 1.0),
              hyscan_dummy_device_check_tvg_constant (dummy_device, 1.0),
              hyscan_sonar_state_tvg_get_constant (state, TEST_SOURCE, &val1),
              val1 == 1.0);

  TEST_SONAR ("tvg linear db",
              hyscan_sonar_tvg_set_linear_db (sonar, TEST_SOURCE, 1.0, 0.1),
              hyscan_dummy_device_check_tvg_linear_db (dummy_device, 1.0, 0.1),
              hyscan_sonar_state_tvg_get_linear_db (state, TEST_SOURCE, &val1, &val2),
              val1 == 1.0 && val2 == 0.1);

  TEST_SONAR ("tvg logarithmic",
              hyscan_sonar_tvg_set_logarithmic (sonar, TEST_SOURCE, 1.0, 0.1, 0.2),
              hyscan_dummy_device_check_tvg_logarithmic (dummy_device, 1.0, 0.1, 0.2),
              hyscan_sonar_state_tvg_get_logarithmic (state, TEST_SOURCE, &val1, &val2, &val3),
              val1 == 1.0 && val2 == 0.1 && val3 == 0.2);

  TEST_SONAR ("tvg disable",
              hyscan_sonar_tvg_disable (sonar, TEST_SOURCE),
              hyscan_dummy_device_check_tvg_disable (dummy_device),
              TRUE,
              hyscan_sonar_state_tvg_get_disabled (state, TEST_SOURCE));

  TEST_SONAR ("tvg auto",
              hyscan_sonar_tvg_set_auto (sonar, TEST_SOURCE, 1.0, 0.1),
              hyscan_dummy_device_check_tvg_auto (dummy_device, 1.0, 0.1),
              hyscan_sonar_state_tvg_get_auto (state, TEST_SOURCE, &val1, &val2),
              val1 == 1.0 && val2 == 0.1);

  g_timeout_add (SYNC_PERIOD / 2, (GSourceFunc) test_sync_failed, NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
fail_by_timeout (gpointer data)
{
  g_error ("Failed by timeout");
  
  return G_SOURCE_REMOVE;
}

static gboolean
track_plan_equal (const HyScanTrackPlan *plan1,
                  const HyScanTrackPlan *plan2)
{
  return ABS (plan1->speed - plan2->speed) < 1e-3 &&
         ABS (plan1->start.lat - plan2->start.lat) < 1e-3 &&
         ABS (plan1->start.lon - plan2->start.lon) < 1e-3 &&
         ABS (plan1->end.lat - plan2->end.lat) < 1e-3 &&
         ABS (plan1->end.lon - plan2->end.lon) < 1e-3;
}

static void
test_started (void)
{
  gchar *project_name;
  gchar *track_name;
  HyScanTrackType track_type;
  HyScanTrackPlan *plan;
  gboolean success;

  g_message ("\"start-stop\" signal emitted");

  /* Отключаем ошибку по таймауту. */
  g_source_remove (timeout_tag);

  /* Отключаем текущий обработчик сигнала. */
  g_signal_handler_disconnect (sonar_model, start_stop_tag);

  /* Проверяем, что локатор работает с нужными параметрами. */
  if (!hyscan_sonar_state_get_start (HYSCAN_SONAR_STATE (sonar_model), &project_name, &track_name, &track_type, &plan))
    g_error ("Sonar has not started");

  success = g_strcmp0 (PROJECT_NAME, project_name) == 0 &&
            g_strcmp0 (TRACK_NAME, track_name) == 0 &&
            track_type == TRACK_TYPE &&
            track_plan_equal (plan, &track_plan);

  g_message ("Sonar has started track \"%s/%s\" (type = %d, plan = %d)", project_name, track_name, track_type, plan != NULL);

  if (!success)
    g_error ("Sonar started with wrong parameters");

  hyscan_track_plan_free (plan);
  g_free (project_name);
  g_free (track_name);

  /* Проверка остановки локатора. */
  g_message ("Test hyscan_sonar stop. Waiting for \"start-stop\" signal...");
  timeout_tag = g_timeout_add (FAIL_TIMEOUT, (GSourceFunc) fail_by_timeout, NULL);
  start_stop_tag = g_signal_connect_swapped (sonar_model, "start-stop",
                                             G_CALLBACK (test_stopped),
                                             GUINT_TO_POINTER(timeout_tag));
  hyscan_sonar_stop (HYSCAN_SONAR (sonar_model));
}

static void
test_stopped (void)
{
  g_message ("\"start-stop\" signal emitted");

  /* Отключаем ошибку по таймауту. */
  g_source_remove (GPOINTER_TO_UINT (timeout_tag));

  /* Отключаем текущий обработчик сигнала. */
  g_signal_handler_disconnect (sonar_model, start_stop_tag);
  
  if (hyscan_sonar_state_get_start (HYSCAN_SONAR_STATE (sonar_model), NULL, NULL, NULL, NULL))
    g_error ("Failed to stop sonar");

  /* Проверка запуска локатора. */
  g_message ("Test hyscan_sonar stop. Waiting for \"start-stop\" signal...");
  timeout_tag = g_timeout_add (FAIL_TIMEOUT, (GSourceFunc) fail_by_timeout, NULL);
  start_stop_tag = g_signal_connect_swapped (sonar_model, "start-stop",
                                             G_CALLBACK (test_control_model_started),
                                             GUINT_TO_POINTER (timeout_tag));
  hyscan_control_model_set_track_sfx (sonar_model, TRACK_SUFFIX, TRUE);
  hyscan_control_model_set_track_name (sonar_model, NULL);
  hyscan_control_model_set_project (sonar_model, PROJECT_NAME);
  hyscan_control_model_start (sonar_model);
}

static void
test_control_model_started (void)
{
  gchar *track_name;
  gchar *project_name;
  HyScanTrackPlan *plan;
  HyScanTrackType track_type;
  gboolean success;
  
  g_message ("\"start-stop\" signal emitted");

  /* Отключаем ошибку по таймауту. */
  g_source_remove (GPOINTER_TO_UINT (timeout_tag));

  /* Отключаем текущий обработчик сигнала. */
  g_signal_handler_disconnect (sonar_model, start_stop_tag);

  if (!hyscan_sonar_state_get_start (HYSCAN_SONAR_STATE (sonar_model), &project_name, &track_name, &track_type, &plan))
    g_error ("Failed to start sonar");

  g_message ("Sonar has started track \"%s/%s\" (type = %d, plan = %d)",
             project_name, track_name, track_type, plan != NULL);

  success = g_strcmp0 (project_name, PROJECT_NAME) == 0 &&
            g_str_has_suffix (track_name, TRACK_SUFFIX) &&
            track_type == TRACK_TYPE &&
            plan == NULL;

  g_free (project_name);
  g_free (track_name);

  if (!success)
    g_error ("Sonar started with wrong parameters");
  
  g_main_quit (loop);
}

static gboolean
test_start (gpointer data)
{
  g_message ("Test hyscan_sonar_start. Waiting for \"start-stop\" signal...");

  timeout_tag = g_timeout_add (FAIL_TIMEOUT, (GSourceFunc) fail_by_timeout, NULL);
  start_stop_tag = g_signal_connect_swapped (sonar_model, "start-stop",
                                             G_CALLBACK (test_started),
                                             GUINT_TO_POINTER (timeout_tag));
  
  hyscan_sonar_start (HYSCAN_SONAR (sonar_model), PROJECT_NAME, TRACK_NAME, TRACK_TYPE, &track_plan);
  
  return G_SOURCE_REMOVE;
}

int
main (int    argc,
      char **argv)
{
  HyScanDB *db;
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

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Failed to connect to db: %s", db_uri);

  dummy_device = hyscan_dummy_device_new (HYSCAN_DUMMY_DEVICE_SIDE_SCAN);

  control = hyscan_control_new ();
  hyscan_control_device_add (control, HYSCAN_DEVICE (dummy_device));
  hyscan_control_device_bind (control);
  hyscan_control_writer_set_db (control, db);

  sonar_model = hyscan_control_model_new (control);
  hyscan_control_model_set_sync_timeout (sonar_model, SYNC_PERIOD);

  loop = g_main_new (FALSE);

  g_idle_add ((GSourceFunc) test_sensor, NULL);
  g_main_run (loop);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_free (db_uri);
  g_object_unref (db);
  g_object_unref (control);
  g_object_unref (dummy_device);
  g_object_unref (sonar_model);

  g_print ("Test done\n");

  return 0;
}
