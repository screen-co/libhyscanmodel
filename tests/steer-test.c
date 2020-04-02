/* steer-test.c
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

#include <hyscan-steer.h>
#include <hyscan-sonar-model.h>

#define TAG_NAV_STATE "[NavState] "
#define TAG_INFO      "[Info] "
#define TAG_OK        "[OK] "
#define TAG_ERROR     "[Error] "
#define PROJET_NAME   "steer-test"

#define HYSCAN_TYPE_NAV_STATE_DUMMY             (hyscan_nav_state_dummy_get_type ())
#define HYSCAN_NAV_STATE_DUMMY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_STATE_DUMMY, HyScanNavStateDummy))
#define HYSCAN_IS_NAV_STATE_DUMMY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_STATE_DUMMY))

typedef struct _HyScanNavStateDummy HyScanNavStateDummy;
typedef struct _HyScanNavStateDummyClass HyScanNavStateDummyClass;
typedef struct _TestTrack TestTrack;

/* Тестовые данные. */
struct _TestTrack
{
  HyScanGeoCartesian2D         start;                          /* Начало галса в декартовой СК. */
  HyScanGeoCartesian2D         end;                            /* Конец галса в декартовой СК. */
};

/* Имплементация интерфейса HyScanNavState для тестирования. */
struct _HyScanNavStateDummy
{
  GObject             parent_instance;                         /* Родительский объект. */
  HyScanGeo          *geo;                                     /* Объект перевода географических координат. */
  HyScanNavStateData  data;                                    /* Текущие данные навигации. */
};

struct _HyScanNavStateDummyClass
{
  GObjectClass        parent_instance;                         /* Родительский класс. */
};


static GMainLoop              *loop;                           /* Главный цикл. */
static HyScanDB               *db;                             /* База данных. */
static HyScanSonarModel       *sonar;                          /* Модель управления локатором. */
static HyScanSonarRecorder    *recorder;                       /* Запись галса. */
static HyScanNavStateDummy    *nav_dummy;                      /* Навигационные данные. */
static HyScanSteer            *steer;                          /* Модель навигации по галсу. */
static HyScanPlannerModel     *planner_model;                  /* Модель объектов планировщика. */
static HyScanPlannerSelection *selection;                      /* Выбор объектов планировщика. */

#define FAIL_TIMEOUT  10000         /* Время до завершения теста, мс. */
#define X_DIRECTION   0             /* Направление оси X, градусы. */
#define LENGTH        20            /* Длина галса, метры. */

/* Координаты точки (0, 0) и направление оси OX. */
static HyScanGeoGeodetic origin = { .lat = 55.570615331814089,
                                    .lon = 38.100472171300588,
                                    .h = X_DIRECTION };

/* Схема галсов. */
static TestTrack tracks[] = {{ .start = {  0,  0 }, .end = { 20,  0 } },   /* Галс 1. */
                             { .start = { 20, 40 }, .end = {  0, 40 } }};  /* Галс 2. */


static HyScanGeoCartesian2D point_select = { -2.0, -2.0 };     /* Точка, выбора галса 1. */
static HyScanGeoCartesian2D point_start  = { -0.5,  0.0 };     /* Точка, включения записи галса 1. */
static HyScanGeoCartesian2D point_stop   = { 21.0,  0.0 };     /* Точка, выключения записи галса 1. */

GType      hyscan_nav_state_dummy_get_type (void);
void       test_autoselect                 (void);
void       test_activated                  (void);
gboolean   fail_test                       (void);
gboolean   set_up_project                  (gpointer user_data);
void       set_up_tracks                   (gpointer user_data);
void       test_started                    (void);
void       test_stopped                    (void);

/* Функция устанавливает местоположение судна. */
void
hyscan_nav_state_dummy_set (HyScanNavStateDummy *dummy,
                            HyScanGeoCartesian2D coord,
                            gdouble              heading,
                            gdouble              speed)
{
  dummy->data.loaded = TRUE;
  hyscan_geo_topoXY2geo (dummy->geo, &dummy->data.coord, coord, 0);
  dummy->data.heading = heading;
  dummy->data.speed = speed;

  g_message (TAG_NAV_STATE "Ship at (%.1f, %.1f), speed %.1f m/s, %.1f grad", coord.x, coord.y, speed, heading);

  g_signal_emit_by_name (dummy, "nav-changed", 0, &dummy->data);
}

/* Функция возвращает местоположение судна. */
gboolean
hyscan_nav_state_dummy_get (HyScanNavState      *state,
                            HyScanNavStateData  *data,
                            gdouble             *time_delta)
{
  HyScanNavStateDummy *dummy = HYSCAN_NAV_STATE_DUMMY (state);

  if (data != NULL)
    *data = dummy->data;

  if (time_delta != NULL)
    *time_delta = 0;

  return TRUE;
}

void
hyscan_nav_state_dummy_iface_init (HyScanNavStateInterface *iface)
{
  iface->get = hyscan_nav_state_dummy_get;
}

static void
hyscan_nav_state_dummy_class_init (HyScanNavStateDummyClass *klass)
{
}

static void
hyscan_nav_state_dummy_init (HyScanNavStateDummy *dummy)
{
  dummy->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  /* Чтобы не писать finalize, поместим ссылку на dummy->geo в данные объекта. */
  g_object_set_data_full (G_OBJECT (dummy), "geo", dummy->geo, g_object_unref);
}

G_DEFINE_TYPE_WITH_CODE (HyScanNavStateDummy, hyscan_nav_state_dummy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_NAV_STATE, hyscan_nav_state_dummy_iface_init))

/* Функция завершает тест неудачей по таймауту. */
gboolean
fail_test (void)
{
  g_error ("Test failed by timeout");

  return G_SOURCE_REMOVE;
}

/* Функция создаёт проект. */
gboolean
set_up_project (gpointer user_data)
{
  /* Создаём проект. */
  hyscan_sonar_recorder_start (recorder);

  /* Добавляем следующий коллбэк. */
  g_signal_connect (sonar, "start-stop", G_CALLBACK (set_up_tracks), NULL);

  return G_SOURCE_REMOVE;
}

/* Функция записывает схему галсов в БД. */
void
set_up_tracks (gpointer user_data)
{
  if (!hyscan_sonar_state_get_start (HYSCAN_SONAR_STATE (sonar), NULL, NULL, NULL, NULL))
    g_error (TAG_ERROR "Failed to create project");

  /* Отключаемся от этого коллбэка. */
  g_signal_handlers_disconnect_by_func (sonar, set_up_tracks, NULL);

  hyscan_sonar_recorder_stop (recorder);

  /* Инициализируем плановые галсы и добавляем их в БД. */
  hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (planner_model), db, PROJET_NAME);
  for (guint i = 0; i < G_N_ELEMENTS (tracks); i++)
    {
      HyScanPlannerTrack track = { .type = HYSCAN_PLANNER_TRACK };
      track.number = i+1;
      track.plan.velocity = 1.0;

      hyscan_geo_topoXY2geo (nav_dummy->geo, &track.plan.start, tracks[i].start, 0);
      hyscan_geo_topoXY2geo (nav_dummy->geo, &track.plan.end, tracks[i].end, 0);
      hyscan_object_model_add_object (HYSCAN_OBJECT_MODEL (planner_model), (HyScanObject *) &track);
    }

  /* Добавляем следующий коллбэк. */
  g_signal_connect (planner_model, "changed", G_CALLBACK (test_autoselect), NULL);
}

/* Функция включает автоопределение галса. */
void
test_autoselect (void)
{
  GHashTable *objects = NULL;

  objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (planner_model));
  g_message (TAG_INFO "Objects size: %u", objects != NULL ? g_hash_table_size (objects) : 0);
  if (objects == NULL || g_hash_table_size (objects) != G_N_ELEMENTS (tracks))
    goto exit;
  
  g_message (TAG_OK "Test planner tracks have been added");

  g_signal_handlers_disconnect_by_func (planner_model, test_autoselect, NULL);

  g_message (TAG_INFO "Awaiting activation...");
  g_signal_connect (selection, "activated", G_CALLBACK (test_activated), NULL);
  hyscan_nav_state_dummy_set (nav_dummy, point_select, 10, 1.0);
  hyscan_steer_set_autoselect (steer, TRUE);

exit:
  g_clear_pointer (&objects, g_hash_table_unref);
}

/* Функция проверяет, что галс был активирован. */
void
test_activated (void)
{
  HyScanPlannerTrack *track;
  HyScanGeoCartesian2D start, end;
  gdouble azimuth, length;
  gchar *active_id;

  active_id = hyscan_planner_selection_get_active_track (selection);

  /* Отключаемся от сигнала. */
  g_signal_handlers_disconnect_by_func (selection, test_activated, NULL);

  track = (HyScanPlannerTrack *) hyscan_object_model_get_id (HYSCAN_OBJECT_MODEL (planner_model), active_id);
  if (!HYSCAN_IS_PLANNER_TRACK (track))
    g_error ("Active track is not found");

  if (track->number != 1)
    g_error ("Track #1 must be activated");

  g_message (TAG_OK "Track #1 has been activated");

  if (!hyscan_steer_get_track_topo (steer, &start, &end))
    g_error ("Failed to get track topo params");

  g_assert_cmpfloat (ABS (start.x - tracks[0].start.x), <, 1e-4);
  g_assert_cmpfloat (ABS (start.y - tracks[0].start.y), <, 1e-4);
  g_assert_cmpfloat (ABS (end.x - tracks[0].end.x), <, 1e-4);
  g_assert_cmpfloat (ABS (end.y - tracks[0].end.y), <, 1e-4);
  g_message (TAG_OK "Track start and end are correct");

  if (!hyscan_steer_get_track_info (steer, &azimuth, &length))
    g_error ("Failed to get track info");

  g_assert_cmpfloat (ABS (length - LENGTH), < , 1e-4);
  g_assert_cmpfloat (ABS (azimuth - X_DIRECTION), < , 1e-4);
  g_message (TAG_OK "Track azimuth and length are correct");

  g_message (TAG_INFO "Awaiting sonar start...");
  g_signal_connect (sonar, "start-stop", G_CALLBACK (test_started), NULL);
  hyscan_steer_set_autostart (steer, TRUE);
  hyscan_nav_state_dummy_set (nav_dummy, point_start, X_DIRECTION, 1.0);

  hyscan_planner_track_free (track);
  g_free (active_id);
}

/* Проверка старта работы локатора. */
void
test_started (void)
{
  gchar *track_name;

  if (!hyscan_sonar_state_get_start (HYSCAN_SONAR_STATE (sonar), NULL, &track_name, NULL, NULL))
    g_error ("Failed to start sonar");

  g_message (TAG_OK "Auto recording has been started with track name \"%s\"", track_name);

  /* Отключаемся от сигнала. */
  g_signal_handlers_disconnect_by_func (sonar, test_started, NULL);

  {
    g_message (TAG_INFO "Awaiting sonar stop...");
    g_signal_connect (sonar, "start-stop", G_CALLBACK (test_stopped), NULL);
    hyscan_steer_set_autostart (steer, TRUE);
    hyscan_nav_state_dummy_set (nav_dummy, point_stop, X_DIRECTION, 1.0);
  }

  g_free (track_name);
}

/* Проверка остановки работы локатора. */
void
test_stopped (void)
{
  gchar *track_name;

  if (hyscan_sonar_state_get_start (HYSCAN_SONAR_STATE (sonar), NULL, &track_name, NULL, NULL))
    g_error ("Failed to stop sonar");

  g_message (TAG_OK "Auto recording has been stopped");

  /* Отключаемся от сигнала. */
  g_signal_handlers_disconnect_by_func (sonar, test_started, NULL);

  g_main_loop_quit (loop);
}

int main (int    argc,
          char **argv)
{
  HyScanControl *control;

  if (argc != 2)
    {
      g_print ("Usage: %s <db-uri>", argv[0]);
      return -1;
    }

  db = hyscan_db_new (argv[1]);
  if (db == NULL)
    g_error ("Failed to connect to database %s", argv[1]);

  control = hyscan_control_new ();
  hyscan_control_writer_set_db (control, db);
  hyscan_control_device_bind (control);

  sonar = hyscan_sonar_model_new (control);

  recorder = hyscan_sonar_recorder_new (HYSCAN_SONAR (sonar), db);
  hyscan_sonar_recorder_set_project (recorder, PROJET_NAME);

  planner_model = hyscan_planner_model_new ();

  nav_dummy = g_object_new (HYSCAN_TYPE_NAV_STATE_DUMMY, NULL);

  selection = hyscan_planner_selection_new (planner_model);
  steer = hyscan_steer_new (HYSCAN_NAV_STATE (nav_dummy), selection, recorder);

  g_idle_add (set_up_project, NULL);
  g_timeout_add (FAIL_TIMEOUT, (GSourceFunc) fail_test, NULL);

  loop = g_main_new (TRUE);
  g_main_loop_run (loop);

  g_message (TAG_OK "Test has been done successfully!");
  hyscan_db_project_remove (db, PROJET_NAME);

  g_object_unref (control);
  g_object_unref (sonar);
  g_object_unref (db);
  g_object_unref (recorder);
  g_object_unref (nav_dummy);
  g_object_unref (planner_model);
  g_object_unref (selection);
  g_object_unref (steer);

  return 0;
}
