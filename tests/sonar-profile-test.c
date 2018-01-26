#include <hyscan-sonar-profile.h>
#include <gio/gio.h>
#include <stdlib.h>

#define TEST_FILE_NAME "sonarprofiletest.ini"

int main (int argc, char **argv)
{
  int res = EXIT_SUCCESS;
  HyScanSonarProfile *p1 = NULL;
  HyScanSonarProfile *p2 = NULL;

  p1 = hyscan_sonar_profile_new_full ("test-sonar", "test/drv/path/", "test-driver-name", "test-sonar-uri", "test-sonar-cfg");
  if (!hyscan_sonar_profile_write_to_file (p1, TEST_FILE_NAME))
    {
      g_warning ("Couldn't write test profile.");
      res = EXIT_FAILURE;
      goto exit;
    }

  p2 = hyscan_sonar_profile_new ();
  hyscan_sonar_profile_read_from_file (p2, TEST_FILE_NAME);
  if (!hyscan_sonar_profile_write_to_file (p1, TEST_FILE_NAME))
    {
      g_warning ("Couldn't read test profile.");
      res = EXIT_FAILURE;
      goto exit;
    }

  if (!g_str_equal (hyscan_sonar_profile_get_name (p1), hyscan_sonar_profile_get_name (p2)) ||
      !g_str_equal (hyscan_sonar_profile_get_driver_path (p1), hyscan_sonar_profile_get_driver_path (p2)) ||
      !g_str_equal (hyscan_sonar_profile_get_driver_name (p1), hyscan_sonar_profile_get_driver_name (p2)) ||
      !g_str_equal (hyscan_sonar_profile_get_sonar_uri (p1), hyscan_sonar_profile_get_sonar_uri (p2)) ||
      !g_str_equal (hyscan_sonar_profile_get_sonar_config (p1), hyscan_sonar_profile_get_sonar_config (p2)))
    {
      g_warning ("Profiles mismatch.");
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
