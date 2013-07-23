#include "dfi-reader.h"
#include <sys/mman.h>
#include <locale.h>

int
main (void)
{
  GError *error = NULL;
  struct dfi_index *dfi;
  const struct dfi_string_list *locales;
  const struct dfi_pointer_array *dfs;
  GMappedFile *mf;
  gint i, n;

  setlocale(LC_ALL, "");

  dfi = dfi_index_new (".");
  g_assert (dfi);

#if 0
  locales = dfi_index_get_locale_names (dfi);
  g_print ("%d locales\n", dfi_string_list_get_length (locales));
  n = dfi_string_list_get_length (locales);
  for (i = 0; i < n; i++)
    g_print ("  %s\n", dfi_string_list_get_string_at_index (locales, dfi, i));
  g_print ("\n\n");

#endif

  dfs = dfi_index_get_desktop_files (dfi);
  g_assert (dfs);

  n = dfi_pointer_array_get_length (dfs, dfi);

#if 0
  for (i = 0; i < n; i++)
    {
      const gchar *key = dfi_pointer_array_get_item_key (dfs, dfi, i);
      const struct dfi_keyfile *kf = dfi_keyfile_from_pointer (dfi, dfi_pointer_array_get_pointer (dfs, i));
      const struct dfi_keyfile_group *groups;
      gint n_groups, j;

      g_print ("==== %s ====\n", key);

      groups = dfi_keyfile_get_groups (kf, dfi, &n_groups);
      for (j = 0; j < n_groups; j++)
        {
          const struct dfi_keyfile_item *items;
          gint n_items, k;

          g_print ("  [%s]\n", dfi_keyfile_group_get_name (&groups[j], dfi));
          items = dfi_keyfile_group_get_items (&groups[j], dfi, kf, &n_items);

          for (k = 0; k < n_items; k++)
            {
              const gchar *key = dfi_keyfile_item_get_key (&items[k], dfi);
              const gchar *locale = dfi_keyfile_item_get_locale (&items[k], dfi);
              const gchar *value = dfi_keyfile_item_get_value (&items[k], dfi);

              if (locale[0])
                {
                  if (g_str_has_prefix (locale, "fr"))
                    g_print ("  %s[%s]=%s\n", key, locale, value);
                }
              else
                g_print ("  %s=%s\n", key, value);
            }

          g_print ("\n");
        }
    }
#endif

  const struct dfi_pointer_array *text_indexes = dfi_index_get_text_indexes (dfi);
  gint language_code = dfi_string_list_binary_search (dfi_index_get_locale_names (dfi), dfi, "fr");
  const struct dfi_text_index *text_index = dfi_text_index_from_pointer (dfi, dfi_pointer_array_get_pointer (text_indexes, language_code));

  gint n_ids;
  guint64 start_time = g_get_monotonic_time();
  const dfi_id * ids = dfi_text_index_get_ids_for_exact_match (dfi, text_index, "système", &n_ids);
  g_print ("%d\n", (gint)(g_get_monotonic_time()- start_time));
  g_print ("got %d\n", n_ids);
  g_print ("%s %s %s\n", dfi_string_list_get_string (dfi_index_get_app_names (dfi), dfi, ids[0]),
                         dfi_string_list_get_string (dfi_index_get_group_names (dfi), dfi, ids[1]),
                         dfi_string_list_get_string (dfi_index_get_key_names (dfi), dfi, ids[2]));

  for (i = 0; i < n_ids / 3; i++)
    {
      const gchar *key = dfi_string_list_get_string (dfi_index_get_app_names (dfi), dfi, ids[3*i]);
      const struct dfi_keyfile *kf = dfi_keyfile_from_pointer (dfi, dfi_pointer_array_get_pointer (dfs, dfi_id_get(ids[3*i])));
      const struct dfi_keyfile_group *groups;
      gint n_groups, j;

      //g_print ("==== %s ====\n", key);

      groups = dfi_keyfile_get_groups (kf, dfi, &n_groups);
      for (j = 0; j < n_groups; j++)
        {
          const struct dfi_keyfile_item *items;
          gint n_items, k;

     //     g_print ("  [%s]\n", dfi_keyfile_group_get_name (&groups[j], dfi));
          items = dfi_keyfile_group_get_items (&groups[j], dfi, kf, &n_items);

          for (k = 0; k < n_items; k++)
            {
              const gchar *key = dfi_keyfile_item_get_key (&items[k], dfi);
              const gchar *locale = dfi_keyfile_item_get_locale (&items[k], dfi);
              const gchar *value = dfi_keyfile_item_get_value (&items[k], dfi);

              if (locale[0])
                {
                  if (g_str_has_prefix (locale, "fr"));
//                    g_print ("  %s[%s]=%s\n", key, locale, value);
                }
              else;
  //              g_print ("  %s=%s\n", key, value);
            }

    //      g_print ("\n");
        }
      
    }
  return 0;
}
