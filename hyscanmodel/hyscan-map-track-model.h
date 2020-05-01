#ifndef __HYSCAN_MAP_TRACK_MODEL_H__
#define __HYSCAN_MAP_TRACK_MODEL_H__

#include <hyscan-map-track.h>
#include <hyscan-map-track-param.h>
#include <hyscan-track-proj-quality.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TRACK_MODEL             (hyscan_map_track_model_get_type ())
#define HYSCAN_MAP_TRACK_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TRACK_MODEL, HyScanMapTrackModel))
#define HYSCAN_IS_MAP_TRACK_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TRACK_MODEL))
#define HYSCAN_MAP_TRACK_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TRACK_MODEL, HyScanMapTrackModelClass))
#define HYSCAN_IS_MAP_TRACK_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TRACK_MODEL))
#define HYSCAN_MAP_TRACK_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TRACK_MODEL, HyScanMapTrackModelClass))

typedef struct _HyScanMapTrackModel HyScanMapTrackModel;
typedef struct _HyScanMapTrackModelPrivate HyScanMapTrackModelPrivate;
typedef struct _HyScanMapTrackModelClass HyScanMapTrackModelClass;

/**
 * HyScanMapTrackModelInfo:
 *
 * @track: проекция галса на карту #HyScanMapTrack
 * @mutex: блокировка доступа к #HyScanMapTrack
 *
 * Структура для управления доступом к проекции галса на карту #HyScanMapTrack.
 */
typedef struct
{
  HyScanMapTrack    *track;
  GMutex             mutex;

  /* Приватная часть. */
  guint              ref_count;           /* Счётчик ссылок. */
  guint              mod_count;           /* Номер изменений данных, обработанный моделью. */
  guint              param_mod_count;     /* Номер изменений параметров, обработанный моделью. */
} HyScanMapTrackModelInfo;

struct _HyScanMapTrackModel
{
  GObject parent_instance;

  HyScanMapTrackModelPrivate *priv;
};

struct _HyScanMapTrackModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                     hyscan_map_track_model_get_type         (void);

HYSCAN_API
HyScanMapTrackModel *     hyscan_map_track_model_new              (HyScanDB                  *db,
                                                                   HyScanCache               *cache);

HYSCAN_API
void                      hyscan_map_track_model_set_project      (HyScanMapTrackModel       *model,
                                                                   const gchar               *project);

HYSCAN_API
void                      hyscan_map_track_model_set_tracks       (HyScanMapTrackModel       *model,
                                                                   gchar                    **tracks);

HYSCAN_API
gchar **                  hyscan_map_track_model_get_tracks       (HyScanMapTrackModel       *model);

HYSCAN_API
HyScanMapTrackModelInfo * hyscan_map_track_model_get              (HyScanMapTrackModel       *model,
                                                                   const gchar               *track_name);

HYSCAN_API
HyScanMapTrackModelInfo * hyscan_map_track_model_lock             (HyScanMapTrackModel       *model,
                                                                   const gchar               *track_name);

HYSCAN_API
void                      hyscan_map_track_model_unlock           (HyScanMapTrackModelInfo   *track_info);

HYSCAN_API
HyScanMapTrackParam *     hyscan_map_track_model_param            (HyScanMapTrackModel       *model,
                                                                   const gchar               *track_name);


HYSCAN_API
void                      hyscan_map_track_model_set_projection   (HyScanMapTrackModel       *model,
                                                                   HyScanGeoProjection       *projection);

HYSCAN_API
HyScanMapTrackModelInfo * hyscan_map_track_model_info_ref         (HyScanMapTrackModelInfo   *info);

HYSCAN_API
void                      hyscan_map_track_model_info_unref       (HyScanMapTrackModelInfo   *info);

G_END_DECLS

#endif /* __HYSCAN_MAP_TRACK_MODEL_H__ */
