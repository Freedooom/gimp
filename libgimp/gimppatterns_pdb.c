/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2000 Peter Mattis and Spencer Kimball
 *
 * gimppatterns_pdb.c
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

#include "gimp.h"

gchar *
gimp_patterns_get_pattern_data (gchar   *name,
				gint    *width,
				gint    *height,
				gint    *mask_bpp,
				gint    *length,
				guint8 **mask_data)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gchar *ret_name = NULL;

  return_vals = gimp_run_procedure ("gimp_patterns_get_pattern_data",
				    &nreturn_vals,
				    GIMP_PDB_STRING, name,
				    GIMP_PDB_END);

  *length = 0;

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    {
      ret_name = g_strdup (return_vals[1].data.d_string);
      *width = return_vals[2].data.d_int32;
      *height = return_vals[3].data.d_int32;
      *mask_bpp = return_vals[4].data.d_int32;
      *length = return_vals[5].data.d_int32;
      *mask_data = g_new (guint8, *length);
      memcpy (*mask_data, return_vals[6].data.d_int8array,
	      *length * sizeof (guint8));
    }

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_name;
}
