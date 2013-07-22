#include <sys/mman.h>
#include <glib.h>

int
main (int argc, char **argv)
{
  GError *error = NULL;
  GMappedFile *mapped;
  gpointer contents;
  gint n_pages;
  guchar *pages;
  gsize size;
  gint i;

  mapped = g_mapped_file_new (argv[1], FALSE, &error);
  g_assert_no_error (error);
  contents = g_mapped_file_get_contents (mapped);
  size = g_mapped_file_get_length (mapped);
  n_pages = (size + 4095) / 4096;
  pages = g_new0 (guchar, n_pages);
  mincore (contents, size, pages);
  for (i = 0; i < n_pages; i++)
    g_print ("%c", (pages[i] & 1) ? '#' : '.');
  g_print ("\n");
  madvise (contents, size, MADV_DONTNEED);
  mincore (contents, size, pages);
  for (i = 0; i < n_pages; i++)
    g_print ("%c", (pages[i] & 1) ? '#' : '.');
  g_print ("\n");

  return 0;
}
