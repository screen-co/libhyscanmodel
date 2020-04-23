#include "hyscan-map-track-model.h"

#define REFRESH_INTERVAL              300000     /* Период обновления данных, мкс. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_ACTIVE_TRACKS,
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_PARAM_SET,
  SIGNAL_LAST
};

struct _HyScanMapTrackModelPrivate
{
  gboolean                 shutdown;
  gboolean                 suspend; // todo: надо ли эту функциональность?
  GHashTable              *tracks;
  GThread                 *watcher;
  GMutex                   lock;          /* Доступ к tracks и project. */
  gchar                   *project;
  HyScanDB                *db;
  HyScanCache             *cache;
  HyScanGeoProjection     *projection;
  gchar                  **active_tracks;
};

static void        hyscan_map_track_model_set_property        (GObject       *object,
                                                               guint          prop_id,
                                                               const GValue  *value,
                                                               GParamSpec    *pspec);
static void        hyscan_map_track_model_get_property        (GObject       *object,
                                                               guint          prop_id,
                                                               GValue        *value,
                                                               GParamSpec    *pspec);
static void        hyscan_map_track_model_object_constructed  (GObject       *object);
static void        hyscan_map_track_model_object_finalize     (GObject       *object);
static gpointer    hyscan_map_track_model_watcher             (gpointer       data);


static guint       hyscan_map_track_model_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMapTrackModel, hyscan_map_track_model, G_TYPE_OBJECT)

static void
hyscan_map_track_model_class_init (HyScanMapTrackModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_map_track_model_set_property;
  object_class->get_property = hyscan_map_track_model_get_property;

  object_class->constructed = hyscan_map_track_model_object_constructed;
  object_class->finalize = hyscan_map_track_model_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "HyScanDB", "HyScanDB object", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "HyScanCache", "HyScanCache object", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_ACTIVE_TRACKS,
    g_param_spec_boxed ("active-tracks", "Active tracks", "NULL-terminated array of active tracks", G_TYPE_STRV,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * HyScanMapTrackModel::changed:
   * @model: указатель на #HyScanMapTrackModel
   *
   * Сигнал посылается при изменении данных галсов.
   */
  hyscan_map_track_model_signals[SIGNAL_CHANGED] =
      g_signal_new ("changed", HYSCAN_TYPE_MAP_TRACK_MODEL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  /**
   * HyScanMapTrackModel::param-set:
   * @model: указатель на #HyScanMapTrackModel
   *
   * Сигнал посылается при изменении параметров галсов.
   */
  hyscan_map_track_model_signals[SIGNAL_PARAM_SET] =
      g_signal_new ("param-set", HYSCAN_TYPE_MAP_TRACK_MODEL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
}

static void
hyscan_map_track_model_init (HyScanMapTrackModel *map_track_model)
{
  map_track_model->priv = hyscan_map_track_model_get_instance_private (map_track_model);
}

static void
hyscan_map_track_model_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanMapTrackModel *map_track_model = HYSCAN_MAP_TRACK_MODEL (object);
  HyScanMapTrackModelPrivate *priv = map_track_model->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_ACTIVE_TRACKS:
      hyscan_map_track_model_set_tracks (map_track_model, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_map_track_model_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  HyScanMapTrackModel *map_track_model = HYSCAN_MAP_TRACK_MODEL (object);
  HyScanMapTrackModelPrivate *priv = map_track_model->priv;

  switch (prop_id)
    {
    case PROP_ACTIVE_TRACKS:
      g_value_take_boxed (value, hyscan_map_track_model_get_tracks (map_track_model));

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_map_track_model_object_constructed (GObject *object)
{
  HyScanMapTrackModel *model = HYSCAN_MAP_TRACK_MODEL (object);
  HyScanMapTrackModelPrivate *priv = model->priv;

  G_OBJECT_CLASS (hyscan_map_track_model_parent_class)->constructed (object);

  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                        (GDestroyNotify) hyscan_map_track_model_info_unref);

  g_mutex_init (&priv->lock);
  priv->watcher = g_thread_new ("mtrack-watch", hyscan_map_track_model_watcher, model);
}

static void
hyscan_map_track_model_object_finalize (GObject *object)
{
  HyScanMapTrackModel *map_track_model = HYSCAN_MAP_TRACK_MODEL (object);
  HyScanMapTrackModelPrivate *priv = map_track_model->priv;

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_clear_pointer (&priv->watcher, g_thread_join);
  g_mutex_clear (&priv->lock);
  g_strfreev (priv->active_tracks);
  g_free (priv->project);
  g_clear_object (&priv->projection);
  g_hash_table_destroy (priv->tracks);

  G_OBJECT_CLASS (hyscan_map_track_model_parent_class)->finalize (object);
}

static HyScanMapTrackModelInfo *
hyscan_map_track_model_get_info (HyScanMapTrackModel *model,
                                 const gchar         *track_name)
{
  HyScanMapTrackModelPrivate *priv = model->priv;
  HyScanMapTrackModelInfo *info;

  g_mutex_lock (&priv->lock);
  info = g_hash_table_lookup (priv->tracks, track_name);
  info = hyscan_map_track_model_info_ref (info);
  g_mutex_unlock (&priv->lock);

  return info;
}

/* Запрашивает перерисовку слоя, если есть изменения в каналах данных. */
static gpointer
hyscan_map_track_model_watcher (gpointer data)
{
  HyScanMapTrackModel *model = HYSCAN_MAP_TRACK_MODEL (data);
  HyScanMapTrackModelPrivate *priv = model->priv;

  gboolean data_changed, param_changed;
  gchar **active_tracks = NULL;
  gint i;

  HyScanMapTrackModelInfo *info;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      g_usleep (REFRESH_INTERVAL);

      if (priv->suspend)
        continue;

      g_mutex_lock (&priv->lock);
      g_clear_pointer (&active_tracks, g_strfreev);
      active_tracks = g_strdupv (priv->active_tracks);
      g_mutex_unlock (&priv->lock);

      /* Проверяем, есть ли новые данные в каждом из галсов. */
      data_changed = param_changed = FALSE;
      for (i = 0; active_tracks[i] != NULL; i++)
        {
          guint32 mod_count, param_mod_count;

          info = hyscan_map_track_model_get_info (model, active_tracks[i]);
          if (info == NULL)
            continue;

          /* Блокируем доступ к галсу и считываем мод-каунты. */
          g_mutex_lock (&info->mutex);

          mod_count = hyscan_map_track_get_mod_count (info->track);
          data_changed |= (mod_count != info->mod_count);
          info->mod_count = mod_count;

          param_mod_count = hyscan_map_track_param_get_mod_count (hyscan_map_track_get_param (info->track));
          param_changed |= (param_mod_count != info->param_mod_count);
          info->param_mod_count = param_mod_count;

          g_mutex_unlock (&info->mutex);

          hyscan_map_track_model_info_unref (info);
        }

      // todo: сделать отправку из main loop
      if (data_changed)
        g_signal_emit (model, hyscan_map_track_model_signals[SIGNAL_CHANGED], 0);

      if (param_changed)
        g_signal_emit (model, hyscan_map_track_model_signals[SIGNAL_PARAM_SET], 0);
    }

  return NULL;
}

HyScanMapTrackModel *
hyscan_map_track_model_new (HyScanDB                  *db,
                            HyScanCache               *cache)
{
  return g_object_new (HYSCAN_TYPE_MAP_TRACK_MODEL,
                       "db", db,
                       "cache", cache,
                       NULL);
}

void
hyscan_map_track_model_set_project (HyScanMapTrackModel *model,
                                    const gchar         *project)
{
  HyScanMapTrackModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model));
  priv = model->priv;

  /* Удаляем текущие галсы. */
  g_mutex_lock (&priv->lock);
  g_hash_table_remove_all (priv->tracks);
  g_free (priv->project);
  priv->project = g_strdup (project);
  g_mutex_unlock (&priv->lock);
}

void
hyscan_map_track_model_set_tracks (HyScanMapTrackModel  *model,
                                   gchar               **tracks)
{
  HyScanMapTrackModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model));
  priv = model->priv;

  g_mutex_lock (&priv->lock);
  g_strfreev (priv->active_tracks);
  priv->active_tracks = tracks != NULL ? g_strdupv (tracks) : g_new0 (gchar *, 1);
  g_mutex_unlock (&priv->lock);

  g_object_notify (G_OBJECT (model), "active-tracks");
}

gchar **
hyscan_map_track_model_get_tracks (HyScanMapTrackModel *model)
{
  HyScanMapTrackModelPrivate *priv;
  gchar **active_tracks;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model), NULL);
  priv = model->priv;

  g_mutex_lock (&priv->lock);
  active_tracks = g_strdupv (priv->active_tracks);
  g_mutex_unlock (&priv->lock);

  return active_tracks;
}

static void
hyscan_map_track_model_emit_param_changed (HyScanMapTrackModel *model)
{
  g_signal_emit (model, hyscan_map_track_model_signals[SIGNAL_PARAM_SET], 0);
}

static void
hyscan_map_track_model_mutex_free (GMutex *mutex)
{
  g_mutex_clear (mutex);
  g_free (mutex);
}

HyScanMapTrackModelInfo *
hyscan_map_track_model_get (HyScanMapTrackModel  *model,
                            const gchar          *track_name)
{
  HyScanMapTrackModelPrivate *priv;
  HyScanMapTrackModelInfo *info;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model), NULL);

  priv = model->priv;
  g_mutex_lock (&priv->lock);
  info = g_hash_table_lookup (priv->tracks, track_name);
  if (info == NULL && priv->project != NULL)
    {
      info = g_slice_new0 (HyScanMapTrackModelInfo);
      g_atomic_int_set (&info->ref_count, 1);
      info->track = hyscan_map_track_new (priv->db, priv->cache, priv->project, track_name, priv->projection);
      g_mutex_init (&info->mutex);

      g_hash_table_insert (priv->tracks, g_strdup (track_name), info);
    }
  info = hyscan_map_track_model_info_ref (info);
  g_mutex_unlock (&priv->lock);

  return info;
}

HyScanMapTrackModelInfo *
hyscan_map_track_model_lookup (HyScanMapTrackModel  *model,
                               const gchar          *track_name)
{
  HyScanMapTrackModelPrivate *priv = model->priv;
  HyScanMapTrackModelInfo *info;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model), NULL);

  info = hyscan_map_track_model_get (model, track_name);

  g_mutex_lock (&info->mutex);
  hyscan_map_track_set_projection (info->track, priv->projection);

  return info;
}

void
hyscan_map_track_model_release (HyScanMapTrackModel *model,
                                HyScanMapTrackModelInfo *track)
{
  g_mutex_unlock (&track->mutex);
  hyscan_map_track_model_info_unref (track);
}

HyScanMapTrackParam *
hyscan_map_track_model_param (HyScanMapTrackModel       *model,
                              const gchar               *track_name)
{
  HyScanMapTrackModelPrivate *priv = model->priv;
  HyScanMapTrackParam *param;
  gchar *project;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model), NULL);

  g_mutex_lock (&priv->lock);
  project = g_strdup (priv->project);
  g_mutex_unlock (&priv->lock);

  param = hyscan_map_track_param_new (NULL, priv->db, project, track_name);
  g_free (project);
  
  return param;
}

HyScanMapTrackModelInfo *
hyscan_map_track_model_info_ref (HyScanMapTrackModelInfo *info)
{
  gboolean already_finalized;
  gint old_value;

  if (info == NULL)
    return NULL;

  old_value = g_atomic_int_add (&info->ref_count, 1);
  already_finalized = (old_value == 0);
  g_return_val_if_fail (!already_finalized, NULL);


  return info;
}

void
hyscan_map_track_model_info_unref (HyScanMapTrackModelInfo *info)
{
  if (!g_atomic_int_dec_and_test (&info->ref_count))
    return;

  g_object_unref (info->track);
  g_mutex_clear (&info->mutex);
}

void
hyscan_map_track_model_set_projection (HyScanMapTrackModel *model,
                                       HyScanGeoProjection *projection)
{
  HyScanMapTrackModelPrivate *priv;
  GHashTableIter iter;
  HyScanMapTrack *track;

  g_return_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model));
  priv = model->priv;

  g_clear_object (&priv->projection);
  priv->projection = g_object_ref (projection);
}
