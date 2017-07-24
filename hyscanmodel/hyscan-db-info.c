/*
 * \file hyscan-db-info.c
 *
 * \brief Исходный файл класса асинхронного мониторинга базы данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-db-info.h"
#include "hyscan-core-schemas.h"

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
  gint                         refresh;                /* Признак необходимости обновления списков. */

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

static HyScanProjectInfo *
                   hyscan_db_info_copy_project_info    (HyScanProjectInfo     *info);

static HyScanTrackInfo *
                   hyscan_db_info_copy_track_info      (HyScanTrackInfo       *info);

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

  hyscan_db_info_signals[SIGNAL_PROJECTS_CHANGED] =
    g_signal_new ("projects-changed", HYSCAN_TYPE_DB_INFO, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

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

  g_atomic_int_set (&priv->shutdown, 1);
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

/* Функция выполняет "глубокое" копирование информации о проекте. */
static HyScanProjectInfo *
hyscan_db_info_copy_project_info (HyScanProjectInfo *info)
{
  HyScanProjectInfo *new_info;

  new_info = g_new0 (HyScanProjectInfo, 1);

  new_info->name = g_strdup (info->name);
  new_info->ctime = g_date_time_to_local (info->ctime);
  new_info->description = g_strdup (info->description);

  return new_info;
}

/* Функция выполняет "глубокое" копирование информации о галсе. */
static HyScanTrackInfo *
hyscan_db_info_copy_track_info (HyScanTrackInfo *info)
{
  HyScanTrackInfo *new_info;
  GHashTable *sources;
  GHashTableIter iter;
  gpointer key, value;

  new_info = g_new0 (HyScanTrackInfo, 1);

  new_info->name = g_strdup (info->name);
  new_info->ctime = g_date_time_to_local (info->ctime);
  new_info->description = g_strdup (info->description);

  new_info->id = g_strdup (info->id);
  new_info->type = info->type;
  new_info->operator_name = g_strdup (info->operator_name);
  new_info->sonar_info = g_object_ref (info->sonar_info);

  sources = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  g_hash_table_iter_init (&iter, info->sources);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (sources, key, g_memdup (value, sizeof (HyScanSourceInfo)));
  new_info->sources = sources;
  new_info->active = info->active;

  return new_info;
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
      if (g_atomic_int_compare_and_exchange (&priv->refresh, 1, 0))
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
                                            g_free, (GDestroyNotify)hyscan_db_info_free_project_info);
          for (i = 0; i < n_projects; i++)
            {
              HyScanProjectInfo *info = NULL;

              if (priv->projects != NULL)
                info = g_hash_table_lookup (priv->projects, project_list[i]);

              /* Если информацию о проекте уже считывали ранее, то используем эту информацию. */
              if (info == NULL)
                info = hyscan_db_info_get_project_info (priv->db, project_list[i]);
              else
                info = hyscan_db_info_copy_project_info (info);

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
                                          g_free, (GDestroyNotify)hyscan_db_info_free_track_info);
          for (i = 0; i < n_tracks; i++)
            {
              HyScanTrackInfo *info = NULL;

              if (priv->tracks != NULL)
                info = g_hash_table_lookup (priv->tracks, track_list[i]);

              /* Если информацию о галсе уже считывали ранее и галс не является активным,
               * то используем эту информацию. Иначе считываем информацию из базы. */
              if ((info != NULL) && !info->active)
                {
                  info = hyscan_db_info_copy_track_info (info);
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

/* Функция создаёт новый объект HyScanDBInfo. */
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

/* Функция устанавливает название проекта для которого будут отслеживаться изменения. */
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

/* Функция считывает название проекта для которого в данный момент отслеживаются изменения. */
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

/* Функция возвращает список проектов и информацию о них. */
GHashTable *
hyscan_db_info_get_projects (HyScanDBInfo *info)
{
  HyScanDBInfoPrivate *priv;
  GHashTable *projects;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (info), NULL);

  priv = info->priv;

  projects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    g_free, (GDestroyNotify)hyscan_db_info_free_project_info);

  g_mutex_lock (&priv->lock);
  if (priv->projects != NULL)
    {
      g_hash_table_iter_init (&iter, priv->projects);
      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (projects, g_strdup (key), hyscan_db_info_copy_project_info (value));
    }
  g_mutex_unlock (&priv->lock);

  return projects;
}

/* Функция возвращает список галсов и информацию о них. */
GHashTable *
hyscan_db_info_get_tracks (HyScanDBInfo *info)
{
  HyScanDBInfoPrivate *priv;
  GHashTable *tracks;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_DB_INFO (info), NULL);

  priv = info->priv;

  tracks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                  g_free, (GDestroyNotify)hyscan_db_info_free_track_info);

  g_mutex_lock (&priv->lock);
  if (priv->tracks != NULL)
    {
      g_hash_table_iter_init (&iter, priv->tracks);
      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (tracks, g_strdup (key), hyscan_db_info_copy_track_info (value));
    }
  g_mutex_unlock (&priv->lock);

  return tracks;
}

/* Функция принудительно запускает процесс обновления. */
void
hyscan_db_info_refresh (HyScanDBInfo *info)
{
  g_return_if_fail (HYSCAN_IS_DB_INFO (info));

  g_atomic_int_set (&info->priv->refresh, 1);
}

/* Функция возвращает информацию о проекте. */
HyScanProjectInfo *
hyscan_db_info_get_project_info (HyScanDB    *db,
                                 const gchar *project_name)
{
  HyScanProjectInfo *info;
  gint32 project_id;
  gint32 param_id;

  /* Открываем проект. */
  project_id = hyscan_db_project_open (db, project_name);
  if (project_id <= 0)
    return NULL;

  info = g_new0 (HyScanProjectInfo, 1);
  info->name = g_strdup (project_name);

  /* Дата и время создания проекта. */
  info->ctime = hyscan_db_project_get_ctime (db, project_id);

  /* Информация о проекте. */
  param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
  if (param_id > 0)
    {
      info->description = hyscan_db_param_get_string (db, param_id, PROJECT_INFO_OBJECT, "/description");
      hyscan_db_close (db, param_id);
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
  HyScanTrackInfo *track_info;
  gchar **channels;
  gint32 track_id;
  gint32 param_id;
  gint32 channel_id;
  guint32 track_mod_counter;
  guint32 channel_mod_counter;
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

  track_info = g_new0 (HyScanTrackInfo, 1);
  track_info->name = g_strdup (track_name);
  track_info->sources = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  /* Дата и время создания галса. */
  track_info->ctime = hyscan_db_track_get_ctime (db, track_id);

  /* Информация о галсе из галса. */
  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id > 0)
    {
      gchar *sonar_info = hyscan_db_param_get_string (db, param_id, NULL, "/sonar");
      gchar *track_type = hyscan_db_param_get_string (db, param_id, NULL, "/type");

      track_info->id = hyscan_db_param_get_string (db, param_id, NULL, "/id");
      track_info->type = hyscan_track_get_type_by_name (track_type);
      track_info->operator_name = hyscan_db_param_get_string (db, param_id, NULL, "/operator");
      track_info->sonar_info = hyscan_data_schema_new_from_string (sonar_info, "info");

      hyscan_db_close (db, param_id);

      g_free (track_type);
      g_free (sonar_info);
    }

  /* Информация о галсе из проекта. */
  param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
  if (param_id > 0)
    {
      track_info->description = hyscan_db_param_get_string (db, param_id, track_name, "/description");
      hyscan_db_close (db, param_id);
    }

  /* Список каналов данных. */
  for (i = 0; channels[i] != NULL; i++)
    {
      HyScanSourceInfo *source_info;
      HyScanSourceType source;
      gboolean active;
      gboolean raw;

      /* Проверяем типы данных. */
      if (!hyscan_channel_get_types_by_name (channels[i], &source, &raw, NULL))
        continue;

      /* Пытаемся узнать есть ли в нём записанные данные. */
      channel_id = hyscan_db_channel_open (db, track_id, channels[i]);
      if (channel_id <= 0)
        continue;

      /* Счётчик изменений в канале данных и признак записи. */
      channel_mod_counter = hyscan_db_get_mod_count (db, channel_id);
      active = hyscan_db_channel_is_writable (db, channel_id);

      /* В канале есть минимум одна запись. */
      if (hyscan_db_channel_get_data_range (db, channel_id, NULL, NULL))
        {
          source_info = g_hash_table_lookup (track_info->sources, GINT_TO_POINTER (source));
          if (source_info == NULL)
            {
              source_info = g_new0 (HyScanSourceInfo, 1);
              source_info->type = source;

              g_hash_table_insert (track_info->sources, GINT_TO_POINTER (source), source_info);
            }

          /* Тип данных канала: сырые или обработанные. */
          if (raw)
            source_info->raw = TRUE;
          else
            source_info->computed = TRUE;

          /* Признак записи в канал и галс. */
          if (active)
            source_info->active = track_info->active = TRUE;
        }

      /* Ставим на карандаш, если канал открыт на запись. */
      else
        {
          if (active)
            hyscan_db_info_add_active_id (actives, db, channel_id, channel_mod_counter);
        }

      hyscan_db_close (db, channel_id);
    }

  /* Если в галсе есть открытый для записи канал, проверим этот галс позже. */
  if (track_info->active)
    hyscan_db_info_add_active_id (actives, db, track_id, track_mod_counter);
  else
    hyscan_db_close (db, track_id);

  g_strfreev (channels);

  return track_info;
}

/* Функция возвращает информацию о галсе. */
HyScanTrackInfo *
hyscan_db_info_get_track_info (HyScanDB    *db,
                               gint32       project_id,
                               const gchar *track_name)
{
  return hyscan_db_info_get_track_info_int (db, project_id, track_name, NULL);
}

/* Функция освобождает память занятую структурой HyScanProjectInfo. */
void
hyscan_db_info_free_project_info (HyScanProjectInfo *info)
{
  g_free (info->name);
  g_free (info->description);
  g_date_time_unref (info->ctime);

  g_free (info);
}

/* Функция освобождает память занятую структурой HyScanTrackInfo. */
void
hyscan_db_info_free_track_info (HyScanTrackInfo *info)
{
  g_free (info->name);
  g_free (info->description);
  g_date_time_unref (info->ctime);

  g_free (info->id);
  g_free (info->operator_name);
  g_clear_object (&info->sonar_info);

  g_clear_pointer (&info->sources, g_hash_table_unref);

  g_free (info);
}
