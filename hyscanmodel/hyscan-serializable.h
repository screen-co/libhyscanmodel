/**
 * \file hyscan-serializable.h
 *
 * \brief Заголовочный файл интерфейса сериализации.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSerializable HyScanSerializable - интерфейс сериализации.
 *
 * Интерфейс сериализации объектов.
 *
 * Сериализация осуществляется с помощью функций:
 * - #hyscan_serializable_read - десериализует объект из файла;
 * - #hyscan_serializable_write - сериализует объект в файл.
 */

#ifndef __HYSCAN_SERIALIZABLE_H__
#define __HYSCAN_SERIALIZABLE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SERIALIZABLE            \
        (hyscan_serializable_get_type ())
#define HYSCAN_SERIALIZABLE(obj)            \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SERIALIZABLE, HyScanSerializable))
#define HYSCAN_IS_SERIALIZABLE(obj)         \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SERIALIZABLE))
#define HYSCAN_SERIALIZABLE_GET_IFACE(obj)  \
        (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SERIALIZABLE, HyScanSerializableInterface))

typedef struct _HyScanSerializable HyScanSerializable;
typedef struct _HyScanSerializableInterface HyScanSerializableInterface;

struct _HyScanSerializableInterface
{
  GTypeInterface   g_iface;

  gboolean         (*read)   (HyScanSerializable  *serializable,
                              const gchar         *name);

  gboolean         (*write)  (HyScanSerializable  *serializable,
                              const gchar         *name);
};

HYSCAN_API
GType     hyscan_serializable_get_type  (void);

/**
 * Десериализует объект из файла.
 *
 * \param serializable указатель на интерфейс \link HyScanSerializable \endlink;
 * \param name имя файла.
 *
 * \return TRUE, если объект десериализован, в противном случае - FALSE.
 */
HYSCAN_API
gboolean  hyscan_serializable_read      (HyScanSerializable  *serializable,
                                         const gchar         *name);

/**
 * Cериализует объект в файл.
 *
 * \param serializable указатель на интерфейс \link HyScanSerializable \endlink;
 * \param name имя файла.
 *
 * \return TRUE, если объект сериализован, в противном случае - FALSE.
 */
HYSCAN_API
gboolean  hyscan_serializable_write     (HyScanSerializable  *serializable,
                                         const gchar         *name);

G_END_DECLS

#endif /* __HYSCAN_SERIALIZABLE_H__ */
