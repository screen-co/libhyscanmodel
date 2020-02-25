/*
 * hyscan-model-manager.c
 *
 *  Created on: 12 фев. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include <hyscan-model-manager.h>

enum
{
  PROP_PROJECT_NAME = 1,    /* Название проекта. */
  PROP_DB,                  /* База данных. */
  PROP_CACHE,               /* Кэш.*/
  N_PROPERTIES
};

struct _HyScanModelManagerPrivate
{
  HyScanObjectModel    *acoustic_marks_model, /* Модель данных акустических меток. */
                       *geo_mark_model,       /* Модель данных гео-меток. */
                       *label_model;          /* Модель данных групп. */
  HyScanMarkLocModel   *acoustic_loc_model;    /* Модель данных "водопадных" меток с координатами. */
  HyScanDBInfo         *track_model;       /* Модель данных галсов. */
  HyScanCache          *cache;             /* Кэш.*/
  HyScanDB             *db;                /* База данных. */
  gchar                *project_name;      /* Название проекта. */

  ModelManagerGrouping  grouping;          /* Тип группировки. */
  gboolean              expand_nodes_mode; /* Развернуть/свернуть все узлы. */
};
/* Названия сигналов.
 * Должны идти в порядке соответвующем ModelManagerSignal
 * */
static const gchar *signals[] = {"wf-marks-changed",     /* Изменение данных в модели "водопадных" меток. */
                                 "geo-marks-changed",    /* Изменение данных в модели гео-меток. */
                                 "wf-marks-loc-changed", /* Изменение данных в модели "водопадных" меток
                                                          * с координатами. */
                                 "labels-changed",       /* Изменение данных в модели групп. */
                                 "tracks-changed",       /* Изменение данных в модели галсов. */
                                 "grouping-changed",     /* Изменение типа группировки. */
                                 "expand-mode-changed"}; /* изменение режима отображения узлов. */

static void       hyscan_model_manager_set_property                     (GObject               *object,
                                                                         guint                  prop_id,
                                                                         const GValue          *value,
                                                                         GParamSpec            *pspec);

static void       hyscan_model_manager_constructed                      (GObject               *object);

static void       hyscan_model_manager_finalize                         (GObject               *object);

static void       hyscan_model_manager_acoustic_marks_model_changed     (HyScanObjectModel     *model,
                                                                         HyScanModelManager    *self);

static void       hyscan_model_manager_track_model_changed              (HyScanDBInfo          *model,
                                                                         HyScanModelManager    *self);

static void       hyscan_model_manager_acoustic_marks_loc_model_changed (HyScanMarkLocModel    *model,
                                                                         HyScanModelManager    *self);

static void       hyscan_model_manager_geo_mark_model_changed           (HyScanObjectModel     *model,
                                                                         HyScanModelManager    *self);

static void       hyscan_model_manager_label_model_changed              (HyScanObjectModel     *model,
                                                                         HyScanModelManager    *self);

static guint      hyscan_model_manager_signals[SIGNAL_MODEL_MANAGER_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanModelManager, hyscan_model_manager, G_TYPE_OBJECT)

void
hyscan_model_manager_class_init (HyScanModelManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  guint index;

  object_class->set_property = hyscan_model_manager_set_property;
  object_class->constructed  = hyscan_model_manager_constructed;
  object_class->finalize     = hyscan_model_manager_finalize;

  /* Название проекта. */
  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project_name", "Project_name", "Project name",
                         "",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* База данных. */
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Data base",
                         "The link to data base",
                         HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Кэш.*/
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache",
                         "The link to main cache with frequency used stafs",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Создание сигналов. */
  for (index = 0; index < SIGNAL_MODEL_MANAGER_LAST; index++)
    {
       hyscan_model_manager_signals[index] =
                    g_signal_new (signals[index],
                                  HYSCAN_TYPE_MODEL_MANAGER,
                                  G_SIGNAL_RUN_LAST,
                                  0, NULL, NULL,
                                  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);
    }
}

void
hyscan_model_manager_init (HyScanModelManager *self)
{
  self->priv = hyscan_model_manager_get_instance_private (self);
}
void
hyscan_model_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
      /* Название проекта */
      case PROP_PROJECT_NAME:
        {
          priv->project_name = g_value_dup_string (value);
        }
      break;
      /* База данных. */
      case PROP_DB:
        {
          priv->db  = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на базу данных. */
          g_object_ref (priv->db);
        }
      break;
      /* Кэш.*/
      case PROP_CACHE:
        {
          priv->cache  = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на кэш. */
          g_object_ref (priv->cache);
        }
      break;
      /* Что-то ещё... */
      default:
        {
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
      break;
    }
}
/* Конструктор. */
void
hyscan_model_manager_constructed (GObject *object)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;
  /*priv->grouping = BY_LABELS;*/
  /* Модель галсов. */
  priv->track_model = hyscan_db_info_new (priv->db);
  hyscan_db_info_set_project (priv->track_model, priv->project_name);
  g_signal_connect (priv->track_model,
                    "tracks-changed",
                    G_CALLBACK (hyscan_model_manager_track_model_changed),
                    self);
  /* Модель "водопадных" меток. */
  priv->acoustic_marks_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  hyscan_object_model_set_project (priv->acoustic_marks_model, priv->db, priv->project_name);
  g_signal_connect (priv->acoustic_marks_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_acoustic_marks_model_changed),
                    self);
  /* Модель данных "водопадных" меток с координатами. */
  priv->acoustic_loc_model = hyscan_mark_loc_model_new (priv->db, priv->cache);
  hyscan_mark_loc_model_set_project (priv->acoustic_loc_model, priv->project_name);
  g_signal_connect (priv->acoustic_loc_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_acoustic_marks_loc_model_changed),
                    self);
  /* Модель геометок. */
  priv->geo_mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_GEOMARK);
  hyscan_object_model_set_project (priv->geo_mark_model, priv->db, priv->project_name);
  g_signal_connect (priv->geo_mark_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_geo_mark_model_changed),
                    self);
  /* Модель данных групп. */
  priv->label_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_LABEL);
  hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);
  g_signal_connect (priv->label_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_label_model_changed),
                    self);
  g_print ("--- MODEL MANAGER ---\n");
  g_print ("Model Manager: %p\n", self);
  g_print ("Track Model: %p\n", priv->track_model);
  g_print ("Waterfall Mark Model: %p\n", priv->acoustic_marks_model);
  g_print ("Waterfall Mark Model with Locations: %p\n", priv->acoustic_loc_model);
  g_print ("Geo Mark Model: %p\n", priv->geo_mark_model);
  g_print ("Label Model: %p\n", priv->label_model);
  g_print ("--- MODEL MANAGER ---\n");
}
/* Деструктор. */
void
hyscan_model_manager_finalize (GObject *object)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */

  g_free (priv->project_name);
  priv->project_name = NULL;
  g_object_unref (priv->cache);
  priv->cache = NULL;
  g_object_unref (priv->db);
  priv->db = NULL;

  G_OBJECT_CLASS (hyscan_model_manager_parent_class)->finalize (object);
}

/* Обработчик сигнала изменения данных в модели галсов. */
void
hyscan_model_manager_track_model_changed (HyScanDBInfo       *model,
                                          HyScanModelManager *self)
{
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_TRACKS_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели акустических меток. */
void
hyscan_model_manager_acoustic_marks_model_changed (HyScanObjectModel  *model,
                                            HyScanModelManager *self)
{
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ACOUSTIC_MARKS_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели акустических меток с координатами. */
void
hyscan_model_manager_acoustic_marks_loc_model_changed (HyScanMarkLocModel *model,
                                                       HyScanModelManager    *self)
{
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ACOUSTIC_MARKS_LOC_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели гео-меток. */
void
hyscan_model_manager_geo_mark_model_changed (HyScanObjectModel  *model,
                                             HyScanModelManager *self)
{
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_GEO_MARKS_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели групп. */
void
hyscan_model_manager_label_model_changed (HyScanObjectModel  *model,
                                          HyScanModelManager *self)
{
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_LABELS_CHANGED], 0);
}

/**
 * hyscan_model_manager_new:
 * @project_name: название проекта
 * @db: указатель на базу данных
 * @cache: указатель на кэш
 *
 * Returns: cоздаёт новый объект #HyScanModelManager
 */
HyScanModelManager*
hyscan_model_manager_new (const gchar *project_name,
                          HyScanDB    *db,
                          HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_MODEL_MANAGER,
                       "project_name", project_name,
                       "db",           db,
                       "cache",        cache,
                       NULL);
}

/**
 * hyscan_model_manager_get_track_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель галсов. Когда модель больше не нужна,
 * необходимо использовать g_object_unref ().
 */
HyScanDBInfo*
hyscan_model_manager_get_track_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MODEL_MANAGER (self), NULL);

  priv = self->priv;
  return g_object_ref (priv->track_model);
}

/**
 * hyscan_model_manager_get_acoustic_mark_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель акустических меток. Когда модель больше не нужна,
 * необходимо использовать g_object_unref ().
 */
HyScanObjectModel*
hyscan_model_manager_get_acoustic_mark_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->acoustic_marks_model);
}

/**
 * hyscan_model_manager_get_geo_mark_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель гео-меток. Когда модель больше не нужна,
 * необходимо использовать g_object_unref ().
 */
HyScanObjectModel*
hyscan_model_manager_get_geo_mark_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->geo_mark_model);
}

/**
 * hyscan_model_manager_get_label_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель групп. Когда модель больше не нужна,
 * необходимо использовать g_object_unref ().
 */
HyScanObjectModel*
hyscan_model_manager_get_label_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->label_model);
}

/**
 * hyscan_model_manager_get_acoustic_mark_loc_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель "водопадных" меток с координатами. Когда модель больше не нужна,
 * необходимо использовать g_object_unref ().
 */
HyScanMarkLocModel*
hyscan_model_manager_get_acoustic_mark_loc_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->acoustic_loc_model);
}

/**
 * hyscan_model_manager_get_signal_title:
 * @self: указатель на Менеджер Моделей
 * signal: сигнал Менеджера Моделей
 *
 * Returns: название сигнала Менеджера Моделей
 */
const gchar*
hyscan_model_manager_get_signal_title (HyScanModelManager *self,
                                       ModelManagerSignal  signal)
{
  return signals[signal];
}

/**
 * hyscan-model-manager-get-db:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на Базу Данных. Когда БД больше не нужна,
 * необходимо использовать g_object_unref ().
 */
HyScanDB*
hyscan_model_manager_get_db (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->db);
}
/**
 * hyscan_model_manager_get_project_name:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: название проекта
 */
gchar*
hyscan_model_manager_get_project_name (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->project_name;
}
/**
 * hyscan-model-manager-get-cache:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на кэш. Когда кэш больше не нужен,
 * необходимо использовать g_object_unref ().
 */
HyScanCache*
hyscan_model_manager_get_cache (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->cache);
}

/**
 * hyscan_model_manager_get_all_labels:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на хэш-таблицу с данными о группах.
 * Когда хэш-таблица больше не нужна, необходимо
 * использовать g_hash_table_unref ().
 */
GHashTable*
hyscan_model_manager_get_all_labels (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_hash_table_ref (hyscan_object_model_get (priv->label_model));
}

/**
 * hyscan_model_manager_get_all_geo_marks:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на хэш-таблицу с данными о гео-метках.
 * Когда хэш-таблица больше не нужна, необходимо
 * использовать g_hash_table_unref ().
 */
GHashTable*
hyscan_model_manager_get_all_geo_marks (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_hash_table_ref (hyscan_object_model_get (priv->geo_mark_model));
}

/**
 * hyscan_model_manager_get_acousticmarks_loc:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на хэш-таблицу с данными о
 * "водопадных" метках с координатами.
 * Когда хэш-таблица больше не нужна, необходимо
 * использовать g_hash_table_unref ().
 */
GHashTable*
hyscan_model_manager_get_all_acoustic_marks_loc (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_hash_table_ref (hyscan_mark_loc_model_get (priv->acoustic_loc_model));
}

/**
 * hyscan_model_manager_get_all_tracks:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на хэш-таблицу с данными о галсах.
 * Когда хэш-таблица больше не нужна, необходимо
 * использовать g_hash_table_unref ().
 */
GHashTable*
hyscan_model_manager_get_all_tracks (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_hash_table_ref (hyscan_db_info_get_tracks (priv->track_model));
}

/**
 * hyscan_model_manager_set_grouping:
 * @self: указатель на Менеджер Моделей
 * @grouping: тип группировки
 *
 * Устанавливает тип группировки и отправляет сигнал об изменении типа группировки
 */
void
hyscan_model_manager_set_grouping (HyScanModelManager   *self,
                                   ModelManagerGrouping  grouping)
{
  HyScanModelManagerPrivate *priv = self->priv;

  priv->grouping = grouping;
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_GROUPING_CHANGED], 0);
}

/**
 * hyscan_model_manager_get_grouping:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: действительный тип группировки
 */
ModelManagerGrouping
hyscan_model_manager_get_grouping (HyScanModelManager   *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->grouping;
}

/**
 * hyscan_model_manager_set_expand_nodes_mode:
 * @self: указатель на Менеджер Моделей
 * @expand_nodes_mode: отображения всех узлов.
 *                     TRUE  - развернуть все узлы,
 *                     FALSE - свернуть все узлы
 *
 * Устанавливает режим отображения всех узлов.
 */
void
hyscan_model_manager_set_expand_nodes_mode (HyScanModelManager *self,
                                            gboolean            expand_nodes_mode)
{
  HyScanModelManagerPrivate *priv = self->priv;

  g_print ("expand_nodes_mode: %d\n", expand_nodes_mode);
  priv->expand_nodes_mode = expand_nodes_mode;
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_EXPAND_NODES_MODE_CHANGED], 0);
}

/**
 * hyscan_model_manager_get_expand_nodes_mode:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: действительный режим отображения всех узлов.
 *          TRUE  - все узлы развернуты,
 *          FALSE - все узлы свернуты
 */
gboolean
hyscan_model_manager_get_expand_nodes_mode   (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->expand_nodes_mode;
}
