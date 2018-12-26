/* hyscan-db-info.c
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * SECTION: hyscan-db-info
 * @Short_description: класс асинхронного мониторинга базы данных
 * @Title: HyScanDBInfo
 *
 * Класс предназначен для асинхронного отслеживания изменений в базе данных и
 * получения информации о проектах, галсах и источниках данных в них. Отправка
 * уведомлений об изменениях производится из основного цикла GMainLoop, таким
 * образом для корректной работы класса требуется вызвать функции
 * g_main_loop_run или gtk_main.
 *
 * Создание объекта производится с помощью функции #hyscan_db_info_new.
 *
 * Основные функции класса работают в неблокирующем режиме, однако возможна
 * ситуация когда список проектов и галсов ещё не полностью считан из базы
 * данных. Рекомендуется получать списки проектов и галсов при получении
 * сигналов об изменениях.
 *
 * Получить текущий список проектов можно при помощи функции
 * #hyscan_db_info_get_projects. Функции #hyscan_db_info_set_project и
 * #hyscan_db_info_get_project используются для установки и чтения названия
 * текущего проекта, для которого отслеживаются изменения. Получить список
 * галсов можно при помощи функции #hyscan_db_info_get_tracks.
 *
 * Функция #hyscan_db_info_refresh используется для принудительного обновления
 * списка проектов, галсов и информации о них.
 *
 * Вспомогательные функции #hyscan_db_info_get_project_info и
 * #hyscan_db_info_get_track_info используются для получения информации о
 * проекте и галсе. Для их работы не требуется создавать экземпляр класса и они
 * блокируют выполнение программы на время работы с базой данных. Эти функции
 * могут быть полезны при создании программ не предъявляющих жёстких требований
 * к отзывчивости.
 */

#include "hyscan-db-info.h"
#include <hyscan-core-schemas.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_DB
};

enum
{
  SIGNAL_PROJECTS_CHANGED,
  SIGNAL_TRACKS_CHANGED,
  SIGNAL_LAST
};

typedef struct
{
  HyScanDB                    *db;                     /* Интерфейс базы данных. */
  gint32                       id;                     /* Идентификатор открытого объекта. */
  guint32                      mod_counter;            /* Счётчик изменений. */
} HyScanDBInfoMonitor;

struct _HyScanDBInfoPrivate
{
  HyScanDB                    *db;                     /* Интерфейс базы данных. */

  guint32                      projects_mod_counter;   /* Текущий счётчик изменений в проектах. */
  gboolean                     projects_update;        /* Признак изменения списка проектов. */
  GHashTable                  *projects;               /* Список проектов. */

  gchar                       *new_project_name;       /* Название нового проекта для слежения. */
  gchar                       *project_name;           /* Название текущего проекта для слежения. */
  gint32                       project_id;             /* Идентификатор проекта для слежения. */

  guint32                      tracks_mod_counter;     /* Текущий счётчик изменений в галсах. */
  gboolean                     tracks_update;          /* Признак изменения числа галсов. */
  GHashTable                  *tracks;                 /* Списк галсов. */

  GHashTable                  *actives;                /* Списк "активных" объектов базы данных. */
  gboolean                     refresh;                /* Признак необходимости обновления списков. */

  guint                        alerter;                /* Идентификатор обработчика сигнализирующего об изменениях. */
  GThread                     *informer;               /* Поток отслеживания изменений. */
  gint                         shutdown;               /* Признак завершения работы. */

  GMutex                       lock;                   /* Блокировка. */
};

static void        hyscan_db_info_set_property         (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void        hyscan_db_info_object_constructed   (GObject               *object);
static void        hyscan_db_info_object_finalize      (GObject               *object);

static void        hyscan_db_info_free_monitor         (gpointer               data);

static HyScanTrackInfo *
                   hyscan_db_info_get_track_info_int   (HyScanDB              *db,
                                                        gint32                 project_id,
                                                        const gchar           *track_name,
                                                        GHashTable            *actives);

static void        hyscan_db_info_add_active_id        (GHashTable            *actives,
                                                        HyScanDB              *db,
                                                        gint32                 id,
                                                        guint32                mod_counter);

static gboolean    hyscan_db_info_alerter              (gpointer               data);

static gpointer    hyscan_db_info_informer             (gpointer               data);

static guint       hyscan_db_info_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDBInfo, hyscan_db_info, G_TYPE_OBJECT)

static void
hyscan_db_info_class_init (HyScanDBInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_db_info_set_property;

  object_class->constructed = hyscan_db_info_object_constructed;
  object_class->finalize = hyscan_db_info_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScan DB", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanDBInfo::projects-changed:
   * @info: указатель на #HyScanDBInfo
   *
   * Сигнал посылается при изменении списка проектов.
   */
  hyscan_db_info_signals[SIGNAL_PROJECTS_CHANGED] =
    g_signal_new ("projects-changed", HYSCAN_TYPE_DB_INFO, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * HyScanDBInfo::tracks-changed:
   * @info: указатель на #HyScanDBInfo
   *
   * Сигнал посылается при изменении списка галсов.
   */
  hyscan_db_info_signals[SIGNAL_TRACKS_CHANGED] =
    g_signal_new ("tracks-changed", HYSCAN_TYPE_DB_INFO, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_db_info_init (HyScanDBInfo *info)
{
  info->priv = hyscan_db_info_get_instance_private (info);
}

static void
hyscan_db_info_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanDBInfo *info = HYSCAN_DB_INFO (object);
  HyScanDBInfoPrivate *priv = info->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_db_info_object_constructed (GObject *object)
{
  HyScanDBInfo *info = HYSCAN_DB_INFO (object);
  HyScanDBInfoPrivate *priv = info->priv;

  priv->project_id = -1;

  g_mutex_init (&priv->lock);

  if (priv->db == NULL)
    return;

  priv->actives = g_hash_table_new_full (NULL, NULL, NULL, hyscan_db_info_free_monitor);
  priv->alerter = g_timeout_add (1000, hyscan_db_info_alerter, info);
  priv->informer = g_thread_new ("db-informer", hyscan_db_info_informer, priv);
}

static void
hyscan_db_info_object_finalize (GObject *object)
{
  HyScanDBInfo *info = HYSCAN_DB_INFO (object);
  HyScanDBInfoPrivate *priv = info->priv;

  if (priv->alerter > 0)
    g_source_remove (priv->alerter);

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_clear_pointer (&priv->informer, g_thread_join);

  g_clear_pointer (&priv->projects, g_hash_table_unref);
  g_clear_pointer (&priv->tracks, g_hash_table_unref);
  g_clear_pointer (&priv->actives, g_hash_table_unref);

  g_free (priv->new_project_name);
  g_free (priv->project_name);

  g_mutex_clear (&priv->lock);
  g_clear_object (&priv->db);

  G_OBJECT_CLASS (hyscan_db_info_parent_class)->finalize (object);
}

/* Функция освобождает память занятую структурой HyScanDBInfoMonitor. */
static void
hyscan_db_info_free_monitor (gpointer data)
{
  HyScanDBInfoMonitor *info = data;

  hyscan_db_close (info->db, info->id);
  g_free (info);
}

/* Функция добавляет идентификатор объекта базы данных в список активных. */
static void
hyscan_db_info_add_active_id (GHashTable *actives,
                              HyScanDB   *db,
                              gint32      id,
                              guint32     mod_counter)
{
  HyScanDBInfoMonitor *info;

  if (actives == NULL)
    return;

  info = g_new0 (HyScanDBInfoMonitor, 1);
  info->db = db;
  info->id = id;
  info->mod_counter = mod_counter;

  g_hash_table_insert (actives, GINT_TO_POINTER (id), info);
}

/* Функция сигнализатор об изменениях в базе данных. */
static gboolean
hyscan_db_info_alerter (gpointer data)
{
  HyScanDBInfo *info = data;
  HyScanDBInfoPrivate *priv = info->priv;

  gboolean projects_update;
  gboolean tracks_update;

  g_mutex_lock (&priv->lock);
  projects_update = priv->projects_update;
  tracks_update = priv->tracks_update;
  priv->projects_update = FALSE;
  priv->tracks_update = FALSE;
  g_mutex_unlock (&priv->lock);

  if (projects_update)
    g_signal_emit (info, hyscan_db_info_signals[SIGNAL_PROJECTS_CHANGED], 0);

  if (tracks_update)
    g_signal_emit (info, hyscan_db_info_signals[SIGNAL_TRACKS_CHANGED], 0);

  return TRUE;
}

/* Поток проверяющий состояние базы данных и собирающий информацию о проектах, галсах и источниках данных. */
static gpointer
hyscan_db_info_informer (gpointer data)
{
  HyScanDBInfoPrivate *priv = data;
  GTimer *check_timer;

  /* Таймер проверки изменений. */
  check_timer = g_timer_new ();

  /* Начальное значение счётчика изменений списка проектов. */
  priv->projects_mod_counter = hyscan_db_get_mod_count (priv->db, 0) - 1;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      guint32 mod_counter;
      gchar *project_name;
      GHashTableIter iter;
      gpointer key, value;
      gboolean check_projects;
      gboolean check_tracks;

      /* Задержка между проверками базы данных, а также завершением работы. */
      g_usleep (100000);

      /* Проверка в обычном режиме - 1 раз в секунду. */
      if (g_timer_elapsed (check_timer, NULL) < 1.0)
        continue;

      g_timer_start (check_timer);

      /* Принудительное обновление. */
      if (g_atomic_int_compare_and_exchange (&priv->refresh, TRUE, FALSE))
        {
          check_projects = TRUE;
          check_tracks = TRUE;
        }
      else
        {
          check_projects = FALSE;
          check_tracks = FALSE;
        }

      /* Проверяем изменение списка проектов при наличии изменений. */
      mod_counter = hyscan_db_get_mod_count (priv->db, 0);
      if (check_projects || (mod_counter != priv->projects_mod_counter))
        {
          GHashTable *projects;
          gchar **project_list;
          guint n_projects;
          guint i;

          /* Список проектов и их число. */
          project_list = hyscan_db_project_list (priv->db);
          n_projects = (project_list == NULL) ? 0 : g_strv_length (project_list);

          /* Информация о проектах. */
          projects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, (GDestroyNotify)hyscan_db_info_project_info_free);
          for (i = 0; i < n_projects; i++)
            {
              HyScanProjectInfo *info = NULL;

              if (priv->projects != NULL)
                info = g_hash_table_lookup (priv->projects, project_list[i]);

              /* Если информацию о проекте уже считывали ранее, то используем эту информацию. */
              if (info == NULL)
                info = hyscan_db_info_get_project_info (priv->db, project_list[i]);
              else
                info = hyscan_db_info_project_info_copy (info);

              if (info != NULL)
                g_hash_table_insert (projects, g_strdup (project_list[i]), info);
            }

          /* Удаляем старую информацию о проектах, запоминаем новую,
           * сигнализируем об изменениях. */
          g_mutex_lock (&priv->lock);
          g_clear_pointer (&priv->projects, g_hash_table_unref);
          priv->projects = projects;
          priv->projects_update = TRUE;
          g_mutex_unlock (&priv->lock);

          priv->projects_mod_counter = mod_counter;
          g_strfreev (project_list);
        }

      /* Название проекта для отслеживания. */
      g_mutex_lock (&priv->lock);
      project_name = g_strdup (priv->new_project_name);
      g_mutex_unlock (&priv->lock);

      /* Открываем проект который будем отслеживать. */
      if (project_name != NULL)
        {
          /* Закрываем предыдущий проект. */
          if (priv->project_id > 0)
            hyscan_db_close (priv->db, priv->project_id);

          /* Открываем проект. */
          if (hyscan_db_is_exist (priv->db, project_name, NULL, NULL))
            priv->project_id = hyscan_db_project_open (priv->db, project_name);
          else
            priv->project_id = -1;

          /* Если проект открыли, очищаем глобальную переменную с именем нового проекта,
           * запоминаем имя текущего проекта и очищаем список галсов. */
          if (priv->project_id > 0)
            {
              priv->tracks_mod_counter = hyscan_db_get_mod_count (priv->db, priv->project_id) - 1;

              g_mutex_lock (&priv->lock);
              if (g_strcmp0 (priv->new_project_name, project_name) == 0)
                {
                  g_clear_pointer (&priv->project_name, g_free);
                  priv->project_name = priv->new_project_name;
                  priv->new_project_name = NULL;
                }

              g_clear_pointer (&priv->tracks, g_hash_table_unref);
              priv->tracks_update = TRUE;
              g_mutex_unlock (&priv->lock);
            }

          g_free (project_name);
        }

      /* Нет проекта для отслеживания. */
      if (priv->project_id <= 0)
        continue;

      /* Проверяем изменения в активных объектах базы данных. */
      g_hash_table_iter_init (&iter, priv->actives);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanDBInfoMonitor *info = value;

          mod_counter = hyscan_db_get_mod_count (priv->db, info->id);
          if (mod_counter != info->mod_counter)
            {
              check_tracks = TRUE;
              break;
            }
        }

      /* Проверяем изменение списка галсов. */
      mod_counter = hyscan_db_get_mod_count (priv->db, priv->project_id);
      if (check_tracks || (mod_counter != priv->tracks_mod_counter))
        {
          GHashTable *tracks;
          gchar **track_list;
          guint n_tracks;
          guint i;

          /* Очищаем список "активных" объектов базы. */
          g_hash_table_remove_all (priv->actives);

          /* Список галсов и их число. */
          track_list = hyscan_db_track_list (priv->db, priv->project_id);
          n_tracks = (track_list == NULL) ? 0 : g_strv_length (track_list);

          /* Информация о галсах. */
          tracks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, (GDestroyNotify)hyscan_db_info_track_info_free);
          for (i = 0; i < n_tracks; i++)
            {
              HyScanTrackInfo *info = NULL;

              if (priv->tracks != NULL)
                info = g_hash_table_lookup (priv->tracks, track_list[i]);

              /* Если информацию о галсе уже считывали ранее и галс не является активным,
               * то используем эту информацию. Иначе считываем информацию из базы. */
              if ((info != NULL) && !info->active)
                {
                  info = hyscan_db_info_track_info_copy (info);
                }
              else
                {
                  info = hyscan_db_info_get_track_info_int (priv->db, priv->project_id,
                                                            track_list[i], priv->actives);
                }

              if (info != NULL)
                g_hash_table_insert (tracks, g_strdup (track_list[i]), info);
            }

          /* Удаляем старую информацию о галсах, запоминаем новую,
           * сигнализируем об изменениях. */
          g_mutex_lock (&priv->lock);
          g_clear_pointer (&priv->tracks, g_hash_table_unref);
          priv->tracks = tracks;
          priv->tracks_update = TRUE;
          g_mutex_unlock (&priv->lock);

          priv->tracks_mod_counter = mod_counter;
          g_strfreev (track_list);
        }
    }

  g_timer_destroy (check_timer);

  return NULL;
}

/**
 * hyscan_db_info_new:
 * @db: указатель на #HyScanDB
 *
 * Функция создаёт новый объект #HyScanDBInfo.
 *
 * Returns: #HyScanDBInfo. Для удаления #g_object_unref.
 */
HyScanDBInfo *
hyscan_db_info_new (HyScanDB *db)
{
  HyScanDBInfo *info;

  info = g_object_new (HYSCAN_TYPE_DB_INFO,
                       "db", db,
                       NULL);

  if (info->priv->db == NULL)
    g_clear_object (&info);

  return info;
}

/**
 * hyscan_db_info_set_project:
 * @info: указатель на #HyScanDBInfo
 * @project_name: название проекта
 *
 * Функция устанавливает название проекта для которого будут отслеживаться
 * изменения в списке галсов и источниках данных. Реальное изменение
 * отслеживаемого проекта может происходить с некоторой задержкой.
 */
void
hyscan_db_info_set_project (HyScanDBInfo *info,
                            const gchar  *project_name)
{
  HyScanDBInfoPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DB_INFO (info));

  priv = info->priv;

  g_mutex_lock (&priv->lock);
  if (g_strcmp0 (priv->project_name, project_name) != 0)
    {
      g_clear_pointer (&priv->new_project_name, g_free);
      priv->new_project_name = g_strdup (project_name);
    }
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_db_info_get_project:
 * @info: указатель на #HyScanDBInfo
 *
 * Функция считывает название проекта для которого в данный момент
 * отслеживаются изменения.
 *
 * Returns: (nullable): Название проекта или %NULL. Для удаления #g_free.
 */
gchar *
hyscan_db_info_get_project (HyScanDBInfo *info)
{
  HyScanDBInfoPrivate *priv;
  gchar *project_name;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (info), NULL);

  priv = info->priv;

  g_mutex_lock (&priv->lock);
  project_name = g_strdup (priv->project_name);
  g_mutex_unlock (&priv->lock);

  return project_name;
}

/**
 * hyscan_db_info_get_projects:
 * @info: указатель на #HyScanDBInfo
 *
 * Функция возвращает список проектов и информацию о них. Информация
 * представлена в виде хэш таблицы в которой ключом является название
 * проекта, а значением указатель на структуру #HyScanProjectInfo.
 *
 * Returns: (transfer full) (element-type HyScanProjectInfo):
 *          Текущий список проектов. Для удаления #g_hash_table_unref.
 */
GHashTable *
hyscan_db_info_get_projects (HyScanDBInfo *info)
{
  HyScanDBInfoPrivate *priv;
  GHashTable *projects;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (info), NULL);

  priv = info->priv;

  projects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                    (GDestroyNotify)hyscan_db_info_project_info_free);

  g_mutex_lock (&priv->lock);
  if (priv->projects != NULL)
    {
      g_hash_table_iter_init (&iter, priv->projects);
      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (projects, g_strdup (key), hyscan_db_info_project_info_copy (value));
    }
  g_mutex_unlock (&priv->lock);

  return projects;
}

/**
 * hyscan_db_info_get_tracks:
 * @info: указатель на #HyScanDBInfo
 *
 * Функция возвращает список галсов и информацию о них для текущего
 * отслеживаемого проекта. Информация представлена в виде хэш таблицы в
 * которой ключом является название галса, а значением указатель на
 * структуру #HyScanTrackInfo.
 *
 * Returns: (transfer full) (element-type HyScanTrackInfo):
 *          Текущий список галсов. Для удаления #g_hash_table_unref.
 */
GHashTable *
hyscan_db_info_get_tracks (HyScanDBInfo *info)
{
  HyScanDBInfoPrivate *priv;
  GHashTable *tracks;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (info), NULL);

  priv = info->priv;

  tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                  (GDestroyNotify)hyscan_db_info_track_info_free);

  g_mutex_lock (&priv->lock);
  if (priv->tracks != NULL)
    {
      g_hash_table_iter_init (&iter, priv->tracks);
      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (tracks, g_strdup (key), hyscan_db_info_track_info_copy (value));
    }
  g_mutex_unlock (&priv->lock);

  return tracks;
}

/**
 * hyscan_db_info_refresh:
 * @info: указатель на #HyScanDBInfo
 *
 * Функция принудительно запускает процесс обновления списков проектов,
 * галсов и информации о них.
 */
void
hyscan_db_info_refresh (HyScanDBInfo *info)
{
  g_return_if_fail (HYSCAN_IS_DB_INFO (info));

  g_atomic_int_set (&info->priv->refresh, TRUE);
}

/**
 * hyscan_db_info_get_project_info:
 * @db: указатель на #HyScanDB
 * @project_name: название проекта
 *
 * Функция возвращает информацию о проекте.
 *
 * Returns: (nullable): (transfer full): Информация о проекте или %NULL
 *          Для удаления #hyscan_db_info_project_info_free.
 */
HyScanProjectInfo *
hyscan_db_info_get_project_info (HyScanDB    *db,
                                 const gchar *project_name)
{
  HyScanProjectInfo *info;
  gint32 project_id;
  gint32 param_id;

  project_id = hyscan_db_project_open (db, project_name);
  if (project_id <= 0)
    return NULL;

  info = g_slice_new0 (HyScanProjectInfo);
  info->name = g_strdup (project_name);

  param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
  if (param_id > 0)
    {
      HyScanParamList *list = hyscan_param_list_new ();

      hyscan_param_list_add (list, "/schema/id");
      hyscan_param_list_add (list, "/schema/version");
      hyscan_param_list_add (list, "/description");
      hyscan_param_list_add (list, "/ctime");
      hyscan_param_list_add (list, "/mtime");

      if ((hyscan_db_param_get (db, param_id, PROJECT_INFO_OBJECT, list)) &&
          (hyscan_param_list_get_integer (list, "/schema/id") == PROJECT_INFO_SCHEMA_ID) &&
          (hyscan_param_list_get_integer (list, "/schema/version") == PROJECT_INFO_SCHEMA_VERSION))
        {
          gint64 ctime = hyscan_param_list_get_integer (list, "/ctime") / G_USEC_PER_SEC;
          gint64 mtime = hyscan_param_list_get_integer (list, "/mtime") / G_USEC_PER_SEC;

          info->description = hyscan_param_list_dup_string (list, "/description");
          info->ctime = g_date_time_new_from_unix_utc (ctime);
          info->mtime = g_date_time_new_from_unix_utc (mtime);
        }
      else
        {
          info->error = TRUE;
        }

      hyscan_db_close (db, param_id);
      g_object_unref (list);
    }

  hyscan_db_close (db, project_id);

  return info;
}

/* Функция возвращает информацию о галсе и заполняет таблицу "активных" объектов. */
static HyScanTrackInfo *
hyscan_db_info_get_track_info_int (HyScanDB    *db,
                                   gint32       project_id,
                                   const gchar *track_name,
                                   GHashTable  *actives)
{
  HyScanTrackInfo *info;
  HyScanParamList *list;
  gchar **channels;
  gint32 track_id;
  gint32 param_id;
  gint32 channel_id;
  guint32 track_mod_counter;
  guint n_sources = 0;
  guint i;

  /* Открываем галс. */
  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id <= 0)
    return NULL;

  /* Счётчик изменений в галсе. */
  track_mod_counter = hyscan_db_get_mod_count (db, track_id);

  /* Не обрабатываем пустые галсы, а добавляем их в список проверяемых. */
  channels = hyscan_db_channel_list (db, track_id);
  if (channels == NULL)
    {
      hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter);
      return NULL;
    }

  list = hyscan_param_list_new ();

  info = g_slice_new0 (HyScanTrackInfo);
  info->name = g_strdup (track_name);

  /* Информация о галсе из галса. */
  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id > 0)
    {
      hyscan_param_list_add (list, "/schema/id");
      hyscan_param_list_add (list, "/schema/version");
      hyscan_param_list_add (list, "/id");
      hyscan_param_list_add (list, "/ctime");
      hyscan_param_list_add (list, "/type");
      hyscan_param_list_add (list, "/operator");
      hyscan_param_list_add (list, "/sonar");

      if ((hyscan_db_param_get (db, param_id, NULL, list)) &&
          (hyscan_param_list_get_integer (list, "/schema/id") == TRACK_SCHEMA_ID) &&
          (hyscan_param_list_get_integer (list, "/schema/version") == TRACK_SCHEMA_VERSION))
        {
          const gchar *track_type = hyscan_param_list_get_string (list, "/type");
          const gchar *sonar_info = hyscan_param_list_get_string (list, "/sonar");
          gint64 ctime = hyscan_param_list_get_integer (list, "/ctime") / G_USEC_PER_SEC;

          info->id = hyscan_param_list_dup_string (list, "/id");
          info->ctime = g_date_time_new_from_unix_utc (ctime);
          info->type = hyscan_track_get_type_by_name (track_type);
          info->operator_name = hyscan_param_list_dup_string (list, "/operator");
          info->sonar_info = hyscan_data_schema_new_from_string (sonar_info, "info");
        }
      else
        {
          info->error = TRUE;
        }

      hyscan_db_close (db, param_id);
    }

  /* Информация о галсе из проекта. */
  param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
  if (param_id > 0)
    {
      hyscan_param_list_clear (list);
      hyscan_param_list_add (list, "/schema/id");
      hyscan_param_list_add (list, "/schema/version");
      hyscan_param_list_add (list, "/mtime");
      hyscan_param_list_add (list, "/description");

      if ((hyscan_db_param_get (db, param_id, track_name, list)) &&
          (hyscan_param_list_get_integer (list, "/schema/id") == TRACK_INFO_SCHEMA_ID) &&
          (hyscan_param_list_get_integer (list, "/schema/version") == TRACK_INFO_SCHEMA_VERSION))
        {
          gint64 mtime = hyscan_param_list_get_integer (list, "/mtime") / G_USEC_PER_SEC;

          info->mtime = g_date_time_new_from_unix_utc (mtime);
          info->description = hyscan_param_list_dup_string (list, "/description");
        }
      else
        {
          info->error = TRUE;
        }

      hyscan_db_close (db, param_id);
    }

  g_object_unref (list);

  /* Список каналов данных. */
  for (i = 0; channels[i] != NULL; i++)
    {
      HyScanSourceType source;
      HyScanChannelType type;
      guint channel;
      gboolean active;

      /* Проверяем типы данных. */
      if (!hyscan_channel_get_types_by_name (channels[i], &source, &type, &channel))
        continue;

      /* Учитываем только первые каналы с данными. */
      if ((type != HYSCAN_CHANNEL_DATA) || (channel != 1))
        continue;

      /* Пытаемся узнать есть ли в нём записанные данные. */
      channel_id = hyscan_db_channel_open (db, track_id, channels[i]);
      if (channel_id <= 0)
        continue;

      active = hyscan_db_channel_is_writable (db, channel_id);

      /* В канале есть минимум одна запись. */
      if (hyscan_db_channel_get_data_range (db, channel_id, NULL, NULL))
        {
          info->sources[source] = TRUE;
          n_sources += 1;

          if (active)
            info->active = TRUE;
        }

      /* Ставим на карандаш, если канал открыт на запись. */
      else if (active)
        {
          guint32 channel_mod_counter = hyscan_db_get_mod_count (db, channel_id);
          hyscan_db_info_add_active_id (actives, db, channel_id, channel_mod_counter);
        }

      hyscan_db_close (db, channel_id);
    }

  g_strfreev (channels);

  /* Если в галсе нет каналов, пропускаем его и добавляем в список проверяемых. */
  if (n_sources == 0)
    {
      hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter);
      hyscan_db_info_track_info_free (info);

      return NULL;
    }

  /* Если в галсе есть открытый для записи канал, проверим этот галс позже. */
  if (info->active)
    hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter);
  else
    hyscan_db_close (db, track_id);

  return info;
}

/**
 * hyscan_db_info_get_track_info:
 * @db: указатель на #HyScanDB
 * @project_id: идентификатор открытого проекта
 * @track_name: название галса
 *
 * Функция возвращает информацию о галсе.
 *
 * Returns: (nullable): (transfer full): Информация о галсе или %NULL
 *          Для удаления #hyscan_db_info_track_info_free.
 */
HyScanTrackInfo *
hyscan_db_info_get_track_info (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name)
{
  return hyscan_db_info_get_track_info_int (db, project_id, track_name, NULL);
}

/**
 * hyscan_db_info_project_info_copy:
 * @info: структура #HyScanProjectInfo для копирования
 *
 * Функция создаёт копию структуры #HyScanProjectInfo.
 *
 * Returns: (transfer full): Новая структура #HyScanProjectInfo.
 * Для удаления #hyscan_db_info_project_info_free.
 */
HyScanProjectInfo *
hyscan_db_info_project_info_copy (HyScanProjectInfo *info)
{
  HyScanProjectInfo *new_info;

  new_info = g_slice_new0 (HyScanProjectInfo);
  new_info->name = g_strdup (info->name);
  new_info->ctime = g_date_time_ref (info->ctime);
  new_info->mtime = g_date_time_ref (info->mtime);
  new_info->description = g_strdup (info->description);

  return new_info;
}

/**
 * hyscan_db_info_project_info_free:
 * @info: структура #HyScanProjectInfo для удаления
 *
 * Функция удаляет структуру #HyScanProjectInfo.
 */
void
hyscan_db_info_project_info_free (HyScanProjectInfo *info)
{
  g_free ((gchar *)info->name);
  g_free ((gchar *)info->description);
  g_clear_pointer (&info->ctime, g_date_time_unref);
  g_clear_pointer (&info->mtime, g_date_time_unref);

  g_slice_free (HyScanProjectInfo, info);
}

/**
 * hyscan_db_info_track_info_copy:
 * @info: структура #HyScanTrackInfo для копирования
 *
 * Функция создаёт копию структуры #HyScanTrackInfo.
 *
 * Returns: (transfer full): Новая структура #HyScanTrackInfo.
 * Для удаления #hyscan_db_info_track_info_free.
 */
HyScanTrackInfo *
hyscan_db_info_track_info_copy (HyScanTrackInfo *info)
{
  HyScanTrackInfo *new_info;

  new_info = g_slice_new0 (HyScanTrackInfo);
  new_info->id = g_strdup (info->id);
  new_info->type = info->type;
  new_info->name = g_strdup (info->name);
  if (info->ctime != NULL)
    new_info->ctime = g_date_time_ref (info->ctime);
  if (info->mtime != NULL)
    new_info->mtime = g_date_time_ref (info->mtime);
  new_info->description = g_strdup (info->description);
  new_info->operator_name = g_strdup (info->operator_name);
  if (info->sonar_info != NULL)
    new_info->sonar_info = g_object_ref (info->sonar_info);
  memcpy (new_info->sources, info->sources, sizeof (info->sources));
  new_info->active = info->active;

  return new_info;
}

/**
 * hyscan_db_info_track_info_free:
 * @info: структура #HyScanTrackInfo для удаления
 *
 * Функция удаляет структуру #HyScanTrackInfo.
 */
void
hyscan_db_info_track_info_free (HyScanTrackInfo *info)
{
  g_free ((gchar*)info->id);
  g_free ((gchar*)info->name);
  g_free ((gchar*)info->description);
  g_free ((gchar*)info->operator_name);
  g_clear_pointer (&info->ctime, g_date_time_unref);
  g_clear_pointer (&info->mtime, g_date_time_unref);
  g_clear_object (&info->sonar_info);

  g_slice_free (HyScanTrackInfo, info);
}
