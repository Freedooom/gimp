/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpfileops_pdb.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* NOTE: This file is autogenerated by pdbgen.pl */

#ifndef __GIMP_FILEOPS_PDB_H__
#define __GIMP_FILEOPS_PDB_H__

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */


gint32   gimp_file_load                   (GimpRunMode  run_mode,
					   const gchar *filename,
					   const gchar *raw_filename);
gboolean gimp_file_save                   (GimpRunMode  run_mode,
					   gint32       image_ID,
					   gint32       drawable_ID,
					   const gchar *filename,
					   const gchar *raw_filename);
gchar*   gimp_temp_name                   (const gchar *extension);
gboolean gimp_register_magic_load_handler (const gchar *procedure_name,
					   const gchar *extensions,
					   const gchar *prefixes,
					   const gchar *magics);
gboolean gimp_register_load_handler       (const gchar *procedure_name,
					   const gchar *extensions,
					   const gchar *prefixes);
gboolean gimp_register_save_handler       (const gchar *procedure_name,
					   const gchar *extensions,
					   const gchar *prefixes);
gboolean gimp_register_file_handler_mime  (const gchar *procedure_name,
					   const gchar *mime_type);


G_END_DECLS

#endif /* __GIMP_FILEOPS_PDB_H__ */
