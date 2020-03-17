/* hyscan-steer.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanModel library.
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
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanModel имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanModel на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-steer
 * @Short_description: модель навигации по плановому галсу
 * @Title: HyScanSteer
 *
 * Модель #HyScanSteer получает навигационные данные в режиме реального времени
 * и определяет отклонение текущего положения и скорости движения от плановых
 * параметров.
 *
 * При записи галса важно именно положение гидролокатора, а не судна как такового,
 * поэтому в виджете есть возможность вносить поправку на смещение антенны с
 * помощью функции hyscan_steer_sensor_set_offset().
 *
 * План галса должен быть установлен в модель выбора планировщика с помощью функции
 * hyscan_planner_selection_activate().
 *
 * Модель может в автоматическом режиме активировать плановый галс, который находится
 * ближе всего к текущему местоположению судна. Для включения автовыбора
 * используется функция hyscan_steer_set_autoselect().
 *
 * Для автоматического старта работы локатора при выходе на текущий плановый галс
 * используется функция hyscan_steer_set_autostart().
 *
 */

#include "hyscan-steer.h"
#include "hyscan-sonar-state.h"
#include <math.h>
#include <hyscan-cartesian.h>

#define DEG2RAD(deg) (deg / 180. * G_PI)  /* Перевод из градусов в радианы. */
#define RAD2DEG(rad) (rad / G_PI * 180.)  /* Перевод из радиан в градусы. */
#define AUTOSELECT_INTERVAL 2000          /* Интервал отслеживания ближайших плановых галсов, мс. */
#define AUTOSELECT_DIST     100           /* Максимальное расстояние, на котором галс может быть выбран автоматом, м. */

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
  PROP_RECORDER,
  PROP_AUTOSTART,
  PROP_AUTOSELECT,
  PROP_THRESHOLD,
  PROP_LAST
};

enum
{
  SIGNAL_POINT,
  SIGNAL_PLAN_CHANGED,
  SIGNAL_LAST,
};

struct _HyScanSteerPrivate
{
  HyScanNavModel         *nav_model;       /* Модель навигационных данных. */
  HyScanObjectModel      *planner_model;   /* Модель объектов планировщика. */
  HyScanPlannerSelection *selection;       /* Модель выбранных объектов планировщика.  */
  HyScanSonarRecorder    *recorder;        /* Модель управления работой гидролокатора. */
  HyScanSonarState       *sonar_state;     /* Состояние гидролокатора. */
  gchar                  *project_name;    /* Название проекта. */

  HyScanAntennaOffset    *offset;          /* Смещение антенны гидролокатор. */
  HyScanGeo              *geo;             /* Перевод географических координат в декартовы. */

  HyScanPlannerZone      *zone;            /* Зона, в которой находится плановый галс. */
  HyScanPlannerTrack     *track;           /* Плановый галс. */
  gchar                  *track_id;        /* ИД планового галса. */

  HyScanGeoCartesian2D    start;           /* Плановая точка начала галса. */
  HyScanGeoCartesian2D    end;             /* Плановая точка конца галса*/
  gdouble                 azimuth;         /* Направление движения по плану галса. */
  gdouble                 length;          /* Длина планового галса. */
  gdouble                 threshold;       /* Допустимое отклонение от прямой линии галса. */

  gboolean                autostart;       /* Автоматический старт локатора включён. */
  gboolean                recording;       /* Локатор включён. */
  gboolean                autoselect;      /* Автоматический выбор ближайшего плана галса. */

  guint                   select_tag;      /* Тэг периодической функции по выбору галса для навигации. */
  HyScanNavModelData     *nav_data;        /* Последние полученные данные навигации. */
  GHashTable             *plan_geo;        /* Хэш-таблица объектов HyScanGeo по каждому из планов галсов. */
};

static void      hyscan_steer_set_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void      hyscan_steer_get_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        GValue                *value,
                                                        GParamSpec            *pspec);
static void      hyscan_steer_object_constructed       (GObject               *object);
static void      hyscan_steer_object_finalize          (GObject               *object);
static void      hyscan_steer_set_track                (HyScanSteer           *steer);
static void      hyscan_steer_nav_changed              (HyScanSteer           *steer);
static void      hyscan_steer_start_stop               (HyScanSteer           *steer);
static gboolean  hyscan_steer_plan_select              (HyScanSteer           *steer);
static void      hyscan_steer_plan_geo_clear           (HyScanSteer *steer);

static guint       hyscan_steer_signals[SIGNAL_LAST] = { 0 };
static GParamSpec* hyscan_steer_prop_autostart;
static GParamSpec* hyscan_steer_prop_autoselect;
static GParamSpec* hyscan_steer_prop_threshold;

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSteer, hyscan_steer, G_TYPE_OBJECT)

static void
hyscan_steer_class_init (HyScanSteerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_steer_set_property;
  object_class->get_property = hyscan_steer_get_property;

  object_class->constructed = hyscan_steer_object_constructed;
  object_class->finalize = hyscan_steer_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanNavModel", "Navigation model",
                         HYSCAN_TYPE_NAV_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "HyScanPlannerSelection", "Model of selected planner objects",
                         HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_RECORDER,
    g_param_spec_object ("recorder", "HyScanSonarRecorder", "Sonar recorder model",
                         HYSCAN_TYPE_SONAR_RECORDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_steer_prop_autostart = g_param_spec_boolean ("autostart", "Auto start",
                                                      "Start sonar when approaching the plan track",
                                                      FALSE,
                                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_AUTOSTART, hyscan_steer_prop_autostart);

  hyscan_steer_prop_autoselect = g_param_spec_boolean ("autoselect", "Plan auto-select",
                                                       "Automatically select the next plan track",
                                                       FALSE,
                                                       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_AUTOSELECT, hyscan_steer_prop_autoselect);

  hyscan_steer_prop_threshold = g_param_spec_double ("threshold", "Threshold",
                                                     "Maximum allowed distance to the plan track",
                                                     0, G_MAXDOUBLE, 2.0,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_THRESHOLD, hyscan_steer_prop_threshold);

  /**
   * HyScanSteer::point:
   * @steer: указатель на #HyScanSteer
   * @point: указатель на новую точку #HyScanSteerPoint
   *
   * Сигнал посылается при изменении местоположения судна.
   */
  hyscan_steer_signals[SIGNAL_POINT] =
    g_signal_new ("point", HYSCAN_TYPE_STEER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * HyScanSteer::plan-changed:
   * @info: указатель на #HyScanSteer
   *
   * Сигнал посылается при изменении плана галса. Текущий план можно получить
   * с помощью функции hyscan_steer_get_track().
   */
  hyscan_steer_signals[SIGNAL_PLAN_CHANGED] =
    g_signal_new ("plan-changed", HYSCAN_TYPE_STEER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_steer_init (HyScanSteer *steer)
{
  steer->priv = hyscan_steer_get_instance_private (steer);
}

static void
hyscan_steer_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  HyScanSteer *steer = HYSCAN_STEER (object);
  HyScanSteerPrivate *priv = steer->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->nav_model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      break;

    case PROP_RECORDER:
      priv->recorder = g_value_dup_object (value);
      break;

    case PROP_AUTOSTART:
      hyscan_steer_set_autostart (steer, g_value_get_boolean (value));
      break;

    case PROP_AUTOSELECT:
      hyscan_steer_set_autoselect (steer, g_value_get_boolean (value));
      break;

    case PROP_THRESHOLD:
      hyscan_steer_set_threshold (steer, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_steer_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  HyScanSteer *steer = HYSCAN_STEER (object);
  HyScanSteerPrivate *priv = steer->priv;

  switch (prop_id)
    {
    case PROP_AUTOSTART:
      g_value_set_boolean (value, priv->autostart);
      break;

    case PROP_AUTOSELECT:
      g_value_set_boolean (value, priv->autoselect);
      break;

    case PROP_THRESHOLD:
      g_value_set_double (value, priv->threshold);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_steer_object_constructed (GObject *object)
{
  HyScanSteer *steer = HYSCAN_STEER (object);
  HyScanSteerPrivate *priv = steer->priv;
  HyScanSonar *sonar;

  G_OBJECT_CLASS (hyscan_steer_parent_class)->constructed (object);

  priv->planner_model = HYSCAN_OBJECT_MODEL (hyscan_planner_selection_get_model (priv->selection));

  /* Сигнал "start-stop" позволяет отслеживать статус работы локатора и необходим для функции автостарта. */
  sonar = hyscan_sonar_recorder_get_sonar (priv->recorder);
  if (HYSCAN_IS_SONAR_STATE (sonar))
    {
      priv->sonar_state = g_object_ref (sonar);
      g_signal_connect_swapped (priv->sonar_state, "start-stop", G_CALLBACK (hyscan_steer_start_stop), steer);
    }
  g_object_unref (sonar);

  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_steer_nav_changed), steer);
  g_signal_connect_swapped (priv->planner_model, "changed", G_CALLBACK (hyscan_steer_set_track), steer);
  g_signal_connect_swapped (priv->planner_model, "changed", G_CALLBACK (hyscan_steer_plan_geo_clear), steer);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_steer_set_track), steer);

  priv->select_tag = g_timeout_add (AUTOSELECT_INTERVAL, (GSourceFunc) hyscan_steer_plan_select, steer);
}

static void
hyscan_steer_object_finalize (GObject *object)
{
  HyScanSteer *steer = HYSCAN_STEER (object);
  HyScanSteerPrivate *priv = steer->priv;

  g_clear_object (&priv->recorder);
  g_clear_object (&priv->selection);
  g_clear_object (&priv->planner_model);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->sonar_state);
  g_clear_object (&priv->geo);
  g_clear_pointer (&priv->plan_geo, g_hash_table_unref);
  g_clear_pointer (&priv->track, hyscan_planner_track_free);
  g_free (priv->track_id);
  g_free (priv->nav_data);
  g_source_remove (priv->select_tag);

  G_OBJECT_CLASS (hyscan_steer_parent_class)->finalize (object);
}

/* Функция выбирает ближайший план галса для навигации. */
static gboolean
hyscan_steer_plan_select (HyScanSteer *steer)
{
  HyScanSteerPrivate *priv = steer->priv;
  HyScanNavModelData *nav_data = priv->nav_data;

  GHashTableIter iter;
  gchar *key;
  HyScanGeo *geo;

  gdouble min_cost;
  const gchar *track_id = NULL;

  /* В некоторых случаях ничего не делаем. */
  if (priv->recording || !priv->autoselect || nav_data == NULL)
    return G_SOURCE_CONTINUE;

  /* Заполняем таблицу из объектов HyScanGeo для планов галсов. 
   * Она очищается каждый раз при изменении объектов планировщика. */
  if (priv->plan_geo == NULL)
    {
      GHashTable *tracks;
      HyScanPlannerTrack *track;

      priv->plan_geo = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
      tracks = hyscan_object_model_get (priv->planner_model);
      g_hash_table_iter_init (&iter, tracks);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
        {
          if (HYSCAN_IS_PLANNER_TRACK (track))
            g_hash_table_insert (priv->plan_geo, g_strdup (key), hyscan_planner_track_geo (&track->plan, NULL));
        }

      g_hash_table_unref (tracks);
    }

  /* Ищем ближайший плановый галс. */
  min_cost = G_MAXDOUBLE;
  g_hash_table_iter_init (&iter, priv->plan_geo);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &geo))
    {
      gdouble cost;
      HyScanGeoCartesian2D position;

      if (!hyscan_geo_geo2topoXY (geo, &position, nav_data->coord))
        continue;

      cost = hypot (position.x, position.y);
      if (cost > min_cost)
        continue;

      min_cost = cost;
      track_id = key;
    }

  /* Если найденный галс недалеко и ешё не активирован, то активируем его. */
  if (min_cost < AUTOSELECT_DIST && g_strcmp0 (priv->track_id, track_id) != 0)
    hyscan_planner_selection_activate (priv->selection, track_id);

  return G_SOURCE_CONTINUE;
}

/* Обработчик сигнала "start-stop". Синхронизирует статус работы локатора. */
static void
hyscan_steer_start_stop (HyScanSteer *steer)
{
  HyScanSteerPrivate *priv = steer->priv;

  priv->recording = hyscan_sonar_state_get_start (priv->sonar_state, NULL, NULL, NULL, NULL);
}

/* Обработчик сигнала "changed" модели навигационных данных. */
static void
hyscan_steer_nav_changed (HyScanSteer *steer)
{
  HyScanSteerPrivate *priv = steer->priv;
  HyScanPlannerTrack *track = priv->track;
  HyScanPlannerZone *zone = priv->zone;
  HyScanNavModelData data;
  HyScanSteerPoint point = { 0 };

  g_clear_pointer (&priv->nav_data, g_free);
  if (hyscan_nav_model_get (priv->nav_model, &data, NULL))
    priv->nav_data = g_memdup (&data, sizeof (data));

  if (priv->nav_data == NULL || priv->geo == NULL)
    return;

  point.nav_data = *priv->nav_data;
  hyscan_steer_calc_point (&point, steer);

  g_signal_emit (steer, hyscan_steer_signals[SIGNAL_POINT], 0, &point);

  if (!priv->autostart)
    return;

  /* Включаем и отключаем запись, если необходимо. */
  if (!priv->recording)
    {
      gdouble speed_along;

      /* Начинаем запись при выполнении условий:
       * (1) движение по направлению галса,
       * (2) положение вдоль галса в интервале (-speed_along * start_time, track_length),
       * (3) расстояние до линии галса меньше заданного значения. */
      speed_along = point.nav_data.speed * cos (point.d_angle);
      if (speed_along > 0 &&                                                      /* (1) */
          -speed_along < point.position.x && point.position.x < priv->length &&   /* (2) */
          ABS (point.d_y) < priv->threshold)                                      /* (3) */
        {
          gchar *suffix;

          if (zone != NULL && track->number > 0)
            suffix = g_strdup_printf ("-plan-%s.%d", zone->name, track->number);
          else if (track->number > 0)
            suffix = g_strdup_printf ("-plan-%d", track->number);
          else if (zone != NULL)
            suffix = g_strdup_printf ("-plan-%s", zone->name);
          else
            suffix = g_strdup ("-plan");

          hyscan_sonar_recorder_set_track_type (priv->recorder, HYSCAN_TRACK_SURVEY);
          hyscan_sonar_recorder_set_plan (priv->recorder, &track->plan);

          /* Суффикс действителен только для следующего старта. */
          hyscan_sonar_recorder_set_suffix (priv->recorder, suffix, TRUE);

          priv->recording = hyscan_sonar_recorder_start (priv->recorder);

          g_free (suffix);
        }
    }
  else
    {
      if (point.position.x > priv->length)
        {
          hyscan_sonar_recorder_stop (priv->recorder);
          priv->recording = FALSE;

          /* Выбираем следующий галс. */
          hyscan_steer_plan_select (steer);
        }
    }
}

/* Обработчик сигнала "changed" модели объектов планировщика.
 * Функция очищает таблицу priv->plan_geo. */
static void
hyscan_steer_plan_geo_clear (HyScanSteer *steer)
{
  g_clear_pointer (&steer->priv->plan_geo, g_hash_table_unref);
}

/* Функция обновляет активный план галса. */
static void
hyscan_steer_set_track (HyScanSteer *steer)
{
  HyScanSteerPrivate *priv = steer->priv;
  HyScanPlannerTrack *track = NULL;
  gchar *active_id;

  active_id = hyscan_planner_selection_get_active_track (priv->selection);
  if (active_id != NULL)
    track = (HyScanPlannerTrack *) hyscan_object_model_get_id (priv->planner_model, active_id);

  g_free (priv->track_id);
  hyscan_planner_track_free (priv->track);
  priv->track_id = active_id;
  priv->track = track;

  g_clear_pointer (&priv->zone, hyscan_planner_zone_free);
  if (track != NULL && track->zone_id != NULL)
    priv->zone = (HyScanPlannerZone *) hyscan_object_model_get_id (priv->planner_model, track->zone_id);

  g_clear_object (&priv->geo);
  if (track != NULL)
    {
      HyScanTrackPlan *plan;

      plan = &track->plan;
      priv->geo = hyscan_planner_track_geo (plan, &priv->azimuth);
      hyscan_geo_geo2topoXY (priv->geo, &priv->start, plan->start);
      hyscan_geo_geo2topoXY (priv->geo, &priv->end, plan->end);
      priv->length = hyscan_cartesian_distance (&priv->start, &priv->end);
    }

  g_signal_emit (steer, hyscan_steer_signals[SIGNAL_PLAN_CHANGED], 0);
}

/**
 * hyscan_steer_new:
 * @model: модель данных навигации
 * @selection: модель выбранных объектов планировщика
 * @recorder: объект управления записью галсов
 *
 * Функция создаёт модель навигации по плану галса.
 *
 * Returns: (transfer full): указатель на новый объект #HyScanSteer, для удаления g_object_unref().
 */
HyScanSteer *
hyscan_steer_new (HyScanNavModel         *model,
                  HyScanPlannerSelection *selection,
                  HyScanSonarRecorder    *recorder)
{
  return g_object_new (HYSCAN_TYPE_STEER,
                       "model", model,
                       "selection", selection,
                       "recorder", recorder,
                       NULL);
}

/**
 * hyscan_steer_get_track:
 * @steer: указатель на #HyScanSteer
 * @track_id: (out) (nullable): идентификатор галса в базе данных
 *
 * Функция возвращает указатель на активный план галса и его идентификатор в базе данных.
 *
 * Returns: активный план галса или %NULL
 */
const HyScanPlannerTrack *
hyscan_steer_get_track (HyScanSteer  *steer,
                        gchar       **track_id)
{
  HyScanSteerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_STEER (steer), NULL);

  priv = steer->priv;

  if (track_id != NULL)
    *track_id = g_strdup (priv->track_id);

  return priv->track;
}

/**
 * hyscan_steer_get_track_topo:
 * @steer: указатель на #HyScanSteer
 * @start: (out) (nullable): координаты начала галса во внутренней декартовой системе координат
 * @end: (out) (nullable): координаты конца галса во внутренней декартовой системе координат
 *
 * Функция возвращает информацию о координатах начала и конца плана галса.
 *
 * Returns: %TRUE, если активный план галса установлен
 */
gboolean
hyscan_steer_get_track_topo (HyScanSteer          *steer,
                             HyScanGeoCartesian2D *start,
                             HyScanGeoCartesian2D *end)
{
  HyScanSteerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_STEER (steer), FALSE);

  priv = steer->priv;

  if (priv->track == NULL)
    return FALSE;

  if (start != NULL)
    *start = priv->start;

  if (end != NULL)
    *end = priv->end;

  return TRUE;
}

/**
 * hyscan_steer_get_track_info:
 * @steer: указатель на #HyScanSteer
 * @azimuth: (out) (nullable): направление галса, градусы
 * @length: (out) (nullable): длина галса, метры
 *
 * Функция возвращает информацию о плане галса.
 *
 * Returns: %TRUE, если активный план галса установлен
 */
gboolean
hyscan_steer_get_track_info (HyScanSteer *steer,
                             gdouble     *azimuth,
                             gdouble     *length)
{
  HyScanSteerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_STEER (steer), FALSE);

  priv = steer->priv;

  if (priv->track == NULL)
    return FALSE;

  if (azimuth != NULL)
    *azimuth = priv->azimuth;

  if (length != NULL)
    *length = priv->length;

  return TRUE;
}

/**
 * hyscan_steer_sensor_set_offset:
 * @steer: указатель на #HyScanSteer
 * @offset: смещение антенны гидролокатора
 *
 * Устанавливает смещение антенны гидролокатора. Смещение используется для
 * внесения поправки в местоположение антенны.
 */
void
hyscan_steer_sensor_set_offset (HyScanSteer               *steer,
                                const HyScanAntennaOffset *offset)
{
  HyScanSteerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_STEER (steer));
  priv = steer->priv;

  g_clear_pointer (&priv->offset, hyscan_antenna_offset_free);
  priv->offset = hyscan_antenna_offset_copy (offset);
}

/**
 * hyscan_steer_set_threshold:
 * @steer: указатель на #HyScanSteer
 * @threshold: допустимое отклонение поперёк галса, метры
 *
 * Функция устанавливает допустимое отклонение положения судна от линии планового галса.
 */
void
hyscan_steer_set_threshold (HyScanSteer *steer,
                            gdouble     threshold)
{
  g_return_if_fail (HYSCAN_IS_STEER (steer));

  steer->priv->threshold = threshold;
  g_object_notify_by_pspec (G_OBJECT (steer), hyscan_steer_prop_threshold);
}

/**
 * hyscan_steer_get_threshold:
 * @steer: указатель на #HyScanSteer
 *
 * Функция возвращает допустимое отклонение положения судна от линии планового галса.
 *
 * Returns: допустимое отклонение поперёк галса, метры
 */
gdouble
hyscan_steer_get_threshold (HyScanSteer *steer)
{
  g_return_val_if_fail (HYSCAN_IS_STEER (steer), FALSE);

  return steer->priv->threshold;
}

/**
 * hyscan_steer_set_autostart:
 * @steer: указатель на #HyScanSteer
 * @autostart: признак включения автоматического старта гидролокатора
 *
 * Функция включает систему автоматического старта гидролокатора при выходе на плановый галс.
 *
 * Если @autostart = %TRUE, то гидролокатор будет переведён в рабочий режим автоматически как только судно выйдет на
 * активный плановый галс.
 */
void
hyscan_steer_set_autostart (HyScanSteer *steer,
                            gboolean     autostart)
{
  HyScanSteerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_STEER (steer));
  priv = steer->priv;

  if (autostart && priv->sonar_state == NULL)
    {
      g_warning ("HyScanSteer: sonar must implement HyScanSonarState to enable autostart");
      return;
    }

  steer->priv->autostart = autostart;
  g_object_notify_by_pspec (G_OBJECT (steer), hyscan_steer_prop_autostart);
}

/**
 * hyscan_steer_get_autostart:
 * @steer: указатель на #HyScanSteer
 *
 * Функция возвращает %TRUE, если включена система автоматического старта гидролокатора при выходе на плановый галс.
 *
 * Returns: признак включенного автостарта.
 */
gboolean
hyscan_steer_get_autostart (HyScanSteer *steer)
{
  g_return_val_if_fail (HYSCAN_IS_STEER (steer), FALSE);

  return steer->priv->autostart;
}

/**
 * hyscan_steer_set_autoselect:
 * @steer: указатель на #HyScanSteer
 * @autoselect: признак включения автоматического выбора галса для навигации
 *
 * Функция включает систему автоматического выбора галса для навигации.
 *
 */
void
hyscan_steer_set_autoselect (HyScanSteer *steer,
                            gboolean     autoselect)
{
  HyScanSteerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_STEER (steer));
  priv = steer->priv;

  if (autoselect && priv->sonar_state == NULL)
    {
      g_warning ("HyScanSteer: sonar must implement HyScanSonarState to enable autoselect");
      return;
    }

  steer->priv->autoselect = autoselect;
  g_object_notify_by_pspec (G_OBJECT (steer), hyscan_steer_prop_autoselect);
}

/**
 * hyscan_steer_get_autoselect:
 * @steer: указатель на #HyScanSteer
 *
 * Функция возвращает %TRUE, если включена система автоматического выбора галса для навигации.
 *
 * Returns: признак включенного автостарта.
 */
gboolean
hyscan_steer_get_autoselect (HyScanSteer *steer)
{
  g_return_val_if_fail (HYSCAN_IS_STEER (steer), FALSE);

  return steer->priv->autoselect;
}

/**
 * hyscan_steer_calc_point:
 * @point: указатель на #HyScanSteerPoint
 * @steer: указатель на #HyScanSteer
 *
 * Функция переводит положение точки #HyScanSteerPoint.nav_data в систему координат активного плана галса
 * и записывает их в структуру @point.
 *
 * Порядок аргументов установлен в нестандартном порядке, чтобы функцию было проще использовать как #GFunc,
 * например, в g_list_foreach().
 */
void
hyscan_steer_calc_point (HyScanSteerPoint *point,
                         HyScanSteer      *steer)
{
  HyScanSteerPrivate *priv = steer->priv;
  gdouble distance;
  gint side;
  gdouble distance_to_end;
  HyScanGeoCartesian2D track_end;
  HyScanGeoCartesian2D ship_pos;

  point->d_angle = point->nav_data.heading - DEG2RAD (priv->azimuth);

  if (!hyscan_geo_geo2topoXY (priv->geo, &ship_pos, point->nav_data.coord))
    return;

  /* Смещение антенны учитываем только для расчёта её местоположения.
   * В качестве курса оставляем курс судна, т.к. иначе показания виджета становятся неочевидными. */
  if (priv->offset != NULL)
    {
      gdouble cosa, sina;

      cosa = cos (DEG2RAD (point->d_angle));
      sina = sin (DEG2RAD (point->d_angle));
      point->position.x = ship_pos.x - priv->offset->starboard * sina + priv->offset->forward * cosa;
      point->position.y = ship_pos.y - priv->offset->starboard * cosa - priv->offset->forward * sina;
    }
  else
    {
      point->position = ship_pos;
    }

  distance = hyscan_cartesian_distance_to_line (&priv->start, &priv->end, &point->position, &point->track);
  side = hyscan_cartesian_side (&priv->start, &priv->end, &point->position);
  point->d_y = side * distance;

  hyscan_geo_geo2topoXY (priv->geo, &track_end, priv->track->plan.end);
  distance_to_end = hyscan_cartesian_distance (&track_end, &point->track);
  point->time_left = distance_to_end / priv->track->plan.velocity;
}

/**
 * hyscan_steer_point_copy:
 * @point: указатель на структуру #HyScanSteerPoint
 *
 * Функция создаёт копию структурыы #HyScanSteerPoint
 *
 * Returns: копия структуры, для удаления hyscan_steer_point_free().
 */
HyScanSteerPoint *
hyscan_steer_point_copy (const HyScanSteerPoint *point)
{
  return g_slice_dup (HyScanSteerPoint, point);
}

/**
 * hyscan_steer_point_free:
 * @point: указатель на структуру #HyScanSteerPoint
 *
 * Функция удаляет структуру @point.
 */
void
hyscan_steer_point_free (HyScanSteerPoint *point)
{
  g_slice_free (HyScanSteerPoint, point);
}
