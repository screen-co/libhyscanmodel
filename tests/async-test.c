#include <hyscan-async.h>

#define N_TEST_REPEATS 5

enum
{
  TEST_PRM = 0,
  TEST_LIST,
  TEST_WAIT,
  TEST_EXIT
};

typedef struct
{
  gint counter;
  gint prm;
} CounterObject;

static int            wait;
static int            test_repeats_counter;
static int            test_id;
static int            expected_count;
static GRand         *rnd;
static CounterObject  obj;
static HyScanAsync   *async;


gboolean    async_cmd_prm  (CounterObject   *obj,
                            gint            *prm);

gboolean    async_cmd_list (CounterObject   *obj,
                            gint            *prm);

gboolean    async_cmd_wait (CounterObject   *obj,
                            gint            *prm);

void        compelted_cb   (HyScanAsync     *async,
                            gboolean         result,
                            gpointer         user_data);

void        started_cb     (HyScanAsync     *async,
                            gpointer         user_data);

gboolean    test_prm       (gpointer         user_data);

gboolean    test_list      (gpointer         user_data);

gboolean    test_wait      (gpointer         user_data);

int
main (int    argc,
      char **argv)
{
  GMainLoop *loop;

  rnd = g_rand_new_with_seed ((guint32)g_get_monotonic_time ());
  async = hyscan_async_new ();

  /* Настройка Mainloop. */
  loop = g_main_loop_new (NULL, TRUE);
  g_signal_connect (async, "completed", G_CALLBACK (compelted_cb), loop);
  g_signal_connect (async, "started", G_CALLBACK (started_cb), loop);

  test_repeats_counter = 0;
  test_id = TEST_PRM;
  g_message ("1. Parameter test.");
  g_idle_add (test_prm, loop);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_rand_free (rnd);
  g_object_unref (async);

  return 0;
}

gboolean
async_cmd_prm (CounterObject *obj,
               gint          *prm)
{
  if (prm == NULL)
    {
      g_warning ("Wrong parameter");
      return FALSE;
    }

  if (*prm != obj->prm)
    {
      g_warning ("Wrong parameter");
      return FALSE;
    }

  obj->prm = *prm;
  return TRUE;
}

gboolean
async_cmd_list (CounterObject *obj,
                gint          *prm)
{
  obj->counter++;
  return TRUE;
}

gboolean
async_cmd_wait (CounterObject *obj,
                gint          *prm)
{
  while (g_atomic_int_get (&wait))
    g_usleep (100000);
  return TRUE;
}

void
compelted_cb (HyScanAsync *async,
              gboolean     result,
              gpointer     user_data)
{
  GMainLoop *loop = user_data;

  test_repeats_counter++;
  switch (test_id)
    {
    case TEST_PRM:
      if (!result)
        {
          g_message ("Parameter test failed.");
          g_main_loop_quit (loop);
          return;
        }
      g_message ("Success [Obtained param: %d].", obj.prm);
      if (test_repeats_counter < N_TEST_REPEATS)
        {
          test_id = TEST_PRM;
          g_idle_add (test_prm, loop);
          return;
        }
      else
        {
          test_id = TEST_LIST;
        }
       break;

    case TEST_LIST:
      if (expected_count != obj.counter)
        {
          g_message ("Multiple queries test failed.");
          g_main_loop_quit (loop);
          return;
        }
      g_message ("Success [Queries counter: %d].", obj.counter);
      if (test_repeats_counter < N_TEST_REPEATS)
        {
          test_id = TEST_LIST;
          obj.counter = 0;
          g_idle_add (test_list, loop);
          return;
        }
      else
        {
          test_id = TEST_WAIT;
        }
      break;

    case TEST_WAIT:
      if (test_repeats_counter < N_TEST_REPEATS)
        {
          test_id = TEST_WAIT;
          g_idle_add (test_wait, loop);
          return;
        }
      else
        {
          test_id = TEST_EXIT;
        }
      break;
      default:
        g_main_loop_quit (loop);
        return;
    }

  switch (test_id)
    {
    case TEST_LIST:
      test_repeats_counter = 0;
      test_id = TEST_LIST;
      obj.counter = 0;
      g_message (" ");
      g_message ("2. Multiple queries test.");
      g_idle_add (test_list, loop);
      break;
    case TEST_WAIT:
      test_repeats_counter = 0;
      g_message (" ");
      g_message ("3. Wait test.");
      g_idle_add (test_wait, loop);
      break;
    default:
      g_message (" ");
      g_message ("All done.");
      g_main_loop_quit (loop);
      return;
    }
}

void
started_cb (HyScanAsync *async,
            gpointer     user_data)
{
}

gboolean
test_prm (gpointer user_data)
{
  obj.prm = g_random_int ();
  hyscan_async_append_query (async, (HyScanAsyncCommand) async_cmd_prm, &obj, &obj.prm, sizeof (obj.prm));
  g_message ("Expected param: %d.", obj.prm);
  hyscan_async_execute (async);
  return G_SOURCE_REMOVE;
}

gboolean
test_list (gpointer user_data)
{
  gint i;
  obj.counter = 0;
  expected_count = g_rand_int (rnd) % 10 + 2;
  for (i = 0; i < expected_count; ++i)
    hyscan_async_append_query (async, (HyScanAsyncCommand) async_cmd_list, &obj, &obj.prm, sizeof (obj.prm));
  g_message ("Appended %d queries.", expected_count);
  hyscan_async_execute (async);
  return G_SOURCE_REMOVE;
}

gboolean
test_wait (gpointer user_data)
{
  g_atomic_int_set (&wait, 1);
  hyscan_async_append_query (async, (HyScanAsyncCommand) async_cmd_wait, &obj, &obj.prm, sizeof (obj.prm));
  hyscan_async_execute (async);
  if (hyscan_async_append_query (async, (HyScanAsyncCommand) async_cmd_wait, &obj, &obj.prm, sizeof (obj.prm)))
    {
      g_message ("Test failed");
      g_atomic_int_set (&wait, 0);
      return G_SOURCE_REMOVE;
    }
  if (hyscan_async_execute (async))
    {
      g_message ("Test failed");
      g_atomic_int_set (&wait, 0);
      return G_SOURCE_REMOVE;
    }

  g_atomic_int_set (&wait, 0);
  g_message ("Success");
  return G_SOURCE_REMOVE;
}
