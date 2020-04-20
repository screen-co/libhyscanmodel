/* hyscan-track-stats.h
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

#ifndef __HYSCAN_TRACK_STATS_H__
#define __HYSCAN_TRACK_STATS_H__

#include <hyscan-db-info.h>
#include <hyscan-cache.h>
#include <hyscan-planner-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TRACK_STATS             (hyscan_track_stats_get_type ())
#define HYSCAN_TRACK_STATS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TRACK_STATS, HyScanTrackStats))
#define HYSCAN_IS_TRACK_STATS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TRACK_STATS))
#define HYSCAN_TRACK_STATS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TRACK_STATS, HyScanTrackStatsClass))
#define HYSCAN_IS_TRACK_STATS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TRACK_STATS))
#define HYSCAN_TRACK_STATS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TRACK_STATS, HyScanTrackStatsClass))

typedef struct _HyScanTrackStats HyScanTrackStats;
typedef struct _HyScanTrackStatsPrivate HyScanTrackStatsPrivate;
typedef struct _HyScanTrackStatsClass HyScanTrackStatsClass;
typedef struct _HyScanTrackStatsInfo HyScanTrackStatsInfo;

struct _HyScanTrackStats
{
  GObject parent_instance;

  HyScanTrackStatsPrivate *priv;
};

struct _HyScanTrackStatsClass
{
  GObjectClass parent_class;
};

/**
 * HyScanTrackStatsInfo:
 * @info: информация о галсе #HyScanTrackInfo
 * @plan: (nullable): план, по которому рассчитана статистика
 * @start_time: время начала учитываемого диапазона в канале навигационных данных, мкс
 * @end_time: время конца учитываемого диапазона в канале навигационных данных, мкс
 * @progress: доля покрытия запланированного галса по расстоянию, от 0 до 1
 * @speed: средняя скорость, м/с
 * @speed_var: среднеквадратическое отклонение скорости, м/с
 * @angle: средний курс движения, градусы
 * @angle_var: среднеквадратическое отклонение курса, градусы
 * @length: длина галса (криволинейная), м
 * @x_length: длина галса (прямолинейная вдоль оси X), @x_max - @x_min, м
 * @x_min: минимальное значение координаты по оси X
 * @x_max: максимальное значение координаты по оси X
 * @y_avg: среднее значение координаты Y, м
 * @y_var: среднеквадратическое отклонение отклонения поперек оси X, м
 * @quality: оценка качества галса, условные единицы от 0 до 1
 *
 * Статистика по движению на галсе и отклонению от плана за период [start_time, end_time].
 *
 * При наличии плана период устанавливается в соответствии с нахождением судна
 * на плановом галсе. Если плана нет, то используется весь период, за который доступны
 * навигационные данные.
 *
 * В случае наличия плана (@plan != NULL):
 * - ось X по направлению запланированного движения, начало координат в точке начала планового галса;
 * если план отсутствует:
 * - ось X по направлению среднего значения курса, начало коориданат в первой точке рассматриваемого интервала.
 *
 */
struct _HyScanTrackStatsInfo
{
  HyScanTrackInfo    *info;
  HyScanTrackPlan    *plan;

  gint64              start_time;
  gint64              end_time;
  gdouble             progress;

  gdouble             speed;
  gdouble             speed_var;
  gdouble             angle;
  gdouble             angle_var;
  gdouble             length;

  gdouble             x_length;
  gdouble             x_min;
  gdouble             x_max;
  gdouble             y_avg;
  gdouble             y_var;
  gdouble             quality;
};

HYSCAN_API
GType                  hyscan_track_stats_get_type          (void);

HYSCAN_API
HyScanTrackStats *     hyscan_track_stats_new               (HyScanDBInfo               *db_info,
                                                             HyScanPlannerModel         *planner_model,
                                                             HyScanCache                *cache);

HYSCAN_API
GHashTable *           hyscan_track_stats_get               (HyScanTrackStats           *tvar);

HYSCAN_API
HyScanTrackStatsInfo * hyscan_track_stats_info_copy         (const HyScanTrackStatsInfo *info);

HYSCAN_API
void                   hyscan_track_stats_info_free         (HyScanTrackStatsInfo       *info);

G_END_DECLS

#endif /* __HYSCAN_TRACK_STATS_H__ */
