#include <hyscan-connector.h>
#include <stdlib.h>

GMainLoop *main_loop = NULL;

gboolean test_entry          (gpointer         connector);
void     connector_completed (HyScanConnector *connector,
                              gpointer         udata);

int main (int argc, char **argv)
{
  int res = EXIT_SUCCESS;
  HyScanDB *db = NULL;
  HyScanConnector *connector = NULL;

  connector = hyscan_connector_new ();
  g_signal_connect (connector, "completed", G_CALLBACK (connector_completed), NULL);

  main_loop = g_main_loop_new (NULL, TRUE);

  g_idle_add (test_entry, connector);
  
  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  if ((db = hyscan_connector_get_db (connector)) == NULL)
    {
      g_warning ("db not connected");
      res = EXIT_FAILURE;
    }

  g_clear_object (&db);
  g_clear_object (&connector);

  g_print ("All done");

  return res;
}

gboolean
test_entry (gpointer connector)
{
  hyscan_connector_execute (HYSCAN_CONNECTOR (connector), "db1.ini", NULL);
  return G_SOURCE_REMOVE;
}

void
connector_completed (HyScanConnector *connector,
                     gpointer         udata)
{
  g_main_loop_quit (main_loop);
}
