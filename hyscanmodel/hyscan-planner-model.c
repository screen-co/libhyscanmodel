/* hyscan-planner-model.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-planner-model
 * @Short_description: Модель данных планировщика
 * @Title: HyScanPlannerModel
 *
 * #HyScanPlannerModel предназначен для асинхронной работы с объектами планировщика.
 * Класс модели расширяет функциональность класса #HyScanObjectModel и позволяет
 * устанавливать географические координаты начала отсчёта топоцентрической системы
 * координат и получить объект пересчёта координат из географической СК
 * в топоцентрическую.
 *
 */

#include "hyscan-planner-model.h"
#include <math.h>
#define DEG2RAD(x) ((x) * G_PI / 180.0)   /* Перевод из градусов в радианы. */
#define EARTH_RADIUS        6378137.0     /* Радиус Земли. */

struct _HyScanPlannerModelPrivate
{
  HyScanGeo                   *geo;
};

static void    hyscan_planner_model_object_finalize    (GObject               *object);
static void    hyscan_planner_model_changed            (HyScanObjectModel     *model);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlannerModel, hyscan_planner_model, HYSCAN_TYPE_OBJECT_MODEL)

static void
hyscan_planner_model_class_init (HyScanPlannerModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectModelClass *model_class = HYSCAN_OBJECT_MODEL_CLASS (klass);

  model_class->changed = hyscan_planner_model_changed;
  object_class->finalize = hyscan_planner_model_object_finalize;
}

static void
hyscan_planner_model_init (HyScanPlannerModel *planner_model)
{
  planner_model->priv = hyscan_planner_model_get_instance_private (planner_model);
}

static void
hyscan_planner_model_object_finalize (GObject *object)
{
  HyScanPlannerModel *planner_model = HYSCAN_PLANNER_MODEL (object);
  HyScanPlannerModelPrivate *priv = planner_model->priv;

  g_clear_object (&priv->geo);

  G_OBJECT_CLASS (hyscan_planner_model_parent_class)->finalize (object);
}

static void
hyscan_planner_model_changed (HyScanObjectModel *model)
{
  HyScanPlannerModel *pmodel = HYSCAN_PLANNER_MODEL (model);
  HyScanPlannerModelPrivate *priv = pmodel->priv;
  HyScanPlannerOrigin *object;
  HyScanGeoGeodetic origin;

  g_clear_object (&priv->geo);

  object = (HyScanPlannerOrigin *) hyscan_object_model_get_id (model, HYSCAN_PLANNER_ORIGIN_ID);
  if (object == NULL)
    return;

  origin.lat = object->origin.lat;
  origin.lon = object->origin.lon;
  origin.h = object->ox;
  priv->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  hyscan_planner_origin_free (object);
}

/**
 * hyscan_planner_model_new:
 *
 * Создаёт модель данных для асинхронного доступа к параметрам объектов планировщика.
 *
 * Returns: (transfer full): указатель на HyScanPlannerModel. Для удаления g_object_unref().
 */
HyScanPlannerModel *
hyscan_planner_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_PLANNER_MODEL,
                       "data-type", HYSCAN_TYPE_OBJECT_DATA_PLANNER, NULL);
}

/**
 * hyscan_planner_model_get_geo:
 * @pmodel: указатель на #HyScanPlannerModel
 *
 * Функция возвращает объект пересчёта координат схемы галсов из географической
 * системы координат в топоцентрическую и обратно.
 *
 * Returns: (transfer full) (nullable): указатель на объект HyScanGeo, для удаления
 *   g_object_unref().
 */
HyScanGeo *
hyscan_planner_model_get_geo (HyScanPlannerModel *pmodel)
{
  HyScanPlannerModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel), NULL);
  priv = pmodel->priv;

  return priv->geo != NULL ? g_object_ref (priv->geo) : NULL;
}

/**
 * hyscan_planner_model_set_origin:
 * @pmodel: указатель на #HyScanPlannerModel
 * @origin: (nullable): координаты начала отсчёта местной системы координат
 *
 * Функция устанавливает начало местной системы координат.
 *
 * Фактическое изменение объекта пересчёта координат произойдёт после записи
 * данных в БД, т.е. при одном из следующих испусканий сигналов "changed".
 */
void
hyscan_planner_model_set_origin (HyScanPlannerModel        *pmodel,
                                 const HyScanPlannerOrigin *origin)
{
  HyScanObjectModel *model;
  HyScanPlannerOrigin *prev_value;

  g_return_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel));
  model = HYSCAN_OBJECT_MODEL (pmodel);

  prev_value = hyscan_planner_model_get_origin (pmodel);

  if (origin == NULL)
    {
      if (prev_value != NULL)
        hyscan_object_model_remove_object (model, HYSCAN_PLANNER_ORIGIN_ID);
    }
  else
    {
      if (prev_value != NULL)
        hyscan_object_model_modify_object (model, HYSCAN_PLANNER_ORIGIN_ID, (const HyScanObject *) origin);
      else
        hyscan_object_model_add_object (model, (const HyScanObject *) origin);
    }

  g_clear_pointer (&prev_value, hyscan_planner_origin_free);
}

/**
 * hyscan_planner_model_get_origin:
 * @pmodel: указатель на #HyScanPlannerModel
 *
 * Возвращает координаты точки отсчёта для схемы галсов проекта.
 *
 * Returns: (transfer full): указатель на структуру #HyScanPlannerOrigin.
 *   Для удаления hyscan_object_free()
 */
HyScanPlannerOrigin *
hyscan_planner_model_get_origin (HyScanPlannerModel *pmodel)
{
  HyScanPlannerOrigin *object;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_MODEL (pmodel), NULL);

  object = (HyScanPlannerOrigin *) hyscan_object_model_get_id (HYSCAN_OBJECT_MODEL (pmodel), HYSCAN_PLANNER_ORIGIN_ID);
  if (object == NULL)
    return NULL;

  if (!HYSCAN_IS_PLANNER_ORIGIN (object))
    {
      hyscan_object_model_remove_object (HYSCAN_OBJECT_MODEL (pmodel), HYSCAN_PLANNER_ORIGIN_ID);
      object = NULL;
    }

  return object;
}

static gdouble
hyscan_planner_model_distance (HyScanGeoPoint *point1,
                               HyScanGeoPoint *point2)
{
  gdouble lon1r;
  gdouble lat1r;

  gdouble lon2r;
  gdouble lat2r;

  gdouble u;
  gdouble v;

  lat1r = DEG2RAD (point1->lat);
  lon1r = DEG2RAD (point1->lon);
  lat2r = DEG2RAD (point2->lat);
  lon2r = DEG2RAD (point2->lon);

  u = sin ((lat2r - lat1r) / 2);
  v = sin ((lon2r - lon1r) / 2);

  return 2.0 * EARTH_RADIUS * asin (sqrt (u * u + cos (lat1r) * cos (lat2r) * v * v));
}

/**
 * hyscan_planner_model_assign_number:
 * @pmodel:
 * @track0_id:
 *
 * Устанавливает порядок обхода галсов начиная с @track0_id таким образом, что
 * минимизируется время перехода между галсами.
 *
 * todo: сделать через решение задачи коммивояжёра.
 */
void
hyscan_planner_model_assign_number (HyScanPlannerModel *pmodel,
                                    const gchar        *track0_id)
{
  GHashTable *objects;
  HyScanPlannerTrack *track;
  gchar *track_id;

  const gchar *zone_id;
  GHashTableIter iter;
  GArray *track_ids;
  gdouble *distances;
  gint i, j, n;
  gint *sorted;
  gboolean *invert;
  gint position;

  objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (pmodel));
  if (objects == NULL)
    goto exit;

  track = g_hash_table_lookup (objects, track0_id);
  if (!HYSCAN_IS_PLANNER_TRACK (track))
    goto exit;
  zone_id = track->zone_id;

  /* Находим все галсы, которые надо упорядочить. */
  track_ids = g_array_new (FALSE, FALSE, sizeof (const gchar *));
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &track_id, (gpointer *) &track))
    {
      if (!HYSCAN_IS_PLANNER_TRACK (track) || g_strcmp0 (track->zone_id, zone_id) != 0)
        continue;

      g_array_append_val (track_ids, track_id);
    }

  /* Составляем симметричную матрицу переходов с галса на галс. */
  n = track_ids->len;
  distances = g_new (gdouble, n * n);
  for (i = 0; i < n; i++)
    {
      HyScanPlannerTrack *track_i, *track_j;
      track_i = g_hash_table_lookup (objects, g_array_index (track_ids, gchar *, i));

      for (j = i + 1; j < n; j++)
        {
          gdouble distance0, distance1, distance2, distance3;

          track_j = g_hash_table_lookup (objects, g_array_index (track_ids, gchar *, j));

          /* Берём минимальное расстояние среди всех возможных переходов. */
          distance0 = hyscan_planner_model_distance (&track_i->plan.start, &track_j->plan.end);
          distance1 = hyscan_planner_model_distance (&track_i->plan.start, &track_j->plan.start);
          distance2 = hyscan_planner_model_distance (&track_i->plan.end, &track_j->plan.end);
          distance3 = hyscan_planner_model_distance (&track_i->plan.end, &track_j->plan.start);

          distances[i * n + j] =
          distances[j * n + i] = MIN (MIN (distance0, distance1), MIN (distance2, distance3));
        }

      /* Диагональный элемент. */
      distances[i * n + i] = G_MAXDOUBLE;
    }

  /* Находим индекс первого галса. */
  for (i = 0; g_strcmp0 (track0_id, g_array_index (track_ids, gchar *, i)) != 0 && i < n; i++)
    ;
  sorted = g_new0 (gint, track_ids->len);
  invert = g_new0 (gboolean, track_ids->len);
  position = 0;
  sorted[i] = ++position;

  /* Находим индексы следующих галсов, минимизируя расстояние до следующего галса. */
  while (position < n)
    {
      gdouble min_distance = G_MAXDOUBLE;
      gdouble invert_next = FALSE;
      gint next = 0;
      HyScanPlannerTrack *track_i, *track_j;
      HyScanGeoPoint *end;

      track_i = g_hash_table_lookup (objects, g_array_index (track_ids, gchar *, i));
      end = invert[i] ? &track_i->plan.start : &track_i->plan.end;

      for (j = 0; j < n; j++)
        {
          gboolean distance, distance_inv;

          if (sorted[j] != 0)
            continue;

          track_j = g_hash_table_lookup (objects, g_array_index (track_ids, gchar *, j));

          distance = hyscan_planner_model_distance (end, &track_j->plan.start);
          distance_inv = hyscan_planner_model_distance (end, &track_j->plan.end);
          if (MIN (distance, distance_inv) > min_distance)
            continue;

          next = j;
          invert_next = (distance_inv < distance);
          min_distance = invert_next ? distance_inv : distance;
        }

      i = next;
      invert[i] = invert_next;
      sorted[i] = ++position;
    }

  /* Обновляем номера галсов в БД. */
  for (i = 0; i < n; i++)
    {
      track_id = g_array_index (track_ids, gchar *, i);
      track = g_hash_table_lookup (objects, track_id);
      track->number = sorted[i];
      if (invert[i])
        {
          HyScanGeoPoint tmp;
          tmp = track->plan.start;
          track->plan.start = track->plan.end;
          track->plan.end = tmp;
        }
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (pmodel), track_id, (HyScanObject *) track);
    }

  g_free (distances);
  g_free (invert);
  g_free (sorted);
  g_array_free (track_ids, TRUE);

exit:
  g_clear_pointer (&objects, g_hash_table_destroy);
}
