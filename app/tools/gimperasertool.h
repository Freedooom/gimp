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

#ifndef __GIMP_ERASER_TOOL_H__
#define __GIMP_ERASER_TOOL_H__


#include "gimppainttool.h"


#define GIMP_TYPE_ERASER_TOOL            (gimp_eraser_tool_get_type ())
#define GIMP_ERASER_TOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_ERASER_TOOL, GimpEraserTool))
#define GIMP_ERASER_TOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_ERASER_TOOL, GimpEraserToolClass))
#define GIMP_IS_ERASER_TOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_ERASER_TOOL))
#define GIMP_IS_ERASER_TOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_ERASER_TOOL))
#define GIMP_ERASER_TOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_ERASER_TOOL, GimpEraserToolClass))


typedef struct _GimpEraserTool      GimpEraserTool;
typedef struct _GimpEraserToolClass GimpEraserToolClass;

struct _GimpEraserTool
{
  GimpPaintTool parent_instance;
};

struct _GimpEraserToolClass
{
  GimpPaintToolClass parent_class;
};


void    gimp_eraser_tool_register (Gimp                     *gimp,
                                   GimpToolRegisterCallback  callback);

GType   gimp_eraser_tool_get_type (void) G_GNUC_CONST;


gboolean   eraser_non_gui            (GimpDrawable *drawable,
                                      gint          num_strokes,
                                      gdouble      *stroke_array,
                                      gint          hardness,
                                      gint          method,
                                      gboolean      anti_erase);
gboolean   eraser_non_gui_default    (GimpDrawable *paint_core,
                                      gint          num_strokes,
                                      gdouble      *stroke_array);


#endif  /*  __GIMP_ERASER_TOOL_H__  */
