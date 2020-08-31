/* hyscan-planner-stats.c
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

/**
 * SECTION: hyscan-planner-stats
 * @Short_description: класс для получения сводной информации о схеме галсов и процессе ее покрытия
 * @Title: HyScanPlannerStats
 *
 * Класс предназначен для сбора и структурирования информации о схеме галсов и статистики её покрытия.
 *
 * Все планы галсов группируются по зонам, при этом свободные галсы помещаются в одну общую вирутальную зону.
 * Также каждый план галса содержит в себе список записанных по нему галсов.
 *
 * Функция hyscan_planner_stats_get() возвращает хэш-таблицу из структур #HyScanPlannerStatsZone,
 * которая содержит сводную информация о полигоне, а также информацию по каждому из вложенных объектов.
 *
 * В итоге формируется следующее дерево структур:
 *
 * - HyScanPlannerStatsZone - сводная информация о полигоне;
 *   |- .tracks = { HyScanPlannerStatsTrack, ... } - таблица планов галсов внутри этого полигона;
 *       |- .records = { HyScanTrackStatsInfo, ... } - таблица записанных галсов по этому плану.
 */

#include "hyscan-planner-stats.h"
#include <stdlib.h>

#define OTHER_ZONE "other-zone"    /* Искусственный идентификатор для группировки галсов, у которых нет зоны. */
#define RAD2DEG(x) (((x) < 0 ? (x) + 2 * G_PI : (x)) / G_PI * 180.0)

enum
{
  PROP_O,
  PROP_TRACK_STATS,
  PROP_PLANNER_MODEL,
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

struct _HyScanPlannerStatsPrivate
{
  HyScanPlannerModel          *planner_model;  /* Модель объектов планировщика. */
  HyScanTrackStats            *track_stats;    /* Объект статистики галсов. */
  GHashTable                  *track_objects;  /* Хэш-таблица объектов планировщика. */
  GHashTable                  *zone_objects;  /* Хэш-таблица объектов планировщика. */
  GHashTable                  *tracks;         /* Хэш таблица статистики галсов. */
  GHashTable                  *zones;          /* Хэш-таблица с итоговой сводной информацией по зонам. */
};

typedef struct
{
  gdouble  start;                              /* Начало интервала. */
  gdouble  end;                                /* Конец интервала. */
  gdouble  quality;                            /* Значение качества. */
} HyScanPlannerStatsInterval;

static void         hyscan_planner_stats_set_property       (GObject                    *object,
                                                             guint                       prop_id,
                                                             const GValue               *value,
                                                             GParamSpec                 *pspec);
static void         hyscan_planner_stats_object_constructed (GObject                    *object);
static void         hyscan_planner_stats_object_finalize    (GObject                    *object);
static void         hyscan_planner_stats_tracks_changed     (HyScanPlannerStats         *planner_stats);
static void         hyscan_planner_stats_planner_changed    (HyScanPlannerStats         *planner_stats);
static void         hyscan_gtk_planner_list_update_tracks   (HyScanPlannerStats         *planner_stats);
static void         hyscan_gtk_planner_list_update_zones    (HyScanPlannerStats         *planner_stats);
static gint         hyscan_planner_stats_track_compare      (HyScanPlannerStatsTrack    *a,
                                                             HyScanPlannerStatsTrack    *b);
static gint         hyscan_planner_stats_interval_compare   (HyScanPlannerStatsInterval *a,
                                                             HyScanPlannerStatsInterval *b);

static guint        hyscan_planner_stats_signals[SIGNAL_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlannerStats, hyscan_planner_stats, G_TYPE_OBJECT)

static void
hyscan_planner_stats_class_init (HyScanPlannerStatsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_planner_stats_set_property;

  object_class->constructed = hyscan_planner_stats_object_constructed;
  object_class->finalize = hyscan_planner_stats_object_finalize;

  g_object_class_install_property (object_class, PROP_TRACK_STATS,
    g_param_spec_object ("track-stats", "Track Stats", "HyScanTrackStats model", HYSCAN_TYPE_TRACK_STATS,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PLANNER_MODEL,
    g_param_spec_object ("planner-model", "Planner Model", "HyScanPlannerModel model", HYSCAN_TYPE_PLANNER_MODEL,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanPlannerStats::changed:
   * @stats: указатель на #HyScanPlannerStats
   *
   * Сигнал посылается при изменении статистики покрытия схемы галсов.
   */
  hyscan_planner_stats_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_PLANNER_STATS, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_planner_stats_init (HyScanPlannerStats *planner_stats)
{
  planner_stats->priv = hyscan_planner_stats_get_instance_private (planner_stats);
}

static void
hyscan_planner_stats_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanPlannerStats *planner_stats = HYSCAN_PLANNER_STATS (object);
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  switch (prop_id)
    {
    case PROP_PLANNER_MODEL:
      priv->planner_model = g_value_dup_object (value);
      break;

    case PROP_TRACK_STATS:
      priv->track_stats = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_planner_stats_object_constructed (GObject *object)
{
  HyScanPlannerStats *planner_stats = HYSCAN_PLANNER_STATS (object);
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  G_OBJECT_CLASS (hyscan_planner_stats_parent_class)->constructed (object);

  g_signal_connect_swapped (priv->track_stats,   "changed", G_CALLBACK (hyscan_planner_stats_tracks_changed), planner_stats);
  g_signal_connect_swapped (priv->planner_model, "changed", G_CALLBACK (hyscan_planner_stats_planner_changed), planner_stats);

  hyscan_planner_stats_tracks_changed (planner_stats);
  hyscan_planner_stats_planner_changed (planner_stats);
}

static void
hyscan_planner_stats_object_finalize (GObject *object)
{
  HyScanPlannerStats *planner_stats = HYSCAN_PLANNER_STATS (object);
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  g_clear_pointer (&priv->tracks, g_hash_table_destroy);
  g_clear_pointer (&priv->track_objects, g_hash_table_destroy);
  g_clear_pointer (&priv->zone_objects, g_hash_table_destroy);
  g_clear_pointer (&priv->zones, g_hash_table_destroy);
  g_clear_object (&priv->planner_model);
  g_clear_object (&priv->track_stats);

  G_OBJECT_CLASS (hyscan_planner_stats_parent_class)->finalize (object);
}

/* Макрос для отладки функции hyscan_planner_stats_track_progress. */
// #define DEBUG_INTERVAL_ENABLE
#ifdef DEBUG_INTERVAL_ENABLE
#define DEBUG_INTERVALS(x) if (track->object->number == 3) \
                             hyscan_planner_stats_debug_intervals (x, intervals)
static void
hyscan_planner_stats_debug_intervals (const gchar *step_name,
                                      GList       *intervals) {
  GList *link;

  g_print ("-- %s\n", step_name);
  for (link = intervals; link != NULL; link = link->next)
    {
      HyScanPlannerStatsInterval *interval = link->data;
      g_print ("%5.2f: (%5.2f, %5.2f)\n", interval->quality, interval->start, interval->end);
    }
  g_print ("\n");
}
#else
#define DEBUG_INTERVALS(x)
#endif

/* Функция определяет статус выполнения плана галса на основе его записей в бд. */
static void
hyscan_planner_stats_track_progress (HyScanPlannerStatsTrack *track,
                                     gdouble                 *progress,
                                     gdouble                 *quality)
{
  GList *intervals = NULL;
  GHashTableIter iter;
  HyScanTrackStatsInfo *info;
  GList *link, *link2, *next_link;

  if (track->records == NULL || g_hash_table_size (track->records) == 0 || track->length == 0.0)
    {
      *progress = 0.0;
      *quality = -1.0;
      return;
    }

  /* Запоминаем интервалы планового галса, покрытые галсами. */
  g_hash_table_iter_init (&iter, track->records);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &info))
    {
      HyScanPlannerStatsInterval *interval;

      interval = g_new0 (HyScanPlannerStatsInterval, 1);
      interval->start = MAX (info->x_min, 0);
      interval->end = MIN (track->length, info->x_max);
      interval->quality = info->quality;

      intervals = g_list_append (intervals, interval);
    }

  DEBUG_INTERVALS ("init");

  /* Упорядочиваем интервалы по возрастанию качества. */
  intervals = g_list_sort (intervals, (GCompareFunc) hyscan_planner_stats_interval_compare);

  DEBUG_INTERVALS ("sorted");

  /* Для каждой пары пересекающихся записей оставляем лучшую по качеству.
   * В итоге получим набор непересекающихся интервалов с лучшими качествами записи
   * на каждом из отрезков.
   *
   * Например, если запись представить как (start, end, quality), то
   *
   *                           до → после
   * { (1, 10, 90), (1, 10, 70) } → { (1, 10, 90) }
   * { (1, 10, 90), (1, 20, 70) } → { (1, 10, 90), (10, 20, 70) }
   * { (5, 10, 90), (1, 20, 70) } → { (1,  5, 70), ( 5, 10, 90), (10, 20, 70) }
   * */
  for (link = intervals; link != NULL; link = next_link)
    {
      HyScanPlannerStatsInterval *current = link->data;
      next_link = link->next;

      for (link2 = link->next; link2 != NULL; link2 = link2->next)
        {
          HyScanPlannerStatsInterval *better = link2->data;

          /* Запись лучшего качества полностью покрывает current. */
          if (better->start <= current->start && current->end <= better->end)
            {
              g_free (link->data);
              intervals = g_list_delete_link (intervals, link);
              DEBUG_INTERVALS ("all");
              break;
            }
          /* Запись лучшего качества покрывает часть в середине current. */
          else if (current->start <= better->start && better->end <= current->end)
            {
              HyScanPlannerStatsInterval *chunk;

              /* Добавляем отрезок после текущего, это приведёт к изменению ссылки на следующий элемент. */
              chunk = g_memdup (current, sizeof (*current));
              link = g_list_insert (link, chunk, 1);
              next_link = link->next;

              current->end = better->start;
              chunk->start = better->end;

              DEBUG_INTERVALS ("split");
            }
          /* Запись лучшего качества перекрывает часть начала. */
          else if (better->start <= current->start && current->start <= better->end)
            {
              current->start = better->end;
              DEBUG_INTERVALS ("start");
            }
          /* Запись лучшего качества перекрывает часть конца. */
          else if (better->start <= current->end && current->end <= better->end)
            {
              current->end = better->start;
              DEBUG_INTERVALS ("end");
            }
        }
    }

  DEBUG_INTERVALS ("final");

  /* Суммируем покрытую длину галса и качество. */
  *progress = 0.0;
  *quality = 0.0;
  for (link = intervals; link != NULL; link = link->next)
    {
      HyScanPlannerStatsInterval *current = link->data;
      gdouble weight;

      weight = (current->end - current->start) / track->length;
      *progress += weight;
      *quality += weight * current->quality;
    }

  *quality /= *progress;

  g_list_free_full (intervals, g_free);
}

/* Функция сравнения интервалов по качеству. */
static gint
hyscan_planner_stats_interval_compare (HyScanPlannerStatsInterval *a,
                                       HyScanPlannerStatsInterval *b)
{
  if (a->quality > b->quality)
    return 1;
  else if (a->quality < b->quality)
    return -1;
  else
    return 0;
}

/* Обновляет информацию о галсах в виджете. */
static void
hyscan_gtk_planner_list_update_tracks (HyScanPlannerStats *planner_stats)
{
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  GHashTableIter objects_iter;
  gchar *track_id;
  HyScanPlannerTrack *track;

  HyScanPlannerStatsZone *zone_info;

  gint i;

  /* Добавляем в список все галсы, которые есть в модели данных планировщика. */
  g_hash_table_iter_init (&objects_iter, priv->track_objects);
  while (g_hash_table_iter_next (&objects_iter, (gpointer *) &track_id, (gpointer *) &track))
    {
      HyScanPlannerStatsTrack *track_info;
      const gchar *parent_key;

      /* ИД родительского элемента галса. */
      if (track->zone_id != NULL && g_hash_table_contains (priv->zone_objects, track->zone_id))
        parent_key = track->zone_id;
      else
        parent_key = OTHER_ZONE;

      if ((zone_info = g_hash_table_lookup (priv->zones, parent_key)) == NULL)
        {
          HyScanPlannerStatsZone zone = {0};

          zone.id = track->zone_id;

          zone_info = hyscan_planner_stats_zone_copy (&zone);
          g_hash_table_insert (priv->zones, g_strdup (parent_key), zone_info);
        }

      /* Записываем информацию о галсе. */
      {
        HyScanPlannerStatsTrack tmp = {0};
        track_info = hyscan_planner_stats_track_copy (&tmp);
        track_info->id = g_strdup (track_id);
        track_info->object = hyscan_planner_track_copy (track);
        track_info->length = hyscan_planner_track_length (&track->plan);
        track_info->time = track_info->length / track->plan.speed;
        track_info->angle = RAD2DEG (hyscan_planner_track_angle (track));

        /* Формируем список записей галса и рассчитываем по ним прогресс выполнения. */
        for (i = 0; track->records != NULL && track->records[i] != NULL; i++)
          {
            HyScanTrackStatsInfo *record;

            record = g_hash_table_lookup (priv->tracks, track->records[i]);
            if (record == NULL)
              continue;

            g_hash_table_insert (track_info->records,
                                 g_strdup (track->records[i]),
                                 hyscan_track_stats_info_copy (record));
          }
        hyscan_planner_stats_track_progress (track_info, &track_info->progress, &track_info->quality);
        g_hash_table_insert (zone_info->tracks, g_strdup (track_id), track_info);
      }
    }
}

/* Функция для сортировки планов галсов в порядке их следования. */
static gint
hyscan_planner_stats_track_compare (HyScanPlannerStatsTrack *a,
                                    HyScanPlannerStatsTrack *b)
{
  guint a_hash, b_hash;

  if (a->object->number < b->object->number)
    return -1;
  else if (a->object->number > b->object->number)
    return 1;

  a_hash = g_str_hash (a->id);
  b_hash = g_str_hash (b->id);

  if (a_hash < b_hash)
    return -1;
  else if (a_hash > b_hash)
    return 1;

  return 0;
}

static void
hyscan_gtk_planner_list_update_zones (HyScanPlannerStats *planner_stats)
{
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  GHashTableIter objects_iter;
  GHashTableIter zone_iter;

  HyScanPlannerStatsZone *zone_info;
  gchar *zone_id;
  HyScanPlannerZone *zone;

  /* Добавляем в список зоны, которые есть в модели данных планировщика. */
  g_hash_table_iter_init (&objects_iter, priv->zone_objects);
  while (g_hash_table_iter_next (&objects_iter, (gpointer *) &zone_id, (gpointer *) &zone))
    {
      if ((zone_info = g_hash_table_lookup (priv->zones, zone_id)) == NULL)
        {
          HyScanPlannerStatsZone add_zone = {0};

          add_zone.id = (gchar *) zone_id;

          zone_info = hyscan_planner_stats_zone_copy (&add_zone);
          g_hash_table_insert (priv->zones, g_strdup (zone_info->id), zone_info);
        }

      zone_info->object = hyscan_planner_zone_copy (zone);
    }

  /* Считаем статистику по всем зонам. */
  g_hash_table_iter_init (&zone_iter, priv->zones);
  while (g_hash_table_iter_next (&zone_iter, NULL, (gpointer *) &zone_info))
    {
      GHashTableIter track_iter;
      GList *tracks = NULL, *link;
      HyScanPlannerStatsTrack *track_info;
      gdouble total_length = 0.0, quality_length = 0.0;

      /* Формируем список галсов полигона в порядке их следования. */
      g_hash_table_iter_init (&track_iter, zone_info->tracks);
      while (g_hash_table_iter_next (&track_iter, NULL, (gpointer *) &track_info))
        tracks = g_list_prepend (tracks, track_info);
      tracks = g_list_sort (tracks, (GCompareFunc) hyscan_planner_stats_track_compare);

      for (link = tracks; link != NULL; link = link->next)
        {
          HyScanPlannerStatsTrack *next_track = link->next != NULL ? link->next->data : NULL;
          gboolean transit_length;
          track_info = link->data;

          /* Считаем расстояние и время движения, включая переходы с галса на галс. */
          zone_info->time += track_info->time;
          zone_info->length += track_info->length;
          if (next_track != NULL)
            {
              HyScanTrackPlan *cur_plan = &track_info->object->plan, *next_plan = &next_track->object->plan;
              transit_length = hyscan_planner_track_transit (cur_plan, next_plan);
              zone_info->length += transit_length;
              zone_info->time += transit_length / (cur_plan->speed > 0 ? cur_plan->speed : 1.0);
            }

          if (track_info->length == 0)
            continue;

          total_length += track_info->length;
          zone_info->progress += track_info->length * track_info->progress;

          if (track_info->quality >= 0)
            {
              quality_length += track_info->length;
              zone_info->quality += track_info->length * track_info->quality;
            }
        }

        zone_info->velocity = (zone_info->time > 0) ? zone_info->length / zone_info->time : 0;

        if (total_length > 0)
          zone_info->progress /= total_length;

        if (quality_length > 0)
          zone_info->quality /= quality_length;
        else
          zone_info->quality = -1.;

        g_list_free (tracks);
    }
}

static void
hyscan_planner_stats_update (HyScanPlannerStats *planner_stats)
{
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  if (priv->track_objects == NULL || priv->tracks == NULL)
    return;

  /* Хэш таблица с информацией о зонах. */
  g_clear_pointer (&priv->zones, g_hash_table_destroy);
  priv->zones = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) hyscan_planner_stats_zone_free);
  hyscan_gtk_planner_list_update_tracks (planner_stats);
  hyscan_gtk_planner_list_update_zones (planner_stats);

  g_signal_emit (planner_stats, hyscan_planner_stats_signals[SIGNAL_CHANGED], 0);
}

static void
hyscan_planner_stats_tracks_changed (HyScanPlannerStats *planner_stats)
{

  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  /* Формируем таблицу: ид галса - HyScanTrackInfo. */
  g_clear_pointer (&priv->tracks, g_hash_table_destroy);
  priv->tracks = hyscan_track_stats_get (priv->track_stats);

  hyscan_planner_stats_update (planner_stats);
}

static void
hyscan_planner_stats_planner_changed (HyScanPlannerStats *planner_stats)
{
  HyScanPlannerStatsPrivate *priv = planner_stats->priv;

  /* Устанавливаем новый список объектов в модель данных. */
  g_clear_pointer (&priv->track_objects, g_hash_table_unref);
  g_clear_pointer (&priv->zone_objects, g_hash_table_unref);
  priv->track_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->planner_model), HYSCAN_TYPE_PLANNER_TRACK);
  priv->zone_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->planner_model), HYSCAN_TYPE_PLANNER_ZONE);

  hyscan_planner_stats_update (planner_stats);
}

/**
 * hyscan_planner_stats_new:
 * @track_stats: статистика галсов
 * @planner_model:
 *
 * Returns: (transfer full): новый объект HyScanPlannerStats, для удаления g_object_unref().
 */
HyScanPlannerStats *
hyscan_planner_stats_new (HyScanTrackStats   *track_stats,
                          HyScanPlannerModel *planner_model)
{
  return g_object_new (HYSCAN_TYPE_PLANNER_STATS,
                       "track-stats", track_stats,
                       "planner-model", planner_model,
                       NULL);
}

/**
 * hyscan_planner_stats_get:
 * @planner_stats: указатель на #HyScanPlannerStats
 *
 * Returns: (transfer full) (element-type HyScanPlannerStatsZone): статистика покрытия зон полигона,
 * для удаления g_hash_table_destroy().
 */
GHashTable *
hyscan_planner_stats_get (HyScanPlannerStats *planner_stats)
{
  HyScanPlannerStatsPrivate *priv;
  GHashTable *hash_table;
  GHashTableIter iter;

  gchar *key;
  HyScanPlannerStatsZone *zone;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_STATS (planner_stats), NULL);
  priv = planner_stats->priv;

  hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_planner_stats_zone_free);

  if (priv->zones == NULL)
    return hash_table;

  g_hash_table_iter_init (&iter, priv->zones);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    g_hash_table_insert (hash_table, g_strdup (key), hyscan_planner_stats_zone_copy (zone));

  return hash_table;
}

/**
 * hyscan_planner_stats_zone_free:
 * @stats_zone: указатель на структуру #HyScanPlannerStatsZone
 *
 * Удаляет структуру #HyScanPlannerStatsZone
 */
void
hyscan_planner_stats_zone_free (HyScanPlannerStatsZone *stats_zone)
{
  g_free (stats_zone->id);
  hyscan_planner_zone_free (stats_zone->object);
  g_hash_table_unref (stats_zone->tracks);
  g_slice_free (HyScanPlannerStatsZone, stats_zone);
}

/**
 * hyscan_planner_stats_track_free:
 * @stats_track: указатель на структуру #HyScanPlannerStatsTrack
 *
 * Удаляет структуру #HyScanPlannerStatsTrack
 */
void
hyscan_planner_stats_track_free (HyScanPlannerStatsTrack *stats_track)
{
  g_free (stats_track->id);
  hyscan_planner_track_free (stats_track->object);
  g_hash_table_unref (stats_track->records);
  g_slice_free (HyScanPlannerStatsTrack, stats_track);
}

/**
 * hyscan_planner_stats_zone_copy:
 * @stats_zone: указатель на структуру #HyScanPlannerStatsZone
 *
 * Создает копию структуры #HyScanPlannerStatsZone.
 *
 * Returns: (transfer full): копия структуры #HyScanPlannerStatsZone, для удаления hyscan_planner_stats_zone_free().
 */
HyScanPlannerStatsZone *
hyscan_planner_stats_zone_copy (const HyScanPlannerStatsZone *stats_zone)
{
  HyScanPlannerStatsZone *copy;

  copy = g_slice_dup (HyScanPlannerStatsZone, stats_zone);
  copy->id = g_strdup (stats_zone->id);
  copy->object = hyscan_planner_zone_copy (stats_zone->object);
  copy->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                        (GDestroyNotify) hyscan_planner_stats_track_free);

  if (stats_zone->tracks != NULL)
    {
      GHashTableIter iter;
      gchar *key;
      HyScanPlannerStatsTrack *track;

      g_hash_table_iter_init (&iter, stats_zone->tracks);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
        g_hash_table_insert (copy->tracks, g_strdup (key), hyscan_planner_stats_track_copy (track));
    }

  return copy;
}

/**
 * hyscan_planner_stats_track_copy:
 * @stats_track: указатель на структуру #HyScanPlannerStatsTrack
 *
 * Создает копию структуры #HyScanPlannerStatsTrack.
 *
 * Returns: (transfer full): копия структуры #HyScanPlannerStatsTrack, для удаления hyscan_planner_stats_track_free().
 */
HyScanPlannerStatsTrack *
hyscan_planner_stats_track_copy (HyScanPlannerStatsTrack *stats_track)
{
  HyScanPlannerStatsTrack *copy;

  copy = g_slice_dup (HyScanPlannerStatsTrack, stats_track);
  copy->id = g_strdup (stats_track->id);
  copy->object = hyscan_planner_track_copy (stats_track->object);
  copy->records = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify) hyscan_track_stats_info_free);

  if (stats_track->records != NULL)
    {
      GHashTableIter iter;
      gchar *key;
      HyScanTrackStatsInfo *record;

      g_hash_table_iter_init (&iter, stats_track->records);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &record))
        g_hash_table_insert (copy->records, g_strdup (key), hyscan_track_stats_info_copy (record));
    }

  return copy;
}
