/**
 * \file hyscan-sensors-profile.h
 *
 * \brief Заголовочный файл класса HyScanSensorsProfile - профиля датчиков.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSensorsProfile HyScanSensorsProfile - профиль датчиков.
 *
 */

#ifndef __HYSCAN_SENSORS_PROFILE_H__
#define __HYSCAN_SENSORS_PROFILE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-serializable.h>
#include <hyscan-sensor-control.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SENSORS_PROFILE            \
        (hyscan_sensors_profile_get_type ())

#define HYSCAN_SENSORS_PROFILE(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SENSORS_PROFILE, HyScanSensorsProfile))

#define HYSCAN_IS_SENSORS_PROFILE(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SENSORS_PROFILE))

#define HYSCAN_SENSORS_PROFILE_CLASS(klass)    \
        (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SENSORS_PROFILE, HyScanSensorsProfileClass))

#define HYSCAN_IS_SENSORS_PROFILE_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SENSORS_PROFILE))

#define HYSCAN_SENSORS_PROFILE_GET_CLASS(obj)  \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SENSORS_PROFILE, HyScanSensorsProfileClass))

typedef struct _HyScanSensorsProfile HyScanSensorsProfile;
typedef struct _HyScanSensorsProfilePrivate HyScanSensorsProfilePrivate;
typedef struct _HyScanSensorsProfileClass HyScanSensorsProfileClass;

struct _HyScanSensorsProfile
{
  GObject                      parent_instance;
  HyScanSensorsProfilePrivate *priv;
};

struct _HyScanSensorsProfileClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_sensors_profile_get_type              (void);

/* Создаёт объект HyScanSensorsProfile. */
HYSCAN_API
HyScanSensorsProfile*  hyscan_sensors_profile_new                   (void);

/* Получает список датчиков. */
HYSCAN_API
gchar**                hyscan_sensors_profile_get_sensors           (HyScanSensorsProfile      *profile,
                                                                     guint                     *length);

/* Получает тип порта датчика. */
HYSCAN_API
HyScanSensorPortType   hyscan_sensors_profile_get_sensor_port_type  (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor);

/* Получает виртуальный датчик. */
HYSCAN_API
gboolean               hyscan_sensors_profile_get_virtual_sensor    (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor,
                                                                     guint                     *channel,
                                                                     gint64                    *time_offset);

/* Получает UART-датчик. */
HYSCAN_API
gboolean               hyscan_sensors_profile_get_uart_sensor       (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor,
                                                                     guint                     *channel,
                                                                     gint64                    *time_offset,
                                                                     HyScanSensorProtocolType  *protocol,
                                                                     guint                     *device,
                                                                     guint                     *mode);

/* Получает UDP-датчик. */
HYSCAN_API
gboolean               hyscan_sensors_profile_get_udp_sensor        (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor,
                                                                     guint                     *channel,
                                                                     gint64                    *time_offset,
                                                                     HyScanSensorProtocolType  *protocol,
                                                                     guint                     *addr,
                                                                     guint16                   *port);

/* Задаёт виртуальный датчик. */
HYSCAN_API
void                   hyscan_sensors_profile_set_virtual_sensor    (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor,
                                                                     guint                      channel,
                                                                     gint64                     time_offset);

/* Задаёт UART-датчик. */
HYSCAN_API
void                   hyscan_sensors_profile_set_uart_sensor       (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor,
                                                                     guint                      channel,
                                                                     gint64                     time_offset,
                                                                     HyScanSensorProtocolType   protocol,
                                                                     guint                      device,
                                                                     guint                      mode);

/* Задаёт UDP-датчик. */
HYSCAN_API
void                   hyscan_sensors_profile_set_udp_sensor        (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor,
                                                                     guint                      channel,
                                                                     gint64                     time_offset,
                                                                     HyScanSensorProtocolType   protocol,
                                                                     guint                      addr,
                                                                     guint16                    port);

/* Удаляет датчик из профиля. */
HYSCAN_API
void                   hyscan_sensors_profile_remove_sensor         (HyScanSensorsProfile      *profile,
                                                                     const gchar               *sensor);

G_END_DECLS

#endif /* __HYSCAN_SENSORS_PROFILE_H__ */
