#include <hyscan-hw-profile.h>


gchar *driver_paths[] = {
  "../../hyscanhydra4drv",
  "../../hyscanhydra4drv/tests",
  "../../hyscanhydra4drv/bin",
  NULL
};

int
main (int argc, char **argv)
{
  HyScanControl * control = NULL;
  HyScanHWProfile * pf = NULL;

  pf = hyscan_hw_profile_new (argv[1]);
  hyscan_hw_profile_set_driver_paths (pf, driver_paths);

  if (hyscan_hw_profile_check (pf))
    control = hyscan_hw_profile_connect (pf);

  g_clear_object (&pf);
  g_clear_object (&control);

  return 0;
}
