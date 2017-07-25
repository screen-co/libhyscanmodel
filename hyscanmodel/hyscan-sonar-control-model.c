/*
 * \file hyscan-sonar-control-model.c
 * \brief Исходный файл класса асинхронного управления гидролокатором.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 */
#include "hyscan-sonar-control-model.h"

/* Параметры запроса установки режима синхронизации. */
typedef struct
{
  HyScanSonarSyncType        sync_type;     /* Режим синхронизации. */
} HyScanParamsSonarSyncType;

/* Параметры запроса установки местоположения антенн ГЛ. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanAntennaPosition      position;      /* Местоположение. */
} HyScanParamsSonarPosition;

/* Параметры запроса установки времени приёма. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    receive_time;  /* Время приёма данных. */
} HyScanParamsSonarReceiveTime;

/* Параметры запроса запуска гидролокатора. */
typedef struct
{
  gchar                     *track_name;    /* Имя галса. */
  HyScanTrackType            track_type;    /* Тип галса. */
} HyScanParamsSonarStart;

/* Параметры запроса установки автоматического режима ВАРУ. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    level;         /* Целевой уровень сигнала. */
  gdouble                    sensitivity;   /* Чувствительность автомата регулировки. */
} HyScanParamsTVGAuto;

/* Параметры запроса установки постоянного уровня усиления. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    gain;          /* Усиление. */
} HyScanParamsTVGConstant;

/* Параметры запроса установки линейного увеличения усиления. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    gain0;         /* Начальный уровень усиления. */
  gdouble                    step;          /* Величина изменения усиления каждые 100 метров. */
} HyScanParamsTVGLinearDB;

/* Параметры запроса установки логарифмического закона изменения усиления. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    gain0;         /* начальный уровень усиления. */
  gdouble                    beta;          /* Коэффициент отражения цели. */
  gdouble                    alpha;         /* Коэффициент затухания. */
} HyScanParamsTVGLogarithmic;

/* Параметры запроса включения/выключения системы ВАРУ. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gboolean                   enable;        /* Включена или выключена. */
} HyScanParamsTVGEnable;

/* Параметры запроса установки режима работы генератора по преднастройкам. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  guint                      preset;        /* Идентификатор преднастройки. */
} HyScanParamsGeneratorPreset;

/* Параметры запроса установки автоматического режима генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanGeneratorSignalType  signal;        /* Тип сигнала. */
} HyScanParamsGeneratorAuto;

/* Параметры запроса установки упрощённого режима генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanGeneratorSignalType  signal;        /* Тип сигнала. */
  gdouble                    power;         /* Мощность. */
} HyScanParamsGeneratorSimple;

/* Параметры запроса установки расширенного режима генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanGeneratorSignalType  signal;        /* Тип сигнала. */
  gdouble                    duration;      /* Длительность. */
  gdouble                    power;         /* Мощность. */
} HyScanParamsGeneratorExtended;

/* Параметры запроса включения/выключения генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gboolean                   enable;        /* Включён или выключен. */
} HyScanParamsGeneratorEnable;

/* Параметры запроса установки режима работы виртуального порта. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  guint                      channel;       /* Канал. */
  gint64                     time_offset;   /* Коррекция времени приёма данных. */
} HyScanParamsSensorVirtualPortParam;

/* Параметры запроса установки режима работы UART порта. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  guint                      channel;       /* Канал. */
  gint64                     time_offset;   /* Коррекция времени приёма данных. */
  HyScanSensorProtocolType   protocol;      /* Протокол обмена данными с датчиком. */
  guint                      uart_device;   /* Идентификатор устройства. */
  guint                      uart_mode;     /* Идентификатор режима работы. */
} HyScanParamsSensorUartPortParam;

/* Параметры запроса установки режима работы UDP/IP порта. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  guint                      channel;       /* Канал. */
  gint64                     time_offset;   /* Коррекция времени приёма данных. */
  HyScanSensorProtocolType   protocol;      /* Протокол обмена данными с датчиком. */
  guint                      ip_address;    /* IP-адрес датчика. */
  guint16                    udp_port;      /* Номер порта датчика. */
} HyScanParamsSensorUdpIpPortParam;

/* Параметры запроса установки местоположения датчика. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  HyScanAntennaPosition      position;      /* Местоположение датчика. */
} HyScanParamsSensorPosition;

/* Параметры запроса включения/выключения датчика. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  gboolean                   enable;        /* Включён или выключен. */
} HyScanParamsSensorEnable;

enum
{
  PROP_O,
  PROP_SONAR_CONTROL
};

struct _HyScanSonarControlModelPrivate
{
  HyScanSonarControl   *sonar_control;   /* Интерфейс синхронного управления ГЛ. */
};

static void
    hyscan_sonar_control_model_set_property                        (GObject                             *object,
                                                                    guint                                prop_id,
                                                                    const GValue                        *value,
                                                                    GParamSpec                          *pspec);
static void
    hyscan_sonar_control_model_constructed                         (GObject                             *object);
static void
    hyscan_sonar_control_model_finalize                            (GObject                             *object);

static gboolean
    hyscan_sonar_control_model_cmd_sensor_set_virtual_port_param   (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSensorVirtualPortParam  *params);
static gboolean
    hyscan_sonar_control_model_cmd_sensor_set_uart_port_param      (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSensorUartPortParam     *params);
static gboolean
    hyscan_sonar_control_model_cmd_sensor_set_udp_ip_port_param    (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSensorUdpIpPortParam    *params);
static gboolean
    hyscan_sonar_control_model_cmd_sensor_set_position             (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSensorPosition          *params);
static gboolean
    hyscan_sonar_control_model_cmd_sensor_set_enable               (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSensorEnable            *params);

static gboolean
    hyscan_sonar_control_model_cmd_generator_set_preset            (HyScanSonarControlModel             *model,
                                                                    HyScanParamsGeneratorPreset         *params);
static gboolean
    hyscan_sonar_control_model_cmd_generator_set_auto              (HyScanSonarControlModel             *model,
                                                                    HyScanParamsGeneratorAuto           *params);
static gboolean
    hyscan_sonar_control_model_cmd_generator_set_simple            (HyScanSonarControlModel             *model,
                                                                    HyScanParamsGeneratorSimple         *params);
static gboolean
    hyscan_sonar_control_model_cmd_generator_set_extended          (HyScanSonarControlModel             *model,
                                                                    HyScanParamsGeneratorExtended       *params);
static gboolean
    hyscan_sonar_control_model_cmd_generator_set_enable            (HyScanSonarControlModel             *model,
                                                                    HyScanParamsGeneratorEnable         *params);

static gboolean
    hyscan_sonar_control_model_cmd_tvg_set_auto                    (HyScanSonarControlModel             *model,
                                                                    HyScanParamsTVGAuto                 *params);
static gboolean
    hyscan_sonar_control_model_cmd_tvg_set_constant                (HyScanSonarControlModel             *model,
                                                                    HyScanParamsTVGConstant             *params);
static gboolean
    hyscan_sonar_control_model_cmd_tvg_set_linear_db               (HyScanSonarControlModel             *model,
                                                                    HyScanParamsTVGLinearDB             *params);
static gboolean
    hyscan_sonar_control_model_cmd_tvg_set_logarithmic             (HyScanSonarControlModel             *model,
                                                                    HyScanParamsTVGLogarithmic          *params);
static gboolean
    hyscan_sonar_control_model_cmd_tvg_set_enable                  (HyScanSonarControlModel             *model,
                                                                    HyScanParamsTVGEnable               *params);

static gboolean
    hyscan_sonar_control_model_cmd_sonar_set_sync_type             (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSonarSyncType           *params);
static gboolean
    hyscan_sonar_control_model_cmd_sonar_set_position              (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSonarPosition           *params);
static gboolean
    hyscan_sonar_control_model_cmd_sonar_set_receive_time          (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSonarReceiveTime        *params);
static gboolean
    hyscan_sonar_control_model_cmd_sonar_start                     (HyScanSonarControlModel             *model,
                                                                    HyScanParamsSonarStart              *params);
static gboolean
    hyscan_sonar_control_model_cmd_sonar_stop                      (HyScanSonarControlModel             *model,
                                                                    gpointer                             unused);
static gboolean
    hyscan_sonar_control_model_cmd_sonar_ping                      (HyScanSonarControlModel             *model,
                                                                    gpointer                             unused);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarControlModel, hyscan_sonar_control_model, HYSCAN_TYPE_ASYNC)

static void
hyscan_sonar_control_model_class_init (HyScanSonarControlModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = hyscan_sonar_control_model_set_property;

  gobject_class->constructed = hyscan_sonar_control_model_constructed;
  gobject_class->finalize = hyscan_sonar_control_model_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_SONAR_CONTROL,
                                   g_param_spec_object ("sonar-control", 
                                                        "SonarControl",
                                                        "HyScan Sonar Control interface",
                                                        HYSCAN_TYPE_SONAR_CONTROL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_control_model_init (HyScanSonarControlModel *sonar_control_model)
{
  sonar_control_model->priv = hyscan_sonar_control_model_get_instance_private (sonar_control_model);
}

static void
hyscan_sonar_control_model_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_SONAR_CONTROL:
      HYSCAN_SONAR_CONTROL_MODEL (object)->priv->sonar_control = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_control_model_constructed (GObject *object)
{
  G_OBJECT_CLASS (hyscan_sonar_control_model_parent_class)->constructed (object);
}

static void
hyscan_sonar_control_model_finalize (GObject *object)
{
  g_clear_object (&HYSCAN_SONAR_CONTROL_MODEL (object)->priv->sonar_control);

  G_OBJECT_CLASS (hyscan_sonar_control_model_parent_class)->finalize (object);
}

/* Команда запроса установки режима синхронизации. */
static gboolean
hyscan_sonar_control_model_cmd_sonar_set_sync_type (HyScanSonarControlModel   *model,
                                                    HyScanParamsSonarSyncType *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_sonar_control_set_sync_type (priv->sonar_control, params->sync_type);
}

/* Команда запроса установки местоположения антенн ГЛ. */
static gboolean
hyscan_sonar_control_model_cmd_sonar_set_position (HyScanSonarControlModel   *model,
                                                   HyScanParamsSonarPosition *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_sonar_control_set_position (priv->sonar_control, params->source, &params->position);
}

/* Команда запроса установки времени приёма ГЛ. */
static gboolean
hyscan_sonar_control_model_cmd_sonar_set_receive_time (HyScanSonarControlModel      *model,
                                                       HyScanParamsSonarReceiveTime *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_sonar_control_set_receive_time (priv->sonar_control, params->source, params->receive_time);
}

/* Команда запроса запуска ГЛ. */
static gboolean
hyscan_sonar_control_model_cmd_sonar_start (HyScanSonarControlModel *model,
                                            HyScanParamsSonarStart  *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;
  gboolean res;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  res = hyscan_sonar_control_start (priv->sonar_control, params->track_name, params->track_type);
  g_free (params->track_name);

  return res;
}

/* Команда запроса останова ГЛ. */
static gboolean
hyscan_sonar_control_model_cmd_sonar_stop (HyScanSonarControlModel *model,
                                           gpointer                 unused)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  return hyscan_sonar_control_stop (priv->sonar_control);
}

/* Команда запроса выполнения одиночного зондирования. */
static gboolean
hyscan_sonar_control_model_cmd_sonar_ping (HyScanSonarControlModel *model,
                                           gpointer                 unused)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  return hyscan_sonar_control_ping (priv->sonar_control);
}

/* Команда запроса установки автоматического режима ВАРУ. */
static gboolean
hyscan_sonar_control_model_cmd_tvg_set_auto (HyScanSonarControlModel *model,
                                             HyScanParamsTVGAuto     *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_tvg_control_set_auto (HYSCAN_TVG_CONTROL (priv->sonar_control),
                                      params->source, params->level, params->sensitivity);
}

/* Команда запроса установки постоянного уровня усиления. */
static gboolean
hyscan_sonar_control_model_cmd_tvg_set_constant (HyScanSonarControlModel *model,
                                                 HyScanParamsTVGConstant *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_tvg_control_set_constant (HYSCAN_TVG_CONTROL (priv->sonar_control),
                                          params->source, params->gain);
}

/* Команда запроса установки линейного увеличения усиления. */
static gboolean
hyscan_sonar_control_model_cmd_tvg_set_linear_db (HyScanSonarControlModel *model,
                                                  HyScanParamsTVGLinearDB *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_tvg_control_set_linear_db (HYSCAN_TVG_CONTROL (priv->sonar_control),
                                           params->source, params->gain0, params->step);
}

/* Команда запроса установки логарифмического закона изменения усиления. */
static gboolean
hyscan_sonar_control_model_cmd_tvg_set_logarithmic (HyScanSonarControlModel    *model,
                                                    HyScanParamsTVGLogarithmic *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_tvg_control_set_logarithmic (HYSCAN_TVG_CONTROL (priv->sonar_control),
                                             params->source, params->gain0, params->beta, params->alpha);
}

/* Команда запроса включения/выключения системы ВАРУ. */
static gboolean
hyscan_sonar_control_model_cmd_tvg_set_enable (HyScanSonarControlModel *model,
                                               HyScanParamsTVGEnable   *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_tvg_control_set_enable (HYSCAN_TVG_CONTROL (priv->sonar_control), params->source, params->enable);
}

/* Команда запроса установки режима работы генератора по преднастройкам. */
static gboolean
hyscan_sonar_control_model_cmd_generator_set_preset (HyScanSonarControlModel     *model,
                                                     HyScanParamsGeneratorPreset *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_generator_control_set_preset (HYSCAN_GENERATOR_CONTROL (priv->sonar_control),
                                              params->source, params->preset);
}

/* Команда запроса установки автоматического режима генератора. */
static gboolean
hyscan_sonar_control_model_cmd_generator_set_auto (HyScanSonarControlModel   *model,
                                                   HyScanParamsGeneratorAuto *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_generator_control_set_auto (HYSCAN_GENERATOR_CONTROL (priv->sonar_control),
                                            params->source, params->signal);
}

/* Команда запроса установки упрощённого режима генератора. */
static gboolean
hyscan_sonar_control_model_cmd_generator_set_simple (HyScanSonarControlModel     *model,
                                                     HyScanParamsGeneratorSimple *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_generator_control_set_simple (HYSCAN_GENERATOR_CONTROL (priv->sonar_control),
                                              params->source, params->signal, params->power);
}

/* Команда запроса установки расширенного режима генератора. */
static gboolean
hyscan_sonar_control_model_cmd_generator_set_extended (HyScanSonarControlModel       *model,
                                                       HyScanParamsGeneratorExtended *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_generator_control_set_extended (HYSCAN_GENERATOR_CONTROL (priv->sonar_control),
                                                params->source, params->signal, params->duration, params->power);
}

/* Команда запроса включения/выключения генератора. */
static gboolean
hyscan_sonar_control_model_cmd_generator_set_enable (HyScanSonarControlModel     *model,
                                                     HyScanParamsGeneratorEnable *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  return hyscan_generator_control_set_enable (HYSCAN_GENERATOR_CONTROL (priv->sonar_control),
                                              params->source, params->enable);
}

/* Команда запроса установки режима работы виртуального порта. */
static gboolean
hyscan_sonar_control_model_cmd_sensor_set_virtual_port_param (HyScanSonarControlModel            *model,
                                                              HyScanParamsSensorVirtualPortParam *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;
  gboolean res;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  res = hyscan_sensor_control_set_virtual_port_param (HYSCAN_SENSOR_CONTROL (priv->sonar_control),
                                                      params->name, params->channel, params->time_offset);
  g_free (params->name);
  return res;
}

/* Команда запроса установки режима работы UART порта. */
static gboolean
hyscan_sonar_control_model_cmd_sensor_set_uart_port_param (HyScanSonarControlModel         *model,
                                                           HyScanParamsSensorUartPortParam *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;
  gboolean res;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  res = hyscan_sensor_control_set_uart_port_param (HYSCAN_SENSOR_CONTROL (priv->sonar_control),
                                                   params->name, params->channel, params->time_offset,
                                                   params->protocol, params->uart_device, params->uart_mode);
  g_free (params->name);
  return res;
}

/* Команда запроса установки режима работы UDP/IP порта. */
static gboolean
hyscan_sonar_control_model_cmd_sensor_set_udp_ip_port_param (HyScanSonarControlModel          *model,
                                                             HyScanParamsSensorUdpIpPortParam *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;
  gboolean res;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  res = hyscan_sensor_control_set_udp_ip_port_param (HYSCAN_SENSOR_CONTROL (priv->sonar_control),
                                                     params->name, params->channel, params->time_offset,
                                                     params->protocol, params->ip_address, params->udp_port);
  g_free (params->name);
  return res;
}

/* Команда запроса установки местоположения датчика. */
static gboolean
hyscan_sonar_control_model_cmd_sensor_set_position (HyScanSonarControlModel    *model,
                                                    HyScanParamsSensorPosition *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;
  gboolean res;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  res = hyscan_sensor_control_set_position (HYSCAN_SENSOR_CONTROL (priv->sonar_control),
                                            params->name, &params->position);
  g_free (params->name);
  return res;
}

/* Команда запроса включения/выключения датчика. */
static gboolean
hyscan_sonar_control_model_cmd_sensor_set_enable (HyScanSonarControlModel  *model,
                                                  HyScanParamsSensorEnable *params)
{
  HyScanSonarControlModelPrivate *priv = model->priv;
  gboolean res;

  if (priv->sonar_control == NULL)
    return FALSE;

  if (params == NULL)
    return FALSE;

  res = hyscan_sensor_control_set_enable (HYSCAN_SENSOR_CONTROL (priv->sonar_control),
                                          params->name, params->enable);
  g_free (params->name);
  return res;
}

/* Создаёт новый класс асинхронного управления гидролокатором. */
HyScanSonarControlModel *
hyscan_sonar_control_model_new (HyScanSonarControl *sonar_control)
{
  return g_object_new (HYSCAN_TYPE_SONAR_CONTROL_MODEL,
                       "sonar-control", sonar_control,
                       NULL);
}

/* Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL. */
gboolean
hyscan_sonar_control_model_sensor_set_virtual_port_param (HyScanSonarControlModel *model,
                                                          const gchar             *name,
                                                          guint                    channel,
                                                          gint64                   time_offset)
{
  HyScanAsyncCommand command;
  HyScanParamsSensorVirtualPortParam params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sensor_set_virtual_port_param;

  params.name = g_strdup (name);
  params.channel = channel;
  params.time_offset = time_offset;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART. */
gboolean
hyscan_sonar_control_model_sensor_set_uart_port_param (HyScanSonarControlModel  *model,
                                                       const gchar              *name,
                                                       guint                     channel,
                                                       gint64                    time_offset,
                                                       HyScanSensorProtocolType  protocol,
                                                       guint                     uart_device,
                                                       guint                     uart_mode)
{
  HyScanAsyncCommand command;
  HyScanParamsSensorUartPortParam params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sensor_set_uart_port_param;

  params.name = g_strdup (name);
  params.channel = channel;
  params.time_offset = time_offset;
  params.protocol = protocol;
  params.uart_device = uart_device;
  params.uart_mode = uart_mode;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP. */
gboolean
hyscan_sonar_control_model_sensor_set_udp_ip_port_param (HyScanSonarControlModel  *model,
                                                         const gchar              *name,
                                                         guint                     channel,
                                                         gint64                    time_offset,
                                                         HyScanSensorProtocolType  protocol,
                                                         guint                     ip_address,
                                                         guint16                   udp_port)
{
  HyScanAsyncCommand command;
  HyScanParamsSensorUdpIpPortParam params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sensor_set_udp_ip_port_param;

  params.name = g_strdup (name);
  params.channel = channel;
  params.time_offset = time_offset;
  params.protocol = protocol;
  params.ip_address = ip_address;
  params.udp_port = udp_port;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает информацию о местоположении приёмных антенн относительно центра масс судна. */
gboolean
hyscan_sonar_control_model_sensor_set_position (HyScanSonarControlModel *model,
                                                const gchar             *name,
                                                HyScanAntennaPosition   *position)
{
  HyScanAsyncCommand command;
  HyScanParamsSensorPosition params;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sensor_set_position;

  params.name = g_strdup (name);
  params.position = *position;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно включает или выключает приём данных на указанном порту. */
gboolean
hyscan_sonar_control_model_sensor_set_enable (HyScanSonarControlModel *model,
                                              const gchar             *name,
                                              gboolean                 enable)
{
  HyScanAsyncCommand command;
  HyScanParamsSensorEnable params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sensor_set_enable;

  params.name = g_strdup (name);
  params.enable = enable;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно включает преднастроенный режим работы генератора. */
gboolean
hyscan_sonar_control_model_generator_set_preset (HyScanSonarControlModel *model,
                                                 HyScanSourceType         source,
                                                 guint                    preset)
{
  HyScanAsyncCommand command;
  HyScanParamsGeneratorPreset params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_generator_set_preset;

  params.source = source;
  params.preset = preset;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно включает автоматический режим работы генератора. */
gboolean
hyscan_sonar_control_model_generator_set_auto (HyScanSonarControlModel    *model,
                                               HyScanSourceType            source,
                                               HyScanGeneratorSignalType   signal)
{
  HyScanAsyncCommand command;
  HyScanParamsGeneratorAuto params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_generator_set_auto;

  params.source = source;
  params.signal = signal;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно включает упрощённый режим работы генератора. */
gboolean
hyscan_sonar_control_model_generator_set_simple (HyScanSonarControlModel    *model,
                                                 HyScanSourceType            source,
                                                 HyScanGeneratorSignalType   signal,
                                                 gdouble                     power)
{
  HyScanAsyncCommand  command;
  HyScanParamsGeneratorSimple params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_generator_set_simple;

  params.source = source;
  params.signal = signal;
  params.power = power;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно включает расширенный режим работы генератора. */
gboolean
hyscan_sonar_control_model_generator_set_extended (HyScanSonarControlModel   *model,
                                                   HyScanSourceType           source,
                                                   HyScanGeneratorSignalType  signal,
                                                   gdouble                    duration,
                                                   gdouble                    power)
{
  HyScanAsyncCommand command;
  HyScanParamsGeneratorExtended params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_generator_set_extended;

  params.source = source;
  params.signal = signal;
  params.duration = duration;
  params.power = power;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}


/* Функция асинхронно включает или выключает формирование сигнала генератором. */
gboolean
hyscan_sonar_control_model_generator_set_enable (HyScanSonarControlModel *model,
                                                 HyScanSourceType         source,
                                                 gboolean                 enable)
{
  HyScanAsyncCommand command;
  HyScanParamsGeneratorEnable params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_generator_set_enable;

  params.source = source;
  params.enable = enable;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}


/* Функция асинхронно включает автоматический режим управления системой ВАРУ. */
gboolean
hyscan_sonar_control_model_tvg_set_auto (HyScanSonarControlModel *model,
                                         HyScanSourceType         source,
                                         gdouble                  level,
                                         gdouble                  sensitivity)
{
  HyScanAsyncCommand command;
  HyScanParamsTVGAuto params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_tvg_set_auto;

  params.source = source;
  params.level = level;
  params.sensitivity = sensitivity;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает постоянный уровень усиления системой ВАРУ. */
gboolean
hyscan_sonar_control_model_tvg_set_constant (HyScanSonarControlModel *model,
                                             HyScanSourceType         source,
                                             gdouble                  gain)
{
  HyScanAsyncCommand command;
  HyScanParamsTVGConstant params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_tvg_set_constant;

  params.source = source;
  params.gain = gain;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает линейное увеличение усиления в дБ на 100 метров. */
gboolean
hyscan_sonar_control_model_tvg_set_linear_db (HyScanSonarControlModel *model,
                                              HyScanSourceType         source,
                                              gdouble                  gain0,
                                              gdouble                  step)
{
  HyScanAsyncCommand command;
  HyScanParamsTVGLinearDB params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_tvg_set_linear_db;

  params.source = source;
  params.gain0 = gain0;
  params.step = step;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает логарифмический вид закона усиления системой ВАРУ. */
gboolean
hyscan_sonar_control_model_tvg_set_logarithmic (HyScanSonarControlModel *model,
                                                HyScanSourceType         source,
                                                gdouble                  gain0,
                                                gdouble                  beta,
                                                gdouble                  alpha)
{
  HyScanAsyncCommand command;
  HyScanParamsTVGLogarithmic params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_tvg_set_logarithmic;

  params.source = source;
  params.gain0 = gain0;
  params.beta = beta;
  params.alpha = alpha;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно включает или выключает систему ВАРУ. */
gboolean
hyscan_sonar_control_model_tvg_set_enable (HyScanSonarControlModel *model,
                                           HyScanSourceType         source,
                                           gboolean                 enable)
{
  HyScanAsyncCommand command;
  HyScanParamsTVGEnable params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_tvg_set_enable;

  params.source = source;
  params.enable = enable;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает тип синхронизации излучения. */
gboolean
hyscan_sonar_control_model_sonar_set_sync_type (HyScanSonarControlModel *model,
                                                HyScanSonarSyncType      sync_type)
{
  HyScanAsyncCommand command;
  HyScanParamsSonarSyncType params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sonar_set_sync_type;

  params.sync_type = sync_type;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно устанавливает информацию о местоположении приёмных антенн
 * относительно центра масс судна. */
gboolean
hyscan_sonar_control_model_sonar_set_position (HyScanSonarControlModel *model,
                                               HyScanSourceType         source,
                                               HyScanAntennaPosition   *position)
{
  HyScanAsyncCommand command;
  HyScanParamsSonarPosition params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  params.source = source;
  params.position = *position;

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sonar_set_position;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно задаёт время приёма эхосигнала источником данных. */
gboolean
hyscan_sonar_control_model_sonar_set_receive_time (HyScanSonarControlModel *model,
                                                   HyScanSourceType         source,
                                                   gdouble                  receive_time)
{
  HyScanAsyncCommand command;
  HyScanParamsSonarReceiveTime params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sonar_set_receive_time;

  params.source = source;
  params.receive_time = receive_time;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно переводит гидролокатор в рабочий режим и включает запись данных. */
gboolean
hyscan_sonar_control_model_sonar_start (HyScanSonarControlModel *model,
                                        const gchar             *track_name,
                                        HyScanTrackType          track_type)
{
  HyScanAsyncCommand command;
  HyScanParamsSonarStart params;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sonar_start;

  params.track_name = g_strdup (track_name);
  params.track_type = track_type;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, &params, sizeof (params));
}

/* Функция асинхронно переводит гидролокатор в ждущий режим и отключает запись данных. */
gboolean
hyscan_sonar_control_model_sonar_stop (HyScanSonarControlModel *model)
{
  HyScanAsyncCommand command;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command =  (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sonar_stop;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, NULL, 0);
}

/* Функция асинхронно выполняет один цикл зондирования и приёма данных. */
gboolean
hyscan_sonar_control_model_sonar_ping (HyScanSonarControlModel    *model)
{
  HyScanAsyncCommand command;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_MODEL (model), FALSE);

  command = (HyScanAsyncCommand) hyscan_sonar_control_model_cmd_sonar_ping;

  return hyscan_async_append_query (HYSCAN_ASYNC (model), command, model, NULL, 0);
}
