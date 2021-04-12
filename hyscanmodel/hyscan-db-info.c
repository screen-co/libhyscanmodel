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

#define HYSCAN_DB_INFO_TIMEOUT      1000    /* Задержка между сигналами об изменениях в базе данных, милисекунды. */
#define HYSCAN_DB_INFO_COND_TIMEOUT 100000  /* Задержка между проверками базы данных, микросекунды. */

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
  gboolean                     writeable;              /* Признак доступа к каналу на запись. */
} HyScanDBInfoMonitor;

struct _HyScanDBInfoPrivate
{
  HyScanDB                    *db;                     /* Интерфейс базы данных. */

  gboolean                     projects_update;        /* Признак изменения списка проектов. */
  GHashTable                  *projects;               /* Список проектов. */

  gchar                       *new_project_name;       /* Название нового проекта для слежения. */
  gboolean                     project_changed;        /* Флаг смены проекта. */
  gchar                       *project_name;           /* Название текущего проекта для слежения. */
  gint32                       project_id;             /* Идентификатор проекта для слежения. */

  gboolean                     tracks_update;          /* Признак изменения числа галсов. */
  GHashTable                  *tracks;                 /* Списк галсов. */

  GHashTable                  *actives;                /* Списк "активных" объектов базы данных. */
  gboolean                     refresh;                /* Признак необходимости обновления списков. */

  guint                        alerter;                /* Идентификатор обработчика сигнализирующего об изменениях. */
  GThread                     *informer;               /* Поток отслеживания изменений. */
  gint                         shutdown;               /* Признак завершения работы. */

  GMutex                       lock;                   /* Блокировка. */
  GCond                        cond;                   /* Триггер для запуска потока. */
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
                                                        guint32                mod_counter,
                                                        gboolean               writeable);

static gboolean    hyscan_db_info_alerter              (gpointer               data);

static gpointer    hyscan_db_info_watcher              (gpointer               data);

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
  g_cond_init (&priv->cond);

  if (priv->db == NULL)
    return;

  priv->actives = g_hash_table_new_full (NULL, NULL, NULL, hyscan_db_info_free_monitor);
  priv->alerter = g_timeout_add (HYSCAN_DB_INFO_TIMEOUT, hyscan_db_info_alerter, info);
  priv->informer = g_thread_new ("db-watcher", hyscan_db_info_watcher, priv);
}

static void
hyscan_db_info_object_finalize (GObject *object)
{
  HyScanDBInfo *info = HYSCAN_DB_INFO (object);
  HyScanDBInfoPrivate *priv = info->priv;

  if (priv->alerter > 0)
    g_source_remove (priv->alerter);

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_cond_signal (&priv->cond);
  g_clear_pointer (&priv->informer, g_thread_join);

  g_clear_pointer (&priv->projects, g_hash_table_unref);
  g_clear_pointer (&priv->tracks, g_hash_table_unref);
  g_clear_pointer (&priv->actives, g_hash_table_unref);

  g_free (priv->new_project_name);
  g_free (priv->project_name);

  g_mutex_clear (&priv->lock);
  g_cond_clear (&priv->cond);
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
                              guint32     mod_counter,
                              gboolean    writeable)
{
  HyScanDBInfoMonitor *info;

  if (actives == NULL)
    return;

  info = g_new0 (HyScanDBInfoMonitor, 1);
  info->db = db;
  info->id = id;
  info->mod_counter = mod_counter;
  info->writeable = writeable;

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
hyscan_db_info_watcher (gpointer data)
{
  HyScanDBInfoPrivate *priv = data;
  gboolean project_changed;
  guint32 tracks_mc;         /* Текущий счётчик изменений в галсах. */
  guint32 projects_mc;       /* Текущий счётчик изменений в проектах. */
  guint32 tracks_mc_old;     /* Предыдущий счётчик изменений в галсах. */
  guint32 projects_mc_old;   /* Предыдущий счётчик изменений в проектах. */
  gboolean check_projects, check_tracks;
  GHashTableIter iter;
  gpointer key, value;
  gint64 end_time;

  /* Начальные значения счётчиков изменений. Специально инициализирую
   * невалидными значениями, чтобы поток не думал, что ничего не изменилось. */
  projects_mc_old = ~hyscan_db_get_mod_count (priv->db, 0);
  tracks_mc_old = projects_mc_old;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      /* Задержка между проверками базы данных, а также завершением работы. */
      g_mutex_lock (&priv->lock);
      end_time = g_get_monotonic_time () + HYSCAN_DB_INFO_COND_TIMEOUT;
      g_cond_wait_until (&priv->cond, &priv->lock, end_time);

      /* Возможно, было запрошено принудительное обновление. */
      check_projects = check_tracks = priv->refresh;
      priv->refresh = FALSE;

      g_mutex_unlock (&priv->lock);

      /* Проверяем изменение списка проектов при наличии изменений. */
      projects_mc = hyscan_db_get_mod_count (priv->db, 0);
      if (check_projects || (projects_mc != projects_mc_old))
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

          projects_mc_old = projects_mc;
          g_strfreev (project_list);
        }

      /* Проверяем, не изменился ли проект. */
      g_mutex_lock (&priv->lock);
      project_changed = priv->project_changed;
      if (project_changed)
        {
          g_clear_pointer (&priv->project_name, g_free);
          priv->project_name = priv->new_project_name;
          priv->new_project_name = NULL;
          priv->project_changed = FALSE;
        }
      g_mutex_unlock (&priv->lock);

      /* Закрываем предыдущий проект. */
      if (project_changed && priv->project_id > 0)
        {
          hyscan_db_close (priv->db, priv->project_id);
          priv->project_id = -1;
        }

      /* Если в данный момент проект не открыт. */
      if (priv->project_id <= 0)
        {
          /* Пробуем открыть проект. */
          if (priv->project_name != NULL && hyscan_db_is_exist (priv->db, priv->project_name, NULL, NULL))
            priv->project_id = hyscan_db_project_open (priv->db, priv->project_name);

          /* Если проект не задан, не существует или не открылся -- выходим. */
          if (priv->project_id <= 0)
            continue;

          /* Если проект открылся, очищаем список галсов. */
          tracks_mc_old = ~hyscan_db_get_mod_count (priv->db, priv->project_id);

          g_mutex_lock (&priv->lock);

          g_clear_pointer (&priv->tracks, g_hash_table_unref);
          priv->tracks_update = TRUE;

          g_mutex_unlock (&priv->lock);
        }

      /* Проверяем изменения в активных объектах базы данных. */
      g_hash_table_iter_init (&iter, priv->actives);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanDBInfoMonitor *info = value;
          guint32 mc;

          /* Проверяем, не пропал ли у канала режим доступа на запись. */
          if (info->writeable)
            {
              if (!hyscan_db_channel_is_writable (priv->db, info->id))
                check_tracks = TRUE;
            }
          /* Проверяем другие изменения в объекте базы данных. */
          else
            {
              mc = hyscan_db_get_mod_count (priv->db, info->id);
              if (mc != info->mod_counter)
                check_tracks = TRUE;
            }

          if (check_tracks)
            break;
        }

      /* Проверяем изменение списка галсов. */
      tracks_mc = hyscan_db_get_mod_count (priv->db, priv->project_id);
      if (check_tracks || (tracks_mc != tracks_mc_old))
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
              if ((info != NULL) && !info->record)
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

          tracks_mc_old = tracks_mc;
          g_strfreev (track_list);
        }
    }

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

  g_clear_pointer (&priv->new_project_name, g_free);
  priv->new_project_name = g_strdup (project_name);
  priv->project_changed = TRUE;
  g_cond_signal (&priv->cond);

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
 * hyscan_db_info_get_db:
 * @info: указатель на #HyScanDBInfo
 *
 * Функция считывает базу данных для которой в данный момент
 * отслеживаются изменения.
 *
 * Returns: (nullable): Указатель на #HyScanDB или %NULL. Для удаления #g_object_unref.
 */
HyScanDB *
hyscan_db_info_get_db (HyScanDBInfo *info)
{
  HyScanDBInfoPrivate *priv;
  HyScanDB *db;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (info), NULL);

  priv = info->priv;

  g_mutex_lock (&priv->lock);
  db = g_object_ref (priv->db);
  g_mutex_unlock (&priv->lock);

  return db;
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
  HyScanDBInfoPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DB_INFO (info));
  priv = info->priv;

  g_mutex_lock (&priv->lock);
  priv->refresh = TRUE;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);
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
      hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter, FALSE);
      return NULL;
    }

  list = hyscan_param_list_new ();

  info = hyscan_db_info_track_info_new ();
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
      hyscan_param_list_add (list, "/plan/start/lat");
      hyscan_param_list_add (list, "/plan/start/lon");
      hyscan_param_list_add (list, "/plan/end/lat");
      hyscan_param_list_add (list, "/plan/end/lon");
      hyscan_param_list_add (list, "/plan/speed");

      if ((hyscan_db_param_get (db, param_id, NULL, list)) &&
          (hyscan_param_list_get_integer (list, "/schema/id") == TRACK_SCHEMA_ID) &&
          (hyscan_param_list_get_integer (list, "/schema/version") == TRACK_SCHEMA_VERSION))
        {
          HyScanTrackPlan plan;
          const gchar *track_type = hyscan_param_list_get_string (list, "/type");
          const gchar *sonar_info = hyscan_param_list_get_string (list, "/sonar");
          gint64 ctime = hyscan_param_list_get_integer (list, "/ctime") / G_USEC_PER_SEC;

          plan.start.lat = hyscan_param_list_get_double (list, "/plan/start/lat");
          plan.start.lon = hyscan_param_list_get_double (list, "/plan/start/lon");
          plan.end.lat = hyscan_param_list_get_double (list, "/plan/end/lat");
          plan.end.lon = hyscan_param_list_get_double (list, "/plan/end/lon");
          plan.speed = hyscan_param_list_get_double (list, "/plan/speed");
          if (plan.speed > 0)
            info->plan = hyscan_track_plan_copy (&plan);

          info->id = hyscan_param_list_dup_string (list, "/id");
          info->ctime = g_date_time_new_from_unix_utc (ctime);
          info->type = hyscan_track_get_type_by_id (track_type);
          info->operator_name = hyscan_param_list_dup_string (list, "/operator");
          info->sonar_info = hyscan_data_schema_new_from_string (sonar_info, "info");
        }
      else
        {
          info->error = TRUE;
        }

      hyscan_db_close (db, param_id);
    }
  else
    {
      info->error = TRUE;
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
      hyscan_param_list_add (list, "/labels");

      if ((hyscan_db_param_get (db, param_id, info->id, list)) &&
          (hyscan_param_list_get_integer (list, "/schema/id") == TRACK_INFO_SCHEMA_ID) &&
          (hyscan_param_list_get_integer (list, "/schema/version") == TRACK_INFO_SCHEMA_VERSION))
        {
          gint64 mtime = hyscan_param_list_get_integer (list, "/mtime") / G_USEC_PER_SEC;

          info->mtime = g_date_time_new_from_unix_utc (mtime);
          info->description = hyscan_param_list_dup_string (list, "/description");
          info->labels = hyscan_param_list_get_integer (list, "/labels");
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
      if (!hyscan_channel_get_types_by_id (channels[i], &source, &type, &channel))
        continue;

      if (type != HYSCAN_CHANNEL_DATA)
        continue;

      /* По акустике учитываем только первые каналы. По датчикам все. */
      if (hyscan_source_is_sonar (source) && (channel != 1))
        continue;
      if (!hyscan_source_is_sensor (source) && !hyscan_source_is_sonar (source))
        continue;

      /* Пытаемся узнать есть ли в нём записанные данные. */
      channel_id = hyscan_db_channel_open (db, track_id, channels[i]);
      if (channel_id <= 0)
        continue;

      param_id = hyscan_db_channel_param_open (db, channel_id);
      if (param_id > 0)
        {
          list = hyscan_param_list_new ();
          hyscan_param_list_add (list, "/schema/id");
          hyscan_param_list_add (list, "/schema/version");

          if (hyscan_source_is_sonar (source))
            {
              HyScanDBInfoSourceInfo *source_info = hyscan_db_info_source_info_new ();

              hyscan_param_list_add (list, "/actuator");

              if ((hyscan_db_param_get (db, param_id, NULL, list)) &&
                  (hyscan_param_list_get_integer (list, "/schema/id") == ACOUSTIC_CHANNEL_SCHEMA_ID) &&
                  (hyscan_param_list_get_integer (list, "/schema/version") == ACOUSTIC_CHANNEL_SCHEMA_VERSION))
                {
                  source_info->actuator = hyscan_param_list_dup_string (list, "/actuator");
                }

              g_hash_table_insert (info->source_infos, GINT_TO_POINTER (source), source_info);
            }
          else if (hyscan_source_is_sensor (source))
            {
              HyScanDBInfoSensorInfo *sensor_info = hyscan_db_info_sensor_info_new ();
              gchar *sensor_name = NULL;

              hyscan_param_list_add (list, "/sensor-name");
              if ((hyscan_db_param_get (db, param_id, NULL, list)) &&
                  (hyscan_param_list_get_integer (list, "/schema/id") == SENSOR_CHANNEL_SCHEMA_ID) &&
                  (hyscan_param_list_get_integer (list, "/schema/version") == SENSOR_CHANNEL_SCHEMA_VERSION))
                {
                  sensor_name = hyscan_param_list_dup_string (list, "/sensor-name");
                }

              sensor_info->channel = channel;
              g_hash_table_insert (info->sensor_infos, sensor_name, sensor_info);
            }

          g_object_unref (list);
          hyscan_db_close (db, param_id);
        }

      active = hyscan_db_channel_is_writable (db, channel_id);

      /* В канале есть минимум одна запись. */
      if (hyscan_db_channel_get_data_range (db, channel_id, NULL, NULL))
        {
          info->sources[source] = TRUE;
          n_sources += 1;

          if (active)
            {
              info->record = TRUE;
              /* Следим, когда канал поменяет свой статус is_writeable. */
              hyscan_db_info_add_active_id (actives, db, channel_id, 0, TRUE);
              channel_id = 0;
          }
        }

      /* Ставим на карандаш, если канал открыт на запись. */
      else if (active)
        {
          guint32 channel_mod_counter = hyscan_db_get_mod_count (db, channel_id);
          hyscan_db_info_add_active_id (actives, db, channel_id, channel_mod_counter, FALSE);
          channel_id = 0;
        }

      /* Если канал не был добавлен в список проверяемых, то закрываем его. */
      if (channel_id > 0)
        hyscan_db_close (db, channel_id);
    }

  g_strfreev (channels);

  /* Если в галсе нет каналов, пропускаем его и добавляем в список проверяемых. */
  if (n_sources == 0)
    {
      hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter, FALSE);
      hyscan_db_info_track_info_free (info);

      return NULL;
    }

  /* Если в галсе есть открытый для записи канал, проверим этот галс позже. */
  if (info->record)
    hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter, FALSE);
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
 * hyscan_db_info_modyfy_track_info:
 * @db_info: указатель на #HyScanDBInfo
 * @track_info: Указатель на #HyScanTrackInfo
 *
 * Функция изменяет объект в параметрах проекта.
 * В результате этой функции все поля объекта будут перезаписаны
 * и в базе данных, и в списке галсов.
 *
 * Returns: TRUE - объект успешно добавлен в базу данных,
 *          FALSE - объект в базу данных не добавлен или произошла ошибка.
 *          Возвращаемое значение ничего не говорит об успешном добавлении
 *          объекта во внутреннюю хэш-таблицу.
 */
gboolean
hyscan_db_info_modify_track_info (HyScanDBInfo     *db_info,
                                  HyScanTrackInfo  *track_info)
{
  HyScanDBInfoPrivate *priv;
  HyScanParamList *list;
  HyScanTrackInfo *track;
  gint32 project_id;
  gint32 param_id;
  gboolean result = FALSE;

  if (track_info == NULL)
    return FALSE;

  if (db_info == NULL)
    return FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (db_info), FALSE);

  priv = db_info->priv;
  project_id = hyscan_db_project_open (priv->db, priv->project_name);

  if (project_id <= 0)
    return FALSE;

  param_id = hyscan_db_project_param_open (priv->db, project_id, PROJECT_INFO_GROUP);

  hyscan_db_close (priv->db, project_id);

  if (param_id <= 0)
    return FALSE;

  list = hyscan_param_list_new ();

  hyscan_param_list_add (list, "/schema/id");
  hyscan_param_list_add (list, "/schema/version");

  if ((hyscan_db_param_get (priv->db, param_id, track_info->id, list)) &&
      (hyscan_param_list_get_integer (list, "/schema/id") == TRACK_INFO_SCHEMA_ID) &&
      (hyscan_param_list_get_integer (list, "/schema/version") == TRACK_INFO_SCHEMA_VERSION))
    {
      hyscan_param_list_clear (list);

      hyscan_param_list_set_integer (list, "/mtime", G_TIME_SPAN_SECOND * g_date_time_to_unix (track_info->mtime));
      hyscan_param_list_set_string  (list, "/description", track_info->description);
      hyscan_param_list_set_integer (list, "/labels", track_info->labels);

      result = hyscan_db_param_set (priv->db, param_id, track_info->id, list);
    }

  g_object_unref (list);
  hyscan_db_close (priv->db, param_id);

  if (!result)
    return FALSE;

  if (priv->tracks == NULL)
    return result;

  g_mutex_lock (&priv->lock);

  track = g_hash_table_lookup (priv->tracks, track_info->name);

  if (track == NULL)
    {
      g_mutex_unlock (&priv->lock);
      return result;
    }

  g_clear_pointer (&track->mtime, g_date_time_unref);
  g_clear_pointer ((gchar**)&track->description, g_free);

  track->mtime = g_date_time_ref (track_info->mtime);
  track->description = g_strdup (track_info->description);
  track->labels = track_info->labels;
  /* Флаг изменения внутренней хэш-таблицы. */
  priv->tracks_update = TRUE;

  g_mutex_unlock (&priv->lock);

  return result;
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

HyScanTrackInfo *
hyscan_db_info_track_info_new (void)
{
  HyScanTrackInfo *new_info;
  new_info = g_slice_new0 (HyScanTrackInfo);

  new_info->source_infos = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                  NULL, (GDestroyNotify)hyscan_db_info_source_info_free);
  new_info->sensor_infos = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  NULL, (GDestroyNotify)hyscan_db_info_sensor_info_free);

  return new_info;
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
  GHashTableIter iter;
  gpointer k, v;

  new_info = hyscan_db_info_track_info_new ();

  new_info->id = g_strdup (info->id);
  new_info->type = info->type;
  new_info->name = g_strdup (info->name);
  if (info->ctime != NULL)
    new_info->ctime = g_date_time_ref (info->ctime);
  if (info->mtime != NULL)
    new_info->mtime = g_date_time_ref (info->mtime);
  new_info->description = g_strdup (info->description);
  new_info->operator_name = g_strdup (info->operator_name);
  new_info->labels = info->labels;
  if (info->sonar_info != NULL)
    new_info->sonar_info = g_object_ref (info->sonar_info);
  new_info->plan = hyscan_track_plan_copy (info->plan);
  memcpy (new_info->sources, info->sources, sizeof (info->sources));
  new_info->record = info->record;

  g_hash_table_iter_init (&iter, info->source_infos);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      HyScanDBInfoSourceInfo * info = v;
      info = hyscan_db_info_source_info_copy (info);
      g_hash_table_insert (new_info->source_infos, k, info);
    }

  g_hash_table_iter_init (&iter, info->sensor_infos);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      HyScanDBInfoSensorInfo * info = v;
      info = hyscan_db_info_sensor_info_copy (info);
      g_hash_table_insert (new_info->sensor_infos, g_strdup(k), info);
    }

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
  g_clear_pointer ((gchar**)&info->id, g_free);
  g_clear_pointer ((gchar**)&info->name, g_free);
  g_clear_pointer ((gchar**)&info->description, g_free);
  g_clear_pointer ((gchar**)&info->operator_name, g_free);
  g_clear_pointer (&info->ctime, g_date_time_unref);
  g_clear_pointer (&info->mtime, g_date_time_unref);
  g_clear_object (&info->sonar_info);
  hyscan_track_plan_free (info->plan);

  g_clear_pointer (&info->source_infos, g_hash_table_unref);
  g_clear_pointer (&info->sensor_infos, g_hash_table_unref);

  g_slice_free (HyScanTrackInfo, info);
}

/**
 * hyscan_db_info_source_info_new:
 *
 * Функция создаёт структуру #HyScanDBInfoSourceInfo.
 *
 * Returns: (transfer full): Новая структура #HyScanDBInfoSourceInfo.
 * Для удаления #hyscan_db_info_source_info_free.
 */
HyScanDBInfoSourceInfo *
hyscan_db_info_source_info_new (void)
{
  return g_slice_new0 (HyScanDBInfoSourceInfo);
}

/**
 * hyscan_db_info_source_info_copy:
 * @info: структура #HyScanDBInfoSourceInfo для копирования
 *
 * Функция создаёт копию структуры #HyScanDBInfoSourceInfo.
 *
 * Returns: (transfer full): Новая структура #HyScanDBInfoSourceInfo.
 * Для удаления #hyscan_db_info_source_info_free.
 */
HyScanDBInfoSourceInfo *
hyscan_db_info_source_info_copy (HyScanDBInfoSourceInfo *info)
{
  HyScanDBInfoSourceInfo *new_info = hyscan_db_info_source_info_new ();
  new_info->actuator = g_strdup (info->actuator);

  return new_info;
}

/**
 * hyscan_db_info_source_info_free:
 * @info: структура #HyScanDBInfoSourceInfo для удаления
 *
 * Функция удаляет структуру #HyScanDBInfoSourceInfo.
 */
void
hyscan_db_info_source_info_free (HyScanDBInfoSourceInfo *info)
{
  g_clear_pointer (&info->actuator, g_free);
  g_slice_free (HyScanDBInfoSourceInfo, info);
}

/**
 * hyscan_db_info_sensor_info_new:
 *
 * Функция создаёт новую структуру #HyScanDBInfoSensorInfo.
 *
 * Returns: (transfer full): Новая структура #HyScanDBInfoSensorInfo.
 * Для удаления #hyscan_db_info_sensor_info_free.
 */
HyScanDBInfoSensorInfo *
hyscan_db_info_sensor_info_new (void)
{
  return g_slice_new0 (HyScanDBInfoSensorInfo);
}

/**
 * hyscan_db_info_sensor_info_copy:
 * @info: структура #HyScanDBInfoSensorInfo для копирования
 *
 * Функция создаёт копию структуры #HyScanDBInfoSensorInfo.
 *
 * Returns: (transfer full): Новая структура #HyScanDBInfoSensorInfo.
 * Для удаления #hyscan_db_info_sensor_info_free.
 */
HyScanDBInfoSensorInfo *
hyscan_db_info_sensor_info_copy (HyScanDBInfoSensorInfo *info)
{
  HyScanDBInfoSensorInfo *new_info = hyscan_db_info_sensor_info_new ();
  new_info->channel = info->channel;

  return new_info;
}

/**
 * hyscan_db_info_sensor_info_free:
 * @info: структура #HyScanDBInfoSensorInfo для удаления
 *
 * Функция удаляет структуру #HyScanDBInfoSensorInfo.
 */
void
hyscan_db_info_sensor_info_free (HyScanDBInfoSensorInfo *info)
{
  g_slice_free (HyScanDBInfoSensorInfo, info);
}

