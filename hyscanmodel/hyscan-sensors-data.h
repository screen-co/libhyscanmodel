/**
 * \file hyscan-sensors-data.h
 *
 * \brief Заголовочный файл хранилища данных датчиков.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <glib.h>
#include <hyscan-api.h>
#include "hyscan-sonar-model.h"

/* Значение хэш-таблицы датчиков. */
typedef struct
{
  HyScanSensorPortType port_type; /* Тип порта. */
  gboolean             state;     /* Состояние датчика. */
  gpointer             port_prm;  /* Параметры порта, зависящие от типа порта. */
} HyScanSensorPrm;

/* Параметры виртуального порта. */
typedef struct
{
  guint  channel;
  gint64 time_offset;
} HyScanVirtualPortPrm;

/* Параметры uart порта. */
typedef struct
{
  guint                       channel;
  gint64                      time_offset;
  HyScanSensorProtocolType    protocol;
  guint                       device;
  guint                       mode;

  HyScanDataSchemaEnumValue **devices;     /* Устройства UART для порта. */
  HyScanDataSchemaEnumValue **modes;       /* Режимы UART для порта. */
} HyScanUARTPortPrm;

/* Параметры udp порта. */
typedef struct
{
  guint                       channel;
  gint64                      time_offset;
  HyScanSensorProtocolType    protocol;
  guint                       addr;
  guint16                     port;

  HyScanDataSchemaEnumValue **addresses;   /* IP-адреса для порта. */
} HyScanUDPPortPrm;

/* Создаёт хранилище данных датчиков. */
HYSCAN_API
GHashTable*           hyscan_sensors_data_new                   (void);

/* Получает тип порта датчика. */
HYSCAN_API
HyScanSensorPortType  hyscan_sensors_data_get_sensor_port_type  (GHashTable                *sensors,
                                                                 const gchar               *sensor);

/* Проверяет состояние датчика. */
HYSCAN_API
gboolean              hyscan_sensors_data_get_state             (GHashTable                *sensors,
                                                                 const gchar               *sensor);

/* Получает UART-датчик. */
HYSCAN_API
gboolean              hyscan_sensors_data_get_uart_sensor       (GHashTable                  *sensors,
                                                                 const gchar                 *sensor,
                                                                 guint                       *channel,
                                                                 gint64                      *time_offset,
                                                                 HyScanSensorProtocolType    *protocol,
                                                                 guint                       *device,
                                                                 guint                       *mode,
                                                                 HyScanDataSchemaEnumValue ***devices,
                                                                 HyScanDataSchemaEnumValue ***modes);

/* Получает UDP-датчик. */
HYSCAN_API
gboolean              hyscan_sensors_data_get_udp_sensor        (GHashTable                  *sensors,
                                                                 const gchar                 *sensor,
                                                                 guint                       *channel,
                                                                 gint64                      *time_offset,
                                                                 HyScanSensorProtocolType    *protocol,
                                                                 guint                       *addr,
                                                                 guint16                     *port,
                                                                 HyScanDataSchemaEnumValue ***addresses);

/* Получает виртуальный датчик. */
HYSCAN_API
gboolean              hyscan_sensors_data_get_virtual_sensor    (GHashTable                *sensors,
                                                                 const gchar               *sensor,
                                                                 guint                     *channel,
                                                                 gint64                    *time_offset);

/* Задаёт датчики для гидролокатора. */
HYSCAN_API
void                  hyscan_sensors_data_set_for_sonar        (GHashTable                 *sensors,
                                                                HyScanSonarModel           *sonar_model);

/* Задаёт состояние датчика. */
HYSCAN_API
void                  hyscan_sensors_data_set_state            (GHashTable                *sensors,
                                                                const gchar               *sensor,
                                                                gboolean                   enable);

/* Задаёт UART-датчик. */
HYSCAN_API
void                  hyscan_sensors_data_set_uart_sensor       (GHashTable                 *sensors,
                                                                 const gchar                *sensor,
                                                                 guint                       channel,
                                                                 gint64                      time_offset,
                                                                 HyScanSensorProtocolType    protocol,
                                                                 guint                       device,
                                                                 guint                       mode,
                                                                 HyScanDataSchemaEnumValue **devices,
                                                                 HyScanDataSchemaEnumValue **modes);

/* Задаёт UDP-датчик. */
HYSCAN_API
void                  hyscan_sensors_data_set_udp_sensor        (GHashTable                 *sensors,
                                                                 const gchar                *sensor,
                                                                 guint                       channel,
                                                                 gint64                      time_offset,
                                                                 HyScanSensorProtocolType    protocol,
                                                                 guint                       addr,
                                                                 guint16                     port,
                                                                 HyScanDataSchemaEnumValue **addresses);

/* Задаёт виртуальный датчик. */
HYSCAN_API
void                  hyscan_sensors_data_set_virtual_sensor    (GHashTable                *sensors,
                                                                 const gchar               *sensor,
                                                                 guint                      channel,
                                                                 gint64                     time_offset);
