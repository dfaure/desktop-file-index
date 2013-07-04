#include "common.h"

#include <string.h>

typedef struct
{
  GHashTable *string_table;
  GHashTable *key_names;
  GHashTable *group_names;
  GHashTable *app_names;
  GHashTable *locale_names;

  GString    *string;
} DesktopFileIndexBuilder;

static guint
desktop_file_index_builder_get_offset (DesktopFileIndexBuilder *builder)
{
  return builder->string->len;
}

static guint
desktop_file_index_builder_write_uint16 (DesktopFileIndexBuilder *builder,
                                         guint16                  value)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  value = GUINT16_TO_LE (value);

  g_string_append_len (builder->string, (gpointer) &value, sizeof value);

  return offset;
}

static guint
desktop_file_index_builder_write_uint32 (DesktopFileIndexBuilder *builder,
                                         guint32                  value)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  value = GUINT32_TO_LE (value);

  g_string_append_len (builder->string, (gpointer) &value, sizeof value);

  return offset;
}

static guint
desktop_file_index_builder_write_string (DesktopFileIndexBuilder *builder,
                                         const gchar             *string)
{
  guint offset = desktop_file_index_builder_get_offset (builder);

  g_string_append (builder->string, string);
  g_string_append_c (builder->string, '\0');

  return offset;
}

static guint
desktop_file_index_builder_write_string_offset (DesktopFileIndexBuilder *builder,
                                                const gchar             *string)
{
  gpointer offset;

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
  guint offset = desktop_file_index_builder_get_offset (builder);
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
  for (i = 0; i < n; i++)
    {
      gpointer ptr;

      g_hash_table_insert (strings, list->pdata[i], GINT_TO_POINTER (i));
      ptr = g_hash_table_lookup (builder->string_table, list->pdata[i]);
      desktop_file_index_builder_write_uint32 (builder, GPOINTER_TO_UINT (ptr));
    }

  return offset;
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
desktop_file_index_builder_write_key_file (DesktopFileIndexBuilder *builder,
                                           DesktopFileIndexKeyfile *keyfile)
{
  guint offset = desktop_file_index_builder_get_offset (builder);
  gint i;

  desktop_file_index_builder_write_uint16 (builder, keyfile->groups->len);
  desktop_file_index_builder_write_uint16 (builder, keyfile->items->len);

  for (i = 0; i < keyfile->groups; i++)
    {
      DesktopFileIndexKeyfileGroup *group = keyfile->groups->pdata[i];

      desktop_file_index_builder_write_id (builder, builder->group_names, group->name);
      desktop_file_index_builder_write_uint16 (builder, group->start);
    }

  for (i = 0; i < keyfile->items; i++)
    {
      DesktopFileIndexKeyfileItem *item = keyfile->items->pdata[i];

      desktop_file_index_builder_write_id (builder, builder->key_names, item->key);
      desktop_file_index_builder_write_id (builder, builder->locale_names, item->locale);
      desktop_file_index_builder_write_string (builder, item->value);
    }
}

static void
serialise (void)
{
  guint32 header_fields[8];
  GString *file_contents;

  file_contents = g_string_new (NULL);

  /* Make room for the header */
  {
    gchar zero[sizeof (struct dfi_header)] = { 0, };
    g_string_append_len (file_contents, zero, sizeof zero);
  }

  /* Write out the string table, filling in the offsets */
  {
    GHashTableIter iter;
    gpointer key;

    g_hash_table_iter_init (&iter, string_table);
    while (g_hash_table_iter_next (&iter, &key, NULL))
      {
        guint offset;

        offset = file_contents->len;
        g_string_append (file_contents, key);
        g_string_append_c (file_contents, '\0');
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
  /* Write out the mime types index */
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

static DesktopFileIndexKeyfile *
desktop_file_index_keyfile_new (DesktopFileIndexBuilder  *builder,
                                GError                  **error)
{
  DesktopFileIndexKeyfile *kf;
  gchar *contents;
  const gchar *c;
  gint line = 1;

  if (!g_file_get_contents (filename, &contents, &length, error))
    return NULL;

  kf = g_slice_new (DesktopFileIndexKeyfile);
  kf->groups = g_ptr_array_new_with_free_func (desktop_file_index_keyfile_group_free);
  kf->items = g_ptr_array_new_with_free_func (desktop_file_index_keyfile_item_free);

  c = contents;
  while (*c)
    {
      gchar *group_name;
      gint line_length;

      line_length = strcspn (c, "\n");

      if (line_length == 0 || c[0] == '#')
        /* looks like a comment */
        ;

      else if (c[0] == '[')
        {
          gchar *group_name;
          gint group_size;

          group_size = strcspn (c + 1, "]");
          if (group_size != line_length - 2)
            {
              g_set_error ("%s:%d: Invalid group line: ']' must be last character on line", filename, i);
              goto err;
            }

          g_ptr_array_add (kf->groups, desktop_file_index_keyfile_group_new (c + 1, group_size, kf->items->len));
          g_hash_table_replace (builder->string_table, g_strndup (c + 1, group_size), NULL);
        }

      else
        {
          gsize key_size;
          const gchar *locale;
          gsize locale_size;
          const gchar *value;
          gsize value_size;

          key_size = strspn (c, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");

          if (key_size && c[key_size] == '[')
            {
              locale = c + key_size + 1;
              locale_size = strspn (locale, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_");
              if (locale_size == 0 || locale[locale_size] != ']' || locale[locale_size + 1] != '=')
                {
                  g_set_error ("%s:%d: Keys containing '[' must then have a locale name, then ']='", filename, i);
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
              g_set_error ("%s:%d: Lines must either be empty, comments, groups or assignments", filename, i);
              goto err;
            }

          g_hash_table_replace (builder->string_table, g_strndup (c, key_size), NULL);
          g_hash_table_replace (builder->string_table, g_strndup (locale, locale_size), NULL);
          g_hash_table_replace (builder->string_table, g_strndup (value, value_size), NULL);
          g_ptr_array_add (kf->items, desktop_file_index_keyfile_item_new (c, key_size,
                                                                           locale, locale_size,
                                                                           value, value_size));
        }

      c += line_length + 1;
      line++;
    }
}
