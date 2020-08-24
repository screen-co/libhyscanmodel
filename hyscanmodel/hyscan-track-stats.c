/* hyscan-track-stats.c
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
 * SECTION: hyscan-track-stats
 * @Short_description: класс асинхронного рассчета навигационной статистики для галсов
 * @Title: HyScanTrackStats
 *
 * Класс предназначен для асинхронного рассчета статистики движения по галсу, в частности
 * средних значений скорости и курса, их среднеквадратических отклонений и качества навигации.
 *
 * При наличии плана галса, вычисляется время нахождения на плановом галсе и статистика
 * отклонения от плана. Используется либо план, который указан в связанном плановом
 * галсе (см. #HyScanPlannerTrack.records), иначе - план, установленный при записи галса
 * #HyScanTrackInfo.plan.
 *
 * Полный список параметров описан в структуре HyScanTrackStatsInfo.
 *
 * Обрабатываются только те галсы, по которым запись уже завершена.
 *
 * Чтобы получать актуальную статистику, необходимо подключиться к сигналу "changed" объекта класса.
 * Функция hyscan_track_stats_get() возвращает хэш-таблицу с текущей статистикой по галсам.
 */

#include "hyscan-track-stats.h"
#include <hyscan-stats.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-planner.h>
#include <gio/gio.h>
#include <math.h>
#include <string.h>
#include <hyscan-map-track-param.h>

#define KNOTS_TO_METER_PER_SEC  (1852. / 3600.)
#define FIT_ANGLE(x) ((x) < 0.0 ? (x) + 360.0 : ((x) >= 360.0 ? (x) - 360.0 : (x)))

#define QUALITY_VELOCITY0     0.2 /* Среднеквадратичное отклонение скорости, для которого качество равно 0.5. */
#define QUALITY_ANGLE0        3.0 /* Среднеквадратичное отклонение курса, для которого качество равно 0.5. */
#define QUALITY_N             3.  /* Скорость убывания качества. */
#define QUALITY_ANGLE_WEIGHT  2.0 /* Вес качества курса, по сравнению со скоростью. Вес скорости = 1. */

enum
{
  PROP_O,
  PROP_DB_INFO,
  PROP_MODEL,
  PROP_CACHE,
};

enum
{
  SIGNAL_TRACKS_CHANGED,
  SIGNAL_LAST
};

typedef struct
{
  guint32               start;   /* Индекс начала диапазона. */
  guint32               end;     /* Индекс конца диапазона. */
  HyScanTrackStatsInfo *sinfo;   /* Статистка по указанному диапазону. */
} HyScanTrackStatsInternal;

struct _HyScanTrackStatsPrivate
{
  HyScanDBInfo                *db_info;       /* HyScanDBInfo для получения информации о галсах. */
  HyScanPlannerModel          *model;         /* Модель объектов планировщика. */
  HyScanCache                 *cache;         /* Кэш. */

  GHashTable                  *tracks;        /* Таблица { track_id: HyScanTrackStatsInfo } */
  GMutex                       mutex;         /* Блокировка доступа к таблице tracks. */
  HyScanCancellable           *cancellable;   /* Объект отмены задания по обработке галсов. */
};

static void            hyscan_track_stats_set_property         (GObject                  *object,
                                                                guint                     prop_id,
                                                                const GValue             *value,
                                                                GParamSpec               *pspec);
static void            hyscan_track_stats_object_constructed   (GObject                  *object);
static void            hyscan_track_stats_object_finalize      (GObject                  *object);
static GHashTable *    hyscan_track_stats_ht_copy              (GHashTable               *ht);
static void            hyscan_track_stats_tracks_changed       (HyScanTrackStats         *track_stats);
static void            hyscan_track_stats_load_spd_trk         (HyScanTrackStatsInternal *intern,
                                                                HyScanNavData            *spd_data,
                                                                HyScanNavData            *trk_data,
                                                                HyScanCancellable        *hcancellable);
static void            hyscan_track_stats_load_xy              (HyScanTrackStatsInternal *intern,
                                                                HyScanNavData            *lat_data,
                                                                HyScanNavData            *lon_data,
                                                                HyScanCancellable        *hcancellable);
static void            hyscan_track_stats_load_range           (HyScanTrackStatsInternal *intern,
                                                                HyScanNavData            *lat_data,
                                                                HyScanNavData            *lon_data,
                                                                HyScanCancellable        *hcancellable);
static void            hyscan_track_stats_load_track           (HyScanTrackStats         *track_stats,
                                                                HyScanCancellable        *cancellable,
                                                                HyScanDB                 *db,
                                                                const gchar              *project,
                                                                HyScanTrackStatsInfo     *sinfo);
static void            hyscan_track_stats_run                  (GTask                    *task,
                                                                HyScanTrackStats         *track_stats,
                                                                gpointer                  task_data,
                                                                GCancellable             *cancellable);
static gdouble         hyscan_track_stats_var                  (gdouble                   avg,
                                                                const gdouble            *values,
                                                                gsize                     n_values);
static gdouble         hyscan_track_stats_avg                  (const gdouble            *values,
                                                                gsize                     n_values);
static void            hyscan_track_stats_ready                (HyScanTrackStats         *track_stats,
                                                                GTask                    *task,
                                                                gpointer                  user_data);
static inline HyScanTrackPlan *
                       hyscan_track_stats_get_plan             (GHashTable               *tracks_map,
                                                                HyScanTrackInfo          *info);
static gdouble         hyscan_track_stats_calc_quality         (gdouble                   speed_var,
                                                                gdouble                   angle_var);

static guint           hyscan_track_stats_signals[SIGNAL_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackStats, hyscan_track_stats, G_TYPE_OBJECT)

static void
hyscan_track_stats_class_init (HyScanTrackStatsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_track_stats_set_property;

  object_class->constructed = hyscan_track_stats_object_constructed;
  object_class->finalize = hyscan_track_stats_object_finalize;

  g_object_class_install_property (object_class, PROP_DB_INFO,
    g_param_spec_object ("db-info", "DB Info", "HyScanDBInfo", HYSCAN_TYPE_DB_INFO,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "ObjectModel", "HyScanObjectModel", HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanTrackStats::changed:
   * @track_stats: указатель на #HyScanTrackStats
   *
   * Сигнал посылается при изменении списка галсов.
   */
  hyscan_track_stats_signals[SIGNAL_TRACKS_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_TRACK_STATS, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_track_stats_init (HyScanTrackStats *track_stats)
{
  track_stats->priv = hyscan_track_stats_get_instance_private (track_stats);
}

static void
hyscan_track_stats_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanTrackStats *track_stats = HYSCAN_TRACK_STATS (object);
  HyScanTrackStatsPrivate *priv = track_stats->priv;

  switch (prop_id)
    {
    case PROP_DB_INFO:
      priv->db_info = g_value_dup_object (value);
      break;

    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_track_stats_object_constructed (GObject *object)
{
  HyScanTrackStats *track_stats = HYSCAN_TRACK_STATS (object);
  HyScanTrackStatsPrivate *priv = track_stats->priv;

  G_OBJECT_CLASS (hyscan_track_stats_parent_class)->constructed (object);

  g_mutex_init (&priv->mutex);
  priv->tracks = hyscan_track_stats_ht_copy (NULL);
  g_signal_connect_swapped (priv->db_info, "tracks-changed", G_CALLBACK (hyscan_track_stats_tracks_changed), track_stats);

  if (priv->model != NULL)
    g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_track_stats_tracks_changed), track_stats);
}

static void
hyscan_track_stats_object_finalize (GObject *object)
{
  HyScanTrackStats *track_stats = HYSCAN_TRACK_STATS (object);
  HyScanTrackStatsPrivate *priv = track_stats->priv;

  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->db_info);
  g_clear_object (&priv->model);
  g_clear_object (&priv->cache);

  g_mutex_clear (&priv->mutex);
  g_hash_table_destroy (priv->tracks);

  G_OBJECT_CLASS (hyscan_track_stats_parent_class)->finalize (object);
}

/* Коллбэк функция по результатам обработки галсов. */
static void
hyscan_track_stats_ready (HyScanTrackStats *track_stats,
                          GTask            *task,
                          gpointer          user_data)
{
  HyScanTrackStatsPrivate *priv = track_stats->priv;
  GHashTable *tracks, *tracks_old;

  tracks = g_task_propagate_pointer (task, NULL);
  if (tracks == NULL)
    return;

  /* Подменяем хэш-таблицу. */
  g_mutex_lock (&priv->mutex);
  tracks_old = priv->tracks;
  priv->tracks = tracks;
  g_mutex_unlock (&priv->mutex);

  g_hash_table_destroy (tracks_old);

  g_signal_emit (track_stats, hyscan_track_stats_signals[SIGNAL_TRACKS_CHANGED], 0);
}

/* Рассчитывает среднее значение. */
static gdouble
hyscan_track_stats_avg (const gdouble *values,
                        gsize          n_values)
{
  gsize i;
  gdouble sum = 0.0;

  if (n_values == 0)
    return 0.0;

  for (i = 0; i < n_values; i++)
    sum += values[i];

  return sum / n_values;
}

/* Рассчитывает среднеквадратичное отклонение. */
static gdouble
hyscan_track_stats_var (gdouble        avg,
                        const gdouble *values,
                        gsize          n_values)
{
  gsize i;
  gdouble sum = 0.0;

  if (n_values == 0)
    return 0.0;

  for (i = 0; i < n_values; i++)
    sum += (values[i] - avg) * (values[i] - avg);

  return sqrt (sum / n_values);
}

/* Рассчитывает время прохождения галса. */
static void
hyscan_track_stats_load_time (HyScanTrackStatsInternal *intern,
                              HyScanNavData            *ndata,
                              HyScanCancellable        *hcancellable)
{
  HyScanTrackStatsInfo *sinfo = intern->sinfo;
  gint64 i;
  gboolean start_found = FALSE, end_found = FALSE;
  gint64 start_time = 0, end_time = 0;

  for (i = intern->start; i <= intern->end && !start_found; i++)
    start_found = hyscan_nav_data_get (ndata, hcancellable, i, &start_time, NULL);

  for (i = intern->end; i >= intern->start && !end_found; i--)
    end_found = hyscan_nav_data_get (ndata, hcancellable, i, &end_time, NULL);

  if (!start_found || !end_found)
    return;

  sinfo->start_time = start_time;
  sinfo->end_time = end_time;
}

/* Функция оценки качества по среднеквадратичному отклонению x:
 *
 *   q(x) = pow (b / (x + b), n).
 *
 * Функция монотонно убывает, при этом q(0) = 1, q(∞) = 0.
 *
 * Параметр n определяет скорость убывания, параметр b подбирается так,
 * что для некоторого заданного значения x0 выполняется q(x0) = 0.5:
 *
 *   b = x0 / (pow (2, 1 / n) - 1).
 */
static gdouble
hyscan_track_stats_calc_quality (gdouble speed_var,
                                 gdouble angle_var)
{
  gdouble quality;
  gdouble v_b, a_b;
  gdouble velocity_q, angle_q;

  v_b = QUALITY_VELOCITY0 / (pow (2, 1. / QUALITY_N) - 1);
  velocity_q = pow (v_b / (speed_var + v_b), QUALITY_N);

  a_b = QUALITY_ANGLE0 / (pow (2, 1. / QUALITY_N) - 1);
  angle_q = pow (a_b / (angle_var + a_b), QUALITY_N);

  quality = (velocity_q + angle_q * QUALITY_ANGLE_WEIGHT) / (1.0 + QUALITY_ANGLE_WEIGHT);

  return quality;
}

/* Рассчитывает скорость и азимут движения. */
static void
hyscan_track_stats_load_spd_trk (HyScanTrackStatsInternal *intern,
                                 HyScanNavData            *spd_data,
                                 HyScanNavData            *trk_data,
                                 HyScanCancellable        *hcancellable)
{
  HyScanTrackStatsInfo *sinfo = intern->sinfo;
  gsize range_size;
  guint32 i;
  gint64 time_prev;
  GArray *velocity_arr, *trk_arr, *x_arr;

  range_size = intern->end - intern->start + 1;
  velocity_arr = g_array_sized_new (FALSE, FALSE, sizeof (gdouble), range_size);
  trk_arr = g_array_sized_new (FALSE, FALSE, sizeof (gdouble), range_size);
  x_arr = g_array_sized_new (FALSE, FALSE, sizeof (gdouble), range_size);

  time_prev = G_MININT64;
  for (i = intern->start; i <= intern->end; i++)
    {
      gdouble velocity, angle;
      gint64 time;

      if (hyscan_nav_data_get (spd_data, hcancellable, i, &time, &velocity))
        {
          g_array_append_val (velocity_arr, velocity);

          if (time_prev != G_MININT64)
            sinfo->length += velocity * (gdouble) (time - time_prev) * 1e-6;

          time_prev = time;
        }

        if (hyscan_nav_data_get (trk_data, hcancellable, i, NULL, &angle))
          g_array_append_val (trk_arr, angle);
    }

  sinfo->speed = hyscan_track_stats_avg ((const gdouble *) velocity_arr->data, velocity_arr->len);
  sinfo->speed_var = hyscan_track_stats_var (sinfo->speed, (const gdouble *) velocity_arr->data, velocity_arr->len);
  sinfo->angle = hyscan_stats_avg_circular ((const gdouble *) trk_arr->data, trk_arr->len);
  sinfo->angle_var = hyscan_stats_var_circular (sinfo->angle, (const gdouble *) trk_arr->data, trk_arr->len);

  /* Переводим из узлов в м/с. */
  sinfo->speed *= KNOTS_TO_METER_PER_SEC;
  sinfo->speed_var *= KNOTS_TO_METER_PER_SEC;
  sinfo->length *= KNOTS_TO_METER_PER_SEC;

  sinfo->quality = hyscan_track_stats_calc_quality (sinfo->speed_var, sinfo->angle_var);

  g_array_free (velocity_arr, TRUE);
  g_array_free (trk_arr, TRUE);
  g_array_free (x_arr, TRUE);
}

/* Загружает отклонения по y (на правый борт) от планового движения и
 * покрытие по x (по направлению движения). */
static void
hyscan_track_stats_load_xy (HyScanTrackStatsInternal *intern,
                            HyScanNavData            *lat_data,
                            HyScanNavData            *lon_data,
                            HyScanCancellable        *hcancellable)
{
  HyScanTrackStatsInfo *sinfo = intern->sinfo;
  GArray *y_arr;
  guint32 i;
  gdouble min_x = G_MAXDOUBLE, max_x = -G_MAXDOUBLE;

  HyScanTrackPlan *plan;
  HyScanGeo *geo = NULL;

  plan = sinfo->plan;
  if (plan != NULL)
    geo = hyscan_planner_track_geo (plan, NULL);

  y_arr = g_array_sized_new (FALSE, FALSE, sizeof (gdouble), intern->end - intern->start + 1);
  for (i = intern->start; i <= intern->end; i++)
    {
      HyScanGeoPoint coord;
      HyScanGeoCartesian2D topo;

      if (hyscan_nav_data_get (lat_data, hcancellable, i, NULL, &coord.lat) &&
          hyscan_nav_data_get (lon_data, hcancellable, i, NULL, &coord.lon))
        {
          /* Если у нас не было плана, то создаем гео-объект с точкой начала
           * из первой попавшейся точки и средним азимутом движения. */
          if (geo == NULL)
            {
              HyScanGeoGeodetic origin;

              origin.lat = coord.lat;
              origin.lon = coord.lon;
              origin.h = sinfo->angle;

              geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
            }

          hyscan_geo_geo2topoXY0 (geo, &topo, coord);
          g_array_append_val (y_arr, topo.y);
          min_x = MIN (min_x, topo.x);
          max_x = MAX (max_x, topo.x);
        }
    }

  /* Если найдена хотя бы одна точка с координатами на галсе. */
  if (min_x != G_MAXDOUBLE)
    {
      sinfo->x_min = min_x;
      sinfo->x_max = max_x;
      sinfo->x_length = max_x - min_x;
    }

  sinfo->y_avg = hyscan_track_stats_avg ((const gdouble *) y_arr->data, y_arr->len);
  sinfo->y_var = hyscan_track_stats_var (sinfo->y_avg, (const gdouble *) y_arr->data, y_arr->len);

  g_array_free (y_arr, TRUE);
  g_clear_object (&geo);
}

/* Ищет план, который следует использовать с указанным галсом. */
static inline HyScanTrackPlan *
hyscan_track_stats_get_plan (GHashTable      *tracks_map,
                             HyScanTrackInfo *info)
{
  HyScanPlannerTrack *plan_track;

  plan_track = g_hash_table_lookup (tracks_map, info->id);

  return hyscan_track_plan_copy (plan_track != NULL ? &plan_track->plan : info->plan);
}

/* Определяет диапазон индексов внутри галса, по которым будет производиться рассчёт.
 * Если галс имеет информацию о плане движения, то учитывается только тот период,
 * когда судно находилось на плановом галсе. */
static void
hyscan_track_stats_load_range (HyScanTrackStatsInternal *stats_intern,
                               HyScanNavData            *lat_data,
                               HyScanNavData            *lon_data,
                               HyScanCancellable        *hcancellable)
{
  HyScanTrackStatsInfo *sinfo = stats_intern->sinfo;
  HyScanGeo *geo = NULL;
  gint64 i;
  guint32 start, end;

  HyScanGeoPoint coord;
  HyScanGeoCartesian2D topo;
  gdouble length;

  gint64 min_i = -1, max_i = -1, prev_i = -1;
  gdouble prev_x = 0;
  gdouble min_x, max_x;

  if (!hyscan_nav_data_get_range (lat_data, &start, &end))
    return;

  /* Если нет информации о плане движения, то берём весь доступный период. */
  if (sinfo->plan == NULL)
    {
      stats_intern->start = start;
      stats_intern->end = end;

      return;
    }

  /* Ось X направлена вдоль планового галса.
   * x = 0 соответствует началу планового галса,
   * x = length соответствует концу галса. */
  geo = hyscan_planner_track_geo (sinfo->plan, NULL);
  hyscan_geo_geo2topoXY0 (geo, &topo, sinfo->plan->end);
  length = topo.x;

  /* Ищем точки на галсе, которые соответствуют началу и концу планового галса. */
  min_x = G_MAXDOUBLE;
  max_x = -G_MAXDOUBLE;
  for (i = start; i <= end; i++)
    {
      if (!hyscan_nav_data_get (lat_data, hcancellable, i, NULL, &coord.lat) ||
          !hyscan_nav_data_get (lon_data, hcancellable, i, NULL, &coord.lon))
        {
          continue;
        }

      if (!hyscan_geo_geo2topoXY0 (geo, &topo, coord))
        continue;

      /* Пересекли линию старта планового галса. */
      if (prev_i != -1 && prev_x <= 0 && 0 <= topo.x)
        {
          min_i = prev_i;
          min_x = prev_x;
        }

      /* Пересекли линию старта, но движемся в противоположном направлении. */
      else if (prev_i != -1 && topo.x <= 0 && 0 <= prev_x)
        {
          min_i = i;
          min_x = topo.x;
        }

      /* Находимся внутри планового галса. */
      else if (topo.x < min_x && 0 <= topo.x && topo.x <= length)
        {
          min_i = i;
          min_x = topo.x;
        }

      /* Пересекли линию финиша планового галса. */
      if (prev_i != -1 && prev_x <= length && length <= topo.x)
        {
          max_i = i;
          max_x = topo.x;
        }

      /* Пересекли линию финиша, но движемся в противоположном направлении. */
      else if (prev_i != -1 && topo.x <= length && length <= prev_x)
        {
          max_i = prev_i;
          max_x = prev_x;
        }

      /* Находимся внутри планового галса. */
      else if (topo.x > max_x && 0 <= topo.x && topo.x <= length)
        {
          max_i = i;
          max_x = topo.x;
        }

      prev_x = topo.x;
      prev_i = i;
    }

  if (max_i == -1 || min_i == -1)
    goto exit;

  if (min_x <= 0.0 && max_x >= length)
    sinfo->progress = 1.0;
  else
    sinfo->progress = (MIN (length, max_x) - MAX (0.0, min_x)) / length;

  stats_intern->start = MIN (min_i, max_i);
  stats_intern->end = MAX (min_i, max_i);

exit:
  g_clear_object (&geo);
}


/* Рассчтывает статистику по указанному галсу. */
static void
hyscan_track_stats_load_track (HyScanTrackStats     *track_stats,
                               HyScanCancellable    *hcancellable,
                               HyScanDB             *db,
                               const gchar          *project,
                               HyScanTrackStatsInfo *sinfo)
{
  HyScanTrackStatsPrivate *priv = track_stats->priv;
  HyScanTrackInfo *tinfo = sinfo->info;
  HyScanNavData *lat_data, *lon_data, *spd_data, *trk_data;
  HyScanTrackStatsInternal stats_intern;
  HyScanMapTrackParam *track_param;

  hyscan_cancellable_push (hcancellable);

  stats_intern.start = 0;
  stats_intern.end = 0;
  stats_intern.sinfo = sinfo;

  track_param = hyscan_map_track_param_new (NULL, db, project, tinfo->name);
  lat_data = hyscan_map_track_param_get_nav_data (track_param, HYSCAN_NMEA_FIELD_LAT, priv->cache);
  lon_data = hyscan_map_track_param_get_nav_data (track_param, HYSCAN_NMEA_FIELD_LON, priv->cache);
  spd_data = hyscan_map_track_param_get_nav_data (track_param, HYSCAN_NMEA_FIELD_SPEED, priv->cache);
  trk_data = hyscan_map_track_param_get_nav_data (track_param, HYSCAN_NMEA_FIELD_TRACK, priv->cache);
  if (lat_data == NULL)
    goto exit;

  hyscan_track_stats_load_range (&stats_intern, lat_data, lon_data, hcancellable);
  hyscan_cancellable_set (hcancellable, 0/4., 1/.4);
  if (stats_intern.start == stats_intern.end)
    goto exit;

  hyscan_track_stats_load_time (&stats_intern, lat_data, hcancellable);
  hyscan_cancellable_set (hcancellable, 1/4., 2/.4);

  hyscan_track_stats_load_spd_trk (&stats_intern, spd_data, trk_data, hcancellable);
  hyscan_cancellable_set (hcancellable, 2/4., 3/.4);

  hyscan_track_stats_load_xy (&stats_intern, lat_data, lon_data, hcancellable);
  hyscan_cancellable_set (hcancellable, 3/4., 4/.4);

exit:
  hyscan_cancellable_pop (hcancellable);
  g_clear_object (&track_param);
  g_clear_object (&lat_data);
  g_clear_object (&lon_data);
  g_clear_object (&spd_data);
  g_clear_object (&trk_data);
}

/* Асинхронная обработка задачи по загрузке галсов. */
static void
hyscan_track_stats_run (GTask            *task,
                        HyScanTrackStats *track_stats,
                        gpointer          task_data,
                        GCancellable     *cancellable)
{
  HyScanTrackStatsPrivate *priv = track_stats->priv;
  HyScanCancellable *hcancellable = HYSCAN_CANCELLABLE (cancellable);
  HyScanDB *db;
  gchar *project;
  GHashTable *track_infos = NULL;
  GHashTable *track_vars = NULL;
  GHashTable *tracks_map = NULL;
  GHashTable *objects = NULL;
  GHashTableIter iter;
  HyScanTrackInfo *track_info;
  gchar *key;
  guint total, i;

  track_vars = hyscan_track_stats_ht_copy (NULL);
  db = hyscan_db_info_get_db (priv->db_info);
  project = hyscan_db_info_get_project (priv->db_info);

  if (db == NULL || project == NULL)
    goto exit;

  track_infos = hyscan_db_info_get_tracks (priv->db_info);

  /* Таблица соответствия запланированных и записаных галсов. */
  tracks_map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  objects = priv->model != NULL ? hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model)) : NULL;
  if (objects != NULL)
    {
      HyScanPlannerTrack *planner_track;

      g_hash_table_iter_init (&iter, objects);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &planner_track))
        {
          gint j;

          if (!HYSCAN_IS_PLANNER_TRACK (planner_track) || planner_track->records == NULL)
            {
              g_hash_table_iter_remove (&iter);
              continue;
            }

          for (j = 0; planner_track->records[j] != NULL; j++)
            g_hash_table_replace (tracks_map, planner_track->records[j], planner_track);
        }
    }

  i = 0;
  total = g_hash_table_size (track_infos);
  g_hash_table_iter_init (&iter, track_infos);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track_info))
    {
      HyScanTrackStatsInfo *sinfo;

      if (g_cancellable_is_cancelled (cancellable))
        goto exit;

      /* Учитываем прогресс выполнения. */
      hyscan_cancellable_set_total (hcancellable, i++, 0, total);

      /* Пропускаем галсы, по которым еще идет запись. */
      if (track_info->record || track_info->id == NULL)
        continue;

      g_hash_table_iter_steal (&iter);
      g_free (key);

      sinfo = g_slice_new0 (HyScanTrackStatsInfo);
      sinfo->info = track_info;
      sinfo->plan = hyscan_track_stats_get_plan (tracks_map, track_info);
      hyscan_track_stats_load_track (track_stats, hcancellable, db, project, sinfo);

      g_hash_table_insert (track_vars, g_strdup (track_info->id), sinfo);
    }

exit:
  g_clear_object (&db);
  g_free (project);
  g_task_return_pointer (task, track_vars, (GDestroyNotify) g_hash_table_destroy);
  g_clear_pointer (&track_infos, g_hash_table_destroy);
  g_clear_pointer (&tracks_map, g_hash_table_destroy);
  g_clear_pointer (&objects, g_hash_table_destroy);
}

static void
hyscan_track_stats_tracks_changed (HyScanTrackStats *track_stats)
{
  HyScanTrackStatsPrivate *priv = track_stats->priv;
  GTask *task;

  /* Отменяем текущую задачу, если такая есть. */
  g_cancellable_cancel (G_CANCELLABLE (priv->cancellable));
  g_clear_object (&priv->cancellable);

  /* Отправляем задачу на расчет статистики в отдельный поток. */
  priv->cancellable = hyscan_cancellable_new ();
  task = g_task_new (track_stats, G_CANCELLABLE (priv->cancellable), (GAsyncReadyCallback) hyscan_track_stats_ready, NULL);
  g_task_run_in_thread (task, (GTaskThreadFunc) hyscan_track_stats_run);
  g_object_unref (task);
}

static GHashTable *
hyscan_track_stats_ht_copy (GHashTable *ht)
{
  GHashTable *copy;
  GHashTableIter iter;
  gchar *key;
  HyScanTrackStatsInfo *value;

  copy = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_track_stats_info_free);
  if (ht == NULL)
    return copy;

  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value))
    g_hash_table_insert (copy, g_strdup (key), hyscan_track_stats_info_copy (value));

  return copy;
}

/**
 * hyscan_track_stats_new:
 * @db_info: указатель на #HyScanDbInfo
 * @model: указатель на #HyScanObjectModel
 * @cache: кэш #HyScanCache
 *
 * Функция создает объект навигационной статистики галса.
 *
 * Returns: (transfer full): новый объект #HyScanTrackStats, для удаления g_object_unref().
 */
HyScanTrackStats *
hyscan_track_stats_new (HyScanDBInfo       *db_info,
                        HyScanPlannerModel *model,
                        HyScanCache        *cache)
{
  return g_object_new (HYSCAN_TYPE_TRACK_STATS,
                       "db-info", db_info,
                       "model", model,
                       "cache", cache,
                       NULL);
}

/**
 * hyscan_track_stats_get:
 * @track_stats: указатель на HyScanTrackStats
 *
 * Функция возвращает хэш-таблицу со статистикой галсов. Ключ - иденитификатор галса,
 * значение - структура #HyScanTrackStatsInfo.
 *
 * Returns: (transfer full) (element-type HyScanTrackStatsInfo): статистика галсов,
 *   для удаления g_hash_table_unref()
 */
GHashTable *
hyscan_track_stats_get (HyScanTrackStats *track_stats)
{
  HyScanTrackStatsPrivate *priv = track_stats->priv;
  GHashTable *htable;

  g_return_val_if_fail (HYSCAN_IS_TRACK_STATS (track_stats), NULL);

  g_mutex_lock (&priv->mutex);
  htable = hyscan_track_stats_ht_copy (priv->tracks);
  g_mutex_unlock (&priv->mutex);

  return htable;
}

/**
 * hyscan_track_stats_info_copy:
 * @info: указатель на #HyScanTrackStatsInfo
 *
 * Функция создает копию структуры #HyScanTrackStatsInfo.
 *
 * Returns: (transfer full): копия структуры, для удаления hyscan_track_stats_info_free().
 */
HyScanTrackStatsInfo *
hyscan_track_stats_info_copy (const HyScanTrackStatsInfo *info)
{
  HyScanTrackStatsInfo *copy;

  if (info == NULL)
    return NULL;

  copy = g_slice_dup (HyScanTrackStatsInfo, info);
  copy->info = info->info != NULL ? hyscan_db_info_track_info_copy (info->info) : NULL;
  copy->plan = hyscan_track_plan_copy (info->plan);

  return copy;
}

/**
 * hyscan_track_stats_info_free:
 * @info: указатель на #HyScanTrackStatsInfo
 *
 * Функция удаляет структуру #HyScanTrackStatsInfo.
 */
void
hyscan_track_stats_info_free (HyScanTrackStatsInfo *info)
{
  if (info == NULL)
    return;

  hyscan_db_info_track_info_free (info->info);
  hyscan_track_plan_free (info->plan);
  g_slice_free (HyScanTrackStatsInfo, info);
}
