#include <hyscan-sonar-model.h>
#include <hyscan-dummy-device.h>

#define TEST_SOURCE HYSCAN_SOURCE_SIDE_SCAN_PORT
#define SYNC_PERIOD (100 * G_TIME_SPAN_MILLISECOND / 1000) /* Период синхронизации параметров ГЛ. */
#define WAIT_PERIOD (SYNC_PERIOD * 4)                      /* Период ожидания синхронизации ГЛ. */

static HyScanSonarModel *sonar_model;
static HyScanControl *control;
static HyScanDummyDevice *dummy_device;
static GMainLoop *loop;

static gboolean test_sonar         (void);
static gboolean test_sensor        (void);
static gboolean test_param         (void);
static gboolean test_atvg_sync     (void);
static gboolean test_sync_failed   (void);
static gboolean test_sync_happened (void);
static void     check_params       (HyScanParam     *param,
                                    HyScanParamList *expect_list);


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

static gboolean
test_param (void)
{
  // {
  //   HyScanDataSchema *schema;
  //   schema = hyscan_param_schema (HYSCAN_PARAM (sonar));
  //   const gchar *const *keys;
  //   gint i;
  //   keys = hyscan_data_schema_list_keys (schema);
  //   for (i = 0; keys[i] != NULL; i++)
  //     {
  //       HyScanDataSchemaKeyAccess access = hyscan_data_schema_key_get_access (schema, keys[i]);
  //
  //       g_print ("Key %s: %c%c%c\n",
  //                keys[i],
  //                (access & HYSCAN_DATA_SCHEMA_ACCESS_READ) ? 'R' : ' ',
  //                (access & HYSCAN_DATA_SCHEMA_ACCESS_WRITE) ? 'W' : ' ',
  //                (access & HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN) ? 'H' : ' '
  //                );
  //     }
  //   g_object_unref (schema);
  // }

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

  g_main_quit (loop);

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
test_sensor (void)
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
test_sync_failed (void)
{
  /* Проверяем, что состояние ещё не синхронизировано. */
  if (hyscan_dummy_device_check_sync (dummy_device))
    g_error ("Sync has happened too early");

  g_timeout_add (WAIT_PERIOD, (GSourceFunc) test_sync_happened, NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
test_sync_happened (void)
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
test_atvg_sync (void)
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
test_sonar (void)
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

int
main (int    argc,
      char **argv)
{
  dummy_device = hyscan_dummy_device_new (HYSCAN_DUMMY_DEVICE_SIDE_SCAN);

  control = hyscan_control_new ();
  hyscan_control_device_add (control, HYSCAN_DEVICE (dummy_device));
  hyscan_control_device_bind (control);

  sonar_model = hyscan_sonar_model_new (control);
  hyscan_sonar_model_set_sync_timeout (sonar_model, SYNC_PERIOD);

  loop = g_main_new (FALSE);

  g_idle_add ((GSourceFunc) test_sensor, NULL);
  g_main_run (loop);

  g_object_unref (control);
  g_object_unref (dummy_device);
  g_object_unref (sonar_model);

  g_print ("Test done\n");

  return 0;
}
