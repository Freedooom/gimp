/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontainerview-utils.h
 * Copyright (C) 2001 Michael Natterer
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

#ifndef __GIMP_CONTAINER_VIEW_UTILS_H__
#define __GIMP_CONTAINER_VIEW_UTILS_H__


/*  private  */

GimpItemGetNameFunc   gimp_container_view_get_built_in_name_func
                                            (GtkType              type);
gboolean              gimp_container_view_is_built_in_name_func
                                            (GimpItemGetNameFunc  get_name_func);


#endif  /*  __GIMP_CONTAINER_VIEW_UTILS_H__  */
