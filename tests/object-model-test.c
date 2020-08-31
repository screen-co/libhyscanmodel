/* object-model-test.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanModel имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanModel на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

#include <hyscan-data-writer.h>
#include <hyscan-object-model.h>

#define if_verbose(...) if (verbose) g_print (__VA_ARGS__);

typedef enum
{
  ADD,             /* Добавить метку. */
  REMOVE,          /* Удалить метку. */
  MODIFY,          /* Изменить. */
  LAST             /* Количество действий. */
} Actions;

void                 changed_cb        (HyScanObjectStore   *model,
                                        gpointer             data);

void                 test_function     (HyScanObjectStore   *model);

gboolean             final_check       (GSList              *expect,
                                        GHashTable          *real);

void                 update_list       (HyScanMarkWaterfall *cur,
                                        HyScanMarkWaterfall *prev,
                                        Actions              action);

gboolean             make_track        (HyScanDB            *db,
                                        gchar               *name);

HyScanMarkWaterfall *make_mark         (void);

static gint        count = 10;             /* Число итераций. */
static gboolean    verbose = FALSE;        /* Выводить подробную информацию. */

static GHashTable *final_marks = NULL;     /* Таблица меток в БД в конце цикла итераций. */
static GSList     *performed = NULL;       /* Список меток, которые должны быть в БД. */

int
main (int argc, char **argv)
{
  HyScanDB *db = NULL;                    /* БД. */
  gchar *db_uri;                          /* Путь к БД. */
  gchar *name = "test";                   /* Проект и галс. */
  GMainLoop *loop = NULL;
  HyScanObjectModel *model = NULL;

  gboolean status = FALSE;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "iterations", 'n', 0, G_OPTION_ARG_INT,  &count,    "How many times to receive data", NULL },
        { "verbose",    'v', 0, G_OPTION_ARG_NONE, &verbose,  "Show sent and received marks", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    {
      g_warning ("Can't open db at %s", db_uri);
      goto exit;
    }

  if (!make_track (db, name))
    {
      g_warning ("Couldn't create track or project");
      goto exit;
    }

  loop = g_main_loop_new (NULL, TRUE);

  model = hyscan_object_model_new ();
  hyscan_object_model_set_types (model, 2,
                                 HYSCAN_TYPE_OBJECT_DATA_WFMARK,
                                 HYSCAN_TYPE_OBJECT_DATA_GEOMARK);
  g_signal_connect (model, "changed", G_CALLBACK (changed_cb), loop);
  hyscan_object_model_set_project (model, db, name);

  g_main_loop_run (loop);

  status = final_check (performed, final_marks);

exit:
  if (db != NULL && status)
    hyscan_db_project_remove (db, name);

  g_free (db_uri);
  g_clear_object (&db);
  g_clear_object (&model);

  if (loop != NULL)
    g_main_loop_unref (loop);

  if (!status)
    {
      g_print ("Test failed.\n");
      return 1;
    }

  g_print ("Test passed.\n");
  return 0;
}

void
changed_cb (HyScanObjectStore *model,
            gpointer           data)
{
  GMainLoop *loop = data;
  GHashTable *ht;
  GHashTableIter iter;
  gpointer key, value;

  ht = hyscan_object_store_get_all (model, HYSCAN_TYPE_MARK_WATERFALL);

  if (count)
    {
      g_print ("%i iterations left...\n", count);
    }
  else
    {
      g_print ("Performing final checks...\n");
      g_main_loop_quit (loop);
      final_marks = g_hash_table_ref (ht);
    }

  if_verbose ("+-------- Actual mark list: --------+\n");

  if (ht != NULL)
    {
      g_hash_table_iter_init (&iter, ht);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanMarkWaterfall *mark = value;

          if_verbose ("| %s: %s\n", (gchar*)key, mark->name);
        }
    }

  if_verbose ("+-----------------------------------+\n");

  g_hash_table_unref (ht);

  /* Добавляем/удаляем метки. */
  test_function (model);
  count--;
}

void
test_function (HyScanObjectStore *model)
{
  gint action;
  GHashTable *ht;
  GHashTableIter iter;
  gpointer value = NULL;
  guint len = 0;
  HyScanMarkWaterfall *mark = NULL;

  if (!count)
    return;

  ht = hyscan_object_store_get_all (model, HYSCAN_TYPE_MARK_WATERFALL);
  len = g_hash_table_size (ht);

  action = (len < 5) ? ADD : g_random_int_range (ADD, LAST);

  mark = make_mark ();

  if (action == ADD)
    {
      if_verbose ("Add <%s>\n", mark->name);
      hyscan_object_store_add (model, (HyScanObject*) mark, NULL);
    }
  else
    {
      gpointer key = NULL;
      gint32 index = g_random_int_range (0, len);

      g_hash_table_iter_init (&iter, ht);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (index-- != 0)
            continue;

          if (action == REMOVE)
            {
              if_verbose ("Remove <%s>\n", ((HyScanMarkWaterfall*)value)->name);
              hyscan_object_store_remove (model, HYSCAN_TYPE_MARK_WATERFALL, key);
            }
          else if (action == MODIFY)
            {
              if_verbose ("Modify <%s> to <%s>\n", ((HyScanMarkWaterfall*)value)->name, mark->name);
              hyscan_object_store_modify (model, key, (HyScanObject*)mark);
            }
          break;
        }
    }
  /* Производим то же изменение в списке performed. */
  update_list (mark, (HyScanMarkWaterfall*)value, action);

  hyscan_mark_waterfall_free (mark);
  g_hash_table_unref (ht);
}

/* Функция выполняет проверку соответствия меток в БД и в списке performed. */
gboolean
final_check (GSList     *expect,
             GHashTable *real)
{
  HyScanMarkWaterfall *mark;
  GHashTableIter iter;
  gpointer value;
  gboolean res;
  GSList *link;

  res = g_hash_table_size (real) == g_slist_length (performed);

  if (!res)
    verbose = TRUE;

  if_verbose ("Total marks in DB: %u\n", g_hash_table_size (real));
  if_verbose ("Total expected marks: %u\n", g_slist_length (performed));

  /* Сравниваем содержимое конечной хэш-таблицы и списка. */
  g_hash_table_iter_init (&iter, real);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      mark = value;
      link = g_slist_find_custom (expect, mark->name, (GCompareFunc) g_strcmp0);
      if (link == NULL)
        continue;

      if_verbose ("%s: OK\n", mark->name);
      g_free (link->data);
      expect = g_slist_delete_link (expect, link);
      g_hash_table_iter_remove (&iter);
    }

  g_hash_table_iter_init (&iter, real);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      mark = value;
      if_verbose ("%s: in DB only\n", mark->name);
    }

  for (link = expect; link != NULL; link = link->next)
    {
      if_verbose ("%s: in expected list only\n", (gchar*)link->data);
    }

  if (res)
    {
      if (0 != g_hash_table_size (real) || 0 != g_slist_length (expect))
        res = FALSE;
    }

  return res;
}

void
update_list (HyScanMarkWaterfall *cur,
             HyScanMarkWaterfall *prev,
             Actions              action)
{
  GSList *link;

  if (action == ADD)
    {
      performed = g_slist_append (performed, g_strdup (cur->name));
      return;
    }

  /* Находим метку в списке. */
  link = g_slist_find_custom (performed, prev->name, (GCompareFunc) g_strcmp0);
  g_assert_nonnull (link);

  g_free (link->data);

  if (action == REMOVE)
    performed = g_slist_delete_link (performed, link);
  else if (action == MODIFY)
    link->data = g_strdup (cur->name);
}

/* Функция создаёт галс и записывает туда какие-то данные. */
gboolean
make_track (HyScanDB *db,
            gchar    *name)
{
  HyScanAcousticDataInfo info = {.data_type = HYSCAN_DATA_FLOAT, .data_rate = 1.0};
  HyScanDataWriter *writer = hyscan_data_writer_new ();
  HyScanBuffer *buffer = hyscan_buffer_new ();
  gint i;

  hyscan_data_writer_set_db (writer, db);
  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, NULL, -1))
    g_error ("Couldn't start data writer.");

  /* Запишем что-то в галс. */
  for (i = 0; i < 2; i++)
    {
      gfloat vals = {0};
      hyscan_buffer_wrap_float (buffer, &vals, 1);
      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                            1, FALSE, 1 + i, &info, buffer);
    }

  g_object_unref (writer);
  g_object_unref (buffer);
  return TRUE;
}

/* Функция создаёт метку с уникальным именем. */
HyScanMarkWaterfall *
make_mark (void)
{
  static gint mark_count = 0;
  gint seed;
  gint seed2;
  HyScanMarkWaterfall *mark;

  mark_count++;
  seed = g_random_int_range (0, 1000);
  seed2 = g_random_int_range (0, 1000);

  mark = hyscan_mark_waterfall_new ();
  mark->name = g_strdup_printf ("Mark %i", mark_count);
  mark->track = g_strdup_printf ("TrackID%05i%05i", seed, seed2);
  mark->description = g_strdup_printf ("description %i", seed);
  mark->operator_name = g_strdup_printf ("Operator %i", seed2);
  mark->labels = seed;
  mark->ctime = seed * 1000;
  mark->mtime = seed * 10;
  mark->source = g_strdup_printf ("Source%i", seed);
  mark->index = seed;
  mark->count = seed;
  mark->width = seed * 2;
  mark->height = seed * 5;

  return mark;
}
