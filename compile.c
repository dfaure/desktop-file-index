#include "common.h"

#include <string.h>
#include <unistd.h>

typedef struct
{
  GHashTable *string_table;
  GHashTable *key_names;
  GHashTable *group_names;
  GHashTable *app_names;
  GHashTable *locale_names;

  GHashTable *desktop_files;

  GString    *string;
} DesktopFileIndexBuilder;

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

static guint
desktop_file_index_builder_write_raw_string (DesktopFileIndexBuilder *builder,
                                             const gchar             *string)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  g_string_append (builder->string, string);
  g_string_append_c (builder->string, '\0');

  return offset;
}

static guint
desktop_file_index_builder_write_string (DesktopFileIndexBuilder *builder,
                                         const gchar             *string)
{
  gpointer offset;

  if (string == NULL)
    return desktop_file_index_builder_write_uint32 (builder, 0);

  offset = g_hash_table_lookup (builder->string_table, string);
  g_assert (offset != NULL);

  return desktop_file_index_builder_write_uint32 (builder, GPOINTER_TO_UINT (offset));
}

static gint
indirect_strcmp (gconstpointer a,
                 gconstpointer b)
{
  const gchar * const *astr = a, * const *bstr = b;

  return strcmp (*astr, *bstr);
}

static guint
desktop_file_index_builder_write_string_list (DesktopFileIndexBuilder *builder,
                                              GHashTable              *strings)
{
  guint offset = desktop_file_index_builder_get_aligned (builder, sizeof (guint32));
  GHashTableIter iter;
  GPtrArray *list;
  gpointer key;
  gint i, n;

  n = g_hash_table_size (strings);
  g_assert_cmpint (n, <, 65536);
  list = g_ptr_array_new ();
  g_hash_table_iter_init (&iter, strings);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_ptr_array_add (list, key);
  g_ptr_array_sort (list, indirect_strcmp);
  g_assert_cmpint (n, ==, list->len);
  desktop_file_index_builder_write_uint16 (builder, list->len);
  desktop_file_index_builder_write_uint16 (builder, 0xffff); /* padding */
  for (i = 0; i < n; i++)
    {
      gpointer ptr;

      g_hash_table_insert (strings, g_strdup (list->pdata[i]), GINT_TO_POINTER (i));
      ptr = g_hash_table_lookup (builder->string_table, list->pdata[i]);
      desktop_file_index_builder_write_uint32 (builder, GPOINTER_TO_UINT (ptr));
    }

  return offset;
}

static guint
desktop_file_index_builder_write_id (DesktopFileIndexBuilder *builder,
                                     GHashTable              *string_list,
                                     const gchar             *string)
{
  gpointer value;

  if (string == NULL)
    return desktop_file_index_builder_write_uint16 (builder, 0xffff);

  value = g_hash_table_lookup (string_list, string);
  g_assert (((void *) (gsize) (guint16) (gsize) value) == value);

  g_print ("id for '%s' is %d\n", string, (gint) (gsize) value);

  return desktop_file_index_builder_write_uint16 (builder, (gsize) value);
}

typedef struct
{
  gchar *key;
  gchar *locale;
  gchar *value;
} DesktopFileIndexKeyfileItem;

typedef struct
{
  gchar *name;
  guint  start;
} DesktopFileIndexKeyfileGroup;

typedef struct
{
  GPtrArray *groups;
  GPtrArray *items;
} DesktopFileIndexKeyfile;

static guint
desktop_file_index_builder_write_keyfile (DesktopFileIndexBuilder *builder,
                                          gpointer                 user_data)
{
  guint offset = desktop_file_index_builder_get_aligned (builder, sizeof (guint16));
  DesktopFileIndexKeyfile *keyfile = user_data;
  gint i;

  desktop_file_index_builder_write_uint16 (builder, keyfile->groups->len);
  desktop_file_index_builder_write_uint16 (builder, keyfile->items->len);

  for (i = 0; i < keyfile->groups->len; i++)
    {
      DesktopFileIndexKeyfileGroup *group = keyfile->groups->pdata[i];

      desktop_file_index_builder_write_id (builder, builder->group_names, group->name);
      desktop_file_index_builder_write_uint16 (builder, group->start);
    }

  for (i = 0; i < keyfile->items->len; i++)
    {
      DesktopFileIndexKeyfileItem *item = keyfile->items->pdata[i];

      desktop_file_index_builder_write_id (builder, builder->key_names, item->key);
      desktop_file_index_builder_write_id (builder, builder->locale_names, item->locale);
      desktop_file_index_builder_write_string (builder, item->value);
    }

  return offset;
}

typedef guint (* DesktopFileIndexBuilderFunc) (DesktopFileIndexBuilder *builder,
                                               gpointer                 user_data);

static guint
desktop_file_index_builder_write_pointer_array (DesktopFileIndexBuilder     *builder,
                                                GHashTable                  *key_table,
                                                guint                        key_table_offset,
                                                GHashTable                  *data_table,
                                                DesktopFileIndexBuilderFunc  func)
{
  GHashTableIter iter;
  gpointer key, value;
  guint *offsets;
  guint offset;
  gint n, i;

  n = g_hash_table_size (key_table);
  g_assert_cmpint (n, ==, g_hash_table_size (data_table));

  offsets = g_new0 (guint, n);

  g_hash_table_iter_init (&iter, key_table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      gpointer data;

      i = GPOINTER_TO_UINT (value);
      data = g_hash_table_lookup (data_table, key);
      offsets[i] = (* func) (builder, data);
    }

  offset = desktop_file_index_builder_get_aligned (builder, sizeof (guint32));

  desktop_file_index_builder_write_uint32 (builder, key_table_offset);

  for (i = 0; i < n; i++)
    {
      g_assert (offsets[i]);
      desktop_file_index_builder_write_uint32 (builder, offsets[i]);
    }

  g_free (offsets);

  return offset;
}

static void
desktop_file_index_builder_serialise (DesktopFileIndexBuilder *builder)
{
  guint32 header_fields[8] = { 0, };

  builder->string = g_string_new (NULL);

  /* Make room for the header */
  g_string_append_len (builder->string, (char *) header_fields, sizeof header_fields);

  /* Write out the string table, filling in the offsets */
  {
    GHashTableIter iter;
    gpointer key;

    g_hash_table_iter_init (&iter, builder->string_table);
    while (g_hash_table_iter_next (&iter, &key, NULL))
      {
        guint offset;

        offset = desktop_file_index_builder_write_raw_string (builder, key);
        g_hash_table_iter_replace (&iter, GUINT_TO_POINTER (offset));
      }
  }

  /* Write out the string lists */
  {
    header_fields[0] = desktop_file_index_builder_write_string_list (builder, builder->app_names);
    header_fields[1] = desktop_file_index_builder_write_string_list (builder, builder->key_names);
    header_fields[2] = desktop_file_index_builder_write_string_list (builder, builder->locale_names);
    header_fields[3] = desktop_file_index_builder_write_string_list (builder, builder->group_names);
  }

  /* Write out the group implementors */
  {
  }

  /* Write out the text indexes */
  {
  }

  /* Write out the desktop file contents */
  {
    header_fields[6] = desktop_file_index_builder_write_pointer_array (builder,
                                                                       builder->app_names,
                                                                       header_fields[0],
                                                                       builder->desktop_files,
                                                                       desktop_file_index_builder_write_keyfile);
  }

  /* Write out the mime types index */
  {
  }

  /* Replace the header */
  memcpy (builder->string->str, header_fields, sizeof header_fields); /* TODO: byteswap */
}

static void
desktop_file_index_keyfile_group_free (gpointer data)
{
  DesktopFileIndexKeyfileGroup *group = data;

  g_free (group->name);

  g_slice_free (DesktopFileIndexKeyfileGroup, group);
}

static void
desktop_file_index_keyfile_item_free (gpointer data)
{
  DesktopFileIndexKeyfileItem *item = data;

  g_free (item->key);
  g_free (item->locale);
  g_free (item->value);

  g_slice_free (DesktopFileIndexKeyfileItem, item);
}

static void
desktop_file_index_keyfile_free (gpointer data)
{
  DesktopFileIndexKeyfile *kf = data;

  g_ptr_array_free (kf->groups, TRUE);
  g_ptr_array_free (kf->items, TRUE);

  g_slice_free (DesktopFileIndexKeyfile, kf);
}

static void
desktop_file_index_builder_intern (DesktopFileIndexBuilder *builder,
                                   GHashTable              *id_table,
                                   const gchar             *string)
{
  if (string == NULL)
    return;

  if (id_table)
    g_hash_table_insert (id_table, g_strdup (string), NULL);

  g_hash_table_insert (builder->string_table, g_strdup (string), NULL);
}

static DesktopFileIndexKeyfile *
desktop_file_index_keyfile_new (DesktopFileIndexBuilder  *builder,
                                const gchar              *filename,
                                GError                  **error)
{
  DesktopFileIndexKeyfile *kf;
  gchar *contents;
  const gchar *c;
  gsize length;
  gint line = 1;

  if (!g_file_get_contents (filename, &contents, &length, error))
    return NULL;

  kf = g_slice_new (DesktopFileIndexKeyfile);
  kf->groups = g_ptr_array_new_with_free_func (desktop_file_index_keyfile_group_free);
  kf->items = g_ptr_array_new_with_free_func (desktop_file_index_keyfile_item_free);

  c = contents;
  while (*c)
    {
      gint line_length;

      line_length = strcspn (c, "\n");

      if (line_length == 0 || c[0] == '#')
        /* looks like a comment */
        ;

      else if (c[0] == '[')
        {
          DesktopFileIndexKeyfileGroup *kfg;
          gint group_size;

          group_size = strcspn (c + 1, "]");
          if (group_size != line_length - 2)
            {
              g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE,
                           "%s:%d: Invalid group line: ']' must be last character on line", filename, line);
              goto err;
            }

          kfg = g_slice_new (DesktopFileIndexKeyfileGroup);

          kfg->name = g_strndup (c + 1, group_size);
          kfg->start = kf->items->len;

          desktop_file_index_builder_intern (builder, builder->group_names, kfg->name);
          g_ptr_array_add (kf->groups, kfg);
        }

      else
        {
          DesktopFileIndexKeyfileItem *kfi;
          gsize key_size;
          const gchar *locale;
          gsize locale_size;
          const gchar *value;
          gsize value_size;

          key_size = strspn (c, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");

          if (key_size && c[key_size] == '[')
            {
              locale = c + key_size + 1;
              locale_size = strspn (locale, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@._");
              if (locale_size == 0 || locale[locale_size] != ']' || locale[locale_size + 1] != '=')
                {
                  g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE,
                               "%s:%d: Keys containing '[' must then have a locale name, then ']='", filename, line);
                  goto err;
                }
              value = locale + locale_size + 2;
              value_size = line_length - locale_size - key_size - 3; /* [ ] = */
            }
          else if (key_size && c[key_size] == '=')
            {
              locale = NULL;
              locale_size = 0;
              value = c + key_size + 1;
              value_size = line_length - key_size - 1; /* = */
            }
          else
            {
              g_set_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE,
                           "%s:%d: Lines must either be empty, comments, groups or assignments", filename, line);
              goto err;
            }

          kfi = g_slice_new (DesktopFileIndexKeyfileItem);
          kfi->key = g_strndup (c, key_size);
          kfi->locale = g_strndup (locale, locale_size);
          kfi->value = g_strndup (value, value_size);

          desktop_file_index_builder_intern (builder, builder->key_names, kfi->key);
          desktop_file_index_builder_intern (builder, builder->locale_names, kfi->locale);
          desktop_file_index_builder_intern (builder, NULL, kfi->value);

          g_ptr_array_add (kf->items, kfi);
        }

      c += line_length + 1;
      line++;
    }

  return kf;

err:
  g_ptr_array_free (kf->groups, TRUE);
  g_ptr_array_free (kf->items, TRUE);

  return NULL;
}

static DesktopFileIndexBuilder *
desktop_file_index_builder_new (void)
{
  DesktopFileIndexBuilder *builder;

  builder = g_slice_new (DesktopFileIndexBuilder);
  builder->string_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  builder->key_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  builder->group_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  builder->app_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  builder->locale_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  builder->desktop_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, desktop_file_index_keyfile_free);
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

  kf = desktop_file_index_keyfile_new (builder, filename, error);
  if (!kf)
    return FALSE;

  desktop_file_index_builder_intern (builder, builder->app_names, desktop_id);

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

      g_assert_no_error (error);
    }
  g_dir_close (dir);

  desktop_file_index_builder_serialise (builder);

  g_file_set_contents ("index.cache", builder->string->str, builder->string->len, &error);
  g_assert_no_error (error);

  return 0;
}
