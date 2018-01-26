/*
 * \file hyscan-db-profile.c
 *
 * \brief Исходный файл класса HyScanDBProfile, выполняющего список запросов в отдельном потоке.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <gio/gio.h>
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

static void   hyscan_db_profile_set_property   (GObject           *object,
                                                guint              prop_id,
                                                const GValue      *value,
                                                GParamSpec        *pspec);
static void   hyscan_db_profile_get_property   (GObject           *object,
                                                guint              prop_id,
                                                GValue            *value,
                                                GParamSpec        *pspec);
static void   hyscan_db_profile_finalize       (GObject           *object);
static void   hyscan_db_profile_clear          (HyScanDBProfile   *profile);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDBProfile, hyscan_db_profile, G_TYPE_OBJECT)

static void
hyscan_db_profile_class_init (HyScanDBProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_profile_set_property;
  object_class->get_property = hyscan_db_profile_get_property;
  object_class->finalize = hyscan_db_profile_finalize;

  g_object_class_install_property (object_class,
                                   PROP_URI,
                                   g_param_spec_string ("uri",
                                                        "URI",
                                                        "Database URI",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Database name",
                                                        NULL, 
                                                        G_PARAM_READWRITE));
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
hyscan_db_profile_finalize (GObject *object)
{
  hyscan_db_profile_clear (HYSCAN_DB_PROFILE (object));

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
hyscan_db_profile_new (void)
{
  return hyscan_db_profile_new_full (NULL, NULL);
}

/* Создаёт объект HyScanDBProfile. */
HyScanDBProfile *
hyscan_db_profile_new_full (const gchar *name,
                            const gchar *uri)
{
  return g_object_new (HYSCAN_TYPE_DB_PROFILE,
                       "name", name,
                       "uri", uri,
                       NULL);
}

/* Десериализация из INI-файла. */
gboolean
hyscan_db_profile_read_from_file (HyScanDBProfile *profile,
                                  const gchar     *filename)
{
  HyScanDBProfilePrivate *priv;
  GKeyFile *key_file;
  gboolean result = TRUE;

  g_return_val_if_fail (HYSCAN_IS_DB_PROFILE (profile), FALSE);

  priv = profile->priv;

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL))
    {
      g_warning ("HyScanDBProfile: there is a problem parsing the file.");
      result = FALSE;
      goto exit;
    }

  /* Очистка профиля. */
  hyscan_db_profile_clear (profile);

  /* Чтение строки с идентификатором системы хранения. */
  priv->uri = g_key_file_get_string (key_file, HYSCAN_DB_PROFILE_GROUP_NAME, HYSCAN_DB_PROFILE_URI_KEY, NULL);
  if (priv->uri == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "uri not found.");
      result = FALSE;
      goto exit;
    }

  /* Чтение строки с именем системы хранения. */
  priv->name = g_key_file_get_string (key_file, HYSCAN_DB_PROFILE_GROUP_NAME, HYSCAN_DB_PROFILE_NAME_KEY, NULL);
  if (priv->name == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "name not found.");
      result = FALSE;
      goto exit;
    }

exit:
  g_key_file_unref (key_file);
  return result;
}

/* Сериализация в INI-файл. */
gboolean
hyscan_db_profile_write_to_file (HyScanDBProfile *profile,
                                 const gchar     *filename)
{
  HyScanDBProfilePrivate *priv;
  GKeyFile *key_file;
  GError *gerror = NULL;
  gboolean result;

  g_return_val_if_fail (HYSCAN_IS_DB_PROFILE (profile), FALSE);

  priv = profile->priv;

  key_file = g_key_file_new ();

  /* URI не задан - на выход. */
  if (priv->uri == NULL)
    {
      result = FALSE;
      goto exit;
    }

  /* Задание значения для ключа идентификатора системы хранения. */
  g_key_file_set_string (key_file, HYSCAN_DB_PROFILE_GROUP_NAME, HYSCAN_DB_PROFILE_URI_KEY,
                         priv->uri);

  /* Задание значения для ключа имени системы хранения. */
  g_key_file_set_string (key_file, HYSCAN_DB_PROFILE_GROUP_NAME, HYSCAN_DB_PROFILE_NAME_KEY,
                         priv->name != NULL ? priv->name : HYSCAN_DB_PROFILE_UNNAMED_VALUE);

  /* Запись в файл. */
  if (!(result = g_key_file_save_to_file (key_file, filename, &gerror)))
    {
      g_warning ("HyScanDBProfile: couldn't write data to file. %s", gerror->message);
      g_error_free (gerror);
    }

exit:
  g_key_file_unref (key_file);
  return result;
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
