#include <hyscan-sonar-profile.h>
#include <gio/gio.h>
#include <stdlib.h>

#define TEST_FILE_NAME "sonarprofiletest.ini"

#define PROFILE_NAME "test-sonar"
#define DRV_PATH "test/drv/path"
#define DRV_NAME "test-drv-name"
#define SONAR_URI "test-uri"
#define SONAR_CFG "test-cgf"

int main (int argc, char **argv)
{
  int res = EXIT_SUCCESS;
  HyScanSonarProfile *p1 = NULL;
  HyScanSonarProfile *p2 = NULL;

  p1 = hyscan_sonar_profile_new_full (PROFILE_NAME, DRV_PATH, DRV_NAME, SONAR_URI, SONAR_CFG);
  if (!g_str_equal (hyscan_sonar_profile_get_name (p1), PROFILE_NAME) ||
      !g_str_equal (hyscan_sonar_profile_get_driver_path (p1), DRV_PATH) ||
      !g_str_equal (hyscan_sonar_profile_get_driver_name (p1), DRV_NAME) ||
      !g_str_equal (hyscan_sonar_profile_get_sonar_uri (p1), SONAR_URI) ||
      !g_str_equal (hyscan_sonar_profile_get_sonar_config (p1), SONAR_CFG))
    {
      g_warning ("Wrong properties.");
      res = EXIT_FAILURE;
      goto exit;
    }

  if (!hyscan_serializable_write (HYSCAN_SERIALIZABLE (p1), TEST_FILE_NAME))
    {
      g_warning ("Couldn't write test profile.");
      res = EXIT_FAILURE;
      goto exit;
    }

  p2 = hyscan_sonar_profile_new ();
  if (!hyscan_serializable_read (HYSCAN_SERIALIZABLE (p2), TEST_FILE_NAME))
    {
      g_warning ("Couldn't read test profile.");
      res = EXIT_FAILURE;
      goto exit;
    }
  if (!hyscan_serializable_write (HYSCAN_SERIALIZABLE (p1), TEST_FILE_NAME))
    {
      g_warning ("Couldn't write test profile.");
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
