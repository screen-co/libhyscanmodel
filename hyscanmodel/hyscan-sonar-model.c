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

#define HYSCAN_SONAR_MODEL_TIMEOUT (1 /* Одна секунда*/)

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
  HyScanControl              *control; /* Бэкэнд. */

  GHashTable                 *sources; /* Параметры датчиков. {HyScanSourceType : HyScanSonarModelSource* } */
  GHashTable                 *sensors; /* Параметры датчиков. {gchar* : HyScanSonarModelSensor* } */

  HyScanParam                *fake;    /* Фейковый бекенд для мейнлупа. */
  HyScanParamList            *incoming;/* Входящий список параметров. */
  GThread                    *thread;  /* Поток. */
  gint                        stop;    /* Остановка потока. */
  GMutex                      lock;    /* Блокировка. */

  GTimer                     *timer;
  gboolean                    changed;
  GCond                       cond;
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

static void     hyscan_sonar_model_changed                 (HyScanSonarModelPrivate   *priv);
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

  hyscan_sonar_model_signals[SIGNAL_START_STOP] =
    g_signal_new ("start-stop", HYSCAN_TYPE_SONAR_MODEL,
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

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
  HyScanDataSchema *schema;

  /* Ретрансляция сигналов. */
  g_signal_connect (HYSCAN_SENSOR (priv->control), "sensor-data",
                    G_CALLBACK (hyscan_sonar_model_sensor_data), self);
  g_signal_connect (HYSCAN_DEVICE (priv->control), "device-state",
                    G_CALLBACK (hyscan_sonar_model_device_state), self);
  g_signal_connect (HYSCAN_DEVICE (priv->control), "device-log",
                    G_CALLBACK (hyscan_sonar_model_device_log), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-signal",
                    G_CALLBACK (hyscan_sonar_model_sonar_signal), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-tvg",
                    G_CALLBACK (hyscan_sonar_model_sonar_tvg), self);
  g_signal_connect (HYSCAN_SONAR (priv->control), "sonar-acoustic-data",
                    G_CALLBACK (hyscan_sonar_model_sonar_acoustic_data), self);

  /* Промежуточный HyScanParam, в который пишет мейн-луп. */
  schema = hyscan_param_schema (HYSCAN_PARAM (priv->control));
  priv->fake = HYSCAN_PARAM (hyscan_data_box_new (schema));

  g_cond_init (&priv->cond);

  /* Поток обмена. */
  g_mutex_init (&priv->lock);
  priv->thread = g_thread_new ("sonar-model", hyscan_sonar_model_param_thread, self);

  g_clear_object (&schema);
}

static void
hyscan_sonar_model_object_finalize (GObject *object)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (object);
  HyScanSonarModelPrivate *priv = self->priv;

  g_clear_object (&priv->control);

  G_OBJECT_CLASS (hyscan_sonar_model_parent_class)->finalize (object);
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

/* Функция инициирует обновление. */
static void
hyscan_sonar_model_changed (HyScanSonarModelPrivate *priv)
{
  g_timer_start (priv->timer);
  g_atomic_int_set (&priv->changed, TRUE);
  g_cond_signal (&priv->cond);
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
  if (dest->mode != src->mode)
    {
      changed = TRUE;
      dest->mode = src->mode;

      switch (src->mode)
        {
        case HYSCAN_SONAR_RECEIVER_MODE_MANUAL:
          dest->receive = src->receive;
          dest->wait = src->wait;

        case HYSCAN_SONAR_RECEIVER_MODE_AUTO:
        case HYSCAN_SONAR_RECEIVER_MODE_NONE:
          break;

        default:
          g_assert_not_reached ();
        }
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

static void
hyscan_sonar_model_sonar_changed (HyScanSonarModel *self,
                                  HyScanSourceType  source)
{
  HyScanSonarModelPrivate *priv = self->priv;

  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_SONAR],
                     g_quark_from_static_string (hyscan_source_get_id_by_type (source)), 0);

  /* Помечаем, что пора обновляться. */
  g_mutex_lock (&priv->lock);
  hyscan_sonar_model_changed (priv);
  g_mutex_unlock (&priv->lock);
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
  if (dest->mode != src->mode)
    {
      changed = TRUE;
      dest->mode = src->mode;

      switch (src->mode)
        {
        case HYSCAN_SONAR_TVG_MODE_AUTO:
          dest->atvg.level       = src->atvg.level;
          dest->atvg.sensitivity = src->atvg.sensitivity;
          break;

        case HYSCAN_SONAR_TVG_MODE_CONSTANT:
          dest->constant.gain    = src->constant.gain;
          break;

        case HYSCAN_SONAR_TVG_MODE_LINEAR_DB:
          dest->linear.gain0     = src->linear.gain0;
          dest->linear.gain_step = src->linear.gain_step;
          break;

        case HYSCAN_SONAR_TVG_MODE_LOGARITHMIC:
          dest->log.gain0        = src->log.gain0;
          dest->log.beta         = src->log.beta;
          dest->log.alpha        = src->log.alpha;
          break;

        case HYSCAN_SONAR_TVG_MODE_NONE:
          break;

        default:
          g_assert_not_reached ();
        }
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
  gboolean cancel = FALSE;
  gboolean result;

  /* Сигнал "before-start" для подготовки всех систем к началу записи и возможности отменить запись. */
  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_BEFORE_START], 0, &cancel);

  result = !cancel && hyscan_sonar_start (HYSCAN_SONAR (self->priv->control),
                                          project_name,
                                          track_name,
                                          track_type,
                                          track_plan);

  /* Сигнал "start-stop" отправляем в любом случае, даже если запись не началась. */
  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_START_STOP], 0, result ? track_name : NULL);

  return result;
}

static gboolean
hyscan_sonar_model_stop (HyScanSonar *sonar)
{
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (sonar);
  gboolean result;

  result = hyscan_sonar_stop (HYSCAN_SONAR (self->priv->control));

  g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_START_STOP], 0, NULL);

  return result;
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
  return hyscan_param_schema (self->priv->fake);
}

static gpointer
hyscan_sonar_model_param_thread (gpointer data)
{
  HyScanSonarModel *self = data;
  HyScanSonarModelPrivate *priv = self->priv;
  HyScanParam *remote = HYSCAN_PARAM (g_object_ref (priv->control));
  HyScanParamList *list, *full;

  list = hyscan_param_list_new ();
  full = hyscan_param_list_new ();

  /* Создаем HyScanParamList со всеми ключами схемы. */
  {
    HyScanDataSchema *schema;
    const gchar * const * keys;

    schema = hyscan_param_schema (remote);
    keys = hyscan_data_schema_list_keys (schema);

    for (; keys != NULL && *keys != NULL; ++keys)
      hyscan_param_list_add (full, *keys);

    g_clear_object (&schema);
  }

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Если ничего не поменялось, жду. */
      if (!g_atomic_int_get (&priv->changed))
        {
          g_mutex_lock (&priv->lock);
          g_cond_wait (&priv->cond, &priv->lock);
          g_mutex_unlock (&priv->lock);
          continue;
        }

      /* Если я здесь я оказался, значит, изменения есть.
       * Жду, пока не пройдет достаточное число времени после последнего
       * изменения. Тут специально стоит continue,  вдруг что-то поменяется,
       * пока поток спит. */
      if (g_timer_elapsed (priv->timer, NULL) < HYSCAN_SONAR_MODEL_TIMEOUT)
        {
          g_usleep (HYSCAN_SONAR_MODEL_TIMEOUT * G_USEC_PER_SEC);
          continue;
        }

      /* Переписываю новые значения во внутренний список. Новый очищаю. */
      g_mutex_lock (&priv->lock);
      g_atomic_int_set (&priv->changed, FALSE);
      hyscan_param_list_update (list, priv->incoming);
      hyscan_param_list_clear (priv->incoming);
      g_mutex_unlock (&priv->lock);

      /* Задаю новые значения, синхронизирую локатор. */
      hyscan_param_set (remote, list);
      hyscan_sonar_sync (HYSCAN_SONAR (remote));

      /* Очищаю список. */
      hyscan_param_list_clear (list);

      /* Считываю полное состояние. */
      hyscan_param_get (remote, full);

      /* Теперь копирую состояние реального HyScanParam в промежуточный,
       * а сверху накатываю свежие изменения. */
      g_mutex_lock (&priv->lock);
      hyscan_param_set (priv->fake, full);
      hyscan_param_set (priv->fake, priv->incoming); // todo: delete?
      g_mutex_unlock (&priv->lock);
    }

  g_clear_object (&remote);
  g_clear_object (&list);
  g_clear_object (&full);

  return NULL;
}

static gboolean
hyscan_sonar_model_param_set (HyScanParam     *param,
                              HyScanParamList *list)
{
  gboolean status;
  HyScanSonarModel *self = HYSCAN_SONAR_MODEL (param);
  HyScanSonarModelPrivate *priv = self->priv;

  g_mutex_lock (&priv->lock);

  /* Пробую задать в фейковый парам, чтобы проверить переданный ParamList. */
  status = hyscan_param_set (priv->fake, list);

  if (status)
    {
      hyscan_param_list_update (priv->incoming, list);
      g_signal_emit (self, hyscan_sonar_model_signals[SIGNAL_PARAM], 0);
    }

  /* Даже если не получилось, инициируем обновление. */
  hyscan_sonar_model_changed (priv);

  g_mutex_unlock (&priv->lock);

  return status;
}

static gboolean
hyscan_sonar_model_param_get (HyScanParam     *param,
                              HyScanParamList *list)
{
  HyScanSonarModelPrivate *priv = HYSCAN_SONAR_MODEL (param)->priv;

  return hyscan_param_get (priv->fake, list);
}


HyScanSonarModel *
hyscan_sonar_model_new (HyScanControl *control)
{
  return g_object_new (HYSCAN_TYPE_SONAR_MODEL,
                       "control", control,
                       NULL);
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
