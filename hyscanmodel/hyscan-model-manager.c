/*
 * hyscan-model-manager.c
 *
 *  Created on: 12 фев. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include "hyscan-model-manager.h"

enum
{
  PROP_PROJECT_NAME = 1,    /* Название проекта. */
  PROP_DB,                  /* База данных. */
  PROP_CACHE,               /* Кэш.*/
  N_PROPERTIES
};

struct _HyScanModelManagerPrivate
{
  HyScanObjectModel  *wf_mark_model,     /* Модель данных "водопадных" меток. */
                     *geo_mark_model;    /* Модель данных гео-меток. */
  HyScanMarkLocModel *wf_mark_loc_model; /* Модель данных "водопадных" меток с координатами. */
  HyScanDBInfo       *track_model;       /* Модель данных галсов. */
  HyScanCache        *cache;             /* Кэш.*/
  HyScanDB           *db;                /* База данных. */
  gchar              *project_name;      /* Название проекта. */
};

static void       hyscan_model_manager_set_property      (GObject               *object,
                                                          guint                  prop_id,
                                                          const GValue          *value,
                                                          GParamSpec            *pspec);
static void       hyscan_model_manager_constructed       (GObject               *object);
static void       hyscan_model_manager_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanModelManager, hyscan_model_manager, G_TYPE_OBJECT)

static void
hyscan_model_manager_class_init (HyScanModelManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
}

static void
hyscan_model_manager_init (HyScanModelManager *self)
{
  self->priv = hyscan_model_manager_get_instance_private (self);
}
static void
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
static void
hyscan_model_manager_constructed (GObject *object)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;
  /* Модель галсов. */
  priv->track_model = hyscan_db_info_new (priv->db);
  /* Модель "водопадных" меток. */
  priv->wf_mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  /* Модель данных "водопадных" меток с координатами. */
  priv->wf_mark_loc_model = hyscan_mark_loc_model_new (priv->db, priv->cache);
  /* Модель геометок. */
  priv->geo_mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_GEOMARK);

}
/* Деструктор. */
static void
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

HyScanModelManager*
hyscan_model_manager_new (gchar       *project_name,
                          HyScanDB    *db,
                          HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_MODEL_MANAGER,
                       "project_name", project_name,
                       "db",           db,
                       "cache",        cache,
                       NULL);
}

HyScanDBInfo*
hyscan_model_manager_get_track_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->track_model);
}

HyScanObjectModel*
hyscan_model_manager_get_wf_mark_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->wf_mark_model);
}

HyScanObjectModel*
hyscan_model_manager_get_geo_mark_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->geo_mark_model);
}

HyScanMarkLocModel*
hyscan_model_manager_get_wf_loc_marks_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->wf_mark_loc_model);
}
