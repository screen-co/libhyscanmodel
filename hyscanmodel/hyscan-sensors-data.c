/**
 * \file hyscan-sensors-data.c
 *
 * \brief Исходный файл хранилища данных датчиков.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include "hyscan-sensors-data.h"

static void
hyscan_sensor_prm_free (gpointer data)
{
  HyScanSensorPrm *sensor_prm;

  if (data == NULL)
    return;

  sensor_prm = (HyScanSensorPrm *) data;
  if (sensor_prm->port_prm != NULL)
    {
      if (sensor_prm->port_type == HYSCAN_SENSOR_PORT_UART)
        {
          hyscan_data_schema_free_enum_values (((HyScanUARTPortPrm *) sensor_prm->port_prm)->devices);
          hyscan_data_schema_free_enum_values (((HyScanUARTPortPrm *) sensor_prm->port_prm)->modes);
        }
      else if (sensor_prm->port_type == HYSCAN_SENSOR_PORT_UDP_IP)
        {
          hyscan_data_schema_free_enum_values (((HyScanUDPPortPrm *) sensor_prm->port_prm)->addresses);
        }
      g_free (sensor_prm->port_prm);
    }
  g_free (sensor_prm);
}

GHashTable * 
hyscan_sensors_data_new (void)
{
    return g_hash_table_new_full (g_str_hash, g_str_equal, NULL, hyscan_sensor_prm_free);
}

/* Получает тип порта датчика. */
HyScanSensorPortType
hyscan_sensors_data_get_sensor_port_type (GHashTable  *sensors,
                                          const gchar *sensor)
{
  HyScanSensorPrm *prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_get_sensor_port_type(): sensors is null");
      return HYSCAN_SENSOR_PORT_INVALID;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_get_sensor_port_type(): sensor is null");
      return HYSCAN_SENSOR_PORT_INVALID;
    }
  
  prm = (HyScanSensorPrm *) g_hash_table_lookup (sensors, sensor);
  if (prm == NULL)
    {
      g_warning ("hyscan_sensors_data_get_sensor_port_type(): sensor %s not found.", sensor);
      return HYSCAN_SENSOR_PORT_INVALID;
    }

  return prm->port_type;
}

/* Проверяет состояние датчика. */
gboolean
hyscan_sensors_data_get_state (GHashTable  *sensors,
                               const gchar *sensor)
{
  HyScanSensorPrm *prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_get_state(): sensors is null");
      return FALSE;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_get_state(): sensor is null.");
      return FALSE;
    }
  
  prm = (HyScanSensorPrm *) g_hash_table_lookup (sensors, sensor);
  if (prm == NULL)
    {
      g_warning ("hyscan_sensors_data_get_state(): sensor %s not found.", sensor);
      return FALSE;
    }

  return prm->state;
}

/* Получает UART-датчик. */
gboolean
hyscan_sensors_data_get_uart_sensor (GHashTable                  *sensors,
                                     const gchar                 *sensor,
                                     guint                       *channel,
                                     gint64                      *time_offset,
                                     HyScanSensorProtocolType    *protocol,
                                     guint                       *device,
                                     guint                       *mode,
                                     HyScanDataSchemaEnumValue ***devices,
                                     HyScanDataSchemaEnumValue ***modes)
{
  HyScanSensorPrm *prm;
  HyScanUARTPortPrm *port_prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_get_uart_sensor(): sensors is null");
      return FALSE;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_get_uart_sensor(): sensor is null.");
      return FALSE;
    }
  
  prm = (HyScanSensorPrm *) g_hash_table_lookup (sensors, sensor);
  if (prm == NULL)
    {
      g_warning ("hyscan_sensors_data_get_uart_sensor(): sensor %s not found.", sensor);
      return FALSE;
    }

  if (prm->port_type != HYSCAN_SENSOR_PORT_UART)
    {
      g_warning ("hyscan_sensors_data_get_uart_sensor(): ports types mismatch.");
      return FALSE;
    }

  port_prm = (HyScanUARTPortPrm *) prm->port_prm;
  if (channel != NULL)
    *channel = port_prm->channel;
  if (time_offset != NULL)
    *time_offset = port_prm->time_offset;
  if (protocol != NULL)
    *protocol = port_prm->protocol;
  if (device != NULL)
    *device = port_prm->device;
  if (mode != NULL)
    *mode = port_prm->mode;
  if (devices != NULL)
    *devices = port_prm->devices;
  if (modes != NULL)
    *modes = port_prm->modes;

  return TRUE;
}

/* Получает UDP-датчик. */
gboolean
hyscan_sensors_data_get_udp_sensor (GHashTable                  *sensors,
                                    const gchar                 *sensor,
                                    guint                       *channel,
                                    gint64                      *time_offset,
                                    HyScanSensorProtocolType    *protocol,
                                    guint                       *addr,
                                    guint16                     *port,
                                    HyScanDataSchemaEnumValue ***addresses)
{
  HyScanSensorPrm *prm;
  HyScanUDPPortPrm *port_prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_get_udp_sensor(): sensors is null");
      return FALSE;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_get_udp_sensor(): sensor is null.");
      return FALSE;
    }

  prm = (HyScanSensorPrm *) g_hash_table_lookup (sensors, sensor);
  if (prm == NULL)
    {
      g_warning ("hyscan_sensors_data_get_udp_sensor(): sensor %s not found.", sensor);
      return FALSE;
    }

  if (prm->port_type != HYSCAN_SENSOR_PORT_UDP_IP)
    {
      g_warning ("hyscan_sensors_data_get_udp_sensor(): ports types mismatch.");
      return FALSE;
    }

  port_prm = (HyScanUDPPortPrm *) prm->port_prm;
  if (channel != NULL)
    *channel = port_prm->channel;
  if (time_offset != NULL)
    *time_offset = port_prm->time_offset;
  if (protocol != NULL)
    *protocol = port_prm->protocol;
  if (addr != NULL)
    *addr = port_prm->addr;
  if (port != NULL)
    *port = port_prm->port;
  if (addresses != NULL)
    *addresses = port_prm->addresses;

  return TRUE;
}

/* Получает виртуальный датчик. */
gboolean
hyscan_sensors_data_get_virtual_sensor (GHashTable  *sensors,
                                        const gchar *sensor,
                                        guint       *channel,
                                        gint64      *time_offset)
{
  HyScanSensorPrm *prm;
  HyScanVirtualPortPrm *port_prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_get_virtual_sensor(): sensors is null");
      return FALSE;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_get_virtual_sensor(): sensor is null.");
      return FALSE;
    }
  
  prm = (HyScanSensorPrm *) g_hash_table_lookup (sensors, sensor);
  if (prm == NULL)
    {
      g_warning ("hyscan_sensors_data_get_virtual_sensor(): sensor %s not found.", sensor);
      return FALSE;
    }

  if (prm->port_type != HYSCAN_SENSOR_PORT_VIRTUAL)
    {
      g_warning ("hyscan_sensors_data_get_virtual_sensor(): ports types mismatch.");
      return FALSE;
    }

  port_prm = (HyScanVirtualPortPrm *) prm->port_prm;
  if (channel != NULL)
    *channel = port_prm->channel;
  if (time_offset != NULL)
    *time_offset = port_prm->time_offset;

  return TRUE;
}

/* Задаёт состояние датчика. */
void
hyscan_sensors_data_set_state (GHashTable  *sensors,
                               const gchar *sensor,
                               gboolean     enable)
{
  HyScanSensorPrm *prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_set_state(): sensors is null");
      return;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_set_state(): sensor is null.");
      return;
    }
  
  prm = (HyScanSensorPrm *) g_hash_table_lookup (sensors, sensor);
  if (prm == NULL)
    {
      g_warning ("hyscan_sensors_data_set_state(): sensor %s not found.", sensor);
      return;
    }

  prm->state = enable;
}

/* Задаёт UART-датчик. */
void
hyscan_sensors_data_set_uart_sensor (GHashTable                 *sensors,
                                     const gchar                *sensor,
                                     guint                       channel,
                                     gint64                      time_offset,
                                     HyScanSensorProtocolType    protocol,
                                     guint                       device,
                                     guint                       mode,
                                     HyScanDataSchemaEnumValue **devices,
                                     HyScanDataSchemaEnumValue **modes)
{
  HyScanSensorPrm *prm;
  HyScanUARTPortPrm *port_prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_set_uart_sensor(): sensors is null");
      return;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_set_uart_sensor(): sensor is null.");
      return;
    }
  
  if (g_hash_table_contains (sensors, sensor))
    g_hash_table_remove(sensors, sensor);

  port_prm = g_new0 (HyScanUARTPortPrm, 1);
  port_prm->channel = channel;
  port_prm->time_offset = time_offset;
  port_prm->protocol = protocol;
  port_prm->device = device;
  port_prm->mode = mode;
  port_prm->devices = devices;
  port_prm->modes = modes;

  prm = g_new0 (HyScanSensorPrm, 1);
  prm->port_type = HYSCAN_SENSOR_PORT_UART;
  prm->port_prm = port_prm;

  g_hash_table_insert (sensors, g_strdup (sensor), prm);
}

/* Задаёт UDP-датчик. */
void
hyscan_sensors_data_set_udp_sensor (GHashTable                 *sensors,
                                    const gchar                *sensor,
                                    guint                       channel,
                                    gint64                      time_offset,
                                    HyScanSensorProtocolType    protocol,
                                    guint                       addr,
                                    guint16                     port,
                                    HyScanDataSchemaEnumValue **addresses)
{
  HyScanSensorPrm *prm;
  HyScanUDPPortPrm *port_prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_set_udp_sensor(): sensors is null");
      return;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_set_udp_sensor(): sensor is null.");
      return;
    }
  
  if (g_hash_table_contains (sensors, sensor))
    g_hash_table_remove(sensors, sensor);

  port_prm = g_new0 (HyScanUDPPortPrm, 1);
  port_prm->channel = channel;
  port_prm->time_offset = time_offset;
  port_prm->protocol = protocol;
  port_prm->addr = addr;
  port_prm->port = port;
  port_prm->addresses = addresses;

  prm = g_new0 (HyScanSensorPrm, 1);
  prm->port_type = HYSCAN_SENSOR_PORT_UDP_IP;
  prm->port_prm = port_prm;

  g_hash_table_insert (sensors, g_strdup (sensor), prm);
}

/* Задаёт виртуальный датчик. */
void
hyscan_sensors_data_set_virtual_sensor (GHashTable  *sensors,
                                        const gchar *sensor,
                                        guint        channel,
                                        gint64       time_offset)
{
  HyScanSensorPrm *prm;
  HyScanVirtualPortPrm *port_prm;

  if (sensors == NULL)
    {
      g_warning ("hyscan_sensors_data_set_virtual_sensor(): sensors is null");
      return;
    }

  if (sensor == NULL)
    {
      g_warning ("hyscan_sensors_data_set_virtual_sensor(): sensor is null.");
      return;
    }
  
  if (g_hash_table_contains (sensors, sensor))
    g_hash_table_remove(sensors, sensor);

  port_prm = g_new0 (HyScanVirtualPortPrm, 1);
  port_prm->channel = channel;
  port_prm->time_offset = time_offset;

  prm = g_new0 (HyScanSensorPrm, 1);
  prm->port_type = HYSCAN_SENSOR_PORT_VIRTUAL;
  prm->port_prm = port_prm;

  g_hash_table_insert (sensors, g_strdup (sensor), prm);
}
