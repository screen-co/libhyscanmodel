/* hyscan-map-track-model.c
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
 * SECTION: hyscan-map-track-model
 * @Short_description: Модель проекций галсов на карту
 * @Title: HyScanMapTrackModel
 *
 * Модель управляет доступом к объектам проекции галса на карту #HyScanMapTrack, а также следит за изменениями
 * в данных и параметрах этих объектов.
 *
 * #HyScanMapTrackModel создаёт и хранит в себе указатели на обработчики #HyScanMapTrack, и предоставляет к ним доступ
 * через структуры #HyScanMapTrackModelInfo. Таким образом класс позволяет повторно использовать уже созданные обработчики,
 * при этом предоставляя доступ к каждому из них одновременно только одному клиенту.
 *
 * Функции:
 * - hyscan_map_track_model_set_project() - установка текущего проекта;
 * - hyscan_map_track_model_set_tracks() - установка списка активных галсов;
 * - hyscan_map_track_model_get() - получение структуры доступа к #HyScanMapTrack;
 * - hyscan_map_track_model_lock() - получение монопольного доступа к #HyScanMapTrack;
 * - hyscan_map_track_model_param() - получение параметров галса;
 * - hyscan_map_track_model_set_projection() - устанавливает картографическую проекцию;
 *
 */

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
  HyScanDB                *db;             /* База данных. */
  HyScanCache             *cache;          /* Кэш. */

  gchar                   *project;        /* Имя проекта. */
  gchar                  **active_tracks;  /* NULL-терминированный список активных галсов. */
  HyScanGeoProjection     *projection;     /* Текущая картографическая проекция. */
  GMutex                   lock;           /* Доступ к active_tracks, project и projection. */

  GHashTable              *tracks;         /* Таблица созданных обработчиков: { имя галса: #HyScanMapTrackModelInfo }. */
  GThread                 *watcher;        /* Поток проверки изменений. */
  gboolean                 shutdown;       /* Признак завершения работы. */
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
  g_object_unref (priv->db);
  g_object_unref (priv->cache);

  G_OBJECT_CLASS (hyscan_map_track_model_parent_class)->finalize (object);
}

/* Следит за изменениями в данных и параметрах проекции и отправляет сигналы, если есть изменения. */
static gpointer
hyscan_map_track_model_watcher (gpointer data)
{
  HyScanMapTrackModel *model = HYSCAN_MAP_TRACK_MODEL (data);
  HyScanMapTrackModelPrivate *priv = model->priv;

  gboolean data_changed, param_changed;
  gchar **active_tracks = NULL;
  gint i;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      g_usleep (REFRESH_INTERVAL);

      g_mutex_lock (&priv->lock);
      active_tracks = g_strdupv (priv->active_tracks);
      g_mutex_unlock (&priv->lock);

      /* Проверяем, есть ли новые данные в каждом из галсов. */
      data_changed = param_changed = FALSE;
      for (i = 0; active_tracks[i] != NULL; i++)
        {
          HyScanMapTrackModelInfo *info;
          guint32 mod_count, param_mod_count;

          /* Поулчает объект доступа к проекции галса. */
          g_mutex_lock (&priv->lock);
          info = g_hash_table_lookup (priv->tracks, active_tracks[i]);
          hyscan_map_track_model_info_ref (info);
          g_mutex_unlock (&priv->lock);

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

      g_clear_pointer (&active_tracks, g_strfreev);

      // todo: сделать отправку из main loop
      if (data_changed)
        g_signal_emit (model, hyscan_map_track_model_signals[SIGNAL_CHANGED], 0);

      if (param_changed)
        g_signal_emit (model, hyscan_map_track_model_signals[SIGNAL_PARAM_SET], 0);
    }

  return NULL;
}

/**
 * hyscan_map_track_model_new:
 * @db: указатель на базу данных #HyScanDB
 * @cache: указатель на кэш #HyScanCache
 *
 * Функция создаёт новый объект #HyScanMapTrackModel
 *
 * Returns: (transfer full): указатель на #HyScanMapTrackModel, для удаления g_object_unref().
 */
HyScanMapTrackModel *
hyscan_map_track_model_new (HyScanDB    *db,
                            HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_MAP_TRACK_MODEL,
                       "db", db,
                       "cache", cache,
                       NULL);
}

/**
 * hyscan_map_track_model_set_project:
 * @model: указатель на #HyScanMapTrackModel
 * @project: имя проекта
 *
 * Функция устанавливает имя проекта.
 */
void
hyscan_map_track_model_set_project (HyScanMapTrackModel *model,
                                    const gchar         *project)
{
  HyScanMapTrackModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model));
  priv = model->priv;

  g_mutex_lock (&priv->lock);
  /* Удаляем текущие обработчики галсов. */
  g_hash_table_remove_all (priv->tracks);
  g_free (priv->project);
  priv->project = g_strdup (project);
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_map_track_model_set_tracks:
 * @model: указатель на #HyScanMapTrackModel
 * @tracks: NULL-терминированный список активных галсов
 *
 * Функция устанавливает список активных галсов.
 */
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

/**
 * hyscan_map_track_model_get:
 * @model: указатель на #HyScanMapTrackModel
 * @track_name: имя галса
 *
 * Функция получает структуру доступа к #HyScanMapTrack. Для получения сконфигурированного объекта #HyScanMapTrack
 * и эксклюзивного доступа к нему воспользуйтесь функцией hyscan_map_track_model_lock().
 *
 * Returns: указатель на #HyScanMapTrackModelInfo, для удаления hyscan_map_track_model_info_unref()
 */
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

/**
 * hyscan_map_track_model_lock:
 * @model: указатель на #HyScanMapTrackModelInfo
 * @track_name: имя галса
 *
 * Функция блокирует доступ к проекции галса на карт, конфигурирует её и возвращает указать на структуру
 * доступа к этой проекции.
 *
 * По окончанию использования объекта необходимо вызвать hyscan_map_track_model_unlock() для разблокировки.
 *
 * Returns: (transfer none): указатель на #HyScanMapTrackModelInfo
 */
HyScanMapTrackModelInfo *
hyscan_map_track_model_lock (HyScanMapTrackModel  *model,
                             const gchar          *track_name)
{
  HyScanMapTrackModelPrivate *priv = model->priv;
  HyScanMapTrackModelInfo *info;
  HyScanGeoProjection *projection;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model), NULL);

  info = hyscan_map_track_model_get (model, track_name);

  g_mutex_lock (&info->mutex);

  g_mutex_lock (&priv->lock);
  projection = g_object_ref (priv->projection);
  g_mutex_unlock (&priv->lock);

  hyscan_map_track_set_projection (info->track, projection);

  return info;
}

/**
 * hyscan_map_track_model_unlock:
 * @track_info: указатель на #HyScanMapTrackModelInfo
 *
 * Функия разблокирует доступ к #HyScanMapTrackModelInfo. Функцию необходимо вызывать после завершения работы
 * со структурой, полученной из hyscan_map_track_model_lock().
 */
void
hyscan_map_track_model_unlock (HyScanMapTrackModelInfo *track_info)
{
  g_mutex_unlock (&track_info->mutex);
  hyscan_map_track_model_info_unref (track_info);
}

/**
 * hyscan_map_track_model_param:
 * @model: указатель на #HyScanMapTrackModel
 * @track_name: имя галса
 *
 * Функция получает параметры указанного галса.
 *
 * Returns: (transfer full): указатель на новый объект #HyScanMapTrackParam, для удаления g_object_unref().
 */
HyScanMapTrackParam *
hyscan_map_track_model_param (HyScanMapTrackModel *model,
                              const gchar         *track_name)
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

/**
 * hyscan_map_track_model_set_projection:
 * @model: указатель на #HyScanMapTrackModel
 * @projection: картографическая проекция #HyScanGeoProjection
 *
 * Функция устанавливает текущую картографическую проекцию.
 */
void
hyscan_map_track_model_set_projection (HyScanMapTrackModel *model,
                                       HyScanGeoProjection *projection)
{
  HyScanMapTrackModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TRACK_MODEL (model));
  priv = model->priv;

  g_mutex_lock (&priv->lock);
  g_clear_object (&priv->projection);
  priv->projection = g_object_ref (projection);
  g_mutex_unlock (&priv->lock);
}


/**
 * hyscan_map_track_model_info_ref:
 * @info: указатель на  #HyScanMapTrackModelInfo
 *
 * Функция атомарно увеличивает счётчик ссылок на объект #HyScanMapTrackModelInfo.
 *
 * Returns: указатель на полученный объект
 */
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

/**
 * hyscan_map_track_model_info_unref:
 * @info: указатель на #HyScanMapTrackModelInfo
 *
 * Функция уменьшает счётчик ссылок на #HyScanMapTrackModelInfo. Если ссылок больше не осталось, объект удаляется.
 */
void
hyscan_map_track_model_info_unref (HyScanMapTrackModelInfo *info)
{
  if (!g_atomic_int_dec_and_test (&info->ref_count))
    return;

  g_object_unref (info->track);
  g_mutex_clear (&info->mutex);
}
