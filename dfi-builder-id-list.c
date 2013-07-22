#include "dfi-builder-text-index.h"

#include <string.h>

GArray *
desktop_file_index_id_list_new (void)
{
  return g_array_new (FALSE, FALSE, sizeof (guint16));
}

void
desktop_file_index_id_list_free (GArray *id_list)
{
  g_array_free (id_list, TRUE);
}

void
desktop_file_index_id_list_add_ids (GArray        *id_list,
                                    const guint16 *ids,
                                    gint           n_ids)
{
  g_array_append_vals (id_list, ids, n_ids);
}

const guint16 *
desktop_file_index_id_list_get_ids (GArray *id_list,
                                    guint  *n_ids)
{
  *n_ids = id_list->len;

  return (guint16 *) id_list->data;
}
