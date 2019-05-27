/* hyscan-profile.h
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

#include "hyscan-profile.h"

enum
{
  PROP_0,
  PROP_FILE,
};

struct _HyScanProfilePrivate
{
  gchar    *file;
  GKeyFile *kf;
  gchar    *name;
};

static void     hyscan_profile_set_property       (GObject               *object,
                                                   guint                  prop_id,
                                                   const GValue          *value,
                                                   GParamSpec            *pspec);
static void     hyscan_profile_object_finalize    (GObject               *object);

static gboolean hyscan_profile_read_real          (HyScanProfile         *profile,
                                                   const gchar           *file);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfile, hyscan_profile, G_TYPE_OBJECT);

static void
hyscan_profile_class_init (HyScanProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_profile_set_property;
  oclass->finalize = hyscan_profile_object_finalize;

  g_object_class_install_property (oclass, PROP_FILE,
    g_param_spec_string ("file", "File", "Path to profile",
                         NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_profile_init (HyScanProfile *profile)
{
  profile->priv = hyscan_profile_get_instance_private (profile);
}

static void
hyscan_profile_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanProfile *self = HYSCAN_PROFILE (object);
  HyScanProfilePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_FILE:
      priv->file = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_profile_object_finalize (GObject *object)
{
  HyScanProfile *self = HYSCAN_PROFILE (object);
  HyScanProfilePrivate *priv = self->priv;

  g_clear_pointer (&priv->file, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->kf, g_key_file_unref);

  G_OBJECT_CLASS (hyscan_profile_parent_class)->finalize (object);
}

static gboolean
hyscan_profile_read_real (HyScanProfile *profile,
                          const gchar   *file)
{
  HyScanProfileClass *klass = HYSCAN_PROFILE_GET_CLASS (profile);
  HyScanProfilePrivate *priv = profile->priv;
  GError *error = NULL;
  gboolean status;

  priv->kf = g_key_file_new ();
  status = g_key_file_load_from_file (priv->kf, file, G_KEY_FILE_NONE, &error);

  if (!status && error->code != G_FILE_ERROR_NOENT)
    {
      g_warning ("HyScanProfile: can't load file <%s>", file);
      g_error_free (error);
      return FALSE;
    }

  if (klass->read == NULL)
    return FALSE;

  return klass->read (profile, priv->kf);
}

gboolean
hyscan_profile_read (HyScanProfile *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), FALSE);

  /* Текущая реализация запрещает читать дважды. */
  if (self->priv->kf != NULL)
    return FALSE;

  return hyscan_profile_read_real (self, self->priv->file);
}

void
hyscan_profile_set_name (HyScanProfile *self,
                         const gchar   *name)
{
  HyScanProfilePrivate *priv;

  g_return_if_fail (HYSCAN_IS_PROFILE (self));
  priv = self->priv;

  g_clear_pointer (&priv->name, g_free);
  priv->name = g_strdup (name);
}

const gchar *
hyscan_profile_get_name (HyScanProfile *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), NULL);

  return self->priv->name;
}

void
hyscan_profile_use (HyScanProfile *self)
{
  HyScanProfileClass * klass = HYSCAN_PROFILE_GET_CLASS (self);

  if (klass->use == NULL)
    {
      g_warning ("not implemented yet");
      return;
    }

  klass->use (self);
}
