/**
 * \file hyscan-connector.c
 *
 * \brief Класс асихронного подключения к БД и ГЛ.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-connector.h"
#include "hyscan-db-profile.h"
#include "hyscan-sonar-profile.h"
#include <hyscan-sonar-driver.h>
#include <hyscan-sonar-client.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_SONAR_CONTROL
};

struct _HyScanConnectorPrivate
{
  gchar              *db_profile;    /* Профиль БД. */
  gchar              *sonar_profile; /* Профиль гидролокатора. */

  HyScanDB           *db;            /* БД. */
  HyScanSonarControl *sonar_control; /* Интерфейс управления гидролокатором. */

  HyScanParam        *sonar;
};

static void      hyscan_connector_get_property       (GObject          *object,
                                                      guint             prop_id,
                                                      GValue           *value,
                                                      GParamSpec       *pspec);

static void      hyscan_connector_finalize           (GObject          *object);

static void      hyscan_connector_clear              (HyScanConnector  *connector);

static gboolean  hyscan_connector_connect_db         (gpointer          connector,
                                                      gpointer          data);

static gboolean  hyscan_connector_connect_sonar      (gpointer          connector,
                                                      gpointer          data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanConnector, hyscan_connector, HYSCAN_TYPE_ASYNC)

static void
hyscan_connector_class_init (HyScanConnectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = hyscan_connector_get_property;
  object_class->finalize = hyscan_connector_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DB,
                                   g_param_spec_string ("db",
                                                        "DB",
                                                        "DB",
                                                        NULL,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_SONAR_CONTROL,
                                   g_param_spec_string ("sonar-control",
                                                        "SonarControl",
                                                        "SonarControl",
                                                        NULL,
                                                        G_PARAM_READABLE));
}

static void
hyscan_connector_init (HyScanConnector *connector)
{
  connector->priv = hyscan_connector_get_instance_private (connector);
}

static void
hyscan_connector_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HyScanConnectorPrivate *priv = HYSCAN_CONNECTOR (object)->priv;

  switch (prop_id)
    {
    case PROP_DB:
      g_value_set_object (value, priv->db);
      break;

    case PROP_SONAR_CONTROL:
      g_value_set_object (value, priv->sonar_control);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_connector_finalize (GObject *object)
{
  HyScanConnector *connector = HYSCAN_CONNECTOR (object);

  hyscan_connector_clear (connector);

  G_OBJECT_CLASS (hyscan_connector_parent_class)->finalize (object);
}

static void
hyscan_connector_clear (HyScanConnector *connector)
{
  HyScanConnectorPrivate *priv = connector->priv;

  g_clear_pointer (&priv->db_profile, g_free);
  g_clear_pointer (&priv->sonar_profile, g_free);

  g_clear_object (&priv->db);
  g_clear_object (&priv->sonar);
  g_clear_object (&priv->sonar_control);
}

/* Подключает БД, указанную в профиле. */
static gboolean
hyscan_connector_connect_db (gpointer connector,
                             gpointer data)
{
  HyScanDBProfile *profile;
  HyScanConnectorPrivate *priv;

  priv = HYSCAN_CONNECTOR (connector)->priv;

  if (priv->db_profile == NULL)
    return TRUE;

  /* Профиль БД. */
  profile = hyscan_db_profile_new ();
  /* Десериализация. */
  if (hyscan_serializable_read (HYSCAN_SERIALIZABLE (profile), priv->db_profile))
    {
      priv->db = hyscan_db_new (hyscan_db_profile_get_uri (profile));
    }

  g_clear_object (&profile);

  return priv->db != NULL;
}

/* Подключает гидролокатор, указанный в профиле. */
static gboolean
hyscan_connector_connect_sonar (gpointer connector,
                                gpointer data)
{
  HyScanConnectorPrivate *priv;
  HyScanSonarProfile *sonar_profile;
  const gchar * driver_path = NULL;
  const gchar * driver_name = NULL;
  const gchar * sonar_uri = NULL;
  const gchar * sonar_config = NULL;

  priv = HYSCAN_CONNECTOR (connector)->priv;

  if (priv->sonar_profile == NULL)
    return TRUE;

  if (priv->db == NULL)
    {
      g_message ("HyScanConnector: db is null.");
      return FALSE;
    }

  /* Профиль гидролокатора. */
  sonar_profile = hyscan_sonar_profile_new ();
  /* Десериализация. */
  if (!hyscan_serializable_read (HYSCAN_SERIALIZABLE (sonar_profile), priv->sonar_profile))
    goto exit;

  driver_path = hyscan_sonar_profile_get_driver_path (sonar_profile);
  driver_name = hyscan_sonar_profile_get_driver_name (sonar_profile);
  sonar_uri = hyscan_sonar_profile_get_sonar_uri (sonar_profile);
  sonar_config = hyscan_sonar_profile_get_sonar_config (sonar_profile);

  /* Загрузка драйвера гидролокатора. */
  if (driver_path == NULL && driver_name == NULL)
    {
      HyScanSonarClient *client = hyscan_sonar_client_new (sonar_uri);

      if (client == NULL)
        {
          g_message ("can't connect to sonar '%s'", sonar_uri);
          goto exit;
        }
      if (!hyscan_sonar_client_set_master (client))
        {
          g_message ("can't setup master connection to '%s'", sonar_uri);
          goto exit;
        }

      priv->sonar = HYSCAN_PARAM (client);
    }
  else
    {
      HyScanSonarDriver *driver = hyscan_sonar_driver_new (driver_path, driver_name);
      if (driver == NULL)
        {
          g_message ("HyScanConnector: couldn't load sonar driver %s",
                     hyscan_sonar_profile_get_driver_name (sonar_profile));
          goto exit;
        }

      /* Соединение с гидролокатором. */
      priv->sonar = hyscan_sonar_discover_connect (HYSCAN_SONAR_DISCOVER (driver),
                                                   sonar_uri, sonar_config);
      if (priv->sonar == NULL)
        {
          g_message ("HyScanConnector: couldn't connect sonar '%s'.",
                     hyscan_sonar_profile_get_sonar_uri (sonar_profile));
          goto exit;
        }
    }

  /* Создание интерфейса управления гидролокатором. */
  priv->sonar_control = hyscan_sonar_control_new (priv->sonar, 4, 4, priv->db);

exit:
  g_clear_object (&sonar_profile);

  return priv->sonar_control != NULL;
}

/* Создаёт HyScanConnector. */
HyScanConnector *
hyscan_connector_new ()
{
  return g_object_new (HYSCAN_TYPE_CONNECTOR, NULL);
}

/* Запускает асинхронное создание. */
gboolean
hyscan_connector_execute (HyScanConnector *connector,
                          const gchar     *db_profile,
                          const gchar     *sonar_profile)
{
  HyScanConnectorPrivate *priv;
  HyScanAsync *async;

  g_return_val_if_fail (HYSCAN_IS_CONNECTOR (connector), FALSE);

  if (db_profile == NULL) {
    g_warning ("HyScanConnector: db profile is null.");
    return FALSE;
  }

  priv = connector->priv;
  async = HYSCAN_ASYNC (connector);

  /* Подготовка к подключению - удаление текущих объектов. */
  hyscan_connector_clear (connector);

  /* Имена файлов профилей дублируются для дальнейшего использования в
   * в функциях, осуществляющих подключение к БД и ГЛ. */
  priv->db_profile = g_strdup (db_profile);
  priv->sonar_profile = g_strdup (sonar_profile);

  /* Порядок выполнения имеет значение.
   * Сперва создаётся HyScanDB, затем - HyScanSonarControl, для создания которого требуется
   * объект HyScanDB. */
  hyscan_async_append_query (async, hyscan_connector_connect_db, connector, NULL, 0);
  hyscan_async_append_query (async, hyscan_connector_connect_sonar, connector, NULL, 0);

  return hyscan_async_execute (async);
}

/* Получает HyScanDB. */
HyScanDB *
hyscan_connector_get_db (HyScanConnector *connector)
{
  g_return_val_if_fail (HYSCAN_IS_CONNECTOR (connector), NULL);

  return connector->priv->db != NULL ? g_object_ref (connector->priv->db) : NULL;
}

/* Получает HyScanSonarControl. */
HyScanSonarControl *
hyscan_connector_get_sonar_control (HyScanConnector *connector)
{
  g_return_val_if_fail (HYSCAN_IS_CONNECTOR (connector), NULL);

  return connector->priv->sonar_control != NULL ? g_object_ref (connector->priv->sonar_control) : NULL;
}
