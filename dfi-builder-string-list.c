#include "dfi-builder-string-list.h"

#include "dfi-builder-string-table.h"

#include <string.h>

GSequence *
desktop_file_index_string_list_new (void)
{
  return g_sequence_new (g_free);
}

void
desktop_file_index_string_list_ensure (GSequence   *string_list,
                                       const gchar *string)
{
  GSequenceIter *iter;

  iter = g_sequence_lookup (string_list, (gpointer) string, (GCompareDataFunc) strcmp, NULL);
  if (!iter)
    g_sequence_insert_sorted (string_list, g_strdup (string), (GCompareDataFunc) strcmp, NULL);
}

guint
desktop_file_index_string_list_get_id (GSequence   *string_list,
                                       const gchar *string)
{
  GSequenceIter *iter;

  iter = g_sequence_lookup (string_list, (gpointer) string, (GCompareDataFunc) strcmp, NULL);
  g_assert (iter != NULL);

  return g_sequence_iter_get_position (iter);
}

void
desktop_file_index_string_list_populate_strings (GSequence  *string_list,
                                                 GHashTable *string_table)
{
  GSequenceIter *iter;

  iter = g_sequence_get_begin_iter (string_list);

  while (!g_sequence_iter_is_end (iter))
    {
      const gchar *string = g_sequence_get (iter);

      desktop_file_index_string_table_add_string (string_table, string);

      iter = g_sequence_iter_next (iter);
    }
}
