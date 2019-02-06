// /**
//  * \file hyscan-sensors-profile.c
//  *
//  * \brief Исходный файл класса HyScanSensorsProfile - профиля датчиков.
//  * \author Vladimir Maximov (vmakxs@gmail.com)
//  * \date 2018
//  * \license Проприетарная лицензия ООО "Экран"
//  *
//  */

// #include <gio/gio.h>
// #include "hyscan-sensors-profile.h"
// #include "hyscan-sensors-data.h"

// struct _HyScanSensorsProfilePrivate
// {
//   GHashTable *sensors;
// };

// static void      hyscan_sensors_profile_interface_init   (HyScanSerializableInterface  *iface);
// static void      hyscan_sensors_profile_constructed      (GObject                      *object);
// static void      hyscan_sensors_profile_finalize         (GObject                      *object);
// static gboolean  hyscan_sensors_profile_read             (HyScanSerializable           *serializable,
//                                                           const gchar                  *name);
// static gboolean  hyscan_sensors_profile_read_sensor_prm  (HyScanSensorsProfile         *profile,
//                                                           GKeyFile                     *key_file,
//                                                           const gchar                  *port);
// static gboolean  hyscan_sensors_profile_write            (HyScanSerializable           *serializable,
//                                                           const gchar                  *name);

// static gboolean  hyscan_read_uart_port_prm               (GKeyFile                     *key_file,
//                                                           const gchar                  *port,
//                                                           HyScanUARTPortPrm            *prm);
// static gboolean  hyscan_read_udp_port_prm                (GKeyFile                     *key_file,
//                                                           const gchar                  *port,
//                                                           HyScanUDPPortPrm             *prm);
// static gboolean  hyscan_read_virtual_port_prm            (GKeyFile                     *key_file,
//                                                           const gchar                  *port,
//                                                           HyScanVirtualPortPrm         *prm);

// G_DEFINE_TYPE_WITH_CODE (HyScanSensorsProfile, hyscan_sensors_profile, G_TYPE_OBJECT,
//                          G_ADD_PRIVATE (HyScanSensorsProfile)
//                          G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SERIALIZABLE, hyscan_sensors_profile_interface_init));

// static void
// hyscan_sensors_profile_interface_init (HyScanSerializableInterface *iface)
// {
//   iface->read = hyscan_sensors_profile_read;
//   iface->write = hyscan_sensors_profile_write;
// }

// static void
// hyscan_sensors_profile_class_init (HyScanSensorsProfileClass *klass)
// {
//   GObjectClass *object_class = G_OBJECT_CLASS (klass);
//   object_class->constructed = hyscan_sensors_profile_constructed;
//   object_class->finalize = hyscan_sensors_profile_finalize;
// }

// static void
// hyscan_sensors_profile_init (HyScanSensorsProfile *profile)
// {
//   profile->priv = hyscan_sensors_profile_get_instance_private (profile);
// }

// static void
// hyscan_sensors_profile_constructed (GObject *object)
// {
//   HyScanSensorsProfilePrivate *priv = HYSCAN_SENSORS_PROFILE (object)->priv;
//   priv->sensors = hyscan_sensors_data_new ();
// }

// static void
// hyscan_sensors_profile_finalize (GObject *object)
// {
//   g_hash_table_unref (HYSCAN_SENSORS_PROFILE (object)->priv->sensors);
//   G_OBJECT_CLASS (hyscan_sensors_profile_parent_class)->finalize (object);
// }

// /* Десериализация из INI-файла. */
// static gboolean
// hyscan_sensors_profile_read (HyScanSerializable *serializable,
//                              const gchar        *name)
// {
//   HyScanSensorsProfile *profile = HYSCAN_SENSORS_PROFILE (serializable);
//   HyScanSensorsProfilePrivate *priv = profile->priv;
//   GKeyFile *key_file;
//   gchar **ports;
//   gchar **groups = NULL;
//   gboolean result = TRUE;

//   key_file = g_key_file_new ();

//   if (!g_key_file_load_from_file (key_file, name, G_KEY_FILE_NONE, NULL))
//     {
//       g_warning ("HyScanSensorsProfile: there is a problem parsing the file.");
//       result = FALSE;
//       goto exit;
//     }

//   /* Очистка профиля. */
//   g_hash_table_remove_all (priv->sensors);

//   /* Получение списка групп. Имя группы суть имя порта. */
//   groups = g_key_file_get_groups (key_file, NULL);
//   if (groups == NULL)
//     goto exit;

//   /* Имя группы суть имя порта. */
//   ports = groups;

//   while (ports != NULL)
//     {
//       const gchar *port = *ports;
//       if (!hyscan_sensors_profile_read_sensor_prm (profile, key_file, port))
//         {
//           /* Очистка профиля, т.к. некоторые датчики могли быть записаны валидно. */
//           g_hash_table_remove_all (priv->sensors);
//           result = FALSE;
//           goto exit;
//         }
//       ports++;
//     }

// exit:
//   g_strfreev (groups);
//   g_key_file_unref (key_file);
//   return result;
// }

// static gboolean
// hyscan_sensors_profile_read_sensor_prm (HyScanSensorsProfile *profile,
//                                         GKeyFile             *key_file,
//                                         const gchar          *port)
// {
//   HyScanSensorsProfilePrivate *priv = profile->priv;
//   gchar *port_type = NULL;
//   gboolean result = TRUE;

//   port_type = g_key_file_get_string (key_file, port, "port-type", NULL);
//   if (port_type == NULL)
//     {
//       g_warning ("HyScanSensorsProfile: %s", "port type not found.");
//       result = FALSE;
//       goto exit;
//     }

//   if (g_str_equal (port_type, "virtual"))
//     {
//       HyScanVirtualPortPrm prm;
//       if (!hyscan_read_virtual_port_prm (key_file, port, &prm))
//         {
//           result = FALSE;
//           goto exit;
//         }
//       hyscan_sensors_data_set_virtual_sensor (priv->sensors, port, prm.channel, prm.time_offset);
//     }
//   else if (g_str_equal (port_type, "uart"))
//     {
//       HyScanUARTPortPrm prm;
//       if (!hyscan_read_uart_port_prm (key_file, port, &prm))
//         {
//           result = FALSE;
//           goto exit;
//         }
//       hyscan_sensors_data_set_uart_sensor (priv->sensors, port, prm.channel, prm.time_offset,
//                                            prm.protocol, prm.device, prm.mode, NULL, NULL);
//     }
//   else if (g_str_equal (port_type, "udp"))
//     {
//       HyScanUDPPortPrm prm;
//       if (!hyscan_read_udp_port_prm (key_file, port, &prm))
//         {
//           result = FALSE;
//           goto exit;
//         }
//       hyscan_sensors_data_set_udp_sensor (priv->sensors, port, prm.channel, prm.time_offset,
//                                           prm.protocol, prm.addr, prm.port, NULL);
//     }
//   else
//     {
//       g_warning ("HyScanSensorsProfile: unknown port type %s", port_type);
//       result = FALSE;
//       goto exit;
//     }

//   hyscan_sensors_data_set_state (priv->sensors, port, TRUE);

// exit:
//   g_free (port_type);
//   return TRUE;
// }

// /* Сериализация в INI-файл. */
// static gboolean
// hyscan_sensors_profile_write (HyScanSerializable *serializable,
//                               const gchar        *name)
// {
//   HyScanSensorsProfile *profile = HYSCAN_SENSORS_PROFILE (serializable);
//   HyScanSensorsProfilePrivate *priv = profile->priv;
//   GKeyFile *key_file;
//   GList *keys = NULL;
//   GList *ports;
//   GError *gerror = NULL;
//   gboolean result;

//   key_file = g_key_file_new ();

//   keys = g_hash_table_get_keys (priv->sensors);
//   if (keys == NULL)
//     return TRUE;

//   ports = keys;
//   while (ports != NULL)
//     {
//       const gchar *sensor;
//       HyScanSensorPrm *sensor_prm;

//       sensor = (const gchar *) ports->data;

//       sensor_prm = g_hash_table_lookup (priv->sensors, sensor);
//       if (sensor_prm == NULL)
//         {
//           g_warning ("hyscan_sensors_profile_write(): null value for key %s", sensor);
//           continue;
//         }

//       if (sensor_prm->port_type == HYSCAN_SENSOR_PORT_VIRTUAL)
//         {
//           HyScanVirtualPortPrm *prm = (HyScanVirtualPortPrm *) sensor_prm->port_prm;
//           g_key_file_set_string (key_file, ports->data, "port-type", "virtual");
//           g_key_file_set_int64 (key_file, ports->data, "time-offset", prm->time_offset);
//           g_key_file_set_integer (key_file, ports->data, "channel", prm->channel);
//         }
//       else if (sensor_prm->port_type == HYSCAN_SENSOR_PORT_UDP_IP)
//         {
//           HyScanUDPPortPrm *prm = (HyScanUDPPortPrm *) sensor_prm->port_prm;
//           g_key_file_set_string (key_file, ports->data, "port-type", "udp");
//           g_key_file_set_int64 (key_file, ports->data, "time-offset", prm->time_offset);
//           g_key_file_set_integer (key_file, ports->data, "channel", prm->channel);
//           g_key_file_set_integer (key_file, ports->data, "protocol", prm->protocol);
//           g_key_file_set_integer (key_file, ports->data, "addr", prm->addr);
//           g_key_file_set_integer (key_file, ports->data, "port", prm->port);
//         }
//       else if (sensor_prm->port_type == HYSCAN_SENSOR_PORT_UART)
//         {
//           HyScanUARTPortPrm *prm = (HyScanUARTPortPrm *) sensor_prm->port_prm;
//           g_key_file_set_string (key_file, ports->data, "port-type", "uart");
//           g_key_file_set_int64 (key_file, ports->data, "time-offset", prm->time_offset);
//           g_key_file_set_integer (key_file, ports->data, "channel", prm->channel);
//           g_key_file_set_integer (key_file, ports->data, "protocol", prm->protocol);
//           g_key_file_set_integer (key_file, ports->data, "device", prm->device);
//           g_key_file_set_integer (key_file, ports->data, "mode", prm->mode);
//         }

//       ports = g_list_next (ports);
//     }

//   /* Запись в файл. */
//   if (!(result = g_key_file_save_to_file (key_file, name, &gerror)))
//     {
//       g_warning ("HyScanSensorsProfile: couldn't write data to file. %s", gerror->message);
//       g_error_free (gerror);
//     }

//   g_list_free (keys);
//   g_key_file_unref (key_file);
//   return result;
// }

// static gboolean
// hyscan_read_uart_port_prm (GKeyFile          *key_file,
//                            const gchar       *port,
//                            HyScanUARTPortPrm *prm)
// {
//   GError *error = NULL;

//   prm->time_offset = g_key_file_get_int64 (key_file, port, "time-offset", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read uart port's time offset. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->channel = g_key_file_get_integer (key_file, port, "channel", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read uart port's channel. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->protocol = g_key_file_get_integer (key_file, port, "protocol", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read uart port's protocol. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->device = g_key_file_get_integer (key_file, port, "device", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read uart port's device. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->mode = g_key_file_get_integer (key_file, port, "mode", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read uart port's mode. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   return TRUE;
// }

// static gboolean
// hyscan_read_udp_port_prm (GKeyFile         *key_file,
//                           const gchar      *port,
//                           HyScanUDPPortPrm *prm)
// {
//   GError *error = NULL;

//   prm->time_offset = g_key_file_get_int64 (key_file, port, "time-offset", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read udp port's time offset. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->channel = g_key_file_get_integer (key_file, port, "channel", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read udp port's channel. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->protocol = g_key_file_get_integer (key_file, port, "protocol", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read udp port's protocol. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->addr = g_key_file_get_integer (key_file, port, "addr", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read udp port's address. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->port = g_key_file_get_integer (key_file, port, "port", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read udp port's port number. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   return TRUE;
// }

// static gboolean
// hyscan_read_virtual_port_prm (GKeyFile             *key_file,
//                               const gchar          *port,
//                               HyScanVirtualPortPrm *prm)
// {
//   GError *error = NULL;

//   prm->time_offset = g_key_file_get_int64 (key_file, port, "time-offset", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read virtual port's time offset. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   prm->channel = g_key_file_get_integer (key_file, port, "channel", &error);
//   if (error != NULL)
//     {
//       g_warning ("HyScanSensorsProfile: couldn't read virtual port's channel. %s", error->message);
//       g_error_free (error);
//       return FALSE;
//     }

//   return TRUE;
// }

// /* Создаёт объект HyScanSensorsProfile. */
// HyScanSensorsProfile *
// hyscan_sensors_profile_new (void)
// {
//   return g_object_new (HYSCAN_TYPE_SENSORS_PROFILE, NULL);
// }

// /* Получает список датчиков. */
// gchar **
// hyscan_sensors_profile_get_sensors (HyScanSensorsProfile *profile,
//                                     guint                *length)
// {
//   g_return_val_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile), NULL);
//   return (gchar **) g_hash_table_get_keys_as_array (profile->priv->sensors, length);
// }

// /* Получает тип порта датчика. */
// HyScanSensorPortType
// hyscan_sensors_profile_get_sensor_port_type (HyScanSensorsProfile *profile,
//                                              const gchar          *sensor)
// {
//   g_return_val_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile), HYSCAN_SENSOR_PORT_INVALID);
//   return hyscan_sensors_data_get_sensor_port_type (profile->priv->sensors, sensor);
// }

// /* Получает виртуальный датчик. */
// gboolean
// hyscan_sensors_profile_get_virtual_sensor (HyScanSensorsProfile *profile,
//                                            const gchar          *sensor,
//                                            guint                *channel,
//                                            gint64               *time_offset)
// {
//   g_return_val_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile), FALSE);
//   return hyscan_sensors_data_get_virtual_sensor (profile->priv->sensors, sensor, channel, time_offset);
// }

// /* Получает UART-датчик. */
// gboolean
// hyscan_sensors_profile_get_uart_sensor (HyScanSensorsProfile     *profile,
//                                         const gchar              *sensor,
//                                         guint                    *channel,
//                                         gint64                   *time_offset,
//                                         HyScanSensorProtocolType *protocol,
//                                         guint                    *device,
//                                         guint                    *mode)
// {
//   g_return_val_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile), FALSE);
//   return hyscan_sensors_data_get_uart_sensor (profile->priv->sensors, sensor, channel,
//                                               time_offset, protocol, device, mode, NULL, NULL);
// }

// /* Получает UDP-датчик. */
// gboolean
// hyscan_sensors_profile_get_udp_sensor (HyScanSensorsProfile     *profile,
//                                        const gchar              *sensor,
//                                        guint                    *channel,
//                                        gint64                   *time_offset,
//                                        HyScanSensorProtocolType *protocol,
//                                        guint                    *addr,
//                                        guint16                  *port)
// {
//   g_return_val_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile), FALSE);
//   return hyscan_sensors_data_get_udp_sensor (profile->priv->sensors, sensor, channel,
//                                              time_offset, protocol, addr, port, NULL);
// }

// /* Задаёт виртуальный датчик. */
// void
// hyscan_sensors_profile_set_virtual_sensor (HyScanSensorsProfile *profile,
//                                            const gchar          *sensor,
//                                            guint                 channel,
//                                            gint64                time_offset)
// {
//   g_return_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile));
//   hyscan_sensors_data_set_virtual_sensor (profile->priv->sensors, sensor, channel, time_offset);
// }

// /* Задаёт UART-датчик. */
// void
// hyscan_sensors_profile_set_uart_sensor (HyScanSensorsProfile     *profile,
//                                         const gchar              *sensor,
//                                         guint                     channel,
//                                         gint64                    time_offset,
//                                         HyScanSensorProtocolType  protocol,
//                                         guint                     device,
//                                         guint                     mode)
// {
//   g_return_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile));
//   hyscan_sensors_data_set_uart_sensor (profile->priv->sensors, sensor, channel, time_offset,
//                                        protocol, device, mode, NULL, NULL);
// }

// /* Задаёт UDP-датчик. */
// void
// hyscan_sensors_profile_set_udp_sensor (HyScanSensorsProfile     *profile,
//                                        const gchar              *sensor,
//                                        guint                     channel,
//                                        gint64                    time_offset,
//                                        HyScanSensorProtocolType  protocol,
//                                        guint                     addr,
//                                        guint16                   port)
// {
//   g_return_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile));
//   hyscan_sensors_data_set_udp_sensor (profile->priv->sensors, sensor, channel, time_offset, protocol, addr, port, NULL);
// }

// /* Удаляет датчик из профиля. */
// void
// hyscan_sensors_profile_remove_sensor (HyScanSensorsProfile *profile,
//                                       const gchar          *sensor)
// {
//   g_return_if_fail (HYSCAN_IS_SENSORS_PROFILE (profile));
//   g_hash_table_remove (profile->priv->sensors, sensor);
// }

