/*
 * \file hyscan-serializable.c
 *
 * \brief Исходный файл интерфейса сериализации.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-serializable.h"

G_DEFINE_INTERFACE (HyScanSerializable, hyscan_serializable, G_TYPE_OBJECT)

static void
hyscan_serializable_default_init (HyScanSerializableInterface *iface)
{
}

/* Десериализует объект из файла. */
gboolean
hyscan_serializable_read (HyScanSerializable *serializable,
                          const gchar        *name)
{
  HyScanSerializableInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SERIALIZABLE (serializable), FALSE);

  iface = HYSCAN_SERIALIZABLE_GET_IFACE (serializable);
  if (iface->read != NULL)
    return (*iface->read) (serializable, name);

  return FALSE;
}

/* Сериализует объект в файл. */
gboolean
hyscan_serializable_write (HyScanSerializable  *serializable,
                           const gchar         *name)
{
  HyScanSerializableInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SERIALIZABLE (serializable), FALSE);

  iface = HYSCAN_SERIALIZABLE_GET_IFACE (serializable);
  if (iface->write != NULL)
    return (*iface->write) (serializable, name);

  return FALSE;
}
