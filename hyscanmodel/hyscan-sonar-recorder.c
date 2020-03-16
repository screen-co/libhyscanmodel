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
  gchar                   *suffix1;
  gchar                   *suffix;

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
  g_free (priv->suffix);
  g_free (priv->suffix1);

  hyscan_track_plan_free (priv->plan);

  g_object_unref (priv->sonar);
  g_object_unref (priv->db);

  G_OBJECT_CLASS (hyscan_sonar_recorder_parent_class)->finalize (object);
}

/* Генерирует имя галса по шаблону "<следующий номер в БД><суффикс><одноразовый суффикс>". */
static void
hyscan_sonar_recorder_generate_name (HyScanSonarRecorder *recorder)
{
  HyScanSonarRecorderPrivate *priv = recorder->priv;
  gint track_num;
  const gchar *suffix, *suffix1;

  gint32 project_id;
  gchar **tracks;
  gchar **strs;

  /* Ищем в текущем проекте галс с самым большим номером в названии. */
  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  tracks = project_id > 0 ? hyscan_db_track_list (priv->db, project_id) : NULL;

  track_num = 1;
  for (strs = tracks; strs != NULL && *strs != NULL; strs++)
    {
      gint number;

      number = g_ascii_strtoll (*strs, NULL, 10);
      if (number >= track_num)
        track_num = number + 1;
    }

  hyscan_db_close (priv->db, project_id);
  g_strfreev (tracks);

  suffix1 = priv->suffix1 != NULL ? priv->suffix1 : "";
  suffix = priv->suffix != NULL ? priv->suffix : "";

  g_free (priv->generated_name);
  priv->generated_name = g_strdup_printf ("%03d%s%s", track_num, suffix, suffix1);

  /* Удаляем одноразовый суффикс. */
  g_clear_pointer (&priv->suffix1, g_free);
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

/* Функция возвращает копию строки, в которой невалидные символы заменены транслитерацией или удалены. */
static gchar *
hyscan_sonar_recorder_name_normalize (const gchar *name)
{
  /* Правила транслитерации. */
  static struct
  {
    const gchar *from;
    const gchar *to;
  } translit[] = {{"а", "a"},   {"А", "A"},
                  {"б", "b"},   {"Б", "B"},
                  {"в", "v"},   {"В", "V"},
                  {"г", "g"},   {"Г", "G"},
                  {"д", "d"},   {"Д", "D"},
                  {"е", "e"},   {"Е", "E"},
                  {"ё", "yo"},  {"Ё", "Yo"},
                  {"ж", "zh"},  {"Ж", "Zh"},
                  {"з", "z"},   {"З", "Z"},
                  {"и", "i"},   {"И", "I"},
                  {"й", "y"},   {"Й", "Y"},
                  {"к", "k"},   {"К", "K"},
                  {"л", "l"},   {"Л", "L"},
                  {"м", "m"},   {"М", "M"},
                  {"н", "n"},   {"Н", "N"},
                  {"о", "o"},   {"О", "O"},
                  {"п", "p"},   {"П", "P"},
                  {"р", "r"},   {"Р", "R"},
                  {"с", "s"},   {"С", "S"},
                  {"т", "t"},   {"Т", "T"},
                  {"у", "u"},   {"У", "U"},
                  {"ф", "f"},   {"Ф", "F"},
                  {"х", "h"},   {"Х", "H"},
                  {"ц", "c"},   {"Ц", "C"},
                  {"ч", "ch"},  {"Ч", "Ch"},
                  {"ш", "sh"},  {"Ш", "Sh"},
                  {"щ", "sch"}, {"Щ", "Sch"},
                  {"ъ", ""},    {"Ъ", ""},
                  {"ы", "y"},   {"Ы", "Y"},
                  {"ь", ""},    {"Ь", ""},
                  {"э", "e"},   {"Э", "E"},
                  {"ю", "yu"},  {"Ю", "Yu"},
                  {"я", "ya"},  {"Я", "Ya"},
                  {" ", "_"}};
  guint map_n = G_N_ELEMENTS (translit);
  guint j;
  const gchar *utf8_char;
  GString *string;

  if (name == NULL || !g_utf8_validate (name, -1, NULL))
    return NULL;

  string = g_string_new (NULL);

  /* Цикл по UTF-8 символам строки. */
  for (utf8_char = name; *utf8_char != '\0'; utf8_char = g_utf8_next_char (utf8_char))
    {
      gchar c = *utf8_char;

      /* Валидные символы оставляем. См. hyscan_db_file_check_name(). */
      if ((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          (c == '-' || c == '_' || c == '.'))
        {
          g_string_append_c (string, c);
        }

      /* Русские буквы заменяем транслитом. */
      else
        {
          for (j = 0; j < map_n; j++)
            {
              if (!g_str_has_prefix (utf8_char, translit[j].from))
                continue;

              g_string_append (string, translit[j].to);
              break;
            }
        }
    }

  return g_string_free (string, FALSE);
}

/**
 * hyscan_sonar_recorder_set_suffix:
 * @recorder: указатель на #HyScanSonarRecorder
 * @suffix: (optional): суффикс имени галса
 * @one_time: %TRUE, суффикс применяется только к следующему старту; %FALSE, если ко всем последующим
 *
 * Функция устанавливает суффикс, который будет добавлен в конце автоматически сгенерированных имён галсов.
 * Недопустимые символы в строке будут транслитерированы или удалены.
 */
void
hyscan_sonar_recorder_set_suffix (HyScanSonarRecorder *recorder,
                                  const gchar         *suffix,
                                  gboolean             one_time)
{
  HyScanSonarRecorderPrivate *priv;
  gchar **suffix_field;

  g_return_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder));

  priv = recorder->priv;

  suffix_field = one_time ? &priv->suffix1 : &priv->suffix;
  g_free (*suffix_field);
  *suffix_field = hyscan_sonar_recorder_name_normalize (suffix);
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
gboolean
hyscan_sonar_recorder_start (HyScanSonarRecorder *recorder)
{

  HyScanSonarRecorderPrivate *priv;
  const gchar *track_name;

  g_return_val_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder), FALSE);

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

  return hyscan_sonar_start (priv->sonar, priv->project_name, track_name, priv->track_type, priv->plan);
}

/**
 * hyscan_sonar_recorder_stop:
 * @recorder: указатель на #HyScanSonarRecorder
 *
 * Функция останавливает работы гидролокатора.
 */
gboolean
hyscan_sonar_recorder_stop (HyScanSonarRecorder *recorder)
{
  HyScanSonarRecorderPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_SONAR_RECORDER (recorder), FALSE);

  priv = recorder->priv;

  return hyscan_sonar_stop (priv->sonar);
}
