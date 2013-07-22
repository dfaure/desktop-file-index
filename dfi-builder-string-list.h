#include <glib.h>

GSequence *             desktop_file_index_string_list_new              (void);

void                    desktop_file_index_string_list_ensure           (GSequence   *string_list,
                                                                         const gchar *string);

guint                   desktop_file_index_string_list_get_id           (GSequence   *string_list,
                                                                         const gchar *string);

void                    desktop_file_index_string_list_populate_strings (GSequence   *string_list,
                                                                         GHashTable  *string_table);
