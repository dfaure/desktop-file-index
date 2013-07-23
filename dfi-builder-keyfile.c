#include "dfi-builder-keyfile.h"

#include <string.h>

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

struct _DesktopFileIndexKeyfile
{
  GPtrArray *groups;
  GPtrArray *items;
};

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

void
desktop_file_index_keyfile_free (DesktopFileIndexKeyfile *keyfile)
{
  g_ptr_array_free (keyfile->groups, TRUE);
  g_ptr_array_free (keyfile->items, TRUE);

  g_slice_free (DesktopFileIndexKeyfile, keyfile);
}

guint
desktop_file_index_keyfile_get_n_groups (DesktopFileIndexKeyfile *keyfile)
{
  return keyfile->groups->len;
}

guint
desktop_file_index_keyfile_get_n_items (DesktopFileIndexKeyfile *keyfile)
{
  return keyfile->items->len;
}

const gchar *
desktop_file_index_keyfile_get_group_name (DesktopFileIndexKeyfile *keyfile,
                                           guint                    group)
{
  DesktopFileIndexKeyfileGroup *kfg;

  kfg = keyfile->groups->pdata[group];

  return kfg->name;
}

void
desktop_file_index_keyfile_get_group_range (DesktopFileIndexKeyfile *keyfile,
                                            guint                    group,
                                            guint                   *start,
                                            guint                   *end)
{
  DesktopFileIndexKeyfileGroup *kfg;

  kfg = keyfile->groups->pdata[group];
  *start = kfg->start;

  if (end)
    {
      if (group == keyfile->groups->len - 1)
        *end = keyfile->items->len;
      else
        {
          kfg = keyfile->groups->pdata[group + 1];
          *end = kfg->start;
        }
    }
}

void
desktop_file_index_keyfile_get_item (DesktopFileIndexKeyfile  *keyfile,
                                     guint                     item,
                                     const gchar             **key,
                                     const gchar             **locale,
                                     const gchar             **value)
{
  DesktopFileIndexKeyfileItem *kfi;

  kfi = keyfile->items->pdata[item];

  *key = kfi->key;
  *locale = kfi->locale;
  *value = kfi->value;
}

const gchar *
desktop_file_index_keyfile_get_value (DesktopFileIndexKeyfile *keyfile,
                                      const gchar * const     *locale_variants,
                                      const gchar             *group_name,
                                      const gchar             *key)
{
  gint start = 0, end = 0;
  gint i;

  /* Find group... */
  for (i = 0; i < keyfile->groups->len; i++)
    {
      DesktopFileIndexKeyfileGroup *group = keyfile->groups->pdata[i];

      if (g_str_equal (group->name, group_name))
        {
          start = group->start;

          if (i < keyfile->groups->len - 1)
            {
              DesktopFileIndexKeyfileGroup *next_group;

              next_group = keyfile->groups->pdata[i + 1];
              end = next_group->start;
            }
          else
            end = keyfile->items->len;
        }
    }

  /* For each locale variant... */
  for (i = 0; locale_variants[i]; i++)
    {
      gint j;

      for (j = start; j < end; j++)
        {
          DesktopFileIndexKeyfileItem *item = keyfile->items->pdata[j];

          /* There are more unique locales than there are keys, so check
           * those first.
           */
          if (item->locale && g_str_equal (item->locale, locale_variants[i]) && g_str_equal (item->key, key))
            return item->value;
        }
    }

  /* Try the NULL locale as a fallback */
  for (i = start; i < end; i++)
    {
      DesktopFileIndexKeyfileItem *item = keyfile->items->pdata[i];

      if (item->locale == NULL && g_str_equal (item->key, key))
        return item->value;
    }

  return NULL;
}

DesktopFileIndexKeyfile *
desktop_file_index_keyfile_new (const gchar  *filename,
                                GError      **error)
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
              locale = "";
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

          g_ptr_array_add (kf->items, kfi);
        }

      c += line_length;

      /* May have unterminated lines... */
      if (*c == '\n')
        c++;

      line++;
    }

  return kf;

err:
  g_ptr_array_free (kf->groups, TRUE);
  g_ptr_array_free (kf->items, TRUE);

  return NULL;
}
