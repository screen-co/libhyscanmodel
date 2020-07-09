/* hyscan-sonar-model.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-sonar-model
 * @Title HyScanSonarModel
 * @Short_description
 *
 */

#include "hyscan-sonar-model.h"
#include <hyscan-data-box.h>
#include <string.h>
#include <hyscan-model-marshallers.h>

#define HYSCAN_SONAR_MODEL_TIMEOUT (1000 /* Одна секунда*/)

typedef struct _HyScanSonarModelStart HyScanSonarModelStart;
typedef struct _HyScanSonarModelTVG HyScanSonarModelTVG;
typedef struct _HyScanSonarModelRec HyScanSonarModelRec;
typedef struct _HyScanSonarModelGen HyScanSonarModelGen;
typedef struct _HyScanSonarModelSource HyScanSonarModelSource;
typedef struct _HyScanSonarModelSensor HyScanSonarModelSensor;

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

struct _HyScanSonarModelStart
{
  HyScanTrackType             track_type;
  gchar                      *project;
  gchar                      *track;
  HyScanTrackPlan            *plan;
};

struct _HyScanSonarModelTVG
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

struct _HyScanSonarModelRec
{
  gboolean                    disabled;
  HyScanSonarReceiverModeType mode;
  gdouble                     receive;
  gdouble                     wait;
};

struct _HyScanSonarModelGen
{
  gboolean                    disabled;
  gint64                      preset;
};

struct _HyScanSonarModelSource
{
  HyScanSonarModelTVG         tvg;
  HyScanSonarModelRec         rec;
  HyScanSonarModelGen         gen;
  HyScanAntennaOffset         offset;
};

struct _HyScanSonarModelSensor
{
  gboolean                    enable;
  HyScanAntennaOffset         offset;
};

struct _HyScanSonarModelPrivate
{
  HyScanControl              *control;         /* Бэкэнд. */

  GHashTable                 *sources;         /* Параметры датчиков. {HyScanSourceType : HyScanSonarModelSource* } */
  GHashTable                 *sensors;         /* Параметры датчиков. {gchar* : HyScanSonarModelSensor* } */

  GThread                    *thread;          /* Поток. */
  gint                        stop;            /* Остановка потока. */
  gboolean                    wakeup;          /* Флаг для пробуждения потока. */
  GCond                       cond;            /* Сигнализатор изменения wakeup. */
  GMutex                      lock;            /* Блокировка. */

  gint                        start_stop;      /* Команда для старта или остановки работы локатора. */
  HyScanSonarModelStart      *start;           /* Параметры для старта локатора. */
  HyScanSonarModelStart      *started;         /* Параметры, с которыми был выполнен старт. */
  HyScanSonarModelStart      *started_state;   /* Параметры, с которыми был выполнен старт. */
  guint                       sync_timeout;    /* Период ожидания новых изменений до синхронизации. */
  guint                       timeout_id;      /* Ид таймаут-функции, запускающей синхронизацию. */
  gboolean                    sync;            /* Флаг о необходимости синхронизации. */
};

static void     hyscan_sonar_model_sonar_state_iface_init  (HyScanSonarStateInterface *iface);
static void     hyscan_sonar_model_sonar_iface_init        (HyScanSonarInterface      *iface);
static void     hyscan_sonar_model_sensor_state_iface_init (HyScanSensorStateInterface *iface);
static void     hyscan_sonar_model_sensor_iface_init       (HyScanSensorInterface      *iface);
static void     hyscan_sonar_model_param_iface_init        (HyScanParamInterface       *iface);
static void     hyscan_sonar_model_set_property            (GObject                   *object,
                                                            guint                      prop_id,
                                                            const GValue              *value,
                                                            GParamSpec                *pspec);
static void     hyscan_sonar_model_object_constructed      (GObject                   *object);
static void     hyscan_sonar_model_object_finalize         (GObject                   *object);

static gboolean hyscan_sonar_model_rec_update              (HyScanSonarModelRec       *dest,
                                                            HyScanSonarModelRec       *src);
static gboolean hyscan_sonar_model_receiver_set            (HyScanSonarModel          *self,
                                                            HyScanSourceType           source,
                                                            HyScanSonarModelRec       *rec);
static gboolean hyscan_sonar_model_gen_update              (HyScanSonarModelGen       *dest,
                                                            HyScanSonarModelGen       *src);
static gboolean hyscan_sonar_model_generator_set           (HyScanSonarModel          *self,
                                                            HyScanSourceType           source,
                                                            HyScanSonarModelGen       *gen);
static gboolean hyscan_sonar_model_tvg_update              (HyScanSonarModelTVG       *dest,
                                                            HyScanSonarModelTVG       *src);
static gboolean hyscan_sonar_model_tvg_set                 (HyScanSonarModel          *self,
                                                            HyScanSourceType           source,
                                                            HyScanSonarModelTVG       *tvg);
static gpointer hyscan_sonar_model_param_thread            (gpointer                   data);
static void     hyscan_sonar_model_sensor_data             (HyScanDevice              *device,
                                                            const gchar               *sensor,
                                                            gint                       source,
                                                            gint64                     time,
                                                            HyScanBuffer              *data,
                                                            HyScanSonarModel          *self);
static void     hyscan_sonar_model_sonar_signal            (HyScanDevice              *device,
                                                            gint                       source,
                                                            guint                      channel,
                                                            gint64                     time,
                                                            HyScanBuffer              *image,
                                                            HyScanSonarModel          *self);
static void     hyscan_sonar_model_sonar_tvg               (HyScanDevice              *device,
                                                            gint                       source,
                                                            guint                      channel,
                                                            gint64                     time,
                                                            HyScanBuffer              *gains,
                                                            HyScanSonarModel          *self);
static void     hyscan_sonar_model_sonar_acoustic_data     (HyScanDevice              *device,
                                                            gint                       source,
                                                            guint                      channel,
                                                            gboolean                   noise,
                                                            gint64                     time,
                                                            HyScanAcousticDataInfo    *info,
                                                            HyScanBuffer              *data,
                                                            HyScanSonarModel          *self);
static void     hyscan_sonar_model_device_state            (HyScanDevice              *device,
                                                            const gchar               *dev_id,
                                                            HyScanSonarModel          *self);
static void     hyscan_sonar_model_device_log              (HyScanDevice              *device,
                                                            const gchar               *source,
                                                            gint64                     time,
                                                            gint                       level,
                                                            const gchar               *message,
                                                            HyScanSonarModel          *self);
static HyScanSonarModelStart *
                hyscan_sonar_model_start_copy               (const HyScanSonarModelStart *start);

static void     hyscan_sonar_model_start_free               (HyScanSonarModelStart    *start);

static guint hyscan_sonar_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanSonarModel, hyscan_sonar_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarModel)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR_STATE, hyscan_sonar_model_sonar_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_sonar_model_sonar_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR_STATE, hyscan_sonar_model_sensor_state_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_sonar_model_sensor_iface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_sonar_model_param_iface_init))

static void
hyscan_sonar_model_class_init (HyScanSonarModelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_sonar_model_set_property;
  oclass->constructed = hyscan_sonar_model_object_constructed;
  oclass->finalize = hyscan_sonar_model_object_finalize;

  g_object_class_install_property (oclass, PROP_CONTROL,
    g_param_spec_object ("control", "HyScanControl", "HyScanControl",
                         HYSCAN_TYPE_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_model_signals[SIGNAL_SONAR] =
    g_signal_new ("sonar", HYSCAN_TYPE_SONAR_MODEL,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE,
                  1, G_TYPE_INT);

  hyscan_sonar_model_signals[SIGNAL_SENSOR] =
    g_signal_new ("sensor", HYSCAN_TYPE_SONAR_MODEL,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  hyscan_sonar_model_signals[SIGNAL_PARAM] =
    g_signal_new ("param", HYSCAN_TYPE_SONAR_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  hyscan_sonar_model_signals[SIGNAL_BEFORE_START] =
    g_signal_new ("before-start", HYSCAN_TYPE_SONAR_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  g_signal_accumulator_true_handled, NULL,
                  hyscan_model_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN,
                  0);

  hyscan_sonar_model_signals[SIGNAL_START_STOP] = g_signal_lookup ("start-stop", HYSCAN_TYPE_SONAR_STATE);
}

static void
hyscan_sonar_model_init (HyScanSonarModel *sonar_model)
{
  sonar_model->priv = hyscan_sonar_model_get_instance_private (sonar_model);
}

static void
hyscan_sonar_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = self->priv;

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
hyscan_sonar_model_object_constructed (GObject *object)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = self->priv;

  guint32 n_sources, i;
  const HyScanSourceType *source;
  const gchar *const *sensor;

  /* Ретрансляция сигналов. */
  g_signal_connect (HYSCAN_SENSOR (priv->control), "sensor-data",
                    G_CALLBACK (hyscan_sonar_model_sensor_data), self);
  // g_signal_connect (HYSCAN_DEVICE (priv->control), "device-state",
  //                   G_CALLBACK (hyscan_sonar_model_device_state), self);
  // g_signal_connect (HYSCAN_DEVICE (priv->control), "device-log",
  //                   G_CALLBACK (hyscan_sonar_model_device_log), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-signal",
                    G_CALLBACK (hyscan_sonar_model_sonar_signal), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-tvg",
                    G_CALLBACK (hyscan_sonar_model_sonar_tvg), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-acoustic-data",
                    G_CALLBACK (hyscan_sonar_model_sonar_acoustic_data), self);

  priv->sync_timeout = HYSCAN_SONAR_MODEL_TIMEOUT;

  /* Получаем список источников данных. */
  priv->sources = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  source = hyscan_control_sources_list (priv->control, &n_sources);
  if (source != NULL)
    {
      for (i = 0; i < n_sources; ++i)
        g_hash_table_insert (priv->sources, GINT_TO_POINTER (source[i]), g_new0 (HyScanSonarModelSource, 1));
    }

  /* Получаем список датчиков. */
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  sensor = hyscan_control_sensors_list (priv->control);
  if (sensor != NULL)
    {
      for (i = 0; sensor[i] != NULL; i++)
        g_hash_table_insert (priv->sensors, g_strdup (sensor[i]), g_new0 (HyScanSonarModelSensor, 1));
    }

  g_cond_init (&priv->cond);

  /* Поток обмена. */
  g_mutex_init (&priv->lock);
  priv->thread = g_thread_new ("sonar-model", hyscan_sonar_model_param_thread, self);
}

static void
hyscan_sonar_model_object_finalize (GObject *object)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = self->priv;

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

  G_OBJECT_CLASS (hyscan_sonar_model_parent_class)->finalize (object);
}

static gboolean
hyscan_sonar_model_start_stop (HyScanSonarModel *self)
{
  HyScanSonarModelPrivate *priv = self->priv;

  g_mutex_lock (&priv->lock);
  hyscan_sonar_model_start_free (priv->started_state);
  priv->started_state = priv->started;
  priv->started = NULL;
  g_mutex_unlock (&priv->lock);

  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_START_STOP], 0);

  return G_SOURCE_REMOVE;
}

/* Обработчик сигнала sensor-data. */
static void
hyscan_sonar_model_sensor_data (HyScanDevice     *device,
                                const gchar      *sensor,
                                gint              source,
                                gint64            time,
                                HyScanBuffer     *data,
                                HyScanSonarModel *self)
{
  g_signal_emit_by_name (self, "sensor-data", sensor, source, time, data);
}

/* Обработчик сигнала sonar-signal. */
static void
hyscan_sonar_model_sonar_signal (HyScanDevice     *device,
                                 gint              source,
                                 guint             channel,
                                 gint64            time,
                                 HyScanBuffer     *image,
                                 HyScanSonarModel *self)
{
  g_signal_emit_by_name (self, "sonar-signal", source, channel, time, image);
}

/* Обработчик сигнала sonar-tvg. */
static void
hyscan_sonar_model_sonar_tvg (HyScanDevice     *device,
                              gint              source,
                              guint             channel,
                              gint64            time,
                              HyScanBuffer     *gains,
                              HyScanSonarModel *self)
{
  g_signal_emit_by_name (self, "sonar-tvg", source, channel, time, gains);
}

/* Обработчик сигнала sonar-acoustic-data. */
static void
hyscan_sonar_model_sonar_acoustic_data (HyScanDevice           *device,
                                        gint                    source,
                                        guint                   channel,
                                        gboolean                noise,
                                        gint64                  time,
                                        HyScanAcousticDataInfo *info,
                                        HyScanBuffer           *data,
                                        HyScanSonarModel       *self)
{
  g_signal_emit_by_name (self, "sonar-acoustic-data",
                                source, channel, noise,
                                time, info, data);
}

/* Обработчик сигнала device-state. */
static void
hyscan_sonar_model_device_state (HyScanDevice     *device,
                                 const gchar      *dev_id,
                                 HyScanSonarModel *self)
{
  g_signal_emit_by_name (self, "device-state", dev_id);
}

/* Обработчик сигнала device-log. */
static void
hyscan_sonar_model_device_log (HyScanDevice     *device,
                               const gchar      *source,
                               gint64            time,
                               gint              level,
                               const gchar      *message,
                               HyScanSonarModel *self)
{
  g_signal_emit_by_name (self, "device-log", source, time, level, message);
}

static HyScanSonarModelStart *
hyscan_sonar_model_start_copy (const HyScanSonarModelStart *start)
{
  HyScanSonarModelStart *copy;

  if (start == NULL)
    return NULL;

  copy = g_slice_new (HyScanSonarModelStart);
  copy->track_type = start->track_type;
  copy->project = g_strdup (start->project);
  copy->track = g_strdup (start->track);
  copy->plan = hyscan_track_plan_copy (start->plan);

  return copy;
}

static void
hyscan_sonar_model_start_free (HyScanSonarModelStart *start)
{
  if (start == NULL)
    return;

  g_free (start->project);
  g_free (start->track);
  hyscan_track_plan_free (start->plan);

  g_slice_free (HyScanSonarModelStart, start);
}

static gboolean
hyscan_sonar_model_rec_update (HyScanSonarModelRec *dest,
                               HyScanSonarModelRec *src)
{
  gboolean changed = FALSE;

  if (dest->disabled != src->disabled)
    {
      changed = TRUE;
      dest->disabled = src->disabled;
    }

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
hyscan_sonar_model_offset_update (HyScanAntennaOffset       *dest,
                                  const HyScanAntennaOffset *src)
{
  if (memcmp (dest, src, sizeof (*dest)) == 0)
    return FALSE;

  *dest = *src;

  return TRUE;
}

static gboolean
hyscan_sonar_model_timeout_sync (gpointer user_data)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (user_data);
  HyScanSonarModelPrivate *priv = self->priv;

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
hyscan_sonar_model_sonar_changed (HyScanSonarModel *self,
                                  HyScanSourceType  source)
{
  HyScanSonarModelPrivate *priv = self->priv;

  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_SONAR],
                 g_quark_from_static_string (hyscan_source_get_id_by_type (source)), 0);

  /* Помечаем, что пора обновляться, но пока не будим фоновый поток. */
  g_mutex_lock (&priv->lock);
  priv->sync = TRUE;
  g_mutex_unlock (&priv->lock);

  /* Устанавливаем (или заменяем) таймаут для пробуждения фонового потока. */
  if (priv->timeout_id > 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (priv->sync_timeout, hyscan_sonar_model_timeout_sync, self);
}

static gboolean
hyscan_sonar_model_receiver_set (HyScanSonarModel    *self,
                                 HyScanSourceType     source,
                                 HyScanSonarModelRec *rec)
{
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelSource *info;
  HyScanSonarModelRec *old_rec;
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

  if (hyscan_sonar_model_rec_update (old_rec, rec))
    hyscan_sonar_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_sonar_model_gen_update (HyScanSonarModelGen *dest,
                               HyScanSonarModelGen *src)
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
hyscan_sonar_model_generator_set (HyScanSonarModel    *self,
                                  HyScanSourceType     source,
                                  HyScanSonarModelGen *gen)
{
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelSource *info;
  HyScanSonarModelGen *old_gen;
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
  if (hyscan_sonar_model_gen_update (old_gen, gen))
    hyscan_sonar_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_sonar_model_tvg_update (HyScanSonarModelTVG *dest,
                               HyScanSonarModelTVG *src)
{
  gboolean changed = FALSE;

  if (dest->disabled != src->disabled)
    {
      changed = TRUE;
      dest->disabled = src->disabled;
    }

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
hyscan_sonar_model_tvg_set (HyScanSonarModel    *self,
                            HyScanSourceType     source,
                            HyScanSonarModelTVG *tvg)
{
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelSource *info;
  HyScanSonarModelTVG *old_tvg;
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
  if (hyscan_sonar_model_tvg_update (old_tvg, tvg))
    hyscan_sonar_model_sonar_changed (self, source);

  return TRUE;
}

static HyScanSonarReceiverModeType
hyscan_sonar_model_receiver_get_mode (HyScanSonarState *sonar,
                                      HyScanSourceType  source)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->rec.mode;
}

static gboolean
hyscan_sonar_model_receiver_get_time (HyScanSonarState *sonar,
                                      HyScanSourceType  source,
                                      gdouble          *receive_time,
                                      gdouble          *wait_time)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  receive_time != NULL ? *receive_time = info->rec.receive : 0;
  wait_time != NULL ? *wait_time = info->rec.wait : 0;

  return TRUE;
}

static gboolean
hyscan_sonar_model_receiver_get_disabled (HyScanSonarState *sonar,
                                          HyScanSourceType  source)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->rec.disabled;
}

static gboolean
hyscan_sonar_model_generator_get_preset (HyScanSonarState *sonar,
                                         HyScanSourceType  source,
                                         gint64           *preset)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  preset != NULL ? *preset = info->gen.preset : 0;
  return TRUE;
}

static gboolean
hyscan_sonar_model_generator_get_disabled (HyScanSonarState *sonar,
                                           HyScanSourceType  source)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->gen.disabled;
}

static HyScanSonarTVGModeType
hyscan_sonar_model_tvg_get_mode (HyScanSonarState *sonar,
                                 HyScanSourceType  source)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->tvg.mode;
}

static gboolean
hyscan_sonar_model_tvg_get_auto (HyScanSonarState *sonar,
                                 HyScanSourceType  source,
                                 gdouble          *level,
                                 gdouble          *sensitivity)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  level != NULL ? *level = info->tvg.atvg.level : 0;
  sensitivity != NULL ? *sensitivity = info->tvg.atvg.sensitivity : 0;
  return TRUE;
}

static gboolean
hyscan_sonar_model_tvg_get_constant (HyScanSonarState *sonar,
                                     HyScanSourceType  source,
                                     gdouble          *gain)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  gain != NULL ? *gain = info->tvg.constant.gain : 0;
  return TRUE;
}

static gboolean
hyscan_sonar_model_tvg_get_linear_db (HyScanSonarState *sonar,
                                      HyScanSourceType  source,
                                      gdouble          *gain0,
                                      gdouble          *gain_step)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  gain0 != NULL ? *gain0 = info->tvg.linear.gain0 : 0;
  gain_step != NULL ? *gain_step = info->tvg.linear.gain_step : 0;
  return TRUE;
}

static gboolean
hyscan_sonar_model_tvg_get_logarithmic (HyScanSonarState *sonar,
                                        HyScanSourceType  source,
                                        gdouble          *gain0,
                                        gdouble          *beta,
                                        gdouble          *alpha)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  gain0 != NULL ? *gain0 = info->tvg.log.gain0 : 0;
  beta != NULL ? *beta = info->tvg.log.beta : 0;
  alpha != NULL ? *alpha = info->tvg.log.alpha : 0;
  return TRUE;
}

static gboolean
hyscan_sonar_model_tvg_get_disabled (HyScanSonarState *sonar,
                                     HyScanSourceType  source)
{
  HyScanSonarModelSource *info;
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  return info->tvg.disabled;
}

static gboolean
hyscan_sonar_model_get_start (HyScanSonarState  *sonar,
                              gchar            **project_name,
                              gchar            **track_name,
                              HyScanTrackType   *track_type,
                              HyScanTrackPlan  **plan)
{
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (sonar)->priv;
  HyScanSonarModelStart *state = priv->started_state;

  if (state == NULL)
    return FALSE;

  project_name != NULL ? *project_name = g_strdup (state->project) : 0;
  track_name != NULL ? *track_name = g_strdup (state->track) : 0;
  track_type != NULL ? *track_type = state->track_type : 0;
  plan != NULL ? *plan = hyscan_track_plan_copy (state->plan) : 0;

  return TRUE;
}

static gboolean
hyscan_sonar_model_antenna_set_offset (HyScanSonar               *sonar,
                                       HyScanSourceType           source,
                                       const HyScanAntennaOffset *offset)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sonar);
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelSource *info;
  HyScanAntennaOffset *old_offset;
  gboolean status;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  old_offset = &info->offset;

  status = hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (priv->control), source, offset);
  if (!status)
    return FALSE;

  if (hyscan_sonar_model_offset_update (old_offset, offset))
    hyscan_sonar_model_sonar_changed (self, source);

  return TRUE;
}

static gboolean
hyscan_sonar_model_receiver_set_time (HyScanSonar      *sonar,
                                      HyScanSourceType  source,
                                      gdouble           receive,
                                      gdouble           wait)
{
  HyScanSonarModelRec rec;

  rec.disabled = FALSE;
  rec.mode = HYSCAN_SONAR_RECEIVER_MODE_MANUAL;
  rec.receive = receive;
  rec.wait = wait;

  return hyscan_sonar_model_receiver_set (HYSCAN_SONAR_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_sonar_model_receiver_set_auto (HyScanSonar      *sonar,
                                      HyScanSourceType  source)
{
  HyScanSonarModelRec rec;

  rec.disabled = FALSE;
  rec.mode = HYSCAN_SONAR_RECEIVER_MODE_AUTO;

  return hyscan_sonar_model_receiver_set (HYSCAN_SONAR_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_sonar_model_receiver_disable (HyScanSonar      *sonar,
                                     HyScanSourceType  source)
{
  HyScanSonarModelRec rec;

  rec.mode = HYSCAN_SONAR_RECEIVER_MODE_NONE;
  rec.disabled = TRUE;

  return hyscan_sonar_model_receiver_set (HYSCAN_SONAR_MODEL (sonar), source, &rec);
}

static gboolean
hyscan_sonar_model_generator_set_preset (HyScanSonar      *sonar,
                                         HyScanSourceType  source,
                                         gint64            preset)
{
  HyScanSonarModelGen gen;

  gen.disabled = FALSE;
  gen.preset = preset;

  return hyscan_sonar_model_generator_set (HYSCAN_SONAR_MODEL (sonar), source, &gen);
}

static gboolean
hyscan_sonar_model_generator_disable (HyScanSonar      *sonar,
                                      HyScanSourceType  source)
{
  HyScanSonarModelGen gen;

  gen.disabled = TRUE;

  return hyscan_sonar_model_generator_set (HYSCAN_SONAR_MODEL (sonar), source, &gen);
}

static gboolean
hyscan_sonar_model_tvg_set_auto (HyScanSonar      *sonar,
                                 HyScanSourceType  source,
                                 gdouble           level,
                                 gdouble           sensitivity)
{
  HyScanSonarModelTVG tvg = {0, };

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_AUTO;
  tvg.atvg.level = level;
  tvg.atvg.sensitivity = sensitivity;

  return hyscan_sonar_model_tvg_set (HYSCAN_SONAR_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_sonar_model_tvg_set_constant (HyScanSonar      *sonar,
                                     HyScanSourceType  source,
                                     gdouble           gain)
{
  HyScanSonarModelTVG tvg = {0, };

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_CONSTANT;
  tvg.constant.gain = gain;

  return hyscan_sonar_model_tvg_set (HYSCAN_SONAR_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_sonar_model_tvg_set_linear_db (HyScanSonar      *sonar,
                                      HyScanSourceType  source,
                                      gdouble           gain0,
                                      gdouble           gain_step)
{
  HyScanSonarModelTVG tvg = {0, };

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_LINEAR_DB;
  tvg.linear.gain0 = gain0;
  tvg.linear.gain_step = gain_step;

  return hyscan_sonar_model_tvg_set (HYSCAN_SONAR_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_sonar_model_tvg_set_logarithmic (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           gain0,
                                        gdouble           beta,
                                        gdouble           alpha)
{
  HyScanSonarModelTVG tvg = {0, };

  tvg.disabled = FALSE;
  tvg.mode = HYSCAN_SONAR_TVG_MODE_LOGARITHMIC;
  tvg.log.gain0 = gain0;
  tvg.log.beta = beta;
  tvg.log.alpha = alpha;

  return hyscan_sonar_model_tvg_set (HYSCAN_SONAR_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_sonar_model_tvg_disable (HyScanSonar      *sonar,
                                HyScanSourceType  source)
{
  HyScanSonarModelTVG tvg = {0, };

  tvg.disabled = TRUE;

  return hyscan_sonar_model_tvg_set (HYSCAN_SONAR_MODEL (sonar), source, &tvg);
}

static gboolean
hyscan_sonar_model_start (HyScanSonar           *sonar,
                          const gchar           *project_name,
                          const gchar           *track_name,
                          HyScanTrackType        track_type,
                          const HyScanTrackPlan *track_plan)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sonar);
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelStart start;
  gboolean cancel = FALSE;

  /* Сигнал "before-start" для подготовки всех систем к началу записи и возможности отменить запись. */
  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_BEFORE_START], 0, &cancel);
  if (cancel)
    {
      g_info ("HyScanSonarModel: sonar start cancelled in \"before-start\" signal");
      return FALSE;
    }

  start.track_type = track_type;
  start.project = (gchar *) project_name;
  start.track = (gchar *) track_name;
  start.plan = (HyScanTrackPlan *) track_plan;

  /* Устанавливаем параметры для старта ГЛ. */
  g_mutex_lock (&priv->lock);
  hyscan_sonar_model_start_free (priv->start);
  priv->start = hyscan_sonar_model_start_copy (&start);
  priv->start_stop = SONAR_START;
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

static gboolean
hyscan_sonar_model_stop (HyScanSonar *sonar)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sonar);
  HyScanSonarModelPrivate *priv = self->priv;

  /* Устанавливаем параметры для остановки ГЛ. */
  g_mutex_lock (&priv->lock);
  priv->start_stop = SONAR_STOP;
  g_clear_pointer (&priv->start, hyscan_sonar_model_start_free);
  priv->wakeup = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

static gboolean
hyscan_sonar_model_sync (HyScanSonar *sonar)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sonar);
  return hyscan_sonar_sync (HYSCAN_SONAR (self->priv->control));
}

static gboolean
hyscan_sonar_model_sensor_get_enabled (HyScanSensorState *sensor,
                                       const gchar       *name)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sensor);
  HyScanSonarModelSensor *info = g_hash_table_lookup (self->priv->sensors, name);

  if (info == NULL)
    return FALSE;

  return info->enable;
}

static gboolean
hyscan_sonar_model_sensor_set_enable (HyScanSensor *sensor,
                                      const gchar  *name,
                                      gboolean      enable)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sensor);
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelSensor *info = g_hash_table_lookup (priv->sensors, name);

  if (info == NULL)
    return FALSE;

  if (!hyscan_sensor_set_enable (HYSCAN_SENSOR (priv->control), name, enable))
    return FALSE;

  /* Такая же логика, как у локаторов (ТВГ, генератор, приемник). */
  if (enable != info->enable)
    {
      info->enable = enable;
      g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_SENSOR],
                     g_quark_from_string (name), 0);
    }

  return TRUE;
}

static gboolean
hyscan_sonar_model_sensor_antenna_set_offset (HyScanSensor              *sensor,
                                              const gchar               *name,
                                              const HyScanAntennaOffset *offset)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sensor);
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonarModelSensor *info = g_hash_table_lookup (priv->sensors, name);

  if (info == NULL)
    return FALSE;

  if (!hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (priv->control), name, offset))
    return FALSE;

  /* Такая же логика, как у локаторов (ТВГ, генератор, приемник). */
  if (memcmp (&info->offset, offset, sizeof (info->offset)) != 0)
    {
      info->offset = *offset;
      g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_SENSOR],
                     g_quark_from_string (name), 0);
      return TRUE;
    }

  return FALSE;
}

static HyScanDataSchema *
hyscan_sonar_model_param_schema (HyScanParam *param)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (param);
  return hyscan_param_schema (HYSCAN_PARAM (self->priv->control));
}

static gpointer
hyscan_sonar_model_param_thread (gpointer data)
{
  HyScanSonarModel *self = data;
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanSonar *sonar = HYSCAN_SONAR (priv->control);

  while (!g_atomic_int_get (&priv->stop))
    {
      HyScanSonarModelStart *start;
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
              g_warning ("HyScanSonarModel: internal sonar failed to start");
              g_clear_pointer (&start, &hyscan_sonar_model_start_free);
            }
        }

      /* Выключаем локатор. */
      else if (start_stop == SONAR_STOP)
        {
          g_clear_pointer (&start, &hyscan_sonar_model_start_free);
          hyscan_sonar_stop (sonar);
        }

      /* Отправляем сигнал об изменении статуса работы локатора. */
      if (start_stop != SONAR_NO_CHANGE)
        {
          g_mutex_lock (&priv->lock);
          hyscan_sonar_model_start_free (priv->started);
          priv->started = start;
          g_mutex_unlock (&priv->lock);

          g_idle_add ((GSourceFunc) hyscan_sonar_model_start_stop, self);
        }
    }

  return NULL;
}

static gboolean
hyscan_sonar_model_param_set (HyScanParam     *param,
                              HyScanParamList *list)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (param);
  HyScanSonarModelPrivate *priv = self->priv;

  return hyscan_param_set (HYSCAN_PARAM (priv->control), list);
}

static gboolean
hyscan_sonar_model_param_get (HyScanParam     *param,
                              HyScanParamList *list)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (param);
  HyScanSonarModelPrivate *priv = self->priv;

  return hyscan_param_get (HYSCAN_PARAM (priv->control), list);
}


HyScanSonarModel *
hyscan_sonar_model_new (HyScanControl *control)
{
  return g_object_new (HYSCAN_TYPE_SONAR_MODEL,
                       "control", control,
                       NULL);
}

void
hyscan_sonar_model_set_sync_timeout (HyScanSonarModel *model,
                                     guint             msec)
{
  HyScanSonarModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_MODEL (model));

  priv = model->priv;

  priv->sync_timeout = msec;
}

guint
hyscan_sonar_model_get_sync_timeout (HyScanSonarModel *model)
{
  HyScanSonarModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_SONAR_MODEL (model), 0);

  priv = model->priv;

  return priv->sync_timeout;
}

static void
hyscan_sonar_model_sonar_state_iface_init (HyScanSonarStateInterface *iface)
{
  iface->receiver_get_mode = hyscan_sonar_model_receiver_get_mode;
  iface->receiver_get_time = hyscan_sonar_model_receiver_get_time;
  iface->receiver_get_disabled = hyscan_sonar_model_receiver_get_disabled;
  iface->generator_get_preset = hyscan_sonar_model_generator_get_preset;
  iface->generator_get_disabled = hyscan_sonar_model_generator_get_disabled;
  iface->tvg_get_mode = hyscan_sonar_model_tvg_get_mode;
  iface->tvg_get_auto = hyscan_sonar_model_tvg_get_auto;
  iface->tvg_get_constant = hyscan_sonar_model_tvg_get_constant;
  iface->tvg_get_linear_db = hyscan_sonar_model_tvg_get_linear_db;
  iface->tvg_get_logarithmic = hyscan_sonar_model_tvg_get_logarithmic;
  iface->tvg_get_disabled = hyscan_sonar_model_tvg_get_disabled;
  iface->get_start = hyscan_sonar_model_get_start;
}

static void
hyscan_sonar_model_sonar_iface_init (HyScanSonarInterface *iface)
{
  iface->antenna_set_offset = hyscan_sonar_model_antenna_set_offset;
  iface->receiver_set_time = hyscan_sonar_model_receiver_set_time;
  iface->receiver_set_auto = hyscan_sonar_model_receiver_set_auto;
  iface->receiver_disable = hyscan_sonar_model_receiver_disable;
  iface->generator_set_preset = hyscan_sonar_model_generator_set_preset;
  iface->generator_disable = hyscan_sonar_model_generator_disable;
  iface->tvg_set_auto = hyscan_sonar_model_tvg_set_auto;
  iface->tvg_set_constant = hyscan_sonar_model_tvg_set_constant;
  iface->tvg_set_linear_db = hyscan_sonar_model_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_sonar_model_tvg_set_logarithmic;
  iface->tvg_disable = hyscan_sonar_model_tvg_disable;
  iface->start = hyscan_sonar_model_start;
  iface->stop = hyscan_sonar_model_stop;
  iface->sync = hyscan_sonar_model_sync;
}

static void
hyscan_sonar_model_sensor_state_iface_init (HyScanSensorStateInterface *iface)
{
  iface->get_enabled = hyscan_sonar_model_sensor_get_enabled;
}

static void
hyscan_sonar_model_sensor_iface_init (HyScanSensorInterface *iface)
{
  iface->antenna_set_offset = hyscan_sonar_model_sensor_antenna_set_offset;
  iface->set_enable = hyscan_sonar_model_sensor_set_enable;
}

static void
hyscan_sonar_model_param_iface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_sonar_model_param_schema;
  iface->set = hyscan_sonar_model_param_set;
  iface->get = hyscan_sonar_model_param_get;
}
