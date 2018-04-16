/*
 * \file hyscan-sonar-model-test.c
 * \brief Тест класса HyScanSonarModel.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-sonar-model.h"
#include "hyscan-sensor-control-server.h"
#include "hyscan-generator-control-server.h"
#include "hyscan-tvg-control-server.h"
#include "hyscan-sonar-control-server.h"
#include "hyscan-control-common.h"

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define TEST_N_REPEATS                 100

#define SONAR_N_SOURCES                3

#define SENSOR_N_PORTS                 4
#define SENSOR_N_UART_DEVICES          SENSOR_N_PORTS
#define SENSOR_N_UART_MODES            8
#define SENSOR_N_IP_ADDRESSES          4

#define GENERATOR_N_PRESETS            32

static gboolean test_result = TRUE;

typedef struct
{
  HyScanSensorPortType                 type;
  gboolean                             enable;
  HyScanSensorProtocolType             protocol;
  guint                                channel;
  gint64                               time_offset;
  HyScanAntennaPosition                position;
} VirtualPortInfo;

typedef struct
{
  HyScanSensorPortType                 type;
  gboolean                             enable;
  HyScanSensorProtocolType             protocol;
  guint                                channel;
  gint64                               time_offset;
  HyScanAntennaPosition                position;
  guint                                uart_device;
  guint                                uart_mode;
  gint                                 uart_device_ids[SENSOR_N_UART_DEVICES+1];
  gchar                               *uart_device_names[SENSOR_N_UART_DEVICES+1];
  gint                                 uart_mode_ids[SENSOR_N_UART_MODES+1];
  gchar                               *uart_mode_names[SENSOR_N_UART_MODES+1];
} UARTPortInfo;

typedef struct
{
  HyScanSensorPortType                 type;
  gboolean                             enable;
  HyScanSensorProtocolType             protocol;
  guint                                channel;
  gint64                               time_offset;
  HyScanAntennaPosition                position;
  guint                                ip_address;
  guint                                udp_port;
  gint                                 ip_address_ids[SENSOR_N_IP_ADDRESSES+1];
  gchar                               *ip_address_names[SENSOR_N_IP_ADDRESSES+1];
} UDPIPPortInfo;

typedef struct
{
  HyScanAntennaPosition                position;
  HyScanRawDataInfo                    raw_info;

  gdouble                              max_receive_time;
  gdouble                              cur_receive_time;
  gboolean                             auto_receive_time;

  gdouble                              signal_rate;
  gdouble                              tvg_rate;

  struct
  {
    HyScanGeneratorModeType            capabilities;
    HyScanGeneratorSignalType          signals;
    gint                               preset_ids[GENERATOR_N_PRESETS+1];
    gchar                             *preset_names[GENERATOR_N_PRESETS+1];
    gdouble                            min_tone_duration;
    gdouble                            max_tone_duration;
    gdouble                            min_lfm_duration;
    gdouble                            max_lfm_duration;

    gboolean                           enable;
    HyScanGeneratorModeType            cur_mode;
    HyScanGeneratorSignalType          cur_signal;
    gint                               cur_preset;
    gdouble                            cur_power;
    gdouble                            cur_duration;
  } generator;

  struct
  {
    HyScanTVGModeType                  capabilities;
    gdouble                            min_gain;
    gdouble                            max_gain;

    gboolean                           enable;
    guint                              cur_mode;
    gdouble                            cur_level;
    gdouble                            cur_sensitivity;
    gdouble                            cur_gain;
    gdouble                            cur_gain0;
    gdouble                            cur_step;
    gdouble                            cur_alpha;
    gdouble                            cur_beta;
  } tvg;
} SourceInfo;

typedef struct
{
  gboolean                             enable;
  gint64                               sync_capabilities;
  HyScanSonarSyncType                  sync_type;
  gchar                               *track_name;
  HyScanTrackType                      track_type;
} SonarInfo;

typedef struct
{
  HyScanSensorControlServer           *sensor;
  HyScanGeneratorControlServer        *generator;
  HyScanTVGControlServer              *tvg;
  HyScanSonarControlServer            *sonar;
} ServerInfo;

static GMainLoop                *main_loop    = NULL;

static int                       n_test_repeats;

static GHashTable               *ports;
static SourceInfo                starboard;
static SourceInfo                port;
static SourceInfo                echosounder;
static SonarInfo                 sonar_info;

static ServerInfo                server;
static HyScanSonarBox           *sonar_box;
static HyScanSonarModel         *sonar_model;
static HyScanSonarControl       *sonar_control;

typedef struct
{
  struct
  {
    gchar                     *name;
    guint                      channel;
    gint64                     time_offset;

    gboolean                   test;
    gboolean                   result;
  } sensor_virtual_port_param_frame;

  struct
  {
    gchar                     *name;
    guint                      channel;
    gint64                     time_offset;
    HyScanSensorProtocolType   protocol;
    guint                      uart_device;
    guint                      uart_mode;

    gboolean                   test;
    gboolean                   result;
  } sensor_uart_port_param_frame;

  struct
  {
    gchar                     *name;
    guint                      channel;
    gint64                     time_offset;
    HyScanSensorProtocolType   protocol;
    guint                      ip_address;
    guint                      udp_port;

    gboolean                   test;
    gboolean                   result;
  } sensor_udp_ip_port_param_frame;

  struct
  {
    gchar                    *name;
    HyScanAntennaPosition     position;

    gboolean                  test;
    gboolean                  result;
  } sensor_set_position_frame;

  struct
  {
    gchar                    *name;
    gboolean                  enable;

    gboolean                  test;
    gboolean                  result;
  } sensor_set_enable_frame;

  struct
  {
    HyScanSourceType          source;
    guint                     preset;

    gboolean                  test;
    gboolean                  result;
  } generator_set_preset_frame;

  struct
  {
    HyScanSourceType          source;
    HyScanGeneratorSignalType signal;

    gboolean                  test;
    gboolean                  result;
  } generator_set_auto_frame;

  struct
  {
    HyScanSourceType          source;
    HyScanGeneratorSignalType signal;
    gdouble                   power;

    gboolean                  test;
    gboolean                  result;
  } generator_set_simple_frame;

  struct
  {
    HyScanSourceType          source;
    HyScanGeneratorSignalType signal;
    gdouble                   duration;
    gdouble                   power;

    gboolean                  test;
    gboolean                  result;
  } generator_set_extended_frame;

  struct
  {
    HyScanSourceType source;
    gboolean                  enable;

    gboolean                  test;
    gboolean                  result;
  } generator_set_enable_frame;

  struct
  {
    HyScanSourceType source;
    gdouble                   level;
    gdouble                   sensitivity;

    gboolean                  test;
    gboolean                  result;
  } tvg_set_auto_frame;

  struct
  {
    HyScanSourceType          source;
    gdouble                   gain;

    gboolean                  test;
    gboolean                  result;
  } tvg_set_constant_frame;

  struct
  {
    HyScanSourceType          source;
    gdouble                   gain0;
    gdouble                   step;

    gboolean                  test;
    gboolean                  result;
  } tvg_set_linear_db_frame;

  struct
  {
    HyScanSourceType          source;
    gdouble                   gain0;
    gdouble                   beta;
    gdouble                   alpha;

    gboolean                  test;
    gboolean                  result;
  } tvg_set_logarithmic_frame;

  struct
  {
    HyScanSourceType          source;
    gboolean                  enable;

    gboolean                  test;
    gboolean                  result;
  } tvg_set_enable_frame;

  struct
  {
    HyScanSourceType          source;
    HyScanAntennaPosition     position;

    gboolean                  test;
    gboolean                  result;
  } sonar_set_position_frame;

  struct
  {
    HyScanSourceType          source;
    gdouble                   receive_time;

    gboolean                  test;
    gboolean                  result;
  } sonar_set_receive_time_frame;

  struct
  {
    HyScanSonarSyncType       sync_type;

    gboolean                  test;
    gboolean                  result;
  } sonar_set_sync_type_frame;

  struct
  {
    gchar                    *track_name;
    HyScanTrackType           track_type;

    gboolean                  test;
    gboolean                  result;
  } sonar_start_frame;

  struct
  {
    gboolean                  test;
    gboolean                  result;
  } sonar_stop_frame;

  struct
  {
    gboolean                  test;
    gboolean                  result;
  } sonar_ping_frame;
} Frames;

Frames frames;

/*
 * Вспомогательные функции.
 */

static gboolean                  pos_equals                     (const HyScanAntennaPosition    *p1,
                                                                 const HyScanAntennaPosition    *p2,
                                                                 const gdouble                   eps);

static HyScanSourceType          source_type_by_index           (const guint                     index);
static SourceInfo               *source_info_by_source_type     (const HyScanSourceType          type);

static gchar                    *random_virtual_port_name       (void);
static gchar                    *random_udp_ip_port_name        (void);
static gchar                    *random_uart_port_name          (void);
static gchar                    *random_port_name_with_prefix   (const gchar                    *prefix);
static gchar                    *random_port_name               (void);
static gint                      random_uart_mode               (const gchar                    *name);
static gint                      random_uart_device             (const gchar                    *name);
static gint                      random_ip_addres               (const gchar                    *name);
static HyScanAntennaPosition     random_antenna_position        (void);
static HyScanSourceType          random_source_type             (void);
static gint                      random_preset                  (const HyScanSourceType          source_type);
static HyScanGeneratorSignalType random_signal                  (void);
static HyScanGeneratorSignalType random_signal_full             (HyScanGeneratorSignalType       accepted_signals);
static HyScanSonarSyncType       random_sync                    (void);
static HyScanTrackType           random_track_type              (void);

/*
 * Функции обратного вызова для серверов SENSOR, GENERATOR, TVG, SONAR.
 */

static gboolean sensor_virtual_port_param   (ServerInfo                 *server,
                                             const gchar                *name,
                                             guint                       channel,
                                             gint64                      time_offset);

static gboolean sensor_uart_port_param      (ServerInfo                 *server,
                                             const gchar                *name,
                                             guint                       channel,
                                             gint64                      time_offset,
                                             HyScanSensorProtocolType    protocol,
                                             guint                       uart_device,
                                             guint                       uart_mode);

static gboolean sensor_udp_ip_port_param    (ServerInfo                 *server,
                                             const gchar                *name,
                                             guint                       channel,
                                             gint64                      time_offset,
                                             HyScanSensorProtocolType    protocol,
                                             guint                       ip_address,
                                             guint                       udp_port);

static gboolean sensor_set_position         (ServerInfo                 *server,
                                             const gchar                *name,
                                             HyScanAntennaPosition      *position);

static gboolean sensor_set_enable           (ServerInfo                 *server,
                                             const gchar                *name,
                                             gboolean                    enable);

static gboolean generator_set_preset        (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             guint                       preset);

static gboolean generator_set_auto          (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             HyScanGeneratorSignalType   signal);

static gboolean generator_set_simple        (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             HyScanGeneratorSignalType   signal,
                                             gdouble                     power);

static gboolean generator_set_extended      (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             HyScanGeneratorSignalType   signal,
                                             gdouble                     duration,
                                             gdouble                     power);

static gboolean generator_set_enable        (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             gboolean                    enable);

static gboolean tvg_set_auto                (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             gdouble                     level,
                                             gdouble                     sensitivity);

static gboolean tvg_set_constant            (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             gdouble                     gain);

static gboolean tvg_set_linear_db           (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             gdouble                     gain0,
                                             gdouble                     step);

static gboolean tvg_set_logarithmic         (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             gdouble                     gain0,
                                             gdouble                     beta,
                                             gdouble                     alpha);

static gboolean tvg_set_enable              (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             gboolean                    enable);

static gboolean sonar_set_position          (ServerInfo                 *server,
                                             HyScanSourceType            source,
                                             HyScanAntennaPosition      *position);

static gboolean sonar_set_receive_time      (ServerInfo                *server,
                                             HyScanSourceType           source,
                                             gdouble                    receive_time);

static gboolean sonar_set_sync_type         (ServerInfo                 *server,
                                             HyScanSonarSyncType         sync_type);

static gboolean sonar_start                 (ServerInfo                 *server,
                                             const gchar                *track_name,
                                             HyScanTrackType             track_type);

static gboolean sonar_stop                  (ServerInfo                 *server);

static gboolean sonar_ping                  (ServerInfo                 *server);

/*
 *  Функция обратного вызова для модели управления гидролокатором.
 */

static void on_sonar_model_params_updated    (HyScanSonarModel          *model,
                                              gboolean                   result,
                                              gpointer                   udata);

/*
 * Основные функции теста.
 */

static HyScanSonarBox   *create_sonar   (void);

static void              init_servers   (HyScanSonarBox                 *sonar_box);

static gboolean          test_entry     (gpointer                        udata);


/* Сравнивает структуры HyScanAntennaPosition. */
static gboolean
pos_equals (const HyScanAntennaPosition *p1,
            const HyScanAntennaPosition *p2,
            const gdouble                eps)
{
  if (fabs (p1->x - p2->x) > eps)
    return FALSE;
  if (fabs (p1->y - p2->y) > eps)
    return FALSE;
  if (fabs (p1->z - p2->z) > eps)
    return FALSE;
  if (fabs (p1->gamma - p2->gamma) > eps)
    return FALSE;
  if (fabs (p1->psi - p2->psi) > eps)
    return FALSE;
  if (fabs (p1->theta - p2->theta) > eps)
    return FALSE;
  return TRUE;
}

/* Функция возвращает тип источника данных по его индексу. */
static HyScanSourceType
source_type_by_index (const guint index)
{
  switch (index)
    {
      case 0:
        return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
      case 1:
        return HYSCAN_SOURCE_SIDE_SCAN_PORT;
      case 2:
        return HYSCAN_SOURCE_ECHOSOUNDER;
      default:
        return HYSCAN_SOURCE_INVALID;
    }
}

/* Функция возвращает информацию об источнике данных по его типу. */
static SourceInfo *
source_info_by_source_type (const HyScanSourceType type)
{
  switch (type)
    {
      case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
        return &starboard;
      case HYSCAN_SOURCE_SIDE_SCAN_PORT:
        return &port;
      case HYSCAN_SOURCE_ECHOSOUNDER:
        return &echosounder;
      default:
        return NULL;
    }
}

/* Возвращает случайное имя виртуального порта из доступных. */
static gchar *
random_virtual_port_name (void)
{
  return random_port_name_with_prefix ("virtual");
}

/* Возвращает случайное имя udp/ip порта из доступных. */
static gchar *
random_udp_ip_port_name (void)
{
  return random_port_name_with_prefix ("udp");
}

/* Возвращает случайное имя uart порта из доступных. */
static gchar *
random_uart_port_name (void)
{
  return random_port_name_with_prefix ("uart");
}

/* Возвращает случайное имя порта.
 * Имя собирается из префикса и номера порта.
 * Номер порта выбирается из доступного интервала. */
static gchar *
random_port_name_with_prefix (const gchar *prefix)
{
  GList *keys, *port;
  gint i, len;
  gchar *target_name;
  gpointer retval = NULL;

  target_name = g_strdup_printf ("%s.%d", prefix, g_random_int_range (0, SENSOR_N_PORTS) + 1);
  keys = g_hash_table_get_keys (ports);
  len = g_list_length (keys);

  port = keys;
  for (i = 0; i < len; ++i)
    {
      if (!g_strcmp0 (port->data, target_name))
        {
          retval = port->data;
          break;
        }

      port = port->next;
    }

  g_list_free (keys);
  g_free (target_name);

  return retval;
}

/* Возвращает случайное имя порта из доступных. */
static gchar *
random_port_name (void)
{
  GList *keys, *port;
  gint i, index;
  gpointer retval;

  keys = g_hash_table_get_keys (ports);
  index = g_random_int_range (0, g_list_length (keys));

  port = keys;
  for (i = 0; i < index; ++i)
    port = port->next;

  retval = port->data;
  g_list_free (keys);

  return retval;
}

/* Возвращает случайный режим uart для указанного порта. */
static gint
random_uart_mode (const gchar *name)
{
  UARTPortInfo *port = g_hash_table_lookup (ports, name);
  return port->uart_mode_ids[g_random_int_range (1, SENSOR_N_UART_MODES)];
}

/* Возвращает случайное uart-устройство для указанного порта. */
static gint
random_uart_device (const gchar *name)
{
  UARTPortInfo *port = g_hash_table_lookup (ports, name);
  return port->uart_device_ids[g_random_int_range (1, SENSOR_N_UART_DEVICES)];
}

/* Возвращает случайный ip-адрес для указанного порта. */
static gint
random_ip_addres (const gchar *name)
{
  UDPIPPortInfo *port = g_hash_table_lookup (ports, name);
  return port->ip_address_ids[g_random_int_range (1, SENSOR_N_IP_ADDRESSES)];
}

/* Возвращает случайное местоположение. */
static HyScanAntennaPosition
random_antenna_position (void)
{
  HyScanAntennaPosition p;
  p.x = g_random_double ();
  p.y = g_random_double ();
  p.z = g_random_double ();
  p.psi = g_random_double ();
  p.gamma = g_random_double ();
  p.theta = g_random_double ();
  return p;
}

/* Возвращает случайный источник из доступных. */
static HyScanSourceType
random_source_type (void)
{
  return source_type_by_index (g_random_int () % 2);
}

/* Возвращает случайную преднастройку для указанного источника. */
static gint
random_preset (const HyScanSourceType source_type)
{
  SourceInfo *source_info = source_info_by_source_type (source_type);
  return source_info->generator.preset_ids[g_random_int () % (GENERATOR_N_PRESETS - 1)];
}

/* Возвращает случайный сигнал. */
static HyScanGeneratorSignalType
random_signal (void)
{
  switch (g_random_int_range (0, 4))
    {
      case 0:
        return HYSCAN_GENERATOR_SIGNAL_AUTO;
      case 1:
        return HYSCAN_GENERATOR_SIGNAL_TONE;
      case 2:
        return HYSCAN_GENERATOR_SIGNAL_LFM;
      case 3:
        return HYSCAN_GENERATOR_SIGNAL_LFMD;
      default:
        return HYSCAN_GENERATOR_SIGNAL_INVALID;
    }
}

/* Возвращает случайный сигнал из указанных. */
static HyScanGeneratorSignalType
random_signal_full (HyScanGeneratorSignalType accepted_signals)
{
  HyScanGeneratorSignalType signal_type;
  if (accepted_signals == HYSCAN_GENERATOR_SIGNAL_INVALID)
    return HYSCAN_GENERATOR_SIGNAL_INVALID;

  do
    signal_type = random_signal ();
  while (!(signal_type & accepted_signals));

  return signal_type;
}

/* Возвращает случайный тип синхронизации. */
static HyScanSonarSyncType
random_sync (void)
{
  switch (g_random_int_range (0, 3))
    {
      case 0:
        return HYSCAN_SONAR_SYNC_INTERNAL;
      case 1:
        return HYSCAN_SONAR_SYNC_SOFTWARE;
      case 2:
        return HYSCAN_SONAR_SYNC_EXTERNAL;
      default:
        return HYSCAN_SONAR_SYNC_INVALID;
    }
}

/* Возвращает случайный тип галса. */
static HyScanTrackType
random_track_type (void)
{
  switch (g_random_int_range (0, 3))
    {
      case 0:
        return HYSCAN_TRACK_TACK;
      case 1:
        return HYSCAN_TRACK_SURVEY;
      case 2:
        return HYSCAN_TRACK_CALIBRATION;
      default:
        return HYSCAN_TRACK_UNSPECIFIED;
    }
}

/* Функция изменяет параметры VIRTUAL порта. */
static gboolean
sensor_virtual_port_param (ServerInfo  *server,
                           const gchar *name,
                           guint        channel,
                           gint64       time_offset)
{
  if (g_strcmp0 (name, frames.sensor_virtual_port_param_frame.name))
    return FALSE;
  if (channel != frames.sensor_virtual_port_param_frame.channel)
    return FALSE;
  if (time_offset != frames.sensor_virtual_port_param_frame.time_offset)
    return FALSE;

  frames.sensor_virtual_port_param_frame.result = TRUE;
  return TRUE;
}

/* Функция изменяет параметры UART порта. */
static gboolean
sensor_uart_port_param (ServerInfo                *server,
                        const gchar               *name,
                        guint                      channel,
                        gint64                     time_offset,
                        HyScanSensorProtocolType   protocol,
                        guint                      uart_device,
                        guint                      uart_mode)
{
  if (g_strcmp0 (name, frames.sensor_uart_port_param_frame.name))
    return FALSE;
  if (channel != frames.sensor_uart_port_param_frame.channel)
    return FALSE;
  if (time_offset != frames.sensor_uart_port_param_frame.time_offset)
    return FALSE;
  if (protocol != frames.sensor_uart_port_param_frame.protocol)
    return FALSE;
  if (uart_device != frames.sensor_uart_port_param_frame.uart_device)
    return FALSE;
  if (uart_mode != frames.sensor_uart_port_param_frame.uart_mode)
    return FALSE;

  frames.sensor_uart_port_param_frame.result = TRUE;
  return TRUE;
}

/* Функция изменяет параметры UDP/IP порта. */
static gboolean
sensor_udp_ip_port_param (ServerInfo                *server,
                          const gchar               *name,
                          guint                      channel,
                          gint64                     time_offset,
                          HyScanSensorProtocolType   protocol,
                          guint                      ip_address,
                          guint                      udp_port)
{
  if (g_strcmp0 (name, frames.sensor_udp_ip_port_param_frame.name))
    return FALSE;
  if (channel != frames.sensor_udp_ip_port_param_frame.channel)
    return FALSE;
  if (time_offset != frames.sensor_udp_ip_port_param_frame.time_offset)
    return FALSE;
  if (protocol != frames.sensor_udp_ip_port_param_frame.protocol)
    return FALSE;
  if (ip_address != frames.sensor_udp_ip_port_param_frame.ip_address)
    return FALSE;
  if (udp_port != frames.sensor_udp_ip_port_param_frame.udp_port)
    return FALSE;

  frames.sensor_udp_ip_port_param_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает местоположение приёмной антенны датчика. */
static gboolean
sensor_set_position (ServerInfo            *server,
                     const gchar           *name,
                     HyScanAntennaPosition *position)
{
  if (g_strcmp0 (name, frames.sensor_set_position_frame.name))
    return FALSE;
  if (!pos_equals (position, &frames.sensor_set_position_frame.position, 10e-3))
    return FALSE;

  frames.sensor_set_position_frame.result = TRUE;
  return TRUE;
}

/* Функция включает и выключает датчик. */
static gboolean
sensor_set_enable (ServerInfo  *server,
                   const gchar *name,
                   gboolean     enable)
{
  if (g_strcmp0 (name, frames.sensor_set_enable_frame.name))
    return FALSE;
  if (enable != frames.sensor_set_enable_frame.enable)
    return FALSE;

  frames.sensor_set_enable_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает режим работы генератора по преднастройкам. */
static gboolean
generator_set_preset (ServerInfo       *server,
                      HyScanSourceType  source,
                      guint             preset)
{
  if (source != frames.generator_set_preset_frame.source)
    return FALSE;
  if (preset != frames.generator_set_preset_frame.preset)
    return FALSE;

  frames.generator_set_preset_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает автоматический режим работы генератора. */
static gboolean
generator_set_auto (ServerInfo                *server,
                    HyScanSourceType           source,
                    HyScanGeneratorSignalType  signal)
{
  if (source != frames.generator_set_auto_frame.source)
    return FALSE;
  if (signal != frames.generator_set_auto_frame.signal)
    return FALSE;

  frames.generator_set_auto_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает упрощённый режим работы генератора. */
static gboolean
generator_set_simple (ServerInfo                *server,
                      HyScanSourceType           source,
                      HyScanGeneratorSignalType  signal,
                      gdouble                    power)
{
  if (source != frames.generator_set_simple_frame.source)
    return FALSE;
  if (signal != frames.generator_set_simple_frame.signal)
    return FALSE;
  if (fabs (power - frames.generator_set_simple_frame.power) > 10e-3)
    return FALSE;

  frames.generator_set_simple_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает расширенный режим работы генератора. */
static gboolean
generator_set_extended (ServerInfo                *server,
                        HyScanSourceType           source,
                        HyScanGeneratorSignalType  signal,
                        gdouble                    duration,
                        gdouble                    power)
{
  if (source != frames.generator_set_extended_frame.source)
    return FALSE;
  if (signal != frames.generator_set_extended_frame.signal)
    return FALSE;
  if (duration != frames.generator_set_extended_frame.duration)
    return FALSE;
  if (fabs (power - frames.generator_set_extended_frame.power) > 10e-3)
    return FALSE;

  frames.generator_set_extended_frame.result = TRUE;
  return TRUE;
}

/* Функция включает или отключает генератор. */
static gboolean
generator_set_enable (ServerInfo       *server,
                      HyScanSourceType  source,
                      gboolean          enable)
{
  if (source != frames.generator_set_enable_frame.source)
    return FALSE;
  if (enable != frames.generator_set_enable_frame.enable)
    return FALSE;

  frames.generator_set_enable_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает автоматический режим управления ВАРУ. */
static gboolean
tvg_set_auto (ServerInfo       *server,
              HyScanSourceType  source,
              gdouble           level,
              gdouble           sensitivity)
{
  if (source != frames.tvg_set_auto_frame.source)
    return FALSE;
  if (fabs (level - frames.tvg_set_auto_frame.level) > 10e-3)
    return FALSE;
  if (fabs (sensitivity - frames.tvg_set_auto_frame.sensitivity) > 10e-3)
    return FALSE;

  frames.tvg_set_auto_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает постоянный уровень усиления ВАРУ. */
static gboolean
tvg_set_constant (ServerInfo       *server,
                  HyScanSourceType  source,
                  gdouble           gain)
{
  if (source != frames.tvg_set_constant_frame.source)
    return FALSE;
  if (fabs (gain - frames.tvg_set_constant_frame.gain) > 10e-3)
    return FALSE;

  frames.tvg_set_constant_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает линейный закон усиления ВАРУ. */
static gboolean
tvg_set_linear_db (ServerInfo       *server,
                   HyScanSourceType  source,
                   gdouble           gain0,
                   gdouble           step)
{
  if (source != frames.tvg_set_linear_db_frame.source)
    return FALSE;
  if (fabs (gain0 - frames.tvg_set_linear_db_frame.gain0) > 10e-3)
    return FALSE;
  if (fabs (step - frames.tvg_set_linear_db_frame.step) > 10e-3)
    return FALSE;

  frames.tvg_set_linear_db_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает логарифмический закон усиления ВАРУ. */
static gboolean
tvg_set_logarithmic (ServerInfo       *server,
                     HyScanSourceType  source,
                     gdouble           gain0,
                     gdouble           beta,
                     gdouble           alpha)
{
  if (source != frames.tvg_set_logarithmic_frame.source)
    return FALSE;
  if (fabs (gain0 - frames.tvg_set_logarithmic_frame.gain0) > 10e-3)
    return FALSE;
  if (fabs (beta - frames.tvg_set_logarithmic_frame.beta) > 10e-3)
    return FALSE;
  if (fabs (alpha - frames.tvg_set_logarithmic_frame.alpha) > 10e-3)
    return FALSE;

  frames.tvg_set_logarithmic_frame.result = TRUE;
  return TRUE;
}

/* Функция включает или отключает ВАРУ. */
static gboolean
tvg_set_enable (ServerInfo       *server,
                HyScanSourceType  source,
                gboolean          enable)
{
  if (source != frames.tvg_set_enable_frame.source)
    return FALSE;
  if (enable != frames.tvg_set_enable_frame.enable)
    return FALSE;

  frames.tvg_set_enable_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает тип синхронизации излучения. */
static gboolean
sonar_set_sync_type (ServerInfo          *server,
                     HyScanSonarSyncType  sync_type)
{
  if (sync_type != frames.sonar_set_sync_type_frame.sync_type)
    return FALSE;

  frames.sonar_set_sync_type_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает местоположение приёмной антенны гидролокатора. */
static gboolean
sonar_set_position (ServerInfo            *server,
                    HyScanSourceType       source,
                    HyScanAntennaPosition *position)
{
  if (source != frames.sonar_set_position_frame.source)
    return FALSE;
  if (!pos_equals (position, &frames.sonar_set_position_frame.position, 10e-3))
    return FALSE;

  frames.sonar_set_position_frame.result = TRUE;
  return TRUE;
}

/* Функция устанавливает время приёма эхосигнала бортом. */
static gboolean
sonar_set_receive_time (ServerInfo       *server,
                        HyScanSourceType  source,
                        gdouble           receive_time)
{
  if (source != frames.sonar_set_receive_time_frame.source)
    return FALSE;
  if (fabs (receive_time - frames.sonar_set_receive_time_frame.receive_time) > 10e-3)
    return FALSE;

  frames.sonar_set_receive_time_frame.result = TRUE;
  return TRUE;
}

/* Функция включает гидролокатор в работу. */
static gboolean
sonar_start (ServerInfo      *server,
             const gchar     *track_name,
             HyScanTrackType  track_type)
{
  if (track_type != frames.sonar_start_frame.track_type)
    return FALSE;

  frames.sonar_start_frame.result = TRUE;
  return TRUE;
}

/* Функция останавливает работу гидролокатора. */
static gboolean
sonar_stop (ServerInfo *server)
{

  frames.sonar_stop_frame.result = TRUE;
  return TRUE;
}

/* Функция выполняет цикл зондирования. */
static gboolean
sonar_ping (ServerInfo *server)
{
  frames.sonar_ping_frame.result = TRUE;
  return TRUE;
}

/* Асинхронное управление гидролокатором завершено. */
static void
on_sonar_model_params_updated (HyScanSonarModel *model,
                               gboolean          result,
                               gpointer          udata)
{

  if (result)
    {
      gboolean retval = TRUE;

      /* Сверка параметров VIRTUAL-порта. */
      if (retval && frames.sensor_virtual_port_param_frame.test)
        {
          guint channel;
          gint64 time_offset;

          hyscan_sonar_model_sensor_get_virtual_params (model, frames.sensor_virtual_port_param_frame.name,
                                                        &channel, &time_offset);

          retval = retval &&
                   (channel == frames.sensor_virtual_port_param_frame.channel) &&
                   (time_offset == frames.sensor_virtual_port_param_frame.time_offset);

          if (!retval)
            g_warning ("sensor_virtual_port_param_frame");
        }

      /* Сверка параметров UART-порта. */
      if (retval && frames.sensor_uart_port_param_frame.test)
        {
          guint channel;
          gint64 time_offset;
          HyScanSensorProtocolType protocol;
          guint uart_device;
          guint uart_mode;

          hyscan_sonar_model_sensor_get_uart_params (model, frames.sensor_uart_port_param_frame.name,
                                                     &channel, &time_offset, &protocol, &uart_device, &uart_mode);

          retval = retval &&
                   (channel == frames.sensor_uart_port_param_frame.channel) &&
                   (time_offset == frames.sensor_uart_port_param_frame.time_offset) &&
                   (protocol == frames.sensor_uart_port_param_frame.protocol) &&
                   (uart_device == frames.sensor_uart_port_param_frame.uart_device) &&
                   (uart_mode == frames.sensor_uart_port_param_frame.uart_mode);

          if (!retval)
            g_warning ("sensor_uart_port_param_frame");
        }

      /* Сверка параметров UDP/IP-порта. */
      if (retval && frames.sensor_udp_ip_port_param_frame.test)
        {
          guint channel;
          gint64 time_offset;
          HyScanSensorProtocolType protocol;
          guint ip_address;
          guint16 udp_port;

          hyscan_sonar_model_sensor_get_udp_ip_params (model, frames.sensor_udp_ip_port_param_frame.name,
                                                       &channel, &time_offset, &protocol, &ip_address, &udp_port);

          retval = retval &&
                   (channel == frames.sensor_udp_ip_port_param_frame.channel) &&
                   (time_offset == frames.sensor_udp_ip_port_param_frame.time_offset) &&
                   (protocol == frames.sensor_udp_ip_port_param_frame.protocol) &&
                   (ip_address == frames.sensor_udp_ip_port_param_frame.ip_address) &&
                   (udp_port == frames.sensor_udp_ip_port_param_frame.udp_port);

          if (!retval)
            g_warning ("sensor_udp_ip_port_param_frame");
        }

      /* Сверка местоположения. */
      if (retval && frames.sonar_set_position_frame.test)
        {
          HyScanAntennaPosition *pos;
          pos = hyscan_sonar_model_sensor_get_position (model, frames.sensor_set_position_frame.name);
          retval = retval &&
                   pos != NULL &&
                   memcmp (pos, &frames.sensor_set_position_frame.position, sizeof (HyScanAntennaPosition)) == 0;
          g_free (pos);

          if (!retval)
            g_warning ("sensor_set_position_frame");
        }

      /* Сверка состояния датчика. */
      if (retval && frames.sensor_set_enable_frame.test)
        {
          gboolean enabled = hyscan_sonar_model_sensor_is_enabled (model, frames.sensor_set_enable_frame.name);
          retval = retval && (frames.sensor_set_enable_frame.enable == enabled);

          if (!retval)
            g_warning ("sensor_set_enable_frame");
        }

      /* Сверка преднастроек генератора. */
      if (retval && frames.generator_set_preset_frame.test)
        {
          guint preset;
          hyscan_sonar_model_gen_get_preset_params (model, frames.generator_set_preset_frame.source, &preset);
          retval = retval && frames.generator_set_preset_frame.preset == preset;

          if (!retval)
            g_warning ("generator_set_preset_frame");
        }

      /* Сверка автоматических параметров генератора. */
      if (retval && frames.generator_set_auto_frame.test)
        {
          HyScanGeneratorSignalType signal;
          hyscan_sonar_model_gen_get_auto_params (model, frames.generator_set_auto_frame.source, &signal);
          retval = retval && frames.generator_set_auto_frame.signal == signal;

          if (!retval)
            g_warning ("generator_set_auto_frame");
        }

      /* Сверка упрощенных параметров генератора. */
      if (retval && frames.generator_set_simple_frame.test)
        {
          HyScanGeneratorSignalType signal;
          gdouble power;
          hyscan_sonar_model_gen_get_simple_params (model, frames.generator_set_simple_frame.source, &signal, &power);
          retval = retval &&
                   frames.generator_set_simple_frame.signal == signal &&
                   frames.generator_set_simple_frame.power == power;

          if (!retval)
            g_warning ("generator_set_simple_frame");
        }

      /* Сверка расширенных параметров генератора. */
      if (retval && frames.generator_set_extended_frame.test)
        {
          HyScanGeneratorSignalType signal;
          gdouble power, duration;
          hyscan_sonar_model_gen_get_extended_params (model, frames.generator_set_extended_frame.source,
                                                      &signal, &duration, &power);
          retval = retval &&
                   frames.generator_set_extended_frame.signal == signal &&
                   frames.generator_set_extended_frame.duration == duration &&
                   frames.generator_set_extended_frame.power == power;

          if (!retval)
            g_warning ("generator_set_extended_frame");
        }

      /* Сверка состояния генератора. */
      if (retval && frames.generator_set_enable_frame.test)
        {
          gdouble enabled = hyscan_sonar_model_gen_is_enabled (model, frames.generator_set_enable_frame.source);
          retval = retval && frames.generator_set_enable_frame.enable == enabled;

          if (!retval)
            g_warning ("generator_set_enable_frame");
        }

      /* Сверка автоматических ВАРУ. */
      if (retval && frames.tvg_set_auto_frame.test)
        {
          gdouble level, sensitivity;
          hyscan_sonar_model_tvg_get_auto_params (model, frames.tvg_set_auto_frame.source, &level, &sensitivity);
          retval = retval &&
                   frames.tvg_set_auto_frame.level == level &&
                   frames.tvg_set_auto_frame.sensitivity == sensitivity;

          if (!retval)
            g_warning ("tvg_set_auto_frame");
        }

      /* Сверка постоянного ВАРУ. */
      if (retval && frames.tvg_set_constant_frame.test)
        {
          gdouble gain;
          hyscan_sonar_model_tvg_get_const_params (model, frames.tvg_set_constant_frame.source, &gain);
          retval = retval && frames.tvg_set_constant_frame.gain == gain;

          if (!retval)
            g_warning ("tvg_set_constant_frame");
        }

      /* Сверка линейного ВАРУ. */
      if (retval && frames.tvg_set_linear_db_frame.test)
        {
          gdouble gain0, step;
          hyscan_sonar_model_tvg_get_linear_db_params (model, frames.tvg_set_linear_db_frame.source,
                                                       &gain0, &step);
          retval = retval &&
                   frames.tvg_set_linear_db_frame.gain0 == gain0 &&
                   frames.tvg_set_linear_db_frame.step == step;

          if (!retval)
            g_warning ("tvg_set_linear_db_frame");
        }

      /* Сверка логарифмичкеского ВАРУ. */
      if (retval && frames.tvg_set_logarithmic_frame.test)
        {
          gdouble gain0, beta, alpha;
          hyscan_sonar_model_tvg_get_logarithmic_params (model, frames.tvg_set_logarithmic_frame.source,
                                                         &gain0, &beta, &alpha);
          retval = retval &&
                   frames.tvg_set_logarithmic_frame.gain0 == gain0 &&
                   frames.tvg_set_logarithmic_frame.beta == beta &&
                   frames.tvg_set_logarithmic_frame.alpha == alpha;

          if (!retval)
            g_warning ("tvg_set_logarithmic_frame");
        }

      /* Сверка состояния ВАРУ. */
      if (retval && frames.tvg_set_enable_frame.test)
        {
          gdouble enabled = hyscan_sonar_model_tvg_is_enabled (model, frames.tvg_set_enable_frame.source);
          retval = retval && frames.tvg_set_enable_frame.enable == enabled;\

          if (!retval)
            g_warning ("tvg_set_enable_frame");
        }

      /* Сверка времени приёма. */
      if (retval && frames.sonar_set_receive_time_frame.test)
        {
          gdouble recv_time = hyscan_sonar_model_get_receive_time (model, frames.sonar_set_receive_time_frame.source);
          retval = retval && frames.sonar_set_receive_time_frame.receive_time == recv_time;

          if (!retval)
            g_warning ("sonar_set_receive_time_frame");
        }

      /* Сверка типа синхронизации. */
      if (retval && frames.sonar_set_sync_type_frame.test)
        {
          retval = retval && frames.sonar_set_sync_type_frame.sync_type == hyscan_sonar_model_get_sync_type (model);

          if (!retval)
            g_warning ("sonar_set_sync_type_frame");
        }

      /* Сверка тип галса. */
      if (retval && frames.sonar_start_frame.test)
        {
          retval = retval && frames.sonar_start_frame.track_type == hyscan_sonar_model_get_track_type (model);

          if (!retval)
            g_warning ("sonar_start_frame");
        }

      /* Сверка местоположения. */
      if (retval && frames.sonar_set_position_frame.test)
        {
          HyScanAntennaPosition *pos;
          pos = hyscan_sonar_model_sonar_get_position (sonar_model, frames.sonar_set_position_frame.source);
          retval = retval &&
                   pos != NULL &&
                   memcmp (pos, &frames.sonar_set_position_frame.position, sizeof (HyScanAntennaPosition)) == 0;
          g_free (pos);

          if (!retval)
            g_warning ("sonar_set_position_frame");
        }

      g_print ("Attempt %3d (out of %d): ", n_test_repeats + 1, TEST_N_REPEATS);
      if (retval)
        {
          g_print ("PASS\n");
        }
      else
        {
          g_print ("FAIL\n");
          test_result = FALSE;
          g_main_loop_quit (main_loop);
          return;
        }
    }
  else
    {
      g_message ("Can't change sonar parameters. Test failed.");
      g_main_loop_quit (main_loop);
      return;
    }

  if (++n_test_repeats < TEST_N_REPEATS)
    g_timeout_add (100, test_entry, NULL);
  else
    g_main_loop_quit (main_loop);
}

/* Создаёт виртуальный гидролокатор. */
static HyScanSonarBox *
create_sonar (void)
{
  HyScanSonarBox *sonar_box;
  HyScanSonarSchema *schema;
  gchar *schema_data;
  guint i, j;

  /* Параметры источников данных. */
  for (i = 0; i < 3; i++)
    {
      SourceInfo *info;

      info = source_info_by_source_type (source_type_by_index (i));
      memset (info, 0, sizeof (SourceInfo));

      info->raw_info.antenna.pattern.vertical = g_random_double ();
      info->raw_info.antenna.pattern.horizontal = g_random_double ();
      info->raw_info.antenna.offset.vertical = g_random_double ();
      info->raw_info.antenna.offset.horizontal = g_random_double ();
      info->raw_info.antenna.frequency = g_random_double ();
      info->raw_info.antenna.bandwidth = g_random_double ();
      info->raw_info.adc.vref = g_random_double ();
      info->raw_info.adc.offset = (gint) (100 * g_random_double ());

      info->max_receive_time = g_random_double () + 0.1;
      info->auto_receive_time = (i == 0) ? TRUE : FALSE;

      info->signal_rate = g_random_double ();
      info->tvg_rate = g_random_double ();

      info->generator.capabilities = HYSCAN_GENERATOR_MODE_PRESET | HYSCAN_GENERATOR_MODE_AUTO |
                                     HYSCAN_GENERATOR_MODE_SIMPLE | HYSCAN_GENERATOR_MODE_EXTENDED;
      info->generator.signals = HYSCAN_GENERATOR_SIGNAL_AUTO | HYSCAN_GENERATOR_SIGNAL_TONE |
                                HYSCAN_GENERATOR_SIGNAL_LFM | HYSCAN_GENERATOR_SIGNAL_LFMD;

      info->generator.min_tone_duration = g_random_double ();
      info->generator.max_tone_duration = info->generator.min_tone_duration + g_random_double ();
      info->generator.min_lfm_duration = g_random_double ();
      info->generator.max_lfm_duration = info->generator.min_lfm_duration + g_random_double ();

      info->generator.enable = FALSE;
      info->generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
      info->generator.cur_signal = HYSCAN_GENERATOR_SIGNAL_AUTO;

      info->tvg.capabilities = HYSCAN_TVG_MODE_AUTO | HYSCAN_TVG_MODE_CONSTANT |
                               HYSCAN_TVG_MODE_LINEAR_DB | HYSCAN_TVG_MODE_LOGARITHMIC;

      info->tvg.min_gain = g_random_double ();
      info->tvg.max_gain = info->tvg.min_gain + g_random_double ();

      info->tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
    }

  /* Параметры гидролокатора по умолчанию. */
  memset (&sonar_info, 0, sizeof (SonarInfo));
  sonar_info.sync_capabilities = HYSCAN_SONAR_SYNC_INTERNAL | HYSCAN_SONAR_SYNC_EXTERNAL | HYSCAN_SONAR_SYNC_SOFTWARE;

  /* Схема гидролокатора. */
  schema = hyscan_sonar_schema_new (HYSCAN_SONAR_SCHEMA_DEFAULT_TIMEOUT);

  /* Порты для датчиков. */
  ports = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  {
    guint pow2;

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        VirtualPortInfo *port = g_new0 (VirtualPortInfo, 1);
        gchar *name = g_strdup_printf ("virtual.%d", i + 1);

        port->type = HYSCAN_SENSOR_PORT_VIRTUAL;
        g_hash_table_insert (ports, name, port);

        hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
      }

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        UARTPortInfo *port = g_new0 (UARTPortInfo, 1);
        gchar *name = g_strdup_printf ("uart.%d", i + 1);

        port->type = HYSCAN_SENSOR_PORT_UART;
        g_hash_table_insert (ports, name, port);

        hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

        /* UART устройства. */
        port->uart_device_names[0] = g_strdup ("Disabled");
        port->uart_device_ids[0] = 0;
        for (j = 1; j <= SENSOR_N_UART_DEVICES; j++)
          {
            port->uart_device_names[j] = g_strdup_printf ("ttyS%d", j);
            port->uart_device_ids[j] =
                hyscan_sonar_schema_sensor_add_uart_device (schema, name, port->uart_device_names[j]);
          }

        /* Режимы UART устройств. */
        port->uart_mode_names[0] = g_strdup  ("Disabled");
        port->uart_mode_ids[0] = 0;
        for (j = 1, pow2 = 1; j <= SENSOR_N_UART_MODES; j += 1, pow2 *= 2)
          {
            port->uart_mode_names[j] = g_strdup_printf ("%d baud", 150 * pow2);
            port->uart_mode_ids[j] = hyscan_sonar_schema_sensor_add_uart_mode (schema, name, port->uart_mode_names[j]);
          }
      }

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        UDPIPPortInfo *port = g_new0 (UDPIPPortInfo, 1);
        gchar *name = g_strdup_printf ("udp.%d", i + 1);

        port->type = HYSCAN_SENSOR_PORT_UDP_IP;
        g_hash_table_insert (ports, name, port);

        hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

        /* IP адреса. */
        port->ip_address_names[0] = g_strdup  ("Disabled");
        port->ip_address_ids[0] = 0;
        for (j = 1; j <= SENSOR_N_IP_ADDRESSES; j++)
          {
            port->ip_address_names[j] = g_strdup_printf ("10.10.10.%d", j + 1);
            port->ip_address_ids[j] =
                hyscan_sonar_schema_sensor_add_ip_address (schema, name, port->ip_address_names[j]);
          }
      }
  }

  /* Типы синхронизации излучения. */
  hyscan_sonar_schema_sync_add (schema, (HyScanSonarSyncType) sonar_info.sync_capabilities);

  /* Источники данных. */
  for (i = 0; i < SONAR_N_SOURCES; i++)
    {
      HyScanSourceType source = source_type_by_index (i);
      SourceInfo *info = source_info_by_source_type (source);

      hyscan_sonar_schema_source_add (schema, source,
                                      info->raw_info.antenna.pattern.vertical,
                                      info->raw_info.antenna.pattern.horizontal,
                                      info->raw_info.antenna.frequency,
                                      info->raw_info.antenna.bandwidth,
                                      info->max_receive_time,
                                      info->auto_receive_time);

      hyscan_sonar_schema_generator_add (schema, source,
                                         info->generator.capabilities,
                                         info->generator.signals,
                                         info->generator.min_tone_duration,
                                         info->generator.max_tone_duration,
                                         info->generator.min_lfm_duration,
                                         info->generator.max_lfm_duration);

      info->generator.preset_ids[0] = 0;
      info->generator.preset_names[0] = g_strdup ("Disabled");
      for (j = 1; j <= GENERATOR_N_PRESETS; j++)
        {
          gchar *preset_name;
          preset_name = g_strdup_printf ("%s.preset.%d", hyscan_channel_get_name_by_types (source, FALSE, 1), j + 1);

          info->generator.preset_ids[j] = hyscan_sonar_schema_generator_add_preset (schema, source,
                                                                                    preset_name, preset_name);
          info->generator.preset_names[j] = preset_name;
        }

      hyscan_sonar_schema_tvg_add (schema, source,
                                   info->tvg.capabilities,
                                   info->tvg.min_gain,
                                   info->tvg.max_gain);

      hyscan_sonar_schema_channel_add (schema, source, 1,
                                       info->raw_info.antenna.offset.vertical,
                                       info->raw_info.antenna.offset.horizontal,
                                       info->raw_info.adc.offset,
                                       (gfloat) info->raw_info.adc.vref);

      hyscan_sonar_schema_source_add_acoustic (schema, source);
    }

  /* Схема гидролокатора. */
  schema_data = hyscan_data_schema_builder_get_data (HYSCAN_DATA_SCHEMA_BUILDER (schema));
  g_object_unref (schema);

  /* Параметры гидролокатора - интерфейс. */
  sonar_box = hyscan_sonar_box_new ();
  hyscan_sonar_box_set_schema (sonar_box, schema_data, "sonar");
  g_free (schema_data);

  return sonar_box;
}

/* Создаёт серверы SENSOR, GENERATOR, TVG, SONAR. */
static void
init_servers (HyScanSonarBox *sonar_box)
{
  /* Сервер управления. */
  server.sensor = hyscan_sensor_control_server_new (sonar_box);
  server.generator = hyscan_generator_control_server_new (sonar_box);
  server.tvg = hyscan_tvg_control_server_new (sonar_box);
  server.sonar = hyscan_sonar_control_server_new (sonar_box);

  g_signal_connect_swapped (server.sensor, "sensor-virtual-port-param",
                            G_CALLBACK (sensor_virtual_port_param), &server);
  g_signal_connect_swapped (server.sensor, "sensor-uart-port-param",
                            G_CALLBACK (sensor_uart_port_param), &server);
  g_signal_connect_swapped (server.sensor, "sensor-udp-ip-port-param",
                            G_CALLBACK (sensor_udp_ip_port_param), &server);
  g_signal_connect_swapped (server.sensor, "sensor-set-position",
                            G_CALLBACK (sensor_set_position), &server);
  g_signal_connect_swapped (server.sensor, "sensor-set-enable",
                            G_CALLBACK (sensor_set_enable), &server);

  g_signal_connect_swapped (server.generator, "generator-set-preset",
                            G_CALLBACK (generator_set_preset), &server);
  g_signal_connect_swapped (server.generator, "generator-set-auto",
                            G_CALLBACK (generator_set_auto), &server);
  g_signal_connect_swapped (server.generator, "generator-set-simple",
                            G_CALLBACK (generator_set_simple), &server);
  g_signal_connect_swapped (server.generator, "generator-set-extended",
                            G_CALLBACK (generator_set_extended), &server);
  g_signal_connect_swapped (server.generator, "generator-set-enable",
                            G_CALLBACK (generator_set_enable), &server);

  g_signal_connect_swapped (server.tvg, "tvg-set-auto",
                            G_CALLBACK (tvg_set_auto), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-constant",
                            G_CALLBACK (tvg_set_constant), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-linear-db",
                            G_CALLBACK (tvg_set_linear_db), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-logarithmic",
                            G_CALLBACK (tvg_set_logarithmic), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-enable",
                            G_CALLBACK (tvg_set_enable), &server);

  g_signal_connect_swapped (server.sonar, "sonar-set-sync-type",
                            G_CALLBACK (sonar_set_sync_type), &server);
  g_signal_connect_swapped (server.sonar, "sonar-set-position",
                            G_CALLBACK (sonar_set_position), &server);
  g_signal_connect_swapped (server.sonar, "sonar-set-receive-time",
                            G_CALLBACK (sonar_set_receive_time), &server);
  g_signal_connect_swapped (server.sonar, "sonar-start",
                            G_CALLBACK (sonar_start), &server);
  g_signal_connect_swapped (server.sonar, "sonar-stop",
                            G_CALLBACK (sonar_stop), &server);
  g_signal_connect_swapped (server.sonar, "sonar-ping",
                            G_CALLBACK (sonar_ping), &server);
}

/* Входная точка в тесты. */
static gboolean
test_entry (gpointer udata)
{
  HyScanSourceType source_type;
  gchar *port_name = "";
  gdouble min_val, max_val;

  memset(&frames, 0, sizeof(Frames));

  /*
   * Тестирование датчиков.
   */

  switch (g_random_int_range (0, 3))
    {
    case 0:
      port_name = random_virtual_port_name ();
      /* SENSOR: SET VIRTUAL PORT PARAM */
      frames.sensor_virtual_port_param_frame.test = TRUE;
      frames.sensor_virtual_port_param_frame.name = port_name;
      frames.sensor_virtual_port_param_frame.channel = (guint) g_random_int_range (1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS);
      frames.sensor_virtual_port_param_frame.time_offset = g_random_int () % G_MAXUINT16;

      hyscan_sonar_model_sensor_set_virtual_params (sonar_model,
                                                    frames.sensor_virtual_port_param_frame.name,
                                                    frames.sensor_virtual_port_param_frame.channel,
                                                    frames.sensor_virtual_port_param_frame.time_offset);
      break;

    case 1:
      port_name = random_uart_port_name ();
      /* SENSOR: SET UART PORT PARAM */
      frames.sensor_uart_port_param_frame.test = TRUE;
      frames.sensor_uart_port_param_frame.name = port_name;
      frames.sensor_uart_port_param_frame.channel = (guint) g_random_int_range (1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS);
      frames.sensor_uart_port_param_frame.time_offset = g_random_int () % G_MAXUINT16;;
      frames.sensor_uart_port_param_frame.protocol = g_random_boolean () ?
                                                     HYSCAN_SENSOR_PROTOCOL_NMEA_0183 :
                                                     HYSCAN_SENSOR_PROTOCOL_SAS;
      frames.sensor_uart_port_param_frame.uart_device =
          (guint) random_uart_device (frames.sensor_uart_port_param_frame.name);
      frames.sensor_uart_port_param_frame.uart_mode = (guint)random_uart_mode (frames.sensor_uart_port_param_frame.name);

      hyscan_sonar_model_sensor_set_uart_params (sonar_model,
                                                 frames.sensor_uart_port_param_frame.name,
                                                 frames.sensor_uart_port_param_frame.channel,
                                                 frames.sensor_uart_port_param_frame.time_offset,
                                                 frames.sensor_uart_port_param_frame.protocol,
                                                 frames.sensor_uart_port_param_frame.uart_device,
                                                 frames.sensor_uart_port_param_frame.uart_mode);
      break;

    case 2:
      port_name = random_udp_ip_port_name ();
      /* SENSOR: SET UDP/IP PORT PARAM */
      frames.sensor_udp_ip_port_param_frame.test = TRUE;
      frames.sensor_udp_ip_port_param_frame.name = port_name;
      frames.sensor_udp_ip_port_param_frame.channel = (guint) g_random_int_range (1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS);
      frames.sensor_udp_ip_port_param_frame.time_offset = g_random_int () % G_MAXUINT16;;
      frames.sensor_udp_ip_port_param_frame.protocol = g_random_boolean () ?
                                                       HYSCAN_SENSOR_PROTOCOL_NMEA_0183 :
                                                       HYSCAN_SENSOR_PROTOCOL_SAS;
      frames.sensor_udp_ip_port_param_frame.ip_address =
          (guint) random_ip_addres (frames.sensor_udp_ip_port_param_frame.name);
      frames.sensor_udp_ip_port_param_frame.udp_port = (guint) g_random_int_range (1024, G_MAXUINT16);

      hyscan_sonar_model_sensor_set_udp_ip_params (sonar_model,
                                                   frames.sensor_udp_ip_port_param_frame.name,
                                                   frames.sensor_udp_ip_port_param_frame.channel,
                                                   frames.sensor_udp_ip_port_param_frame.time_offset,
                                                   frames.sensor_udp_ip_port_param_frame.protocol,
                                                   frames.sensor_udp_ip_port_param_frame.ip_address,
                                                   (guint16)frames.sensor_udp_ip_port_param_frame.udp_port);
      break;
    }

  /* SENSOR: SET ENABLE */
  frames.sensor_set_enable_frame.test = TRUE;
  frames.sensor_set_enable_frame.name = port_name;
  frames.sensor_set_enable_frame.enable = g_random_boolean ();

  hyscan_sonar_model_sensor_set_enable (sonar_model,
                                        frames.sensor_set_enable_frame.name,
                                        frames.sensor_set_enable_frame.enable);

  /* SENSOR: SET POSITION */
  frames.sensor_set_position_frame.test = TRUE;
  frames.sensor_set_position_frame.name = random_port_name ();
  frames.sensor_set_position_frame.position = random_antenna_position ();

  hyscan_sonar_model_sensor_set_position (sonar_model,
                                          frames.sensor_set_position_frame.name,
                                          frames.sensor_set_position_frame.position);

  /*
   * Тестирование источников.
   */

  source_type = random_source_type ();

  switch (g_random_int_range (0, 4))
    {
    case 0:
      /* GENERATOR: SET PRESET */
      frames.generator_set_preset_frame.test = TRUE;
      frames.generator_set_preset_frame.source = source_type;
      frames.generator_set_preset_frame.preset = (guint) random_preset (frames.generator_set_preset_frame.source);

      hyscan_sonar_model_gen_set_preset (sonar_model,
                                         frames.generator_set_preset_frame.source,
                                         (guint) frames.generator_set_preset_frame.preset);
      break;

    case 1:
      /* GENERATOR: SET AUTO */
      frames.generator_set_auto_frame.test = TRUE;
      frames.generator_set_auto_frame.source = source_type;
      frames.generator_set_auto_frame.signal = random_signal ();

      hyscan_sonar_model_gen_set_auto (sonar_model,
                                       frames.generator_set_auto_frame.source,
                                       frames.generator_set_auto_frame.signal);
      break;

    case 2:
      /* GENERATOR: SET SIMPLE */
      frames.generator_set_simple_frame.test = TRUE;
      frames.generator_set_simple_frame.source = source_type;
      frames.generator_set_simple_frame.signal = random_signal ();
      frames.generator_set_simple_frame.power = g_random_double ();

      hyscan_sonar_model_gen_set_simple (sonar_model,
                                         frames.generator_set_simple_frame.source,
                                         frames.generator_set_simple_frame.signal,
                                         frames.generator_set_simple_frame.power);
      break;

    case 3:
      /* GENERATOR: SET EXTENDED */
      frames.generator_set_extended_frame.test = TRUE;
      frames.generator_set_extended_frame.source = source_type;
      frames.generator_set_extended_frame.signal = random_signal_full (HYSCAN_GENERATOR_SIGNAL_LFM
                                                                       | HYSCAN_GENERATOR_SIGNAL_LFMD
                                                                       | HYSCAN_GENERATOR_SIGNAL_TONE);
      hyscan_generator_control_get_duration_range (HYSCAN_GENERATOR_CONTROL (sonar_control),
                                                   frames.generator_set_extended_frame.source,
                                                   frames.generator_set_extended_frame.signal,
                                                   &min_val, &max_val);
      frames.generator_set_extended_frame.duration = g_random_double_range (min_val, max_val);
      frames.generator_set_extended_frame.power = g_random_double ();

      hyscan_sonar_model_gen_set_extended (sonar_model,
                                           frames.generator_set_extended_frame.source,
                                           frames.generator_set_extended_frame.signal,
                                           frames.generator_set_extended_frame.duration,
                                           frames.generator_set_extended_frame.power);
      break;
    }

  /* GENERATOR: SET ENABLE */
  frames.generator_set_enable_frame.test = TRUE;
  frames.generator_set_enable_frame.source = source_type;
  frames.generator_set_enable_frame.enable = g_random_boolean ();

  hyscan_sonar_model_gen_set_enable (sonar_model,
                                     frames.generator_set_enable_frame.source,
                                     frames.generator_set_enable_frame.enable);

  switch (g_random_int_range (0, 4))
    {
    case 0:
        /* TVG: SET AUTO */
      frames.tvg_set_auto_frame.test = TRUE;
      frames.tvg_set_auto_frame.source = source_type;
      if (g_random_boolean ())
        {
          frames.tvg_set_auto_frame.sensitivity = -1.0;
          frames.tvg_set_auto_frame.level = -1.0;
        }
      else
        {
          frames.tvg_set_auto_frame.sensitivity = g_random_double ();
          frames.tvg_set_auto_frame.level = g_random_double ();
        }

      hyscan_sonar_model_tvg_set_auto (sonar_model,
                                       frames.tvg_set_auto_frame.source,
                                       frames.tvg_set_auto_frame.level,
                                       frames.tvg_set_auto_frame.sensitivity);
      break;

    case 1:
      /* TVG: SET CONSTANT */
      frames.tvg_set_constant_frame.test = TRUE;
      frames.tvg_set_constant_frame.source = source_type;
      hyscan_tvg_control_get_gain_range (HYSCAN_TVG_CONTROL (sonar_control),
                                         frames.tvg_set_constant_frame.source,
                                         &min_val, &max_val);
      frames.tvg_set_constant_frame.gain = g_random_double_range (min_val, max_val);

      hyscan_sonar_model_tvg_set_constant (sonar_model,
                                           frames.tvg_set_constant_frame.source,
                                           frames.tvg_set_constant_frame.gain);
      break;

    case 2:
      /* TVG: SET LINEAR DB */
      frames.tvg_set_linear_db_frame.test = TRUE;
      frames.tvg_set_linear_db_frame.source = source_type;
      hyscan_tvg_control_get_gain_range (HYSCAN_TVG_CONTROL (sonar_control),
                                         frames.tvg_set_linear_db_frame.source,
                                         &min_val, &max_val);
      frames.tvg_set_linear_db_frame.gain0 = g_random_double_range (min_val, max_val);
      frames.tvg_set_linear_db_frame.step = (max_val - min_val) / g_random_double_range (5.0, 10.0);

      hyscan_sonar_model_tvg_set_linear_db (sonar_model,
                                            frames.tvg_set_linear_db_frame.source,
                                            frames.tvg_set_linear_db_frame.gain0,
                                            frames.tvg_set_linear_db_frame.step);
      break;

    case 3:
      /* TVG: SET LOGARITHMIC */
      frames.tvg_set_logarithmic_frame.test = TRUE;
      frames.tvg_set_logarithmic_frame.source = source_type;
      hyscan_tvg_control_get_gain_range (HYSCAN_TVG_CONTROL (sonar_control),
                                         frames.tvg_set_logarithmic_frame.source,
                                         &min_val, &max_val);
      frames.tvg_set_logarithmic_frame.gain0 = g_random_double_range (min_val, max_val);
      frames.tvg_set_logarithmic_frame.beta = g_random_double ();
      frames.tvg_set_logarithmic_frame.alpha = g_random_double ();

      hyscan_sonar_model_tvg_set_logarithmic (sonar_model,
                                              frames.tvg_set_logarithmic_frame.source,
                                              frames.tvg_set_logarithmic_frame.gain0,
                                              frames.tvg_set_logarithmic_frame.beta,
                                              frames.tvg_set_logarithmic_frame.alpha);
      break;
    }

  /* TVG: SET ENABLE */
  frames.tvg_set_enable_frame.test = TRUE;
  frames.tvg_set_enable_frame.source = source_type;
  frames.tvg_set_enable_frame.enable = g_random_boolean ();

  hyscan_sonar_model_tvg_set_enable (sonar_model,
                                     frames.tvg_set_enable_frame.source,
                                     frames.tvg_set_enable_frame.enable);

  /* SONAR: SET RECEIVE TIME */
  frames.sonar_set_receive_time_frame.test = TRUE;
  frames.sonar_set_receive_time_frame.source = source_type;
  max_val = hyscan_sonar_control_get_max_receive_time (sonar_control, frames.sonar_set_receive_time_frame.source);
  frames.sonar_set_receive_time_frame.receive_time = g_random_double_range (0.0, max_val);

  hyscan_sonar_model_set_receive_time (sonar_model,
                                       frames.sonar_set_receive_time_frame.source,
                                       frames.sonar_set_receive_time_frame.receive_time);

  /* SONAR: SET POSITION */
  frames.sonar_set_position_frame.test = TRUE;
  frames.sonar_set_position_frame.source = source_type;
  frames.sonar_set_position_frame.position = random_antenna_position ();

  hyscan_sonar_model_sonar_set_position (sonar_model,
                                         frames.sonar_set_position_frame.source,
                                         frames.sonar_set_position_frame.position);

  /* SONAR: SET SYNC */
  frames.sonar_set_sync_type_frame.test = TRUE;
  frames.sonar_set_sync_type_frame.sync_type = random_sync ();

  hyscan_sonar_model_set_sync_type (sonar_model,
                                    frames.sonar_set_sync_type_frame.sync_type);

  /* SONAR: START */
  frames.sonar_start_frame.test = TRUE;
  frames.sonar_start_frame.track_type = random_track_type ();
  hyscan_sonar_model_set_track_type (sonar_model, frames.sonar_start_frame.track_type);
  hyscan_sonar_model_sonar_start (sonar_model);

  return G_SOURCE_REMOVE;
}

int main (int argc, char **argv)
{
  g_random_set_seed ((guint32) (g_get_monotonic_time () % G_MAXUINT32));

  /* Инициализация виртуального гидролокатора. */
  sonar_box = create_sonar ();
  init_servers (sonar_box);

  /* Управление виртуальным гидролокатором. */
  sonar_control = hyscan_sonar_control_new (HYSCAN_PARAM (sonar_box), 0, 0, NULL);

  /* Асинхронное управление гидролокатором. */
  sonar_model = g_object_new (HYSCAN_TYPE_SONAR_MODEL,
                              "sonar-control", sonar_control,
                              NULL);
  g_signal_connect (sonar_model, "sonar-params-updated", G_CALLBACK (on_sonar_model_params_updated), NULL);

  /* Создание MainLoop. */
  main_loop = g_main_loop_new (NULL, TRUE);

  n_test_repeats = 0;
  g_idle_add (test_entry, NULL);

  g_main_loop_run (main_loop);

  /* MainLoop завершен. Освобождение занятых ресурсов. */
  g_main_loop_unref (main_loop);
  g_object_unref (sonar_model);
  g_object_unref (sonar_control);

  g_object_unref (server.sensor);
  g_object_unref (server.generator);
  g_object_unref (server.tvg);
  g_object_unref (server.sonar);

  g_object_unref (sonar_box);

  {
    guint i, j;

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        gchar *name = g_strdup_printf ("uart.%d", i + 1);
        UARTPortInfo *port = g_hash_table_lookup (ports, name);

        for (j = 0; j <= SENSOR_N_UART_DEVICES; j++)
          g_free (port->uart_device_names[j]);
        for (j = 0; j <= SENSOR_N_UART_MODES; j++)
          g_free (port->uart_mode_names[j]);

        g_free (name);
      }

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        gchar *name = g_strdup_printf ("udp.%d", i + 1);
        UDPIPPortInfo *port = g_hash_table_lookup (ports, name);

        for (j = 0; j <= SENSOR_N_IP_ADDRESSES; j++)
          g_free (port->ip_address_names[j]);

        g_free (name);
      }

    g_hash_table_unref (ports);

    for (i = 0; i < SONAR_N_SOURCES; i++)
      {
        HyScanSourceType source = source_type_by_index (i);
        SourceInfo *info = source_info_by_source_type (source);

        for (j = 0; j <= GENERATOR_N_PRESETS; j++)
          g_free (info->generator.preset_names[j]);
      }
  }

  xmlCleanupParser ();

  return test_result;
}
