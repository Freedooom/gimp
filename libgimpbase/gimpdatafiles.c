/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Datafiles module copyight (C) 1996 Federico Mena Quintero
 * federico@nuclecu.unam.mx
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#ifdef G_OS_WIN32
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif
#endif /* G_OS_WIN32 */

#include "gimpbasetypes.h"

#include "gimpdatafiles.h"
#include "gimpenv.h"


#ifdef G_OS_WIN32
/*
 * On Windows there is no concept like the Unix executable flag.
 * There is a weak emulation provided by the MS C Runtime using file
 * extensions (com, exe, cmd, bat). This needs to be extended to treat
 * scripts (Python, Perl, ...) as executables, too. We use the PATHEXT
 * variable, which is also used by cmd.exe.
 */
static gboolean
is_script (const gchar *filename)
{
  static gchar **exts = NULL;

  const gchar   *ext = strrchr (filename, '.');
  gchar         *pathext;
  gint           i;

  if (exts == NULL)
    {
      pathext = g_getenv ("PATHEXT");
      if (pathext != NULL)
	{
	  exts = g_strsplit (pathext, G_SEARCHPATH_SEPARATOR_S, 100);
	}
      else
	{
	  exts = g_new (gchar *, 1);
	  exts[0] = NULL;
	}
    }

  i = 0;
  while (exts[i] != NULL)
    {
      if (g_strcasecmp (ext, exts[i]) == 0)
	return TRUE;
      i++;
    }

  return FALSE;
}
#else  /* !G_OS_WIN32 */
#define is_script(filename) FALSE
#endif

gboolean
gimp_datafiles_check_extension (const gchar *filename,
				const gchar *extension)
{
  gint name_len;
  gint ext_len;

  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (extension != NULL, FALSE);

  name_len = strlen (filename);
  ext_len  = strlen (extension);

  if (! (name_len && ext_len && (name_len > ext_len)))
    return FALSE;

  return (g_ascii_strcasecmp (&filename[name_len - ext_len], extension) == 0);
}

void
gimp_datafiles_read_directories (const gchar            *path_str,
				 GFileTest               flags,
				 GimpDatafileLoaderFunc  loader_func,
				 gpointer                user_data)
{
  GimpDatafileData  file_data = { 0 };
  struct stat       filestat;
  gchar            *local_path;
  GList            *path;
  GList            *list;
  gchar            *filename;
  gint              err;
  GDir             *dir;
  const gchar      *dir_ent;

  g_return_if_fail (path_str != NULL);
  g_return_if_fail (loader_func != NULL);

  file_data.user_data = user_data;

  local_path = g_strdup (path_str);

#ifdef __EMX__
  /*
   *  Change drive so opendir works.
   */
  if (local_path[1] == ':')
    {
      _chdrive (local_path[0]);
    }
#endif

  path = gimp_path_parse (local_path, 16, TRUE, NULL);

  for (list = path; list; list = g_list_next (list))
    {
      dir = g_dir_open ((gchar *) list->data, 0, NULL);

      if (! dir)
	{
	  g_message ("error reading datafiles directory \"%s\"",
		     (gchar *) list->data);
	}
      else
	{
	  while ((dir_ent = g_dir_read_name (dir)))
	    {
	      filename = g_build_filename ((gchar *) list->data,
                                           dir_ent, NULL);

	      /* Check the file and see that it is not a sub-directory */
	      err = stat (filename, &filestat);

              file_data.filename = filename;
              file_data.atime    = filestat.st_atime;
              file_data.mtime    = filestat.st_mtime;
              file_data.ctime    = filestat.st_ctime;

	      if (! err)
		{
                  if (flags & G_FILE_TEST_EXISTS)
                    {
                      (* loader_func) (&file_data);
                    }
                  else if ((flags & G_FILE_TEST_IS_REGULAR) &&
                           S_ISREG (filestat.st_mode))
                    {
                      (* loader_func) (&file_data);
                    }
		  else if ((flags & G_FILE_TEST_IS_DIR) &&
                           S_ISDIR (filestat.st_mode))
		    {
		      (* loader_func) (&file_data);
		    }
#ifndef G_OS_WIN32
		  else if ((flags & G_FILE_TEST_IS_SYMLINK) &&
                           S_ISLNK (filestat.st_mode))
		    {
		      (* loader_func) (&file_data);
		    }
#endif
		  else if ((flags & G_FILE_TEST_IS_EXECUTABLE) &&
                           ((filestat.st_mode & S_IXUSR) ||
                            (S_ISREG (filestat.st_mode) &&
                             is_script (filename))))
		    {
		      (* loader_func) (&file_data);
		    }
		}

	      g_free (filename);
	    }

	  g_dir_close (dir);
	}
    }

  gimp_path_free (path);
  g_free (local_path);
}
