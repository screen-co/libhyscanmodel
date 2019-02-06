/* hyscan-profile-loader.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#include "hyscan-profile-loader.h"

/* По умолчанию я буду искать в папке hyscan. Это можно переопределить на
 * этапе компиляции для standalone сборок. */
#ifndef HYSCAN_PROFILE_LOADER_PATH
# define HYSCAN_PROFILE_LOADER_PATH "hyscan"
#endif

enum
{
  PROP_0,
  PROP_SUBFOLDER,
  PROP_SYSFOLDER,
  PROP_TYPE
};

struct _HyScanProfileLoaderPrivate
{
  GType       type;
  gchar      *subfolder;
  gchar      *sysfolder;

  GHashTable *profiles; /* {gchar* file_name : HyScanProfile*} */
};

static void    hyscan_profile_loader_set_property             (GObject               *object,
                                                               guint                  prop_id,
                                                               const GValue          *value,
                                                               GParamSpec            *pspec);
static void    hyscan_profile_loader_object_constructed       (GObject               *object);
static void    hyscan_profile_loader_object_finalize          (GObject               *object);
static void    hyscan_profile_loader_load                     (HyScanProfileLoader   *self,
                                                               const gchar           *dir);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileLoader, hyscan_profile_loader, G_TYPE_OBJECT);

static void
hyscan_profile_loader_class_init (HyScanProfileLoaderClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_profile_loader_set_property;
  oclass->constructed = hyscan_profile_loader_object_constructed;
  oclass->finalize = hyscan_profile_loader_object_finalize;

  g_object_class_install_property (oclass, PROP_SUBFOLDER,
    g_param_spec_string ("subfolder", "Subfolder", "Subfolder to look for profiles", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_SYSFOLDER,
    g_param_spec_string ("sysfolder", "SysFolder", "Folder with system profiles", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_TYPE,
    g_param_spec_gtype ("type", "Type", "Type of profile objects to create", HYSCAN_TYPE_PROFILE,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_profile_loader_init (HyScanProfileLoader *profile_loader)
{
  profile_loader->priv = hyscan_profile_loader_get_instance_private (profile_loader);
}

static void
hyscan_profile_loader_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanProfileLoader *self = HYSCAN_PROFILE_LOADER (object);
  HyScanProfileLoaderPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_SUBFOLDER:
      priv->subfolder = g_value_dup_string (value);
      break;

    case PROP_SYSFOLDER:
      priv->sysfolder = g_value_dup_string (value);
      break;

    case PROP_TYPE:
      priv->type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_profile_loader_object_constructed (GObject *object)
{
  HyScanProfileLoader *self = HYSCAN_PROFILE_LOADER (object);
  HyScanProfileLoaderPrivate *priv = self->priv;
  gchar *folder;

  priv->profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  /* Профили загружаются из двух каталогов. */
  folder = g_build_filename (priv->sysfolder,
                             HYSCAN_PROFILE_LOADER_PATH,
                             priv->subfolder, NULL);
  hyscan_profile_loader_load (self, folder);
  g_free (folder);

  folder = g_build_filename (g_get_user_config_dir (),
                             HYSCAN_PROFILE_LOADER_PATH,
                             priv->subfolder, NULL);
  hyscan_profile_loader_load (self, folder);
  g_free (folder);
}

static void
hyscan_profile_loader_object_finalize (GObject *object)
{
  HyScanProfileLoader *self = HYSCAN_PROFILE_LOADER (object);
  HyScanProfileLoaderPrivate *priv = self->priv;

  g_clear_pointer (&priv->subfolder, g_free);
  g_clear_pointer (&priv->sysfolder, g_free);
  g_clear_pointer (&priv->profiles, g_hash_table_unref);

  G_OBJECT_CLASS (hyscan_profile_loader_parent_class)->finalize (object);
}

static void
hyscan_profile_loader_load (HyScanProfileLoader *self,
                            const gchar         *folder)
{
  HyScanProfileLoaderPrivate *priv = self->priv;
  const gchar *filename;
  GError *error = NULL;
  GDir *dir;

  dir = g_dir_open (folder, 0, &error);
  if (error != NULL)
    {
      g_warning ("HyScanProfileLoader: %s", error->message);
      g_clear_pointer (&error, g_error_free);
      return;
    }

  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      gchar *fullname;
      HyScanProfile *profile;

      fullname = g_build_filename (folder, filename, NULL);
      profile = g_object_new (priv->type, "file", fullname, NULL);

      if (profile != NULL)
        g_hash_table_insert (priv->profiles, g_strdup (fullname), profile);

      g_free (fullname);
    }

  g_dir_close (dir);
}

HyScanProfileLoader *
hyscan_profile_loader_new (const gchar *subfolder,
                           const gchar *systemfolder,
                           GType        profile_type)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_LOADER,
                       "subfolder", subfolder,
                       "sysfolder", systemfolder,
                       "type", profile_type,
                       NULL);
}

GHashTable *
hyscan_profile_loader_list (HyScanProfileLoader *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_LOADER (self), NULL);

  return g_hash_table_ref (self->priv->profiles);
}
