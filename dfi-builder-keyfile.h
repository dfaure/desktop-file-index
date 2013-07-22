#include <glib.h>

typedef struct _DesktopFileIndexKeyfile DesktopFileIndexKeyfile;

DesktopFileIndexKeyfile * desktop_file_index_keyfile_new                (const gchar              *filename,
                                                                         GError                  **error);

void                    desktop_file_index_keyfile_free                 (DesktopFileIndexKeyfile  *keyfile);

const gchar *           desktop_file_index_keyfile_get_value            (DesktopFileIndexKeyfile  *keyfile,
                                                                         const gchar * const      *locale_variants,
                                                                         const gchar              *group_name,
                                                                         const gchar              *key);

guint                   desktop_file_index_keyfile_get_n_groups         (DesktopFileIndexKeyfile  *keyfile);

guint                   desktop_file_index_keyfile_get_n_items          (DesktopFileIndexKeyfile  *keyfile);

const gchar *           desktop_file_index_keyfile_get_group_name       (DesktopFileIndexKeyfile  *keyfile,
                                                                         guint                     group);

void                    desktop_file_index_keyfile_get_group_range      (DesktopFileIndexKeyfile  *keyfile,
                                                                         guint                     group,
                                                                         guint                    *start,
                                                                         guint                    *end);

void                    desktop_file_index_keyfile_get_item             (DesktopFileIndexKeyfile  *keyfile,
                                                                         guint                     item,
                                                                         const gchar             **key,
                                                                         const gchar             **locale,
                                                                         const gchar             **value);
