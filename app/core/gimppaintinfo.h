/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#ifndef __GIMP_PAINT_INFO_H__
#define __GIMP_PAINT_INFO_H__


#include "gimpdata.h"


#define GIMP_TYPE_PAINT_INFO            (gimp_paint_info_get_type ())
#define GIMP_PAINT_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PAINT_INFO, GimpPaintInfo))
#define GIMP_PAINT_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PAINT_INFO, GimpPaintInfoClass))
#define GIMP_IS_PAINT_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PAINT_INFO))
#define GIMP_IS_PAINT_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PAINT_INFO))
#define GIMP_PAINT_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PAINT_INFO, GimpPaintInfoClass))


typedef struct _GimpPaintInfoClass GimpPaintInfoClass;

struct _GimpPaintInfo
{
  GimpObject        parent_instance;

  Gimp             *gimp;

  GType             paint_type;
  GType             paint_options_type;

  gchar            *pdb_string;

  GimpPaintOptions *paint_options;
};

struct _GimpPaintInfoClass
{
  GimpObjectClass  parent_class;
};


GType           gimp_paint_info_get_type (void) G_GNUC_CONST;

GimpPaintInfo * gimp_paint_info_new      (Gimp        *gimp,
                                          GType        paint_type,
                                          GType        paint_options_type,
                                          const gchar *pdb_string);


#endif  /*  __GIMP_PAINT_INFO_H__  */
