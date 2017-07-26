/*
 * \file hyscan-sonar-model.c
 * \brief Исходный файл класса HyScanSonarModel - модель гидролокатора.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 */
#include "hyscan-sonar-model.h"
#include "hyscan-sonar-control-model.h"

#define HYSCAN_SONAR_MODEL_CHECK_FOR_UPDATES_INTERVAL 50    /* Интервал между проверками изменений параметров. */
#define HYSCAN_SONAR_MODEL_UPDATE_TIMEOUT             500   /* Период буферизации. */

/* Параметры датчика. */
typedef struct
{
  /* Параметры UDP/IP-порта. */
  struct
  {
    HyScanSensorProtocolType     protocol;      /* Протокол обмена данными с датчиком. */
    guint                        addr;          /* Идентификатор IP-адреса. */
    guint16                      port;          /* UDP-порт. */
  }                              udp_ip;

  /* Параметры UART-порта. */
  struct
  {
    HyScanSensorProtocolType     protocol;      /* Протокол обмена данными с датчиком. */
    guint                        device;        /* Идентификатор устройства UART. */
    guint                        mode;          /* Идентификатор режима работы устройства UART. */
  }                              uart;

  guint                          channel;       /* Номер канала. */
  gint64                         time_offset;   /* Коррекция времени приёма данных. */
  gboolean                       modified;      /* Поменялись параметры. */

  /* Местоположение датчика. */
  struct
  {
    HyScanAntennaPosition        cval;          /* Текущее значение. */
    HyScanAntennaPosition        nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              position;

  /* Состояние приёма данных. */
  struct
  {
    gboolean                     cval;          /* Текущее значение. */
    gboolean                     nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              enabled;
} HyScanSensorParams;

/* Параметры генератора. */
typedef struct
{
  /* Параметры преднастроек. */
  struct
  {
    guint                        preset;       /* Идентификатор преднастройки. */
  }                              preset_prm;

  /* Параметры автоматического режима. */
  struct
  {
    HyScanGeneratorSignalType    signal_type;  /* Тип излучаемого сигнала. */
  }                              auto_prm;

  /* Параметры упрощенного режима. */
  struct
  {
    HyScanGeneratorSignalType    signal_type;  /* Тип излучаемого сигнала. */
    gdouble                      power;        /* Энергия сигнала. */
  }                              simple_prm;

  /* Параметры расширенного режима. */
  struct
  {
    HyScanGeneratorSignalType    signal_type;  /* Тип излучаемого сигнала. */
    gdouble                      duration;     /* Задержка. */
    gdouble                      power;        /* Энергия сигнала. */
  }                              extended_prm;

  /* Режим работы. */
  struct
  {
    HyScanGeneratorModeType      cval;          /* Текущее значение. */
    HyScanGeneratorModeType      nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              mode;

  /* Включен или выключен. */
  struct
  {
    gboolean                     cval;          /* Текущее значение. */
    gboolean                     nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              enabled;
} HyScanGenParams;

/* Параметры ВАРУ. */
typedef struct
{
  /* Параметры автоматического режима. */
  struct
  {
    gdouble                      level;         /* Целевой уровень сигнала. */
    gdouble                      sensitivity;   /* Чувствительность автомата регулировки. */
  }                              auto_prm;

  /* Параметры постоянного режима. */
  struct
  {
    gdouble                      gain;          /* Уровень усиления. */
  }                              const_prm;

  /* Параметры линейного режима. */
  struct
  {
    gdouble                      gain0;         /* Начальный уровень усиления. */
    gdouble                      step;          /* Шаг увеличения усиления. */
  }                              lin_db_prm;

  /* Параметры логарифмического режима. */
  struct
  {
    gdouble                      gain0;         /* Начальный уровень усиления. */
    gdouble                      beta;          /* Коэф. отражения. */
    gdouble                      alpha;         /* Коэф. затухания. */
  }                              log_prm;

  /* Режим работы. */
  struct
  {
    HyScanTVGModeType            cval;          /* Текущее значение. */
    HyScanTVGModeType            nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              mode;

  /* Включена или выключена. */
  struct
  {
    gboolean                     cval;          /* Текущее значение. */
    gboolean                     nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              enabled;
} HyScanTVGParams;

/* Параметры источника. */
typedef struct
{
  /* Метстоположение. */
  struct
  {
    HyScanAntennaPosition        cval;          /* Текущее значение. */
    HyScanAntennaPosition        nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              position;

  /* Время приёма. */
  struct
  {
    gdouble                      cval;          /* Текущее значение. */
    gdouble                      nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              receive_time;
} HyScanSrcParams;

/* Контейнер параметров источника. */
typedef struct
{
  HyScanGenParams                gen;           /* Параметры ВАРУ. */
  HyScanTVGParams                tvg;           /* Параметры генератора. */
  HyScanSrcParams                src;           /* Параметры источника. */
} HyScanSrcParamsContainer;

/* Параметры гидролокатора. */
typedef struct
{
  /* Тип синхронизации. */
  struct
  {
    HyScanSonarSyncType          cval;          /* Текущее значение. */
    HyScanSonarSyncType          nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              sync_type;

  /* Состояние записи. */
  struct
  {
    gboolean                     cval;          /* Текущее значение. */
    gboolean                     nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              record_state;

  /* Одиночное зондирование. */
  struct
  {
    gboolean                     cval;          /* Текущее значение. */
    gboolean                     nval;          /* Новое значение. */
    gboolean                     modified;      /* Флаг изменений. */
  }                              ping_state;

  HyScanTrackType                track_type;    /* Тип галса. */
  gchar                         *track_name;    /* Название записываемого галса. */
} HyScanSonarParams;

/* Идентификаторы свойств. */
enum
{
  PROP_O,
  PROP_DB_INFO,        /* Модель системы хранения. */
  PROP_SONAR_CONTROL   /* Интерфейс управления гидролокатором. */
};

/* Индексы сигналов в массиве идентификаторов сигналов. */
enum
{
  SIGNAL_SONAR_CONTROL_STATE_CHANGED,  /* Изменение состояния системы управления гидролокатором. */
  SIGNAL_SONAR_PARAMS_UPDATED,         /* Обновлены параметры гидролокатора. */
  SIGNAL_ACTIVE_TRACK_CHANGED,         /* Изменен записываемый галс. */
  SIGNAL_LAST
};

/* Массив идентификаторов сигналов. */
static guint hyscan_sonar_model_signals[SIGNAL_LAST] = { 0 };

struct _HyScanSonarModelPrivate
{
  HyScanSonarControl       *sonar_control;                 /* Интерфейс управления гидролокатором. */
  HyScanSonarControlModel  *sonar_control_model;           /* Управление гидролокатором. */
  HyScanDBInfo             *db_info;                       /* Модель БД. */

  GTimer                   *check_for_updates_timer;       /* Таймер изменения параметров ГЛ. */
  guint                     check_for_updates_source_id;   /* Идентификатор таймера изменения параметров ГЛ. */
  gboolean                  update;                        /* Флаг, указывающий, что параметры ГЛ были изменены. */
  gboolean                  force_update;                  /* Форсированное обновление параметров. */

  gdouble                   sound_velocity;                   /* Скорость звука. */

  gboolean                  sonar_control_state;           /* Флаг, указывающий, что ГЛ в данный момент занят. */
  HyScanSonarParams         sonar_params;                  /* Общие параметры ГЛ. */
  GHashTable               *sources_params;                /* Параметры источников данных ГЛ. */
  GHashTable               *sensors_params;                /* Параметры датчиков. */

  HyScanSourceType         *sources;                       /* Список источников. */
  gchar                   **ports;                         /* Список портов. */
};

static void       hyscan_sonar_model_set_property              (GObject            *object,
                                                                guint               prop_id,
                                                                const GValue       *value,
                                                                GParamSpec         *pspec);
static void       hyscan_sonar_model_object_constructed        (GObject            *object);
static void       hyscan_sonar_model_object_finalize           (GObject            *object);
static void       hyscan_sonar_model_set_valid_params          (HyScanSonarModel   *model);
static void       hyscan_sonar_model_set_sonar_control_state   (HyScanSonarModel   *model,
                                                                gboolean            state);
static gchar*     hyscan_sonar_model_generate_track_name       (HyScanSonarModel   *model);
static void       hyscan_sonar_model_on_started                (HyScanSonarModel   *model,
                                                                HyScanAsync        *async);
static void       hyscan_sonar_model_on_completed              (HyScanSonarModel   *model,
                                                                gboolean            result,
                                                                HyScanAsync        *async);
static gboolean   hyscan_sonar_model_update_sensor_params      (HyScanSonarModel   *model,
                                                                const gchar        *port_name);
static gboolean   hyscan_sonar_model_update_tvg_params         (HyScanSonarModel   *model,
                                                                HyScanSourceType    source_type,
                                                                HyScanTVGParams    *prm);
static gboolean   hyscan_sonar_model_update_gen_params         (HyScanSonarModel   *model,
                                                                HyScanSourceType    source_type,
                                                                HyScanGenParams    *prm);
static gboolean   hyscan_sonar_model_update_src_params         (HyScanSonarModel   *model,
                                                                HyScanSourceType    source_type,
                                                                HyScanSrcParams    *prm);
static gboolean   hyscan_sonar_model_update_sensors            (HyScanSonarModel   *model);
static gboolean   hyscan_sonar_model_update_sources            (HyScanSonarModel   *model);
static gboolean   hyscan_sonar_model_update_sonar              (HyScanSonarModel   *model);
static gboolean   hyscan_sonar_model_check_for_updates         (gpointer            sonar_model_ptr);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarModel, hyscan_sonar_model, G_TYPE_OBJECT)

static void
hyscan_sonar_model_class_init (HyScanSonarModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = hyscan_sonar_model_set_property;
  gobject_class->constructed = hyscan_sonar_model_object_constructed;
  gobject_class->finalize = hyscan_sonar_model_object_finalize;

  /* Свойства.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DB_INFO,
                                   g_param_spec_object ("db-info",
                                                        "DBInfo",
                                                        "Database info",
                                                        HYSCAN_TYPE_DB_INFO,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_SONAR_CONTROL,
                                   g_param_spec_object ("sonar-control",
                                                        "SonarControl",
                                                        "Sonar Control",
                                                        HYSCAN_TYPE_SONAR_CONTROL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /* Сигналы.
   */
  hyscan_sonar_model_signals[SIGNAL_SONAR_CONTROL_STATE_CHANGED] =
    g_signal_new ("sonar-control-state-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  hyscan_sonar_model_signals[SIGNAL_SONAR_PARAMS_UPDATED] =
    g_signal_new ("sonar-params-updated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  hyscan_sonar_model_signals[SIGNAL_ACTIVE_TRACK_CHANGED] =
    g_signal_new ("active-track-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
hyscan_sonar_model_init (HyScanSonarModel *model)
{
  HyScanSonarModelPrivate *priv;
  priv = hyscan_sonar_model_get_instance_private (model);

  priv->sound_velocity = 1500.0; /* Этот подход будет изменён в будущем релизе. */
  priv->sonar_control_state = TRUE;
  priv->sources_params = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  priv->sensors_params = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  model->priv = priv;
}

static void
hyscan_sonar_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanSonarModel *sonar_model = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = sonar_model->priv;

  switch (prop_id)
    {
    case PROP_DB_INFO:
      priv->db_info = g_value_dup_object (value);
      break;

    case PROP_SONAR_CONTROL:
      priv->sonar_control = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_model_object_constructed (GObject *object)
{
  HyScanSourceType *source;
  HyScanSonarModel *model = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = model->priv;

  G_OBJECT_CLASS (hyscan_sonar_model_parent_class)->constructed (object);

  if (!HYSCAN_IS_SONAR_CONTROL (priv->sonar_control))
    return;

  /* Инициализация источников данных.
   */
  if ((priv->sources = hyscan_sonar_control_source_list (priv->sonar_control)) == NULL)
    {
      g_clear_object (&priv->sonar_control);
      return;
    }

  /* Инициализация модели управления гидролокатором.
   */
  if ((priv->sonar_control_model = hyscan_sonar_control_model_new (priv->sonar_control)) == NULL)
    {
      g_clear_object (&priv->sonar_control);
      g_clear_pointer (&priv->sources, g_free);
      return;
    }

  g_signal_connect_swapped (priv->sonar_control_model, "started",
                            G_CALLBACK (hyscan_sonar_model_on_started), model);
  g_signal_connect_swapped (priv->sonar_control_model, "completed",
                            G_CALLBACK (hyscan_sonar_model_on_completed), model);

  /* Инициализация параметров источников данных.
   */
  for (source = priv->sources; *source != HYSCAN_SOURCE_INVALID; ++source)
    {
      HyScanSrcParamsContainer *container = g_new0 (HyScanSrcParamsContainer, 1);
      gpointer source_ptr = GINT_TO_POINTER ((HyScanSourceType) *source);
      g_hash_table_insert (priv->sources_params, source_ptr, container);
    }

  /* Инициализация параметров датчиков.
   */
  if ((priv->ports = hyscan_sensor_control_list_ports (HYSCAN_SENSOR_CONTROL (priv->sonar_control))) != NULL)
    {
      gchar **ports;
      for (ports = priv->ports; *ports != NULL; ++ports)
        g_hash_table_insert (priv->sensors_params, *ports, g_new0 (HyScanSensorParams, 1));
    }

  /* Задание допустимых значений параметров. */
  hyscan_sonar_model_set_valid_params (model);

  /* Инициализация периодической проверки буфера настроек на наличие изменений.
   */
  priv->check_for_updates_timer = g_timer_new ();
  priv->check_for_updates_source_id = g_timeout_add (HYSCAN_SONAR_MODEL_CHECK_FOR_UPDATES_INTERVAL,
                                                     hyscan_sonar_model_check_for_updates, model);
}

static void
hyscan_sonar_model_object_finalize (GObject *object)
{
  HyScanSonarModel *sonar_model = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = sonar_model->priv;

  g_source_remove (priv->check_for_updates_source_id);
  g_timer_destroy (priv->check_for_updates_timer);

  g_clear_object (&priv->sonar_control);
  g_clear_object (&priv->sonar_control_model);
  g_clear_object (&priv->db_info);

  g_clear_pointer (&priv->sensors_params, g_hash_table_unref);
  g_clear_pointer (&priv->sources_params, g_hash_table_unref);

  g_free (priv->sources);
  g_strfreev (priv->ports);

  g_free (priv->sonar_params.track_name);

  G_OBJECT_CLASS (hyscan_sonar_model_parent_class)->finalize (object);
}

/*
 * Задаёт докустимые значения параметров.
 *
 * Исходя из принципа  stateless,  пользователь  запрашивает только  те  параметры, 
 * которые  были  предварительно  им  заданы, при  этом,  API  позволяет  запросить
 * значения любых параметров (захотел - получил), в том числе и тех, которые ещё не
 * настраивались - в  этом  случае  возвращаемые  значения не будут соответствовать
 * реальным  заданным значениям, однако этот факт не означает, что можно возвращать
 * значения, выходящие за рамки допустимых.
 */
static void
hyscan_sonar_model_set_valid_params (HyScanSonarModel *model)
{
  HyScanSonarSyncType sync_caps;
  HyScanSonarModelPrivate *priv = model->priv;
  HyScanSonarControl *sc = priv->sonar_control;
  HyScanSourceType *source;

  /* Тип синхронизации.
   */
  sync_caps = hyscan_sonar_control_get_sync_capabilities (sc);
  if (sync_caps & HYSCAN_SONAR_SYNC_SOFTWARE)
    priv->sonar_params.sync_type.cval = HYSCAN_SONAR_SYNC_SOFTWARE;
  else if (sync_caps & HYSCAN_SONAR_SYNC_INTERNAL)
    priv->sonar_params.sync_type.cval = HYSCAN_SONAR_SYNC_INTERNAL;
  else if (sync_caps & HYSCAN_SONAR_SYNC_EXTERNAL)
    priv->sonar_params.sync_type.cval = HYSCAN_SONAR_SYNC_EXTERNAL;
  else
    g_warning ("HyScanSonarModel: invalid sync capabilities");

  /* Задание допустимых значений параметров источников данных.
   */
  for (source = priv->sources; *source != HYSCAN_SOURCE_INVALID; ++source)
    {
      HyScanGeneratorControl *gc;
      HyScanTVGControl *tc;
      HyScanTVGModeType tvg_mode_caps;
      gdouble min_gain, max_gain, gain;
      HyScanGeneratorModeType gen_mode_caps;
      HyScanGeneratorSignalType signals_types, signal_type = HYSCAN_GENERATOR_SIGNAL_INVALID;
      HyScanSourceType source_type;
      HyScanSrcParamsContainer *prm;

      gc = HYSCAN_GENERATOR_CONTROL (sc);
      tc = HYSCAN_TVG_CONTROL (sc);
      source_type = (HyScanSourceType) *source;
      prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type));

      /* Время приёма эхосигнала. */
      prm->src.receive_time.cval = 1.0;

      /* Режим генератора.
       */
      gen_mode_caps = hyscan_generator_control_get_capabilities (gc, source_type);
      if (gen_mode_caps & HYSCAN_GENERATOR_MODE_PRESET)
        prm->gen.mode.cval = HYSCAN_GENERATOR_MODE_PRESET;
      else if (gen_mode_caps & HYSCAN_GENERATOR_MODE_AUTO)
        prm->gen.mode.cval = HYSCAN_GENERATOR_MODE_AUTO;
      else if (gen_mode_caps & HYSCAN_GENERATOR_MODE_SIMPLE)
        prm->gen.mode.cval = HYSCAN_GENERATOR_MODE_SIMPLE;
      else if (gen_mode_caps & HYSCAN_GENERATOR_MODE_EXTENDED)
        prm->gen.mode.cval = HYSCAN_GENERATOR_MODE_EXTENDED;
      else
        g_warning ("HyScanSonarModel: invalid generator capabilities.");

      /* Определение доступного сигнала.
       */
      signals_types = hyscan_generator_control_get_signals (gc, source_type);
      if (signals_types & HYSCAN_GENERATOR_SIGNAL_AUTO)
        signal_type = HYSCAN_GENERATOR_SIGNAL_AUTO;
      else if (signals_types & HYSCAN_GENERATOR_SIGNAL_TONE)
        signal_type = HYSCAN_GENERATOR_SIGNAL_TONE;
      else if (signals_types & HYSCAN_GENERATOR_SIGNAL_LFM)
        signal_type = HYSCAN_GENERATOR_SIGNAL_LFM;
      else if (signals_types & HYSCAN_GENERATOR_SIGNAL_LFMD)
        signal_type = HYSCAN_GENERATOR_SIGNAL_LFMD;
      else
        g_warning ("HyScanSonarModel: invalid singals types.");

      /* Задание сигнала.
       */
      if (signal_type != HYSCAN_GENERATOR_SIGNAL_INVALID)
        {
          prm->gen.auto_prm.signal_type = signal_type;
          prm->gen.simple_prm.signal_type = signal_type;
          prm->gen.extended_prm.signal_type = signal_type;
        }

      /* Задание энергии сигнала
       */
      prm->gen.simple_prm.power = 1.0;
      prm->gen.extended_prm.power = 1.0;

      /* Режим генератора.
       */
      tvg_mode_caps = hyscan_tvg_control_get_capabilities (tc, source_type);
      if (tvg_mode_caps & HYSCAN_TVG_MODE_AUTO)
        prm->tvg.mode.cval = HYSCAN_TVG_MODE_AUTO;
      else if (tvg_mode_caps & HYSCAN_TVG_MODE_CONSTANT)
        prm->tvg.mode.cval = HYSCAN_TVG_MODE_CONSTANT;
      else if (tvg_mode_caps & HYSCAN_TVG_MODE_LINEAR_DB)
        prm->tvg.mode.cval = HYSCAN_TVG_MODE_LINEAR_DB;
      else if (tvg_mode_caps & HYSCAN_TVG_MODE_LOGARITHMIC)
        prm->tvg.mode.cval = HYSCAN_TVG_MODE_LOGARITHMIC;
      else
        g_warning ("HyScanSonarModel: invalid TVG capabilities.");

      /* Диапазон усилений. */
      hyscan_tvg_control_get_gain_range (tc, source_type, &min_gain, &max_gain);
      gain = (max_gain + min_gain) / 2;

      /* Задание автоматических параметров ВАРУ.
       */
      prm->tvg.auto_prm.sensitivity = 1.0;
      prm->tvg.auto_prm.level = 1.0;

      /* Задание постоянных параметров ВАРУ. */
      prm->tvg.const_prm.gain = gain;

      /* Задание линейных параметров ВАРУ.
       */
      prm->tvg.lin_db_prm.gain0 = gain;
      prm->tvg.lin_db_prm.step = (max_gain - gain) * 0.2;

      /* Задание логарифмических параметров ВАРУ.
       */
      prm->tvg.log_prm.gain0 = gain;
      prm->tvg.log_prm.beta = 50.0;
      prm->tvg.log_prm.alpha = 0.5;
    }
}

/* Задаёт состояние системы управления гидролокатором. */
static void
hyscan_sonar_model_set_sonar_control_state (HyScanSonarModel *model,
                                            gboolean          state)
{
  HyScanSonarModelPrivate *priv = model->priv;
  if (priv->sonar_control_state != state)
    {
      priv->sonar_control_state = state;
      g_signal_emit (model, hyscan_sonar_model_signals [SIGNAL_SONAR_CONTROL_STATE_CHANGED], 0);
    }
}

/* Генерирует названия галсов. */
static gchar *
hyscan_sonar_model_generate_track_name (HyScanSonarModel *model)
{
  gchar *track = NULL;

  /* Если система хранения задана, осуществляется поиск следующего свободного названия,
   * в противном случае выбирается имя, содержащее текущие дату и время. */
  if (model->priv->db_info != NULL)
    {
      GHashTable *tracks = hyscan_db_info_get_tracks (model->priv->db_info);
      guint n_tracks = g_hash_table_size (tracks);

      do
        {
          g_free (track);
          track = g_strdup_printf ("Track%02d", ++n_tracks);
        } while (g_hash_table_lookup (tracks, track) != NULL);

      g_hash_table_unref (tracks);
    }
  else
    {
      GDateTime *dt = g_date_time_new_now_local ();
      gchar *name_base = g_date_time_format (dt, "Track%y%m%d%H%M%S");

      track = g_strdup_printf ("%s%d", name_base, (gint) (g_get_monotonic_time () % 1000));

      g_free (name_base);
      g_date_time_unref (dt);
    }

  return track;
}

/* Обработчик сигнала "started" модели управления гидролокатором. */
static void
hyscan_sonar_model_on_started (HyScanSonarModel *model,
                               HyScanAsync      *async)
{
}

/* Обработчик сигнала "completed" модели управления гидролокатором. */
static void
hyscan_sonar_model_on_completed (HyScanSonarModel *model,
                                 gboolean          result,
                                 HyScanAsync      *async)
{
  /* Разрешить общение с гидролокатором и уведомить об этом событии потребителям. */
  hyscan_sonar_model_set_sonar_control_state (model, TRUE);

  /* Уведомить потребителей об изменении параметров. */
  g_signal_emit (model, hyscan_sonar_model_signals [SIGNAL_SONAR_PARAMS_UPDATED], 0, result);
}

/* Обновляет параметры датчика. */
static gboolean
hyscan_sonar_model_update_sensor_params (HyScanSonarModel *model,
                                         const gchar      *port_name)
{
  HyScanSensorParams *prm;
  HyScanSonarControlModel *scm = model->priv->sonar_control_model;

  if ((prm = g_hash_table_lookup (model->priv->sensors_params, port_name)) != NULL)
    {
      /* Обновление местоположения источника. */
      if (prm->position.modified)
        {
          prm->position.modified = FALSE;
          if (!hyscan_sonar_control_model_sensor_set_position (scm, port_name, &prm->position.nval))
            {
              g_warning ("HyScanSonarModel: can't set position.");
              return FALSE;
            }
          prm->position.cval = prm->position.nval;
        }

      /* Включение датчика. */
      if (prm->enabled.modified)
        {
          prm->enabled.modified = FALSE;
          if (!hyscan_sonar_control_model_sensor_set_enable (scm, port_name, prm->enabled.nval))
            {
              g_warning ("HyScanSonarModel: can't enable sensor.");
              return FALSE;
            }
          prm->enabled.cval = prm->enabled.nval;
        }

      /* Изменение параметров датчика, если датчик включен. */
      if (prm->enabled.cval && prm->modified)
        {
          HyScanSensorPortType port_type =
              hyscan_sensor_control_get_port_type (HYSCAN_SENSOR_CONTROL (model->priv->sonar_control), port_name);

          prm->modified = FALSE;

          switch (port_type)
            {
            case HYSCAN_SENSOR_PORT_VIRTUAL:
              if (!hyscan_sonar_control_model_sensor_set_virtual_port_param (scm, port_name,
                                                                             prm->channel,
                                                                             prm->time_offset))
                {
                  g_warning ("HyScanSonarModel: can't set virtual port param.");
                  return FALSE;
                }
              break;

            case HYSCAN_SENSOR_PORT_UART:
              if (!hyscan_sonar_control_model_sensor_set_uart_port_param (scm, port_name,
                                                                          prm->channel,
                                                                          prm->time_offset,
                                                                          prm->uart.protocol,
                                                                          prm->uart.device,
                                                                          prm->uart.mode))
                {
                  g_warning ("HyScanSonarModel: can't set uart port param.");
                  return FALSE;
                }
              break;

            case HYSCAN_SENSOR_PORT_UDP_IP:
              if (!hyscan_sonar_control_model_sensor_set_udp_ip_port_param (scm, port_name,
                                                                            prm->channel,
                                                                            prm->time_offset,
                                                                            prm->udp_ip.protocol,
                                                                            prm->udp_ip.addr,
                                                                            prm->udp_ip.port))
                {
                  g_warning ("HyScanSonarModel: can't set udp/ip port param.");
                  return FALSE;
                }
              break;

            default:
              g_warning ("HyScanSonarModel: invalid port type.");
              return FALSE;
            }
        }
    }

  return TRUE;
}

/* Обновляет параметры источника. */
static gboolean
hyscan_sonar_model_update_src_params (HyScanSonarModel *model,
                                      HyScanSourceType  source_type,
                                      HyScanSrcParams  *prm)
{
  HyScanSonarControlModel *scm = model->priv->sonar_control_model;

  /* Обновление местоположения источника. */
  if (prm->position.modified)
    {
      prm->position.modified = FALSE;
      if (!hyscan_sonar_control_model_sonar_set_position (scm, source_type, &prm->position.nval))
        {
          g_warning ("Can't set position.");
          return FALSE;
        }
      prm->position.cval = prm->position.nval;
    }

  /* Обновилось время приёма. */
  if ((model->priv->force_update || hyscan_sonar_model_get_record_state (model)) && prm->receive_time.modified)
    {
      prm->receive_time.modified = FALSE;
      if (!hyscan_sonar_control_model_sonar_set_receive_time (scm, source_type, prm->receive_time.nval))
        {
          g_warning ("Can't set receive time.");
          return FALSE;
        }
      prm->receive_time.cval = prm->receive_time.nval;
    }

  return TRUE;
}

/* Обновляет параметры генератора. */
static gboolean
hyscan_sonar_model_update_gen_params (HyScanSonarModel *model,
                                      HyScanSourceType  source_type,
                                      HyScanGenParams  *prm)
{
  HyScanSonarControlModel *scm = model->priv->sonar_control_model;

  /* Включение генератора источника. */
  if (prm->enabled.modified)
    {
      prm->enabled.modified = FALSE;
      if (!hyscan_sonar_control_model_generator_set_enable (scm, source_type, prm->enabled.nval))
        {
          g_warning ("Can't enable generator.");
          return FALSE;
        }
      prm->enabled.cval = prm->enabled.nval;
    }

  /* Изменение режима генератора. */
  if ((model->priv->force_update || hyscan_sonar_model_get_record_state (model)) &&
      prm->enabled.cval && prm->mode.modified)
    {
      prm->mode.modified = FALSE;

      switch (prm->mode.nval)
        {
        case HYSCAN_GENERATOR_MODE_PRESET:
          if (!hyscan_sonar_control_model_generator_set_preset (scm, source_type, prm->preset_prm.preset))
            {
              g_warning ("Can't set preset.");
              return FALSE;
            }
          break;

        case HYSCAN_GENERATOR_MODE_AUTO:
          if (!hyscan_sonar_control_model_generator_set_auto (scm, source_type, prm->auto_prm.signal_type))
            {
              g_warning ("Can't set generator auto.");
              return FALSE;
            }
          break;

        case HYSCAN_GENERATOR_MODE_SIMPLE:
          if (!hyscan_sonar_control_model_generator_set_simple (scm, source_type,
                                                                prm->simple_prm.signal_type,
                                                                prm->simple_prm.power))
            {
              g_warning ("Can't set generator simple.");
              return FALSE;
            }
          break;

        case HYSCAN_GENERATOR_MODE_EXTENDED:
          if (!hyscan_sonar_control_model_generator_set_extended (scm, source_type,
                                                                  prm->extended_prm.signal_type,
                                                                  prm->extended_prm.duration,
                                                                  prm->extended_prm.power))
            {
              g_warning ("Can't set generator extended.");
              return FALSE;
            }
          break;

        default:
          g_warning ("Generator unexpected: %d.", prm->mode.nval);
          return FALSE;
        }

      prm->mode.cval = prm->mode.nval;
    }

  return TRUE;
}

/* Обновляет параметры ВАРУ. */
static gboolean
hyscan_sonar_model_update_tvg_params (HyScanSonarModel *model,
                                      HyScanSourceType  source_type,
                                      HyScanTVGParams  *prm)
{
  HyScanSonarControlModel *scm = model->priv->sonar_control_model;

  /* Включение ВАРУ. */
  if (prm->enabled.modified)
    {
      prm->enabled.modified = FALSE;
      if (!hyscan_sonar_control_model_tvg_set_enable (scm, source_type, prm->enabled.nval))
        {
          g_warning ("Can't set generator auto.");
          return FALSE;
        }
      prm->enabled.cval = prm->enabled.nval;
    }

  /* Изменение режима ВАРУ. */
  if ((model->priv->force_update || hyscan_sonar_model_get_record_state (model)) &&
      prm->enabled.cval && prm->mode.modified)
    {
      prm->mode.modified = FALSE;

      /* Применение параметров ВАРУ. */
      switch (prm->mode.nval)
        {
        case HYSCAN_TVG_MODE_AUTO:
          if (!hyscan_sonar_control_model_tvg_set_auto (scm, source_type,
                                                        prm->auto_prm.level,
                                                        prm->auto_prm.sensitivity))
            {
              g_warning ("Can't set TVG auto.");
              return FALSE;
            }
          break;

        case HYSCAN_TVG_MODE_CONSTANT:
          if (!hyscan_sonar_control_model_tvg_set_constant (scm, source_type, prm->const_prm.gain))
            {
              g_warning ("Can't set TVG const.");
              return FALSE;
            }
          break;

        case HYSCAN_TVG_MODE_LINEAR_DB:
          if (!hyscan_sonar_control_model_tvg_set_linear_db (scm, source_type,
                                                             prm->lin_db_prm.gain0,
                                                             prm->lin_db_prm.step))
            {
              g_warning ("Can't set TVG linear.");
              return FALSE;
            }
          break;

        case HYSCAN_TVG_MODE_LOGARITHMIC:
          if (!hyscan_sonar_control_model_tvg_set_logarithmic (scm, source_type,
                                                               prm->log_prm.gain0,
                                                               prm->log_prm.beta,
                                                               prm->log_prm.alpha))
            {
              g_warning ("Can't set TVG log.");
              return FALSE;
            }
          break;

        default:
          g_warning ("TVG unexpected: %d.", prm->mode.nval);
          return FALSE;
        }

      prm->mode.cval = prm->mode.nval;
    }

  return TRUE;
}

/* Обновляет параметры датчиков. */
static gboolean
hyscan_sonar_model_update_sensors (HyScanSonarModel *model)
{
  gchar **port;

  if (model->priv->ports == NULL)
    return TRUE;

  /* Поиск изменений в параметрах датчиков. */
  for (port = model->priv->ports; *port != NULL; ++port)
    if (!hyscan_sonar_model_update_sensor_params (model, *port))
      return FALSE;

  return TRUE;
}

/* Обновляет параметры источников. */
static gboolean
hyscan_sonar_model_update_sources (HyScanSonarModel *model)
{
  HyScanSourceType *source;

  if (model->priv->sources == NULL)
    return TRUE;

  for (source = model->priv->sources; *source != HYSCAN_SOURCE_INVALID; ++source)
    {
      HyScanSrcParamsContainer *prm = NULL;
      HyScanSourceType source_type = (HyScanSourceType) *source;

      if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
        continue;

      if (!(hyscan_sonar_model_update_gen_params (model, source_type, &prm->gen) &&
            hyscan_sonar_model_update_tvg_params (model, source_type, &prm->tvg) &&
            hyscan_sonar_model_update_src_params (model, source_type, &prm->src)))
        {
          return FALSE;
        }
    }

  return TRUE;
}

/* Обновляет параметры гидролокатора. */
static gboolean
hyscan_sonar_model_update_sonar (HyScanSonarModel *model)
{
  HyScanSonarParams *prm = &model->priv->sonar_params;
  HyScanSonarControlModel *scm = model->priv->sonar_control_model;

  /* Изменение типа синхронизации. */
  if ((model->priv->force_update || hyscan_sonar_model_get_record_state (model)) && prm->sync_type.modified)
    {
      prm->sync_type.modified = FALSE;
      if (!hyscan_sonar_control_model_sonar_set_sync_type (scm, prm->sync_type.nval))
        {
          g_warning ("Can't set sync.");
          return FALSE;
        }
      prm->sync_type.cval = prm->sync_type.nval;
    }

  /* Изменение статуса записи. */
  if (prm->record_state.modified)
    {
      prm->record_state.modified = FALSE;
      if (prm->record_state.nval)
        {
          gchar *track_name = hyscan_sonar_model_generate_track_name (model);
          if (!hyscan_sonar_control_model_sonar_start (scm, track_name, prm->track_type))
            {
              g_warning ("Can't start sonar.");
              g_free (track_name);
              return FALSE;
            }

          /* Задать имя записываемого галса. */
          g_free (prm->track_name);
          prm->track_name = track_name;

          /* Уведомить потребителей об изменении записываемого галса. */
          g_signal_emit (model, hyscan_sonar_model_signals [SIGNAL_ACTIVE_TRACK_CHANGED], 0, track_name);
        }
      else if (!hyscan_sonar_control_model_sonar_stop (scm))
        {
          g_warning ("Can't stop sonar.");
          return FALSE;
        }
      prm->record_state.cval = prm->record_state.nval;
    }

  return TRUE;
}

/* Проверяет наличие изменений и применяет их. */
static gboolean
hyscan_sonar_model_check_for_updates (gpointer sonar_model_ptr)
{
  HyScanSonarModel *model = HYSCAN_SONAR_MODEL (sonar_model_ptr);
  HyScanSonarModelPrivate *priv = model->priv;

  /* При принудительном обновлении, изменения применяются без таймера. */
  if (!priv->force_update)
    {
      gdouble elapsed;

      /* Проверить флаг наличия изменённых параметров. */
      if (!priv->update)
        return G_SOURCE_CONTINUE;

      elapsed = g_timer_elapsed (priv->check_for_updates_timer, NULL) * G_TIME_SPAN_MILLISECOND;
      if (elapsed < HYSCAN_SONAR_MODEL_UPDATE_TIMEOUT)
        return G_SOURCE_CONTINUE;
    }

  /* Сброс флага наличия изменений. */
  priv->update = FALSE;

  /* Остановка таймера последнего изменения параметров. */
  g_timer_stop (priv->check_for_updates_timer);

  /* Обновление параметров датчиков, параметров источников данных, основных параметров гидролокатора. */
  if (hyscan_sonar_model_update_sensors (model) &&
      hyscan_sonar_model_update_sources (model) &&
      hyscan_sonar_model_update_sonar (model) &&
      hyscan_async_execute (HYSCAN_ASYNC (priv->sonar_control_model)))
    {
      hyscan_sonar_model_set_sonar_control_state (model, FALSE);
    }

  /* Сброс флага принудительного обновления делается в любом случае. */
  priv->force_update = FALSE;

  return G_SOURCE_CONTINUE;
}

/* Создаёт объект HyScanSonarModel. */
HyScanSonarModel *
hyscan_sonar_model_new (HyScanSonarControl *sonar_control,
                        HyScanDBInfo       *db_info)
{
  g_return_val_if_fail (HYSCAN_IS_DB_INFO (db_info), NULL);
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (sonar_control), NULL);

  return g_object_new (HYSCAN_TYPE_SONAR_MODEL,
                       "sonar-control", sonar_control,
                       "db-info", db_info,
                       NULL);
}

/* Вызывает отправку изменений вне зависимости от режима работы гидролокатора. */
void hyscan_sonar_model_flush (HyScanSonarModel *model)
{
  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));
  model->priv->force_update = TRUE;
}

/* Проверяет состояние системы управления гидролокатором. */
gboolean
hyscan_sonar_model_get_sonar_control_state (HyScanSonarModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), FALSE);
  return model->priv->sonar_control_state;
}

/* Возвращает копию объекта HyScanSonarControl, связанную с этой моделью. */
HyScanSonarControl *
hyscan_sonar_model_get_sonar_control (HyScanSonarModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), NULL);
  return HYSCAN_IS_SONAR_CONTROL (model->priv->sonar_control) ? g_object_ref (model->priv->sonar_control) : NULL;
}

/* Включает или выключает приём данных на указанном порту. */
void
hyscan_sonar_model_sensor_set_enable (HyScanSonarModel *model,
                                      const gchar      *port_name,
                                      gboolean          enabled)
{
  HyScanSonarModelPrivate *priv;
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sensors_params, port_name)) == NULL)
    return;

  prm->enabled.nval = enabled;
  prm->enabled.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL. */
void
hyscan_sonar_model_sensor_set_virtual_params (HyScanSonarModel *model,
                                              const gchar      *port_name,
                                              guint             channel,
                                              gint64            time_offset)
{
  HyScanSonarModelPrivate *priv;
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sensors_params, port_name)) == NULL)
    return;

  prm->channel = channel;
  prm->time_offset = time_offset;
  prm->modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART. */
void
hyscan_sonar_model_sensor_set_uart_params (HyScanSonarModel         *model,
                                           const gchar              *port_name,
                                           guint                     channel,
                                           gint64                    time_offset,
                                           HyScanSensorProtocolType  protocol,
                                           guint                     device,
                                           guint                     mode)
{
  HyScanSonarModelPrivate *priv;
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sensors_params, port_name)) == NULL)
    return;

  prm->channel = channel;
  prm->time_offset = time_offset;
  prm->uart.protocol = protocol;
  prm->uart.device = device;
  prm->uart.mode = mode;
  prm->modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP. */
void
hyscan_sonar_model_sensor_set_udp_ip_params (HyScanSonarModel         *model,
                                             const gchar              *port_name,
                                             guint                     channel,
                                             gint64                    time_offset,
                                             HyScanSensorProtocolType  protocol,
                                             guint                     addr,
                                             guint16                   port)
{
  HyScanSonarModelPrivate *priv;
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sensors_params, port_name)) == NULL)
    return;

  prm->channel = channel;
  prm->time_offset = time_offset;
  prm->udp_ip.protocol = protocol;
  prm->udp_ip.addr = addr;
  prm->udp_ip.port = port;
  prm->modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт информацию о местоположении датчика относительно центра масс судна. */
void
hyscan_sonar_model_sensor_set_position (HyScanSonarModel      *model,
                                        const gchar           *port_name,
                                        HyScanAntennaPosition  position)
{
  HyScanSonarModelPrivate *priv;
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sensors_params, port_name)) == NULL)
    return;

  prm->position.nval = position;
  prm->position.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Проверяет, включен ли приём данных по указанному порту. */
gboolean
hyscan_sonar_model_sensor_is_enabled (HyScanSonarModel *model,
                                      const gchar      *port_name)
{
  HyScanSensorParams *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), FALSE);

  if ((prm = g_hash_table_lookup (model->priv->sensors_params, port_name)) == NULL)
    return FALSE;

  return prm->enabled.cval;
}

/* Получает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL. */
void
hyscan_sonar_model_sensor_get_virtual_params (HyScanSonarModel *model,
                                              const gchar      *port_name,
                                              guint            *channel,
                                              gint64           *time_offset)
{
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sensors_params, port_name)) == NULL)
    return;

  if (channel != NULL)
    *channel = prm->channel;
  if (time_offset != NULL)
    *time_offset = prm->time_offset;
}

/* Получает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART. */
void
hyscan_sonar_model_sensor_get_uart_params (HyScanSonarModel         *model,
                                           const gchar              *port_name,
                                           guint                    *channel,
                                           gint64                   *time_offset,
                                           HyScanSensorProtocolType *protocol,
                                           guint                    *device,
                                           guint                    *mode)
{
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sensors_params, port_name)) == NULL)
    return;

  if (channel != NULL)
    *channel = prm->channel;
  if (time_offset != NULL)
    *time_offset = prm->time_offset;
  if (protocol != NULL)
    *protocol = prm->uart.protocol;
  if (device != NULL)
    *device = prm->uart.device;
  if (mode != NULL)
    *mode = prm->uart.mode;
}

/* Получает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP. */
void
hyscan_sonar_model_sensor_get_udp_ip_params (HyScanSonarModel         *model,
                                             const gchar              *port_name,
                                             guint                    *channel,
                                             gint64                   *time_offset,
                                             HyScanSensorProtocolType *protocol,
                                             guint                    *addr,
                                             guint16                  *port)
{
  HyScanSensorParams *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sensors_params, port_name)) == NULL)
    return;

  if (channel != NULL)
    *channel = prm->channel;
  if (time_offset != NULL)
    *time_offset = prm->time_offset;
  if (protocol != NULL)
    *protocol = prm->udp_ip.protocol;
  if (addr != NULL)
    *addr = prm->udp_ip.addr;
  if (port != NULL)
    *port = prm->udp_ip.port;
}

/* Получает информацию о местоположении приёмных антенн относительно центра масс судна. */
HyScanAntennaPosition *
hyscan_sonar_model_sensor_get_position (HyScanSonarModel *model,
                                        const gchar      *port_name)
{
  HyScanSensorParams *prm;
  HyScanAntennaPosition *pos;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), NULL);

  if ((prm = g_hash_table_lookup (model->priv->sensors_params, port_name)) == NULL)
    return NULL;

  pos = g_new0 (HyScanAntennaPosition, 1);
  *pos = prm->position.cval;

  return pos;
}

/* Задаёт преднастроенный режим работы генератора. */
void
hyscan_sonar_model_gen_set_preset (HyScanSonarModel *model,
                                   HyScanSourceType  source_type,
                                   guint             preset)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->gen.preset_prm.preset = preset;

  prm->gen.mode.nval = HYSCAN_GENERATOR_MODE_PRESET;
  prm->gen.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт автоматический режим работы генератора. */
void
hyscan_sonar_model_gen_set_auto (HyScanSonarModel          *model,
                                 HyScanSourceType           source_type,
                                 HyScanGeneratorSignalType  signal_type)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->gen.auto_prm.signal_type = signal_type;

  prm->gen.mode.nval = HYSCAN_GENERATOR_MODE_AUTO;
  prm->gen.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт упрощённый режим работы генератора. */
void
hyscan_sonar_model_gen_set_simple (HyScanSonarModel          *model,
                                   HyScanSourceType           source_type,
                                   HyScanGeneratorSignalType  signal_type,
                                   gdouble                    power)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->gen.simple_prm.signal_type = signal_type;
  prm->gen.simple_prm.power = power;

  prm->gen.mode.nval = HYSCAN_GENERATOR_MODE_SIMPLE;
  prm->gen.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт расширенный режим работы генератора. */
void
hyscan_sonar_model_gen_set_extended (HyScanSonarModel          *model,
                                     HyScanSourceType           source_type,
                                     HyScanGeneratorSignalType  signal_type,
                                     gdouble                    duration,
                                     gdouble                    power)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->gen.extended_prm.signal_type = signal_type;
  prm->gen.extended_prm.duration = duration;
  prm->gen.extended_prm.power = power;

  prm->gen.mode.nval = HYSCAN_GENERATOR_MODE_EXTENDED;
  prm->gen.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Включает или выключает формирование сигнала генератором. */
void
hyscan_sonar_model_gen_set_enable (HyScanSonarModel *model,
                                   HyScanSourceType  source_type,
                                   gboolean          enabled)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->gen.enabled.nval = enabled;
  prm->gen.enabled.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Проверяет, включено или выключено формирование сигнала генератором. */
gboolean
hyscan_sonar_model_gen_is_enabled (HyScanSonarModel *model,
                                   HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), FALSE);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return FALSE;

  return prm->gen.enabled.cval;
}

/* Получает режим работы генератора. */
HyScanGeneratorModeType
hyscan_sonar_model_gen_get_mode (HyScanSonarModel *model,
                                 HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), HYSCAN_GENERATOR_MODE_INVALID);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return HYSCAN_GENERATOR_MODE_INVALID;

  return prm->gen.mode.cval;
}

/* Получает параметры преднастроенного режима работы генератора. */
void
hyscan_sonar_model_gen_get_preset_params (HyScanSonarModel *model,
                                          HyScanSourceType  source_type,
                                          guint            *preset)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (preset != NULL)
    *preset = prm->gen.preset_prm.preset;
}

/* Получает параметры автоматического режима работы генератора. */
void
hyscan_sonar_model_gen_get_auto_params (HyScanSonarModel          *model,
                                        HyScanSourceType           source_type,
                                        HyScanGeneratorSignalType *signal_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (signal_type != NULL)
    *signal_type = prm->gen.auto_prm.signal_type;
}

/* Получает параметры упрощённого режима работы генератора. */
void
hyscan_sonar_model_gen_get_simple_params (HyScanSonarModel          *model,
                                          HyScanSourceType           source_type,
                                          HyScanGeneratorSignalType *signal_type,
                                          gdouble                   *power)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (signal_type != NULL)
    *signal_type = prm->gen.simple_prm.signal_type;
  if (power != NULL)
    *power = prm->gen.simple_prm.power;
}

/* Получает параметры расширенного режима работы генератора. */
void
hyscan_sonar_model_gen_get_extended_params (HyScanSonarModel          *model,
                                            HyScanSourceType           source_type,
                                            HyScanGeneratorSignalType *signal_type,
                                            gdouble                   *duration,
                                            gdouble                   *power)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (signal_type != NULL)
    *signal_type = prm->gen.extended_prm.signal_type;
  if (duration != NULL)
    *duration = prm->gen.extended_prm.duration;
  if (power != NULL)
    *power = prm->gen.extended_prm.power;
}

/* Задаёт автоматический режим управления системой ВАРУ. */
void
hyscan_sonar_model_tvg_set_auto (HyScanSonarModel *model,
                                 HyScanSourceType  source_type,
                                 gdouble           level,
                                 gdouble           sensitivity)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->tvg.auto_prm.level = level;
  prm->tvg.auto_prm.sensitivity = sensitivity;

  prm->tvg.mode.nval = HYSCAN_TVG_MODE_AUTO;
  prm->tvg.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт постоянный уровень усиления системой ВАРУ. */
void
hyscan_sonar_model_tvg_set_constant (HyScanSonarModel *model,
                                     HyScanSourceType  source_type,
                                     gdouble           gain)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->tvg.const_prm.gain = gain;

  prm->tvg.mode.nval = HYSCAN_TVG_MODE_CONSTANT;
  prm->tvg.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт линейное увеличение усиления в дБ на 100 метров. */
void
hyscan_sonar_model_tvg_set_linear_db (HyScanSonarModel *model,
                                      HyScanSourceType  source_type,
                                      gdouble           gain0,
                                      gdouble           step)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->tvg.lin_db_prm.gain0 = gain0;
  prm->tvg.lin_db_prm.step = step;

  prm->tvg.mode.nval = HYSCAN_TVG_MODE_LINEAR_DB;
  prm->tvg.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт логарифмический вид закона усиления системой ВАРУ. */
void
hyscan_sonar_model_tvg_set_logarithmic (HyScanSonarModel *model,
                                        HyScanSourceType  source_type,
                                        gdouble           gain0,
                                        gdouble           beta,
                                        gdouble           alpha)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->tvg.log_prm.gain0 = gain0;
  prm->tvg.log_prm.beta = beta;
  prm->tvg.log_prm.alpha = alpha;

  prm->tvg.mode.nval = HYSCAN_TVG_MODE_LOGARITHMIC;
  prm->tvg.mode.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Функция включает или выключает систему ВАРУ. */
void
hyscan_sonar_model_tvg_set_enable (HyScanSonarModel *model,
                                   HyScanSourceType  source_type,
                                   gboolean          enabled)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->tvg.enabled.nval = enabled;
  prm->tvg.enabled.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Проверяет, включена или выключена система ВАРУ. */
gboolean
hyscan_sonar_model_tvg_is_enabled (HyScanSonarModel *model,
                                   HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), FALSE);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return FALSE;

  return prm->tvg.enabled.cval;
}

/* Получает режим работы системы ВАРУ. */
HyScanTVGModeType
hyscan_sonar_model_tvg_get_mode (HyScanSonarModel *model,
                                 HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), HYSCAN_TVG_MODE_INVALID);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return HYSCAN_TVG_MODE_INVALID;

  return prm->tvg.mode.cval;
}

/* Проверяет, работает ли система ВАРУ в полностью автоматическом режиме. */
gboolean
hyscan_sonar_model_tvg_is_auto (HyScanSonarModel *model,
                                HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), FALSE);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return FALSE;

  return prm->tvg.auto_prm.sensitivity < 0 || prm->tvg.auto_prm.level < 0;
}

/* Получает параметры автоматического режим управления системой ВАРУ. */
void
hyscan_sonar_model_tvg_get_auto_params (HyScanSonarModel *model,
                                        HyScanSourceType  source_type,
                                        gdouble          *level,
                                        gdouble          *sensitivity)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (level != NULL)
    *level = prm->tvg.auto_prm.level;
  if (sensitivity != NULL)
    *sensitivity = prm->tvg.auto_prm.sensitivity;
}

/* Получает параметры режима управления системой ВАРУ при постоянном уровне усиления. */
void
hyscan_sonar_model_tvg_get_const_params (HyScanSonarModel *model,
                                         HyScanSourceType  source_type,
                                         gdouble          *gain)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (gain != NULL)
    *gain = prm->tvg.const_prm.gain;
}

/* Получает параметры режима управления усилением системой ВАРУ по линейному закону. */
void
hyscan_sonar_model_tvg_get_linear_db_params (HyScanSonarModel *model,
                                             HyScanSourceType  source_type,
                                             gdouble          *gain,
                                             gdouble          *step)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (gain != NULL)
    *gain = prm->tvg.lin_db_prm.gain0;
  if (step != NULL)
    *step = prm->tvg.lin_db_prm.step;
}

/* Получает параметры режима управления усилением системой ВАРУ по логарифмическому закону. */
void
hyscan_sonar_model_tvg_get_logarithmic_params (HyScanSonarModel *model,
                                               HyScanSourceType  source_type,
                                               gdouble          *gain,
                                               gdouble          *beta,
                                               gdouble          *alpha)
{
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  if (gain != NULL)
    *gain = prm->tvg.log_prm.gain0;
  if (beta != NULL)
    *beta = prm->tvg.log_prm.beta;
  if (alpha != NULL)
    *alpha = prm->tvg.log_prm.alpha;
}

/* Задаёт информацию о местоположении приёмных антенн относительно центра масс судна. */
void
hyscan_sonar_model_sonar_set_position (HyScanSonarModel      *model,
                                       HyScanSourceType       source_type,
                                       HyScanAntennaPosition  position)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->src.position.nval = position;
  prm->src.position.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт время приёма эхосигнала источником данных. */
void
hyscan_sonar_model_set_receive_time (HyScanSonarModel *model,
                                     HyScanSourceType  source_type,
                                     gdouble           receive_time)
{
  HyScanSonarModelPrivate *priv;
  HyScanSrcParamsContainer *prm;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  if ((prm = g_hash_table_lookup (priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return;

  prm->src.receive_time.nval = receive_time;
  prm->src.receive_time.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт дальность работы гидролокатора. */
void
hyscan_sonar_model_set_distance (HyScanSonarModel *model,
                                 HyScanSourceType  source_type,
                                 gdouble           distance)
{
  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));
  hyscan_sonar_model_set_receive_time (model, source_type, 2.0 * distance / model->priv->sound_velocity);
}

/* Задаёт тип синхронизации излучения. */
void
hyscan_sonar_model_set_sync_type (HyScanSonarModel    *model,
                                  HyScanSonarSyncType  sync_type)
{
  HyScanSonarModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  priv->sonar_params.sync_type.nval = sync_type;
  priv->sonar_params.sync_type.modified = TRUE;

  priv->update = TRUE;
  g_timer_start (priv->check_for_updates_timer);
}

/* Задаёт тип следующего записываемого галса. */
void
hyscan_sonar_model_set_track_type (HyScanSonarModel *model,
                                   HyScanTrackType   track_type)
{
  HyScanSonarModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  priv->sonar_params.track_type = track_type;
}

/* Переводит гидролокатор в рабочий режим и включает запись данных. */
void
hyscan_sonar_model_sonar_start (HyScanSonarModel *model)
{
  HyScanSonarModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  priv->sonar_params.record_state.nval = TRUE;
  priv->sonar_params.record_state.modified = TRUE;
  priv->force_update = TRUE;
}

/* Переводит гидролокатор в ждущий режим и отключает запись данных. */
void
hyscan_sonar_model_sonar_stop (HyScanSonarModel *model)
{
  HyScanSonarModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  priv->sonar_params.record_state.nval = FALSE;
  priv->sonar_params.record_state.modified = TRUE;
  priv->force_update = TRUE;
}

/* Выполняет один цикл зондирования и приёма данных. */
void
hyscan_sonar_model_sonar_ping (HyScanSonarModel *model)
{
  HyScanSonarModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  if (!priv->sonar_control_state)
    return;

  priv->sonar_params.ping_state.nval = TRUE;
  priv->sonar_params.ping_state.modified = TRUE;
  priv->force_update = TRUE;
}

/* Получает информацию о местоположении приёмных антенн относительно центра масс судна. */
HyScanAntennaPosition *
hyscan_sonar_model_sonar_get_position (HyScanSonarModel *model,
                                       HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;
  HyScanAntennaPosition *pos;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), NULL);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return NULL;

  pos = g_new (HyScanAntennaPosition, 1);
  *pos = prm->src.position.cval;

  return pos;
}

/* Получает время приёма эхосигнала источником данных. */
gdouble
hyscan_sonar_model_get_receive_time (HyScanSonarModel *model,
                                     HyScanSourceType  source_type)
{
  HyScanSrcParamsContainer *prm;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), -G_MAXDOUBLE);

  if ((prm = g_hash_table_lookup (model->priv->sources_params, GINT_TO_POINTER (source_type))) == NULL)
    return -G_MAXDOUBLE;

  return prm->src.receive_time.cval;
}

/* Получает дальность источника данных. */
gdouble
hyscan_sonar_model_get_distance (HyScanSonarModel *model,
                                 HyScanSourceType  source_type)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), -G_MAXDOUBLE);
  return hyscan_sonar_model_get_receive_time (model, source_type) * model->priv->sound_velocity / 2.0;
}

/* Получает максимальную дальность источника данных. */
gdouble
hyscan_sonar_model_get_max_distance (HyScanSonarModel *model,
                                     HyScanSourceType  source_type)
{
  HyScanSonarModelPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), -G_MAXDOUBLE);

  priv = model->priv;

  return hyscan_sonar_control_get_max_receive_time (priv->sonar_control, source_type) * priv->sound_velocity / 2.0;
}

/* Получает скорость звука. */
gdouble
hyscan_sonar_model_get_sound_velocity (HyScanSonarModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), -G_MAXDOUBLE);
  return model->priv->sound_velocity;
}

/* Получает тип синхронизации. */
HyScanSonarSyncType
hyscan_sonar_model_get_sync_type (HyScanSonarModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), HYSCAN_SONAR_SYNC_INVALID);
  return model->priv->sonar_params.sync_type.cval;
}

/* Получает тип галса. */
HyScanTrackType
hyscan_sonar_model_get_track_type (HyScanSonarModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), HYSCAN_TRACK_UNSPECIFIED);
  return model->priv->sonar_params.track_type;
}

/* Получает состояние записи. */
gboolean
hyscan_sonar_model_get_record_state (HyScanSonarModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), FALSE);
  return model->priv->sonar_params.record_state.cval;
}
