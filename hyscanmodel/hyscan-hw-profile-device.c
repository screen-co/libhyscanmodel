#include "hyscan-hw-profile-device.h"
#include <hyscan-driver.h>

#define HYSCAN_HW_PROFILE_DEVICE_DRIVER "driver"
#define HYSCAN_HW_PROFILE_DEVICE_URI "uri"

struct _HyScanHWProfileDevicePrivate
{
  gchar           *group;

  gchar          **paths;

  gchar           *uri;
  HyScanDiscover  *discover;
  HyScanParamList *params;
};

static void    hyscan_hw_profile_device_object_constructed (GObject *object);
static void    hyscan_hw_profile_device_object_finalize    (GObject *object);

/* TODO: Change G_TYPE_OBJECT to type of the base class. */
G_DEFINE_TYPE_WITH_PRIVATE (HyScanHWProfileDevice, hyscan_hw_profile_device, G_TYPE_OBJECT);

static void
hyscan_hw_profile_device_class_init (HyScanHWProfileDeviceClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->constructed = hyscan_hw_profile_device_object_constructed;
  oclass->finalize = hyscan_hw_profile_device_object_finalize;

}

static void
hyscan_hw_profile_device_init (HyScanHWProfileDevice *hw_profile_device)
{
  hw_profile_device->priv = hyscan_hw_profile_device_get_instance_private (hw_profile_device);
}

static void
hyscan_hw_profile_device_object_constructed (GObject *object)
{
  // HyScanHWProfileDevice *hw_profile_device = HYSCAN_HW_PROFILE_DEVICE (object);
  // HyScanHWProfileDevicePrivate *priv = hw_profile_device->priv;

  G_OBJECT_CLASS (hyscan_hw_profile_device_parent_class)->constructed (object);
}

static void
hyscan_hw_profile_device_object_finalize (GObject *object)
{
  // HyScanHWProfileDevice *hw_profile_device = HYSCAN_HW_PROFILE_DEVICE (object);
  // HyScanHWProfileDevicePrivate *priv = hw_profile_device->priv;

  G_OBJECT_CLASS (hyscan_hw_profile_device_parent_class)->finalize (object);
}

HyScanHWProfileDevice *
hyscan_hw_profile_device_new (GKeyFile *keyfile)
{
  return g_object_new (HYSCAN_TYPE_HW_PROFILE_DEVICE, NULL);
}

static HyScanDiscover *
hyscan_hw_profile_device_find_driver (gchar       **paths,
                                      const gchar  *name)
{
  HyScanDriver * driver;

  if (paths == NULL)
    return NULL;

  /* Проходим по нуль-терминированному списку и возвращаем первый драйвер. */
  for (; *paths != NULL; ++paths)
    {
      driver = hyscan_driver_new (*paths, name);
      if (driver != NULL)
        return HYSCAN_DISCOVER (driver);
    }

  return NULL;
}

static HyScanParamList *
hyscan_hw_profile_device_read_params (GKeyFile         *kf,
                                      const gchar      *group,
                                      HyScanDataSchema *schema)
{
  gchar ** keys, ** iter;
  HyScanParamList * params;

  keys = g_key_file_get_keys (kf, group, NULL, NULL);
  if (keys == NULL)
    return NULL;

  params = hyscan_param_list_new ();

  for (iter = keys; *iter != NULL; ++iter)
    {
      HyScanDataSchemaKeyType type;

      if (g_str_equal (*iter, HYSCAN_HW_PROFILE_DEVICE_DRIVER) ||
          g_str_equal (*iter, HYSCAN_HW_PROFILE_DEVICE_URI))
        {
          continue;
        }

      type = hyscan_data_schema_key_get_value_type (schema, *iter);

      switch (type)
        {
        gboolean bool_val;
        gint64 int_val;
        gdouble dbl_val;
        gchar *str_val;

        case HYSCAN_DATA_SCHEMA_KEY_BOOLEAN:
          bool_val = g_key_file_get_boolean (kf, group, *iter, NULL);
          hyscan_param_list_set_boolean (params, *iter, bool_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_INTEGER:
          int_val = g_key_file_get_int64 (kf, group, *iter, NULL);
          hyscan_param_list_set_integer (params, *iter, int_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_ENUM:
          int_val = g_key_file_get_int64 (kf, group, *iter, NULL);
          hyscan_param_list_set_enum (params, *iter, int_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_DOUBLE:
          dbl_val = g_key_file_get_double (kf, group, *iter, NULL);
          hyscan_param_list_set_double (params, *iter, dbl_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_STRING:
          str_val = g_key_file_get_string (kf, group, *iter, NULL);
          hyscan_param_list_set_string (params, *iter, str_val);
          g_free (str_val);
          break;

        default:
          g_warning ("HyScanHWProfileDevice: invalid key type");
          break;
        }
    }

  g_strfreev (keys);

  return params;
}

void
hyscan_hw_profile_device_set_group (HyScanHWProfileDevice *hw_device,
                                    const gchar           *group)
{
  g_return_if_fail (HYSCAN_IS_HW_PROFILE_DEVICE (hw_device));

  g_clear_pointer (&hw_device->priv->group, g_free);
  hw_device->priv->group = g_strdup (group);
}

void
hyscan_hw_profile_device_set_paths (HyScanHWProfileDevice  *hw_device,
                                    gchar                 **paths)
{
  g_return_if_fail (HYSCAN_IS_HW_PROFILE_DEVICE (hw_device));

  g_clear_pointer (&hw_device->priv->paths, g_strfreev);
  hw_device->priv->paths = g_strdupv ((gchar**)paths);

}

void
hyscan_hw_profile_device_read (HyScanHWProfileDevice *hw_device,
                               GKeyFile              *kf)
{
  HyScanHWProfileDevicePrivate *priv;
  gchar *driver;
  HyScanDataSchema *schema;

  g_return_if_fail (HYSCAN_IS_HW_PROFILE_DEVICE (hw_device));
  priv = hw_device->priv;

  // TODO: add error handling.

  /* Определяем название драйвера и путь к устройству. */
  driver = g_key_file_get_string (kf, priv->group, HYSCAN_HW_PROFILE_DEVICE_DRIVER, NULL);
  priv->uri = g_key_file_get_string (kf, priv->group, HYSCAN_HW_PROFILE_DEVICE_URI, NULL);

  /* Считываем схему данных. */
  priv->discover = hyscan_hw_profile_device_find_driver (priv->paths, driver);
  schema = hyscan_discover_config (priv->discover, priv->uri);

  /* По этой схеме считываем ключи. */
  priv->params = hyscan_hw_profile_device_read_params (kf, priv->group, schema);

  g_object_unref (schema);
}

gboolean
hyscan_hw_profile_device_check (HyScanHWProfileDevice *hw_device)
{
  HyScanHWProfileDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_HW_PROFILE_DEVICE (hw_device), FALSE);
  priv = hw_device->priv;

  return hyscan_discover_check (priv->discover, priv->uri, priv->params);
}

HyScanDevice *
hyscan_hw_profile_device_connect (HyScanHWProfileDevice *hw_device)
{
  HyScanHWProfileDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_HW_PROFILE_DEVICE (hw_device), NULL);
  priv = hw_device->priv;

  return hyscan_discover_connect (priv->discover, priv->uri, priv->params);
}
