#include "hyscan-profile-common.h"

#define HYSCAN_CONFIG_DIRECTORY_NAME "HyScan"

/* Получает NULL-терминированный массив названий профилей. */
gchar **
hyscan_profile_common_list_profiles (const gchar *profiles_path)
{
  gchar **profiles = NULL;
  GError *error = NULL;
  GDir *dir;

  dir = g_dir_open (profiles_path, 0, &error);
  if (error == NULL)
    {
      const gchar *filename;
      guint nprofiles = 0;

      while ((filename = g_dir_read_name (dir)) != NULL)
        {
          gchar *fullname = g_strdup_printf ("%s%s%s", profiles_path, G_DIR_SEPARATOR_S, filename);
          if (g_file_test (fullname, G_FILE_TEST_IS_REGULAR))
            {
              profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
              profiles[nprofiles - 1] = g_strdup (filename);
            }
          g_free (fullname);
        }

      if (profiles != NULL)
        {
          profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
          profiles[nprofiles - 1] = NULL;
        }
    }
  else
    {
      g_warning ("hyscan_profile_common_list_profiles(): %s", error->message);
      g_error_free (error);
    }

  return profiles;
}
