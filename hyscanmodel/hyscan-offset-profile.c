/*
 * \file hyscan-db-profile.c
 *
 * \brief Исходный файл класса HyScanOffsetProfile - профиля БД.
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2018
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <gio/gio.h>
#include "hyscan-offset-profile.h"

#define HYSCAN_OFFSET_PROFILE_INFO_GROUP "_"
#define HYSCAN_OFFSET_PROFILE_NAME "name"

#define HYSCAN_OFFSET_PROFILE_X "x"
#define HYSCAN_OFFSET_PROFILE_Y "y"
#define HYSCAN_OFFSET_PROFILE_Z "z"
#define HYSCAN_OFFSET_PROFILE_PSI "psi"
#define HYSCAN_OFFSET_PROFILE_GAMMA "gamma"
#define HYSCAN_OFFSET_PROFILE_THETA "theta"

struct _HyScanOffsetProfilePrivate
{
  GHashTable *sonars;  /* Таблица смещений для локаторов. {HyScanSourceType : HyScanAntennaOffset} */
  GHashTable *sensors; /* Таблица смещений для антенн. {gchar* : HyScanAntennaOffset} */
};

static void     hyscan_offset_profile_object_finalize  (GObject             *object);
static void     hyscan_offset_profile_clear            (HyScanOffsetProfile *profile);
static gboolean hyscan_offset_profile_read             (HyScanProfile       *profile,
                                                        GKeyFile            *file);
static gboolean hyscan_offset_profile_info_group       (HyScanOffsetProfile *profile,
                                                        GKeyFile            *kf,
                                                        const gchar         *group);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanOffsetProfile, hyscan_offset_profile, HYSCAN_TYPE_PROFILE);

static void
hyscan_offset_profile_class_init (HyScanOffsetProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_offset_profile_object_finalize;
  pklass->read = hyscan_offset_profile_read;
}

static void
hyscan_offset_profile_init (HyScanOffsetProfile *profile)
{
  profile->priv = hyscan_offset_profile_get_instance_private (profile);
}

static void
hyscan_offset_profile_object_finalize (GObject *object)
{
  HyScanOffsetProfile *self = HYSCAN_OFFSET_PROFILE (object);

  hyscan_offset_profile_clear (self);

  G_OBJECT_CLASS (hyscan_offset_profile_parent_class)->finalize (object);
}

static void
hyscan_offset_profile_clear (HyScanOffsetProfile *profile)
{
  HyScanOffsetProfilePrivate *priv = profile->priv;

  g_clear_pointer (&priv->sonars, g_hash_table_unref);
  g_clear_pointer (&priv->sensors, g_hash_table_unref);

  priv->sonars = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL, (GDestroyNotify)hyscan_antenna_offset_free);
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify)hyscan_antenna_offset_free);
}

/* Создаёт объект HyScanOffsetProfile. */
HyScanOffsetProfile *
hyscan_offset_profile_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_OFFSET_PROFILE,
                       "file", file,
                       NULL);
}

/* Десериализация из INI-файла. */
static gboolean
hyscan_offset_profile_read (HyScanProfile *profile,
                            GKeyFile      *file)
{
  HyScanOffsetProfile *self = HYSCAN_OFFSET_PROFILE (profile);
  HyScanOffsetProfilePrivate *priv = self->priv;
  gchar **groups, **iter;

  /* Очищаем, если что-то было. */
  hyscan_offset_profile_clear (self);

  groups = g_key_file_get_groups (file, NULL);
  for (iter = groups; iter != NULL && *iter != NULL; ++iter)
    {
      HyScanAntennaOffset offset;
      HyScanSourceType source;
      HyScanChannelType type;
      guint channel;

      /* Возможно, это группа с информацией. */
      if (hyscan_offset_profile_info_group (self, file, *iter))
        continue;

      offset.x = g_key_file_get_double (file, *iter, HYSCAN_OFFSET_PROFILE_X, NULL);
      offset.y = g_key_file_get_double (file, *iter, HYSCAN_OFFSET_PROFILE_Y, NULL);
      offset.z = g_key_file_get_double (file, *iter, HYSCAN_OFFSET_PROFILE_Z, NULL);
      offset.psi = g_key_file_get_double (file, *iter, HYSCAN_OFFSET_PROFILE_PSI, NULL);
      offset.gamma = g_key_file_get_double (file, *iter, HYSCAN_OFFSET_PROFILE_GAMMA, NULL);
      offset.theta = g_key_file_get_double (file, *iter, HYSCAN_OFFSET_PROFILE_THETA, NULL);

      /* Если название группы совпадает с названием того или иного
       * HyScanSourceType, то это локатор. Иначе -- датчик. */
      if (hyscan_channel_get_types_by_id (*iter, &source, &type, &channel))
        g_hash_table_insert (priv->sonars, GINT_TO_POINTER (source), hyscan_antenna_offset_copy (&offset));
      else
        g_hash_table_insert (priv->sensors, g_strdup (*iter), hyscan_antenna_offset_copy (&offset));
    }

  g_strfreev (groups);

  return TRUE;
}

static gboolean
hyscan_offset_profile_info_group (HyScanOffsetProfile *profile,
                                  GKeyFile            *kf,
                                  const gchar         *group)
{
  gchar *name;

  if (!g_str_equal (group, HYSCAN_OFFSET_PROFILE_INFO_GROUP))
    return FALSE;

  name = g_key_file_get_string (kf, group, HYSCAN_OFFSET_PROFILE_NAME, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), name);

  g_free (name);
  return TRUE;
}

GHashTable *
hyscan_offset_profile_get_sonars (HyScanOffsetProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_OFFSET_PROFILE (profile), NULL);

  return g_hash_table_ref (profile->priv->sonars);
}

GHashTable *
hyscan_offset_profile_get_sensors (HyScanOffsetProfile *profile)
{
  g_return_val_if_fail (HYSCAN_IS_OFFSET_PROFILE (profile), NULL);

  return g_hash_table_ref (profile->priv->sensors);
}

gboolean
hyscan_offset_profile_apply (HyScanOffsetProfile *profile,
                             HyScanControl       *control)
{
  gpointer k;
  HyScanAntennaOffset *v;
  GHashTableIter iter;
  HyScanOffsetProfilePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_OFFSET_PROFILE (profile), FALSE);
  priv = profile->priv;

  g_hash_table_iter_init (&iter, priv->sonars);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (control), (HyScanSourceType)k, v))
        return FALSE;
    }

  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (control), (const gchar*)k, v))
        return FALSE;
    }

  return TRUE;
}
