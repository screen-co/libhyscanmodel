/*
 * hyscan-model-manager.h
 *
 *  Created on: 12 фев. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#ifndef __HYSCAN_MODEL_MANAGER_H__
#define __HYSCAN_MODEL_MANAGER_H__

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

/* Сигналы.
 * Должны идти в порядке соответвующем signals[].
 * */
typedef enum
{
  SIGNAL_ACOUSTIC_MARKS_CHANGED,      /* Изменение данных в модели "водопадных" меток. */
  SIGNAL_GEO_MARKS_CHANGED,           /* Изменение данных в модели гео-меток. */
  SIGNAL_ACOUSTIC_MARKS_LOC_CHANGED,  /* Изменение данных в модели "водопадных" меток с координатами. */
  SIGNAL_LABELS_CHANGED,              /* изменение данных в модели групп. */
  SIGNAL_TRACKS_CHANGED,              /* Изменение данных в модели галсов. */
  SIGNAL_GROUPING_CHANGED,            /* Изменение типа группировки. */
  SIGNAL_EXPAND_NODES_MODE_CHANGED,   /* Изменение режима ототбражения всех узлов. */
  SIGNAL_MODEL_MANAGER_LAST           /* Количество сигналов. */
}ModelManagerSignal;

/* Тип представления. */
typedef enum
{
  UNGROUPED,       /* Табличное. */
  BY_LABELS,       /* Древовидный с группировкой по группам. */
  BY_TYPES,        /* Древовидный с группировкой по типам. */
  N_VIEW_TYPES     /* Количество типов представления. */
}ModelManagerGrouping;

/* Типы объектов. */
enum
{
  LABEL,          /* Группа. */
  GEO_MARK,       /* гео-метка. */
  ACOUSTIC_MARK, /* Акустическая метка. */
  TRACK,          /* Галс. */
  TYPES           /* Количество объектов. */
};

struct _HyScanModelManager
{
  GObject parent_instance;

  HyScanModelManagerPrivate *priv;
};

struct _HyScanModelManagerClass
{
  GObjectClass parent_class;
};

GType                hyscan_model_manager_get_type                    (void);

HyScanModelManager*  hyscan_model_manager_new                         (const gchar          *project_name,
                                                                       HyScanDB             *db,
                                                                       HyScanCache          *cache);

HyScanDBInfo*        hyscan_model_manager_get_track_model             (HyScanModelManager   *self);

HyScanObjectModel*   hyscan_model_manager_get_acoustic_mark_model     (HyScanModelManager   *self);

HyScanObjectModel*   hyscan_model_manager_get_geo_mark_model          (HyScanModelManager   *self);

HyScanObjectModel*   hyscan_model_manager_get_label_model             (HyScanModelManager   *self);

HyScanMarkLocModel*  hyscan_model_manager_get_acoustic_mark_loc_model (HyScanModelManager   *self);

const gchar*         hyscan_model_manager_get_signal_title            (HyScanModelManager   *self,
                                                                       ModelManagerSignal    signal_title);

gchar*               hyscan_model_manager_get_project_name            (HyScanModelManager   *self);

HyScanDB*            hyscan_model_manager_get_db                      (HyScanModelManager   *self);

HyScanCache*         hyscan_model_manager_get_cache                   (HyScanModelManager   *self);

GHashTable*          hyscan_model_manager_get_all_labels              (HyScanModelManager   *self);

GHashTable*          hyscan_model_manager_get_all_geo_marks           (HyScanModelManager   *self);

GHashTable*          hyscan_model_manager_get_all_acoustic_marks_loc  (HyScanModelManager   *self);

GHashTable*          hyscan_model_manager_get_all_tracks              (HyScanModelManager   *self);

void                 hyscan_model_manager_set_grouping                (HyScanModelManager   *self,
                                                                       ModelManagerGrouping  grouping);

ModelManagerGrouping hyscan_model_manager_get_grouping                (HyScanModelManager   *self);

void                 hyscan_model_manager_set_expand_nodes_mode       (HyScanModelManager   *self,
                                                                       gboolean              expand_nodes_mode);

gboolean             hyscan_model_manager_get_expand_nodes_mode       (HyScanModelManager   *self);

G_END_DECLS

#endif /* __HYSCAN_MODEL_MANAGER_H__ */
