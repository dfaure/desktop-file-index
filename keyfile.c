static gboolean
desktop_file_index_keyfile_scan (const gchar                      *filename,
                                 DesktopFileIndexKeyfileCallback   callback,
                                 GError                          **error)
{
  gchar *contents;
  const gchar *c;
  gint line = 1;

  if (!g_file_get_contents (filename, &contents, &length, error))
    return NULL;

  c = contents;
  while (*c)
    {
      gint line_length;

      line_length = strcspn (c, "\n");

      if (line_length == 0 || c[0] == '#')
        ;

      else if (c[0] == '[')
        {
          gint group_size;

          group_size = strcspn (c + 1, "]");
          if (group_size != line_length - 2)
            {
              g_set_error ("%s:%d: Invalid group line: ']' must be last character on line", filename, i);
              g_free (contents);
              return FALSE;
            }


        }

      else
        {
        }

      c += line_length + 1;
      line++;
    }
}
