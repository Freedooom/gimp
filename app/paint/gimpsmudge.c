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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "gdk/gdkkeysyms.h"
#include "appenv.h"
#include "drawable.h"
#include "errors.h"
#include "smudge.h"
#include "gdisplay.h"
#include "gimplut.h"
#include "paint_funcs.h"
#include "paint_core.h"
#include "paint_options.h"
#include "selection.h"
#include "tool_options_ui.h"
#include "tools.h"
#include "gimage.h"

#define ROUND(x) (int)((x) + .5)

#include "libgimp/gimpintl.h"

/*  the smudge structures  */

typedef struct _SmudgeOptions SmudgeOptions;
struct _SmudgeOptions
{
  PaintOptions  paint_options;

  double        pressure;
  double        pressure_d;
  GtkObject    *pressure_w;

};

static PixelRegion accumPR;
static unsigned char *accum_data;

/*  the smudge tool options  */
static SmudgeOptions * smudge_options = NULL;

static void         smudge_motion 	(PaintCore *, GimpDrawable *);
static void 	    smudge_init   	(PaintCore *, GimpDrawable *);
static void 	    smudge_finish   	(PaintCore *, GimpDrawable *);

static void 
smudge_nonclipped_painthit_coords (PaintCore *paint_core,
	gint * x, gint* y, gint* w, gint *h);
static void
smudge_allocate_accum_buffer ( gint w, gint h, 
	gint bytes, gint do_fill);

/* functions  */

static void
smudge_options_reset (void)
{
  SmudgeOptions *options = smudge_options;

  paint_options_reset ((PaintOptions *) options);

  gtk_adjustment_set_value (GTK_ADJUSTMENT (options->pressure_w),
			    options->pressure_d);
}

static SmudgeOptions *
smudge_options_new (void)
{
  SmudgeOptions *options;

  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *scale;

  /*  the new smudge tool options structure  */
  options = (SmudgeOptions *) g_malloc (sizeof (SmudgeOptions));
  paint_options_init ((PaintOptions *) options,
		      SMUDGE,
		      smudge_options_reset);

  options->pressure = options->pressure_d = 50.0;

  /*  the main vbox  */
  vbox = ((ToolOptions *) options)->main_vbox;

  /*  the pressure scale  */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Pressure:"));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  options->pressure_w =
    gtk_adjustment_new (options->pressure_d, 0.0, 100.0, 1.0, 1.0, 0.0);
  scale = gtk_hscale_new (GTK_ADJUSTMENT (options->pressure_w));
  gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
  gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DELAYED);
  gtk_signal_connect (GTK_OBJECT (options->pressure_w), "value_changed",
		      (GtkSignalFunc) tool_options_double_adjustment_update,
		      &options->pressure);
  gtk_widget_show (scale);
  gtk_widget_show (hbox);

  return options;
}

void *
smudge_paint_func (PaintCore    *paint_core,
		     GimpDrawable *drawable,
		     int           state)
{
  switch (state)
    {
    case INIT_PAINT:
      smudge_init (paint_core, drawable);
      break;
    case MOTION_PAINT:
      smudge_motion (paint_core, drawable);
      break;
    case FINISH_PAINT:
      smudge_finish (paint_core, drawable);
      break;
    }

  return NULL;
}

static void
smudge_finish ( PaintCore *paint_core,
		 GimpDrawable * drawable)
{
  if (accum_data)
  {
    g_free (accum_data);
    accum_data = NULL;
  }
}

static void 
smudge_nonclipped_painthit_coords (PaintCore *paint_core,
	gint * x, gint* y, gint* w, gint *h)
{
  /* Note: these are the brush mask size plus a border of 1 pixel */
  *x = (gint) paint_core->curx - paint_core->brush->mask->width/2 - 1;
  *y = (gint) paint_core->cury - paint_core->brush->mask->height/2 - 1;
  *w = paint_core->brush->mask->width + 2;
  *h = paint_core->brush->mask->height + 2;
}

static void
smudge_init ( PaintCore *paint_core,
		 GimpDrawable * drawable)
{
  GImage *gimage;
  TempBuf * area;
  PixelRegion  srcPR;
  gint x,y,w,h;
  gint was_clipped;

  /*  adjust the x and y coordinates to the upper left corner of the brush  */
  smudge_nonclipped_painthit_coords (paint_core, &x, &y, &w, &h);
  
  if (! (gimage = drawable_gimage (drawable)))
    return;

  /*  If the image type is indexed, don't smudge  */
  if ((drawable_type (drawable) == INDEXED_GIMAGE) ||
      (drawable_type (drawable) == INDEXEDA_GIMAGE))
    return;

  area = paint_core_get_paint_area (paint_core, drawable);

  if (!area)
    was_clipped = TRUE;
  else if (x != area->x || y != area->y || w != area->width || h != area->height)
    was_clipped = TRUE;
  else 
    was_clipped = FALSE;

  smudge_allocate_accum_buffer (w,h, 
	drawable_bytes(drawable), was_clipped);

  if (!area) return;

  accumPR.x = area->x - x; 
  accumPR.y = area->y - y;
  accumPR.w = area->width;
  accumPR.h = area->height;
  accumPR.rowstride = accumPR.bytes * w;
  accumPR.data = accum_data 
	+ accumPR.rowstride * accumPR.y 
	+ accumPR.x * accumPR.bytes;

  pixel_region_init (&srcPR, drawable_data (drawable), 
	    area->x, area->y, area->width, area->height, FALSE);

  /* copy the region under the original painthit. */
  copy_region (&srcPR, &accumPR);

  accumPR.x = 0; 
  accumPR.y = 0;
  accumPR.w = area->width;
  accumPR.h = area->height;
  accumPR.rowstride = accumPR.bytes * accumPR.w;
  accumPR.data = accum_data;
}

static void
smudge_allocate_accum_buffer (
				gint w, 
				gint h, 
				gint bytes, 
				gint do_fill
				)
{ 
  /*  Allocate the accumulation buffer */
  accumPR.bytes = bytes;
  accum_data = g_malloc (w * h * bytes);
 
  if (do_fill)
  {
    guchar color[3] = {0,0,0};
    accumPR.x = 0; 
    accumPR.y = 0;
    accumPR.w = w;
    accumPR.h = h;
    accumPR.rowstride = accumPR.bytes * w;
    accumPR.data = accum_data;
    color_region (&accumPR, (const guchar*)&color);
  }
}

Tool *
tools_new_smudge ()
{
  Tool * tool;
  PaintCore * private;

  /*  The tool options  */
  if (! smudge_options)
    {
      smudge_options = smudge_options_new ();
      tools_register (SMUDGE, (ToolOptions *) smudge_options);

      /*  press all default buttons  */
      smudge_options_reset ();
    }

  tool = paint_core_new (SMUDGE);
  /*tool->modifier_key_func = smudge_modifier_key_func;*/

  private = (PaintCore *) tool->private;
  private->paint_func = smudge_paint_func;

  return tool;
}

void
tools_free_smudge (Tool *tool)
{
  paint_core_free (tool);
}

static void
smudge_motion (PaintCore *paint_core,
		 GimpDrawable *drawable)
{
  GImage *gimage;
  TempBuf * area;
  PixelRegion srcPR, destPR, tempPR;
  gfloat pressure;
  gfloat brush_opacity;
  gint x,y,w,h;

  if (! (gimage = drawable_gimage (drawable)))
    return;

  /*  If the image type is indexed, don't smudge  */
  if ((drawable_type (drawable) == INDEXED_GIMAGE) ||
      (drawable_type (drawable) == INDEXEDA_GIMAGE))
    return;

  smudge_nonclipped_painthit_coords (paint_core, &x, &y, &w, &h);

  /*  Get the paint area */
  if (! (area = paint_core_get_paint_area (paint_core, drawable)))
    return;

  /* srcPR will be the pixels under the current painthit from 
     the drawable*/

  pixel_region_init (&srcPR, drawable_data (drawable), 
	area->x, area->y, area->width, area->height, FALSE);

  brush_opacity = PAINT_OPTIONS_GET_OPACITY (smudge_options);
  pressure = (smudge_options->pressure)/100.0;
 
  /* The tempPR will be the built up buffer (for smudge) */ 
  tempPR.bytes = accumPR.bytes;
  tempPR.rowstride = accumPR.rowstride;
  tempPR.x = area->x - x; 
  tempPR.y = area->y - y;
  tempPR.w = area->width;
  tempPR.h = area->height;
  tempPR.data = accum_data 
	+ tempPR.rowstride * tempPR.y 
	+ tempPR.x * tempPR.bytes;

  /* The dest will be the paint area we got above (= canvas_buf) */    

  destPR.bytes = area->bytes;                                     
  destPR.x = 0; destPR.y = 0;                                     
  destPR.w = area->width;                                         
  destPR.h = area->height;                                        
  destPR.rowstride = area->width * area->bytes;                  
  destPR.data = temp_buf_data (area); 

  /*  
     Smudge uses the buffer Accum.
     For each successive painthit Accum is built like this
	Accum =  pressure*Accum  + (1-pressure)*I.
     where I is the pixels under the current painthit. 
     Then the paint area (canvas_buf) is built as 
	(Accum,1) (if no alpha),
  */

  blend_region (&srcPR, &tempPR, &tempPR, ROUND(pressure * 255.0));

  /* re-init the tempPR */

  tempPR.bytes = accumPR.bytes;
  tempPR.rowstride = accumPR.rowstride;
  tempPR.x = area->x - x; 
  tempPR.y = area->y - y;
  tempPR.w = area->width;
  tempPR.h = area->height;
  tempPR.data = accum_data 
	+ tempPR.rowstride * tempPR.y 
	+ tempPR.x * tempPR.bytes;

  if (!drawable_has_alpha (drawable))                             
    add_alpha_region (&tempPR, &destPR);                          
  else                                                            
    copy_region(&tempPR, &destPR);

  /*Replace the newly made paint area to the gimage*/ 
  paint_core_replace_canvas (paint_core, drawable, ROUND(brush_opacity * 255.0),
				OPAQUE_OPACITY, PRESSURE, INCREMENTAL);
 
}

static void *
smudge_non_gui_paint_func (PaintCore *paint_core,
			     GimpDrawable *drawable,
			     int        state)
{
  smudge_motion (paint_core, drawable);

  return NULL;
}

gboolean
smudge_non_gui (GimpDrawable *drawable,
    		  double        pressure,
		  int           num_strokes,
		  double       *stroke_array)
{
  int i;

  if (paint_core_init (&non_gui_paint_core, drawable,
		       stroke_array[0], stroke_array[1]))
    {
      /* Set the paint core's paint func */
      non_gui_paint_core.paint_func = smudge_non_gui_paint_func;

      non_gui_paint_core.startx = non_gui_paint_core.lastx = stroke_array[0];
      non_gui_paint_core.starty = non_gui_paint_core.lasty = stroke_array[1];

      if (num_strokes == 1)
	smudge_non_gui_paint_func (&non_gui_paint_core, drawable, 0);

      for (i = 1; i < num_strokes; i++)
	{
	  non_gui_paint_core.curx = stroke_array[i * 2 + 0];
	  non_gui_paint_core.cury = stroke_array[i * 2 + 1];

	  paint_core_interpolate (&non_gui_paint_core, drawable);

	  non_gui_paint_core.lastx = non_gui_paint_core.curx;
	  non_gui_paint_core.lasty = non_gui_paint_core.cury;
	}

      /* Finish the painting */
      paint_core_finish (&non_gui_paint_core, drawable, -1);

      /* Cleanup */
      paint_core_cleanup ();
      return TRUE;
    }
  else
    return FALSE;
}
