/* hyscan-planner-stats.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_PLANNER_STATS_H__
#define __HYSCAN_PLANNER_STATS_H__

#include <hyscan-planner-model.h>
#include "hyscan-track-stats.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER_STATS             (hyscan_planner_stats_get_type ())
#define HYSCAN_PLANNER_STATS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER_STATS, HyScanPlannerStats))
#define HYSCAN_IS_PLANNER_STATS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER_STATS))
#define HYSCAN_PLANNER_STATS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER_STATS, HyScanPlannerStatsClass))
#define HYSCAN_IS_PLANNER_STATS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER_STATS))
#define HYSCAN_PLANNER_STATS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER_STATS, HyScanPlannerStatsClass))

typedef struct _HyScanPlannerStats HyScanPlannerStats;
typedef struct _HyScanPlannerStatsPrivate HyScanPlannerStatsPrivate;
typedef struct _HyScanPlannerStatsClass HyScanPlannerStatsClass;
typedef struct _HyScanPlannerStatsZone HyScanPlannerStatsZone;
typedef struct _HyScanPlannerStatsTrack HyScanPlannerStatsTrack;

struct _HyScanPlannerStats
{
  GObject parent_instance;

  HyScanPlannerStatsPrivate *priv;
};

struct _HyScanPlannerStatsClass
{
  GObjectClass parent_class;
};

/**
 * HyScanPlannerStatsZone:
 * @id: идентификтор объекта в БД или %NULL
 * @object: стуктура с информацией о зоне полигона #HyScanPlannerZone или %NULL
 * @tracks: (element-type HyScanPlannerStatsTrack): хэш-таблица статистики по каждому плановому галсу в этой зоне
 * @progress: прогресс выполнения, от 0.0 до 1.0
 * @quality: качество выполнения, от 0.0 до 1.0
 * @length: общая длина плановых галсов
 * @velocity: средняя плановая скорость движения
 * @time: оценка времени для покрытия всех галсов
 *
 * Статистика по схеме галсов внутри полигона.
 */
struct _HyScanPlannerStatsZone
{
  gchar             *id;
  HyScanPlannerZone *object;
  GHashTable        *tracks;

  gdouble            progress;
  gdouble            quality;
  gdouble            length;
  gdouble            time;
  gdouble            velocity;
};

/**
 * HyScanPlannerStatsTrack:
 * @id: идентификтор объекта в БД
 * @object: стуктура с информацией о плане галса #HyScanPlannerTrack
 * @records: (element-type HyScanTrackStatsInfo): хэш-таблица статистики по каждому записанному галсу
 * @progress: прогресс выполнения, от 0.0 до 1.0
 * @quality: качество выполнения, от 0.0 до 1.0
 * @length: длина планового галса, м
 * @angle: запланированный курс движения, градусы
 * @time: оценка времени для покрытия галса, с
 *
 * Статистика покрытия запланированного галса.
 */
struct _HyScanPlannerStatsTrack
{
  gchar              *id;
  HyScanPlannerTrack *object;
  GHashTable         *records;
  gdouble             progress;
  gdouble             quality;
  gdouble             length;
  gdouble             angle;
  gdouble             time;
};

HYSCAN_API
GType                     hyscan_planner_stats_get_type         (void);

HYSCAN_API
HyScanPlannerStats *      hyscan_planner_stats_new              (HyScanTrackStats              *track_stats,
                                                                 HyScanPlannerModel            *planner_model);

HYSCAN_API
GHashTable *              hyscan_planner_stats_get              (HyScanPlannerStats            *planner_stats);

HYSCAN_API
void                      hyscan_planner_stats_zone_free        (HyScanPlannerStatsZone        *stats_zone);

HYSCAN_API
HyScanPlannerStatsZone  * hyscan_planner_stats_zone_copy        (const HyScanPlannerStatsZone  *stats_zone);

HYSCAN_API
void                      hyscan_planner_stats_track_free       (HyScanPlannerStatsTrack       *stats_track);

HYSCAN_API
HyScanPlannerStatsTrack * hyscan_planner_stats_track_copy       (HyScanPlannerStatsTrack       *stats_track);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_STATS_H__ */
