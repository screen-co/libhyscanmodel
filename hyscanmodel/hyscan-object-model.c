/* hyscan-object-model.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
 * Copyright 2019-2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanModel library.
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
 * SECTION: hyscan-object-model
 * @Short_description: класс асинхронной работы с объектами параметров проекта
 * @Title: HyScanObjectModel
 *
 * HyScanObjectModel реализует интерфейс хранилища объектов HyScanObjectStore и
 * представляет собой асинхронную обертку для классов #HyScanObjectData.
 *
 * HyScanObjectModel может работать с несколькими HyScanObjectData одновременно,
 * конкретные типы классов указываются в функции hyscan_object_model_set_types().
 *
 * Класс содержит все методы, необходимые для создания, изменения и удаления
 * объектов из определённой группы параметров проекта.
 *
 * Сигнал HyScanObjectModel::changed сигнализирует о том, что есть изменения в списке объектов.
 * Прямо в обработчике сигнала можно получить актуальный список объектов.
 *
 * Класс полностью потокобезопасен и может использоваться в MainLoop.
 *
 */

#include "hyscan-object-model.h"

#define CHECK_INTERVAL_US (250 * G_TIME_SPAN_MILLISECOND)    /* Период проверки изменений в параметрах проекта, мкс. */
#define FLUSH_INTERVAL_US (2 * G_TIME_SPAN_SECOND)           /* Период сброса промежуточных изменений, мкс. */
#define ALERT_INTERVAL_MS (500)                              /* Период проверки для отправки сигнала "changed", мс. */

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

typedef enum
{
  TASK_ADD,       /* Добавить объект. */
  TASK_MODIFY,    /* Изменить объект. */
  TASK_REMOVE,    /* Удалить объект. */
  TASK_AUTOMATIC  /* Автоматически определить стратегию. */
} HyScanObjectModelAction;

typedef enum
{
  RESULT_DONE,     /* Задание выполнено. */
  RESULT_RETRY,    /* Задание не выполнено, можно попробовать позже. */
} HyScanObjectModelResult;

typedef struct
{
  HyScanObjectStore *data;                   /* Хранилище объектов. */
  GType             *types;                  /* Типы объектов, которые берутся из этого хранилища. */
  guint              n_types;                /* Количество типов объектов. */
  guint32            mod_count;              /* Последний обработанный номер изменения. */
  gboolean           connected;              /* Признак того, что в хранилище открыт текущий проект. */
} HyScanObjectModelSource;

/* Задание, структура с информацией о том, что требуется сделать. */
typedef struct
{
  gchar                      *id;            /* Идентификатор объекта. */
  GType                       type;          /* Тип объекта. */
  HyScanObject               *object;        /* Объект. */
  HyScanObjectModelAction     action;        /* Требуемое действие. */
} HyScanObjectModelTask;

/* Состояние объекта. */
typedef struct
{
  HyScanDB                *db;               /* БД. */
  gchar                   *project;          /* Проект. */
  gboolean                 project_changed;  /* Флаг смены БД/проекта. */
} HyScanObjectModelState;

struct _HyScanObjectModelPrivate
{
  HyScanObjectModelState   cur_state;        /* Текущее состояние. */
  HyScanObjectModelState   new_state;        /* Желаемое состояние. */
  GMutex                   state_lock;       /* Блокировка состояния. */

  GHashTable              *type_store;       /* Соответствие типов объектов с их реальными хранилищами. */
  HyScanObjectModelSource *stores;           /* Массив хранилищ. */
  guint                    stores_n;         /* Количество хранилищ. */
  GList                   *unchanged_type;   /* Список типов хранилищ, которые не изменились. */

  GThread                 *processing;       /* Поток обработки. */
  gint                     stop;             /* Флаг остановки. */
  GSList                  *tasks;            /* Список заданий. */
  GMutex                   tasks_lock;       /* Блокировка списка заданий. */

  GCond                    im_cond;          /* Триггер отправки изменений в БД. */
  gint                     im_flag;          /* Наличие изменений в БД. */

  guint                    alerter;          /* Идентификатор обработчика сигнализирующего об изменениях. */
  gint                     objects_changed;  /* Флаг, сигнализирующий о том, что список объектов поменялся. */

  GHashTable              *objects;          /* Список объектов (отдаваемый наружу). */
  GMutex                   objects_lock;     /* Блокировка списка объектов. */
  guint32                  mod_count;        /* Счётчик всех изменений. */
};

static void                     hyscan_object_model_interface_init         (HyScanObjectStoreInterface *iface);
static void                     hyscan_object_model_object_constructed     (GObject                    *object);
static void                     hyscan_object_model_object_finalize        (GObject                    *object);
static void                     hyscan_object_model_clear_state            (HyScanObjectModelState     *state);
static gboolean                 hyscan_object_model_track_sync             (HyScanObjectModelPrivate   *priv);
static GHashTable *             hyscan_object_model_make_ht                (void);
static GHashTable *             hyscan_object_model_make_ht_typed          (HyScanObjectModelPrivate   *priv);
static HyScanObjectModelTask *  hyscan_object_model_task_new               (HyScanObjectModelPrivate   *priv,
                                                                            const gchar                *id,
                                                                            GType                       type,
                                                                            const HyScanObject         *object,
                                                                            HyScanObjectModelAction     action);
static void                     hyscan_object_model_task_push              (HyScanObjectModelPrivate   *priv,
                                                                            HyScanObjectModelTask      *task);
static void                     hyscan_object_model_task_free              (HyScanObjectModelTask      *task);
static HyScanObjectModelResult  hyscan_object_model_task_do                (HyScanObjectModelPrivate   *priv,
                                                                            HyScanObjectModelTask      *task);
static void                     hyscan_object_model_task_do_ht             (HyScanObjectModelTask      *task,
                                                                            GHashTable                 *hash_table);
static void                     hyscan_object_model_do_all_tasks           (HyScanObjectModelPrivate   *priv,
                                                                            gboolean                    allow_retry);
static GHashTable *             hyscan_object_model_ht_merge               (HyScanObjectModelPrivate   *priv,
                                                                            GHashTable                 *objects,
                                                                            GList                      *ids);
static gboolean                 hyscan_object_model_ht_equal               (GHashTable                 *table1,
                                                                            GHashTable                 *table2,
                                                                            GList                      *unchanged_types);
static void                     hyscan_object_model_take_objects           (HyScanObjectModelPrivate   *priv,
                                                                            GHashTable                 *hash_table);
static void                     hyscan_object_model_retrieve_store         (HyScanObjectModelPrivate   *priv,
                                                                            GHashTable                 *object_list,
                                                                            HyScanObjectStore          *data);
static gpointer                 hyscan_object_model_processing             (HyScanObjectModel          *model);
static gboolean                 hyscan_object_model_signaller              (gpointer                    data);
static HyScanObject *           hyscan_object_model_lookup                 (GHashTable                 *hash_table,
                                                                            GType                       type,
                                                                            const gchar                *id);
static void                     hyscan_object_model_ht_insert              (GHashTable                 *hash_table,
                                                                            gchar                      *id,
                                                                            HyScanObject               *object);
static gboolean                 hyscan_object_model_ht_contains            (GHashTable                 *hash_table,
                                                                            GType                       type,
                                                                            const gchar                *id);
static void                     hyscan_object_model_ht_remove              (GHashTable                 *hash_table,
                                                                            GType                       type,
                                                                            const gchar                *id);

static guint        hyscan_object_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanObjectModel, hyscan_object_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanObjectModel)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_OBJECT_STORE, hyscan_object_model_interface_init))

static void
hyscan_object_model_class_init (HyScanObjectModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_object_model_object_constructed;
  object_class->finalize = hyscan_object_model_object_finalize;

  /**
   * HyScanObjectModel::changed:
   * @model: указатель на #HyScanObjectModel
   *
   * Сигнал отправляется при изменениях в списке объектов.
   */
  hyscan_object_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_OBJECT_MODEL,
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_object_model_init (HyScanObjectModel *model)
{
  model->priv = hyscan_object_model_get_instance_private (model);
}

static void
hyscan_object_model_object_constructed (GObject *object)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (object);
  HyScanObjectModelPrivate *priv = model->priv;

  g_mutex_init (&priv->tasks_lock);
  g_mutex_init (&priv->objects_lock);
  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->im_cond);
}

static void
hyscan_object_model_object_finalize (GObject *object)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (object);
  HyScanObjectModelPrivate *priv = model->priv;
  guint i;

  if (priv->alerter > 0)
    g_source_remove (priv->alerter);

  g_atomic_int_set (&priv->stop, TRUE);
  g_clear_pointer (&priv->processing, g_thread_join);

  g_mutex_clear (&priv->tasks_lock);
  g_mutex_clear (&priv->objects_lock);
  g_mutex_clear (&priv->state_lock);
  g_cond_clear (&priv->im_cond);

  hyscan_object_model_clear_state (&priv->new_state);
  hyscan_object_model_clear_state (&priv->cur_state);
  g_clear_pointer (&priv->objects, g_hash_table_unref);
  g_clear_pointer (&priv->type_store, g_hash_table_unref);
  for (i = 0; i < priv->stores_n; i++)
    {
      g_clear_object(&priv->stores[i].data);
      g_free (priv->stores[i].types);
    }
  g_free (priv->stores);

  G_OBJECT_CLASS (hyscan_object_model_parent_class)->finalize (object);
}

/* Функция очищает состояние. */
static void
hyscan_object_model_clear_state (HyScanObjectModelState *state)
{
  g_clear_pointer (&state->project, g_free);
  g_clear_object (&state->db);
}

/* Функция синхронизирует состояния. */
static gboolean
hyscan_object_model_track_sync (HyScanObjectModelPrivate *priv)
{
  g_mutex_lock (&priv->state_lock);

  /* Проверяем, нужна ли синхронизация. */
  if (!priv->new_state.project_changed)
    {
      g_mutex_unlock (&priv->state_lock);
      return FALSE;
    }

  g_clear_pointer (&priv->cur_state.project, g_free);
  g_clear_object (&priv->cur_state.db);
  priv->cur_state.db = priv->new_state.db;
  priv->cur_state.project = priv->new_state.project;
  priv->new_state.db = NULL;
  priv->new_state.project = NULL;
  priv->new_state.project_changed = FALSE;

  g_mutex_unlock (&priv->state_lock);
  return TRUE;
}

/* Функция создаёт хэш-таблицу для хранения объектов. */
static GHashTable *
hyscan_object_model_make_ht (void)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_object_free);
}

/* Функция создаёт хэш-таблицу для хранения объектов. */
static GHashTable *
hyscan_object_model_make_ht_typed (HyScanObjectModelPrivate *priv)
{
  GHashTable *table;
  GHashTableIter iter;
  gpointer key;

  table = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_hash_table_unref);

  g_hash_table_iter_init (&iter, priv->type_store);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GType type = GPOINTER_TO_SIZE (key);

      g_hash_table_insert (table, GSIZE_TO_POINTER (type), hyscan_object_model_make_ht ());
    }

  return table;
}

/* Функция создаёт новое задание. */
static HyScanObjectModelTask *
hyscan_object_model_task_new (HyScanObjectModelPrivate *priv,
                              const gchar              *id,
                              GType                     type,
                              const HyScanObject       *object,
                              HyScanObjectModelAction   action)
{
  HyScanObjectModelTask *task;
  gboolean generate_id;
  gchar *task_id;

  generate_id = (id == NULL) &&
                ((action == TASK_ADD) || (action == TASK_AUTOMATIC && object != NULL));

  if (generate_id)
    {
      HyScanObjectModelSource *source;
      source = g_hash_table_lookup (priv->type_store, GSIZE_TO_POINTER (object->type));
      if (source == NULL)
        {
          g_warning ("HyScanObjectModel: object type \"%s\" is not supported", g_type_name (object->type));
          return NULL;
        }

      task_id = hyscan_object_data_generate_id (HYSCAN_OBJECT_DATA (source->data), object);
    }
  else
    {
      task_id = g_strdup (id);
    }

  /* Создаём задание. */
  task = g_slice_new (HyScanObjectModelTask);
  task->action = action;
  task->type = type;
  task->id = task_id;
  task->object = hyscan_object_copy (object);

  return task;
}

/* Функция создаёт новое задание. */
static void
hyscan_object_model_task_push (HyScanObjectModelPrivate *priv,
                               HyScanObjectModelTask    *task)
{
  /* Отправляем задание в очередь. */
  g_mutex_lock (&priv->tasks_lock);
  priv->tasks = g_slist_append (priv->tasks, task);
  g_mutex_unlock (&priv->tasks_lock);

  /* Сигнализируем об изменениях.
   * Документация гласит, что блокировать мьютекс хорошо, но не обязательно. */
  g_atomic_int_set (&priv->im_flag, TRUE);
  g_cond_signal (&priv->im_cond);
}

/* Функция освобождает задание. */
static void
hyscan_object_model_task_free (HyScanObjectModelTask *task)
{
  hyscan_object_free (task->object);
  g_free (task->id);
  g_slice_free (HyScanObjectModelTask, task);
}

/* Функция выполняет задание. Возвращает %RESULT_RETRY, если задание не может быть
 * выполнено в данный момент и может быть снова добавлено в очередь. */
static HyScanObjectModelResult
hyscan_object_model_task_do (HyScanObjectModelPrivate *priv,
                             HyScanObjectModelTask    *task)
{
  HyScanObjectModelSource *source;

  source = g_hash_table_lookup (priv->type_store, GSIZE_TO_POINTER (task->type));
  if (source == NULL)
    {
      g_warning ("HyScanObjectModel: can't find store for type %s", g_type_name (task->type));
      return RESULT_DONE;
    }

  if (!source->connected)
    return RESULT_RETRY;

  switch (task->action)
    {
      case TASK_ADD:
        if (!hyscan_object_store_add (source->data, task->object, NULL))
          g_warning ("HyScanObjectModel: failed to add object");
        break;

      case TASK_MODIFY:
        if (!hyscan_object_store_modify (source->data, task->id, task->object))
          g_warning ("HyScanObjectModel: failed to modify object <%s>", task->id);
        break;

      case TASK_REMOVE:
        if (!hyscan_object_store_remove (source->data, task->type, task->id))
          g_warning ("HyScanObjectModel: failed to remove object <%s>", task->id);
        break;

      case TASK_AUTOMATIC:
        if (!hyscan_object_store_set (source->data, task->type, task->id, task->object))
          g_warning ("HyScanObjectModel: failed to set object <%s>", task->id);
        break;
    }

  return RESULT_DONE;
}

/* Функция выполняет задание в хэш-таблице. */
static void
hyscan_object_model_task_do_ht (HyScanObjectModelTask *task,
                                GHashTable            *hash_table)
{
  switch (task->action)
    {
      case TASK_ADD:
        g_warning ("HyScanObjectModel: TASK_ADD is not implemented for hash table, task->id is required");
        break;

      case TASK_MODIFY:
        hyscan_object_model_ht_insert (hash_table, g_strdup (task->id), hyscan_object_copy (task->object));
        break;

      case TASK_REMOVE:
        hyscan_object_model_ht_remove (hash_table, task->type, task->id);
        break;

      case TASK_AUTOMATIC:
        if (task->object != NULL)
          hyscan_object_model_ht_insert (hash_table, g_strdup (task->id), hyscan_object_copy (task->object));
        else
          hyscan_object_model_ht_remove (hash_table, task->type, task->id);
        break;

      default:
        g_warning ("HyScanObjectModel: unknown task action %d", task->action);
    }
}

/* Функция выполняет все задания. */
static void
hyscan_object_model_do_all_tasks (HyScanObjectModelPrivate *priv,
                                  gboolean                  allow_retry)
{
  GSList *tasks, *link;

  /* Переписываем задания во временный список, чтобы
   * как можно меньше задерживать основной поток. */
  g_mutex_lock (&priv->tasks_lock);
  tasks = priv->tasks;
  priv->tasks = NULL;
  g_mutex_unlock (&priv->tasks_lock);

  /* Все задания отправляем в БД и очищаем наш временный список. */
  for (link = tasks; link != NULL; link = link->next)
    {
      HyScanObjectModelTask *task = link->data;
      HyScanObjectModelResult result;

      result = hyscan_object_model_task_do (priv, task);
      if (result == RESULT_RETRY)
        {
          if (allow_retry)
            {
              hyscan_object_model_task_push (priv, task);
              continue;
            }

          g_warning ("HyScanObjectModel: task has not been done, won't retry");
        }

      hyscan_object_model_task_free (task);
    }

  g_slist_free (tasks);
}

/* Функция формирует промежуточную хэш-таблицу, состоящую как из новых объектов из таблицы objects,
 * так и старых из таблицы priv->objects. */
static GHashTable *
hyscan_object_model_ht_merge (HyScanObjectModelPrivate  *priv,
                              GHashTable                *objects,
                              GList                     *ids)
{
  GHashTable *ht;
  GList *link;

  ht = hyscan_object_model_make_ht_typed (priv);

  /* Первым проходом копируем новые данные. */
  for (link = ids; link != NULL; link = link->next)
    {
      HyScanObjectId *id = link->data;
      const HyScanObject *object;

      object = hyscan_object_model_lookup (objects, id->type, id->id);
      if (object == NULL)
        continue;

      hyscan_object_model_ht_insert (ht, g_strdup (id->id), hyscan_object_copy (object));
    }

  /* Если старых данных нет, то возвращаем что есть. */
  if (priv->objects == NULL)
    return ht;

  /* Вторым проходом дополняем старыми данными. */
  g_mutex_lock (&priv->objects_lock);
  for (link = ids; link != NULL; link = link->next)
    {
      HyScanObjectId *id = link->data;
      const HyScanObject *object;

      if (hyscan_object_model_ht_contains (ht, id->type, id->id))
        continue;

      object = hyscan_object_model_lookup (priv->objects, id->type, id->id);
      if (object == NULL)
        continue;

      hyscan_object_model_ht_insert (ht, g_strdup (id->id), hyscan_object_copy (object));
    }
  g_mutex_unlock (&priv->objects_lock);

  return ht;
}

/* Функция возвращает %TRUE, если хэш-таблицы содержат одинаковые объекты. */
static gboolean
hyscan_object_model_ht_equal (GHashTable *table1,
                              GHashTable *table2,
                              GList      *unchanged_types)
{
  GHashTableIter iter;
  gpointer key, value;

  if (table1 == NULL || table2 == NULL)
    return FALSE;

  g_hash_table_iter_init (&iter, table1);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GHashTableIter objects1_iter;
      GHashTable *objects1 = value;
      GHashTable *objects2 = g_hash_table_lookup (table2, key);
      const HyScanObject *object1;
      const gchar *object_id;

      if (!g_list_find (unchanged_types, key))
        continue;

      if (g_hash_table_size (objects1) != g_hash_table_size (objects2))
        return FALSE;

      g_hash_table_iter_init (&objects1_iter, objects1);
      while (g_hash_table_iter_next (&objects1_iter, (gpointer *) &object_id, (gpointer *) &object1))
        {
          const HyScanObject *object2 = g_hash_table_lookup (objects2, object_id);

          if (objects2 == NULL)
            return FALSE;

          if (!hyscan_object_equal (object1, object2))
            return FALSE;
        }
    }

  return TRUE;
}

/* Функция устанавливает новую хэш-таблицу объектов и инициирует отправление сигнала "changed". */
static void
hyscan_object_model_take_objects (HyScanObjectModelPrivate  *priv,
                                  GHashTable                *hash_table)
{
  GHashTable *temp;
  GSList *link;

  /* Накатываем в hash_table изменения, которые ещё не применились в БД. */
  g_mutex_lock (&priv->tasks_lock);
  for (link = priv->tasks; link != NULL; link = link->next)
    {
      HyScanObjectModelTask *task = link->data;
      hyscan_object_model_task_do_ht (task, hash_table);
    }

  /* Пишем обновлённую хэш-таблицу. */
  g_mutex_lock (&priv->objects_lock);
  if (!hyscan_object_model_ht_equal (priv->objects, hash_table, priv->unchanged_type))
    {
      temp = priv->objects;
      priv->objects = hash_table;
      priv->objects_changed = TRUE;
      priv->mod_count++;
    }
  else
    {
      g_debug ("HyScanObjectModel: hash tables are equal, do not emit signal");
      temp = hash_table;
    }
  g_mutex_unlock (&priv->objects_lock);

  g_mutex_unlock (&priv->tasks_lock);

  g_clear_pointer (&temp, g_hash_table_unref);
}

/* Функция добавляет объект в хэш-таблицу. */
static void
hyscan_object_model_ht_insert (GHashTable   *hash_table,
                               gchar        *id,
                               HyScanObject *object)
{
  GHashTable *objects_table;

  objects_table = g_hash_table_lookup (hash_table, GSIZE_TO_POINTER (object->type));
  if (objects_table == NULL)
    return;

  g_hash_table_insert (objects_table, id, object);
}

/* Функция удаляет объект из хэш-таблицы. */
static void
hyscan_object_model_ht_remove (GHashTable   *hash_table,
                               GType         type,
                               const gchar  *id)
{
  GHashTable *objects_table;

  objects_table = g_hash_table_lookup (hash_table, GSIZE_TO_POINTER (type));
  if (objects_table == NULL)
    return;

  g_hash_table_remove (objects_table, id);
}

/* Функция получает объект из хэш-таблицы. */
static HyScanObject *
hyscan_object_model_lookup (GHashTable   *hash_table,
                            GType         type,
                            const gchar  *id)
{
  GHashTable *objects_table;

  objects_table = g_hash_table_lookup (hash_table, GSIZE_TO_POINTER (type));
  if (objects_table == NULL)
    return NULL;

  return g_hash_table_lookup (objects_table, id);
}

/* Функция проверяет наличие объекта в хэш-таблице. */
static gboolean
hyscan_object_model_ht_contains (GHashTable   *hash_table,
                                 GType         type,
                                 const gchar  *id)
{
  GHashTable *objects_table;

  objects_table = g_hash_table_lookup (hash_table, GSIZE_TO_POINTER (type));
  if (objects_table == NULL)
    return FALSE;

  return g_hash_table_contains (objects_table, id);
}

/* Функция забирает объекты из указанного хранилища data. */
static void
hyscan_object_model_retrieve_store (HyScanObjectModelPrivate *priv,
                                    GHashTable               *object_list,
                                    HyScanObjectStore        *data)
{
  HyScanObject *object;
  GList *id_list;
  GList *link;
  gint64 start;

  start = g_get_monotonic_time ();

  /* Считываем список идентификаторов. */
  id_list = hyscan_object_store_get_ids (data);

  /* Поштучно копируем из БД в хэш-таблицу. */
  for (link = id_list; link != NULL; link = link->next)
    {
      HyScanObjectId *id = link->data;

      object = hyscan_object_store_get (data, id->type, id->id);
      if (object == NULL)
        continue;

      hyscan_object_model_ht_insert (object_list, g_strdup (id->id), object);

      /* Если мы уже достаточно долго выгружаем объекты, то передадим пользователям класса промежуточные результаты. */
      if (g_get_monotonic_time() - start > FLUSH_INTERVAL_US)
        {
          GHashTable *mixed;

          g_debug ("HyScanObjectModel: flushing intermediate results (%ld us)", g_get_monotonic_time() - start);

          mixed = hyscan_object_model_ht_merge (priv, object_list, id_list);
          hyscan_object_model_take_objects (priv, mixed);

          start = g_get_monotonic_time ();
        }
    }

  g_list_free_full (id_list, (GDestroyNotify) hyscan_object_id_free);
}

/* Функция подключает объекты HyScanObjectData к текущему проекту БД. */
static gboolean
hyscan_object_model_connect_stores (HyScanObjectModelPrivate *priv)
{
  guint i;
  gboolean ready;

  ready = TRUE;
  for (i = 0; i < priv->stores_n; i++)
    {
      HyScanObjectModelSource *source = &priv->stores[i];
      HyScanObjectData *data = HYSCAN_OBJECT_DATA (source->data);

      if (source->connected)
        continue;

      source->connected = hyscan_object_data_project_open (data, priv->cur_state.db, priv->cur_state.project);
      if (!source->connected)
        ready = FALSE;
    }

  return ready;
}

/* Функция получает объекты из всех зарегистрированных хранилищ. */
static void
hyscan_object_model_retrieve_all (HyScanObjectModel *model)
{
  HyScanObjectModelPrivate *priv = model->priv;
  GHashTable *object_list;
  guint i;
  GList *changed_types = NULL;

  g_clear_pointer (&priv->unchanged_type, g_list_free);

  object_list = hyscan_object_model_make_ht_typed (priv);
  for (i = 0; i < priv->stores_n; i++)
    {
      HyScanObjectModelSource *source = &priv->stores[i];
      guint32 mod_count;

      if (!source->connected)
        continue;

      /* Запоминаем мод_каунт перед(!) забором объектов. */
      mod_count = hyscan_object_store_get_mod_count (source->data, G_TYPE_BOXED);

      /* Если mod_count не поменялся, то не лезем в бд, а просто копируем данные из внутреннего буфера. */
      if (source->mod_count == mod_count)
        {
          guint j;

          for (j = 0; j < source->n_types; j++)
            {
              GType type = source->types[j];
              GHashTable *type_objects;

              /* Запоминаем типы объектов, которые не изменились в базе данных.
               * Позже в hyscan_object_model_take_objects() их можно опустить при сравнении старой таблицы с новой. */
              priv->unchanged_type = g_list_append (priv->unchanged_type, GSIZE_TO_POINTER (type));

              type_objects = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (model), type);
              if (type_objects != NULL)
                g_hash_table_insert (object_list, GSIZE_TO_POINTER (type), type_objects);
            }

          continue;
        }

      hyscan_object_model_retrieve_store (priv, object_list, source->data);
      source->mod_count = mod_count;
    }
  hyscan_object_model_take_objects (priv, object_list);
}

/* Поток асинхронной работы с БД. */
static gpointer
hyscan_object_model_processing (HyScanObjectModel *model)
{
  HyScanObjectModelPrivate *priv = model->priv;
  GMutex im_lock; /* Мьютекс нужен для GCond. */

  g_mutex_init (&im_lock);

  while (!g_atomic_int_get (&priv->stop))
    {
      gboolean any_changes;
      gboolean connected;
      guint i;

      /* Проверяем наличие изменений в источниках данных. */
      any_changes = FALSE;
      for (i = 0; i < priv->stores_n; i++)
        {
          HyScanObjectModelSource *source = &priv->stores[i];
          guint32 type_mod_count;

          if (!source->connected)
            continue;

          type_mod_count = hyscan_object_store_get_mod_count (source->data, G_TYPE_BOXED);
          if (type_mod_count != source->mod_count)
            {
              any_changes = TRUE;
              break;
            }
        }

      /* Ждём, когда нужно действовать. */
      if (!any_changes && !g_atomic_int_get (&priv->im_flag))
        {
          gboolean triggered;
          gint64 until = g_get_monotonic_time () + CHECK_INTERVAL_US;

          g_mutex_lock (&im_lock);
          triggered = g_cond_wait_until (&priv->im_cond, &im_lock, until);
          g_mutex_unlock (&im_lock);

          if (!triggered)
            continue;
        }

      g_atomic_int_set (&priv->im_flag, FALSE);

      /* Если проект поменялся, надо выполнить все
       * текущие задачи и пересоздать объект object_data. */
      if (hyscan_object_model_track_sync (priv))
        {
          hyscan_object_model_do_all_tasks (priv, FALSE);
          for (i = 0; i < priv->stores_n; i++)
            priv->stores[i].connected = FALSE;
        }

      /* Подключаем неподключённые HyScanObjectData's к текущему проекту. */
      connected = hyscan_object_model_connect_stores (priv);

      /* Выполняем все задания. */
      hyscan_object_model_do_all_tasks (priv, TRUE);

      /* Получаем свежие данные из БД. */
      hyscan_object_model_retrieve_all (model);

      /* Если остались неподключённые хранилища (например потому, что проект ещё не создан),
       * повторим попытку через некоторое время. */
      if (!connected)
        {
          g_warning ("HyScanObjectModel: some datas not connected yet, retrying...");
          g_atomic_int_set (&priv->im_flag, TRUE);
          g_usleep (CHECK_INTERVAL_US);
        }
    }

  g_mutex_clear (&im_lock);
  return NULL;
}

/* Функция эмитирует сигнал "changed", если есть изменения. */
static void
hyscan_object_model_signal (HyScanObjectModel *model)
{
  HyScanObjectModelPrivate *priv = model->priv;
  gboolean objects_changed;
  guint32 mod_count;

  /* Если есть изменения, сигнализируем. */
  g_mutex_lock (&priv->objects_lock);
  objects_changed = priv->objects_changed;
  priv->objects_changed = FALSE;
  mod_count = priv->mod_count;
  g_mutex_unlock (&priv->objects_lock);

  if (objects_changed)
    {
      g_signal_emit (model, hyscan_object_model_signals[SIGNAL_CHANGED], 0, NULL);
      g_debug ("HyScanObjectModel: \"changed\" %u", mod_count);
    }
}

/* Функция эмитирует сигнал "changed", если есть изменения. */
static gboolean
hyscan_object_model_signaller (gpointer data)
{
  hyscan_object_model_signal (HYSCAN_OBJECT_MODEL (data));

  return G_SOURCE_CONTINUE;
}

/* Функция эмитирует сигнал "changed", если есть изменения. */
static gboolean
hyscan_object_model_signaller_once (gpointer data)
{
  hyscan_object_model_signal (HYSCAN_OBJECT_MODEL (data));

  return G_SOURCE_REMOVE;
}

/* Функция сразу же отправляет сигнал об изменении данных. */
static void
hyscan_object_model_object_unlock_and_signal (HyScanObjectModel *model,
                                              GMutex            *objects_lock)
{
  HyScanObjectModelPrivate *priv = model->priv;

  priv->objects_changed = TRUE;
  priv->mod_count++;
  g_mutex_unlock (objects_lock);

  g_idle_add (hyscan_object_model_signaller_once, model);
}

static gboolean
hyscan_object_model_add (HyScanObjectStore   *store,
                         const HyScanObject  *object,
                         gchar              **given_id)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  HyScanObjectModelTask *task;

  g_return_val_if_fail (object != NULL, FALSE);

  /* Т.к. клиент сразу хочет получит given_id, то вместо TASK_ADD делаем TASK_AUTOMATIC с предопределённым id. */
  task = hyscan_object_model_task_new (priv, NULL, object->type, object, TASK_AUTOMATIC);
  if (task == NULL)
    return FALSE;

  /* Сразу пишем изменения во внутренний буфер. */
  g_mutex_lock (&priv->objects_lock);
  hyscan_object_model_task_do_ht (task, priv->objects);
  hyscan_object_model_object_unlock_and_signal (model, &priv->objects_lock);

  (given_id != NULL) ? *given_id = g_strdup (task->id) : NULL;

  hyscan_object_model_task_push (priv, task);

  return TRUE;
}

static gboolean
hyscan_object_model_modify (HyScanObjectStore  *store,
                            const gchar        *id,
                            const HyScanObject *object)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  HyScanObjectModelTask *task;

  g_return_val_if_fail (id != NULL && object != NULL, FALSE);

  task = hyscan_object_model_task_new (priv, id, object->type, object, TASK_MODIFY);

  /* Сразу пишем изменения во внутренний буфер. */
  g_mutex_lock (&priv->objects_lock);
  hyscan_object_model_task_do_ht (task, priv->objects);
  hyscan_object_model_object_unlock_and_signal (model, &priv->objects_lock);

  hyscan_object_model_task_push (priv, task);

  return TRUE;
}

static gboolean
hyscan_object_model_remove (HyScanObjectStore *store,
                            GType              type,
                            const gchar       *id)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  HyScanObjectModelTask *task;

  g_return_val_if_fail (id != NULL, FALSE);

  task = hyscan_object_model_task_new (priv, id, type, NULL, TASK_REMOVE);

  /* Сразу пишем изменения во внутренний буфер. */
  g_mutex_lock (&priv->objects_lock);
  hyscan_object_model_task_do_ht (task, priv->objects);
  hyscan_object_model_object_unlock_and_signal (model, &priv->objects_lock);

  hyscan_object_model_task_push (model->priv, task);

  return TRUE;
}

static gboolean
hyscan_object_model_set (HyScanObjectStore  *store,
                         GType               type,
                         const gchar        *id,
                         const HyScanObject *object)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  HyScanObjectModelTask *task;

  g_return_val_if_fail (id != NULL || object != NULL, FALSE);

  task = hyscan_object_model_task_new (priv, id, type, object, TASK_AUTOMATIC);

  /* Сразу пишем изменения во внутренний буфер. */
  g_mutex_lock (&priv->objects_lock);
  hyscan_object_model_task_do_ht (task, priv->objects);
  hyscan_object_model_object_unlock_and_signal (model, &priv->objects_lock);

  hyscan_object_model_task_push (priv, task);

  return TRUE;
}

static GHashTable*
hyscan_object_model_get_all (HyScanObjectStore *store,
                             GType              type)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv;
  GHashTable *type_objects;
  GHashTable *objects;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_MODEL (model), NULL);
  priv = model->priv;

  g_mutex_lock (&priv->objects_lock);

  /* Проверяем, что объекты есть. */
  if (priv->objects == NULL)
    {
      g_mutex_unlock (&priv->objects_lock);
      return NULL;
    }

  /* Копируем объекты. */
  type_objects = g_hash_table_lookup (priv->objects, GSIZE_TO_POINTER (type));
  objects = hyscan_object_model_copy (type_objects);
  g_mutex_unlock (&priv->objects_lock);

  return objects;
}

static HyScanObject *
hyscan_object_model_get (HyScanObjectStore *store,
                         GType              type,
                         const gchar       *id)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  HyScanObject *object, *copy = NULL;

  g_return_val_if_fail (id != NULL, NULL);

  g_mutex_lock (&priv->objects_lock);

  /* Проверяем, что объекты есть. */
  if (priv->objects == NULL)
    {
      g_mutex_unlock (&priv->objects_lock);
      return NULL;
    }

  object = hyscan_object_model_lookup (priv->objects, type, id);
  copy = hyscan_object_copy (object);
  g_mutex_unlock (&priv->objects_lock);

  return copy;
}

static guint32
hyscan_object_model_get_mod_count (HyScanObjectStore  *store,
                                   GType               type)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  guint32 mod_count;

  g_mutex_lock (&priv->objects_lock);
  mod_count = priv->mod_count;
  g_mutex_unlock (&priv->objects_lock);

  return mod_count;
}

static GList *
hyscan_object_model_get_ids (HyScanObjectStore *store)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (store);
  HyScanObjectModelPrivate *priv = model->priv;
  GList *list = NULL;
  GHashTableIter type_iter;
  gpointer key, value;

  g_mutex_lock (&priv->objects_lock);
  g_hash_table_iter_init (&type_iter, priv->objects);
  while (g_hash_table_iter_next (&type_iter, &key, &value))
    {
      GType type = GPOINTER_TO_SIZE (key);
      GHashTable *type_objects = value;
      GHashTableIter iter;
      gchar *id;

      g_hash_table_iter_init (&iter, type_objects);
      while (g_hash_table_iter_next (&iter, (gpointer *) &id, NULL))
        {
          HyScanObjectId *object_id;

          object_id = hyscan_object_id_new ();
          object_id->type = type;
          object_id->id = g_strdup (id);
          list = g_list_append (list, id);
        }
    }
  g_mutex_unlock (&priv->objects_lock);

  return list;
}

static void
hyscan_object_model_interface_init (HyScanObjectStoreInterface *iface)
{
  iface->get_mod_count = hyscan_object_model_get_mod_count;
  iface->get = hyscan_object_model_get;
  iface->modify = hyscan_object_model_modify;
  iface->get_ids = hyscan_object_model_get_ids;
  iface->add = hyscan_object_model_add;
  iface->get_all = hyscan_object_model_get_all;
  iface->remove = hyscan_object_model_remove;
  iface->set = hyscan_object_model_set;
}

/**
 * hyscan_object_model_new:
 *
 * Функция создаёт новый объект HyScanObjectModel. Класс работы с параметрами
 * должен быть наследником абстрактного класса  #HyScanObjectDataClass
 *
 * Returns: объект HyScanObjectModel.
 */
HyScanObjectModel *
hyscan_object_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_OBJECT_MODEL, NULL);
}

/**
 * hyscan_object_model_new:
 * @n_types: число классов #HyScanObjectDataClass.
 * @...: типы классов для работы с параметрами проекта, наследник #HyScanObjectDataClass.
 *
 * Функция создаёт новый объект HyScanObjectModel. Класс работы с параметрами
 * должен быть наследником абстрактного класса  #HyScanObjectDataClass
 *
 * Returns: объект HyScanObjectModel.
 */
gboolean
hyscan_object_model_set_types (HyScanObjectModel *model,
                               guint              n_types,
                               ...)
{
  HyScanObjectModelPrivate *priv;
  GHashTableIter iter;
  va_list types;
  guint i;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_MODEL (model), FALSE);
  priv = model->priv;

  if (priv->stores != NULL)
    return FALSE;

  /* Создаём хранилища. */
  priv->stores = g_new0 (HyScanObjectModelSource, n_types);
  priv->stores_n = n_types;
  priv->type_store = g_hash_table_new (NULL, NULL);

  va_start (types, n_types);
  for (i = 0; i < n_types; i++)
    {
      GType type = va_arg (types, GType);;
      HyScanObjectModelSource *source = &priv->stores[i];
      HyScanObjectData *data;
      guint j, n_object_types;
      const GType *object_types;

      data = hyscan_object_data_new (type);
      source->data = HYSCAN_OBJECT_STORE (data);

      /* Запоминаем типы объектов, которые обрабатываются данным хранилищем. */
      object_types = hyscan_object_store_list_types (HYSCAN_OBJECT_STORE (data), &n_object_types);
      if (n_object_types == 0)
        g_warning ("HyScanObjectModel: data class %s do not manage any object types", g_type_name (type));
      for (j = 0; j < n_object_types; j++)
        g_hash_table_insert (priv->type_store, GSIZE_TO_POINTER (object_types[j]), source);

    }
  va_end (types);

  /* Поскольку теоретически два хранилища могут обрабатывать один и тот же тип объектов, то необходимо по данным
   * priv->types_store составить обратное соответствие (хранилище - типы объектов). */
  g_hash_table_iter_init (&iter, priv->type_store);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanObjectModelSource *source = value;
      GType type = GPOINTER_TO_SIZE (key);

      source->n_types++;
      source->types = g_realloc_n (source->types, source->n_types, sizeof (GType));
      source->types[source->n_types - 1] = type;
    }

  /* Запускаем поток обмена данными. */
  priv->stop = FALSE;
  priv->processing = g_thread_new ("object-model", (GThreadFunc) hyscan_object_model_processing, model);
  priv->alerter = g_timeout_add (ALERT_INTERVAL_MS, hyscan_object_model_signaller, model);

  return TRUE;
}

/**
 * hyscan_object_model_set_project:
 * @model: указатель на #HyScanObjectModel
 * @db: база данных #HyScanDb
 * @project: имя проекта
 *
 * Функция устанавливает проект.
 */
void
hyscan_object_model_set_project (HyScanObjectModel *model,
                                 HyScanDB          *db,
                                 const gchar       *project)
{
  HyScanObjectModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_OBJECT_MODEL (model));
  priv = model->priv;

  if (project == NULL || db == NULL)
    return;

  g_mutex_lock (&priv->state_lock);

  hyscan_object_model_clear_state (&priv->new_state);

  priv->new_state.db = g_object_ref (db);
  priv->new_state.project = g_strdup (project);
  priv->new_state.project_changed = TRUE;

  /* Сигнализируем об изменениях. */
  g_atomic_int_set (&priv->im_flag, TRUE);
  g_cond_signal (&priv->im_cond);

  g_mutex_unlock (&priv->state_lock);
}

/**
 * hyscan_object_model_refresh:
 * @model: указатель на #HyScanObjectModel
 *
 * Функция инициирует принудительное обновление списка объектов.
 */
void
hyscan_object_model_refresh (HyScanObjectModel *model)
{
  g_return_if_fail (HYSCAN_IS_OBJECT_MODEL (model));

  g_atomic_int_set (&model->priv->im_flag, TRUE);
  g_cond_signal (&model->priv->im_cond);
}

/**
 * hyscan_object_model_copy:
 * @src: таблица объектов
 *
 * Вспомогательная функция для создания копии таблицы объектов.
 *
 * Returns: (transfer full): новая таблица, для удаления g_hash_table_unref().
 */
GHashTable *
hyscan_object_model_copy (GHashTable *src)
{
  GHashTable *dst;
  GHashTableIter iter;
  gpointer k, v;

  if (src == NULL)
    return NULL;

  dst = hyscan_object_model_make_ht ();

  /* Переписываем объекты. */
  g_hash_table_iter_init (&iter, src);
  while (g_hash_table_iter_next (&iter, &k, &v))
    g_hash_table_insert (dst, g_strdup (k), hyscan_object_copy (v));

  return dst;
}
