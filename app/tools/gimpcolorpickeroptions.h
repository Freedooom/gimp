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

#ifndef  __GIMP_COLOR_PICKER_OPTIONS_H__
#define  __GIMP_COLOR_PICKER_OPTIONS_H__


#include "tool_options.h"


#define GIMP_TYPE_COLOR_PICKER_OPTIONS            (gimp_color_picker_options_get_type ())
#define GIMP_COLOR_PICKER_OPTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_COLOR_PICKER_OPTIONS, GimpColorPickerOptions))
#define GIMP_COLOR_PICKER_OPTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_COLOR_PICKER_OPTIONS, GimpColorPickerOptionsClass))
#define GIMP_IS_COLOR_PICKER_OPTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_COLOR_PICKER_OPTIONS))
#define GIMP_IS_COLOR_PICKER_OPTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_COLOR_PICKER_OPTIONS))
#define GIMP_COLOR_PICKER_OPTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_COLOR_PICKER_OPTIONS, GimpColorPickerOptionsClass))


typedef struct _GimpColorPickerOptions GimpColorPickerOptions;
typedef struct _GimpToolOptionsClass   GimpColorPickerOptionsClass;

struct _GimpColorPickerOptions
{
  GimpToolOptions  parent_instance;

  gboolean         sample_merged;
  gboolean         sample_merged_d;
  GtkWidget       *sample_merged_w;

  gboolean         sample_average;
  gboolean         sample_average_d;
  GtkWidget       *sample_average_w;

  gdouble          average_radius;
  gdouble          average_radius_d;
  GtkObject       *average_radius_w;

  gboolean         update_active;
  gboolean         update_active_d;
  GtkWidget       *update_active_w;
};


GType   gimp_color_picker_options_get_type (void) G_GNUC_CONST;

void    gimp_color_picker_options_gui      (GimpToolOptions *tool_options);


#endif  /*  __GIMP_COLOR_PICKER_OPTIONS_H__  */
