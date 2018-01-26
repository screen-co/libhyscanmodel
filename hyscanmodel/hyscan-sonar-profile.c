#include "hyscan-sonar-profile.h"

#define HYSCAN_SONAR_PROFILE_GROUP_NAME "sonar"
#define HYSCAN_SONAR_PROFILE_NAME_KEY "name"
#define HYSCAN_SONAR_PROFILE_DRV_PATH_KEY "driver-path"
#define HYSCAN_SONAR_PROFILE_DRV_NAME_KEY "driver-name"
#define HYSCAN_SONAR_PROFILE_URI_KEY "uri"
#define HYSCAN_SONAR_PROFILE_CFG_KEY "config"

enum
{
  PROP_O,
  PROP_NAME,
  PROP_DRIVER_PATH,
  PROP_DRIVER_NAME,
  PROP_URI,
  PROP_CONFIG
};

struct _HyScanSonarProfilePrivate
{
  gchar  *name;        /* Имя локатора. */
  gchar  *driver_path; /* Путь к драйверу локатора. */
  gchar  *driver_name; /* Имя драйвера локатора. */
  gchar  *uri;         /* Уникальный идентификатор локатора. */
  gchar  *config;      /* Конфигурация локатора. */
};

static void   hyscan_sonar_profile_set_property   (GObject              *object,
                                                   guint                 prop_id,
                                                   const GValue         *value,
                                                   GParamSpec           *pspec);
static void   hyscan_sonar_profile_get_property   (GObject              *object,
                                                   guint                 prop_id,
                                                   GValue               *value,
                                                   GParamSpec           *pspec);
static void   hyscan_sonar_profile_finalize       (GObject              *object);
static void   hyscan_sonar_profile_clear          (HyScanSonarProfile   *profile);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarProfile, hyscan_sonar_profile, G_TYPE_OBJECT)

static void
hyscan_sonar_profile_class_init (HyScanSonarProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_profile_set_property;
  object_class->get_property = hyscan_sonar_profile_get_property;
  object_class->finalize = hyscan_sonar_profile_finalize;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Sonar Name",
                                                        NULL,
                                                        G_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_DRIVER_PATH,
                                   g_param_spec_string ("driver-path",
                                                        "DriverPath",
                                                        "Driver path",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DRIVER_NAME,
                                   g_param_spec_string ("driver-name",
                                                        "DriverName",
                                                        "Driver name",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_URI,
                                   g_param_spec_string ("uri",
                                                        "URI",
                                                        "Sonar URI",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_CONFIG,
                                   g_param_spec_string ("config",
                                                        "Confif",
                                                        "Config",
                                                        NULL, 
                                                        G_PARAM_READWRITE));
}

static void
hyscan_sonar_profile_init (HyScanSonarProfile *profile)
{
  profile->priv = hyscan_sonar_profile_get_instance_private (profile);
}

static void
hyscan_sonar_profile_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanSonarProfile *profile = HYSCAN_SONAR_PROFILE (object);
  HyScanSonarProfilePrivate *priv = profile->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_DRIVER_PATH:
      priv->driver_path = g_value_dup_string (value);
      break;

    case PROP_DRIVER_NAME:
      priv->driver_name = g_value_dup_string (value);
      break;

    case PROP_URI:
      priv->uri = g_value_dup_string (value);
      break;

    case PROP_CONFIG:
      priv->config = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_profile_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  HyScanSonarProfile *profile = HYSCAN_SONAR_PROFILE (object);
  HyScanSonarProfilePrivate *priv = profile->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_DRIVER_PATH:
      g_value_set_string (value, priv->driver_path);
      break;

    case PROP_DRIVER_NAME:
      g_value_set_string (value, priv->driver_name);
      break;

    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;

    case PROP_CONFIG:
      g_value_set_string (value, priv->config);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_profile_finalize (GObject *object)
{
  hyscan_sonar_profile_clear (HYSCAN_SONAR_PROFILE (object));

  G_OBJECT_CLASS (hyscan_sonar_profile_parent_class)->finalize (object);
}

static void
hyscan_sonar_profile_clear (HyScanSonarProfile *profile)
{
  HyScanSonarProfilePrivate *priv = profile->priv;

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->driver_path, g_free);
  g_clear_pointer (&priv->driver_name, g_free);
  g_clear_pointer (&priv->uri, g_free);
  g_clear_pointer (&priv->config, g_free);
}

/* Создаёт объект HyScanSonarProfile. */
HyScanSonarProfile *
hyscan_sonar_profile_new (void)
{
  return hyscan_sonar_profile_new_full (NULL, NULL, NULL, NULL, NULL);
}

/* Создаёт объект HyScanSonarProfile. */
HyScanSonarProfile *
hyscan_sonar_profile_new_full (const gchar *name,
                               const gchar *driver_path,
                               const gchar *driver_name,
                               const gchar *uri,
                               const gchar *config)
{
  return g_object_new (HYSCAN_TYPE_SONAR_PROFILE,
                       "name", name,
                       "driver-path", driver_path,
                       "driver-name", driver_name,
                       "uri", uri,
                       "config", config,
                       NULL);
}

/* Десериализация из INI-файла. */
gboolean
hyscan_sonar_profile_read_from_file (HyScanSonarProfile *profile,
                                     const gchar        *filename)
{
  HyScanSonarProfilePrivate *priv;
  GKeyFile *key_file;
  gboolean result = TRUE;

  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), FALSE);

  priv = profile->priv;

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL))
    {
      g_warning ("HyScanSonarProfile: there is a problem parsing the file.");
      result = FALSE;
      goto exit;
    }

  hyscan_sonar_profile_clear (profile);

  /* Чтение строки с именем локатора. */
  priv->name = g_key_file_get_string (key_file,
                                      HYSCAN_SONAR_PROFILE_GROUP_NAME,
                                      HYSCAN_SONAR_PROFILE_NAME_KEY,
                                      NULL);
  if (priv->name == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "name not found.");
      result = FALSE;
      goto exit;
    }

  /* Чтение строки с путём к драйверу. */
  priv->driver_path = g_key_file_get_string (key_file,
                                             HYSCAN_SONAR_PROFILE_GROUP_NAME,
                                             HYSCAN_SONAR_PROFILE_DRV_PATH_KEY,
                                             NULL);
  if (priv->name == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "uri not found.");
      result = FALSE;
      goto exit;
    }

  /* Чтение строки с идентификатором системы хранения. */
  priv->driver_name = g_key_file_get_string (key_file,
                                             HYSCAN_SONAR_PROFILE_GROUP_NAME,
                                             HYSCAN_SONAR_PROFILE_DRV_NAME_KEY,
                                             NULL);
  if (priv->driver_name == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "driver name not found.");
      result = FALSE;
      goto exit;
    }

  /* Чтение строки с идентификатором системы хранения. */
  priv->uri = g_key_file_get_string (key_file,
                                     HYSCAN_SONAR_PROFILE_GROUP_NAME,
                                     HYSCAN_SONAR_PROFILE_URI_KEY,
                                     NULL);
  if (priv->uri == NULL)
    {
      g_warning ("HyScanDBProfile: %s", "uri not found.");
      result = FALSE;
      goto exit;
    }

  /* Чтение строки с идентификатором системы хранения. */
  priv->config = g_key_file_get_string (key_file,
                                        HYSCAN_SONAR_PROFILE_GROUP_NAME,
                                        HYSCAN_SONAR_PROFILE_CFG_KEY,
                                        NULL);

exit:
  g_key_file_unref (key_file);
  return result;
}

/* Сериализация в INI-файл. */
gboolean
hyscan_sonar_profile_write_to_file (HyScanSonarProfile *profile,
                                    const gchar        *filename)
{
  HyScanSonarProfilePrivate *priv;
  GKeyFile *key_file;
  GError *gerror = NULL;
  gboolean result;

  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), FALSE);

  priv = profile->priv;

  key_file = g_key_file_new ();

  /* uri, driver_path, driver_name не заданы - на выход! */
  if (priv->name == NULL || priv->driver_path == NULL || priv->driver_name == NULL || priv->uri == NULL)
    {
      result = FALSE;
      goto exit;
    }

  /* Задание значения для ключа пути к драйверу гидролокатора. */
  g_key_file_set_string (key_file,
                         HYSCAN_SONAR_PROFILE_GROUP_NAME,
                         HYSCAN_SONAR_PROFILE_NAME_KEY,
                         priv->name);

  /* Задание значения для ключа пути к драйверу гидролокатора. */
  g_key_file_set_string (key_file,
                         HYSCAN_SONAR_PROFILE_GROUP_NAME,
                         HYSCAN_SONAR_PROFILE_DRV_PATH_KEY,
                         priv->driver_path);

  /* Задание значения для ключа имени драйвера гидролокатора. */
  g_key_file_set_string (key_file,
                         HYSCAN_SONAR_PROFILE_GROUP_NAME,
                         HYSCAN_SONAR_PROFILE_DRV_NAME_KEY,
                         priv->driver_name);

  /* Задание значения для ключа идентификатора гидролокатора. */
  g_key_file_set_string (key_file,
                         HYSCAN_SONAR_PROFILE_GROUP_NAME,
                         HYSCAN_SONAR_PROFILE_URI_KEY,
                         priv->uri);


  /* Задание значения для ключа конфигурации. */
  if (priv->config != NULL)
    {
      g_key_file_set_string (key_file,
                             HYSCAN_SONAR_PROFILE_GROUP_NAME,
                             HYSCAN_SONAR_PROFILE_CFG_KEY,
                             priv->config);
    }

  /* Запись в файл. */
  if (!(result = g_key_file_save_to_file (key_file, filename, &gerror)))
    {
      g_warning ("HyScanSonarProfile: couldn't write data to file %s. Error: %s",
                 filename, gerror->message);
      g_error_free (gerror);
    }

exit:
  g_key_file_unref (key_file);
  return result;
}

/* Получает имя системы хранения. */
const gchar *
hyscan_sonar_profile_get_name (HyScanSonarProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), NULL);

  return profile->priv->name;
}

const gchar *
hyscan_sonar_profile_get_driver_path (HyScanSonarProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), NULL);

  return profile->priv->driver_path;
}

const gchar *
hyscan_sonar_profile_get_driver_name (HyScanSonarProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), NULL);

  return profile->priv->driver_name;
}

const gchar *
hyscan_sonar_profile_get_sonar_uri (HyScanSonarProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), NULL);

  return profile->priv->uri;
}

const gchar *
hyscan_sonar_profile_get_sonar_config (HyScanSonarProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_PROFILE (profile), NULL);

  return profile->priv->config;
}

/* Задаёт имя локатора. */
void
hyscan_sonar_profile_set_name (HyScanSonarProfile *profile,
                               const gchar        *name)
{
  g_return_if_fail (HYSCAN_IS_SONAR_PROFILE (profile));

  g_free (profile->priv->name);
  profile->priv->name = g_strdup (name);
}

void
hyscan_sonar_profile_set_driver_path (HyScanSonarProfile *profile,
                                      const gchar        *driver_path)
{
  g_return_if_fail (HYSCAN_IS_SONAR_PROFILE (profile));

  g_free (profile->priv->driver_path);
  profile->priv->driver_path = g_strdup (driver_path);
}

void
hyscan_sonar_profile_set_driver_name (HyScanSonarProfile *profile,
                                      const gchar        *driver_name)
{
  g_return_if_fail (HYSCAN_IS_SONAR_PROFILE (profile));

  g_free (profile->priv->driver_name);
  profile->priv->driver_name = g_strdup (driver_name);
}

void
hyscan_sonar_profile_set_sonar_uri (HyScanSonarProfile *profile,
                                    const gchar        *uri)
{
  g_return_if_fail (HYSCAN_IS_SONAR_PROFILE (profile));

  g_free (profile->priv->uri);
  profile->priv->uri = g_strdup (uri);
}

void
hyscan_sonar_profile_set_sonar_config (HyScanSonarProfile *profile,
                                       const gchar        *config)
{
  g_return_if_fail (HYSCAN_IS_SONAR_PROFILE (profile));

  g_free (profile->priv->config);
  profile->priv->config = g_strdup (config);
}
