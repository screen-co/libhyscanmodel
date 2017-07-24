/**
 * \file hyscan-db-info.h
 *
 * \brief Заголовочный файл класса асинхронного мониторинга базы данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanDBInfo HyScanDBInfo - класс асинхронного мониторинга базы данных
 *
 * Класс предназначен для асинхронного отслеживания изменений в базе данных и получения
 * информации о проектах, галсах и источниках данных в них. Отправка уведомлений об
 * изменениях производится из основного цикла GMainLoop, таким образом для корректной
 * работы класса требуется вызвать функции g_main_loop_run или gtk_main.
 *
 * Создание объекта производится с помощью функции #hyscan_db_info_new.
 *
 * Основные функции класса работают в неблокирующем режиме. При этом возможна ситуация, что
 * текущие списки проектов или галсов будут отличаться от содержимого базы данных. Рекомендуется
 * получать списки проектов и галсов при получении уведомлений об изменениях.
 *
 * При изменениях в базе данных отправляются сигналы:
 *
 * - "projects-changed" - при изменении списка проектов;
 * - "tracks-changed" - при изменении списка галсов или источников данных в них.
 *
 * Прототип обработчиков этих сигналов:
 *
 * \code
 *
 * void    changed_cb  (HyDBInfo    *info,
 *                      gpointer     user_data);
 *
 * \endcode
 *
 * Получить текущий список проектов можно при помощи функции #hyscan_db_info_get_projects. Функции
 * #hyscan_db_info_set_project и #hyscan_db_info_get_project используются для установки и чтения
 * названия текущего проекта, для которого отслеживаются изменения. Получить список галсов можно
 * при помощи функции #hyscan_db_info_get_tracks.
 *
 * Функция #hyscan_db_info_refresh используется для полного обновления списка проектов, галсов
 * и информации о них.
 *
 * Вспомогательные функции #hyscan_db_info_get_project_info и #hyscan_db_info_get_track_info
 * используются для получения информации о проекте и галсе. Для их работы не требуется создавать
 * экземпляр класса и они блокируют выполнение программы на время работы с базой данных. Эти функции
 * могут быть полезны при создании программ не предъявляющих жёстких требований к отзывчивости.
 *
 * Функции #hyscan_db_info_free_project_info и #hyscan_db_info_free_track_info используются для
 * освобождения памяти используемой структурами \link HyScanProjectInfo \endlink и
 * \link HyScanTrackInfo \endlink.
 *
 */

#ifndef __HYSCAN_DB_INFO_H__
#define __HYSCAN_DB_INFO_H__

#include <hyscan-db.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

/** \brief Информация о проекте */
typedef struct
{
  gchar               *name;                   /**< Название проекта. */
  GDateTime           *ctime;                  /**< Дата и время создания проекта. */
  gchar               *description;            /**< Описание проекта. */
} HyScanProjectInfo;

/** \brief Информация о галсе */
typedef struct
{
  gchar               *name;                   /**< Название галса. */
  GDateTime           *ctime;                  /**< Дата и время создания галса. */
  gchar               *description;            /**< Описание галса. */

  gchar               *id;                     /**< Уникальный идентификатор галса. */
  HyScanTrackType      type;                   /**< Тип галса. */
  gchar               *operator_name;          /**< Имя оператора локатора записавшего галс. */
  HyScanDataSchema    *sonar_info;             /**< Общая информация о гидролокаторе. */

  GHashTable          *sources;                /**< Список источников данных. Хэш таблица, в которой ключом
                                                    является идентификатор источника данных GINT_TO_POINTER (source),
                                                    а значением указатель на структуру \link HyScanSourceInfo \endlink.
                                                    Пользователь не должен модифицировать данные в этих структурах. */
  gboolean             active;                 /**< Признак записи данных в галс. */
} HyScanTrackInfo;

/** \brief Информация об источнике данных */
typedef struct
{
  HyScanSourceType     type;                   /**< Тип источника данных. */
  gboolean             computed;               /**< Признак наличия обработанных данных. */
  gboolean             raw;                    /**< Признак наличия сырых данных. */
  gboolean             active;                 /**< Признак записи данных от источника. */
} HyScanSourceInfo;

#define HYSCAN_TYPE_DB_INFO             (hyscan_db_info_get_type ())
#define HYSCAN_DB_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DB_INFO, HyScanDBInfo))
#define HYSCAN_IS_DB_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DB_INFO))
#define HYSCAN_DB_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DB_INFO, HyScanDBInfoClass))
#define HYSCAN_IS_DB_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DB_INFO))
#define HYSCAN_DB_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DB_INFO, HyScanDBInfoClass))

typedef struct _HyScanDBInfo HyScanDBInfo;
typedef struct _HyScanDBInfoPrivate HyScanDBInfoPrivate;
typedef struct _HyScanDBInfoClass HyScanDBInfoClass;

struct _HyScanDBInfo
{
  GObject parent_instance;

  HyScanDBInfoPrivate *priv;
};

struct _HyScanDBInfoClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_db_info_get_type                 (void);

/**
 *
 * Функция создаёт новый объект \link HyScanDBInfo \endlink.
 *
 * \param db указатель на \link HyScanDB \endlink.
 *
 * \return Указатель на новый объект \link HyScanDBInfo \endlink.
 *
 */
HYSCAN_API
HyScanDBInfo          *hyscan_db_info_new                      (HyScanDB              *db);

/**
 *
 * Функция устанавливает название проекта для которого будут отслеживаться изменения
 * в списке галсов и источниках данных. Реальное изменение отслеживаемого проекта
 * может происходить с некоторой задержкой.
 *
 * Функцию можно безопасно вызывать в главном потоке GUI.
 *
 * \param info указатель на \link HyScanDBInfo \endlink;
 * \param project_name название проекта.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_db_info_set_project              (HyScanDBInfo          *info,
                                                                const gchar           *project_name);

/**
 *
 * Функция считывает название проекта для которого в данный момент отслеживаются
 * изменения. Пользователь должен освободить строку с названием проекта с помощью
 * функции g_free ();
 *
 * Функцию можно безопасно вызывать в главном потоке GUI.
 *
 * \param info указатель на \link HyScanDBInfo \endlink.
 *
 * \return Название текущего отслеживаемого проекта.
 *
 */
HYSCAN_API
gchar                 *hyscan_db_info_get_project              (HyScanDBInfo          *info);

/**
 *
 * Функция возвращает список проектов и информацию о них. Информация представлена
 * в виде хэш таблицы в которой ключом является название проекта, а значением
 * указатель на структуру \link HyScanProjectInfo \endlink. Пользователь не должен
 * модифицировать данные в этих структурах. После использования пользователь должен
 * освободить память, занимаемую списком, с помощью функции g_hash_table_unref.
 *
 * Функцию можно безопасно вызывать в главном потоке GUI.
 *
 * \param info указатель на \link HyScanDBInfo \endlink.
 *
 * \return Текущий список проектов.
 *
 */
HYSCAN_API
GHashTable            *hyscan_db_info_get_projects             (HyScanDBInfo          *info);

/**
 *
 * Функция возвращает список галсов и информацию о них для текущего отслеживаемого
 * проекта. Информация представлена в виде хэш таблицы в которой ключом является
 * название галса, а значением указатель на структуру \link HyScanTrackInfo \endlink.
 * Пользователь не должен модифицировать данные в этих структурах. После использования
 * пользователь должен освободить память, занимаемую списком, с помощью функции
 * g_hash_table_unref.
 *
 * Функцию можно безопасно вызывать в главном потоке GUI.
 *
 * \param info указатель на \link HyScanDBInfo \endlink.
 *
 * \return Текущий список галсов.
 *
 */
HYSCAN_API
GHashTable            *hyscan_db_info_get_tracks               (HyScanDBInfo          *info);

/**
 *
 * Функция принудительно запускает процесс обновления списков проектов, галсов и информации о них.
 *
 * \param info указатель на \link HyScanDBInfo \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_db_info_refresh                  (HyScanDBInfo          *info);

/**
 *
 * Функция возвращает информацию о проекте. После использования пользователь должен
 * освободить память, занимаемую информацией, функцией #hyscan_db_info_free_project_info.
 *
 * \param db указатель на \link HyScanDB \endlink.
 * \param project_name название проекта.
 *
 * \return Информация о проекте \link HyScanProjectInfo \endlink.
 *
 */
HYSCAN_API
HyScanProjectInfo     *hyscan_db_info_get_project_info         (HyScanDB              *db,
                                                                const gchar           *project_name);

/**
 *
 * Функция возвращает информацию о галсе. После использования пользователь должен
 * освободить память, занимаемую информацией, функцией #hyscan_db_info_free_track_info.
 * Проект должен быть открыт или создан с помощью функции
 * \link hyscan_db_project_open \endlink или \link hyscan_db_project_create \endlink.
 *
 * \param db указатель на \link HyScanDB \endlink.
 * \param project_id идентификатор открытого проекта;
 * \param track_name название галса.
 *
 * \return Информация о галсе \link HyScanTrackInfo \endlink.
 *
 */
HYSCAN_API
HyScanTrackInfo       *hyscan_db_info_get_track_info           (HyScanDB              *db,
                                                                gint32                 project_id,
                                                                const gchar           *track_name);

/**
 *
 * Функция освобождает память занятую структурой \link HyScanProjectInfo \endlink.
 *
 * \param info указатель на \link HyScanProjectInfo \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_db_info_free_project_info        (HyScanProjectInfo     *info);

/**
 *
 * Функция освобождает память занятую структурой \link HyScanTrackInfo \endlink.
 *
 * \param info указатель на \link HyScanTrackInfo \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_db_info_free_track_info          (HyScanTrackInfo       *info);

G_END_DECLS

#endif /* __HYSCAN_DB_INFO_H__ */
