#include <hyscan-hw-profile.h>
#include <hyscan-db-profile.h>
#include <hyscan-offset-profile.h>
#include <hyscan-profile-loader.h>

void
test (const gchar * subdir,
      const gchar * sysdir,
      GType         type)
{
  HyScanProfileLoader *loader;
  GHashTable *profiles;
  GHashTableIter iter;
  gpointer k, v;

  g_print ("%s:\n", subdir);

  loader = hyscan_profile_loader_new (subdir, sysdir, type);
  profiles = hyscan_profile_loader_list (loader);

  g_hash_table_iter_init (&iter, profiles);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      g_print ("  %s : %s\n", (gchar*)k, hyscan_profile_get_name ((HyScanProfile *)v));
    }

  g_hash_table_unref (profiles);
}

int
main (int argc, char **argv)
{

  test ("hw-profiles", "/etc/", HYSCAN_TYPE_HW_PROFILE);
  test ("db-profiles", "/etc/", HYSCAN_TYPE_DB_PROFILE);
  test ("offset-profiles", "/etc/", HYSCAN_TYPE_OFFSET_PROFILE);

  return 0;
}
