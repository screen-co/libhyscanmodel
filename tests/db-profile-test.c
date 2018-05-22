#include <hyscan-db-profile.h>
#include <gio/gio.h>
#include <stdlib.h>

#define TEST_FILE_NAME "profiletest.ini"

int main (int argc, char **argv)
{
  int res = EXIT_SUCCESS;
  HyScanDBProfile *p1 = NULL;
  HyScanDBProfile *p2 = NULL;


  p1 = hyscan_db_profile_new_full ("testname", "testuri");
  if (!hyscan_serializable_write (HYSCAN_SERIALIZABLE (p1), TEST_FILE_NAME))
    {
      g_warning ("Couldn't write test profile.");
      res = EXIT_FAILURE;
      goto exit;
    }

  p2 = hyscan_db_profile_new ();
  hyscan_serializable_read (HYSCAN_SERIALIZABLE (p2), TEST_FILE_NAME);
  if (!hyscan_serializable_write (HYSCAN_SERIALIZABLE (p1), TEST_FILE_NAME))
    {
      g_warning ("Couldn't read test profile.");
      res = EXIT_FAILURE;
      goto exit;
    }

  if (!g_str_equal (hyscan_db_profile_get_uri (p1), hyscan_db_profile_get_uri (p2)) ||
      !g_str_equal (hyscan_db_profile_get_name (p1), hyscan_db_profile_get_name (p2)))
    {
      g_warning ("URIs or names are not equals.");
      res = EXIT_FAILURE;
      goto exit;
    }

  g_message ("All done.");

exit:
  {
    GFile *file = g_file_new_for_path (TEST_FILE_NAME);
    if (file != NULL)
      {
        g_file_delete (file, NULL, NULL);
        g_object_unref (file);
      }
  }

  g_clear_object (&p1);
  g_clear_object (&p2);

  return res;
}
