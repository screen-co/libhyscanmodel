/* hyscan-control-model.c
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanModel.
 *
 * HyScanModel is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanModel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanModel имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanModel на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-control-model
 * @Title HyScanControlModel
 * @Short_description Асинхронная модель управления гидролокатором.
 *
 * Класс #HyScanControlModel предназначен для асинхронного управления
 * гидролокатором и датчиками, зарегистрированными в #HyScanControl.
 *
 * Класс реализует интерфейсы:
 *
 * - HyScanParam: управление внутренними параметрами устройств (синхронно).
 * - HyScanSonar: управление локатором (асинхронно),
 * - HyScanSensor: управление датчиками (синхронно),
 * - HyScanDevice: управление устройством (синхронно),
 * - HyScanSonarState: состояние локатора,
 * - HyScanSensorState: состояние датчиков,
 * - HyScanDeviceState: состояние устройства.
 *
 * Вызовы #HyScanParam, #HyScanSensor и #HyScanDevice напрямую транслируются в используемый #HyScanControl.
 *
 * ## Управление локатором
 *
 * Интерфейс #HyScanSonar реализован асинхронно для функций hyscan_sonar_start(),
 * hyscan_sonar_stop() и hyscan_sonar_sync().
 *
 * Синхронизация состояния гидролокатора hyscan_sonar_sync() происходит автоматически
 * после изменения какого-либо из параметров. Несколько последовательных изменений
 * параметров объединяются в одну синхронизацию, если между ними прошло время
 * не больше периода синхронизации. Период синхронизации можно задать в функции
 * hyscan_control_model_set_sync_timeout().
 *
 * Установка параметров старта локатора hyscan_sonar_start() может быть произведена
 * в различных частях программы с помощью функций конфигурации старта:
 *
 * - hyscan_control_model_set_project(),
 * - hyscan_control_model_set_track_name(),
 * - hyscan_control_model_set_track_type(),
 * - hyscan_control_model_set_plan().
 *
 * Название галса может быть сгенерировано автоматически, если установить %NULL в
 * hyscan_control_model_set_track_name(). Также для сгенерированного названия
 * будет применён суффикс, указанный в hyscan_control_model_set_track_sfx().
 *
 * Параметры, указанные в этих функция будут использованы при последующем вызове
 * hyscan_control_model_start().
 *
 * ## Состояние локаторов и датчиков
 *
 * Все установленные параметры локатора и датчиков запоминаются моделью и
 * могут быть получены при помощи интерфейсов #HyScanSonarState и #HyScanSensorState.
 *
 */

#include "hyscan-control-model.h"
#include <hyscan-data-box.h>
#include <hyscan-model-marshallers.h>

#define HYSCAN_CONTROL_MODEL_TIMEOUT (1000 /* Одна секунда*/)

typedef struct _HyScanControlModelStart HyScanControlModelStart;
typedef union  _HyScanControlModelTVG HyScanControlModelTVG;
typedef struct _HyScanControlModelTVGAuto HyScanControlModelTVGAuto;
typedef struct _HyScanControlModelTVGConst HyScanControlModelTVGConst;
typedef struct _HyScanControlModelTVGLinear HyScanControlModelTVGLinear;
typedef struct _HyScanControlModelTVGLog HyScanControlModelTVGLog;
typedef struct _HyScanControlModelRec HyScanControlModelRec;
typedef struct _HyScanControlModelGen HyScanControlModelGen;
typedef struct _HyScanControlModelSource HyScanControlModelSource;
typedef struct _HyScanControlModelSensor HyScanControlModelSensor;
typedef union  _HyScanControlModelActuator HyScanControlModelActuator;
typedef struct _HyScanControlModelActuatorManual HyScanControlModelActuatorManual;
typedef struct _HyScanControlModelActuatorScan HyScanControlModelActuatorScan;

enum
{
  PROP_0,
  PROP_CONTROL,
};

enum
{
  SIGNAL_SONAR,
  SIGNAL_SENSOR,
  SIGNAL_ACTUATOR,
  SIGNAL_BEFORE_START,
  SIGNAL_START_STOP,
  SIGNAL_START_SENT,
  SIGNAL_STOP_SENT,
  SIGNAL_LAST
};

enum
{
  SONAR_NO_CHANGE,
  SONAR_START,
  SONAR_STOP,
};

/* Параметры включения работы локатора, см. hyscan_sonar_start() и hyscan_control_model_start(). */
struct _HyScanControlModelStart
{
  HyScanTrackType             track_type;          /* Тип галса. */
  gchar                      *project;             /* Название проекта. */
  gchar                      *track;               /* Название галса. */
  HyScanTrackPlan            *plan;                /* План галса. */
  gboolean                    generate_name;       /* Необходимо сгенерировать название галса. */
  gchar                      *generate_suffix;     /* Суффикс для генерации названия галса. */
};

/* Параметры автоматического режима системы ВАРУ, см. hyscan_sonar_tvg_set_auto(). */
struct _HyScanControlModelTVGAuto
{
  HyScanSonarTVGModeType      mode;                 /* Режим работы системы ВАРУ. */
  gdouble                     level;                /* Целевой уровень сигнала. */
  gdouble                     sensitivity;          /* Чувствительность автомата регулировки. */
};

/* Параметры постоянного уровня усиления системы ВАРУ, см. hyscan_sonar_tvg_set_constant(). */
struct _HyScanControlModelTVGConst
{
  HyScanSonarTVGModeType      mode;                 /* Режим работы системы ВАРУ. */
  gdouble                     gain;                 /* Уровень усиления, дБ. */
};

/* Параметры линейного увеличения уровня усиления системы ВАРУ, см. hyscan_sonar_tvg_set_linear_db(). */
struct _HyScanControlModelTVGLinear
{
  HyScanSonarTVGModeType      mode;                 /* Режим работы системы ВАРУ. */
  gdouble                     gain0;                /* Начальный уровень усиления, дБ. */
  gdouble                     gain_step;            /* Величина изменения усиления каждые 100 метров, дБ. */
};

/* Параметры логарифмического увеличения уровня усиления системы ВАРУ, см. hyscan_sonar_tvg_set_logarithmic(). */
struct _HyScanControlModelTVGLog
{
  HyScanSonarTVGModeType      mode;                 /* Режим работы системы ВАРУ. */
  gdouble                     gain0;                /* Начальный уровень усиления, дБ. */
  gdouble                     beta;                 /* Коэффициент поглощения цели, дБ. */
  gdouble                     alpha;                /* Коэффициент затухания, дБ/м. */
};

/* Параметры режимов работы системы ВАРУ. */
union _HyScanControlModelTVG
{
  HyScanSonarTVGModeType      mode;                 /* Режим работы системы ВАРУ. */
  HyScanControlModelTVGAuto   atvg;                 /* Параметры автоматического режима. */
  HyScanControlModelTVGConst  constant;             /* Параметры постоянного уровня усиления. */
  HyScanControlModelTVGLinear linear;               /* Параметры линейного увеличения уровня усиления. */
  HyScanControlModelTVGLog    log;                  /* Параметры логарифмического увеличения уровня усиления. */
};

/* Параметры работы приёмника. */
struct _HyScanControlModelRec
{
  HyScanSonarReceiverModeType mode;                 /* Режим работы приёмника. */
  gdouble                     receive;              /* Время приёма эхосигнала, секунды. */
  gdouble                     wait;                 /* Время задержки излучения после приёма, секунды. */
};

/* Параметры работы генератора. */
struct _HyScanControlModelGen
{
  gboolean                    disabled;             /* Генератор выключен. */
  gint64                      preset;               /* Режим работы генератора. */
};

/* Параметры работы генератора. */
struct _HyScanControlModelSource
{
  HyScanControlModelTVG       tvg;                  /* Параметры работы системы вару. */
  HyScanControlModelRec       rec;                  /* Параметры работы приёмника. */
  HyScanControlModelGen       gen;                  /* Параметры работы генератора. */
  HyScanAntennaOffset         offset;               /* Смещение локатора. */
};

/* Параметры датчика. */
struct _HyScanControlModelSensor
{
  gboolean                    enable;               /* Датчик включён. */
  HyScanAntennaOffset         offset;               /* Смещение датчика. */
};

/* Актуатор сканирует. */
struct _HyScanControlModelActuatorScan
{
  HyScanActuatorModeType      mode;                 /* Режим актуатора. */

  gdouble                     from;                 /* Начальный угол. */
  gdouble                     to;                   /* Конечный угол. */
  gdouble                     speed;                /* Скорость сканирования. */
};

/* Актуатор в ручном режиме. */
struct _HyScanControlModelActuatorManual
{
  HyScanActuatorModeType      mode;                 /* Режим актуатора. */
  gdouble                     angle;                /* Заданный угол. */
};

/* Параметры режимов работы актуатора. */
union _HyScanControlModelActuator
{
  HyScanActuatorModeType            mode;                /* Режим актуатора. */
  HyScanControlModelActuatorScan    scan;                /* Актуатор сканирует. */
  HyScanControlModelActuatorManual  manual;              /* Актуатор в ручном режиме. */
};

struct _HyScanControlModelPrivate
{
  HyScanControl              *control;         /* Бэкэнд. */

  GHashTable                 *sources;         /* Параметры источников гидролокационных данных. {HyScanSourceType : HyScanControlModelSource* } */
  GHashTable                 *sensors;         /* Параметры датчиков. {gchar* : HyScanControlModelSensor* } */
  GHashTable                 *actuators;       /* Параметры актуаторов. {gchar* : HyScanControlModelActuator* } */

  GThread                    *thread;          /* Поток. */
  gint                        shutdown;        /* Остановка потока, atomic. */
  gboolean                    wakeup;          /* Флаг для пробуждения потока. */
  GCond                       cond;            /* Сигнализатор изменения wakeup. */
  GMutex                      lock;            /* Блокировка. */

  gint                        start_stop;      /* Команда для старта или остановки работы локатора. */
  HyScanControlModelStart    *start;           /* Параметры для старта локатора. */
  HyScanControlModelStart    *started;         /* Параметры, с которыми был выполнен старт (внутреннее значение). */
  HyScanControlModelStart    *started_state;   /* Параметры, с которыми был выполнен старт (текущее значение). */
  guint                       sync_timeout;    /* Период ожидания новых изменений до синхронизации. */
  guint                       timeout_id;      /* Ид таймаут-функции, запускающей синхронизацию. */
  gboolean                    sync;            /* Флаг о необходимости синхронизации. */

  HyScanTrackType             track_type;      /* Тип галса. */
  HyScanTrackPlan            *plan;            /* План галса. */
  gchar                      *project_name;    /* Название проекта. */
  gchar                      *track_name;      /* Название галса. */
  gchar                      *suffix1;         /* Суффикс названия галса (одноразовый). */
  gchar                      *suffix;          /* Суффикс названия галса (постоянный). */
  gboolean                    disconnected;    /* Устройство отключено. */
  GList                      *sound_velocity;  /* Профиль скорости звука. */
};

static void     hyscan_control_model_sonar_state_iface_init    (HyScanSonarStateInterface      *iface);
static void     hyscan_control_model_sonar_iface_init          (HyScanSonarInterface           *iface);
static void     hyscan_control_model_sensor_state_iface_init   (HyScanSensorStateInterface     *iface);
static void     hyscan_control_model_sensor_iface_init         (HyScanSensorInterface          *iface);
static void     hyscan_control_model_device_state_iface_init   (HyScanDeviceStateInterface     *iface);
static void     hyscan_control_model_device_iface_init         (HyScanDeviceInterface          *iface);
static void     hyscan_control_model_actuator_state_iface_init (HyScanActuatorStateInterface   *iface);
static void     hyscan_control_model_actuator_iface_init       (HyScanActuatorInterface        *iface);
static void     hyscan_control_model_param_iface_init        (HyScanParamInterface           *iface);
static void     hyscan_control_model_set_property            (GObject                        *object,
                                                              guint                           prop_id,
                                                              const GValue                   *value,
                                                              GParamSpec                     *pspec);
static void     hyscan_control_model_object_constructed      (GObject                        *object);
static void     hyscan_control_model_object_finalize         (GObject                        *object);

static gboolean hyscan_control_model_receiver_set            (HyScanControlModel             *self,
                                                              HyScanSourceType                source,
                                                              HyScanControlModelRec          *rec);
static gboolean hyscan_control_model_gen_update              (HyScanControlModelGen          *dest,
                                                              HyScanControlModelGen          *src);
static gboolean hyscan_control_model_generator_set           (HyScanControlModel             *self,
                                                              HyScanSourceType                source,
                                                              HyScanControlModelGen          *gen);
static gboolean hyscan_control_model_tvg_update              (HyScanControlModelTVG          *dest,
                                                              HyScanControlModelTVG          *src);
static gboolean hyscan_control_model_tvg_set                 (HyScanControlModel             *self,
                                                              HyScanSourceType                source,
                                                              HyScanControlModelTVG          *tvg);
static gboolean hyscan_control_model_actuator_set            (HyScanControlModel             *self,
                                                              const gchar                    *name,
                                                              HyScanControlModelActuator     *info);
static gpointer hyscan_control_model_thread                  (gpointer                        data);
static void     hyscan_control_model_sensor_data             (HyScanDevice                   *device,
                                                              const gchar                    *sensor,
                                                              gint                            source,
                                                              gint64                          time,
                                                              HyScanBuffer                   *data,
                                                              HyScanControlModel             *self);
static void     hyscan_control_model_sonar_signal            (HyScanDevice                   *device,
                                                              gint                            source,
                                                              guint                           channel,
                                                              gint64                          time,
                                                              HyScanBuffer                   *image,
                                                              HyScanControlModel             *self);
static void     hyscan_control_model_sonar_tvg               (HyScanDevice                   *device,
                                                              gint                            source,
                                                              guint                           channel,
                                                              gint64                          time,
                                                              HyScanBuffer                   *gains,
                                                              HyScanControlModel             *self);
static void     hyscan_control_model_sonar_acoustic_data     (HyScanDevice                   *device,
                                                              gint                            source,
                                                              guint                           channel,
                                                              gboolean                        noise,
                                                              gint64                          time,
                                                              HyScanAcousticDataInfo         *info,
                                                              HyScanBuffer                   *data,
                                                              HyScanControlModel             *self);
static void     hyscan_control_model_device_state            (HyScanDevice                   *device,
                                                              const gchar                    *dev_id,
                                                              HyScanControlModel             *self);
static void     hyscan_control_model_device_log              (HyScanDevice                   *device,
                                                              const gchar                    *source,
                                                              gint64                          time,
                                                              gint                            level,
                                                              const gchar                    *message,
                                                              HyScanControlModel             *self);
static HyScanControlModelStart *
                hyscan_control_model_start_copy               (const HyScanControlModelStart *start);

static void     hyscan_control_model_start_free               (HyScanControlModelStart       *start);
static void     hyscan_control_model_send_start               (HyScanControlModel            *self,
                                                               const HyScanControlModelStart *start);
static gboolean hyscan_control_model_emit_before_start        (HyScanControlModel            *self);


static guint hyscan_control_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanControlModel, hyscan_control_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanControlModel)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR_STATE, hyscan_control_model_sonar_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_control_model_sonar_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR_STATE, hyscan_control_model_sensor_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_control_model_sensor_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE, hyscan_control_model_device_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE_STATE, hyscan_control_model_device_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_ACTUATOR, hyscan_control_model_actuator_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_ACTUATOR_STATE, hyscan_control_model_actuator_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_control_model_param_iface_init))

static void
hyscan_control_model_class_init (HyScanControlModelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_control_model_set_property;
  oclass->constructed = hyscan_control_model_object_constructed;
  oclass->finalize = hyscan_control_model_object_finalize;

  g_object_class_install_property (oclass, PROP_CONTROL,
    g_param_spec_object ("control", "HyScanControl", "HyScanControl",
                         HYSCAN_TYPE_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanControlModel::sonar
   * @model: указатель на #HyScanControlModel
   * @source: идентификатор источника данных #HyScanSourceType
   *
   * Сигнал оповещает об изменении параметров источника. В детализации сигнала
   * можно указать тип источника данных, для получения сигналов об изменении
   * параметров только указанного источника.
   *
   */
  hyscan_control_model_signals[SIGNAL_SONAR] =
    g_signal_new ("sonar", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE,
                  1, G_TYPE_INT);

  /**
   * HyScanControlModel::sensor
   * @model: указатель на #HyScanControlModel
   * @sensor: имя датчика
   *
   * Сигнал оповещает об изменении параметров датчика. В детализации сигнала
   * можно указать имя датчика, для получения сигналов об изменении параметров
   * только указанного датчика.
   *
   */
  hyscan_control_model_signals[SIGNAL_SENSOR] =
    g_signal_new ("sensor", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  /**
   * HyScanControlModel::actuator
   * @model: указатель на #HyScanControlModel
   * @actuator: имя актуатора
   *
   * Сигнал оповещает об изменении параметров актуатора. В детализации сигнала
   * можно указать имя актуатора, для получения сигналов об изменении параметров
   * только указанного актуатора.
   *
   */
  hyscan_control_model_signals[SIGNAL_ACTUATOR] =
    g_signal_new ("actuator", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  /**
   * HyScanControlModel::before-start
   * @model: указатель на #HyScanControlModel
   *
   * DEPRECATED! Сигнал используется для поддержки legacy-кода и не предназначен для использования в новых разработках.
   *
   * Сигнал отправляется в момент вызова hyscan_sonar_start() модели, для возможности отмены старта.
   * Обработчики сигнала могут вернуть %TRUE, чтобы отменить старт локатора.
   *
   * Для отслеживания изменения статуса работы локатора используется сигнал HyScanSonarState::start-stop.
   *
   * Returns: %TRUE, если необходимо остановить включение локатора.
   */
  hyscan_control_model_signals[SIGNAL_BEFORE_START] =
    g_signal_new ("before-start", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  g_signal_accumulator_true_handled, NULL,
                  hyscan_model_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN,
                  0);

  /**
   * HyScanControlModel::start-sent
   * @model: указатель на #HyScanControlModel
   *
   * Сигнал отправляется в момент успешного вызова hyscan_sonar_start() или hyscan_control_model_start().
   *
   * Последующий сигнал HyScanSonarState::start-stop оповестит о реальном включении локатора.
   */
  hyscan_control_model_signals[SIGNAL_START_SENT] =
    g_signal_new ("start-sent", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * HyScanControlModel::stop-sent
   * @model: указатель на #HyScanControlModel
   *
   * Сигнал отправляется в момент успешного вызова hyscan_sonar_stop().
   *
   * Последующий сигнал HyScanSonarState::start-stop оповестит о реальном выключении локатора.
   */
  hyscan_control_model_signals[SIGNAL_STOP_SENT] =
    g_signal_new ("stop-sent", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  hyscan_control_model_signals[SIGNAL_START_STOP] = g_signal_lookup ("start-stop", HYSCAN_TYPE_SONAR_STATE);
}

static void
hyscan_control_model_init (HyScanControlModel *control_model)
{
  control_model->priv = hyscan_control_model_get_instance_private (control_model);
}

static void
hyscan_control_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (object);
  HyScanControlModelPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      priv->control = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_control_model_object_constructed (GObject *object)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (object);
  HyScanControlModelPrivate *priv = self->priv;

  guint32 n_sources, i;
  const HyScanSourceType *source;
  const gchar *const *sensor;
  const gchar *const *actuator;

  /* Ретрансляция сигналов. */
  g_signal_connect (HYSCAN_SENSOR (priv->control), "sensor-data",
                    G_CALLBACK (hyscan_control_model_sensor_data), self);
  g_signal_connect (HYSCAN_DEVICE (priv->control), "device-state",
                    G_CALLBACK (hyscan_control_model_device_state), self);
  g_signal_connect (HYSCAN_DEVICE (priv->control), "device-log",
                    G_CALLBACK (hyscan_control_model_device_log), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-signal",
                    G_CALLBACK (hyscan_control_model_sonar_signal), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-tvg",
                    G_CALLBACK (hyscan_control_model_sonar_tvg), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-acoustic-data",
                    G_CALLBACK (hyscan_control_model_sonar_acoustic_data), self);

  priv->sync_timeout = HYSCAN_CONTROL_MODEL_TIMEOUT;

  /* Получаем список источников данных. */
  priv->sources = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  source = hyscan_control_sources_list (priv->control, &n_sources);
  if (source != NULL)
    {
      for (i = 0; i < n_sources; ++i)
        g_hash_table_insert (priv->sources, GINT_TO_POINTER (source[i]), g_new0 (HyScanControlModelSource, 1));
    }

  /* Получаем список датчиков. */
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  sensor = hyscan_control_sensors_list (priv->control);
  if (sensor != NULL)
    {
      for (i = 0; sensor[i] != NULL; i++)
        g_hash_table_insert (priv->sensors, g_strdup (sensor[i]), g_new0 (HyScanControlModelSensor, 1));
    }

  /* Получаем список актуаторов. */
  priv->actuators = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  actuator = hyscan_control_actuators_list (priv->control);
  if (actuator != NULL)
    {
      for (i = 0; actuator[i] != NULL; i++)
        g_hash_table_insert (priv->actuators, g_strdup (actuator[i]), g_new0 (HyScanControlModelActuator, 1));
    }

  g_cond_init (&priv->cond);

  /* Поток обмена. */
  g_mutex_init (&priv->lock);
  priv->thread = g_thread_new ("control-model", hyscan_control_model_thread, self);

  /* Тип галса по умолчанию. */
  priv->track_type = HYSCAN_TRACK_SURVEY;
}

static void
hyscan_control_model_object_finalize (GObject *object)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (object);
  HyScanControlModelPrivate *priv = self->priv;

  /* Завершаем поток обработки. */
  g_atomic_int_set (&priv->shutdown, TRUE);
  g_mutex_lock (&priv->lock);
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);
  g_thread_join (priv->thread);

  g_clear_object (&priv->control);
  g_hash_table_destroy (priv->sensors);
  g_hash_table_destroy (priv->sources);
  g_hash_table_destroy (priv->actuators);

  hyscan_control_model_start_free (priv->start);
  hyscan_control_model_start_free (priv->started);
  hyscan_control_model_start_free (priv->started_state);

  hyscan_track_plan_free (priv->plan);
  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->suffix);
  g_free (priv->suffix1);

  G_OBJECT_CLASS (hyscan_control_model_parent_class)->finalize (object);
}

static gboolean
hyscan_control_model_start_stop (HyScanControlModel *self)
{
  HyScanControlModelPrivate *priv = self->priv;

  g_mutex_lock (&priv->lock);
  hyscan_control_model_start_free (priv->started_state);
  priv->started_state = priv->started;
  priv->started = NULL;
  g_mutex_unlock (&priv->lock);

  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_START_STOP], 0);

  return G_SOURCE_REMOVE;
}

/* Обработчик сигнала sensor-data. */
static void
hyscan_control_model_sensor_data (HyScanDevice       *device,
                                  const gchar        *sensor,
                                  gint                source,
                                  gint64              time,
                                  HyScanBuffer       *data,
                                  HyScanControlModel *self)
{
  g_signal_emit_by_name (self, "sensor-data", sensor, source, time, data);
}

/* Обработчик сигнала sonar-signal. */
static void
hyscan_control_model_sonar_signal (HyScanDevice       *device,
                                   gint                source,
                                   guint               channel,
                                   gint64              time,
                                   HyScanBuffer       *image,
                                   HyScanControlModel *self)
{
  g_signal_emit_by_name (self, "sonar-signal", source, channel, time, image);
}

/* Обработчик сигнала sonar-tvg. */
static void
hyscan_control_model_sonar_tvg (HyScanDevice       *device,
                                gint                source,
                                guint               channel,
                                gint64              time,
                                HyScanBuffer       *gains,
                                HyScanControlModel *self)
{
  g_signal_emit_by_name (self, "sonar-tvg", source, channel, time, gains);
}

/* Обработчик сигнала sonar-acoustic-data. */
static void
hyscan_control_model_sonar_acoustic_data (HyScanDevice           *device,
                                          gint                    source,
                                          guint                   channel,
                                          gboolean                noise,
                                          gint64                  time,
                                          HyScanAcousticDataInfo *info,
                                          HyScanBuffer           *data,
                                          HyScanControlModel     *self)
{
  g_signal_emit_by_name (self, "sonar-acoustic-data",
                                source, channel, noise,
                                time, info, data);
}

/* Обработчик сигнала device-state. */
static void
hyscan_control_model_device_state (HyScanDevice       *device,
                                   const gchar        *dev_id,
                                   HyScanControlModel *self)
{
  g_signal_emit_by_name (self, "device-state", dev_id);
}

/* Обработчик сигнала device-log. */
static void
hyscan_control_model_device_log (HyScanDevice       *device,
                                 const gchar        *source,
                                 gint64              time,
                                 gint                level,
                                 const gchar        *message,
                                 HyScanControlModel *self)
{
  g_signal_emit_by_name (self, "device-log", source, time, level, message);
}

static HyScanControlModelStart *
hyscan_control_model_start_copy (const HyScanControlModelStart *start)
{
  HyScanControlModelStart *copy;

  if (start == NULL)
    return NULL;

  copy = g_slice_new (HyScanControlModelStart);
  copy->track_type = start->track_type;
  copy->generate_name = start->generate_name;
  copy->generate_suffix = g_strdup (start->generate_suffix);
  copy->project = g_strdup (start->project);
  copy->track = g_strdup (start->track);
  copy->plan = hyscan_track_plan_copy (start->plan);

  return copy;
}

static void
hyscan_control_model_start_free (HyScanControlModelStart *start)
{
  if (start == NULL)
    return;

  g_free (start->generate_suffix);
  g_free (start->project);
  g_free (start->track);
  hyscan_track_plan_free (start->plan);

  g_slice_free (HyScanControlModelStart, start);
}

/* Генерирует имя галса по шаблону "<следующий номер в БД><суффикс>". */
static gchar *
hyscan_control_model_generate_name (HyScanControlModel *model,
                                    const gchar        *project,
                                    const gchar        *suffix)
{
  HyScanControlModelPrivate *priv = model->priv;
  HyScanDB *db;
  gint track_num;

  gint32 project_id;
  gchar **tracks;
  gchar **strs;

  db = hyscan_control_writer_get_db (priv->control);
  /* Работа без системы хранения. */
  if (db == NULL)
    return NULL;

  /* Ищем в текущем проекте галс с самым большим номером в названии. */
  project_id = hyscan_db_project_open (db, project);
  tracks = project_id > 0 ? hyscan_db_track_list (db, project_id) : NULL;

  track_num = 1;
  for (strs = tracks; strs != NULL && *strs != NULL; strs++)
    {
      gint number;

      number = g_ascii_strtoll (*strs, NULL, 10);
      if (number >= track_num)
        track_num = number + 1;
    }

  hyscan_db_close (db, project_id);
  g_strfreev (tracks);
  g_object_unref (db);

  return g_strdup_printf ("%03d%s", track_num, suffix != NULL ? suffix : "");;
}

/* Функция пробуждает поток с целью синхронизации. */
static gboolean
hyscan_control_model_timeout_sync (gpointer user_data)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (user_data);
  HyScanControlModelPrivate *priv = self->priv;

  /* Будим фоновый поток. */
  g_mutex_lock (&priv->lock);
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  /* Сбрасываем ид таймаут-функции, т.к. она сейчас удалится. */
  priv->timeout_id = 0;

  return G_SOURCE_REMOVE;
}

/* Функция инициирует или откладывает на попозже синхронизацию оборудования. */
static void
hyscan_control_model_init_sync (HyScanControlModel *self)
{
  HyScanControlModelPrivate *priv = self->priv;

  /* Помечаем, что пора обновляться, но пока не будим фоновый поток. */
  g_mutex_lock (&priv->lock);
  priv->sync = TRUE;
  g_mutex_unlock (&priv->lock);

  /* Устанавливаем (или заменяем) таймаут для пробуждения фонового потока. */
  if (priv->timeout_id > 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (priv->sync_timeout, hyscan_control_model_timeout_sync, self);
}

/* Функция должна быть вызывана, когда изменились пар-ры локатора. */
static void
hyscan_control_model_sonar_changed (HyScanControlModel *self,
                                    HyScanSourceType    source)
{
  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_SONAR],
                 g_quark_from_static_string (hyscan_source_get_id_by_type (source)), source);

  hyscan_control_model_init_sync (self);
}

/* Функция должна быть вызывана, когда изменились пар-ры сенсора. */
static void
hyscan_control_model_sensor_changed (HyScanControlModel *self,
                                     const gchar        *name)
{
  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_SENSOR],
                 g_quark_from_string (name), name);

  hyscan_control_model_init_sync (self);
}

/* Функция должна быть вызывана, когда изменились пар-ры актуатора. */
static void
hyscan_control_model_actuator_changed (HyScanControlModel *self,
                                       const gchar        *name)
{
  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_ACTUATOR],
                 g_quark_from_string (name), name);

  hyscan_control_model_init_sync (self);
}


static gboolean
hyscan_control_model_rec_set_internal (HyScanSonar           *sonar,
                                       HyScanSourceType       source,
                                       HyScanControlModelRec *rec)
{
  gboolean status = FALSE;

  switch (rec->mode)
    {
    case HYSCAN_SONAR_RECEIVER_MODE_NONE:
      status = hyscan_sonar_receiver_disable (sonar, source);
      break;

    case HYSCAN_SONAR_RECEIVER_MODE_MANUAL:
      status = hyscan_sonar_receiver_set_time (sonar, source,
                                               rec->receive,
                                               rec->wait);
      break;

    case HYSCAN_SONAR_RECEIVER_MODE_AUTO:
      status = hyscan_sonar_receiver_set_auto (sonar, source);
      break;

    default:
      g_assert_not_reached ();
    }

  if (!status)
    {
      g_warning ("HyScanControlModel: couldn't setup Receiver for %s",
                 hyscan_source_get_name_by_type (source));
    }

  return status;
}

static gboolean
hyscan_control_model_rec_update (HyScanControlModelRec *dest,
                                 HyScanControlModelRec *src)
{
  gboolean changed = FALSE;

  changed = dest->mode != src->mode;
  dest->mode = src->mode;

  if (src->mode == HYSCAN_SONAR_RECEIVER_MODE_MANUAL)
    {
      changed |= dest->receive != src->receive ||
                 dest->wait != src->wait;

      dest->receive = src->receive;
      dest->wait = src->wait;
    }

  return changed;
}

static gboolean
hyscan_control_model_receiver_set (HyScanControlModel    *self,
                                   HyScanSourceType       source,
                                   HyScanControlModelRec *rec)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  gboolean status;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  status = hyscan_control_model_rec_set_internal (HYSCAN_SONAR (priv->control),
                                                  source, rec);
  if (!status)
    return FALSE;

  if (hyscan_control_model_rec_update (&info->rec, rec))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}


static gboolean
hyscan_control_model_gen_set_internal (HyScanSonar           *sonar,
                                       HyScanSourceType       source,
                                       HyScanControlModelGen *gen)
{
  gboolean status;

  if (gen->disabled)
    status = hyscan_sonar_generator_disable (sonar, source);
  else
    status = hyscan_sonar_generator_set_preset (sonar, source, gen->preset);

  if (!status)
    {
      g_warning ("HyScanControlModel: couldn't setup Generator for %s",
                 hyscan_source_get_name_by_type (source));
    }

  return status;
}

static gboolean
hyscan_control_model_gen_update (HyScanControlModelGen *dest,
                                 HyScanControlModelGen *src)
{
  gboolean changed = FALSE;

  if (dest->disabled != src->disabled)
    changed = TRUE;
  if (dest->preset != src->preset)
    changed = TRUE;

  *dest = *src;

  return changed;
}

static gboolean
hyscan_control_model_generator_set (HyScanControlModel    *self,
                                    HyScanSourceType       source,
                                    HyScanControlModelGen *gen)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  gboolean status;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  status = hyscan_control_model_gen_set_internal (HYSCAN_SONAR (priv->control),
                                                  source, gen);

  if (!status)
    return FALSE;

  /* Если параметры отличаются от уже установленных, эмиттируем сигнал . */
  if (hyscan_control_model_gen_update (&info->gen, gen))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_control_model_tvg_set_internal (HyScanSonar           *sonar,
                                       HyScanSourceType       source,
                                       HyScanControlModelTVG *tvg)
{
  gboolean status = FALSE;

  switch (tvg->mode)
    {
    case HYSCAN_SONAR_TVG_MODE_NONE:
      status = hyscan_sonar_tvg_disable (sonar, source);
      break;

    case HYSCAN_SONAR_TVG_MODE_AUTO:
      status = hyscan_sonar_tvg_set_auto (sonar, source,
                                          tvg->atvg.level,
                                          tvg->atvg.sensitivity);
      break;

    case HYSCAN_SONAR_TVG_MODE_CONSTANT:
      status = hyscan_sonar_tvg_set_constant (sonar, source,
                                              tvg->constant.gain);
      break;

    case HYSCAN_SONAR_TVG_MODE_LINEAR_DB:
      status = hyscan_sonar_tvg_set_linear_db (sonar, source,
                                               tvg->linear.gain0,
                                               tvg->linear.gain_step);
      break;

    case HYSCAN_SONAR_TVG_MODE_LOGARITHMIC:
      status = hyscan_sonar_tvg_set_logarithmic (sonar, source,
                                                 tvg->log.gain0,
                                                 tvg->log.beta,
                                                 tvg->log.alpha);
      break;

    default:
      g_assert_not_reached ();
    }

  if (!status)
    {
      g_warning ("HyScanControlModel: couldn't setup TVG for %s",
                 hyscan_source_get_name_by_type (source));
    }

  return status;
}

/* Копирует режим работы вару из src в dst. Возвращает %TRUE, если значения отличались. */
static gboolean
hyscan_control_model_tvg_update (HyScanControlModelTVG *dest,
                                 HyScanControlModelTVG *src)
{
  gboolean changed;

  changed = dest->mode != src->mode;

  switch (src->mode)
    {
    case HYSCAN_SONAR_TVG_MODE_AUTO:
      changed |= dest->atvg.level != src->atvg.level ||
                 dest->atvg.sensitivity != src->atvg.sensitivity;
      break;

    case HYSCAN_SONAR_TVG_MODE_CONSTANT:
      changed |= dest->constant.gain != src->constant.gain;
      break;

    case HYSCAN_SONAR_TVG_MODE_LINEAR_DB:
      changed |= dest->linear.gain0 != src->linear.gain0 ||
                 dest->linear.gain_step != src->linear.gain_step;
      break;

    case HYSCAN_SONAR_TVG_MODE_LOGARITHMIC:
      changed |= dest->log.gain0 != src->log.gain0 ||
                 dest->log.beta != src->log.beta ||
                 dest->log.alpha != src->log.alpha;
      break;

    case HYSCAN_SONAR_TVG_MODE_NONE:
      break;

    default:
      g_assert_not_reached ();
    }

  /* HyScanControlModelTVG это юнион, поэтому я могу позволить себе просто
   * переписать. */
  if (changed)
    *dest = *src;

  return changed;
}

/* Устанавливает режим работы системы ВАРУ. */
static gboolean
hyscan_control_model_tvg_set (HyScanControlModel    *self,
                              HyScanSourceType       source,
                              HyScanControlModelTVG *tvg)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  gboolean status = FALSE;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  /* Настраиваю ВАРУ. */
  status = hyscan_control_model_tvg_set_internal (HYSCAN_SONAR (priv->control),
                                                  source, tvg);

  if (!status)
    return FALSE;

  /* Если параметры отличаются от уже установленных, эмиттируем сигнал. */
  if (hyscan_control_model_tvg_update (&info->tvg, tvg))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_control_model_actuator_set (HyScanControlModel         *self,
                                   const gchar                *name,
                                   HyScanControlModelActuator *actuator)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelActuator *info;
  gboolean status = FALSE;
  gboolean changed;

  info = g_hash_table_lookup (priv->actuators, name);
  if (info == NULL)
    return FALSE;

  if (info->mode == HYSCAN_ACTUATOR_MODE_NONE)
    {
      status = hyscan_actuator_disable (HYSCAN_ACTUATOR (priv->control), name);
    }
  else if (info->mode == HYSCAN_ACTUATOR_MODE_SCAN)
    {
      status = hyscan_actuator_scan (HYSCAN_ACTUATOR (priv->control), name,
                                     info->scan.from,
                                     info->scan.to,
                                     info->scan.speed);
    }
  else if (info->mode == HYSCAN_ACTUATOR_MODE_MANUAL)
    {
      status = hyscan_actuator_manual (HYSCAN_ACTUATOR (priv->control), name,
                                       info->manual.angle);
    }

  if (!status)
    return FALSE;

  changed = info->mode != actuator->mode;
  info->mode = actuator->mode;

  if (actuator->mode == HYSCAN_ACTUATOR_MODE_MANUAL)
    {
      changed |= info->manual.angle != actuator->manual.angle;
      info->manual = actuator->manual;
    }
  if (actuator->mode == HYSCAN_ACTUATOR_MODE_SCAN)
    {
      changed |= info->scan.from != actuator->scan.from ||
                 info->scan.to != actuator->scan.to ||
                 info->scan.speed != actuator->scan.speed;
      info->scan = actuator->scan;
    }

  if (changed)
    hyscan_control_model_actuator_changed (self, name);

  return TRUE;
}


static void
hyscan_control_model_send_start (HyScanControlModel            *self,
                                 const HyScanControlModelStart *start)
{
  HyScanControlModelPrivate *priv = self->priv;

  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_START_SENT], 0);

  /* Устанавливаем параметры для старта ГЛ. */
  g_mutex_lock (&priv->lock);
  hyscan_control_model_start_free (priv->start);
  priv->start = hyscan_control_model_start_copy (start);
  priv->start_stop = SONAR_START;
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);
}

static gboolean
hyscan_control_model_emit_before_start (HyScanControlModel *self)
{
  gboolean cancel = FALSE;

  /* Сигнал "before-start" для подготовки всех систем к началу записи и возможности отменить запись. */
  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_BEFORE_START], 0, &cancel);
  if (cancel)
    {
      g_info ("HyScanControlModel: sonar start cancelled in \"before-start\" signal");
      return FALSE;
    }

  return TRUE;
}

/* Эта простынка отправляет все текущие параметры (окромя офсетов)
 * в HyScanControl и должна вызываться в потоке. */
static gboolean
hyscan_control_model_setup_before_start (HyScanControlModel *self)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControl *backend = priv->control;
  GHashTableIter iter;
  gpointer k, v;
  gboolean status;

  /* Настраиваю локаторы. */
  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      HyScanSourceType source = GPOINTER_TO_INT (k);
      HyScanControlModelSource *info = v;

      if (!hyscan_control_model_tvg_set_internal (HYSCAN_SONAR (backend), source, &info->tvg))
        return FALSE;

      if (!hyscan_control_model_rec_set_internal (HYSCAN_SONAR (backend), source, &info->rec))
        return FALSE;

      if (!hyscan_control_model_gen_set_internal (HYSCAN_SONAR (backend), source, &info->gen))
        return FALSE;
    }

  /* Настраиваю датчики. */
  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      const gchar *name = k;
      HyScanControlModelSensor *info = v;

      if (hyscan_sensor_set_enable (HYSCAN_SENSOR (backend), name, info->enable))
        continue;

      g_warning ("HyScanControlModel: couldn't enable sensor %s", name);
      return FALSE;
    }

  /* Настраиваю актуаторы. */
  g_hash_table_iter_init (&iter, priv->actuators);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      const gchar *name = k;
      HyScanControlModelActuator *info = v;

      switch (info->mode)
        {
        case HYSCAN_ACTUATOR_MODE_NONE:
          status = hyscan_actuator_disable (HYSCAN_ACTUATOR (backend), name);
          break;

        case HYSCAN_ACTUATOR_MODE_SCAN:
          status = hyscan_actuator_scan (HYSCAN_ACTUATOR (backend), name,
                                         info->scan.from,
                                         info->scan.to,
                                         info->scan.speed);
          break;

        case HYSCAN_ACTUATOR_MODE_MANUAL:
          status = hyscan_actuator_manual (HYSCAN_ACTUATOR (backend), name,
                                           info->manual.angle);
          break;

        default:
          g_assert_not_reached ();
        }

      if (status)
        continue;

      g_warning ("HyScanControlModel: couldn't setup actuator %s", name);
      return FALSE;
    }

  return TRUE;
}

static gpointer
hyscan_control_model_thread (gpointer data)
{
  HyScanControlModel *self = data;
  HyScanControlModelPrivate *priv = self->priv;
  HyScanSonar *sonar = HYSCAN_SONAR (priv->control);

  while (!g_atomic_int_get (&priv->shutdown))
    {
      HyScanControlModelStart *start;
      gint start_stop;
      gboolean sync;

      /* Если ничего не поменялось, жду. */
      {
        g_mutex_lock (&priv->lock);

        while (!priv->wakeup)
          g_cond_wait (&priv->cond, &priv->lock);

        /* Переписываем все изменения. */
        sync = priv->sync;
        priv->sync = FALSE;

        start_stop = priv->start_stop;
        priv->start_stop = SONAR_NO_CHANGE;

        start = priv->start;
        priv->start = NULL;

        priv->wakeup = FALSE;

        g_mutex_unlock (&priv->lock);
      }

      /* Синхронизирую локатор. */
      if (sync)
        hyscan_device_sync (HYSCAN_DEVICE (sonar));

      /* Включаем локатор. */
      if (start_stop == SONAR_START)
        {
          gboolean status;

          if (start->generate_name)
            {
              g_free (start->track);
              start->track = hyscan_control_model_generate_name (self, start->project, start->generate_suffix);
            }

          status = hyscan_control_model_setup_before_start (self);

          status &= hyscan_sonar_start (sonar,
                                        start->project, start->track,
                                        start->track_type, start->plan);

          if (!status)
            {
              g_warning ("HyScanControlModel: internal sonar failed to start");
              g_clear_pointer (&start, &hyscan_control_model_start_free);
            }
        }

      /* Выключаем локатор. */
      else if (start_stop == SONAR_STOP)
        {
          g_clear_pointer (&start, &hyscan_control_model_start_free);
          hyscan_sonar_stop (sonar);
        }

      /* Отправляем сигнал об изменении статуса работы локатора. */
      if (start_stop != SONAR_NO_CHANGE)
        {
          g_mutex_lock (&priv->lock);
          hyscan_control_model_start_free (priv->started);
          priv->started = start;
          g_mutex_unlock (&priv->lock);

          g_idle_add ((GSourceFunc) hyscan_control_model_start_stop, self);
        }
    }

  return NULL;
}

/*
 *
 * Имплементации интерфейсов.
 *
 */
static HyScanSonarReceiverModeType
hyscan_control_model_receiver_get_mode (HyScanSonarState *sonar,
                                        HyScanSourceType  source)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return HYSCAN_SONAR_RECEIVER_MODE_NONE;

  return info->rec.mode;
}

static gboolean
hyscan_control_model_receiver_get_time (HyScanSonarState *sonar,
                                        HyScanSourceType  source,
                                        gdouble          *receive_time,
                                        gdouble          *wait_time)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  if (info->rec.mode != HYSCAN_SONAR_RECEIVER_MODE_MANUAL)
    return FALSE;

  receive_time != NULL ? *receive_time = info->rec.receive : 0;
  wait_time != NULL ? *wait_time = info->rec.wait : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_receiver_get_disabled (HyScanSonarState *sonar,
                                            HyScanSourceType  source)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->rec.mode == HYSCAN_SONAR_RECEIVER_MODE_NONE;
}

static gboolean
hyscan_control_model_generator_get_preset (HyScanSonarState *sonar,
                                           HyScanSourceType  source,
                                           gint64           *preset)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  if (info->gen.disabled)
    return FALSE;

  preset != NULL ? *preset = info->gen.preset : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_generator_get_disabled (HyScanSonarState *sonar,
                                             HyScanSourceType  source)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->gen.disabled;
}

static HyScanSonarTVGModeType
hyscan_control_model_tvg_get_mode (HyScanSonarState *sonar,
                                   HyScanSourceType  source)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return HYSCAN_SONAR_TVG_MODE_NONE;

  return info->tvg.mode;
}

static gboolean
hyscan_control_model_tvg_get_auto (HyScanSonarState *sonar,
                                   HyScanSourceType  source,
                                   gdouble          *level,
                                   gdouble          *sensitivity)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  if (info->tvg.mode != HYSCAN_SONAR_TVG_MODE_AUTO)
    return FALSE;

  level != NULL ? *level = info->tvg.atvg.level : 0;
  sensitivity != NULL ? *sensitivity = info->tvg.atvg.sensitivity : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_tvg_get_constant (HyScanSonarState *sonar,
                                       HyScanSourceType  source,
                                       gdouble          *gain)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  if (info->tvg.mode != HYSCAN_SONAR_TVG_MODE_CONSTANT)
    return FALSE;

  gain != NULL ? *gain = info->tvg.constant.gain : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_tvg_get_linear_db (HyScanSonarState *sonar,
                                        HyScanSourceType  source,
                                        gdouble          *gain0,
                                        gdouble          *gain_step)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  if (info->tvg.mode != HYSCAN_SONAR_TVG_MODE_LINEAR_DB)
    return FALSE;

  gain0 != NULL ? *gain0 = info->tvg.linear.gain0 : 0;
  gain_step != NULL ? *gain_step = info->tvg.linear.gain_step : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_tvg_get_logarithmic (HyScanSonarState *sonar,
                                          HyScanSourceType  source,
                                          gdouble          *gain0,
                                          gdouble          *beta,
                                          gdouble          *alpha)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  if (info->tvg.mode != HYSCAN_SONAR_TVG_MODE_LOGARITHMIC)
    return FALSE;

  gain0 != NULL ? *gain0 = info->tvg.log.gain0 : 0;
  beta != NULL ? *beta = info->tvg.log.beta : 0;
  alpha != NULL ? *alpha = info->tvg.log.alpha : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_tvg_get_disabled (HyScanSonarState *sonar,
                                       HyScanSourceType  source)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->tvg.mode == HYSCAN_SONAR_TVG_MODE_NONE;
}

static gboolean
hyscan_control_model_get_start (HyScanSonarState  *sonar,
                                gchar            **project_name,
                                gchar            **track_name,
                                HyScanTrackType   *track_type,
                                HyScanTrackPlan  **plan)
{
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;
  HyScanControlModelStart *state = priv->started_state;

  if (state == NULL)
    return FALSE;

  project_name != NULL ? *project_name = g_strdup (state->project) : 0;
  track_name != NULL ? *track_name = g_strdup (state->track) : 0;
  track_type != NULL ? *track_type = state->track_type : 0;
  plan != NULL ? *plan = hyscan_track_plan_copy (state->plan) : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_sonar_antenna_set_offset (HyScanSonar               *sonar,
                                               HyScanSourceType           source,
                                               const HyScanAntennaOffset *offset)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sonar);
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  gboolean status;
  gboolean changed;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  status = hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (priv->control), source, offset);
  if (!status)
    return FALSE;

  changed = info->offset.forward != offset->forward ||
            info->offset.starboard != offset->starboard ||
            info->offset.vertical != offset->vertical ||
            info->offset.yaw != offset->yaw ||
            info->offset.pitch != offset->pitch ||
            info->offset.roll != offset->roll;

  info->offset = *offset;

  if (changed)
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_control_model_receiver_set_time (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           receive,
                                        gdouble           wait)
{
  HyScanControlModelRec rec = { .mode = HYSCAN_SONAR_RECEIVER_MODE_MANUAL };

  rec.receive = receive;
  rec.wait = wait;

  return hyscan_control_model_receiver_set (HYSCAN_CONTROL_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_control_model_receiver_set_auto (HyScanSonar      *sonar,
                                        HyScanSourceType  source)
{
  HyScanControlModelRec rec = { .mode = HYSCAN_SONAR_RECEIVER_MODE_AUTO };

  return hyscan_control_model_receiver_set (HYSCAN_CONTROL_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_control_model_receiver_disable (HyScanSonar      *sonar,
                                       HyScanSourceType  source)
{
  HyScanControlModelRec rec = { .mode = HYSCAN_SONAR_RECEIVER_MODE_NONE };

  return hyscan_control_model_receiver_set (HYSCAN_CONTROL_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_control_model_generator_set_preset (HyScanSonar      *sonar,
                                           HyScanSourceType  source,
                                           gint64            preset)
{
  HyScanControlModelGen gen = {0};

  gen.disabled = FALSE;
  gen.preset = preset;

  return hyscan_control_model_generator_set (HYSCAN_CONTROL_MODEL (sonar), source, &gen);
}

static gboolean
hyscan_control_model_generator_disable (HyScanSonar      *sonar,
                                        HyScanSourceType  source)
{
  HyScanControlModelGen gen = {0};

  gen.disabled = TRUE;

  return hyscan_control_model_generator_set (HYSCAN_CONTROL_MODEL (sonar), source, &gen);
}

static gboolean
hyscan_control_model_tvg_set_auto (HyScanSonar      *sonar,
                                   HyScanSourceType  source,
                                   gdouble           level,
                                   gdouble           sensitivity)
{
  HyScanControlModelTVG tvg = { .mode = HYSCAN_SONAR_TVG_MODE_AUTO };

  tvg.atvg.level = level;
  tvg.atvg.sensitivity = sensitivity;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_set_constant (HyScanSonar      *sonar,
                                       HyScanSourceType  source,
                                       gdouble           gain)
{
  HyScanControlModelTVG tvg = { .mode = HYSCAN_SONAR_TVG_MODE_CONSTANT };

  tvg.constant.gain = gain;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_set_linear_db (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           gain0,
                                        gdouble           gain_step)
{
  HyScanControlModelTVG tvg = { .mode = HYSCAN_SONAR_TVG_MODE_LINEAR_DB };

  tvg.linear.gain0 = gain0;
  tvg.linear.gain_step = gain_step;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_set_logarithmic (HyScanSonar      *sonar,
                                          HyScanSourceType  source,
                                          gdouble           gain0,
                                          gdouble           beta,
                                          gdouble           alpha)
{
  HyScanControlModelTVG tvg = { .mode = HYSCAN_SONAR_TVG_MODE_LOGARITHMIC };

  tvg.log.gain0 = gain0;
  tvg.log.beta = beta;
  tvg.log.alpha = alpha;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_disable (HyScanSonar      *sonar,
                                  HyScanSourceType  source)
{
  HyScanControlModelTVG tvg = { .mode = HYSCAN_SONAR_TVG_MODE_NONE };

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_sonar_start (HyScanSonar           *sonar,
                                  const gchar           *project_name,
                                  const gchar           *track_name,
                                  HyScanTrackType        track_type,
                                  const HyScanTrackPlan *track_plan)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sonar);
  HyScanControlModelStart start = {0};

  /* Сигнал "before-start" для подготовки всех систем к началу записи и возможности отменить запись. */
  if (!hyscan_control_model_emit_before_start (self))
    return FALSE;

  start.track_type = track_type;
  start.project = (gchar *) project_name;
  start.track = (gchar *) track_name;
  start.plan = (HyScanTrackPlan *) track_plan;
  hyscan_control_model_send_start (self, &start);

  return TRUE;
}

static gboolean
hyscan_control_model_sonar_stop (HyScanSonar *sonar)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sonar);
  HyScanControlModelPrivate *priv = self->priv;

  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_STOP_SENT], 0);

  /* Устанавливаем параметры для остановки ГЛ. */
  g_mutex_lock (&priv->lock);
  priv->start_stop = SONAR_STOP;
  g_clear_pointer (&priv->start, hyscan_control_model_start_free);
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return TRUE;
}


static gboolean
hyscan_control_model_sensor_get_enabled (HyScanSensorState *sensor,
                                       const gchar       *name)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sensor);
  HyScanControlModelSensor *info = g_hash_table_lookup (self->priv->sensors, name);

  if (info == NULL)
    return FALSE;

  return info->enable;
}

static gboolean
hyscan_control_model_sensor_antenna_set_offset (HyScanSensor              *sensor,
                                                const gchar               *name,
                                                const HyScanAntennaOffset *offset)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sensor);
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSensor *info = g_hash_table_lookup (priv->sensors, name);
  gboolean changed;

  if (info == NULL)
    return FALSE;

  if (!hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (priv->control), name, offset))
    return FALSE;

  /* Такая же логика, как у локаторов (ТВГ, генератор, приемник). */
  changed = info->offset.forward != offset->forward ||
            info->offset.starboard != offset->starboard ||
            info->offset.vertical != offset->vertical ||
            info->offset.yaw != offset->yaw ||
            info->offset.pitch != offset->pitch ||
            info->offset.roll != offset->roll;

  info->offset = *offset;
  if (changed)
    hyscan_control_model_sensor_changed (self, name);

  return TRUE;
}

static gboolean
hyscan_control_model_sensor_set_enable (HyScanSensor *sensor,
                                        const gchar  *name,
                                        gboolean      enable)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sensor);
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSensor *info = g_hash_table_lookup (priv->sensors, name);

  if (info == NULL)
    return FALSE;

  if (!hyscan_sensor_set_enable (HYSCAN_SENSOR (priv->control), name, enable))
    return FALSE;

  /* Такая же логика, как у локаторов (ТВГ, генератор, приемник). */
  if (enable != info->enable)
    {
      info->enable = enable;
      hyscan_control_model_sensor_changed (self, name);
    }

  return TRUE;
}

static gboolean
hyscan_control_model_device_state_get_disconnected (HyScanDeviceState *device_state)
{
  return HYSCAN_CONTROL_MODEL (device_state)->priv->disconnected;
}

static GList *
hyscan_control_model_device_state_get_sound_velocity (HyScanDeviceState *device_state)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (device_state);
  HyScanControlModelPrivate *priv = self->priv;

  return g_list_copy_deep (priv->sound_velocity, (GCopyFunc) hyscan_sound_velocity_copy, NULL);
}


static gboolean
hyscan_control_model_device_disconnect (HyScanDevice *device)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (device);
  HyScanControlModelPrivate *priv = self->priv;

  if (!hyscan_device_disconnect (HYSCAN_DEVICE (priv->control)))
    return FALSE;

  priv->disconnected = TRUE;

  return TRUE;
}

static gboolean
hyscan_control_model_device_set_sound_velocity (HyScanDevice *device,
                                                GList        *svp)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (device);
  HyScanControlModelPrivate *priv = self->priv;

  if (!hyscan_device_set_sound_velocity (HYSCAN_DEVICE (priv->control), svp))
    return FALSE;

  g_list_free_full (priv->sound_velocity, (GDestroyNotify) hyscan_sound_velocity_free);
  priv->sound_velocity = g_list_copy_deep (svp, (GCopyFunc) hyscan_sound_velocity_copy, NULL);

  return TRUE;
}

static gboolean
hyscan_control_model_sync (HyScanDevice *device)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (device);
  return hyscan_device_sync (HYSCAN_DEVICE (self->priv->control));
}

static HyScanActuatorModeType
hyscan_control_model_actuator_state_get_mode (HyScanActuatorState *actuator,
                                              const gchar         *name)
{
  HyScanControlModelActuator *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (actuator)->priv;

  info = g_hash_table_lookup (priv->actuators, name);
  if (info == NULL)
    return HYSCAN_ACTUATOR_MODE_NONE;

  return info->mode;
}

static gboolean
hyscan_control_model_actuator_state_get_disable (HyScanActuatorState *actuator,
                                                 const gchar         *name)
{
  HyScanControlModelActuator *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (actuator)->priv;

  info = g_hash_table_lookup (priv->actuators, name);
  if (info == NULL)
    return FALSE;

  return info->mode == HYSCAN_ACTUATOR_MODE_NONE;
}

static gboolean
hyscan_control_model_actuator_state_get_scan (HyScanActuatorState *actuator,
                                              const gchar         *name,
                                              gdouble             *from,
                                              gdouble             *to,
                                              gdouble             *speed)
{
  HyScanControlModelActuator *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (actuator)->priv;

  info = g_hash_table_lookup (priv->actuators, name);
  if (info == NULL)
    return FALSE;

  if (info->mode != HYSCAN_ACTUATOR_MODE_SCAN)
    return FALSE;

  from != NULL ? *from = info->scan.from : 0;
  to != NULL ? *to = info->scan.to : 0;
  speed != NULL ? *speed = info->scan.speed : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_actuator_state_get_manual (HyScanActuatorState *actuator,
                                                const gchar         *name,
                                                gdouble             *angle)
{
  HyScanControlModelActuator *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (actuator)->priv;

  info = g_hash_table_lookup (priv->actuators, name);
  if (info == NULL)
    return FALSE;

  if (info->mode != HYSCAN_ACTUATOR_MODE_MANUAL)
    return FALSE;

  angle != NULL ? *angle = info->manual.angle : 0;

  return TRUE;
}

static gboolean
hyscan_control_model_actuator_disable (HyScanActuator *actuator,
                                       const gchar    *name)
{
  HyScanControlModelActuator info = { .mode = HYSCAN_ACTUATOR_MODE_NONE};

  return hyscan_control_model_actuator_set (HYSCAN_CONTROL_MODEL (actuator), name, &info);
}

static gboolean
hyscan_control_model_actuator_scan (HyScanActuator *actuator,
                                    const gchar    *name,
                                    gdouble         from,
                                    gdouble         to,
                                    gdouble         speed)
{
  HyScanControlModelActuator info = { .mode = HYSCAN_ACTUATOR_MODE_SCAN};

  info.scan.from = from;
  info.scan.to = to;
  info.scan.speed = speed;

  return hyscan_control_model_actuator_set (HYSCAN_CONTROL_MODEL (actuator), name, &info);
}

static gboolean
hyscan_control_model_actuator_manual (HyScanActuator *actuator,
                                      const gchar    *name,
                                      gdouble         angle)
{
  HyScanControlModelActuator info = { .mode = HYSCAN_ACTUATOR_MODE_MANUAL};

  info.manual.angle = angle;

  return hyscan_control_model_actuator_set (HYSCAN_CONTROL_MODEL (actuator), name, &info);
}

static HyScanDataSchema *
hyscan_control_model_param_schema (HyScanParam *param)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (param);
  return hyscan_param_schema (HYSCAN_PARAM (self->priv->control));
}

static gboolean
hyscan_control_model_param_set (HyScanParam     *param,
                                HyScanParamList *list)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (param);
  HyScanControlModelPrivate *priv = self->priv;

  return hyscan_param_set (HYSCAN_PARAM (priv->control), list);
}

static gboolean
hyscan_control_model_param_get (HyScanParam     *param,
                                HyScanParamList *list)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (param);
  HyScanControlModelPrivate *priv = self->priv;

  return hyscan_param_get (HYSCAN_PARAM (priv->control), list);
}

/*
 *
 * Публичные методы.
 *
 */

/**
 * hyscan_control_model_new:
 * @control: указатель на #HyScanControl
 *
 * Функция создаёт новый объект #HyScanControlModel.
 *
 * Returns: (transfer full): указатель на новый объект #HyScanControlModel, для удаления g_object_unref().
 */
HyScanControlModel *
hyscan_control_model_new (HyScanControl *control)
{
  return g_object_new (HYSCAN_TYPE_CONTROL_MODEL,
                       "control", control,
                       NULL);
}

/**
 * hyscan_control_model_get_control:
 * @model: указатель на #HyScanControlModel
 *
 * Функция возвращает используемый HyScanControl.
 *
 * Returns: (transfer full): указатель на используемый #HyScanControl, для удаления g_object_unref().
 */
HyScanControl *
hyscan_control_model_get_control (HyScanControlModel *model)
{
  HyScanControlModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_CONTROL_MODEL (model), NULL);
  priv = model->priv;

  return g_object_ref (priv->control);
}

/**
 * hyscan_control_model_set_sync_timeout:
 * @model: указатель на #HyScanControlModel
 * @msec: время ожидания до синхронизации параметров, милисекунды
 *
 * Функция устанавливает время ожидания новых значений параметров локатора. Если в течение указанного
 * времени новые параметры не поступили, то на реальном локаторе будет вызван hyscan_sonar_sync().
 */
void
hyscan_control_model_set_sync_timeout (HyScanControlModel *model,
                                       guint               msec)
{
  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  model->priv->sync_timeout = msec;
}

/**
 * hyscan_control_model_get_sync_timeout:
 * @model: указатель на #HyScanControlModel
 *
 * Функция возвращает время ожидания новых значений параметров локатора.
 *
 * Returns: время ожидания новых значений параметров локатора, милисекунды.
 */
guint
hyscan_control_model_get_sync_timeout (HyScanControlModel *model)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL_MODEL (model), 0);

  return model->priv->sync_timeout;
}

/**
 * hyscan_control_model_set_track_sfx:
 * @model: указатель на #HyScanSonarRecorder
 * @suffix: (optional): суффикс имени галса
 * @one_time: %TRUE, суффикс применяется только к следующему старту; %FALSE, если ко всем последующим
 *
 * Функция устанавливает суффикс, который будет добавлен в конце автоматически сгенерированных имён галсов.
 * Суффикс должен быть валидным именем галса.
 */
void
hyscan_control_model_set_track_sfx (HyScanControlModel *model,
                                    const gchar        *suffix,
                                    gboolean            one_time)
{
  HyScanControlModelPrivate *priv;
  gchar **suffix_field;

  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  priv = model->priv;

  suffix_field = one_time ? &priv->suffix1 : &priv->suffix;
  g_free (*suffix_field);
  *suffix_field = g_strdup (suffix);
}

/**
 * hyscan_control_model_set_plan:
 * @model: указатель на #HyScanControlModel
 * @plan: (nullable): план галса
 *
 * Функция устанавливает план галса, который будет использован в
 * hyscan_control_model_start().
 */
void
hyscan_control_model_set_plan (HyScanControlModel *model,
                               HyScanTrackPlan    *plan)
{
  HyScanControlModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  priv = model->priv;

  hyscan_track_plan_free (priv->plan);
  priv->plan = hyscan_track_plan_copy (plan);
}

/**
 * hyscan_control_model_set_track_name:
 * @model: указатель на #HyScanControlModel
 * @project_name: название проекта
 *
 * Функция устанавливает название проекта, которое будет использовано в
 * hyscan_control_model_start().
 */
void
hyscan_control_model_set_project (HyScanControlModel *model,
                                  const gchar        *project_name)
{
  HyScanControlModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  priv = model->priv;

  g_free (priv->project_name);
  priv->project_name = g_strdup (project_name);
}

/**
 * hyscan_control_model_set_track_name:
 * @model: указатель на #HyScanControlModel
 * @track_type: тип галса
 *
 * Функция устанавливает тип галса, который будет использован в
 * hyscan_control_model_start().
 */
void
hyscan_control_model_set_track_type (HyScanControlModel *model,
                                     HyScanTrackType     track_type)
{
  HyScanControlModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  priv = model->priv;

  priv->track_type = track_type;
}

/**
 * hyscan_control_model_set_track_name:
 * @model: указатель на #HyScanControlModel
 * @track_name: (nullable): название галса
 *
 * Функция устанавливает название галса, которое будет использовано в
 * hyscan_control_model_start().
 *
 * Если @track_name = %NULL, то имя галса будет сгенерировано автоматически.
 */
void
hyscan_control_model_set_track_name (HyScanControlModel *model,
                                     const gchar        *track_name)
{
  HyScanControlModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  priv = model->priv;

  g_free (priv->track_name);
  priv->track_name = g_strdup (track_name);
}

/**
 * hyscan_control_model_start:
 * @model: указатель на #HyScanControlModel
 *
 * Функция переводит гидролокатор в рабочий режим с параметрами,
 * указанными при помощи функций предварительной настройки старта:
 *
 * - hyscan_control_model_set_track_sfx(),
 * - hyscan_control_model_set_plan(),
 * - hyscan_control_model_set_project(),
 * - hyscan_control_model_set_track_name(),
 * - hyscan_control_model_set_track_type(),
 *
 * Returns: %TRUE, если команда выполнена успешно; иначе %FALSE.
 */
gboolean
hyscan_control_model_start (HyScanControlModel *model)
{
  HyScanControlModelPrivate *priv;
  HyScanControlModelStart start;
  gchar *generate_suffix = NULL;
  gboolean success;

  g_return_val_if_fail (HYSCAN_IS_CONTROL_MODEL (model), FALSE);
  priv = model->priv;

  if (!hyscan_control_model_emit_before_start (model))
    {
      success = FALSE;
      goto exit;
    }

  generate_suffix = g_strdup_printf ("%s%s",
                                     priv->suffix != NULL ? priv->suffix : "",
                                     priv->suffix1 != NULL ? priv->suffix1 : "");

  start.track_type = priv->track_type;
  start.project = (gchar *) priv->project_name;
  start.track = (gchar *) priv->track_name;
  start.plan = priv->plan;
  start.generate_name = (start.track == NULL);
  start.generate_suffix = generate_suffix;
  hyscan_control_model_send_start (model, &start);
  success = TRUE;

  g_free (generate_suffix);

exit:
  g_clear_pointer (&priv->suffix1, g_free);

  return success;
}

static void
hyscan_control_model_sonar_state_iface_init (HyScanSonarStateInterface *iface)
{
  iface->receiver_get_mode = hyscan_control_model_receiver_get_mode;
  iface->receiver_get_time = hyscan_control_model_receiver_get_time;
  iface->receiver_get_disabled = hyscan_control_model_receiver_get_disabled;
  iface->generator_get_preset = hyscan_control_model_generator_get_preset;
  iface->generator_get_disabled = hyscan_control_model_generator_get_disabled;
  iface->tvg_get_mode = hyscan_control_model_tvg_get_mode;
  iface->tvg_get_auto = hyscan_control_model_tvg_get_auto;
  iface->tvg_get_constant = hyscan_control_model_tvg_get_constant;
  iface->tvg_get_linear_db = hyscan_control_model_tvg_get_linear_db;
  iface->tvg_get_logarithmic = hyscan_control_model_tvg_get_logarithmic;
  iface->tvg_get_disabled = hyscan_control_model_tvg_get_disabled;
  iface->get_start = hyscan_control_model_get_start;
}

static void
hyscan_control_model_sonar_iface_init (HyScanSonarInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_model_sonar_antenna_set_offset;
  iface->receiver_set_time = hyscan_control_model_receiver_set_time;
  iface->receiver_set_auto = hyscan_control_model_receiver_set_auto;
  iface->receiver_disable = hyscan_control_model_receiver_disable;
  iface->generator_set_preset = hyscan_control_model_generator_set_preset;
  iface->generator_disable = hyscan_control_model_generator_disable;
  iface->tvg_set_auto = hyscan_control_model_tvg_set_auto;
  iface->tvg_set_constant = hyscan_control_model_tvg_set_constant;
  iface->tvg_set_linear_db = hyscan_control_model_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_control_model_tvg_set_logarithmic;
  iface->tvg_disable = hyscan_control_model_tvg_disable;
  iface->start = hyscan_control_model_sonar_start;
  iface->stop = hyscan_control_model_sonar_stop;
}

static void
hyscan_control_model_sensor_state_iface_init (HyScanSensorStateInterface *iface)
{
  iface->get_enabled = hyscan_control_model_sensor_get_enabled;
  iface->antenna_get_offset = NULL;
}

static void
hyscan_control_model_sensor_iface_init (HyScanSensorInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_model_sensor_antenna_set_offset;
  iface->set_enable = hyscan_control_model_sensor_set_enable;
}

static void
hyscan_control_model_device_state_iface_init (HyScanDeviceStateInterface *iface)
{
  iface->get_disconnected = hyscan_control_model_device_state_get_disconnected;
  iface->get_sound_velocity = hyscan_control_model_device_state_get_sound_velocity;
}

static void
hyscan_control_model_device_iface_init (HyScanDeviceInterface *iface)
{
  iface->disconnect = hyscan_control_model_device_disconnect;
  iface->set_sound_velocity = hyscan_control_model_device_set_sound_velocity;
  iface->sync = hyscan_control_model_sync;
}

static void
hyscan_control_model_actuator_state_iface_init (HyScanActuatorStateInterface *iface)
{
  iface->get_mode = hyscan_control_model_actuator_state_get_mode;
  iface->get_disable = hyscan_control_model_actuator_state_get_disable;
  iface->get_scan = hyscan_control_model_actuator_state_get_scan;
  iface->get_manual = hyscan_control_model_actuator_state_get_manual;
}

static void
hyscan_control_model_actuator_iface_init (HyScanActuatorInterface *iface)
{
  iface->disable = hyscan_control_model_actuator_disable;
  iface->scan = hyscan_control_model_actuator_scan;
  iface->manual = hyscan_control_model_actuator_manual;
}

static void
hyscan_control_model_param_iface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_control_model_param_schema;
  iface->set = hyscan_control_model_param_set;
  iface->get = hyscan_control_model_param_get;
}
