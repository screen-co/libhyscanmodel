/* hyscan-sonar-recorder.c
 *
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
 * SECTION: hyscan-sonar-recorder
 * @Title HyScanSonarRecorder
 * @Short_description: класс настройки параметров старта гидролокатора
 *
 * Класс разделяет настройку параметров запуска гидролокатора и непосредственно его запуск.
 *
 * Параметров старта гидролокатора устанавливаются функциями:
 * - hyscan_sonar_recorder_set_project() - установка проекта,
 * - hyscan_sonar_recorder_set_name() - установка имени галса,
 * - hyscan_sonar_recorder_set_suffix() - установка суффикса для автоматически сгенерированных имён галсов,
 * - hyscan_sonar_recorder_set_plan() - установка плана галса,
 * - hyscan_sonar_recorder_set_type() - установка типа галса.
 *
 * Перевод гидролокатора в рабочий режим с указанными ранее параметрами производится функцией
 * hyscan_sonar_recorder_start().
 *
 * Получить используемый объект управления локатором можно при помощи функции hyscan_sonar_recoder_get_sonar().
 */

#include <hyscan-db.h>
#include "hyscan-sonar-recorder.h"

enum
{
  PROP_O,
  PROP_SONAR,
  PROP_DB,
};

struct _HyScanSonarRecorderPrivate
{
  HyScanSonar             *sonar;
  HyScanDB                *db;

  gchar                   *project_name;
  gchar                   *track_name;
  gchar                   *generated_name;
  gchar                   *track_name_suffix;

  HyScanTrackPlan         *plan;
  HyScanTrackType          track_type;
};

static void    hyscan_sonar_recorder_set_property             (GObject              *object,
                                                               guint                 prop_id,
                                                               const GValue         *value,
                                                               GParamSpec           *pspec);
static void    hyscan_sonar_recorder_object_constructed       (GObject              *object);
static void    hyscan_sonar_recorder_object_finalize          (GObject              *object);
static void    hyscan_sonar_recorder_generate_name            (HyScanSonarRecorder  *recorder);

G_DEFINE_TYPE_WITH_CODE (HyScanSonarRecorder, hyscan_sonar_recorder, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarRecorder))

static void
hyscan_sonar_recorder_class_init (HyScanSonarRecorderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_recorder_set_property;

  object_class->constructed = hyscan_sonar_recorder_object_constructed;
  object_class->finalize = hyscan_sonar_recorder_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "HyScanSonar", "HyScanSonar object", HYSCAN_TYPE_SONAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "HyScanDB", "HyScanDB object", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_sonar_recorder_init (HyScanSonarRecorder *sonar_recorder)
{
  sonar_recorder->priv = hyscan_sonar_recorder_get_instance_private (sonar_recorder);
}

static void
hyscan_sonar_recorder_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanSonarRecorder *sonar_recorder = HYSCAN_SONAR_RECORDER (object);
  HyScanSonarRecorderPrivate *priv = sonar_recorder->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      priv->sonar = g_value_dup_object (value);
      break;

    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_recorder_object_constructed (GObject *object)
{
  HyScanSonarRecorder *sonar_recorder = HYSCAN_SONAR_RECORDER (object);
  HyScanSonarRecorderPrivate *priv = sonar_recorder->priv;

  G_OBJECT_CLASS (hyscan_sonar_recorder_parent_class)->constructed (object);

  priv->track_type = HYSCAN_TRACK_SURVEY;
}

static void
hyscan_sonar_recorder_object_finalize (GObject *object)
{
  HyScanSonarRecorder *sonar_recorder = HYSCAN_SONAR_RECORDER (object);
  HyScanSonarRecorderPrivate *priv = sonar_recorder->priv;

  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->generated_name);
  g_free (priv->track_name_suffix);

  hyscan_track_plan_free (priv->plan);

  g_object_unref (priv->sonar);
  g_object_unref (priv->db);

  G_OBJECT_CLASS (hyscan_sonar_recorder_parent_class)->finalize (object);
}

/* Генерирует имя галса как следующий номер галса в БД. */
static void
hyscan_sonar_recorder_generate_name (HyScanSonarRecorder *recorder)
{
  HyScanSonarRecorderPrivate *priv = recorder->priv;
  gint track_num = 1;
  const gchar *suffix;

  /* Число галсов в проекте. */
  gint number;
  gint32 project_id;
  gchar **tracks;
  gchar **strs;

  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  tracks = project_id > 0 ? hyscan_db_track_list (priv->db, project_id) : NULL;

  for (strs = tracks; strs != NULL && *strs != NULL; strs++)
    {
      /* Ищем галс с самым большим номером в названии. */
      number = g_ascii_strtoll (*strs, NULL, 10);
      if (number >= track_num)
        track_num = number + 1;
    }

  hyscan_db_close (priv->db, project_id);
  g_strfreev (tracks);

  suffix = priv->track_name_suffix != NULL ? priv->track_name_suffix : "";

  g_free (priv->generated_name);
  priv->generated_name = g_strdup_printf ("%d%s", track_num, suffix);
}

/**
 * hyscan_sonar_recorder_new:
 * @sonar: интерфейс управления гидролокатором #HyScanSonar
 *
 * Создаёт объект управления гидролокатором #HyScanSonarRecorder
 *
 * Returns: новый объект #HyScanSonarRecorder, для удаления g_object_unref().
 */
HyScanSonarRecorder *
hyscan_sonar_recorder_new (HyScanSonar *sonar,
                           HyScanDB    *db)
{
  return g_object_new (HYSCAN_TYPE_SONAR_RECORDER,
                       "sonar", sonar,
                       "db", db,
                       NULL);
}

/**
 * hyscan_sonar_recorder_get_sonar:
 * @recorder: указатель на #HyScanSonarRecorder
 *
 * Функция получает объект управления гидролокатором.
 *
 * Returns: (transfer full): объект управления гидролокатором, для удаления g_object_unref().
 */
HyScanSonar *
hyscan_sonar_recorder_get_sonar (HyScanSonarRecorder *recorder)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder), NULL);

  priv = recorder->priv;

  return g_object_ref (priv->sonar);
}

/**
 * hyscan_sonar_recorder_set_suffix:
 * @recorder: указатель на #HyScanSonarRecorder
 * @suffix: (optional): суффикс имени галса
 *
 * Функция устанавливает суффикс, который будет добавлен в конце автоматически сгенерированных имён галсов.
 */
void
hyscan_sonar_recorder_set_suffix (HyScanSonarRecorder *recorder,
                                  const gchar         *suffix)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  g_free (priv->track_name_suffix);
  priv->track_name_suffix = g_strdup (suffix);
}

/**
 * hyscan_sonar_recorder_set_plan:
 * @recorder: указатель на #HyScanSonarRecoder
 * @plan: (optional): план галса
 *
 * Функция устанавливает план галса
 */
void
hyscan_sonar_recorder_set_plan (HyScanSonarRecorder *recorder,
                                HyScanTrackPlan     *plan)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  hyscan_track_plan_free (priv->plan);
  priv->plan = hyscan_track_plan_copy (plan);
}

/**
 * hyscan_sonar_recorder_set_project:
 * @recorder: указатель на #HyScanSonarRecoder
 * @project: имя проекта
 *
 * Функция устанавливает имя проекта
 */
void
hyscan_sonar_recorder_set_project (HyScanSonarRecorder *recorder,
                                   const gchar         *project_name)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  g_free (priv->project_name);
  priv->project_name = g_strdup (project_name);
}

/**
 * hyscan_sonar_recorder_set_track_name:
 * @recorder: указатель на #HyScanSonarRecoder
 * @track_name: (optional): имя галса
 *
 * Функция устанавливает имя галса. Если @track_name = %NULL, то имя галса будет сгенерировано автоматически.
 */
void
hyscan_sonar_recorder_set_track_name (HyScanSonarRecorder *recorder,
                                      const gchar         *track_name)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  g_free (priv->track_name);
  priv->track_name = g_strdup (track_name);
}

/**
 * hyscan_sonar_recorder_set_track_type:
 * @recorder: указатель на #HyScanSonarRecoder
 * @track_type: тип галса #HyScanTrackType
 *
 * Функция устанавливает тип галса.
 */
void
hyscan_sonar_recorder_set_track_type (HyScanSonarRecorder *recorder,
                                      HyScanTrackType      track_type)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  priv->track_type = track_type;
}

/**
 * hyscan_sonar_recorder_start:
 * @recorder: указатель на #HyScanSonarRecoder
 *
 * Функция запускает работу гидролокатора с указанными ранее параметрами.
 */
void
hyscan_sonar_recorder_start (HyScanSonarRecorder *recorder)
{

  HyScanSonarRecorderPrivate *priv;
  const gchar *track_name;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;
  if (priv->track_name == NULL)
    {
      hyscan_sonar_recorder_generate_name (recorder);
      track_name = priv->generated_name;
    }
  else
    {
      track_name = priv->track_name;
    }

  hyscan_sonar_start (priv->sonar, priv->project_name, track_name, priv->track_type, priv->plan);
}

/**
 * hyscan_sonar_recorder_stop:
 * @recorder: указатель на #HyScanSonarRecorder
 *
 * Функция останавливает работы гидролокатора.
 */
void
hyscan_sonar_recorder_stop (HyScanSonarRecorder *recorder)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  hyscan_sonar_stop (priv->sonar);
}
