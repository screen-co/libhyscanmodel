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
  gchar * file;
  gchar * name;
};

static void     hyscan_profile_set_property            (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void     hyscan_profile_object_constructed      (GObject               *object);
static void     hyscan_profile_object_finalize         (GObject               *object);

static gboolean hyscan_profile_read                    (HyScanProfile         *profile,
                                                        const gchar           *file);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfile, hyscan_profile, G_TYPE_OBJECT);

static void
hyscan_profile_class_init (HyScanProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_profile_set_property;
  oclass->constructed = hyscan_profile_object_constructed;
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
hyscan_profile_object_constructed (GObject *object)
{
  HyScanProfile *self = HYSCAN_PROFILE (object);
  HyScanProfilePrivate *priv = self->priv;

  hyscan_profile_read (self, priv->file);
}

static void
hyscan_profile_object_finalize (GObject *object)
{
  HyScanProfile *self = HYSCAN_PROFILE (object);
  HyScanProfilePrivate *priv = self->priv;

  g_clear_pointer (&priv->file, g_free);
  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (hyscan_profile_parent_class)->finalize (object);
}

static gboolean
hyscan_profile_read (HyScanProfile *profile,
                     const gchar   *file)
{
  HyScanProfileClass * klass = HYSCAN_PROFILE_GET_CLASS (profile);

  if (klass->read == NULL)
    return FALSE;

  return klass->read (profile, file);
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
