#include "hyscan-hw-profile.h"
#include <hyscan-hw-profile-device.h>
#include <hyscan-driver.h>

#define HYSCAN_HW_PROFILE_INFO_GROUP "_"
#define HYSCAN_HW_PROFILE_NAME "name"

typedef struct
{
  gchar                 *group;
  HyScanHWProfileDevice *device;
} HyScanHWProfileItem;

struct _HyScanHWProfilePrivate
{
  gchar    **drivers;
  GList     *devices;
};

static void     hyscan_hw_profile_object_finalize         (GObject               *object);
static void     hyscan_hw_profile_item_free               (gpointer               data);
static void     hyscan_hw_profile_clear                   (HyScanHWProfile       *profile);
static gboolean hyscan_hw_profile_read                    (HyScanProfile         *profile,
                                                           GKeyFile              *file);
static gboolean hyscan_hw_profile_info_group              (HyScanProfile         *profile,
                                                           GKeyFile              *kf,
                                                           const gchar           *group);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanHWProfile, hyscan_hw_profile, HYSCAN_TYPE_PROFILE);

static void
hyscan_hw_profile_class_init (HyScanHWProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_hw_profile_object_finalize;
  pklass->read = hyscan_hw_profile_read;
}

static void
hyscan_hw_profile_init (HyScanHWProfile *profile)
{
  profile->priv = hyscan_hw_profile_get_instance_private (profile);
}

static void
hyscan_hw_profile_object_finalize (GObject *object)
{
  HyScanHWProfile *profile = HYSCAN_HW_PROFILE (object);
  HyScanHWProfilePrivate *priv = profile->priv;

  g_strfreev (priv->drivers);
  hyscan_hw_profile_clear (profile);

  G_OBJECT_CLASS (hyscan_hw_profile_parent_class)->finalize (object);
}

static void
hyscan_hw_profile_item_free (gpointer data)
{
  HyScanHWProfileItem *item = data;

  if (item == NULL)
    return;

  g_clear_pointer (&item->group, g_free);
  g_clear_object (&item->device);
  g_free (item);
}

static void
hyscan_hw_profile_clear (HyScanHWProfile *profile)
{
  HyScanHWProfilePrivate *priv = profile->priv;

  g_list_free_full (priv->devices, hyscan_hw_profile_item_free);
}

static gboolean
hyscan_hw_profile_read (HyScanProfile *profile,
                        GKeyFile      *file)
{
  HyScanHWProfile *self = HYSCAN_HW_PROFILE (profile);
  HyScanHWProfilePrivate *priv = self->priv;
  gchar **groups, **iter;

  /* Очищаем, если что-то было. */
  hyscan_hw_profile_clear (self);

  groups = g_key_file_get_groups (file, NULL);
  for (iter = groups; iter != NULL && *iter != NULL; ++iter)
    {
      HyScanHWProfileDevice * device;
      HyScanHWProfileItem *item;

      if (hyscan_hw_profile_info_group (profile, file, *iter))
        continue;

      device = hyscan_hw_profile_device_new (file);
      hyscan_hw_profile_device_set_group (device, *iter);
      hyscan_hw_profile_device_set_paths (device, priv->drivers);
      hyscan_hw_profile_device_read (device, file);

      item = g_new0 (HyScanHWProfileItem, 1);
      item->group = g_strdup (*iter);
      item->device = device;

      priv->devices = g_list_append (priv->devices, item);
    }

  g_strfreev (groups);

  return TRUE;
}

static gboolean
hyscan_hw_profile_info_group (HyScanProfile *profile,
                              GKeyFile      *kf,
                              const gchar   *group)
{
  gchar *name;

  if (!g_str_equal (group, HYSCAN_HW_PROFILE_INFO_GROUP))
    return FALSE;

  name = g_key_file_get_string (kf, group, HYSCAN_HW_PROFILE_NAME, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), name);

  g_free (name);
  return TRUE;
}

HyScanHWProfile *
hyscan_hw_profile_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_HW_PROFILE,
                       "file", file,
                       NULL);
}

void
hyscan_hw_profile_set_driver_paths (HyScanHWProfile  *self,
                                    gchar           **driver_paths)
{
  g_return_if_fail (HYSCAN_IS_HW_PROFILE (self));

  g_strfreev (self->priv->drivers);
  self->priv->drivers = g_strdupv (driver_paths);
}

GList *
hyscan_hw_profile_list (HyScanHWProfile *self)
{
  GList *list = NULL;
  GList *link;
  HyScanHWProfilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_HW_PROFILE (self), NULL);
  priv = self->priv;

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWProfileItem *src = link->data;
      HyScanHWProfileItem *dst = g_new0 (HyScanHWProfileItem, 1);
      dst->group = g_strdup (src->group);
      dst->device = g_object_ref (src->device);

      list = g_list_append (list, dst);
    }

  return list;
}

gboolean
hyscan_hw_profile_check (HyScanHWProfile *self)
{
  HyScanHWProfilePrivate *priv;
  GList *link;
  gboolean st = TRUE;

  g_return_val_if_fail (HYSCAN_IS_HW_PROFILE (self), FALSE);
  priv = self->priv;

  if (priv->devices == NULL)
    return FALSE;

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWProfileItem *item = link->data;
      st &= hyscan_hw_profile_device_check (item->device);
    }

  return st;
}

HyScanControl *
hyscan_hw_profile_connect (HyScanHWProfile *self)
{
  HyScanHWProfilePrivate *priv;
  HyScanControl * control;
  GList *link;

  g_return_val_if_fail (HYSCAN_IS_HW_PROFILE (self), NULL);
  priv = self->priv;

  if (priv->devices == NULL)
    return FALSE;

  control = hyscan_control_new ();

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWProfileItem *item = link->data;
      HyScanDevice *device;

      device = hyscan_hw_profile_device_connect (item->device);

      if (device == NULL)
        {
          g_warning ("couldn't connect to device");
          g_clear_object (&control);
          break;
        }

      if (!hyscan_control_device_add (control, device))
        {
          g_warning ("couldn't add device");
          g_clear_object (&control);
          break;
        }
    }

  return control;
}

HyScanControl *
hyscan_hw_profile_connect_simple (const gchar *file)
{
  HyScanHWProfile * profile;
  HyScanControl * control;

  profile = hyscan_hw_profile_new (file);
  if (profile == NULL)
    {
      return NULL;
    }

  if (!hyscan_hw_profile_check (profile))
    {
      g_object_unref (profile);
      return NULL;
    }

  control = hyscan_hw_profile_connect (profile);
  hyscan_control_device_bind (control);

  g_object_unref (profile);
  return control;
}
