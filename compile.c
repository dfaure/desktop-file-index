#include "common.h"

#include "dfi-builder-string-table.h"
#include "dfi-builder-keyfile.h"
#include "dfi-builder-text-index.h"
#include "dfi-builder-string-list.h"
#include "dfi-builder-id-list.h"

#include <string.h>
#include <unistd.h>
#include <locale.h>

typedef struct
{
  GHashTable *locale_string_tables;  /* string tables */

  GSequence *app_names;              /* string list */
  GSequence *key_names;              /* string list */
  GSequence *locale_names;           /* string list */
  GSequence *group_names;            /* string list */

  GSequence  *c_text_index;          /* text index */
  GSequence  *mime_types;            /* text index */

  GHashTable *locale_text_indexes;   /* str -> text index */
  GHashTable *group_implementors;    /* str -> id list */
  GHashTable *desktop_files;         /* str -> Keyfile */

  GString    *string;                /* file contents */
} DesktopFileIndexBuilder;

#define foreach_sequence_item(iter, sequence) \
  for (iter = g_sequence_get_begin_iter (sequence);                     \
       !g_sequence_iter_is_end (iter);                                  \
       iter = g_sequence_iter_next (iter))

#define foreach_sequence_item_and_position(iter, sequence, counter) \
  for (counter = 0, iter = g_sequence_get_begin_iter (sequence);        \
       !g_sequence_iter_is_end (iter);                                  \
       iter = g_sequence_iter_next (iter), counter++)

static GHashTable *
desktop_file_index_builder_get_string_table (DesktopFileIndexBuilder *builder,
                                             const gchar             *locale)
{
  return desktop_file_index_string_tables_get_table (builder->locale_string_tables, locale);
}

static guint
desktop_file_index_builder_get_offset (DesktopFileIndexBuilder *builder)
{
  return builder->string->len;
}

static void
desktop_file_index_builder_align (DesktopFileIndexBuilder *builder,
                                  guint                    size)
{
  while (builder->string->len & (size - 1))
    g_string_append_c (builder->string, '\0');
}

static guint
desktop_file_index_builder_get_aligned (DesktopFileIndexBuilder *builder,
                                        guint                    size)
{
  desktop_file_index_builder_align (builder, size);

  return desktop_file_index_builder_get_offset (builder);
}

static void
desktop_file_index_builder_check_alignment (DesktopFileIndexBuilder *builder,
                                            guint                    size)
{
  g_assert (~builder->string->len & (size - 1));
}

static guint
desktop_file_index_builder_write_uint16 (DesktopFileIndexBuilder *builder,
                                         guint16                  value)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  desktop_file_index_builder_check_alignment (builder, sizeof (guint16));

  value = GUINT16_TO_LE (value);

  g_string_append_len (builder->string, (gpointer) &value, sizeof value);

  return offset;
}

static guint
desktop_file_index_builder_write_uint32 (DesktopFileIndexBuilder *builder,
                                         guint32                  value)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  desktop_file_index_builder_check_alignment (builder, sizeof (guint32));

  value = GUINT32_TO_LE (value);

  g_string_append_len (builder->string, (gpointer) &value, sizeof value);

  return offset;
}

#if 0
static guint
desktop_file_index_builder_write_raw_string (DesktopFileIndexBuilder *builder,
                                             const gchar             *string)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  g_string_append (builder->string, string);
  g_string_append_c (builder->string, '\0');

  return offset;
}
XXX
#endif

static guint
desktop_file_index_builder_write_string (DesktopFileIndexBuilder *builder,
                                         const gchar             *from_locale,
                                         const gchar             *string)
{
  guint offset;

  offset = desktop_file_index_string_tables_get_offset (builder->locale_string_tables, from_locale, string);

  return desktop_file_index_builder_write_uint32 (builder, offset);
}

static guint
desktop_file_index_builder_write_string_list (DesktopFileIndexBuilder *builder,
                                              GSequence               *strings)
{
  guint offset = desktop_file_index_builder_get_aligned (builder, sizeof (guint32));
  GSequenceIter *iter;

  desktop_file_index_builder_write_uint16 (builder, g_sequence_get_length (strings));
  desktop_file_index_builder_write_uint16 (builder, 0xffff); /* padding */

  for (iter = g_sequence_get_begin_iter (strings); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter))
    desktop_file_index_builder_write_string (builder, "", g_sequence_get (iter));

  return offset;
}

static guint
desktop_file_index_builder_write_id (DesktopFileIndexBuilder *builder,
                                     GSequence               *string_list,
                                     const gchar             *string)
{
  GSequenceIter *iter;
  guint value;

  if (string == NULL)
    return desktop_file_index_builder_write_uint16 (builder, G_MAXUINT16);

  iter = g_sequence_lookup (string_list, (gpointer) string, (GCompareDataFunc) strcmp, NULL);
  g_assert (iter != NULL);

  value = g_sequence_iter_get_position (iter);
  g_assert_cmpuint (value, <, G_MAXUINT16);

  return desktop_file_index_builder_write_uint16 (builder, (gsize) value);
}

static guint
desktop_file_index_builder_write_keyfile (DesktopFileIndexBuilder *builder,
                                          const gchar             *app,
                                          gpointer                 data)
{
  guint offset = desktop_file_index_builder_get_aligned (builder, sizeof (guint16));
  DesktopFileIndexKeyfile *keyfile = data;
  gint n_groups, n_items;
  gint i;

  n_groups = desktop_file_index_keyfile_get_n_groups (keyfile);
  n_items = desktop_file_index_keyfile_get_n_items (keyfile);

  desktop_file_index_builder_write_uint16 (builder, n_groups);
  desktop_file_index_builder_write_uint16 (builder, n_items);

  for (i = 0; i < n_groups; i++)
    {
      const gchar *group_name;
      guint start;

      group_name = desktop_file_index_keyfile_get_group_name (keyfile, i);
      desktop_file_index_keyfile_get_group_range (keyfile, i, &start, NULL);

      desktop_file_index_builder_write_id (builder, builder->group_names, group_name);
      desktop_file_index_builder_write_uint16 (builder, start);
    }

  for (i = 0; i < n_items; i++)
    {
      const gchar *key, *locale, *value;

      desktop_file_index_keyfile_get_item (keyfile, i, &key, &locale, &value);

      desktop_file_index_builder_write_id (builder, builder->key_names, key);
      desktop_file_index_builder_write_id (builder, builder->locale_names, locale);
      desktop_file_index_builder_write_string (builder, locale, value);
    }

  return offset;
}

typedef guint (* DesktopFileIndexBuilderFunc) (DesktopFileIndexBuilder *builder,
                                               const gchar             *key,
                                               gpointer                 data);

static guint
desktop_file_index_builder_write_pointer_array (DesktopFileIndexBuilder     *builder,
                                                GSequence                   *key_list,
                                                guint                        key_list_offset,
                                                GHashTable                  *data_table,
                                                DesktopFileIndexBuilderFunc  func)
{
  GSequenceIter *iter;
  guint *offsets;
  gint n, i = 0;
  guint offset;

  n = g_sequence_get_length (key_list);
  offsets = g_new0 (guint, n);

  for (iter = g_sequence_get_begin_iter (key_list); !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter))
    {
      const gchar *key = g_sequence_get (iter);
      gpointer data;

      data = g_hash_table_lookup (data_table, key);
      offsets[i++] = (* func) (builder, key, data);
    }
  g_assert (i == n);

  offset = desktop_file_index_builder_get_aligned (builder, sizeof (guint32));
  desktop_file_index_builder_write_uint32 (builder, key_list_offset);

  for (i = 0; i < n; i++)
    desktop_file_index_builder_write_uint32 (builder, offsets[i]);

  g_free (offsets);

  return offset;
}

static guint
desktop_file_index_builder_write_id_list (DesktopFileIndexBuilder *builder,
                                          const gchar             *key,
                                          gpointer                 data)
{
  GArray *id_list = data;
  const guint16 *ids;
  guint offset;
  guint n_ids;
  guint i;

  ids = desktop_file_index_id_list_get_ids (id_list, &n_ids);

  offset = desktop_file_index_builder_write_uint16 (builder, n_ids);

  for (i = 0; i < n_ids; i++)
    desktop_file_index_builder_write_uint16 (builder, ids[i]);

  return offset;
}

static guint
desktop_file_index_builder_write_text_index (DesktopFileIndexBuilder *builder,
                                             const gchar             *key,
                                             gpointer                 data)
{
  GSequence *text_index = data;
  const gchar *locale = key;
  GHashTable *string_table;
  GSequenceIter *iter;
  const gchar **strings;
  guint *id_lists;
  guint offset;
  guint n_items;
  guint i;

  string_table = desktop_file_index_builder_get_string_table (builder, locale);
  if (!desktop_file_index_string_table_is_written (string_table))
    {
      GHashTable *c_string_table;

      c_string_table = desktop_file_index_string_tables_get_table (builder->locale_string_tables, "");
      desktop_file_index_string_table_write (string_table, c_string_table, builder->string);
    }

  n_items = g_sequence_get_length (text_index);

  strings = g_new (const gchar *, n_items);
  id_lists = g_new (guint, n_items);

  desktop_file_index_builder_align (builder, sizeof (guint16));

  foreach_sequence_item_and_position (iter, text_index, i)
    {
      GArray *id_list;

      desktop_file_index_text_index_get_item (iter, &strings[i], &id_list);
      id_lists[i] = desktop_file_index_builder_write_id_list (builder, NULL, id_list);
    }

  desktop_file_index_builder_align (builder, sizeof (guint32));

  offset = desktop_file_index_builder_get_offset (builder);

  desktop_file_index_builder_write_uint32 (builder, n_items);

  for (i = 0; i < n_items; i++)
    {
      desktop_file_index_builder_write_string (builder, locale, strings[i]);
      desktop_file_index_builder_write_uint32 (builder, id_lists[i]);
    }

  g_free (strings);
  g_free (id_lists);

  return offset;
}


static void
desktop_file_index_builder_serialise (DesktopFileIndexBuilder *builder)
{
  guint32 header_fields[8] = { 0, };

  builder->string = g_string_new (NULL);

  /* Make room for the header */
  g_string_append_len (builder->string, (char *) header_fields, sizeof header_fields);

  /* Write out the C string table, filling in the offsets
   *
   * We have to do this first because all of the string lists (apps,
   * keys, locales, groups) are stored as strings in the C locale.
   */
  {
    GHashTable *c_table;

    c_table = desktop_file_index_builder_get_string_table (builder, "");
    desktop_file_index_string_table_write (c_table, NULL, builder->string);
  }

  /* Write out the string lists.  This will work because they only
   * refer to strings in the C locale.
   */
  {
    header_fields[0] = desktop_file_index_builder_write_string_list (builder, builder->app_names);
    header_fields[1] = desktop_file_index_builder_write_string_list (builder, builder->key_names);
    header_fields[2] = desktop_file_index_builder_write_string_list (builder, builder->locale_names);
    header_fields[3] = desktop_file_index_builder_write_string_list (builder, builder->group_names);
  }

  /* Write out the group implementors */
  {
    /*
    header_fields[4] = desktop_file_index_builder_write_pointer_array (builder,
                                                                       builder->group_names,
                                                                       header_fields[3],
                                                                       builder->group_implementors,
                                                                       desktop_file_index_builder_write_id_list);
                                                                       */
  }

  /* Write out the text indexes for the actual locales.
   *
   * Note: we do this by visiting each item in the locale string list,
   * which doesn't include the C locale, so we won't end up emitting the
   * C locale again here.
   *
   * Note: this function will write out the locale-specific string
   * tables alongside the table for each locale in order to improve
   * locality.
   */
  {
    header_fields[5] = desktop_file_index_builder_write_pointer_array (builder,
                                                                       builder->locale_names,
                                                                       header_fields[2],
                                                                       builder->locale_text_indexes,
                                                                       desktop_file_index_builder_write_text_index);
  }

  /* Write out the desktop file contents.
   *
   * We have to do this last because the desktop files refer to strings
   * from all the locales and those are only actually written in the
   * last step.
   *
   * TODO: we could improve things a bit by storing the desktop files at
   * the front of the cache, but this would require a two-pass
   * approach...
   */
  {
    header_fields[6] = desktop_file_index_builder_write_pointer_array (builder,
                                                                       builder->app_names,
                                                                       header_fields[0],
                                                                       builder->desktop_files,
                                                                       desktop_file_index_builder_write_keyfile);
  }

  /* Write out the mime types index */
  {
    //header_fields[7] = desktop_file_index_builder_write_text_index (builder, NULL, builder->mime_types);
  }

  /* Replace the header */
  {
    guint32 *file = (guint32 *) builder->string->str;
    gint i;

    for (i = 0; i < G_N_ELEMENTS (header_fields); i++)
      file[i] = GUINT32_TO_LE (header_fields[i]);
  }
}

static void
desktop_file_index_builder_add_strings_for_keyfile (DesktopFileIndexBuilder *builder,
                                                    DesktopFileIndexKeyfile *keyfile)
{
  guint n_groups;
  guint i;

  n_groups = desktop_file_index_keyfile_get_n_groups (keyfile);

  for (i = 0; i < n_groups; i++)
    {
      const gchar *group_name;
      guint start, end;
      guint j;

      group_name = desktop_file_index_keyfile_get_group_name (keyfile, i);
      desktop_file_index_keyfile_get_group_range (keyfile, i, &start, &end);

      desktop_file_index_string_list_ensure (builder->group_names, group_name);

      for (j = start; j < end; j++)
        {
          const gchar *key, *locale, *value;

          desktop_file_index_keyfile_get_item (keyfile, j, &key, &locale, &value);

          desktop_file_index_string_list_ensure (builder->key_names, key);

          if (locale)
            desktop_file_index_string_list_ensure (builder->locale_names, locale);

          desktop_file_index_string_tables_add_string (builder->locale_string_tables, locale, value);
        }
    }
}

static void
desktop_file_index_builder_add_strings (DesktopFileIndexBuilder *builder)
{
  GHashTableIter keyfile_iter;
  gpointer key, value;

  builder->locale_string_tables = desktop_file_index_string_tables_create ();
  builder->app_names = desktop_file_index_string_list_new ();
  builder->key_names = desktop_file_index_string_list_new ();
  builder->locale_names = desktop_file_index_string_list_new ();
  builder->group_names = desktop_file_index_string_list_new ();

  g_hash_table_iter_init (&keyfile_iter, builder->desktop_files);
  while (g_hash_table_iter_next (&keyfile_iter, &key, &value))
    {
      DesktopFileIndexKeyfile *keyfile = value;
      const gchar *app = key;

      desktop_file_index_string_list_ensure (builder->app_names, app);
      desktop_file_index_builder_add_strings_for_keyfile (builder, keyfile);
    }

  {
    GHashTable *c_string_table;

    c_string_table = desktop_file_index_string_tables_get_table (builder->locale_string_tables, "");

    desktop_file_index_string_list_populate_strings (builder->app_names, c_string_table);
    desktop_file_index_string_list_populate_strings (builder->group_names, c_string_table);
    desktop_file_index_string_list_populate_strings (builder->key_names, c_string_table);
    desktop_file_index_string_list_populate_strings (builder->locale_names, c_string_table);
  }
}

static GSequence *
desktop_file_index_builder_index_one_locale (DesktopFileIndexBuilder *builder,
                                             const gchar             *locale)
{
  const gchar *fields[] = { "Name", "GenericName", "X-GNOME-FullName", "Comment", "Keywords" };
  gchar **locale_variants;
  GHashTableIter keyfile_iter;
  gpointer key, val;
  GSequence *text_index;

  if (locale)
    locale_variants = g_get_locale_variants (locale);
  else
    locale_variants = g_new0 (gchar *, 0 + 1);

  text_index = desktop_file_index_text_index_new ();

  g_hash_table_iter_init (&keyfile_iter, builder->desktop_files);
  while (g_hash_table_iter_next (&keyfile_iter, &key, &val))
    {
      DesktopFileIndexKeyfile *kf = val;
      const gchar *app = key;
      gint i;

      for (i = 0; i < G_N_ELEMENTS (fields); i++)
        {
          const gchar *value;

          value = desktop_file_index_keyfile_get_value (kf, (const gchar **) locale_variants, "Desktop Entry", fields[i]);

          if (value)
            {
              guint16 ids[3];

              ids[0] = desktop_file_index_string_list_get_id (builder->app_names, app);
              ids[1] = desktop_file_index_string_list_get_id (builder->group_names, "Desktop Entry");
              ids[2] = desktop_file_index_string_list_get_id (builder->key_names, fields[i]);

              desktop_file_index_text_index_add_ids_tokenised (text_index, value, ids, 3);
            }
        }
    }

  g_free (locale_variants);

  return text_index;
}

static void
desktop_file_index_builder_index_strings (DesktopFileIndexBuilder *builder)
{
  GHashTable *c_string_table;
  GSequenceIter *iter;

  c_string_table = desktop_file_index_string_tables_get_table (builder->locale_string_tables, "");
  builder->c_text_index = desktop_file_index_builder_index_one_locale (builder, "");
  desktop_file_index_text_index_populate_strings (builder->c_text_index, c_string_table);

  builder->locale_text_indexes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                        (GDestroyNotify) g_sequence_free);

  foreach_sequence_item (iter, builder->locale_names)
    {
      const gchar *locale = g_sequence_get (iter);
      GHashTable *string_table;
      GSequence *text_index;

      text_index = desktop_file_index_builder_index_one_locale (builder, locale);
      g_hash_table_insert (builder->locale_text_indexes, g_strdup (locale), text_index);
      string_table = desktop_file_index_string_tables_get_table (builder->locale_string_tables, locale);
      desktop_file_index_text_index_populate_strings (text_index, string_table);
    }
}

static DesktopFileIndexBuilder *
desktop_file_index_builder_new (void)
{
  DesktopFileIndexBuilder *builder;

  builder = g_slice_new0 (DesktopFileIndexBuilder);
  builder->desktop_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) desktop_file_index_keyfile_free);
  builder->string = NULL;

  return builder;
}

static gboolean
desktop_file_index_builder_add_desktop_file (DesktopFileIndexBuilder  *builder,
                                             const gchar              *desktop_id,
                                             const gchar              *filename,
                                             GError                  **error)
{
  DesktopFileIndexKeyfile *kf;

  kf = desktop_file_index_keyfile_new (filename, error);
  if (!kf)
    return FALSE;

  g_hash_table_insert (builder->desktop_files, g_strdup (desktop_id), kf);

  return TRUE;
}

int
main (int argc, char **argv)
{
  DesktopFileIndexBuilder *builder;
  GError *error = NULL;
  const gchar *name;
  GDir *dir;

  setlocale (LC_ALL, "");

  builder = desktop_file_index_builder_new ();

  dir = g_dir_open (argv[1], 0, &error);
  g_assert_no_error (error);
  while ((name = g_dir_read_name (dir)))
    {
      gchar *fullname;

      if (!g_str_has_suffix (name, ".desktop"))
        continue;

      fullname = g_build_filename (argv[1], name, NULL);
      desktop_file_index_builder_add_desktop_file (builder, name, fullname, &error);
      g_free (fullname);

      if (error)
        {
          g_printerr ("%s\n", error->message);
          g_clear_error (&error);
        }
    }
  g_dir_close (dir);

  desktop_file_index_builder_add_strings (builder);

  desktop_file_index_builder_index_strings (builder);

  desktop_file_index_builder_serialise (builder);

  g_file_set_contents ("index.cache", builder->string->str, builder->string->len, &error);
  g_assert_no_error (error);

  return 0;
}
