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

#include "config.h"

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "base/temp-buf.h"

#include "paint-funcs/paint-funcs.h"

#include "core/gimp.h"
#include "core/gimpbrush.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpgradient.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"

#include "gimppaintbrushtool.h"
#include "paint_options.h"

#include "libgimp/gimpintl.h"


#define PAINT_LEFT_THRESHOLD           0.05
#define PAINTBRUSH_DEFAULT_INCREMENTAL FALSE


static void   gimp_paintbrush_tool_class_init (GimpPaintbrushToolClass *klass);
static void   gimp_paintbrush_tool_init       (GimpPaintbrushTool      *tool);

static void   gimp_paintbrush_tool_paint      (GimpPaintTool        *paint_core,
					       GimpDrawable         *drawable,
					       PaintState            state);

static void   gimp_paintbrush_tool_motion     (GimpPaintTool        *paint_tool,
					       GimpDrawable         *drawable,
					       PaintPressureOptions *pressure_options,
					       PaintGradientOptions *gradient_options,
					       gdouble               fade_out,
					       gdouble               gradient_length,
					       gboolean              incremental,
					       GradientPaintMode     gradient_type);


/*  local variables  */
static gboolean non_gui_incremental = PAINTBRUSH_DEFAULT_INCREMENTAL;

static GimpPaintToolClass *parent_class = NULL;


/*  public functions  */

void
gimp_paintbrush_tool_register (Gimp                     *gimp,
                               GimpToolRegisterCallback  callback)
{
  (* callback) (gimp,
                GIMP_TYPE_PAINTBRUSH_TOOL,
                paint_options_new,
                TRUE,
                "gimp:paintbrush_tool",
                _("Paintbrush"),
                _("Paint fuzzy brush strokes"),
                N_("/Tools/Paint Tools/Paintbrush"), "P",
                NULL, "tools/paintbrush.html",
                GIMP_STOCK_TOOL_PAINTBRUSH);
}

GType
gimp_paintbrush_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpPaintbrushToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_paintbrush_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpPaintbrushTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_paintbrush_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_PAINT_TOOL,
					  "GimpPaintbrushTool",
                                          &tool_info, 0);
    }

  return tool_type;
}


/*  private functions  */

static void
gimp_paintbrush_tool_class_init (GimpPaintbrushToolClass *klass)
{
  GimpPaintToolClass *paint_tool_class;

  paint_tool_class = GIMP_PAINT_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  paint_tool_class->paint = gimp_paintbrush_tool_paint;
}

static void
gimp_paintbrush_tool_init (GimpPaintbrushTool *paintbrush)
{
  GimpTool      *tool;
  GimpPaintTool *paint_tool;

  tool       = GIMP_TOOL (paintbrush);
  paint_tool = GIMP_PAINT_TOOL (paintbrush);

  tool->tool_cursor = GIMP_PAINTBRUSH_TOOL_CURSOR;

  paint_tool->pick_colors  = TRUE;
  paint_tool->flags       |= TOOL_CAN_HANDLE_CHANGING_BRUSH;
}

static void
gimp_paintbrush_tool_paint (GimpPaintTool *paint_tool,
			    GimpDrawable  *drawable,
			    PaintState     state)
{
  PaintOptions         *paint_options;
  PaintPressureOptions *pressure_options;
  PaintGradientOptions *gradient_options;
  gboolean              incremental;
  GimpImage            *gimage;
  gdouble               fade_out;
  gdouble               gradient_length;
  gdouble               unit_factor;

  gimage = gimp_drawable_gimage (drawable);

  g_return_if_fail (gimage != NULL);

  if (! gimage)
    return;

  paint_options = (PaintOptions *) GIMP_TOOL (paint_tool)->tool_info->tool_options;

  if (paint_options)
    {
      pressure_options = paint_options->pressure_options;
      gradient_options = paint_options->gradient_options;
      incremental      = paint_options->incremental;
    }
  else
    {
      pressure_options = &non_gui_pressure_options;
      gradient_options = &non_gui_gradient_options;
      incremental      = non_gui_incremental;
    }

  switch (state)
    {
    case INIT_PAINT:
      break;

    case MOTION_PAINT:
      switch (gradient_options->fade_unit)
	{
	case GIMP_UNIT_PIXEL:
	  fade_out = gradient_options->fade_out;
	  break;
	case GIMP_UNIT_PERCENT:
	  fade_out = (MAX (gimage->width, gimage->height) *
		      gradient_options->fade_out / 100);
	  break;
	default:
	  unit_factor = gimp_unit_get_factor (gradient_options->fade_unit);
	  fade_out = (gradient_options->fade_out *
		      MAX (gimage->xresolution,
			   gimage->yresolution) / unit_factor);
	  break;
	}

      switch (gradient_options->gradient_unit)
	{
	case GIMP_UNIT_PIXEL:
	  gradient_length = gradient_options->gradient_length;
	  break;
	case GIMP_UNIT_PERCENT:
	  gradient_length = (MAX (gimage->width, gimage->height) *
			     gradient_options->gradient_length / 100);
	  break;
	default:
	  unit_factor = gimp_unit_get_factor (gradient_options->gradient_unit);
	  gradient_length = (gradient_options->gradient_length *
			     MAX (gimage->xresolution,
				  gimage->yresolution) / unit_factor);
	  break;
	}

      gimp_paintbrush_tool_motion (paint_tool, drawable,
				   pressure_options,
				   gradient_options,
				   gradient_options->use_fade ? fade_out : 0,
				   gradient_options->use_gradient ? gradient_length : 0,
				   incremental,
				   gradient_options->gradient_type);
      break;

    case FINISH_PAINT:
      break;

    default:
      break;
    }
}

static void
gimp_paintbrush_tool_motion (GimpPaintTool        *paint_tool,
			     GimpDrawable         *drawable,
			     PaintPressureOptions *pressure_options,
			     PaintGradientOptions *gradient_options,
			     gdouble               fade_out,
			     gdouble               gradient_length,
			     gboolean              incremental,
			     GradientPaintMode     gradient_type)
{
  GimpImage            *gimage;
  GimpContext          *context;
  TempBuf              *area;
  gdouble               x, paint_left;
  guchar                local_blend = OPAQUE_OPACITY;
  guchar                temp_blend  = OPAQUE_OPACITY;
  guchar                col[MAX_CHANNELS];
  GimpRGB               color;
  gint                  mode;
  gint                  opacity;
  gdouble               scale;
  PaintApplicationMode  paint_appl_mode = incremental ? INCREMENTAL : CONSTANT;

  if (! (gimage = gimp_drawable_gimage (drawable)))
    return;

  context = gimp_get_current_context (gimage->gimp);

  if (pressure_options->size)
    scale = paint_tool->cur_coords.pressure;
  else
    scale = 1.0;

  if (pressure_options->color)
    gradient_length = 1.0; /* not really used, only for if cases */

  /*  Get a region which can be used to paint to  */
  if (! (area = gimp_paint_tool_get_paint_area (paint_tool, drawable, scale)))
    return;

  /*  factor in the fade out value  */
  if (fade_out)
    {
      /*  Model the amount of paint left as a gaussian curve  */
      x = ((double) paint_tool->pixel_dist / fade_out);
      paint_left = exp (- x * x * 5.541);    /*  ln (1/255)  */
      local_blend = (int) (255 * paint_left);
    }

  if (local_blend)
    {
      /*  set the alpha channel  */
      temp_blend = local_blend;
      mode = gradient_type;

      if (gradient_length)
	{
          GimpGradient *gradient;

          gradient = gimp_context_get_gradient (context);

	  if (pressure_options->color)
	    gimp_gradient_get_color_at (gradient,
					paint_tool->cur_coords.pressure,
                                        &color);
	  else
	    gimp_paint_tool_get_color_from_gradient (paint_tool,
                                                     gradient,
                                                     gradient_length,
						     &color,
                                                     mode);

	  temp_blend = (gint) ((color.a * local_blend));

	  gimp_rgb_get_uchar (&color,
			      &col[RED_PIX],
			      &col[GREEN_PIX],
			      &col[BLUE_PIX]);
	  col[ALPHA_PIX] = OPAQUE_OPACITY;

	  /* always use incremental mode with gradients */
	  /* make the gui cool later */
	  paint_appl_mode = INCREMENTAL;
	  color_pixels (temp_buf_data (area), col,
			area->width * area->height, area->bytes);
	}
      /* we check to see if this is a pixmap, if so composite the
       * pixmap image into the area instead of the color
       */
      else if (paint_tool->brush && paint_tool->brush->pixmap)
	{
	  gimp_paint_tool_color_area_with_pixmap (paint_tool, gimage, drawable,
						  area,
						  scale, SOFT);
	  paint_appl_mode = INCREMENTAL;
	}
      else
	{
	  gimp_image_get_foreground (gimage, drawable, col);
	  col[area->bytes - 1] = OPAQUE_OPACITY;
	  color_pixels (temp_buf_data (area), col,
			area->width * area->height, area->bytes);
	}

      opacity = (gdouble) temp_blend;

      if (pressure_options->opacity)
	opacity = opacity * 2.0 * paint_tool->cur_coords.pressure;

      gimp_paint_tool_paste_canvas (paint_tool, drawable,
				    MIN (opacity, 255),
				    gimp_context_get_opacity (context) * 255,
				    gimp_context_get_paint_mode (context),
				    pressure_options->pressure ? PRESSURE : SOFT,
				    scale, paint_appl_mode);
    }
}


/*  non-gui stuff  */

static GimpPaintbrushTool *non_gui_paintbrush = NULL;

gboolean
gimp_paintbrush_tool_non_gui_default (GimpDrawable *drawable,
				      gint          num_strokes,
				      gdouble      *stroke_array)
{
  GimpPaintTool *paint_tool;
  gint           i;

  if (! non_gui_paintbrush)
    {
      non_gui_paintbrush = g_object_new (GIMP_TYPE_PAINTBRUSH_TOOL, NULL);
    }

  paint_tool = GIMP_PAINT_TOOL (non_gui_paintbrush);

  /* Hmmm... PDB paintbrush should have gradient type added to it!
   * thats why the code below is duplicated.
   */
  if (gimp_paint_tool_start (paint_tool, drawable,
			     stroke_array[0],
			     stroke_array[1]))
    {
      paint_tool->start_coords.x = paint_tool->last_coords.x = stroke_array[0];
      paint_tool->start_coords.y = paint_tool->last_coords.y = stroke_array[1];

      gimp_paint_tool_paint (paint_tool, drawable, MOTION_PAINT);

      for (i = 1; i < num_strokes; i++)
	{
	  paint_tool->cur_coords.x = stroke_array[i * 2 + 0];
	  paint_tool->cur_coords.y = stroke_array[i * 2 + 1];

	  gimp_paint_tool_interpolate (paint_tool, drawable);

	  paint_tool->last_coords.x = paint_tool->cur_coords.x;
	  paint_tool->last_coords.y = paint_tool->cur_coords.y;
	}

      gimp_paint_tool_finish (paint_tool, drawable);

      return TRUE;
    }

  return FALSE;
}

gboolean
gimp_paintbrush_tool_non_gui (GimpDrawable *drawable,
			      gint          num_strokes,
			      gdouble      *stroke_array,
			      gdouble       fade_out,
			      gint          method,
			      gdouble       gradient_length)
{
  GimpPaintTool *paint_tool;
  gint           i;

  if (! non_gui_paintbrush)
    {
      non_gui_paintbrush = g_object_new (GIMP_TYPE_PAINTBRUSH_TOOL, NULL);
    }

  paint_tool = GIMP_PAINT_TOOL (non_gui_paintbrush);

  /* Code duplicated above */
  if (gimp_paint_tool_start (paint_tool, drawable,
			     stroke_array[0],
			     stroke_array[1]))
    {
      non_gui_gradient_options.fade_out        = fade_out;
      non_gui_gradient_options.gradient_length = gradient_length;
      non_gui_gradient_options.gradient_type   = LOOP_TRIANGLE;
      non_gui_incremental                      = method;

      paint_tool->start_coords.x = paint_tool->last_coords.x = stroke_array[0];
      paint_tool->start_coords.y = paint_tool->last_coords.y = stroke_array[1];

      gimp_paint_tool_paint (paint_tool, drawable, MOTION_PAINT);

      for (i = 1; i < num_strokes; i++)
       {
         paint_tool->cur_coords.x = stroke_array[i * 2 + 0];
         paint_tool->cur_coords.y = stroke_array[i * 2 + 1];

         gimp_paint_tool_interpolate (paint_tool, drawable);

	 paint_tool->last_coords.x = paint_tool->cur_coords.x;
         paint_tool->last_coords.y = paint_tool->cur_coords.y;
       }

      gimp_paint_tool_finish (paint_tool, drawable);

      return TRUE;
    }

  return FALSE;
}
