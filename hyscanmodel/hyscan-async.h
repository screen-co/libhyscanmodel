/**
 * \file hyscan-async.h
 *
 * \brief Заголовочный файл класса HyScanAsync, выполняющего список запросов в отдельном потоке.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanAsync HyScanAsync - выполнение списка запросов в отдельном потоке.
 *
 * Порядок работы с классом:
 * 1. Добавить запросы в список функцией #hyscan_async_append_query.
 * 2. Запустить выполнение запросов функцией #hyscan_async_execute.
 * 3. В случае успешного запуска, испускается сигнал "started".
 * 4. После выполнения списка запросов, испускается сигнал "compeleted".
 *
 * Прототип обработчика сигнала "started":
 * \code
 * void started_cb (HyScanAsync *async,
 *                  gpointer     user_data);
 * \endcode
 *
 * Прототип обработчика сигнала "completed":
 * \code
 * void compelted_cb (HyScanAsync *async,
 *                    gboolean     result,
 *                    gpointer     user_data);
 * \endcode
 * где:
 * - result - результат выполнения запросов.
 *
 * Сигнал "completed" вернет в результате TRUE только в случае, если все запросы будут успешно
 * выполнены (т.е. команды вернут TRUE). Если команда из списка вернёт FALSE, выполнение запросов
 * прекращается, результатом выполнения запросов будет FALSE.
 *
 */

#ifndef __HYSCAN_ASYNC_H__
#define __HYSCAN_ASYNC_H__

#include <glib-object.h>
#include <hyscan-api.h>

/** Асинхронная команда. */
typedef gboolean (*HyScanAsyncCommand) (gpointer object, gpointer data);

G_BEGIN_DECLS

#define HYSCAN_TYPE_ASYNC             (hyscan_async_get_type ())
#define HYSCAN_ASYNC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_ASYNC, HyScanAsync))
#define HYSCAN_IS_ASYNC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_ASYNC))
#define HYSCAN_ASYNC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_ASYNC, HyScanAsyncClass))
#define HYSCAN_IS_ASYNC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_ASYNC))
#define HYSCAN_ASYNC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_ASYNC, HyScanAsyncClass))

typedef struct _HyScanAsync HyScanAsync;
typedef struct _HyScanAsyncPrivate HyScanAsyncPrivate;
typedef struct _HyScanAsyncClass HyScanAsyncClass;

struct _HyScanAsync
{
  GObject parent_instance;

  HyScanAsyncPrivate *priv;
};

struct _HyScanAsyncClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType        hyscan_async_get_type      (void);

/**
 * Создаёт новый объект \link HyScanAsync \endlink.
 *
 * \return указатель на класс \link HyScanAsync \endlink.
 */
HYSCAN_API
HyScanAsync *hyscan_async_new           (void);

/**
 * Добавляет запрос в список.
 * Данная функция создаёт копию данных, переданных в data, которые удаляются
 * после выполнения запроса. Если данные data содержат указатели на динамически
 * распеределенную память, её необходимо самостоятельно освободить после
 * выполнения запроса.
 *
 * \param async указатель на класс \link HyScanAsync \endlink;
 * \param command указатель на функцию типа \link HyScanAsyncCommand \endlink;
 * \param object первый параметр, передаваемый в HyScanAsyncCommand;
 * \param data второй параметр, передаваемый в HyScanAsyncCommand;
 * \param data_size размер данных data.
 *
 * \return TRUE, если удалось добавить запрос, либо FALSE, если произошла ошибка.
 */
HYSCAN_API
gboolean     hyscan_async_append_query  (HyScanAsync        *async,
                                         HyScanAsyncCommand  command,
                                         gpointer            object,
                                         gconstpointer       data,
                                         gsize               data_size);

/**
 * Запускает выполнение списка запросов в отдельном потоке.
 *
 * \param async указатель на класс \link HyScanAsync \endlink.
 *
 * \return TRUE, если удалось запустить выполнение запросов, либо FALSE,
 * если список запросов пуст, запросы ещё выполняются, или в случае ошибки.
 */
HYSCAN_API
gboolean     hyscan_async_execute       (HyScanAsync    *async);

G_END_DECLS

#endif /* __HYSCAN_ASYNC_H__ */
