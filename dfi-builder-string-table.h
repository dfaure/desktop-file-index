#include <glib.h>

GHashTable *            desktop_file_index_string_tables_create         (void);

GHashTable *            desktop_file_index_string_tables_get_table      (GHashTable  *string_tables,
                                                                         const gchar *locale);

void                    desktop_file_index_string_tables_add_string     (GHashTable  *string_tables,
                                                                         const gchar *locale,
                                                                         const gchar *string);

void                    desktop_file_index_string_table_add_string      (GHashTable  *string_table,
                                                                         const gchar *string);

guint                   desktop_file_index_string_tables_get_offset     (GHashTable  *string_table,
                                                                         const gchar *locale,
                                                                         const gchar *string);

guint                   desktop_file_index_string_table_get_offset      (GHashTable  *string_table,
                                                                         const gchar *string);

gboolean                desktop_file_index_string_table_is_written      (GHashTable *string_table);

void                    desktop_file_index_string_table_write           (GHashTable *string_table,
                                                                         GHashTable *shared_table,
                                                                         GString    *file);
