/*
 * hyscan-model-manager.h
 *
 *  Created on: 12 фев. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#ifndef __HYSCAN_MODEL_MANAGER_H__
#define __HYSCAN_MODEL_MANAGER_H__

/*#include <gtk/gtk.h>*/

#include <hyscan-db-info.h>
#include <hyscan-object-model.h>
#include <hyscan-mark-loc-model.h>
#include <hyscan-object-data-label.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MODEL_MANAGER             (hyscan_model_manager_get_type ())
#define HYSCAN_MODEL_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MODEL_MANAGER, HyScanModelManager))
#define HYSCAN_IS_MODEL_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MODEL_MANAGER))
#define HYSCAN_MODEL_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MODEL_MANAGER, HyScanModelManagerClass))
#define HYSCAN_IS_MODEL_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MODEL_MANAGER))
#define HYSCAN_MODEL_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MODEL_MANAGER, HyScanModelManagerClass))

typedef struct _HyScanModelManager HyScanModelManager;
typedef struct _HyScanModelManagerPrivate HyScanModelManagerPrivate;
typedef struct _HyScanModelManagerClass HyScanModelManagerClass;

/* Сигналы.*/
typedef enum
{
  SIGNAL_WF_MARKS_CHANGED,      /* Изменение данных в модели "водопадных" меток. */
  SIGNAL_GEO_MARKS_CHANGED,     /* Изменение данных в модели гео-меток. */
  SIGNAL_WF_MARKS_LOC_CHANGED,  /* Изменение данных в модели "водопадных" меток с координатами. */
  SIGNAL_LABELS_CHANGED,        /* изменение данных в модели групп. */
  SIGNAL_TRACKS_CHANGED,        /* Изменение данных в модели галсов. */
  SIGNAL_LAST
}ModelManagerSignal;

struct _HyScanModelManager
{
  GObject parent_instance;

  HyScanModelManagerPrivate *priv;
};

struct _HyScanModelManagerClass
{
  GObjectClass parent_class;
};

GType                 hyscan_model_manager_get_type               (void);

HyScanModelManager*   hyscan_model_manager_new                    (const gchar             *project_name,
                                                                   HyScanDB                *db,
                                                                   HyScanCache             *cache);

HyScanDBInfo*         hyscan_model_manager_get_track_model        (HyScanModelManager      *self);

HyScanObjectModel*    hyscan_model_manager_get_wf_mark_model      (HyScanModelManager      *self);

HyScanObjectModel*    hyscan_model_manager_get_geo_mark_model     (HyScanModelManager      *self);

HyScanObjectModel*    hyscan_model_manager_get_label_model        (HyScanModelManager      *self);

HyScanMarkLocModel*   hyscan_model_manager_get_wf_mark_loc_model  (HyScanModelManager      *self);

const gchar*          hyscan_model_manager_get_signal_title       (HyScanModelManager      *self,
                                                                   ModelManagerSignal       signal_title);

gchar*                hyscan_model_manager_get_project_name       (HyScanModelManager      *self);

HyScanDB*             hyscan_model_manager_get_db                 (HyScanModelManager      *self);

HyScanCache*          hyscan_model_manager_get_cache              (HyScanModelManager      *self);

GHashTable*           hyscan_model_manager_get_all_labels         (HyScanModelManager      *self);

GHashTable*           hyscan_model_manager_get_all_geo_marks      (HyScanModelManager      *self);

GHashTable*           hyscan_model_manager_get_all_wf_marks_loc   (HyScanModelManager      *self);

GHashTable*           hyscan_model_manager_get_all_tracks         (HyScanModelManager      *self);

G_END_DECLS

#endif /* __HYSCAN_MODEL_MANAGER_H__ */
