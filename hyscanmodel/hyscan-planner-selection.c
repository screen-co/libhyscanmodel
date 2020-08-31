/* hyscan-gtk-planner-selection.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-gtk-planner-selection
 * @Short_description: Модель выбранных объектов планировщика
 * @Title: HyScanGtkPlannerSelection
 *
 * Модель хранит в себе информацию о выбранных объектах планировщика:
 * - выбранные планы галсов
 * - активный план галс (по которому идёт навигация)
 * - выбранная зона полигона и вершина на ней
 *
 * При изменении этих объектов модель отправляет соответственно один из сигналов:
 * - HyScanGtkPlannerSelection::tracks-changed
 * - HyScanGtkPlannerSelection::activated
 * - HyScanGtkPlannerSelection::zone-changed
 *
 */

#include "hyscan-planner-selection.h"

enum
{
  PROP_O,
  PROP_MODEL
};

enum
{
  SIGNAL_TRACKS_CHANGED,
  SIGNAL_ZONE_CHANGED,
  SIGNAL_ACTIVATED,
  SIGNAL_LAST,
};

struct _HyScanPlannerSelectionPrivate
{
  HyScanPlannerModel          *model;           /* Модель объектов планировщика. */
  GHashTable                  *track_objects;   /* Объекты планов галсов. */
  GHashTable                  *zone_objects;    /* Объекты зон. */
  GArray                      *tracks;          /* Массив выбранных галсов. */
  gint                         vertex_index;    /* Вершина в выбранной зоне. */
  gchar                       *zone_id;         /* Выбранная зона. */
  gchar                       *active_plan;     /* Активный план галса, по которому идёт навигация. */
  HyScanDBInfo                *watch_db_info;   /* Модель галсов в БД. */
  gulong                       watch_id;        /* Ид обработчика сигнала HyScanDBInfo::tracks-changed. */
  gchar                       *recording_track; /* Ид галса, который сейчас записывается в БД. */
};

static void    hyscan_planner_selection_set_property             (GObject                *object,
                                                                  guint                   prop_id,
                                                                  const GValue           *value,
                                                                  GParamSpec             *pspec);
static void    hyscan_planner_selection_object_constructed       (GObject                *object);
static void    hyscan_planner_selection_object_finalize          (GObject                *object);
static void    hyscan_planner_selection_changed                  (HyScanPlannerSelection *selection);
static void    hyscan_planner_selection_clear_func               (gpointer data);
static void    hyscan_planner_selection_track_watcher            (HyScanPlannerSelection *selection);

static guint hyscan_planner_selection_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlannerSelection, hyscan_planner_selection, G_TYPE_OBJECT)

static void
hyscan_planner_selection_class_init (HyScanPlannerSelectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_planner_selection_set_property;

  object_class->constructed = hyscan_planner_selection_object_constructed;
  object_class->finalize = hyscan_planner_selection_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanPlannerModel", "Planner objects model",
                         HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanPlannerSelection::tracks-changed:
   * @planner: указатель на #HyScanGtkMapPlanner
   * @tracks: NULL-терминированный массив выбранных галсов
   *
   * Сигнал посылается при изменении списка выбранных галсов.
   */
  hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED] =
    g_signal_new ("tracks-changed", HYSCAN_TYPE_PLANNER_SELECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * HyScanPlannerSelection::zone-changed:
   * @planner: указатель на #HyScanGtkMapPlanner
   *
   * Сигнал посылается при изменении выбранной зоны.
   */
  hyscan_planner_selection_signals[SIGNAL_ZONE_CHANGED] =
    g_signal_new ("zone-changed", HYSCAN_TYPE_PLANNER_SELECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * HyScanPlannerSelection::activated:
   * @planner: указатель на #HyScanGtkMapPlanner
   *
   * Сигнал посылается при изменении активного галса.
   */
  hyscan_planner_selection_signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated", HYSCAN_TYPE_PLANNER_SELECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_planner_selection_init (HyScanPlannerSelection *planner_selection)
{
  planner_selection->priv = hyscan_planner_selection_get_instance_private (planner_selection);
}

static void
hyscan_planner_selection_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanPlannerSelection *planner_selection = HYSCAN_PLANNER_SELECTION (object);
  HyScanPlannerSelectionPrivate *priv = planner_selection->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_planner_selection_object_constructed (GObject *object)
{
  HyScanPlannerSelection *selection = HYSCAN_PLANNER_SELECTION (object);
  HyScanPlannerSelectionPrivate *priv = selection->priv;

  G_OBJECT_CLASS (hyscan_planner_selection_parent_class)->constructed (object);

  priv->tracks = g_array_new (TRUE, FALSE, sizeof (gchar *));
  g_array_set_clear_func (priv->tracks, hyscan_planner_selection_clear_func);

  priv->track_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->model), HYSCAN_TYPE_PLANNER_TRACK);
  priv->zone_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->model), HYSCAN_TYPE_PLANNER_ZONE);
  priv->vertex_index = -1;

  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_planner_selection_changed), selection);
}

static void
hyscan_planner_selection_object_finalize (GObject *object)
{
  HyScanPlannerSelection *planner_selection = HYSCAN_PLANNER_SELECTION (object);
  HyScanPlannerSelectionPrivate *priv = planner_selection->priv;

  if (priv->watch_db_info != NULL)
    {
      g_signal_handler_disconnect (priv->watch_db_info, priv->watch_id);
      g_object_unref (priv->watch_db_info);
    }

  g_clear_pointer (&priv->track_objects, g_hash_table_destroy);
  g_clear_pointer (&priv->zone_objects, g_hash_table_destroy);
  g_clear_object (&priv->model);
  g_array_free (priv->tracks, TRUE);
  g_free (priv->active_plan);
  g_free (priv->recording_track);

  G_OBJECT_CLASS (hyscan_planner_selection_parent_class)->finalize (object);
}

static void
hyscan_planner_selection_clear_func (gpointer data)
{
  gchar **track_ptr = data;

  g_free (*track_ptr);
}

/* Проверяем, что выбранные галсы остались в базе данных. */
static void
hyscan_planner_selection_changed (HyScanPlannerSelection *selection)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;
  guint i;
  guint len_before;

  g_clear_pointer (&priv->track_objects, g_hash_table_destroy);
  g_clear_pointer (&priv->zone_objects, g_hash_table_destroy);
  priv->track_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->model), HYSCAN_TYPE_PLANNER_TRACK);
  priv->zone_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->model), HYSCAN_TYPE_PLANNER_ZONE);

  if (priv->tracks->len == 0)
    return;

  len_before = priv->tracks->len;

  /* Удаляем из массива tracks все галсы, которых больше нет в модели. */
  if (priv->track_objects == NULL)
    {
      hyscan_planner_selection_remove_all (selection);
      return;
    }

  for (i = 0; i < priv->tracks->len;)
    {
      HyScanObject *object;
      gchar *id;

      id = g_array_index (priv->tracks, gchar *, i);
      object = g_hash_table_lookup (priv->track_objects, id);
      if (object == NULL)
        {
          g_array_remove_index (priv->tracks, i);
          continue;
        }

      ++i;
    }

  if (len_before != priv->tracks->len)
    g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

/* Обработчик сигнала HyScanDBInfo::tracks-changed.
 * Добавляет в список плановых галсов информацию о записанных галсах. */
static void
hyscan_planner_selection_track_watcher (HyScanPlannerSelection *selection)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;
  GHashTable *tracks;
  const gchar *record_id = NULL;

  /* Находим текущий записываемый галс. */
  tracks = hyscan_db_info_get_tracks (priv->watch_db_info);
  if (tracks != NULL)
    {
      GHashTableIter iter;
      HyScanTrackInfo *track_info;

      g_hash_table_iter_init (&iter, tracks);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_info) && record_id == NULL)
        {
          if (!track_info->record)
            continue;

          record_id = track_info->id;
        }
    }

  if (g_strcmp0 (record_id, priv->recording_track) != 0)
    {
      g_free (priv->recording_track);
      priv->recording_track = g_strdup (record_id);
      if (priv->recording_track != NULL)
        hyscan_planner_selection_record (selection, priv->recording_track);
    }

  g_clear_pointer (&tracks, g_hash_table_destroy);
}

/**
 * hyscan_planner_selection_new:
 * @model: указатель на #HyScanPlannerModel
 *
 * Returns: (transfer full): указатель на объект #HyScanPlannerSelection, для
 *   удаления g_object_unref().
 */
HyScanPlannerSelection *
hyscan_planner_selection_new (HyScanPlannerModel *model)
{
  return g_object_new (HYSCAN_TYPE_PLANNER_SELECTION,
                       "model", model,
                       NULL);
}

/**
 * hyscan_planner_selection_get_tracks:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Returns: (transfer full): NULL-терминированный массив выбранных галсов
 */
gchar **
hyscan_planner_selection_get_tracks (HyScanPlannerSelection *selection)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), NULL);

  return g_strdupv ((gchar **) selection->priv->tracks->data);
}

/**
 * hyscan_planner_selection_get_active_track:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Returns: идентификатор активного галса, для удаления g_free().
 */
gchar *
hyscan_planner_selection_get_active_track (HyScanPlannerSelection  *selection)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), NULL);

  return g_strdup (selection->priv->active_plan);
}

/**
 * hyscan_planner_selection_get_model:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Returns: (transfer full): модель объектов планировщика, для удаления g_object_unref().
 */
HyScanPlannerModel *
hyscan_planner_selection_get_model (HyScanPlannerSelection  *selection)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), NULL);

  return g_object_ref (selection->priv->model);
}

/**
 * hyscan_planner_selection_get_zone:
 * @selection: указатель на #HyScanPlannerSelection
 * @vertex_index: (out) (nullable): индекс выбранной вершины
 *
 * Returns: идентификатор выбранной зоны, для удаления g_free().
 */
gchar *
hyscan_planner_selection_get_zone (HyScanPlannerSelection *selection,
                                   gint                   *vertex_index)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), FALSE);

  if (priv->zone_id == NULL)
    return NULL;

  vertex_index != NULL ? (*vertex_index = priv->vertex_index) : 0;

  return g_strdup (priv->zone_id);
}

/**
 * hyscan_planner_selection_set_zone:
 * @selection: указатель на #HyScanPlannerSelection
 * @zone_id: (nullable): идентификатор выбранной зоны
 * @vertex_index: индекс выбранной вершины
 *
 * Устанавливает выбранную зону и её вершину.
 */
void
hyscan_planner_selection_set_zone (HyScanPlannerSelection *selection,
                                   const gchar            *zone_id,
                                   gint                    vertex_index)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;
  HyScanPlannerZone *object;
  gint last_vertex_index;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));

  if (zone_id != NULL)
    {
      object = g_hash_table_lookup (priv->zone_objects, zone_id);
      if (object == NULL)
        return;

      last_vertex_index = (gint) object->points_len - 1;
    }
  else
    {
      last_vertex_index = 0;
    }

  vertex_index = MIN (vertex_index, last_vertex_index);

  if (g_strcmp0 (priv->zone_id, zone_id) == 0 && priv->vertex_index == vertex_index)
    return;

  priv->zone_id = g_strdup (zone_id);
  priv->vertex_index = vertex_index;

  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_ZONE_CHANGED], 0);
}

/**
 * hyscan_planner_selection_activate:
 * @selection: указатель на HyScanPlannerSelection
 * @track_id: (nullable): идентификатор планового галса
 *
 * Активирует плановый галс @track_id.
 */
void
hyscan_planner_selection_activate (HyScanPlannerSelection *selection,
                                   const gchar            *track_id)
{
  HyScanPlannerSelectionPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  g_free (priv->active_plan);
  priv->active_plan = g_strdup (track_id);

  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_ACTIVATED], 0);
}

/**
 * hyscan_planner_selection_set_tracks:
 * @selection: указатель на #HyScanPlannerSelection
 * @tracks: NULL-терминированный список плановых галсов
 *
 * Выбирает галсы с идентификаторами @tracks.
 */
void
hyscan_planner_selection_set_tracks (HyScanPlannerSelection  *selection,
                                     gchar                  **tracks)
{
  HyScanPlannerSelectionPrivate *priv;
  gchar *track_id;
  gint i;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  g_array_remove_range (priv->tracks, 0, priv->tracks->len);
  for (i = 0; tracks[i] != NULL; ++i)
    {
      track_id = g_strdup (tracks[i]);
      g_array_append_val (priv->tracks, track_id);
    }

  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

/**
 * hyscan_planner_selection_append:
 * @selection: указатель на #HyScanPlannerSelection
 * @track_id: идентификатор планового галса
 *
 * Добавляет галс @track_id к выбору галсов.
 */
void
hyscan_planner_selection_append (HyScanPlannerSelection  *selection,
                                 const gchar             *track_id)
{
  HyScanPlannerSelectionPrivate *priv;
  gchar *new_track_id;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  if (hyscan_planner_selection_contains (selection, track_id))
    return;

  g_return_if_fail (g_hash_table_contains (priv->track_objects, track_id));

  new_track_id = g_strdup (track_id);
  g_array_append_val (priv->tracks, new_track_id);
  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

/**
 * hyscan_planner_selection_remove:
 * @selection: указатель на #HyScanPlannerSelection
 * @track_id: идентификатор планового галса
 *
 * Удаляет плановый галс @track_id из выбора галсов.
 */
void
hyscan_planner_selection_remove (HyScanPlannerSelection  *selection,
                                 const gchar             *track_id)
{
  HyScanPlannerSelectionPrivate *priv;
  guint i;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  for (i = 0; i < priv->tracks->len; i++)
    {
      if (g_strcmp0 (track_id, g_array_index (priv->tracks, gchar *, i)) != 0)
        continue;

      g_array_remove_index (priv->tracks, i);
      g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
      return;
    }
}

/**
 * hyscan_planner_selection_remove_all:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Удаляет все галсы из выбора.
 */
void
hyscan_planner_selection_remove_all (HyScanPlannerSelection  *selection)
{
  HyScanPlannerSelectionPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  if (priv->tracks->len == 0)
    return;

  g_array_remove_range (priv->tracks, 0, priv->tracks->len);
  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

/**
 * hyscan_planner_selection_contains:
 * @selection: указатель на #HyScanPlannerSelection
 * @track_id: идентификатор планового галса
 *
 * Проверяет, находится ли плановый галс в выборе, или нет.
 *
 * Returns: %TRUE, если выбор содержит плановый галс @track_id, иначе %FALSE
 */
gboolean
hyscan_planner_selection_contains (HyScanPlannerSelection  *selection,
                                   const gchar             *track_id)
{
  HyScanPlannerSelectionPrivate *priv;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), FALSE);
  priv = selection->priv;

  if (priv->tracks == NULL)
    return FALSE;

  for (i = 0; i < priv->tracks->len; ++i)
    {
      const gchar *id;

      id = g_array_index (priv->tracks, gchar *, i);
      if (g_strcmp0 (id, track_id) == 0)
        return TRUE;
    }

  return FALSE;
}

/**
 * hyscan_planner_selection_watch_records:
 * @selection: указатель на #HyScanPlannerSelection
 * @db_info: (nullable): модель галсов #HyScanDBInfo для отслеживания или %NULL для прекращения отслеживания.
 *
 * Функция включает автоматическое связывание текущего плана с записываемым в БД галса.
 *
 * Активный план, указанный в hyscan_planner_selection_activate(), будет связываться
 * со всеми записываемыми галсами.
 *
 * Галс считается записываемым, если он имеет значение #HyScanTrackInfo.record = %TRUE.
 */
void
hyscan_planner_selection_watch_records (HyScanPlannerSelection *selection,
                                        HyScanDBInfo           *db_info)
{
  HyScanPlannerSelectionPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;


  if (priv->watch_db_info != NULL)
    {
      g_signal_handler_disconnect (priv->watch_db_info, priv->watch_id);
      g_object_unref (priv->watch_db_info);
    }

  priv->watch_db_info = g_object_ref (db_info);
  priv->watch_id = g_signal_connect_swapped (priv->watch_db_info,
                                             "tracks-changed",
                                             G_CALLBACK (hyscan_planner_selection_track_watcher),
                                             selection);
}

/**
 * hyscan_planner_selection_record:
 * @selection: указатель на #HyScanPlannerSelection
 * @track_id: идентификатор записываемого галса
 *
 * Функция добавляет в активному плану галс с идентификатором @track_id.
 */
void
hyscan_planner_selection_record (HyScanPlannerSelection *selection,
                                 const gchar            *track_id)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;
  gchar *active_track;
  HyScanPlannerTrack *track = NULL;

  active_track = hyscan_planner_selection_get_active_track (selection);
  if (active_track == NULL)
    return;

  track = (HyScanPlannerTrack *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (priv->model),
                                                          HYSCAN_TYPE_PLANNER_TRACK,
                                                          active_track);
  if (!HYSCAN_IS_PLANNER_TRACK (track))
    goto exit;

  if (track->records != NULL && g_strv_contains ((const gchar *const *) track->records, track_id))
    goto exit;

  hyscan_planner_track_record_append (track, track_id);
  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (priv->model), active_track, (const HyScanObject *) track);

exit:
  g_free (active_track);
  g_clear_pointer (&track, hyscan_planner_track_free);
}
