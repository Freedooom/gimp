/* The GIMP -- an image manipulation program
 *
 * This file Copyright (C) 1999 Simon Budig
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

/*
 * This function is to get a set of npoints different coordinates for
 * the range from start to end (each in the range from 0 to 1 and
 * start < end.
 * returns the number of created coords. Make sure that the points-
 * Array is allocated.
 */

#ifndef __PATH_BEZIER_H__
#define __PATH_BEZIER_H__

#include <gdk/gdk.h>
#include <glib.h>

#include "path_toolP.h"

guint
path_bezier_get_points (PathTool *path_tool,
                        PathSegment *segment,
			GdkPoint *points,
			guint npoints,
			gdouble start,
			gdouble end);

void
path_bezier_get_point (PathTool *path_tool,
		       PathSegment *segment,
		       gdouble position,
		       gdouble *x,
		       gdouble *y);

void
path_bezier_draw_handles (Tool *tool,
			  PathSegment *segment);
			  
void
path_bezier_draw_segment (Tool *tool,
			  PathSegment *segment);
			  

gdouble
path_bezier_on_segment (Tool *tool,
			PathSegment *segment,
			gint x,
			gint y,
			gint halfwidth,
			gint *distance);

void
path_bezier_drag_segment (PathTool *path_tool,
			  PathSegment *segment,
			  gdouble position,
			  gint x,
			  gint y);

gboolean
path_bezier_on_handles (PathTool *path_tool,
			PathSegment *segment,
			gint x,
			gint y,
			gint halfwidth);

void
path_bezier_drag_handles (PathTool *path_tool,
			  PathSegment *segment,
			  gint x,
			  gint y);

PathSegment *
path_bezier_insert_anchor (PathTool *path_tool,
			   PathSegment *segment,
			   gdouble position);

void
path_bezier_update_segment (PathTool *path_tool,
			    PathSegment *segment);

void
path_bezier_flip_segment (PathTool *path_tool,
			  PathSegment *segment);

#endif /*  __PATH_BEZIER_H__ */

			
