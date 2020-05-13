/* hyscan-object-model.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * HyScanObjectModel представляет собой асинхронную обертку для #HyScanObjectData.
 * Класс содержит все методы, необходимые для создания, изменения и удаления
 * объектов из определённой группы параметров проекта.
 *
 * Изменение списка объектов выполняется при помощи функций:
 * - hyscan_object_model_add_object(),
 * - hyscan_object_model_modify_object(),
 * - hyscan_object_model_remove_object().
 *
 * Получить объекты можно двумя способами:
 * - hyscan_object_model_get() - получение всех объектов в виде хэш-таблицы,
 * - hyscan_object_model_get_id() - получение одного объекта по его идентификатору.
 *
 * Сигнал HyScanObjectModel::changed сигнализирует о том, что есть изменения в списке объектов.
 * Прямо в обработчике сигнала можно получить актуальный список объектов.
 *
 * Класс полностью потокобезопасен и может использоваться в MainLoop.
 *
 */

#include "hyscan-object-model.h"

#define CHECK_INTERVAL_US (250 * G_TIME_SPAN_MILLISECOND)    /* Период проверки изменений в параметрах проекта, мкс. */
#define ALERT_INTERVAL_MS (500)                              /* Период проверки для отправки сигнала "changed". */

enum
{
  PROP_O,
  PROP_DATA_TYPE
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

typedef enum
{
  OBJECT_ADD    = 1001,                      /* Добавление объекта. */
  OBJECT_MODIFY = 1002,                      /* Изменение объекта. */
  OBJECT_REMOVE = 1003                       /* Удаление объекта. */
} HyScanObjectModelAction;

/* Задание, структура с информацией о том, что требуется сделать. */
typedef struct
{
  gchar                      *id;            /* Идентификатор объекта. */
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
  GType                    data_type;        /* Класс работы с данными. */
  HyScanObjectModelState   cur_state;        /* Текущее состояние. */
  HyScanObjectModelState   new_state;        /* Желаемое состояние. */
  GMutex                   state_lock;       /* Блокировка состояния. */

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
};

static void         hyscan_object_model_object_constructed     (GObject                   *object);
static void         hyscan_object_model_object_finalize        (GObject                   *object);
static void         hyscan_object_model_set_property           (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void         hyscan_object_model_clear_state            (HyScanObjectModelState    *state);
static gboolean     hyscan_object_model_track_sync             (HyScanObjectModelPrivate  *priv);
static GHashTable * hyscan_object_model_make_ht                (void);
static void         hyscan_object_model_add_task               (HyScanObjectModel         *model,
                                                                const gchar               *id,
                                                                const HyScanObject        *object,
                                                                HyScanObjectModelAction    action);
static void         hyscan_object_model_free_task              (gpointer                   _task);
static void         hyscan_object_model_do_task                (gpointer                   data,
                                                                gpointer                   user_data);
static void         hyscan_object_model_do_all_tasks           (HyScanObjectModelPrivate  *priv,
                                                                HyScanObjectData          *data);
static GHashTable * hyscan_object_model_get_all_objects        (HyScanObjectModelPrivate  *priv,
                                                                HyScanObjectData          *data);
static gpointer     hyscan_object_model_processing             (HyScanObjectModelPrivate  *priv);
static gboolean     hyscan_object_model_signaller              (gpointer                   data);
                    
static guint        hyscan_object_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanObjectModel, hyscan_object_model, G_TYPE_OBJECT);

static void
hyscan_object_model_class_init (HyScanObjectModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_object_model_object_constructed;
  object_class->finalize = hyscan_object_model_object_finalize;
  object_class->set_property = hyscan_object_model_set_property;

  g_object_class_install_property (object_class, PROP_DATA_TYPE,
    g_param_spec_gtype ("data-type", "Object data type",
                        "The GType of HyScanObjectData inheritor", HYSCAN_TYPE_OBJECT_DATA,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanObjectModel::changed:
   * @model: указатель на #HyScanObjectModel
   *
   * Сигнал отправляется при изменениях в списке объектов.
   */
  hyscan_object_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_OBJECT_MODEL,
                  G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (HyScanObjectModelClass, changed), NULL, NULL,
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

  priv->stop = FALSE;
  priv->processing = g_thread_new ("object-model", (GThreadFunc) hyscan_object_model_processing, priv);
  priv->alerter = g_timeout_add (ALERT_INTERVAL_MS, hyscan_object_model_signaller, model);
}

static void
hyscan_object_model_object_finalize (GObject *object)
{
  HyScanObjectModel *model = HYSCAN_OBJECT_MODEL (object);
  HyScanObjectModelPrivate *priv = model->priv;

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

  G_OBJECT_CLASS (hyscan_object_model_parent_class)->finalize (object);
}

static void
hyscan_object_model_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanObjectModel *data = HYSCAN_OBJECT_MODEL (object);
  HyScanObjectModelPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DATA_TYPE:
      priv->data_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
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

/* Функция создаёт новое задание. */
static void
hyscan_object_model_add_task (HyScanObjectModel       *model,
                              const gchar             *id,
                              const HyScanObject      *object,
                              HyScanObjectModelAction  action)
{
  HyScanObjectDataClass *klass;
  HyScanObjectModelPrivate *priv = model->priv;
  HyScanObjectModelTask *task;

  klass = g_type_class_ref (priv->data_type);

  /* Создаём задание. */
  task = g_slice_new (HyScanObjectModelTask);
  task->action = action;
  task->id = (id != NULL) ? g_strdup (id) : NULL;
  task->object = hyscan_object_copy (object);

  g_type_class_unref (klass);

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
hyscan_object_model_free_task (gpointer _task)
{
  HyScanObjectModelTask *task = _task;

  hyscan_object_free (task->object);
  g_free (task->id);
  g_slice_free (HyScanObjectModelTask, task);
}

/* Функция выполняет задание. */
static void
hyscan_object_model_do_task (gpointer data,
                             gpointer user_data)
{
  HyScanObjectModelTask *task = data;
  HyScanObjectData *mdata = user_data;

  switch (task->action)
    {
    case OBJECT_ADD:
      if (!hyscan_object_data_add (mdata, task->object, NULL))
        g_warning ("Failed to add object");
      break;

    case OBJECT_MODIFY:
      if (!hyscan_object_data_modify (mdata, task->id, task->object))
        g_warning ("Failed to modify object <%s>", task->id);
      break;

    case OBJECT_REMOVE:
      if (!hyscan_object_data_remove (mdata, task->id))
        g_warning ("Failed to remove object <%s>", task->id);
      break;
    }
}

/* Функция выполняет все задания. */
static void
hyscan_object_model_do_all_tasks (HyScanObjectModelPrivate  *priv,
                                  HyScanObjectData          *data)
{
  GSList *tasks;

  /* Переписываем задания во временный список, чтобы
   * как можно меньше задерживать основной поток. */
  g_mutex_lock (&priv->tasks_lock);
  tasks = priv->tasks;
  priv->tasks = NULL;
  g_mutex_unlock (&priv->tasks_lock);

  /* Все задания отправляем в БД и очищаем наш временный список. */
  g_slist_foreach (tasks, hyscan_object_model_do_task, data);
  g_slist_foreach (tasks, (GFunc) hyscan_object_model_free_task, NULL);
  g_slist_free (tasks);
}

/* Функция забирает объекты из БД. */
static GHashTable *
hyscan_object_model_get_all_objects (HyScanObjectModelPrivate *priv,
                                     HyScanObjectData         *data)
{
  HyScanObject *object;
  GHashTable *object_list;
  gchar **id_list;
  guint len, i;

  /* Считываем список идентификаторов. Прошу обратить внимание, что
   * возврат хэш-таблицы с 0 элементов -- это нормальная ситуация, например,
   * если раньше был 1 объект, а потом его удалили. */
  object_list = hyscan_object_model_make_ht ();
  id_list = hyscan_object_data_get_ids (data, &len);

  /* Поштучно копируем из БД в хэш-таблицу. */
  for (i = 0; i < len; i++)
    {
      object = hyscan_object_data_get (data, id_list[i]);
      if (object == NULL)
        continue;

      g_hash_table_insert (object_list, g_strdup (id_list[i]), object);
    }

  g_strfreev (id_list);

  return object_list;
}

/* Поток асинхронной работы с БД. */
static gpointer
hyscan_object_model_processing (HyScanObjectModelPrivate *priv)
{
  HyScanObjectData *object_data = NULL; /* Объект работы с объектами. */
  GMutex im_lock;                       /* Мьютекс нужен для GCond. */
  guint32 oldmc, mc;                    /* Значения счётчика изменений. */

  oldmc = mc = 0;
  g_mutex_init (&im_lock);

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Ждём, когда нужно действовать. */
      if (object_data != NULL)
        mc = hyscan_object_data_get_mod_count (object_data);

      if (oldmc == mc && !g_atomic_int_get (&priv->im_flag))
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
          if (object_data != NULL)
            hyscan_object_model_do_all_tasks (priv, object_data);
          g_clear_object (&object_data);
        }

      if (object_data == NULL)
        {
          /* Создаём объект работы с объектами. */
          if (priv->cur_state.db != NULL && priv->cur_state.project != NULL)
            {
              object_data = hyscan_object_data_new (priv->data_type, priv->cur_state.db, priv->cur_state.project);

              if (!hyscan_object_data_is_ready (object_data))
                g_clear_object (&object_data);
            }

          /* Если не получилось (например потому, что проект ещё не создан),
           * повторим через некоторое время. */
          if (object_data == NULL)
            {
              g_atomic_int_set (&priv->im_flag, TRUE);
              g_usleep (CHECK_INTERVAL_US);
              continue;
            }

          mc = hyscan_object_data_get_mod_count (object_data);
        }

      /* Выполняем все задания. */
      hyscan_object_model_do_all_tasks (priv, object_data);

      /* Запоминаем мод_каунт перед(!) забором объектов. */
      oldmc = hyscan_object_data_get_mod_count (object_data);
      {
        GHashTable *object_list;        /* Объекты из параметров проекта БД. */
        GHashTable *temp;               /* Для обмена списка объектов. */

        object_list = hyscan_object_model_get_all_objects (priv, object_data);

        /* Воруем хэш-таблицу из привата, помещаем туда свежесозданную,
         * инициируем отправление сигнала. */
        g_mutex_lock (&priv->objects_lock);
        temp = priv->objects;
        priv->objects = g_hash_table_ref (object_list);
        priv->objects_changed = TRUE;
        g_mutex_unlock (&priv->objects_lock);

        /* Убираем свои ссылки на хэш-таблицы. */
        g_clear_pointer (&temp, g_hash_table_unref);
        g_clear_pointer (&object_list, g_hash_table_unref);
      }
    }

  g_clear_object (&object_data);
  g_mutex_clear (&im_lock);
  return NULL;
}

/* Функция эмитирует сигнал "changed", если есть изменения. */
static gboolean
hyscan_object_model_signaller (gpointer data)
{
  HyScanObjectModel *model = data;
  HyScanObjectModelPrivate *priv = model->priv;
  gboolean objects_changed;

  /* Если есть изменения, сигнализируем. */
  g_mutex_lock (&priv->objects_lock);
  objects_changed = priv->objects_changed;
  priv->objects_changed = FALSE;
  g_mutex_unlock (&priv->objects_lock);

  if (objects_changed)
    g_signal_emit (model, hyscan_object_model_signals[SIGNAL_CHANGED], 0, NULL);

  return G_SOURCE_CONTINUE;
}

/**
 * hyscan_object_model_new:
 * @data_type: тип класса для работы с параметрами проекта, наследник #HyScanObjectDataClass.
 *
 * Функция создаёт новый объект HyScanObjectModel. Класс работы с параметрами
 * должен быть наследником абстрактного класса  #HyScanObjectDataClass
 *
 * Returns: объект HyScanObjectModel.
 */
HyScanObjectModel*
hyscan_object_model_new (GType data_type)
{
  return g_object_new (HYSCAN_TYPE_OBJECT_MODEL,
                       "data-type", data_type, NULL);
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
 * hyscan_object_model_add_object:
 * @model: указатель на #HyScanObjectModel
 * @object: создаваемый объект.
 * 
 * Функция создает объект в параметрах проекта.
 *
 */
void
hyscan_object_model_add_object (HyScanObjectModel  *model,
                                const HyScanObject *object)
{
  g_return_if_fail (HYSCAN_IS_OBJECT_MODEL (model));
  g_return_if_fail (object != NULL);

  hyscan_object_model_add_task (model, NULL, object, OBJECT_ADD);
}

/**
 * hyscan_object_model_modify_object:
 * @model: указатель на #HyScanObjectModel
 * @id: идентификатор объекта
 * @object: новые значения.
 * 
 * Функция изменяет объект в параметрах проекта.
 * В результате этой функции все поля объекта будут перезаписаны.
 */
void
hyscan_object_model_modify_object (HyScanObjectModel  *model,
                                   const gchar        *id,
                                   const HyScanObject *object)
{
  g_return_if_fail (HYSCAN_IS_OBJECT_MODEL (model));
  g_return_if_fail (id != NULL && object != NULL);

  hyscan_object_model_add_task (model, id, object, OBJECT_MODIFY);
}

/**
 * hyscan_object_model_remove_object:
 * @model: указатель на #HyScanObjectModel
 * @id: идентификатор объекта.
 * 
 * Функция удаляет объект из параметров проекта.
 *
 */
void
hyscan_object_model_remove_object (HyScanObjectModel *model,
                                   const gchar       *id)
{
  g_return_if_fail (HYSCAN_IS_OBJECT_MODEL (model));
  g_return_if_fail (id != NULL);

  hyscan_object_model_add_task (model, id, NULL, OBJECT_REMOVE);
}

/**
 * hyscan_object_model_get:
 * @model: указатель на #HyScanObjectModel
 * 
 * Функция возвращает список объектов из внутреннего буфера.
 *
 * Returns: (transfer full): GHashTable, где ключом является идентификатор объекта,
 * а значением - структура #HyScanObject.
 */
GHashTable*
hyscan_object_model_get (HyScanObjectModel *model)
{
  HyScanObjectModelPrivate *priv;
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
  objects = hyscan_object_model_copy (model, priv->objects);
  g_mutex_unlock (&priv->objects_lock);

  return objects;
}

/**
 * hyscan_object_model_get_id:
 * @model: указатель на модель #HyScanObjectModel
 * @id: идентификатор объекта
 *
 * Функция возвращает объект из внутреннего буфера по его ID.
 *
 * Returns: (transfer full) (nullable): указатель на объект или %NULL, если объект не найден
 */
HyScanObject *
hyscan_object_model_get_id (HyScanObjectModel *model,
                            const gchar       *id)
{
  HyScanObjectModelPrivate *priv;
  HyScanObjectDataClass *data_class;
  HyScanObject *object, *copy = NULL;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_MODEL (model), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  priv = model->priv;

  g_mutex_lock (&priv->objects_lock);

  /* Проверяем, что объекты есть. */
  if (priv->objects == NULL)
    {
      g_mutex_unlock (&priv->objects_lock);
      return NULL;
    }

  /* Копируем объект. */
  data_class = g_type_class_ref (priv->data_type);

  object = g_hash_table_lookup (priv->objects, id);
  copy = hyscan_object_copy (object);
  g_mutex_unlock (&priv->objects_lock);

  g_type_class_unref (data_class);

  return copy;
}

/**
 * hyscan_object_model_copy:
 * @model: указатель на #HyScanObjectModel
 * @src: таблица объектов
 *
 * Вспомогательная функция для создания копии таблицы объектов.
 *
 * Returns: (transfer full): новая таблица, для удаления g_hash_table_unref().
 */
GHashTable *
hyscan_object_model_copy (HyScanObjectModel *model,
                          GHashTable        *src)
{
  HyScanObjectModelPrivate *priv;
  HyScanObjectDataClass *data_class;
  GHashTable *dst;
  GHashTableIter iter;
  gpointer k, v;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_MODEL (model), NULL);
  priv = model->priv;

  if (src == NULL)
    return NULL;

  dst = hyscan_object_model_make_ht ();
  data_class = g_type_class_ref (priv->data_type);

  /* Переписываем объекты. */
  g_hash_table_iter_init (&iter, src);
  while (g_hash_table_iter_next (&iter, &k, &v))
    g_hash_table_insert (dst, g_strdup (k), hyscan_object_copy (v));

  g_type_class_unref (data_class);

  return dst;
}
