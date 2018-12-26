#include "hyscan-hw-profile.h"
#include <hyscan-hw-profile-device.h>
#include <hyscan-driver.h>

enum
{
  PROP_0,
  PROP_FILE_NAME
};

typedef struct
{
  gchar                 *group;
  HyScanHWProfileDevice *device;
  gboolean               check_status;
} HyScanHWProfileItem;

struct _HyScanHWProfilePrivate
{
  gchar     *file_name;
  GKeyFile  *kf;

  gchar    **drivers;
  GList     *devices;
};

static void    hyscan_hw_profile_set_property             (GObject               *object,
                                                           guint                  prop_id,
                                                           const GValue          *value,
                                                           GParamSpec            *pspec);
static void    hyscan_hw_profile_object_constructed       (GObject               *object);
static void    hyscan_hw_profile_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanHWProfile, hyscan_hw_profile, G_TYPE_OBJECT);

static void
hyscan_hw_profile_class_init (HyScanHWProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_hw_profile_set_property;
  oclass->constructed = hyscan_hw_profile_object_constructed;
  oclass->finalize = hyscan_hw_profile_object_finalize;

  g_object_class_install_property (oclass, PROP_FILE_NAME,
    g_param_spec_string ("file", "FilePath", "Full path to file", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_hw_profile_init (HyScanHWProfile *hw_profile)
{
  hw_profile->priv = hyscan_hw_profile_get_instance_private (hw_profile);
}

static void
hyscan_hw_profile_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanHWProfile *hw_profile = HYSCAN_HW_PROFILE (object);
  HyScanHWProfilePrivate *priv = hw_profile->priv;

  switch (prop_id)
    {
    case PROP_FILE_NAME:
      priv->file_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_hw_profile_object_constructed (GObject *object)
{
  HyScanHWProfile *hw_profile = HYSCAN_HW_PROFILE (object);
  HyScanHWProfilePrivate *priv = hw_profile->priv;
  GKeyFileFlags kf_flags;
  gboolean st;

  G_OBJECT_CLASS (hyscan_hw_profile_parent_class)->constructed (object);

  /* Загружаем файл профиля. */
  priv->kf = g_key_file_new ();

  kf_flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
  st = g_key_file_load_from_file (priv->kf, priv->file_name, kf_flags, NULL);

  if (!st)
    g_clear_pointer (&priv->kf, g_key_file_unref);

}

static void
hyscan_hw_profile_object_finalize (GObject *object)
{
  HyScanHWProfile *hw_profile = HYSCAN_HW_PROFILE (object);
  HyScanHWProfilePrivate *priv = hw_profile->priv;

  g_key_file_unref (priv->kf);
  g_free (priv->file_name);
  g_strfreev (priv->drivers);

  G_OBJECT_CLASS (hyscan_hw_profile_parent_class)->finalize (object);
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

void
hyscan_hw_profile_read (HyScanHWProfile *self)
{
 gchar **groups, **iter;
 HyScanHWProfilePrivate *priv;

 g_return_if_fail (HYSCAN_IS_HW_PROFILE (self));
 priv = self->priv;

 groups = g_key_file_get_groups (priv->kf, NULL);

 for (iter = groups; iter != NULL && *iter != NULL; ++iter)
   {
     HyScanHWProfileDevice * device;
     HyScanHWProfileItem *item;

     device = hyscan_hw_profile_device_new (priv->kf);
     hyscan_hw_profile_device_set_group (device, *iter);
     hyscan_hw_profile_device_set_paths (device, priv->drivers);
     hyscan_hw_profile_device_read (device, priv->kf);

     item = g_new0 (HyScanHWProfileItem, 1);
     item->group = g_strdup (*iter);
     item->device = device;

     priv->devices = g_list_append (priv->devices, item);
   }
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

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWProfileItem *item = link->data;
      st &= item->check_status = hyscan_hw_profile_device_check (item->device);
    }

  return st;
}

HyScanControl *
hyscan_hw_profile_connect (HyScanHWProfile *self)
{
  HyScanHWProfilePrivate *priv;
  HyScanControl * control;
  GList *link;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_HW_PROFILE (self), NULL);
  priv = self->priv;

  control = hyscan_control_new ();

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWProfileItem *item = link->data;
      HyScanDevice *device;

      if (!item->check_status)
        g_warning ("check was not performed or failed");

      device = hyscan_hw_profile_device_connect (item->device);

      if (device != NULL)
        {
          status = hyscan_control_device_add (control, device);

          if (!status)
            g_warning ("couldn't add device");
        }
    }

  hyscan_control_device_bind (control);

  return control;
}

HyScanControl *
hyscan_hw_profile_connect_simple (const gchar *file)
{
  HyScanHWProfile * profile;
  HyScanControl * control;

  profile = hyscan_hw_profile_new (file);
  if (profile == NULL)
    return NULL;

  hyscan_hw_profile_read (profile);
  if (!hyscan_hw_profile_check (profile))
    {
      g_object_unref (profile);
      return NULL;
    }

  control = hyscan_hw_profile_connect (profile);
  g_object_unref (profile);

  return control;
}
