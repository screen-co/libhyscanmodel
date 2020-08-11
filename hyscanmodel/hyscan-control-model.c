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
 */

#include "hyscan-control-model.h"
#include <hyscan-data-box.h>
#include <string.h>
#include <hyscan-model-marshallers.h>

#define HYSCAN_CONTROL_MODEL_TIMEOUT (1000 /* Одна секунда*/)

typedef struct _HyScanControlModelStart HyScanControlModelStart;
typedef struct _HyScanControlModelTVG HyScanControlModelTVG;
typedef struct _HyScanControlModelRec HyScanControlModelRec;
typedef struct _HyScanControlModelGen HyScanControlModelGen;
typedef struct _HyScanControlModelSource HyScanControlModelSource;
typedef struct _HyScanControlModelSensor HyScanControlModelSensor;

enum
{
  PROP_0,
  PROP_CONTROL,
};

enum
{
  SIGNAL_SONAR,
  SIGNAL_SENSOR,
  SIGNAL_PARAM,
  SIGNAL_BEFORE_START,
  SIGNAL_START_STOP,
  SIGNAL_LAST
};

enum
{
  SONAR_NO_CHANGE,
  SONAR_START,
  SONAR_STOP,
};

struct _HyScanControlModelStart
{
  HyScanTrackType             track_type;
  gchar                      *project;
  gchar                      *track;
  HyScanTrackPlan            *plan;
};

struct _HyScanControlModelTVG
{
  gboolean                    disabled;
  HyScanSonarTVGModeType      mode;

  struct
  {
    gdouble                   level;
    gdouble                   sensitivity;
  } atvg;
  struct
  {
    gdouble                   gain;
  } constant;
  struct
  {
    gdouble                   gain0;
    gdouble                   gain_step;
  } linear;
  struct
  {
    gdouble                   gain0;
    gdouble                   beta;
    gdouble                   alpha;
  } log;
};

struct _HyScanControlModelRec
{
  gboolean                    disabled;
  HyScanSonarReceiverModeType mode;
  gdouble                     receive;
  gdouble                     wait;
};

struct _HyScanControlModelGen
{
  gboolean                    disabled;
  gint64                      preset;
};

struct _HyScanControlModelSource
{
  HyScanControlModelTVG       tvg;
  HyScanControlModelRec       rec;
  HyScanControlModelGen       gen;
  HyScanAntennaOffset         offset;
};

struct _HyScanControlModelSensor
{
  gboolean                    enable;
  HyScanAntennaOffset         offset;
};

struct _HyScanControlModelPrivate
{
  HyScanControl              *control;         /* Бэкэнд. */

  GHashTable                 *sources;         /* Параметры датчиков. {HyScanSourceType : HyScanControlModelSource* } */
  GHashTable                 *sensors;         /* Параметры датчиков. {gchar* : HyScanControlModelSensor* } */

  GThread                    *thread;          /* Поток. */
  gint                        stop;            /* Остановка потока. */
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
  gchar                      *generated_name;  /* Сгенерированное имя галса. */
  gchar                      *suffix1;         /* Суффикс названия галса (одноразовый). */
  gchar                      *suffix;          /* Суффикс названия галса (постоянный). */
};

static void     hyscan_control_model_sonar_state_iface_init  (HyScanSonarStateInterface      *iface);
static void     hyscan_control_model_sonar_iface_init        (HyScanSonarInterface           *iface);
static void     hyscan_control_model_sensor_state_iface_init (HyScanSensorStateInterface     *iface);
static void     hyscan_control_model_sensor_iface_init       (HyScanSensorInterface          *iface);
static void     hyscan_control_model_param_iface_init        (HyScanParamInterface           *iface);
static void     hyscan_control_model_set_property            (GObject                        *object,
                                                              guint                           prop_id,
                                                              const GValue                   *value,
                                                              GParamSpec                     *pspec);
static void     hyscan_control_model_object_constructed      (GObject                        *object);
static void     hyscan_control_model_object_finalize         (GObject                        *object);

static gboolean hyscan_control_model_rec_update              (HyScanControlModelRec          *dest,
                                                              HyScanControlModelRec          *src);
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
static gpointer hyscan_control_model_thread            (gpointer                        data);
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

static guint hyscan_control_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanControlModel, hyscan_control_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanControlModel)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR_STATE, hyscan_control_model_sonar_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_control_model_sonar_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR_STATE, hyscan_control_model_sensor_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_control_model_sensor_iface_init)
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
   * Сигнал оповещает об измении параметров локатора. В детализации сигнала можно указать тип источника даннных,
   * для получения сигналов об изменении параметров только указанного источника.
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
   * HyScanControlModel::sonar
   * @model: указатель на #HyScanControlModel
   * @sensor: имя датчика
   *
   * Сигнал оповещает об измении параметров датчика. В детализации сигнала можно указать имя датчика,
   * для получения сигналов об изменении параметров только указанного датчика.
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
   * HyScanControlModel::before-start
   * @model: указатель на #HyScanControlModel
   *
   * Сигнал отправляется в момент вызова hyscan_sonar_start() модели, для возможности отмены старта.
   * Обработчики сигнала могут вернуть %TRUE, чтобы отменить старт локатора.
   *
   * Для остлеживания изменения статуса работы локатора используйтся сигнал HyScanSonarState::start-stop.
   *
   * Returns: %TRUE, если необходимо остановить включение локатора.
   */
  hyscan_control_model_signals[SIGNAL_PARAM] =
    g_signal_new ("param", HYSCAN_TYPE_CONTROL_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * HyScanControlModel::before-start
   * @model: указатель на #HyScanControlModel
   *
   * DEPRECATED! Сигнал используется для поддержки legacy-кода и не предназначен для использования в новых разработках.
   *
   * Сигнал отправляется в момент вызова hyscan_sonar_start() модели, для возможности отмены старта.
   * Обработчики сигнала могут вернуть %TRUE, чтобы отменить старт локатора.
   *
   * Для остлеживания изменения статуса работы локатора используйтся сигнал HyScanSonarState::start-stop.
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

  /* Ретрансляция сигналов. */
  g_signal_connect (HYSCAN_SENSOR (priv->control), "sensor-data",
                    G_CALLBACK (hyscan_control_model_sensor_data), self);
  // g_signal_connect (HYSCAN_DEVICE (priv->control), "device-state",
  //                   G_CALLBACK (hyscan_control_model_device_state), self);
  // g_signal_connect (HYSCAN_DEVICE (priv->control), "device-log",
  //                   G_CALLBACK (hyscan_control_model_device_log), self);
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
  g_atomic_int_set (&priv->stop, TRUE);
  g_mutex_lock (&priv->lock);
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);
  g_thread_join (priv->thread);

  g_clear_object (&priv->control);
  g_hash_table_destroy (priv->sensors);
  g_hash_table_destroy (priv->sources);

  hyscan_track_plan_free (priv->plan);
  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->generated_name);
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
hyscan_control_model_device_state (HyScanDevice     *device,
                                   const gchar      *dev_id,
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

  g_free (start->project);
  g_free (start->track);
  hyscan_track_plan_free (start->plan);

  g_slice_free (HyScanControlModelStart, start);
}

static gboolean
hyscan_control_model_rec_update (HyScanControlModelRec *dest,
                                 HyScanControlModelRec *src)
{
  gboolean changed = FALSE;

  changed |= dest->disabled != src->disabled;
  dest->disabled = src->disabled;

  changed |= dest->mode != src->mode;
  dest->mode = src->mode;

  switch (src->mode)
    {
    case HYSCAN_SONAR_RECEIVER_MODE_MANUAL:
      changed |= dest->receive != src->receive || dest->wait != src->wait;

      dest->receive = src->receive;
      dest->wait = src->wait;
      break;

    case HYSCAN_SONAR_RECEIVER_MODE_AUTO:
    case HYSCAN_SONAR_RECEIVER_MODE_NONE:
      break;

    default:
      g_assert_not_reached ();
    }

  return changed;
}

static gboolean
hyscan_control_model_offset_update (HyScanAntennaOffset       *dest,
                                    const HyScanAntennaOffset *src)
{
  if (memcmp (dest, src, sizeof (*dest)) == 0)
    return FALSE;

  *dest = *src;

  return TRUE;
}

/* Генерирует имя галса по шаблону "<следующий номер в БД><суффикс><одноразовый суффикс>". */
static void
hyscan_control_model_generate_name (HyScanControlModel *model)
{
  HyScanControlModelPrivate *priv = model->priv;
  gint track_num;
  const gchar *suffix, *suffix1;
  HyScanDB *db;

  gint32 project_id;
  gchar **tracks;
  gchar **strs;

  db = hyscan_control_writer_get_db (priv->control);
  /* Работа без системы хранения. */
  if (db == NULL)
    {
      g_free (priv->generated_name);
      priv->generated_name = NULL;
      return;
    }

  /* Ищем в текущем проекте галс с самым большим номером в названии. */
  project_id = hyscan_db_project_open (db, priv->project_name);
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

  suffix1 = priv->suffix1 != NULL ? priv->suffix1 : "";
  suffix = priv->suffix != NULL ? priv->suffix : "";

  g_free (priv->generated_name);
  priv->generated_name = g_strdup_printf ("%03d%s%s", track_num, suffix, suffix1);

  /* Удаляем одноразовый суффикс. */
  g_clear_pointer (&priv->suffix1, g_free);
  g_object_unref (db);
}

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

/* Функция инициирует обновление. */
static void
hyscan_control_model_sonar_changed (HyScanControlModel *self,
                                    HyScanSourceType  source)
{
  HyScanControlModelPrivate *priv = self->priv;

  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_SONAR],
                 g_quark_from_static_string (hyscan_source_get_id_by_type (source)), source);

  /* Помечаем, что пора обновляться, но пока не будим фоновый поток. */
  g_mutex_lock (&priv->lock);
  priv->sync = TRUE;
  g_mutex_unlock (&priv->lock);

  /* Устанавливаем (или заменяем) таймаут для пробуждения фонового потока. */
  if (priv->timeout_id > 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (priv->sync_timeout, hyscan_control_model_timeout_sync, self);
}

static gboolean
hyscan_control_model_receiver_set (HyScanControlModel    *self,
                                   HyScanSourceType       source,
                                   HyScanControlModelRec *rec)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  HyScanControlModelRec *old_rec;
  gboolean status = FALSE;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  old_rec = &info->rec;

  if (rec->disabled)
    status = hyscan_sonar_receiver_disable (HYSCAN_SONAR (priv->control), source);
  else if (rec->mode == HYSCAN_SONAR_RECEIVER_MODE_MANUAL)
    status = hyscan_sonar_receiver_set_time (HYSCAN_SONAR (priv->control), source, rec->receive, rec->wait);
  else if (rec->mode == HYSCAN_SONAR_RECEIVER_MODE_AUTO)
    status = hyscan_sonar_receiver_set_auto (HYSCAN_SONAR (priv->control), source);

  if (!status)
    return FALSE;

  if (hyscan_control_model_rec_update (old_rec, rec))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_control_model_gen_update (HyScanControlModelGen *dest,
                                 HyScanControlModelGen *src)
{
  gboolean changed = FALSE;

  if (dest->disabled != src->disabled)
    {
      changed = TRUE;
      dest->disabled = src->disabled;
    }
  if (dest->preset != src->preset)
    {
      changed = TRUE;
      dest->preset = src->preset;
    }

  return changed;
}

static gboolean
hyscan_control_model_generator_set (HyScanControlModel    *self,
                                    HyScanSourceType       source,
                                    HyScanControlModelGen *gen)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  HyScanControlModelGen *old_gen;
  gboolean status;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  old_gen = &info->gen;

  /* Отключение ВАРУ. */
  if (gen->disabled)
    status = hyscan_sonar_generator_disable (HYSCAN_SONAR (priv->control), source);
  else
    status = hyscan_sonar_generator_set_preset (HYSCAN_SONAR (priv->control), source, gen->preset);

  if (!status)
    return FALSE;

  /* Если параметры отличаются от уже установленных, эмиттируем сигнал . */
  if (hyscan_control_model_gen_update (old_gen, gen))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_control_model_tvg_update (HyScanControlModelTVG *dest,
                                 HyScanControlModelTVG *src)
{
  gboolean changed = FALSE;

  changed |= dest->disabled != src->disabled;
  dest->disabled = src->disabled;

  changed |= dest->mode != src->mode;
  dest->mode = src->mode;

  switch (src->mode)
    {
    case HYSCAN_SONAR_TVG_MODE_AUTO:
      changed |= dest->atvg.level       != src->atvg.level ||
                 dest->atvg.sensitivity != src->atvg.sensitivity;

      dest->atvg.level       = src->atvg.level;
      dest->atvg.sensitivity = src->atvg.sensitivity;
      break;

    case HYSCAN_SONAR_TVG_MODE_CONSTANT:
      changed |= dest->constant.gain || src->constant.gain;

      dest->constant.gain    = src->constant.gain;
      break;

    case HYSCAN_SONAR_TVG_MODE_LINEAR_DB:
      changed |= dest->linear.gain0     != src->linear.gain0 ||
                 dest->linear.gain_step != src->linear.gain_step;

      dest->linear.gain0     = src->linear.gain0;
      dest->linear.gain_step = src->linear.gain_step;
      break;

    case HYSCAN_SONAR_TVG_MODE_LOGARITHMIC:
      changed |= dest->log.gain0 != src->log.gain0 ||
                 dest->log.beta  != src->log.beta  ||
                 dest->log.alpha != src->log.alpha;

      dest->log.gain0        = src->log.gain0;
      dest->log.beta         = src->log.beta;
      dest->log.alpha        = src->log.alpha;
      break;

    case HYSCAN_SONAR_TVG_MODE_NONE:
      break;

    default:
      g_assert_not_reached ();
    }

  return changed;
}

static gboolean
hyscan_control_model_tvg_set (HyScanControlModel    *self,
                              HyScanSourceType       source,
                              HyScanControlModelTVG *tvg)
{
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  HyScanControlModelTVG *old_tvg;
  gboolean status = FALSE;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;
  old_tvg = &info->tvg;

  /* Отключение ВАРУ. */
  if (tvg->disabled || tvg->mode == HYSCAN_SONAR_TVG_MODE_NONE)
    {
      status = hyscan_sonar_tvg_disable (HYSCAN_SONAR (priv->control), source);
    }
  /* Переключение и настройка режимов. */
  else if (tvg->mode == HYSCAN_SONAR_TVG_MODE_AUTO)
    {
      status = hyscan_sonar_tvg_set_auto (HYSCAN_SONAR (priv->control), source,
                                          tvg->atvg.level,
                                          tvg->atvg.sensitivity);
    }
  else if (tvg->mode == HYSCAN_SONAR_TVG_MODE_CONSTANT)
    {
      status = hyscan_sonar_tvg_set_constant (HYSCAN_SONAR (priv->control), source,
                                              tvg->constant.gain);
    }
  else if (tvg->mode == HYSCAN_SONAR_TVG_MODE_LINEAR_DB)
    {
      status = hyscan_sonar_tvg_set_linear_db (HYSCAN_SONAR (priv->control), source,
                                               tvg->linear.gain0,
                                               tvg->linear.gain_step);
    }
  else if (tvg->mode == HYSCAN_SONAR_TVG_MODE_LOGARITHMIC)
    {
      status = hyscan_sonar_tvg_set_logarithmic (HYSCAN_SONAR (priv->control), source,
                                                 tvg->log.gain0,
                                                 tvg->log.beta,
                                                 tvg->log.alpha);
    }

  if (!status)
    return FALSE;

  /* Если параметры отличаются от уже установленных, эмиттируем сигнал. */
  if (hyscan_control_model_tvg_update (old_tvg, tvg))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static HyScanSonarReceiverModeType
hyscan_control_model_receiver_get_mode (HyScanSonarState *sonar,
                                        HyScanSourceType  source)
{
  HyScanControlModelSource *info;
  HyScanControlModelPrivate *priv = HYSCAN_CONTROL_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

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

  return info->rec.disabled;
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
    return FALSE;

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

  return info->tvg.disabled;
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
hyscan_control_model_antenna_set_offset (HyScanSonar               *sonar,
                                         HyScanSourceType           source,
                                         const HyScanAntennaOffset *offset)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sonar);
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSource *info;
  HyScanAntennaOffset *old_offset;
  gboolean status;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  old_offset = &info->offset;

  status = hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (priv->control), source, offset);
  if (!status)
    return FALSE;

  if (hyscan_control_model_offset_update (old_offset, offset))
    hyscan_control_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_control_model_receiver_set_time (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           receive,
                                        gdouble           wait)
{
  HyScanControlModelRec rec = {0};

  rec.disabled = FALSE;
  rec.mode = HYSCAN_SONAR_RECEIVER_MODE_MANUAL;
  rec.receive = receive;
  rec.wait = wait;

  return hyscan_control_model_receiver_set (HYSCAN_CONTROL_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_control_model_receiver_set_auto (HyScanSonar      *sonar,
                                        HyScanSourceType  source)
{
  HyScanControlModelRec rec = {0};

  rec.disabled = FALSE;
  rec.mode = HYSCAN_SONAR_RECEIVER_MODE_AUTO;

  return hyscan_control_model_receiver_set (HYSCAN_CONTROL_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_control_model_receiver_disable (HyScanSonar      *sonar,
                                       HyScanSourceType  source)
{
  HyScanControlModelRec rec = {0};

  rec.disabled = TRUE;
  rec.mode = HYSCAN_SONAR_RECEIVER_MODE_NONE;

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
  HyScanControlModelTVG tvg = {0};

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_AUTO;
  tvg.atvg.level = level;
  tvg.atvg.sensitivity = sensitivity;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_set_constant (HyScanSonar      *sonar,
                                       HyScanSourceType  source,
                                       gdouble           gain)
{
  HyScanControlModelTVG tvg = {0};

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_CONSTANT;
  tvg.constant.gain = gain;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_set_linear_db (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           gain0,
                                        gdouble           gain_step)
{
  HyScanControlModelTVG tvg = {0};

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_LINEAR_DB;
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
  HyScanControlModelTVG tvg = {0};

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_LOGARITHMIC;
  tvg.log.gain0 = gain0;
  tvg.log.beta = beta;
  tvg.log.alpha = alpha;

  return hyscan_control_model_tvg_set (HYSCAN_CONTROL_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_control_model_tvg_disable (HyScanSonar      *sonar,
                                  HyScanSourceType  source)
{
  HyScanControlModelTVG tvg = {0};

  tvg.disabled = TRUE;

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
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelStart start;
  gboolean cancel = FALSE;

  /* Сигнал "before-start" для подготовки всех систем к началу записи и возможности отменить запись. */
  g_signal_emit (self, hyscan_control_model_signals[SIGNAL_BEFORE_START], 0, &cancel);
  if (cancel)
    {
      g_info ("HyScanControlModel: sonar start cancelled in \"before-start\" signal");
      return FALSE;
    }

  start.track_type = track_type;
  start.project = (gchar *) project_name;
  start.track = (gchar *) track_name;
  start.plan = (HyScanTrackPlan *) track_plan;

  /* Устанавливаем параметры для старта ГЛ. */
  g_mutex_lock (&priv->lock);
  hyscan_control_model_start_free (priv->start);
  priv->start = hyscan_control_model_start_copy (&start);
  priv->start_stop = SONAR_START;
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

static gboolean
hyscan_control_model_stop (HyScanSonar *sonar)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sonar);
  HyScanControlModelPrivate *priv = self->priv;

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
hyscan_control_model_sync (HyScanSonar *sonar)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sonar);
  return hyscan_sonar_sync (HYSCAN_SONAR (self->priv->control));
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
      g_signal_emit (self, hyscan_control_model_signals[SIGNAL_SENSOR],
                     g_quark_from_string (name), name);
    }

  return TRUE;
}

static gboolean
hyscan_control_model_sensor_antenna_set_offset (HyScanSensor              *sensor,
                                                const gchar               *name,
                                                const HyScanAntennaOffset *offset)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (sensor);
  HyScanControlModelPrivate *priv = self->priv;
  HyScanControlModelSensor *info = g_hash_table_lookup (priv->sensors, name);

  if (info == NULL)
    return FALSE;

  if (!hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (priv->control), name, offset))
    return FALSE;

  /* Такая же логика, как у локаторов (ТВГ, генератор, приемник). */
  if (memcmp (&info->offset, offset, sizeof (info->offset)) != 0)
    {
      info->offset = *offset;
      g_signal_emit (self, hyscan_control_model_signals[SIGNAL_SENSOR],
                     g_quark_from_string (name), name);
      return TRUE;
    }

  return FALSE;
}

static HyScanDataSchema *
hyscan_control_model_param_schema (HyScanParam *param)
{
  HyScanControlModel *self = HYSCAN_CONTROL_MODEL (param);
  return hyscan_param_schema (HYSCAN_PARAM (self->priv->control));
}

static gpointer
hyscan_control_model_thread (gpointer data)
{
  HyScanControlModel *self = data;
  HyScanControlModelPrivate *priv = self->priv;
  HyScanSonar *sonar = HYSCAN_SONAR (priv->control);

  while (!g_atomic_int_get (&priv->stop))
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

      /* Типа задержка при обмене данными. */
      // g_usleep (G_TIME_SPAN_SECOND);

      /* Синхронизирую локатор. */
      if (sync)
        hyscan_sonar_sync (sonar);

      /* Включаем локатор. */
      if (start_stop == SONAR_START)
        {
          gboolean status;

          status = hyscan_sonar_start (sonar,
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
  iface->antenna_set_offset = hyscan_control_model_antenna_set_offset;
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
  iface->stop = hyscan_control_model_stop;
  iface->sync = hyscan_control_model_sync;
}

static void
hyscan_control_model_sensor_state_iface_init (HyScanSensorStateInterface *iface)
{
  iface->get_enabled = hyscan_control_model_sensor_get_enabled;
}

static void
hyscan_control_model_sensor_iface_init (HyScanSensorInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_model_sensor_antenna_set_offset;
  iface->set_enable = hyscan_control_model_sensor_set_enable;
}

static void
hyscan_control_model_param_iface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_control_model_param_schema;
  iface->set = hyscan_control_model_param_set;
  iface->get = hyscan_control_model_param_get;
}

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
  HyScanControlModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL_MODEL (model));

  priv = model->priv;

  priv->sync_timeout = msec;
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
  HyScanControlModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_CONTROL_MODEL (model), 0);

  priv = model->priv;

  return priv->sync_timeout;
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
  const gchar *track_name;

  g_return_val_if_fail (HYSCAN_IS_CONTROL_MODEL (model), FALSE);

  priv = model->priv;
  if (priv->track_name == NULL)
    {
      hyscan_control_model_generate_name (model);
      track_name = priv->generated_name;
    }
  else
    {
      track_name = priv->track_name;
    }

  return hyscan_sonar_start (HYSCAN_SONAR (model), priv->project_name, track_name, priv->track_type, priv->plan);
}
