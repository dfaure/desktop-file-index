#include <glib.h>

GSequence *             desktop_file_index_text_index_new               (void);

void                    desktop_file_index_text_index_add_ids           (GSequence     *text_index,
                                                                         const gchar   *token,
                                                                         const guint16 *ids,
                                                                         gint           n_ids);

void                    desktop_file_index_text_index_add_ids_tokenised (GSequence     *text_index,
                                                                         const gchar   *string_to_tokenise,
                                                                         const guint16 *ids,
                                                                         gint           n_ids);

void                    desktop_file_index_text_index_get_item          (GSequenceIter  *iter,
                                                                         const gchar   **token,
                                                                         GArray        **id_list);

void                    desktop_file_index_text_index_populate_strings  (GSequence     *text_index,
                                                                         GHashTable    *string_table);
