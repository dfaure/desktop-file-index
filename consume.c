/*
 * Copyright © 2013 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "common.h"

#include <string.h>

/* DesktopFileIndex struct {{{1 */
typedef struct
{
  GBytes                       *bytes;
  const gchar                  *data;
  guint                         file_size;

  const struct dfi_string_list *app_names;
  const struct dfi_string_list *key_names;
  const struct dfi_string_list *locale_names;
  const struct dfi_string_list *group_names;

  const dfi_pointer            *implementors;    /* id lists, associated with group_names */
  const dfi_pointer            *text_indexes;    /* text indexes, associated with locale_names */
  const dfi_pointer            *desktop_files;   /* desktop files, associated with app_names */

  const struct dfi_text_index  *mime_types;
} DesktopFileIndex;

/* dfi_uint16, dfi_uint32 {{{1 */

static guint
dfi_uint16_get (dfi_uint16 value)
{
  return GUINT16_FROM_LE (value.le);
}

static guint
dfi_uint32_get (dfi_uint32 value)
{
  return GUINT32_FROM_LE (value.le);
}

/* dfi_pointer {{{1 */
static gconstpointer
dfi_pointer_dereference (const DesktopFileIndex *dfi,
                         dfi_pointer             pointer,
                         gint                    min_size)
{
  guint offset = dfi_uint32_get (pointer.offset);

  /* Check to make sure we don't wrap */
  if (offset + min_size < min_size)
    return NULL;

  /* Check to make sure we don't pass the end of the file */
  if (offset + min_size > dfi->file_size)
    return NULL;

  return dfi->data + offset;
}

/* dfi_string {{{1 */

static gboolean
dfi_string_is_flagged (dfi_string string)
{
  return (dfi_uint32_get(string.offset) & (1u << 31)) != 0;
}

static const gchar *
dfi_string_get (const DesktopFileIndex *dfi,
                dfi_string              string)
{
  guint32 offset = dfi_uint32_get (string.offset);

  offset &= ~(1u << 31);

  if (offset < dfi->file_size)
    return dfi->data + offset;
  else
    return "";
}

/* dfi_id, dfi_id_list {{{1 */

static gboolean
dfi_id_valid (dfi_id id)
{
  return dfi_uint16_get (id) != 0xffff;
}

static guint
dfi_id_get (dfi_id id)
{
  return dfi_uint16_get (id);
}

static const dfi_id *
dfi_id_list_get_ids (const struct dfi_id_list *list,
                     gint                     *n_ids)
{
  if (list == NULL)
    {
      *n_ids = 0;
      return NULL;
    }

  *n_ids = dfi_uint16_get (list->n_ids);

  return list->ids;
}

static const struct dfi_id_list *
dfi_id_list_from_pointer (const DesktopFileIndex *dfi,
                          dfi_pointer             pointer)
{
  const struct dfi_id_list *list;
  guint need_size;

  need_size = sizeof (dfi_uint16);

  list = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!list)
    return NULL;

  /* n_ids is 16bit, so no overflow danger */
  need_size += sizeof (dfi_id) * dfi_uint16_get (list->n_ids);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

/* dfi_string_list {{{1 */

static const struct dfi_string_list *
dfi_string_list_from_pointer (const DesktopFileIndex *dfi,
                              dfi_pointer             pointer)
{
  const struct dfi_string_list *list;
  guint need_size;

  need_size = sizeof (dfi_uint16);

  list = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!list)
    return NULL;

  /* n_strings is 16bit, so no overflow danger */
  need_size += sizeof (dfi_string) * dfi_uint16_get (list->n_strings);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

static gint
dfi_string_list_binary_search (const DesktopFileIndex       *dfi,
                               const struct dfi_string_list *list,
                               const gchar                  *string)
{
  guint l, r;

  l = 0;
  r = dfi_uint16_get (list->n_strings);

  while (l < r)
    {
      guint m;
      gint x;

      m = l + (r - l) / 2;

      x = strcmp (string, dfi_string_get (dfi, list->strings[m]));

      if (x > 0)
        l = m + 1;
      else if (x < 0)
        r = m;
      else
        return m;
    }

  return -1;
}

static guint
dfi_string_list_get_length (const struct dfi_string_list *list)
{
  return dfi_uint16_get (list->n_strings);
}

static const gchar *
dfi_string_list_get_string (const DesktopFileIndex       *dfi,
                            const struct dfi_string_list *list,
                            dfi_id                        id)
{
  gint i = dfi_id_get (id);

  if (list == NULL)
    return NULL;

  if (i < dfi_uint16_get (list->n_strings))
    return dfi_string_get (dfi, list->strings[i]);
  else
    return "";
}

static const dfi_pointer *
dfi_string_list_get_associated_pointer_array (const DesktopFileIndex       *dfi,
                                              const struct dfi_string_list *list,
                                              dfi_pointer                   pointer)
{
  return dfi_pointer_dereference (dfi, pointer, sizeof (dfi_pointer) * dfi_string_list_get_length (list));
}

/* dfi_text_index, dfi_text_index_item {{{1 */

static const struct dfi_text_index *
dfi_text_index_from_pointer (const DesktopFileIndex *dfi,
                             dfi_pointer             pointer)
{
  const struct dfi_text_index *text_index;
  guint need_size;
  guint n_items;

  need_size = sizeof (dfi_uint16);

  text_index = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!text_index)
    return NULL;

  /* It's 32 bit, so make sure this won't overflow when we multiply */
  n_items = dfi_uint32_get (text_index->n_items);
  if (n_items > (1u << 24))
    return NULL;

  need_size += sizeof (struct dfi_text_index_item) * n_items;

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

static const gchar *
dfi_text_index_get_string (const DesktopFileIndex      *dfi,
                           const struct dfi_text_index *text_index,
                           dfi_id                       id)
{
  guint i = dfi_id_get (id);

  if G_UNLIKELY (text_index == NULL)
    return "";

  if (i < dfi_uint32_get (text_index->n_items))
    return dfi_string_get (dfi, text_index->items[i].key);
  else
    return "";
}

static const struct dfi_text_index_item *
dfi_text_index_binary_search (const DesktopFileIndex      *dfi,
                              const struct dfi_text_index *text_index,
                              const gchar                 *string)
{
  guint l, r;

  if G_UNLIKELY (text_index == NULL)
    return NULL;

  l = 0;
  r = dfi_uint32_get (text_index->n_items);

  while (l < r)
    {
      guint m;
      gint x;

      m = l + (r - l) / 2;

      x = strcmp (string, dfi_string_get (dfi, text_index->items[m].key));

      if (x > 0)
        l = m + 1;
      else if (x < 0)
        r = m;
      else
        return text_index->items + m;
    }

  return NULL;
}

static const dfi_id *
dfi_text_index_item_get_ids (const DesktopFileIndex           *dfi,
                             const struct dfi_text_index_item *item,
                             gint                             *n_results)
{
  if (item == NULL)
    return NULL;

  if (dfi_string_is_flagged (item->key))
    {
      if (!dfi_id_valid (item->value.pair[0]))
        {
          *n_results = 0;
          return NULL;
        }
      else if (!dfi_id_valid (item->value.pair[1]))
        {
          *n_results = 1;
          return item->value.pair;
        }
      else
        {
          *n_results = 2;
          return item->value.pair;
        }
    }
  else
    return dfi_id_list_get_ids (dfi_id_list_from_pointer (dfi, item->value.pointer), n_results);
}

static const dfi_id *
dfi_text_index_get_ids_for_exact_match (const DesktopFileIndex      *dfi,
                                        const struct dfi_text_index *index,
                                        const gchar                 *string,
                                        gint                        *n_results)
{
  const struct dfi_text_index_item *item;

  item = dfi_text_index_binary_search (dfi, index, string);

  return dfi_text_index_item_get_ids (dfi, item, n_results);
}

/* dfi_desktop_file, dfi_desktop_group, dfi_desktop_line {{{1 */
static const struct dfi_desktop_file *
dfi_desktop_file_from_pointer (const DesktopFileIndex *dfi,
                               dfi_pointer             pointer)
{
  const struct dfi_desktop_file *file;
  guint need_size;

  need_size = sizeof (struct dfi_desktop_file);

  file = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!file)
    return NULL;

  /* All sizes 16bit ints, so no overflow danger */
  need_size += sizeof (struct dfi_desktop_group) * dfi_uint16_get (file->n_groups);
  need_size += sizeof (struct dfi_desktop_group); /* EOF group */
  need_size += sizeof (struct dfi_desktop_item) * dfi_uint16_get (file->n_items);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

static const struct dfi_desktop_file_group *
dfi_desktop_file_get_groups (const DesktopFileIndex        *dfi,
                             const struct dfi_desktop_file *file,
                             gint                          *n_groups)
{
  *n_groups = dfi_uint16_get (file->n_groups);

  return G_STRUCT_MEMBER_P (file, sizeof (struct dfi_desktop_file));
}

static const gchar *
dfi_desktop_file_group_get_name (const DesktopFileIndex         *dfi,
                                 const struct dfi_desktop_group *group)
{
  return dfi_string_list_get_string (dfi, dfi->group_names, group->name_id);
}

static const struct dfi_desktop_file_item *
dfi_desktop_file_group_get_items (const DesktopFileIndex         *dfi,
                                  const struct dfi_desktop_file  *file,
                                  const struct dfi_desktop_group *group,
                                  gint                           *n_items)
{
  guint start, end;

  start = dfi_uint16_get (group->items_index);
  end = dfi_uint16_get (group[1].items_index);

  if (start <= end && end <= dfi_uint16_get (file->n_items))
    {
      *n_items = end - start;
      return G_STRUCT_MEMBER_P (file, sizeof (struct dfi_desktop_file) +
                                      sizeof (struct dfi_desktop_group) * dfi_uint16_get (file->n_groups) +
                                      sizeof (struct dfi_desktop_group) +
                                      start);
    }
  else
    {
      *n_items = 0;
      return NULL;
    }
}

static const gchar *
dfi_desktop_file_item_get_key (const DesktopFileIndex        *dfi,
                               const struct dfi_desktop_item *item)
{
  return dfi_string_list_get_string (dfi, dfi->key_names, item->key_id);
}

static const gchar *
dfi_desktop_file_item_get_locale (const DesktopFileIndex        *dfi,
                                  const struct dfi_desktop_item *item)
{
  return dfi_string_list_get_string (dfi, dfi->locale_names, item->locale_id);
}

static const gchar *
dfi_desktop_file_item_get_value (const DesktopFileIndex        *dfi,
                                 const struct dfi_desktop_item *item)
{
  return dfi_string_get (dfi, item->value);
}

/* dfi_header {{{1 */

const struct dfi_header *
dfi_header_get (const DesktopFileIndex *dfi)
{
  dfi_pointer ptr = { };

  return dfi_pointer_dereference (dfi, ptr, sizeof (struct dfi_header));
}

/* DesktopFileIndex implementation {{{1 */

DesktopFileIndex *
desktop_file_index_new (GBytes *bytes)
{
  const struct dfi_header *header;
  DesktopFileIndex *dfi;
  gsize size;

  dfi = g_slice_new (DesktopFileIndex);
  dfi->data = g_bytes_get_data (bytes, &size);
  if (size > G_MAXINT)
    goto err;
  dfi->file_size = size;

  header = dfi_header_get (dfi);
  if (!header)
    goto err;

  dfi->app_names = dfi_string_list_from_pointer (dfi, header->app_names);
  dfi->key_names = dfi_string_list_from_pointer (dfi, header->key_names);
  dfi->locale_names = dfi_string_list_from_pointer (dfi, header->locale_names);
  dfi->group_names = dfi_string_list_from_pointer (dfi, header->group_names);

  if (!dfi->app_names || !dfi->key_names || !dfi->locale_names || !dfi->group_names)
   goto err;

  dfi->implementors = dfi_string_list_get_associated_pointer_array (dfi, dfi->group_names, header->implementors);
  dfi->text_indexes = dfi_string_list_get_associated_pointer_array (dfi, dfi->locale_names, header->text_indexes);
  dfi->desktop_files = dfi_string_list_get_associated_pointer_array (dfi, dfi->app_names, header->desktop_files);
  dfi->mime_types = dfi_text_index_from_pointer (dfi, header->mime_types);

  if (!dfi->mime_types || !dfi->implementors || !dfi->text_indexes || !dfi->desktop_files)
    goto err;

  dfi->bytes = g_bytes_ref (bytes);

  return dfi;

err:
  g_slice_free (DesktopFileIndex, dfi);
  return NULL;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
