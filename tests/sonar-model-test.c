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

enum
{
  TEST_START,
  TEST_SET,
  TEST_STOP
};

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

typedef struct
{
  gchar                     *name;
  guint                      channel;
  gint64                     time_offset;

  gboolean                   result;
} SensorVirtualPortParamFrame;

typedef struct
{
  gchar                     *name;
  guint                      channel;
  gint64                     time_offset;
  HyScanSensorProtocolType   protocol;
  guint                      uart_device;
  guint                      uart_mode;

  gboolean                   result;
} SensorUartPortParamFrame;

typedef struct
{
  gchar                     *name;
  guint                      channel;
  gint64                     time_offset;
  HyScanSensorProtocolType   protocol;
  guint                      ip_address;
  guint                      udp_port;

  gboolean                   result;
} SensorUdpIpPortParamFrame;

typedef struct
{
  gchar                    *name;
  HyScanAntennaPosition     position;

  gboolean                  result;
} SensorSetPositionFrame;

typedef struct
{
  gchar                    *name;
  gboolean                  enable;

  gboolean                  result;
} SensorSetEnableFrame;

typedef struct
{
  HyScanSourceType          source;
  guint                     preset;

  gboolean                  result;
} GeneratorSetPresetFrame;

typedef struct
{
  HyScanSourceType          source;
  HyScanGeneratorSignalType signal_type;

  gboolean                  result;
} GeneratorSetAutoFrame;

typedef struct
{
  HyScanSourceType          source;
  HyScanGeneratorSignalType signal_type;
  gdouble                   power;

  gboolean                  result;
} GeneratorSetSimpleFrame;

typedef struct
{
  HyScanSourceType          source;
  HyScanGeneratorSignalType signal_type;
  gdouble                   duration;
  gdouble                   power;

  gboolean                  result;
} GeneratorSetExtendedFrame;

typedef struct
{
  HyScanSourceType          source;
  gboolean                  enable;

  gboolean                  result;
} GeneratorSetEnableFrame;

typedef struct
{
  HyScanSourceType source;
  gdouble                   level;
  gdouble                   sensitivity;

  gboolean                  result;
} TvgSetAutoFrame;

typedef struct
{
  HyScanSourceType          source;
  gdouble                   gain;

  gboolean                  result;
} TvgSetConstantFrame;

typedef struct
{
  HyScanSourceType          source;
  gdouble                   gain0;
  gdouble                   step;

  gboolean                  result;
} TvgSetLinearDBFrame;

typedef struct
{
  HyScanSourceType          source;
  gdouble                   gain0;
  gdouble                   beta;
  gdouble                   alpha;

  gboolean                  result;
} TvgSetLogarithmicFrame;

typedef struct
{
  HyScanSourceType          source;
  gboolean                  enable;

  gboolean                  result;
} TvgSetEnableFrame;

typedef struct
{
  HyScanSourceType          source;
  HyScanAntennaPosition     position;

  gboolean                  result;
} SonarSetPositionFrame;

typedef struct
{
  HyScanSourceType          source;
  gdouble                   receive_time;

  gboolean                  result;
} SonarSetReceiveTimeFrame;

typedef struct
{
  HyScanSonarSyncType       sync_type;
  gboolean                  result;
} SonarSetSyncTypeFrame;

typedef struct
{
  gchar                    *track_name;
  HyScanTrackType           track_type;
  gboolean                  result;
} SonarStartFrame;

typedef struct
{
  gboolean                  result;
} SonarStopFrame;

typedef struct
{  gboolean                 result;
} SonarPingFrame;

/* Параметры датчика. */
typedef struct
{
  SensorVirtualPortParamFrame virtual_port_frame;
  SensorUartPortParamFrame    uart_port_frame;
  SensorUdpIpPortParamFrame   udp_port_frame;
  SensorSetPositionFrame      set_position_frame;
  SensorSetEnableFrame        set_enable_frame;
} SensorFrames;

/* Параметры источника данных. */
typedef struct
{
  GeneratorSetPresetFrame    gen_set_preset_frame;
  GeneratorSetAutoFrame      gen_set_auto_frame;
  GeneratorSetSimpleFrame    gen_set_simple_frame;
  GeneratorSetExtendedFrame  gen_set_extended_frame;
  GeneratorSetEnableFrame    gen_set_enable_frame;
  TvgSetAutoFrame            tvg_set_auto_frame;
  TvgSetConstantFrame        tvg_set_constant_frame;
  TvgSetLinearDBFrame        tvg_set_linear_db_frame;
  TvgSetLogarithmicFrame     tvg_set_logarithmic_frame;
  TvgSetEnableFrame          tvg_set_enable_frame;
  SonarSetPositionFrame      set_position_frame;
  SonarSetReceiveTimeFrame   set_receive_time_frame;
} SourceFrames;

typedef struct
{
  SonarSetSyncTypeFrame sync_type_frame;
  SonarStartFrame       start_frame;
  SonarStopFrame        stop_frame;
  SonarPingFrame        ping_frame;
} SonarFrames;

/* Данные приложения. */
typedef struct
{
  int                 test_id;           /* Идентификатор теста. */
  int                 result;            /* Результат тестирования. */

  ServerInfo          server;            /* Сервер. */

  GMainLoop          *main_loop;         /* MainLoop. */
  
  GHashTable         *sensors_frames;    /* Хеш-таблица параметров датчиков. */
  GHashTable         *sources_frames;    /* Хеш-таблица параметров источников данных. */
  SonarFrames         sonar_frames;      /* Параметры гидролокатора. */

  HyScanSonarModel   *sonar_model;       /* Модель локатора */
  HyScanSonarControl *sonar_control;     /* Интерфейс управления локтаором. */

  GHashTable         *ports;
  SourceInfo          starboard;
  SourceInfo          port;
  SourceInfo          echosounder;
  SonarInfo           sonar_info;
} ApplicationData;

static gboolean                   pos_equals                     (const HyScanAntennaPosition    *p1,
                                                                  const HyScanAntennaPosition    *p2);

static HyScanSourceType           source_type_by_index           (guint                           index);

static SourceInfo*                source_info_by_source_type     (ApplicationData                *app_data,
                                                                  HyScanSourceType                type);

static gint                       random_uart_mode               (GHashTable                     *ports,
                                                                  const gchar                    *name);
static gint                       random_uart_device             (GHashTable                     *ports,
                                                                  const gchar                    *name);
static gint                       random_ip_addres               (GHashTable                     *ports,
                                                                  const gchar                    *name);

static HyScanAntennaPosition      random_antenna_position        (void);
static gint                       random_preset                  (SourceInfo                     *source_info);
static HyScanGeneratorSignalType  random_signal_type             (void);
static HyScanGeneratorSignalType  random_accepted_signal_type    (HyScanGeneratorSignalType       accepted_signals);
static HyScanSonarSyncType        random_sync                    (void);
static HyScanTrackType            random_track_type              (void);

/*
 * Функции обратного вызова для серверов SENSOR, GENERATOR, TVG, SONAR.
 */

static gboolean sensor_virtual_port_param   (GHashTable                 *sensors_frames,
                                             const gchar                *name,
                                             guint                       channel,
                                             gint64                      time_offset);

static gboolean sensor_uart_port_param      (GHashTable                 *sensors_frames,
                                             const gchar                *name,
                                             guint                       channel,
                                             gint64                      time_offset,
                                             HyScanSensorProtocolType    protocol,
                                             guint                       uart_device,
                                             guint                       uart_mode);

static gboolean sensor_udp_ip_port_param    (GHashTable                 *sensors_frames,
                                             const gchar                *name,
                                             guint                       channel,
                                             gint64                      time_offset,
                                             HyScanSensorProtocolType    protocol,
                                             guint                       ip_address,
                                             guint                       udp_port);

static gboolean sensor_set_position         (GHashTable                 *sensors_frames,
                                             const gchar                *name,
                                             HyScanAntennaPosition      *position);

static gboolean sensor_set_enable           (GHashTable                 *sensors_frames,
                                             const gchar                *name,
                                             gboolean                    enable);

static gboolean generator_set_preset        (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             guint                       preset);

static gboolean generator_set_auto          (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             HyScanGeneratorSignalType   signal_type);

static gboolean generator_set_simple        (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             HyScanGeneratorSignalType   signal_type,
                                             gdouble                     power);

static gboolean generator_set_extended      (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             HyScanGeneratorSignalType   signal_type,
                                             gdouble                     duration,
                                             gdouble                     power);

static gboolean generator_set_enable        (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gboolean                    enable);

static gboolean tvg_set_auto                (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gdouble                     level,
                                             gdouble                     sensitivity);

static gboolean tvg_set_constant            (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gdouble                     gain);

static gboolean tvg_set_linear_db           (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gdouble                     gain0,
                                             gdouble                     step);

static gboolean tvg_set_logarithmic         (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gdouble                     gain0,
                                             gdouble                     beta,
                                             gdouble                     alpha);

static gboolean tvg_set_enable              (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gboolean                    enable);

static gboolean sonar_set_position          (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             HyScanAntennaPosition      *position);

static gboolean sonar_set_receive_time      (GHashTable                 *sources_frames,
                                             HyScanSourceType            source,
                                             gdouble                     receive_time);

static gboolean sonar_set_sync_type         (SonarFrames                *sonar_frames,
                                             HyScanSonarSyncType         sync_type);

static gboolean sonar_start                 (SonarFrames                *sonar_frames,
                                             const gchar                *track_name,
                                             HyScanTrackType             track_type);

static gboolean sonar_stop                  (SonarFrames                *sonar_frames);

static gboolean sonar_ping                  (SonarFrames                *sonar_frames);


static void             on_sonar_params_changed  (ApplicationData   *app_data,
                                                  gboolean           result,
                                                  HyScanSonarModel  *model);

static HyScanSonarBox*  make_mock_sonar          (ApplicationData   *app_data);

static void             init_servers             (ApplicationData   *app_data,
                                                  HyScanSonarBox    *mock_sonar);
static void             free_servers_data        (ApplicationData   *app_data);
static void             free_frames_data         (ApplicationData   *app_data);

static void             test                     (ApplicationData   *app_data);
static void             do_test                  (ApplicationData   *app_data,
                                                  int                test_id);
static void             complete_test            (ApplicationData   *app_data,
                                                  int                test_result);
static gboolean         test_source              (ApplicationData   *app_data);


/* Сравнивает структуры HyScanAntennaPosition. */
static gboolean
pos_equals (const HyScanAntennaPosition *p1,
            const HyScanAntennaPosition *p2)
{
  return p1->x == p2->x &&
         p1->y == p2->y &&
         p1->z == p2->z &&
         p1->gamma == p2->gamma &&
         p1->psi == p2->psi &&
         p1->theta == p2->theta;
}

/* Функция возвращает тип источника данных по его индексу. */
static HyScanSourceType
source_type_by_index (guint index)
{
  HyScanSourceType source_type;

  switch (index)
    {
    case 0:
      source_type = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
      break;

    case 1:
      source_type = HYSCAN_SOURCE_SIDE_SCAN_PORT;
      break;

    case 2:
      source_type = HYSCAN_SOURCE_ECHOSOUNDER;
      break;

    default:
      source_type = HYSCAN_SOURCE_INVALID;
    }

  return source_type;
}

/* Функция возвращает информацию об источнике данных по его типу. */
static SourceInfo *
source_info_by_source_type (ApplicationData *app_data,
                            HyScanSourceType type)
{
  SourceInfo *source_info;

  switch (type)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
      source_info = &app_data->starboard;
      break;

    case HYSCAN_SOURCE_SIDE_SCAN_PORT:
      source_info = &app_data->port;
      break;

    case HYSCAN_SOURCE_ECHOSOUNDER:
      source_info = &app_data->echosounder;
      break;

    default:
      source_info = NULL;
    }

  return source_info;
}

/* Возвращает случайный режим uart для указанного порта. */
static gint
random_uart_mode (GHashTable  *ports,
                  const gchar *name)
{
  UARTPortInfo *port = g_hash_table_lookup (ports, name);
  return port->uart_mode_ids[g_random_int_range (1, SENSOR_N_UART_MODES)];
}

/* Возвращает случайное uart-устройство для указанного порта. */
static gint
random_uart_device (GHashTable  *ports,
                    const gchar *name)
{
  UARTPortInfo *port = g_hash_table_lookup (ports, name);
  return port->uart_device_ids[g_random_int_range (1, SENSOR_N_UART_DEVICES)];
}

/* Возвращает случайный ip-адрес для указанного порта. */
static gint
random_ip_addres (GHashTable  *ports,
                  const gchar *name)
{
  UDPIPPortInfo *port = g_hash_table_lookup (ports, name);
  return port->ip_address_ids[g_random_int_range (1, SENSOR_N_IP_ADDRESSES)];
}

/* Возвращает случайное местоположение. */
static HyScanAntennaPosition
random_antenna_position (void)
{
  HyScanAntennaPosition p = {
    g_random_double (),
    g_random_double (),
    g_random_double (),
    g_random_double (),
    g_random_double (),
    g_random_double ()
  };
  return p;
}

/* Возвращает случайную преднастройку для указанного источника. */
static gint
random_preset (SourceInfo *source_info)
{
  return source_info->generator.preset_ids[g_random_int () % (GENERATOR_N_PRESETS - 1)];
}

/* Возвращает случайный сигнал. */
static HyScanGeneratorSignalType
random_signal_type (void)
{
  HyScanGeneratorSignalType signal_type;

  switch (g_random_int_range (0, 4))
    {
    case 0:
      signal_type = HYSCAN_GENERATOR_SIGNAL_AUTO;
      break;

    case 1:
      signal_type = HYSCAN_GENERATOR_SIGNAL_TONE;
      break;

    case 2:
      signal_type = HYSCAN_GENERATOR_SIGNAL_LFM;
      break;

    case 3:
      signal_type = HYSCAN_GENERATOR_SIGNAL_LFMD;
      break;

    default:
      signal_type = HYSCAN_GENERATOR_SIGNAL_INVALID;
    }

  return signal_type;
}

/* Возвращает случайный сигнал из указанных. */
static HyScanGeneratorSignalType
random_accepted_signal_type (HyScanGeneratorSignalType accepted_signals)
{
  HyScanGeneratorSignalType signal_type;

  if (accepted_signals == HYSCAN_GENERATOR_SIGNAL_INVALID)
    return HYSCAN_GENERATOR_SIGNAL_INVALID;

  do
    signal_type = random_signal_type ();
  while (!(signal_type & accepted_signals));

  return signal_type;
}

/* Возвращает случайный тип синхронизации. */
static HyScanSonarSyncType
random_sync (void)
{
  HyScanSonarSyncType sync_type;

  switch (g_random_int_range (0, 3))
    {
    case 0:
      sync_type = HYSCAN_SONAR_SYNC_INTERNAL;
      break;

    case 1:
      sync_type = HYSCAN_SONAR_SYNC_SOFTWARE;
      break;

    case 2:
      sync_type = HYSCAN_SONAR_SYNC_EXTERNAL;
      break;

    default:
      sync_type = HYSCAN_SONAR_SYNC_INVALID;
    }

  return sync_type;
}

/* Возвращает случайный тип галса. */
static HyScanTrackType
random_track_type (void)
{
  HyScanTrackType track_type;

  switch (g_random_int_range (0, 3))
    {
    case 0:
      track_type = HYSCAN_TRACK_TACK;
      break;

    case 1:
      track_type = HYSCAN_TRACK_SURVEY;
      break;

    case 2:
      track_type = HYSCAN_TRACK_CALIBRATION;
      break;

    default:
      track_type = HYSCAN_TRACK_UNSPECIFIED;
    }

  return track_type;
}

/* Функция изменяет параметры VIRTUAL порта. */
static gboolean
sensor_virtual_port_param (GHashTable  *sensors_frames,
                           const gchar *name,
                           guint        channel,
                           gint64       time_offset)
{
  SensorFrames *sensor_frames;
  SensorVirtualPortParamFrame *frame;

  /* Получение параметров для источника данных. */
  sensor_frames = g_hash_table_lookup (sensors_frames, name);
  if (sensor_frames == NULL)
    {
      g_warning ("sensor_virtual_port_param(): unknown sensor %s", name);
      return FALSE;
    }

  frame = &sensor_frames->virtual_port_frame;

  frame->result = !g_strcmp0 (name, frame->name) &&
                  channel == frame->channel &&
                  time_offset == frame->time_offset;

  return frame->result;
}

/* Функция изменяет параметры UART порта. */
static gboolean
sensor_uart_port_param (GHashTable                *sensors_frames,
                        const gchar               *name,
                        guint                      channel,
                        gint64                     time_offset,
                        HyScanSensorProtocolType   protocol,
                        guint                      uart_device,
                        guint                      uart_mode)
{
  SensorFrames *sensor_frames;
  SensorUartPortParamFrame *frame;

  /* Получение параметров для источника данных. */
  sensor_frames = g_hash_table_lookup (sensors_frames, name);
  if (sensor_frames == NULL)
    {
      g_warning ("sensor_uart_port_param(): unknown sensor %s", name);
      return FALSE;
    }

  frame = &sensor_frames->uart_port_frame;
  frame->result = !g_strcmp0 (name, frame->name) &&
                  channel == frame->channel &&
                  time_offset == frame->time_offset &&
                  protocol == frame->protocol &&
                  uart_device == frame->uart_device &&
                  uart_mode == frame->uart_mode;
  return frame->result;
}

/* Функция изменяет параметры UDP/IP порта. */
static gboolean
sensor_udp_ip_port_param (GHashTable                *sensors_frames,
                          const gchar               *name,
                          guint                      channel,
                          gint64                     time_offset,
                          HyScanSensorProtocolType   protocol,
                          guint                      ip_address,
                          guint                      udp_port)
{
  SensorFrames *sensor_frames;
  SensorUdpIpPortParamFrame *frame;

  /* Получение параметров для источника данных. */
  sensor_frames = g_hash_table_lookup (sensors_frames, name);
  if (sensor_frames == NULL)
    {
      g_warning ("sensor_udp_ip_port_param(): unknown sensor %s", name);
      return FALSE;
    }

  frame = &sensor_frames->udp_port_frame;
  frame->result  = !g_strcmp0 (name, frame->name) &&
                   channel == frame->channel &&
                   time_offset == frame->time_offset &&
                   protocol == frame->protocol &&
                   ip_address == frame->ip_address &&
                   udp_port == frame->udp_port;
  return frame->result;
}

/* Функция устанавливает местоположение приёмной антенны датчика. */
static gboolean
sensor_set_position (GHashTable            *sensors_frames,
                     const gchar           *name,
                     HyScanAntennaPosition *position)
{
  SensorFrames *sensor_frames;
  SensorSetPositionFrame *frame;

  /* Получение параметров для источника данных. */
  sensor_frames = g_hash_table_lookup (sensors_frames, name);
  if (sensor_frames == NULL)
    {
      g_warning ("sensor_set_position(): unknown sensor %s", name);
      return FALSE;
    }

  frame = &sensor_frames->set_position_frame;
  frame->result = !g_strcmp0 (name, frame->name) &&
           pos_equals (position, &frame->position);
  return frame->result;
}

/* Функция включает и выключает датчик. */
static gboolean
sensor_set_enable (GHashTable  *sensors_frames,
                   const gchar *name,
                   gboolean     enable)
{
  SensorFrames *sensor_frames;
  SensorSetEnableFrame *frame;

  /* Получение параметров для источника данных. */
  sensor_frames = g_hash_table_lookup (sensors_frames, name);
  if (sensor_frames == NULL)
    {
      g_warning ("sensor_set_enable(): unknown sensor %s", name);
      return FALSE;
    }

  frame = &sensor_frames->set_enable_frame;
  frame->result = !g_strcmp0 (name, frame->name) &&
                  enable == frame->enable;
  return frame->result;
}

/* Функция устанавливает режим работы генератора по преднастройкам. */
static gboolean
generator_set_preset (GHashTable       *sources_frames,
                      HyScanSourceType  source,
                      guint             preset)
{
  SourceFrames *source_frames;
  GeneratorSetPresetFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("generator_set_preset(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->gen_set_preset_frame;
  frame->result = source == frame->source &&
                  preset == frame->preset;
  return frame->result;
}

/* Функция устанавливает автоматический режим работы генератора. */
static gboolean
generator_set_auto (GHashTable                *sources_frames,
                    HyScanSourceType           source,
                    HyScanGeneratorSignalType  signal_type)
{
  SourceFrames *source_frames;
  GeneratorSetAutoFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("generator_set_auto(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->gen_set_auto_frame;
  frame->result = source == frame->source &&
                  signal_type == frame->signal_type;
  return frame->result;
}

/* Функция устанавливает упрощённый режим работы генератора. */
static gboolean
generator_set_simple (GHashTable                *sources_frames,
                      HyScanSourceType           source,
                      HyScanGeneratorSignalType  signal_type,
                      gdouble                    power)
{
  SourceFrames *source_frames;
  GeneratorSetSimpleFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("generator_set_simple(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->gen_set_simple_frame;
  frame->result = source == frame->source &&
                  signal_type == frame->signal_type &&
                  power == frame->power;
  return frame->result;
}

/* Функция устанавливает расширенный режим работы генератора. */
static gboolean
generator_set_extended (GHashTable                *sources_frames,
                        HyScanSourceType           source,
                        HyScanGeneratorSignalType  signal_type,
                        gdouble                    duration,
                        gdouble                    power)
{
  SourceFrames *source_frames;
  GeneratorSetExtendedFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("generator_set_extended(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->gen_set_extended_frame;
  frame->result = source == frame->source &&
                  signal_type == frame->signal_type &&
                  duration == frame->duration &&
                  power == frame->power;
  return frame->result;
}

/* Функция включает или отключает генератор. */
static gboolean
generator_set_enable (GHashTable       *sources_frames,
                      HyScanSourceType  source,
                      gboolean          enable)
{
  SourceFrames *source_frames;
  GeneratorSetEnableFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("generator_set_enable(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->gen_set_enable_frame;
  frame->result = source == frame->source &&
                  enable == frame->enable;
  return frame->result;
}

/* Функция устанавливает автоматический режим управления ВАРУ. */
static gboolean
tvg_set_auto (GHashTable       *sources_frames,
              HyScanSourceType  source,
              gdouble           level,
              gdouble           sensitivity)
{
  SourceFrames *source_frames;
  TvgSetAutoFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("tvg_set_auto(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->tvg_set_auto_frame;
  frame->result = source == frame->source &&
                  level == frame->level &&
                  sensitivity == frame->sensitivity;
  return frame->result;
}

/* Функция устанавливает постоянный уровень усиления ВАРУ. */
static gboolean
tvg_set_constant (GHashTable       *sources_frames,
                  HyScanSourceType  source,
                  gdouble           gain)
{
  SourceFrames *source_frames;
  TvgSetConstantFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("tvg_set_constant(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->tvg_set_constant_frame;
  frame->result = source == frame->source &&
                  gain == frame->gain;
  return frame->result;
}

/* Функция устанавливает линейный закон усиления ВАРУ. */
static gboolean
tvg_set_linear_db (GHashTable       *sources_frames,
                   HyScanSourceType  source,
                   gdouble           gain0,
                   gdouble           step)
{
  SourceFrames *source_frames;
  TvgSetLinearDBFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("tvg_set_linear_db(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->tvg_set_linear_db_frame;
  frame->result = source == frame->source &&
                  gain0 == frame->gain0 &&
                  step == frame->step;
  return frame->result;
}

/* Функция устанавливает логарифмический закон усиления ВАРУ. */
static gboolean
tvg_set_logarithmic (GHashTable       *sources_frames,
                     HyScanSourceType  source,
                     gdouble           gain0,
                     gdouble           beta,
                     gdouble           alpha)
{
  SourceFrames *source_frames;
  TvgSetLogarithmicFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("tvg_set_logarithmic(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->tvg_set_logarithmic_frame;
  frame->result = source == frame->source &&
                  gain0 == frame->gain0 &&
                  beta == frame->beta &&
                  alpha == frame->alpha;
  return frame->result;
}

/* Функция включает или отключает ВАРУ. */
static gboolean
tvg_set_enable (GHashTable       *sources_frames,
                HyScanSourceType  source,
                gboolean          enable)
{
  SourceFrames *source_frames;
  TvgSetEnableFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("tvg_set_enable(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->tvg_set_enable_frame;
  frame->result = source == frame->source &&
                  enable == frame->enable;
  return frame->result;
}

/* Функция устанавливает местоположение приёмной антенны гидролокатора. */
static gboolean
sonar_set_position (GHashTable            *sources_frames,
                    HyScanSourceType       source,
                    HyScanAntennaPosition *position)
{
  SourceFrames *source_frames;
  SonarSetPositionFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("sonar_set_position(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->set_position_frame;
  frame->result = source == frame->source &&
                  pos_equals (position, &frame->position);
  return frame->result;
}

/* Функция устанавливает время приёма эхосигнала бортом. */
static gboolean
sonar_set_receive_time (GHashTable       *sources_frames,
                        HyScanSourceType  source,
                        gdouble           receive_time)
{
  SourceFrames *source_frames;
  SonarSetReceiveTimeFrame *frame;

  /* Получение параметров для источника данных. */
  source_frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source));
  if (source_frames == NULL)
    {
      g_warning ("sonar_set_receive_time(): unknown source %d", source);
      return FALSE;
    }

  frame = &source_frames->set_receive_time_frame;
  frame->result = source == frame->source &&
                  receive_time == frame->receive_time;
  return frame->result;
}

/* Функция устанавливает тип синхронизации излучения. */
static gboolean
sonar_set_sync_type (SonarFrames         *sonar_frames,
                     HyScanSonarSyncType  sync_type)
{
  sonar_frames->sync_type_frame.result = sync_type == sonar_frames->sync_type_frame.sync_type;
  return sonar_frames->sync_type_frame.result;
}

/* Функция включает гидролокатор в работу. */
static gboolean
sonar_start (SonarFrames     *sonar_frames,
             const gchar     *track_name,
             HyScanTrackType  track_type)
{
  SonarStartFrame *frame;

  frame = &sonar_frames->start_frame;
  frame->result = !g_strcmp0 (track_name, frame->track_name) &&
                  track_type == frame->track_type;
  return frame->result;
}

/* Функция останавливает работу гидролокатора. */
static gboolean
sonar_stop (SonarFrames *sonar_frames)
{
  sonar_frames->stop_frame.result = TRUE;
  return TRUE;
}

/* Функция выполняет цикл зондирования. */
static gboolean
sonar_ping (SonarFrames *sonar_frames)
{
  sonar_frames->ping_frame.result = TRUE;
  return TRUE;
}

static gboolean
check_virtual_sensor (const gchar      *name,
                      SensorFrames     *frames,
                      HyScanSonarModel *sonar_model)
{
  guint channel;
  gint64 time_offset;
  SensorVirtualPortParamFrame *frame;

  if (!frames->set_enable_frame.enable)
    return TRUE;

  frame = &frames->virtual_port_frame;

  if (!frame->result)
    {
      g_message ("%s set params failed.", name);
      return FALSE;
    }

  hyscan_sonar_model_sensor_get_virtual_params (sonar_model, name, &channel, &time_offset);

  return channel == frame->channel &&
         time_offset == frame->time_offset;
}

static gboolean
check_uart_sensor (const gchar      *name,
                   SensorFrames     *frames,
                   HyScanSonarModel *sonar_model)
{
  guint channel;
  gint64 time_offset;
  HyScanSensorProtocolType protocol;
  guint uart_device;
  guint uart_mode;
  SensorUartPortParamFrame *frame;

  if (!frames->set_enable_frame.enable)
    return TRUE;

  frame = &frames->uart_port_frame;

  if (!frame->result)
    {
      g_message ("%s set params failed.", name);
      return FALSE;
    }

  hyscan_sonar_model_sensor_get_uart_params (sonar_model,
                                             name,
                                             &channel,
                                             &time_offset,
                                             &protocol,
                                             &uart_device,
                                             &uart_mode);

  return channel == frame->channel &&
         time_offset == frame->time_offset &&
         protocol == frame->protocol &&
         uart_device == frame->uart_device &&
         uart_mode == frame->uart_mode;
}

static gboolean
check_udp_sensor (const gchar      *name,
                  SensorFrames     *frames,
                  HyScanSonarModel *sonar_model)
{
  guint channel;
  gint64 time_offset;
  HyScanSensorProtocolType protocol;
  guint ip_address;
  guint16 udp_port;
  SensorUdpIpPortParamFrame *frame;

  if (!frames->set_enable_frame.enable)
    return TRUE;

  frame = &frames->udp_port_frame;

  if (!frame->result)
    {
      g_message ("%s set params failed.", name);
      return FALSE;
    }

  hyscan_sonar_model_sensor_get_udp_ip_params (sonar_model,
                                               name,
                                               &channel,
                                               &time_offset,
                                               &protocol,
                                               &ip_address,
                                               &udp_port);

  return channel == frame->channel &&
         time_offset == frame->time_offset &&
         protocol == frame->protocol &&
         ip_address == frame->ip_address &&
         udp_port == frame->udp_port;
}

static gboolean
check_sensor_params (const gchar      *name,
                     SensorFrames     *frames,
                     HyScanSonarModel *sonar_model)
{
  HyScanAntennaPosition *pos;
  gboolean result = TRUE;

  if (!frames->set_enable_frame.result)
    {
      g_message ("Sensor %s enable failed", name);
      result = FALSE;
      goto exit;
    }

  if (!frames->set_position_frame.result)
    {
      g_message ("Sensor %s position failed", name);
      result = FALSE;
      goto exit;
    }

  if (hyscan_sonar_model_sensor_is_enabled (sonar_model, name) != frames->set_enable_frame.enable)
    {
      g_message ("Sensor %s enable mismatch", name);
      result = FALSE;
      goto exit;
    }

  pos = hyscan_sonar_model_sensor_get_position (sonar_model, name);
  if (pos == NULL)
    {
      g_message ("Sensor %s position is NULL", name);
      result = FALSE;
      goto exit;
    }

  result = pos_equals (pos, &frames->set_position_frame.position);
  g_free (pos);
  
  if (!result)
    g_message ("Sensor %s position mismatch", name);

exit:
  return result;
}

static gboolean
check_tvg_enable (HyScanSonarModel *sonar_model,
                  HyScanSourceType  source_type,
                  SourceFrames     *frames)
{
  TvgSetEnableFrame *frame;
  gdouble enabled;

  frame = &frames->tvg_set_enable_frame;

  if (!frame->result)
    {
      g_message ("source %d: tvg enable failed.", source_type);
      return FALSE;
    }

  enabled = hyscan_sonar_model_tvg_is_enabled (sonar_model, source_type);

  return enabled == frame->enable;
}

static gboolean
check_tvg_auto (HyScanSonarModel *sonar_model,
                HyScanSourceType  source_type,
                SourceFrames     *frames)
{
  TvgSetAutoFrame *frame;
  gdouble level, sensitivity;

  if (!frames->tvg_set_enable_frame.enable)
    return TRUE;

  frame = &frames->tvg_set_auto_frame;

  if (!frame->result)
    {
      g_message ("source %d: tvg auto failed.", source_type);
      return FALSE;
    }

  hyscan_sonar_model_tvg_get_auto_params (sonar_model, source_type, &level, &sensitivity);

  return level == frame->level &&
         sensitivity == frame->sensitivity;
}

static gboolean
check_tvg_constant (HyScanSonarModel *sonar_model,
                    HyScanSourceType  source_type,
                    SourceFrames     *frames)
{
  TvgSetConstantFrame *frame;
  double gain;

  if (!frames->tvg_set_enable_frame.enable)
    return TRUE;

  frame = &frames->tvg_set_constant_frame;

  if (!frame->result)
    {
      g_message ("source %d: tvg constant failed.", source_type);
      return FALSE;
    }

  hyscan_sonar_model_tvg_get_const_params (sonar_model, source_type, &gain);

  return gain == frame->gain;
}

static gboolean
check_tvg_linear_db (HyScanSonarModel *sonar_model,
                     HyScanSourceType  source_type,
                     SourceFrames     *frames)
{
  TvgSetLinearDBFrame *frame;
  gdouble gain0, step;

  if (!frames->tvg_set_enable_frame.enable)
    return TRUE;

  frame = &frames->tvg_set_linear_db_frame;

  if (!frame->result)
    {
      g_message ("source %d: tvg linear db failed.", source_type);
      return FALSE;
    }

  hyscan_sonar_model_tvg_get_linear_db_params (sonar_model, source_type, &gain0, &step);

  return gain0 == frame->gain0 &&
         step == frame->step;
}

static gboolean
check_tvg_logarithmic (HyScanSonarModel *sonar_model,
                       HyScanSourceType  source_type,
                       SourceFrames     *frames)
{
  TvgSetLogarithmicFrame *frame;
  gdouble gain0, beta, alpha;

  if (!frames->tvg_set_enable_frame.enable)
    return TRUE;

  frame = &frames->tvg_set_logarithmic_frame;

  if (!frame->result)
    {
      g_message ("source %d: tvg logarigthmic failed.", source_type);
      return FALSE;
    }

  hyscan_sonar_model_tvg_get_logarithmic_params (sonar_model, source_type, &gain0, &beta, &alpha);

  return gain0 == frame->gain0 &&
         beta == frame->beta &&
         alpha == frame->alpha;
}

static gboolean
check_generator_enable (HyScanSonarModel *sonar_model,
                        HyScanSourceType  source_type,
                        SourceFrames     *frames)
{
  GeneratorSetEnableFrame *frame;
  gdouble enabled;

  frame = &frames->gen_set_enable_frame;

  if (!frame->result)
    {
      g_message ("source %d: generator enable failed.", source_type);
      return FALSE;
    }

  enabled = hyscan_sonar_model_gen_is_enabled (sonar_model, source_type);

  return enabled == frame->enable;
}

static gboolean
check_generator_preset (HyScanSonarModel *sonar_model,
                        HyScanSourceType  source_type,
                        SourceFrames     *frames)
{
  GeneratorSetPresetFrame *frame;
  guint preset;

  if (!frames->gen_set_enable_frame.enable)
    return TRUE;

  frame = &frames->gen_set_preset_frame;

  if (!frame->result)
    {
      g_message ("source %d: generator preset failed.", source_type);
      return FALSE;
    }

  hyscan_sonar_model_gen_get_preset_params (sonar_model, source_type, &preset);

  return preset == frame->preset;
}

static gboolean
check_generator_auto (HyScanSonarModel *sonar_model,
                      HyScanSourceType  source_type,
                      SourceFrames     *frames)
{
  HyScanGeneratorSignalType signal_type;
  GeneratorSetAutoFrame *frame;

  if (!frames->gen_set_enable_frame.enable)
    return TRUE;

  frame = &frames->gen_set_auto_frame;

  if (!frame->result)
    {
      g_message ("source %d: generator auto failed.", source_type);
      return FALSE;
    }

  hyscan_sonar_model_gen_get_auto_params (sonar_model, source_type, &signal_type);

  return signal_type == frame->signal_type;
}

static gboolean
check_generator_simple (HyScanSonarModel *sonar_model,
                        HyScanSourceType  source_type,
                        SourceFrames     *frames)
{
  HyScanGeneratorSignalType signal_type;
  gdouble power;
  GeneratorSetSimpleFrame *frame;

  if (!frames->gen_set_enable_frame.enable)
    return TRUE;

  frame = &frames->gen_set_simple_frame;

  if (!frame->result)
    {
      g_message ("source %d: generator extended failed.", source_type);
      return FALSE;
    }
  
  hyscan_sonar_model_gen_get_simple_params (sonar_model, source_type, &signal_type, &power);

  return signal_type == frame->signal_type &&
         power == frame->power;
}

static gboolean
check_generator_extended (HyScanSonarModel *sonar_model,
                          HyScanSourceType  source_type,
                          SourceFrames     *frames)
{
  HyScanGeneratorSignalType signal_type;
  gdouble power, duration;
  GeneratorSetExtendedFrame *frame;

  if (!frames->gen_set_enable_frame.enable)
    return TRUE;

  frame = &frames->gen_set_extended_frame;

  if (!frame->result)
    {
      g_message ("source %d: generator extended failed.", source_type);
      return FALSE;
    }
  
  hyscan_sonar_model_gen_get_extended_params (sonar_model, source_type, &signal_type, &duration, &power);

  return signal_type == frame->signal_type &&
         duration  == frame->duration &&
         power == frame->power;
}

static gboolean
check_source (HyScanSonarModel *sonar_model,
              HyScanSourceType  source_type,
              SourceFrames     *frames)
{
  HyScanAntennaPosition *pos;
  gboolean result = TRUE;

  if (!frames->set_receive_time_frame.result)
    {
      g_message ("Source %d receive time failed", source_type);
      result = FALSE;
      goto exit;
    }

  if (!frames->set_position_frame.result)
    {
      g_message ("Source %d position failed", source_type);
      result = FALSE;
      goto exit;
    }

  if (hyscan_sonar_model_get_receive_time (sonar_model, source_type) != frames->set_receive_time_frame.receive_time)
    {
      g_message ("Source %d receive time mismatch", source_type);
      result = FALSE;
      goto exit;
    }

  pos = hyscan_sonar_model_sonar_get_position (sonar_model, source_type);
  if (pos == NULL)
    {
      g_message ("Source %d position is NULL", source_type);
      result = FALSE;
      goto exit;
    }

  result = pos_equals (pos, &frames->set_position_frame.position);
  g_free (pos);
  
  if (!result)
    g_message ("Source %d position mismatch", source_type);

exit:
  return result;
}

static gboolean
check_sonar_sync_type (HyScanSonarModel      *sonar_model,
                       SonarSetSyncTypeFrame *frame)
{
  HyScanSonarSyncType sync_type;

  if (!frame->result)
    {
      g_message ("Sonar set sync type failed");
      return FALSE;
    }

  /* Тип синхронизации. */
  sync_type = hyscan_sonar_model_get_sync_type (sonar_model);

  return sync_type == frame->sync_type;
}

static gboolean
check_sonar_start (HyScanSonarModel *sonar_model,
                   SonarStartFrame  *frame)
{
  gboolean record_state;
  HyScanTrackType track_type;

  if (!frame->result)
    {
      g_message ("Sonar start failed");
      return FALSE;
    }

  /* Состояние локатора. */
  record_state = hyscan_sonar_model_get_record_state (sonar_model);
  /* Тип записываемого галса. */
  track_type = hyscan_sonar_model_get_track_type (sonar_model);

  return record_state &&
         track_type == frame->track_type;
}

static gboolean
check_test_start (ApplicationData *app_data)
{
  int i;
  HyScanSonarModel *sonar_model;
  GHashTable *sensors_frames, *sources_frames;
  gboolean result = TRUE;

  sonar_model = app_data->sonar_model;

  sensors_frames = app_data->sensors_frames;

  /* Проверка соответствия параметров датчиков. */
  for (i = 1; i <= SENSOR_N_PORTS; ++i)
    {
      SensorFrames *frames;
      gchar *name;

      name = g_strdup_printf ("virtual.%d", i);
      frames = g_hash_table_lookup (sensors_frames, name);
      if (!check_sensor_params (name, frames, sonar_model) ||
          !check_virtual_sensor (name, frames, sonar_model))
        {
          result = FALSE;
          g_free (name);
          goto exit;
        }
      g_free (name);

      name = g_strdup_printf ("uart.%d", i);
      frames = g_hash_table_lookup (sensors_frames, name);
      if (!check_sensor_params (name, frames, sonar_model) ||
          !check_uart_sensor (name, frames, sonar_model))
        {
          result = FALSE;
          g_free (name);
          goto exit;
        }
      g_free (name);

      name = g_strdup_printf ("udp.%d", i);
      frames = g_hash_table_lookup (sensors_frames, name);
      if (!check_sensor_params (name, frames, sonar_model) ||
          !check_udp_sensor (name, frames, sonar_model))
        {
          result = FALSE;
          g_free (name);
          goto exit;
        }
      g_free (name);
    }

  sources_frames = app_data->sources_frames;

  /* Проверка соответствия параметров источников данных. */
  for (i = 0; i < SONAR_N_SOURCES; ++i)
    {
      HyScanSourceType source_type;
      SourceFrames *frames;

      source_type = source_type_by_index (i);
      frames = g_hash_table_lookup (sources_frames, GINT_TO_POINTER (source_type));
      if (!check_source (sonar_model, source_type, frames))
        {
          result = FALSE;
          goto exit;
        }

      if (!check_tvg_enable (sonar_model, source_type, frames))
        {
          result = FALSE;
          goto exit;
        }
      if (!check_tvg_auto (sonar_model, source_type, frames))
        {
          result = FALSE;
          goto exit;
        }

      if (!check_generator_enable (sonar_model, source_type, frames))
        {
          result = FALSE;
          goto exit;
        }
      if (!check_generator_auto (sonar_model, source_type, frames))
        {
          result = FALSE;
          goto exit;
        }
    }

  /* Проверка соответствия параметров гидролокатора. */
  if (!check_sonar_sync_type (sonar_model, &app_data->sonar_frames.sync_type_frame))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_sonar_start (sonar_model, &app_data->sonar_frames.start_frame))
    {
      result = FALSE;
      goto exit;
    }
    
    g_message ("OK\n");

exit:
  return result;
}

static gboolean
check_test_set (ApplicationData  *app_data)
{
  HyScanSonarModel *sonar_model;
  HyScanSourceType source_type;
  SourceFrames *frames;
  gboolean result = TRUE;

  sonar_model = app_data->sonar_model;

  source_type = HYSCAN_SOURCE_ECHOSOUNDER;
  frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (source_type));
  if (!check_tvg_enable (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_tvg_constant (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }

  if (!check_generator_enable (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_generator_simple (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }

  source_type = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
  frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (source_type));
  if (!check_tvg_enable (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_tvg_linear_db (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }

  if (!check_generator_enable (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_generator_extended (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }

  source_type = HYSCAN_SOURCE_SIDE_SCAN_PORT;
  frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (source_type));
  if (!check_tvg_enable (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_tvg_logarithmic (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }

  if (!check_generator_enable (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }
  if (!check_generator_preset (sonar_model, source_type, frames))
    {
      result = FALSE;
      goto exit;
    }

  g_message ("OK\n");

exit:
  return result;
}

/* Проверяет результат теста останова локатора. */
static gboolean
check_test_stop (ApplicationData *app_data)
{
  gboolean record_state;
  gboolean result;

  /* Состояние локатора. */
  record_state = hyscan_sonar_model_get_record_state (app_data->sonar_model);
  /* Локатор выключен и фукнция останова локатора вызывалась. */
  result = !record_state && app_data->sonar_frames.stop_frame.result;

  if (result)
    g_message ("OK\n");

  return result;    
}

/* Асинхронное управление гидролокатором завершено. */
static void
on_sonar_params_changed (ApplicationData  *app_data,
                         gboolean          result,
                         HyScanSonarModel *model)
{
  (void) model;

  if (!result)
    {
      g_message ("Couldn't change sonar params.");
      complete_test (app_data, EXIT_FAILURE);
      return;
    }

  switch (app_data->test_id)
    {
    /* Проверка результатов теста запуска локатора. */
    case TEST_START:
      if (check_test_start (app_data))
        {
          do_test (app_data, TEST_SET);
        }
      else 
        {
          g_warning ("FAILED: sonar start test.");
          complete_test (app_data, EXIT_FAILURE);
        }
      break;

    /* Проверка результатов теста установки параметров локатора. */
    case TEST_SET:
      if (check_test_set (app_data))
        {
          do_test (app_data, TEST_STOP);
        }
      else 
        {
          g_warning ("FAILED: sonar set test.");
          complete_test (app_data, EXIT_FAILURE);
        }
      break;

    /* Проверка результатов теста останова локатора. */
    case TEST_STOP:
      {
        if (check_test_stop (app_data))
        {
          g_message ("All done.");
          complete_test (app_data, EXIT_SUCCESS);
        }
        else
        {
          g_warning ("FAILED: sonar stop test.");
          complete_test (app_data, EXIT_FAILURE);
        }
      }
      break;

    default:
      g_warning ("Invalid test id.");
      complete_test (app_data, EXIT_FAILURE);
    }
}

/* Создаёт виртуальный гидролокатор. */
static HyScanSonarBox *
make_mock_sonar (ApplicationData *app_data)
{
  HyScanSonarBox *sonar_box;
  HyScanSonarSchema *schema;
  gchar *schema_data;
  guint i, j;

  /* Параметры источников данных. */
  for (i = 0; i < SONAR_N_SOURCES; ++i)
    {
      SourceInfo *info;

      info = source_info_by_source_type (app_data, source_type_by_index (i));
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
      info->auto_receive_time = i % 2 ? TRUE : FALSE;

      info->signal_rate = g_random_double ();
      info->tvg_rate = g_random_double ();

      info->generator.capabilities = HYSCAN_GENERATOR_MODE_PRESET |
                                     HYSCAN_GENERATOR_MODE_AUTO |
                                     HYSCAN_GENERATOR_MODE_SIMPLE |
                                     HYSCAN_GENERATOR_MODE_EXTENDED;

      info->generator.signals = HYSCAN_GENERATOR_SIGNAL_AUTO |
                                HYSCAN_GENERATOR_SIGNAL_TONE |
                                HYSCAN_GENERATOR_SIGNAL_LFM |
                                HYSCAN_GENERATOR_SIGNAL_LFMD;

      info->generator.min_tone_duration = g_random_double ();
      info->generator.max_tone_duration = info->generator.min_tone_duration + g_random_double ();
      info->generator.min_lfm_duration = g_random_double ();
      info->generator.max_lfm_duration = info->generator.min_lfm_duration + g_random_double ();

      info->generator.enable = FALSE;
      info->generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
      info->generator.cur_signal = HYSCAN_GENERATOR_SIGNAL_AUTO;

      info->tvg.capabilities = HYSCAN_TVG_MODE_AUTO |
                               HYSCAN_TVG_MODE_CONSTANT |
                               HYSCAN_TVG_MODE_LINEAR_DB |
                               HYSCAN_TVG_MODE_LOGARITHMIC;

      info->tvg.min_gain = g_random_double ();
      info->tvg.max_gain = info->tvg.min_gain + g_random_double ();

      info->tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
    }

  /* Параметры гидролокатора по умолчанию. */
  memset (&app_data->sonar_info, 0, sizeof (SonarInfo));
  app_data->sonar_info.sync_capabilities = HYSCAN_SONAR_SYNC_INTERNAL | HYSCAN_SONAR_SYNC_EXTERNAL | HYSCAN_SONAR_SYNC_SOFTWARE;

  /* Схема гидролокатора. */
  schema = hyscan_sonar_schema_new (HYSCAN_SONAR_SCHEMA_DEFAULT_TIMEOUT);

  /* Порты для датчиков. */
  app_data->ports = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  {
    guint pow2;

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        VirtualPortInfo *port = g_new0 (VirtualPortInfo, 1);
        gchar *name = g_strdup_printf ("virtual.%d", i + 1);

        port->type = HYSCAN_SENSOR_PORT_VIRTUAL;
        g_hash_table_insert (app_data->ports, name, port);

        hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
      }

    for (i = 0; i < SENSOR_N_PORTS; i++)
      {
        UARTPortInfo *port = g_new0 (UARTPortInfo, 1);
        gchar *name = g_strdup_printf ("uart.%d", i + 1);

        port->type = HYSCAN_SENSOR_PORT_UART;
        g_hash_table_insert (app_data->ports, name, port);

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
        g_hash_table_insert (app_data->ports, name, port);

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
  hyscan_sonar_schema_sync_add (schema, (HyScanSonarSyncType) app_data->sonar_info.sync_capabilities);

  /* Источники данных. */
  for (i = 0; i < SONAR_N_SOURCES; i++)
    {
      HyScanSourceType source = source_type_by_index (i);
      SourceInfo *info = source_info_by_source_type (app_data, source);

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
init_servers (ApplicationData *app_data,
              HyScanSonarBox  *mock_sonar)
{
  HyScanSensorControlServer *sensor_server;
  HyScanGeneratorControlServer *generator_server;
  HyScanTVGControlServer *tvg_server;
  HyScanSonarControlServer *sonar_server;

  sensor_server = hyscan_sensor_control_server_new (mock_sonar);
  generator_server = hyscan_generator_control_server_new (mock_sonar);
  tvg_server = hyscan_tvg_control_server_new (mock_sonar);
  sonar_server = hyscan_sonar_control_server_new (mock_sonar);

  g_signal_connect_swapped (sensor_server,
                            "sensor-virtual-port-param",
                            G_CALLBACK (sensor_virtual_port_param),
                            app_data->sensors_frames);
  g_signal_connect_swapped (sensor_server,
                            "sensor-uart-port-param",
                            G_CALLBACK (sensor_uart_port_param),
                            app_data->sensors_frames);
  g_signal_connect_swapped (sensor_server,
                            "sensor-udp-ip-port-param",
                            G_CALLBACK (sensor_udp_ip_port_param),
                            app_data->sensors_frames);
  g_signal_connect_swapped (sensor_server,
                            "sensor-set-position",
                            G_CALLBACK (sensor_set_position),
                            app_data->sensors_frames);
  g_signal_connect_swapped (sensor_server,
                            "sensor-set-enable",
                            G_CALLBACK (sensor_set_enable),
                            app_data->sensors_frames);

  g_signal_connect_swapped (generator_server,
                            "generator-set-preset",
                            G_CALLBACK (generator_set_preset),
                            app_data->sources_frames);
  g_signal_connect_swapped (generator_server,
                            "generator-set-auto",
                            G_CALLBACK (generator_set_auto),
                            app_data->sources_frames);
  g_signal_connect_swapped (generator_server,
                            "generator-set-simple",
                            G_CALLBACK (generator_set_simple),
                            app_data->sources_frames);
  g_signal_connect_swapped (generator_server,
                            "generator-set-extended",
                            G_CALLBACK (generator_set_extended),
                            app_data->sources_frames);
  g_signal_connect_swapped (generator_server,
                            "generator-set-enable",
                            G_CALLBACK (generator_set_enable),
                            app_data->sources_frames);

  g_signal_connect_swapped (tvg_server,
                            "tvg-set-auto",
                            G_CALLBACK (tvg_set_auto),
                            app_data->sources_frames);
  g_signal_connect_swapped (tvg_server,
                            "tvg-set-constant",
                            G_CALLBACK (tvg_set_constant),
                            app_data->sources_frames);
  g_signal_connect_swapped (tvg_server,
                            "tvg-set-linear-db",
                            G_CALLBACK (tvg_set_linear_db),
                            app_data->sources_frames);
  g_signal_connect_swapped (tvg_server,
                            "tvg-set-logarithmic",
                            G_CALLBACK (tvg_set_logarithmic),
                            app_data->sources_frames);
  g_signal_connect_swapped (tvg_server,
                            "tvg-set-enable",
                            G_CALLBACK (tvg_set_enable),
                            app_data->sources_frames);

  g_signal_connect_swapped (sonar_server,
                            "sonar-set-position",
                            G_CALLBACK (sonar_set_position),
                            app_data->sources_frames);
  g_signal_connect_swapped (sonar_server,
                            "sonar-set-receive-time",
                            G_CALLBACK (sonar_set_receive_time),
                            app_data->sources_frames);
  g_signal_connect_swapped (sonar_server,
                            "sonar-set-sync-type",
                            G_CALLBACK (sonar_set_sync_type),
                            &app_data->sonar_frames);
  g_signal_connect_swapped (sonar_server,
                            "sonar-start",
                            G_CALLBACK (sonar_start),
                            &app_data->sonar_frames);
  g_signal_connect_swapped (sonar_server,
                            "sonar-stop",
                            G_CALLBACK (sonar_stop),
                            &app_data->sonar_frames);
  g_signal_connect_swapped (sonar_server,
                            "sonar-ping",
                            G_CALLBACK (sonar_ping),
                            &app_data->sonar_frames);

  app_data->server.sensor = sensor_server;
  app_data->server.generator = generator_server;
  app_data->server.tvg = tvg_server;
  app_data->server.sonar = sonar_server;
}

static void
test_sensor_set_position (HyScanSonarModel       *sonar_model,
                          const gchar            *name,
                          SensorSetPositionFrame *frame)
{
  frame->result = FALSE;
  frame->name = g_strdup (name);
  frame->position = random_antenna_position ();
  hyscan_sonar_model_sensor_set_position (sonar_model, frame->name, frame->position);
}

static void
test_sensor_set_enable (HyScanSonarModel     *sonar_model,
                        const gchar          *name,
                        SensorSetEnableFrame *frame)
{
  frame->result = FALSE;
  frame->name = g_strdup (name);
  frame->enable = g_random_boolean ();
  hyscan_sonar_model_sensor_set_enable (sonar_model, frame->name, frame->enable);
}

static void
test_sensor_set_virtual_params (HyScanSonarModel            *sonar_model,
                                const gchar                 *name,
                                SensorVirtualPortParamFrame *frame)
{
  frame->result = FALSE;
  frame->name = g_strdup (name);
  frame->channel = (guint) g_random_int_range (1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS);
  frame->time_offset = g_random_int () % G_MAXUINT16;
  hyscan_sonar_model_sensor_set_virtual_params (sonar_model, frame->name, frame->channel, frame->time_offset);
}

static void
test_sensor_set_uart_params (HyScanSonarModel         *sonar_model,
                             GHashTable               *ports,
                             const gchar              *name,
                             SensorUartPortParamFrame *frame)
{
  frame->result = FALSE;
  frame->name = g_strdup (name);
  frame->channel = (guint) g_random_int_range (1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS);
  frame->time_offset = g_random_int () % G_MAXUINT16;
  frame->protocol = g_random_boolean () ? HYSCAN_SENSOR_PROTOCOL_NMEA_0183 : HYSCAN_SENSOR_PROTOCOL_SAS;
  frame->uart_device = (guint) random_uart_device (ports, name);
  frame->uart_mode = (guint) random_uart_mode (ports, name);
  hyscan_sonar_model_sensor_set_uart_params (sonar_model,
                                             frame->name,
                                             frame->channel,
                                             frame->time_offset,
                                             frame->protocol,
                                             frame->uart_device,
                                             frame->uart_mode);
}

static void
test_sensor_set_udp_params (HyScanSonarModel          *sonar_model,
                            GHashTable                *ports,
                            const gchar               *name,
                            SensorUdpIpPortParamFrame *frame)
{
  frame->result = FALSE;
  frame->name = g_strdup (name);
  frame->channel = (guint) g_random_int_range (1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS);
  frame->time_offset = g_random_int () % G_MAXUINT16;;
  frame->protocol = g_random_boolean () ? HYSCAN_SENSOR_PROTOCOL_NMEA_0183 : HYSCAN_SENSOR_PROTOCOL_SAS;
  frame->ip_address = (guint) random_ip_addres (ports, name);
  frame->udp_port = (guint) g_random_int_range (1024, G_MAXUINT16);
  hyscan_sonar_model_sensor_set_udp_ip_params (sonar_model,
                                               frame->name,
                                               frame->channel,
                                               frame->time_offset,
                                               frame->protocol,
                                               frame->ip_address,
                                               (guint16) frame->udp_port);
}

static void
test_generator_set_preset (HyScanSonarModel        *sonar_model,
                           SourceInfo              *source_info,
                           HyScanSourceType         source_type,
                           GeneratorSetPresetFrame *frame)
{
  frame->result = FALSE;
  frame->source = source_type;
  frame->preset = (guint) random_preset (source_info);
  hyscan_sonar_model_gen_set_preset (sonar_model, frame->source, (guint) frame->preset);
}

static void
test_generator_set_auto (HyScanSonarModel      *sonar_model,
                         HyScanSourceType       source_type,
                         GeneratorSetAutoFrame *frame)
{
  frame->result = FALSE;
  frame->source = source_type;
  frame->signal_type = random_signal_type ();
  hyscan_sonar_model_gen_set_auto (sonar_model, frame->source, frame->signal_type);
}


static void
test_generator_set_simple (HyScanSonarModel        *sonar_model,
                           HyScanSourceType         source_type,
                           GeneratorSetSimpleFrame *frame)
{
  frame->result = FALSE;
  frame->source = source_type;
  frame->signal_type = random_signal_type ();
  frame->power = g_random_double ();
  hyscan_sonar_model_gen_set_simple (sonar_model, frame->source, frame->signal_type, frame->power);
}

static void
test_generator_set_extended (HyScanSonarModel          *sonar_model,
                             HyScanGeneratorControl    *generator_control,
                             HyScanSourceType           source_type,
                             GeneratorSetExtendedFrame *frame)
{
  gdouble min_duration, max_duration;
  HyScanGeneratorSignalType signal_type;

  signal_type = random_accepted_signal_type (HYSCAN_GENERATOR_SIGNAL_LFM |
                                             HYSCAN_GENERATOR_SIGNAL_LFMD |
                                             HYSCAN_GENERATOR_SIGNAL_TONE);

  hyscan_generator_control_get_duration_range (generator_control, source_type, signal_type,
                                               &min_duration, &max_duration);

  frame->result = FALSE;
  frame->source = source_type;
  frame->signal_type = signal_type;
  frame->duration = g_random_double_range (min_duration, max_duration);
  frame->power = g_random_double ();
  hyscan_sonar_model_gen_set_extended (sonar_model, frame->source, frame->signal_type, frame->duration, frame->power);
}

static void
test_generator_set_enable (HyScanSonarModel        *sonar_model,
                           HyScanSourceType         source_type,
                           GeneratorSetEnableFrame *frame,
                           gboolean                 enable)
{
  frame->result = FALSE;
  frame->source = source_type;
  frame->enable = enable;
  hyscan_sonar_model_gen_set_enable (sonar_model, frame->source, frame->enable);
}

static void
test_tvg_set_auto (HyScanSonarModel *sonar_model,
                   HyScanSourceType  source_type,
                   TvgSetAutoFrame  *frame)
{
  gboolean semiauto = g_random_boolean ();

  frame->result = FALSE;
  frame->source = source_type;
  frame->sensitivity = semiauto ? g_random_double_range (0.0, 1.0) : -1.0;
  frame->level = semiauto ? g_random_double_range (0.0, 1.0) : -1.0;
  hyscan_sonar_model_tvg_set_auto (sonar_model, frame->source, frame->level, frame->sensitivity);
}

static void
test_tvg_set_constant (HyScanSonarModel    *sonar_model,
                       HyScanTVGControl    *tvg_control,
                       HyScanSourceType     source_type,
                       TvgSetConstantFrame *frame)
{
  gdouble min_range, max_range;

  hyscan_tvg_control_get_gain_range (tvg_control, source_type, &min_range, &max_range);

  frame->result = FALSE;
  frame->source = source_type;
  frame->gain = g_random_double_range (min_range, max_range);
  hyscan_sonar_model_tvg_set_constant (sonar_model, frame->source, frame->gain);
}

static void
test_tvg_set_linear_db (HyScanSonarModel    *sonar_model,
                        HyScanTVGControl    *tvg_control,
                        HyScanSourceType     source_type,
                        TvgSetLinearDBFrame *frame)
{
  gdouble min_range, max_range;

  hyscan_tvg_control_get_gain_range (tvg_control, source_type, &min_range, &max_range);

  frame->result = FALSE;
  frame->source = source_type;
  frame->gain0 = g_random_double_range (min_range, max_range);
  frame->step = (max_range - min_range) / 10.0;
  hyscan_sonar_model_tvg_set_linear_db (sonar_model, frame->source, frame->gain0, frame->step);
}

static void
test_tvg_set_logarithmic (HyScanSonarModel       *sonar_model,
                          HyScanTVGControl       *tvg_control,
                          HyScanSourceType        source_type,
                          TvgSetLogarithmicFrame *frame)
{
  gdouble min_range, max_range;

  hyscan_tvg_control_get_gain_range (tvg_control, source_type, &min_range, &max_range);

  frame->result = FALSE;
  frame->source = source_type;
  frame->gain0 = g_random_double_range (min_range, max_range);
  frame->beta = g_random_double ();
  frame->alpha = g_random_double ();
  hyscan_sonar_model_tvg_set_logarithmic (sonar_model, frame->source, frame->gain0, frame->beta, frame->alpha);
}

static void
test_tvg_set_enable (HyScanSonarModel  *sonar_model,
                     HyScanSourceType   source_type,
                     TvgSetEnableFrame *frame,
                     gboolean           enable)
{
  frame->result = TRUE;
  frame->source = source_type;
  frame->enable = enable;
  hyscan_sonar_model_tvg_set_enable (sonar_model, frame->source, frame->enable);
}

static void
test_source_set_receive_time (HyScanSonarModel         *sonar_model,
                              HyScanSonarControl       *sonar_control,
                              HyScanSourceType          source_type,
                              SonarSetReceiveTimeFrame *frame)
{
  gdouble max_recv_time;

  max_recv_time = hyscan_sonar_control_get_max_receive_time (sonar_control, source_type);

  frame->result = FALSE;
  frame->source = source_type;
  frame->receive_time = g_random_double_range (0.0, max_recv_time);
  hyscan_sonar_model_set_receive_time (sonar_model, frame->source, frame->receive_time);
}

static void
test_source_set_position (HyScanSonarModel      *sonar_model,
                          HyScanSourceType       source_type,
                          SonarSetPositionFrame *frame)
{
  frame->result = FALSE;
  frame->source = source_type;
  frame->position = random_antenna_position ();
  hyscan_sonar_model_sonar_set_position (sonar_model, frame->source, frame->position);
}

static void
test_sonar_set_sync_type (HyScanSonarModel      *sonar_model,
                          SonarFrames           *sonar_frames)
{
  SonarSetSyncTypeFrame *frame = &sonar_frames->sync_type_frame;
  frame->result = FALSE;
  frame->sync_type = random_sync ();
  hyscan_sonar_model_set_sync_type (sonar_model, frame->sync_type);
}

static void
test_sonar_start (HyScanSonarModel *sonar_model,
                  SonarFrames      *sonar_frames)
{
  SonarStartFrame *frame = &sonar_frames->start_frame;
  frame->result = FALSE;
  frame->track_name = g_strdup ("trackname");
  frame->track_type = random_track_type ();
  hyscan_sonar_model_set_track_type (sonar_model, frame->track_type);
  hyscan_sonar_model_sonar_start (sonar_model, frame->track_name);
}

static void
test_sonar_stop (HyScanSonarModel *sonar_model,
                 SonarFrames      *sonar_frames)
{
  SonarStopFrame *frame = &sonar_frames->stop_frame;
  frame->result = FALSE;
  hyscan_sonar_model_sonar_stop (sonar_model);
}

static void
test_start (ApplicationData *app_data)
{
  int i;
  HyScanSonarModel *sonar_model;

  sonar_model = app_data->sonar_model;

  /* Тестирование датчиков. */
  for (i = 1; i <= SENSOR_N_PORTS; ++i)
    {
      SensorFrames *frames;
      gchar *name;

      /* Тестирование виртуального порта. */
      name = g_strdup_printf ("virtual.%d", i);
      frames = g_hash_table_lookup (app_data->sensors_frames, name);
      test_sensor_set_position (sonar_model, name, &frames->set_position_frame);
      test_sensor_set_enable (sonar_model, name, &frames->set_enable_frame);
      test_sensor_set_virtual_params (sonar_model, name, &frames->virtual_port_frame);
      g_free (name);

      /* Тестирование uart порта. */
      name = g_strdup_printf ("uart.%d", i);
      frames = g_hash_table_lookup (app_data->sensors_frames, name);
      test_sensor_set_position (sonar_model, name, &frames->set_position_frame);
      test_sensor_set_enable (sonar_model, name, &frames->set_enable_frame);
      test_sensor_set_uart_params (sonar_model, app_data->ports, name, &frames->uart_port_frame);
      g_free (name);

      /* Тестирование udp/ip порта. */
      name = g_strdup_printf ("udp.%d", i);
      frames = g_hash_table_lookup (app_data->sensors_frames, name);
      test_sensor_set_position (sonar_model, name, &frames->set_position_frame);
      test_sensor_set_enable (sonar_model, name, &frames->set_enable_frame);
      test_sensor_set_udp_params (sonar_model, app_data->ports, name, &frames->udp_port_frame);
      g_free (name);
    }
  /* Тестирование источников. */
  for (i = 0; i < SONAR_N_SOURCES; ++i)
    {
      SourceFrames *frames;
      HyScanSourceType source_type;

      source_type = source_type_by_index (i);
      frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (source_type));
      test_source_set_position (sonar_model, source_type, &frames->set_position_frame);
      test_source_set_receive_time (sonar_model, app_data->sonar_control, source_type, &frames->set_receive_time_frame);
      test_generator_set_enable (sonar_model, source_type, &frames->gen_set_enable_frame, g_random_boolean ());
      test_generator_set_auto (sonar_model, source_type, &frames->gen_set_auto_frame);
      test_tvg_set_enable (sonar_model, source_type, &frames->tvg_set_enable_frame, g_random_boolean ());
      test_tvg_set_auto (sonar_model, source_type, &frames->tvg_set_auto_frame);
    }
  /* Тест синхронизации. */
  test_sonar_set_sync_type (app_data->sonar_model, &app_data->sonar_frames);
  /* Тест запуска локатора. */
  test_sonar_start (app_data->sonar_model, &app_data->sonar_frames);
}

static void 
test_set (ApplicationData *app_data)
{
  HyScanSonarModel *sonar_model;
  SourceFrames *frames;
  SourceInfo *source_info;

  sonar_model = app_data->sonar_model;

  /* Тестирование простой настройки генератора и константного ВАРУ. */
  frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (HYSCAN_SOURCE_ECHOSOUNDER));
  test_generator_set_enable (sonar_model, HYSCAN_SOURCE_ECHOSOUNDER, &frames->gen_set_enable_frame, TRUE);
  test_generator_set_simple (sonar_model,
                             HYSCAN_SOURCE_ECHOSOUNDER,
                             &frames->gen_set_simple_frame);
  test_tvg_set_enable (sonar_model, HYSCAN_SOURCE_ECHOSOUNDER, &frames->tvg_set_enable_frame, TRUE);
  test_tvg_set_constant (sonar_model,
                         HYSCAN_TVG_CONTROL (app_data->sonar_control),
                         HYSCAN_SOURCE_ECHOSOUNDER,
                         &frames->tvg_set_constant_frame);

  /* Тестирование расширенной настройки генератора и линейного ВАРУ. */
  frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD));
  test_generator_set_enable (sonar_model, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &frames->gen_set_enable_frame, TRUE);
  test_generator_set_extended (sonar_model,
                               HYSCAN_GENERATOR_CONTROL (app_data->sonar_control),
                               HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                               &frames->gen_set_extended_frame);
  test_tvg_set_enable (sonar_model, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &frames->tvg_set_enable_frame, TRUE);
  test_tvg_set_linear_db (sonar_model,
                          HYSCAN_TVG_CONTROL (app_data->sonar_control),
                          HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                          &frames->tvg_set_linear_db_frame);

  /* Тестирование преднастроек генератора и логарифмического ВАРУ. */
  frames = g_hash_table_lookup (app_data->sources_frames, GINT_TO_POINTER (HYSCAN_SOURCE_SIDE_SCAN_PORT));
  source_info = source_info_by_source_type (app_data, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  test_generator_set_enable (sonar_model, HYSCAN_SOURCE_SIDE_SCAN_PORT, &frames->gen_set_enable_frame, TRUE);
  test_generator_set_preset (sonar_model,
                             source_info,
                             HYSCAN_SOURCE_SIDE_SCAN_PORT,
                             &frames->gen_set_preset_frame);
  test_tvg_set_enable (sonar_model, HYSCAN_SOURCE_SIDE_SCAN_PORT, &frames->tvg_set_enable_frame, TRUE);
  test_tvg_set_logarithmic (sonar_model,
                            HYSCAN_TVG_CONTROL (app_data->sonar_control),
                            HYSCAN_SOURCE_SIDE_SCAN_PORT,
                            &frames->tvg_set_logarithmic_frame);
}

/* Входная точка в тесты. */
static void
test (ApplicationData *app_data)
{
  switch (app_data->test_id)
    {
    case TEST_START:
      /* Тест запуска локатора. */
      g_message ("START TEST");
      test_start (app_data);
      break;

    case TEST_SET:
      /* Тест установки параметров локатора. */
      g_message ("SET TEST");
      test_set (app_data);
      break;

    case TEST_STOP:
      /* Тест останова локатора. */
      g_message ("STOP TEST");
      test_sonar_stop (app_data->sonar_model, &app_data->sonar_frames);
      break;

    default:
      g_warning ("Unknown test id");
      complete_test (app_data, EXIT_FAILURE);
    }
}

static void
free_servers_data (ApplicationData *app_data)
{
  guint i, j;

  for (i = 1; i <= SENSOR_N_PORTS; ++i)
    {
      gchar *port_name = g_strdup_printf ("uart.%d", i);
      UARTPortInfo *uart_port = g_hash_table_lookup (app_data->ports, port_name);

      for (j = 0; j <= SENSOR_N_UART_DEVICES; j++)
        {
          g_free (uart_port->uart_device_names[j]);
        }
      for (j = 0; j <= SENSOR_N_UART_MODES; j++)
        {
          g_free (uart_port->uart_mode_names[j]);
        }
      g_free (port_name);
    }

  for (i = 1; i <= SENSOR_N_PORTS; ++i)
    {
      gchar *port_name = g_strdup_printf ("udp.%d", i);
      UDPIPPortInfo *udp_port = g_hash_table_lookup (app_data->ports, port_name);

      for (j = 0; j <= SENSOR_N_IP_ADDRESSES; ++j)
        {
          g_free (udp_port->ip_address_names[j]);
        }
      g_free (port_name);
    }

  g_hash_table_unref (app_data->ports);

  for (i = 0; i < SONAR_N_SOURCES; ++i)
    {
      SourceInfo *info = source_info_by_source_type (app_data, source_type_by_index (i));

      for (j = 0; j <= GENERATOR_N_PRESETS; ++j)
        {
          g_free (info->generator.preset_names[j]);
        }
    }
}

static void
free_frames_data (ApplicationData *app_data)
{
  int i;

  /* Освобождение памяти, занятой параметрами источников. */
  g_hash_table_unref (app_data->sources_frames);

  /* Освобождение памяти, занятой параметрами датчиков. */
  for (i = 1; i <= SENSOR_N_PORTS; ++i)
    {
      SensorFrames *frames;
      gchar *name;

      name = g_strdup_printf ("virtual.%d", i);
      frames = g_hash_table_lookup (app_data->sensors_frames, name);
      g_free (frames->virtual_port_frame.name);
      g_free (frames->set_enable_frame.name);
      g_free (frames->set_position_frame.name);
      g_free (name);

      name = g_strdup_printf ("uart.%d", i);
      frames = g_hash_table_lookup (app_data->sensors_frames, name);
      g_free (frames->uart_port_frame.name);
      g_free (frames->set_enable_frame.name);
      g_free (frames->set_position_frame.name);
      g_free (name);

      name = g_strdup_printf ("udp.%d", i);
      frames = g_hash_table_lookup (app_data->sensors_frames, name);
      g_free (frames->udp_port_frame.name);
      g_free (frames->set_enable_frame.name);
      g_free (frames->set_position_frame.name);
      g_free (name);
    }
  g_hash_table_unref (app_data->sensors_frames);
}

static gboolean
test_source (ApplicationData *app_data)
{
  do_test (app_data, TEST_START);
  return G_SOURCE_REMOVE;
}

static void
complete_test (ApplicationData *app_data,
               int              test_result)
{
  app_data->result = test_result;
  g_main_loop_quit (app_data->main_loop);
}

static void
do_test (ApplicationData *app_data,
         int              test_id)
{
  app_data->test_id = test_id;
  test (app_data);
}

int
main (int    argc,
      char **argv)
{
  ApplicationData app_data;
  HyScanSonarBox *mock_sonar;
  int i;

  g_random_set_seed ((guint32) (g_get_monotonic_time () % G_MAXUINT32));

  /* Параметры источников данных. */
  app_data.sources_frames = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  for (i = 0; i < SONAR_N_SOURCES; ++i)
    {
      HyScanSourceType source_type = source_type_by_index (i);
      g_hash_table_insert (app_data.sources_frames, GINT_TO_POINTER (source_type), g_new0 (SourceFrames, 1));
    }
  /* Параметры датчиков. */
  app_data.sensors_frames = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  for (i = 1; i <= SENSOR_N_PORTS; ++i)
    {
      g_hash_table_insert (app_data.sensors_frames, g_strdup_printf ("virtual.%d", i), g_new0 (SensorFrames, 1));
      g_hash_table_insert (app_data.sensors_frames, g_strdup_printf ("uart.%d", i), g_new0 (SensorFrames, 1));
      g_hash_table_insert (app_data.sensors_frames, g_strdup_printf ("udp.%d", i), g_new0 (SensorFrames, 1));
    }

  /* Инициализация виртуального гидролокатора. */
  mock_sonar = make_mock_sonar (&app_data);
  init_servers (&app_data, mock_sonar);

  /* Управление виртуальным гидролокатором. */
  app_data.sonar_control = hyscan_sonar_control_new (HYSCAN_PARAM (mock_sonar), 0, 0, NULL);
  /* Модель управления гидролокатором. */
  app_data.sonar_model = hyscan_sonar_model_new (app_data.sonar_control);
  g_signal_connect_swapped (app_data.sonar_model,
                            "sonar-params-changed",
                            G_CALLBACK (on_sonar_params_changed),
                            &app_data);

  /* Создание MainLoop. */
  app_data.main_loop = g_main_loop_new (NULL, TRUE);

  /* Точка входа в тест. */
  g_idle_add ((GSourceFunc) test_source, &app_data);

  /* Запуск MainLoop. */
  g_main_loop_run (app_data.main_loop);

  /* Освобождение занятых ресурсов. */
  g_main_loop_unref (app_data.main_loop);
  g_object_unref (app_data.sonar_model);
  g_object_unref (app_data.sonar_control);

  g_object_unref (app_data.server.sensor);
  g_object_unref (app_data.server.generator);
  g_object_unref (app_data.server.tvg);
  g_object_unref (app_data.server.sonar);

  g_object_unref (mock_sonar);

  free_servers_data (&app_data);
  free_frames_data (&app_data);

  xmlCleanupParser ();

  return app_data.result;
}
