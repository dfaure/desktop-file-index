#include "dfi-reader.h"
#include <locale.h>

int
main (void)
{
  GError *error = NULL;
  struct dfi_index *dfi;
  const struct dfi_pointer_array *dfs;
  GMappedFile *mf;
  gint i, n;

  setlocale(LC_ALL, "");

  mf = g_mapped_file_new ("index.cache", FALSE, &error);
  g_assert_no_error (error);

  dfi = dfi_index_new (g_mapped_file_get_contents (mf),
                       g_mapped_file_get_length (mf),
                       (GDestroyNotify) g_mapped_file_unref,
                       mf);
  g_assert (dfi);

  dfs = dfi_index_get_desktop_files (dfi);
  g_assert (dfs);

  n = dfi_pointer_array_get_length (dfs, dfi);
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

              if (locale)
                g_print ("  %s[%s]=%s\n", key, locale, value);
              else
                g_print ("  %s=%s\n", key, value);
            }

          g_print ("\n");
        }
    }

  return 0;
}
