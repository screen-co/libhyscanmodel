/*
 * \file hyscan-db-profile.c
 *
 * \brief Исходный файл класса HyScanDBProfile - профиля БД.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-db-profile.h"

#define HYSCAN_DB_PROFILE_GROUP_NAME "db"
#define HYSCAN_DB_PROFILE_URI_KEY "uri"
#define HYSCAN_DB_PROFILE_NAME_KEY "name"
#define HYSCAN_DB_PROFILE_UNNAMED_VALUE "unnamed"

enum
{
  PROP_O,
  PROP_URI,
  PROP_NAME
};

struct _HyScanDBProfilePrivate
{
  gchar *name;   /* Имя БД. */
  gchar *uri;    /* Идентификатор БД.*/
};

static void   hyscan_db_profile_set_property    (GObject                      *object,
                                                 guint                         prop_id,
                                                 const GValue                 *value,
                                                 GParamSpec                   *pspec);
static void   hyscan_db_profile_get_property    (GObject                      *object,
                                                 guint                         prop_id,
                                                 GValue                       *value,
                                                 GParamSpec                   *pspec);
static void   hyscan_db_profile_object_finalize     (GObject                      *object);
static void   hyscan_db_profile_clear           (HyScanDBProfile              *profile);
static gboolean hyscan_db_profile_read          (HyScanProfile                *profile,
                                                 const gchar                  *name);

G_DEFINE_TYPE_WITH_CODE (HyScanDBProfile, hyscan_db_profile, HYSCAN_TYPE_PROFILE,
                         G_ADD_PRIVATE (HyScanDBProfile));

static void
hyscan_db_profile_class_init (HyScanDBProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->set_property = hyscan_db_profile_set_property;
  oclass->get_property = hyscan_db_profile_get_property;
  oclass->finalize = hyscan_db_profile_object_finalize;

  pklass->read = hyscan_db_profile_read;

  g_object_class_install_property (oclass, PROP_URI,
    g_param_spec_string ("uri", "URI", "Database URI",
                         NULL, G_PARAM_READWRITE));

  g_object_class_install_property (oclass, PROP_NAME,
    g_param_spec_string ("name", "Name", "Database name",
                         NULL, G_PARAM_READWRITE));
}

static void
hyscan_db_profile_init (HyScanDBProfile *profile)
{
  profile->priv = hyscan_db_profile_get_instance_private (profile);
}

static void
hyscan_db_profile_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanDBProfile *profile = HYSCAN_DB_PROFILE (object);

  switch (prop_id)
    {
    case PROP_URI:
      hyscan_db_profile_set_uri (profile, g_value_get_string (value));
      break;

    case PROP_NAME:
      hyscan_db_profile_set_name (profile, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_profile_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  HyScanDBProfile *profile = HYSCAN_DB_PROFILE (object);
  HyScanDBProfilePrivate *priv = profile->priv;

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_profile_object_finalize (GObject *object)
{
  HyScanDBProfile *self = HYSCAN_DB_PROFILE (object);

  hyscan_db_profile_clear (self);

  G_OBJECT_CLASS (hyscan_db_profile_parent_class)->finalize (object);
}

static void
hyscan_db_profile_clear (HyScanDBProfile *profile)
{
  HyScanDBProfilePrivate *priv = profile->priv;

  g_clear_pointer (&priv->uri, g_free);
  g_clear_pointer (&priv->name, g_free);
}

/* Создаёт объект HyScanDBProfile. */
HyScanDBProfile *
hyscan_db_profile_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_DB_PROFILE,
                       "file", file,
                       NULL);
}

/* Десериализация из INI-файла. */
static gboolean
hyscan_db_profile_read (HyScanProfile *profile,
                        const gchar   *file)
{
  HyScanDBProfile *self = HYSCAN_DB_PROFILE (profile);
  HyScanDBProfilePrivate *priv = self->priv;
  GKeyFile *kf;
  GError *error = NULL;
  gboolean status = FALSE;

  kf = g_key_file_new ();
  if (!g_key_file_load_from_file (kf, file, G_KEY_FILE_NONE, &error))
    {
      /* Если произошла ошибка при загрузке параметров не связанная с существованием файла,
         сигнализируем о ней. */
      if (error->code != G_FILE_ERROR_NOENT)
        {
          g_warning ("HyScanOffsetProfile: can't load file <%s>", file);
          goto exit;
        }
    }

  /* Очистка профиля. */
  hyscan_db_profile_clear (self);

  priv->name = g_key_file_get_string (kf, HYSCAN_DB_PROFILE_GROUP_NAME, HYSCAN_DB_PROFILE_NAME_KEY, NULL);
  priv->uri = g_key_file_get_string (kf, HYSCAN_DB_PROFILE_GROUP_NAME, HYSCAN_DB_PROFILE_URI_KEY, NULL);
  if (priv->uri == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "uri not found.");
      goto exit;
    }

  hyscan_profile_set_name (profile, priv->name != NULL ? priv->name : priv->uri);

  status = TRUE;

exit:
  g_key_file_unref (kf);
  return status;
}

/* Получает имя системы хранения. */
const gchar *
hyscan_db_profile_get_name (HyScanDBProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_DB_PROFILE (profile), NULL);

  return profile->priv->name;
}

/* Получает URI. */
const gchar *
hyscan_db_profile_get_uri (HyScanDBProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_DB_PROFILE (profile), NULL);

  return profile->priv->uri;
}

/* Задаёт имя системы хранения. */
void
hyscan_db_profile_set_name (HyScanDBProfile *profile,
                            const gchar     *name)
{
  g_return_if_fail (HYSCAN_IS_DB_PROFILE (profile));

  g_free (profile->priv->name);
  profile->priv->name = g_strdup (name);
}

/* Задаёт URI. */
void
hyscan_db_profile_set_uri (HyScanDBProfile *profile,
                           const gchar     *uri)
{
  g_return_if_fail (HYSCAN_IS_DB_PROFILE (profile));

  g_free (profile->priv->uri);
  profile->priv->uri = g_strdup (uri);
}

HyScanDB *
hyscan_db_profile_connect (HyScanDBProfile *profile)
{
  HyScanDBProfilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DB_PROFILE (profile), NULL);
  priv = profile->priv;

  if (priv->uri == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "uri not set");
      return NULL;
    }

  return hyscan_db_new (priv->uri);
}
