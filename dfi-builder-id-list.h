#include <glib.h>

GArray *                desktop_file_index_id_list_new                  (void);

void                    desktop_file_index_id_list_free                 (GArray        *id_list);

void                    desktop_file_index_id_list_add_ids              (GArray        *id_list,
                                                                         const guint16 *ids,
                                                                         gint           n_ids);

const guint16 *         desktop_file_index_id_list_get_ids              (GArray         *id_list,
                                                                         guint          *n_ids);

