/*
 * colorsel_triangle module (C) 1999 Simon Budig <Simon.Budig@unix-ag.org>
 *    http://www.home.unix-ag.org/simon/gimp/colorsel.html
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
 *
 * Ported to loadable colour selector interface by Austin Donnelly
 * <austin@gimp.org>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"

#include "gimpmodregister.h"

#include "libgimp/gimpcolorselector.h"
#include "libgimp/gimpmodule.h"

#include "libgimp/gimpintl.h"


/* prototypes */
static GtkWidget * colorsel_triangle_new  (const GimpHSV    *hsv,
					   const GimpRGB    *rgb,
					   gboolean          show_alpha,
					   GimpColorSelectorCallback callback,
					   gpointer          callback_data,
					   gpointer         *selector_data);

static void colorsel_triangle_free        (gpointer          selector_data);

static void colorsel_triangle_set_color   (gpointer          selector_data,
					   const GimpHSV    *hsv,
					   const GimpRGB    *rgb);
static void colorsel_xy_to_triangle_buf   (const gint        x,
					   const gint        y,
					   const gdouble     hue,
					   guchar           *buf,
					   const gint        sx,
					   const gint        sy,
					   const gint        vx,
					   const gint        vy,
					   const gint        hx,
					   const gint        hy);


/* local methods */
static GimpColorSelectorMethods methods = 
{
  colorsel_triangle_new,
  colorsel_triangle_free,
  colorsel_triangle_set_color,
  NULL  /*  set_channel  */
};


static GimpModuleInfo info =
{
  NULL,
  N_("Painter-style color selector as a pluggable color selector"),
  "Simon Budig <Simon.Budig@unix-ag.org>",
  "v0.03",
  "(c) 1999, released under the GPL",
  "17 Jan 1999"
};

static const GtkTargetEntry targets[] =
{
  { "application/x-color", 0 }
};


#define COLORWHEELRADIUS    (GIMP_COLOR_SELECTOR_SIZE / 2)
#define COLORTRIANGLERADIUS (COLORWHEELRADIUS - GIMP_COLOR_SELECTOR_BAR_SIZE)
#define PREVIEWSIZE         (2 * COLORWHEELRADIUS + 1)

#define BGCOLOR 180

#define PREVIEW_MASK   GDK_EXPOSURE_MASK | \
                       GDK_BUTTON_PRESS_MASK | \
                       GDK_BUTTON_RELEASE_MASK | \
                       GDK_BUTTON_MOTION_MASK 

typedef enum
{
  HUE = 0,
  SATURATION,
  VALUE,
  RED,
  GREEN,
  BLUE,
  ALPHA
} ColorSelectFillType;

struct _ColorSelect
{
  GimpHSV                    hsv;
  GimpRGB                    rgb;

  gdouble                    oldsat;
  gdouble                    oldval;
  gint                       mode;
  GtkWidget                 *preview;
  GimpColorSelectorCallback  callback;
  gpointer                   data;
};

typedef struct _ColorSelect ColorSelect;


static GtkWidget * create_preview                 (ColorSelect *coldata);

static void        update_previews                (ColorSelect *coldata,
						   gboolean     hue_changed);


/*************************************************************/

/* globaly exported init function */
G_MODULE_EXPORT GimpModuleStatus
module_init (GimpModuleInfo **inforet)
{
  GimpColorSelectorID id;

#ifndef __EMX__
  id = gimp_color_selector_register (_("Triangle"), "triangle.html", &methods);
#else
  id = mod_color_selector_register  (_("Triangle"), "triangle.html", &methods);
#endif

  if (id)
    {
      info.shutdown_data = id;
      *inforet = &info;
      return GIMP_MODULE_OK;
    }
  else
    {
      return GIMP_MODULE_UNLOAD;
    }
}

G_MODULE_EXPORT void
module_unload (gpointer                     shutdown_data,
	       GimpColorSelectorFinishedCB  completed_cb,
	       gpointer                     completed_data)
{
#ifndef __EMX__
  gimp_color_selector_unregister (shutdown_data, completed_cb, completed_data);
#else
  mod_color_selector_unregister (shutdown_data, completed_cb, completed_data);
#endif
}

/*************************************************************/
/* methods */

static GtkWidget *
colorsel_triangle_new (const GimpHSV             *hsv,
		       const GimpRGB             *rgb,
		       gboolean                   show_alpha,
		       GimpColorSelectorCallback  callback,
		       gpointer                   callback_data,
		       /* RETURNS: */
		       gpointer                  *selector_data)
{
  ColorSelect *coldata;
  GtkWidget   *preview;
  GtkWidget   *frame;
  GtkWidget   *hbox;
  GtkWidget   *vbox;

  coldata = g_new (ColorSelect, 1);

  coldata->hsv    = *hsv;
  coldata->rgb    = *rgb;

  coldata->oldsat = 0;
  coldata->oldval = 0;

  coldata->mode = 0;

  coldata->callback = callback;
  coldata->data     = callback_data;

  preview = create_preview (coldata);
  coldata->preview = preview;

  update_previews (coldata, TRUE);

  *selector_data = coldata;

  vbox = gtk_vbox_new (FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame), preview);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, FALSE, 0); 
  gtk_widget_show_all (vbox);

  return vbox;
}

static void
colorsel_triangle_free (gpointer selector_data)
{
  /* anything else needed to go? */
  g_free (selector_data);
}

static void
colorsel_triangle_set_color (gpointer       selector_data,
			     const GimpHSV *hsv,
			     const GimpRGB *rgb)
{
  ColorSelect *coldata;

  coldata = selector_data;

  coldata->hsv = *hsv;
  coldata->rgb = *rgb;

  update_previews (coldata, TRUE);
}


/*************************************************************/
/* helper functions */

static void 
update_previews (ColorSelect *coldata,
		 gint         hue_changed) 
{
  GtkWidget *preview;
  guchar     buf[3 * PREVIEWSIZE];
  gint       x, y, k, r2, dx, col;
  gint       x0, y0;
  gdouble    hue, sat, val, atn;
  gint       hx,hy, sx,sy, vx,vy;

  hue = (gdouble) coldata->hsv.h * 2 * G_PI;

  /* Colored point (value = 1, saturation = 1) */
  hx = RINT (sin (hue) * COLORTRIANGLERADIUS);
  hy = RINT (cos (hue) * COLORTRIANGLERADIUS);

  /* Black point (value = 0, saturation not important) */
  sx = RINT (sin (hue - 2 * G_PI / 3) * COLORTRIANGLERADIUS);
  sy = RINT (cos (hue - 2 * G_PI / 3) * COLORTRIANGLERADIUS);

  /* White point (value = 1, saturation = 0) */
  vx = RINT (sin (hue + 2 * G_PI / 3) * COLORTRIANGLERADIUS);
  vy = RINT (cos (hue + 2 * G_PI / 3) * COLORTRIANGLERADIUS);

  hue = coldata->hsv.h * 360.0;
  preview = coldata->preview;

  if (hue_changed)
    {
      for (y = COLORWHEELRADIUS; y >= -COLORWHEELRADIUS; y--)
	{
	  dx = RINT (sqrt (fabs (COLORWHEELRADIUS * COLORWHEELRADIUS - y * y)));
	  for (x = -dx, k = 0; x <= dx; x++)
	    {
	      buf[k] = buf[k+1] = buf[k+2] = BGCOLOR;
	      r2 = (x * x) + (y * y);

	      if (r2 <= COLORWHEELRADIUS * COLORWHEELRADIUS)
		{
		  if (r2 > COLORTRIANGLERADIUS * COLORTRIANGLERADIUS) 
		    { 
		      atn = atan2 (x, y);
		      if (atn < 0)
			atn = atn + 2 * G_PI;
		      gimp_hsv_to_rgb4 (buf + k, atn / (2 * G_PI), 1, 1);
		    }
		  else
		    {
		      colorsel_xy_to_triangle_buf (x, y, hue, buf + k, hx, hy, sx, sy, vx, vy);
		    }
		}

	      k += 3;
	    }

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf,
				COLORWHEELRADIUS - dx,
				COLORWHEELRADIUS - y, 2 * dx + 1);
	}
  
      /* marker in outer ring */
  
      x0 = RINT (sin (hue * G_PI / 180) *
		 ((gdouble) (COLORWHEELRADIUS - COLORTRIANGLERADIUS + 1) / 2 +
		  COLORTRIANGLERADIUS));
      y0 = RINT (cos (hue * G_PI / 180) *
		 ((gdouble) (COLORWHEELRADIUS - COLORTRIANGLERADIUS + 1) / 2 +
		  COLORTRIANGLERADIUS));

      atn = atan2 (x0, y0);
      if (atn < 0)
	atn = atn + 2 * G_PI;
      gimp_hsv_to_rgb4 (buf, atn / (2 * G_PI), 1, 1);

      col = INTENSITY (buf[0], buf[1], buf[2]) > 127 ? 0 : 255;
  
      for (y = y0 - 4 ; y <= y0 + 4 ; y++)
	{
	  for (x = x0 - 4, k=0 ; x <= x0 + 4 ; x++)
	    {
	      r2 = (x - x0) * (x - x0) + (y - y0) * (y - y0);

	      if (r2 <= 20 && r2 >= 6)
		{
		  buf[k] = buf[k+1] = buf[k+2] = col;
		}
	      else
		{
		  atn = atan2 (x, y);
		  if (atn < 0)
		    atn = atn + 2 * G_PI;
		  gimp_hsv_to_rgb4 (buf + k, atn / (2 * G_PI), 1, 1);
		}

	      k += 3;
	    }

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf,
				COLORWHEELRADIUS + x0 - 4,
				COLORWHEELRADIUS - y, 9);
	}
    }
  else
    {
      /* delete marker in triangle */
  
      sat = coldata->oldsat;
      val = coldata->oldval;
      x0 = RINT (sx + (vx - sx) * val + (hx - vx) * sat * val);
      y0 = RINT (sy + (vy - sy) * val + (hy - vy) * sat * val);

      for (y = y0 - 4 ; y <= y0 + 4 ; y++)
	{
	  for (x = x0 - 4, k=0 ; x <= x0 + 4 ; x++)
	    {
	      buf[k] = buf[k+1] = buf[k+2] = BGCOLOR;
	      r2 = (x - x0) * (x - x0) + (y - y0) * (y - y0);

	      if (x * x + y * y > COLORTRIANGLERADIUS * COLORTRIANGLERADIUS)
		{
		  atn = atan2 (x, y);
		  if (atn < 0)
		    atn = atn + 2 * G_PI;
		  gimp_hsv_to_rgb4 (buf + k, atn / (2 * G_PI), 1, 1);
		}
	      else
		{
		  colorsel_xy_to_triangle_buf (x, y, hue, buf + k, hx, hy, sx, sy, vx, vy);
		}

	      k += 3;
	    }

	  gtk_preview_draw_row (GTK_PREVIEW (preview), buf,
				COLORWHEELRADIUS + x0 - 4,
				COLORWHEELRADIUS - y, 9);
	}
    }

  /* marker in triangle */

  col = gimp_rgb_intensity (&coldata->rgb) > 0.5 ? 0 : 255;

  sat = coldata->oldsat = coldata->hsv.s;
  val = coldata->oldval = coldata->hsv.v;

  x0 = RINT (sx + (vx - sx) * val + (hx - vx) * sat * val);
  y0 = RINT (sy + (vy - sy) * val + (hy - vy) * sat * val);

  for (y = y0 - 4 ; y <= y0 + 4 ; y++)
    {
      for (x = x0 - 4, k=0 ; x <= x0 + 4 ; x++)
	{
	  buf[k] = buf[k+1] = buf[k+2] = BGCOLOR;
	  r2 = (x - x0) * (x - x0) + (y - y0) * (y - y0);

	  if (r2 <= 20 && r2 >= 6)
	    {
	      buf[k] = buf[k+1] = buf[k+2] = col;
	    }
	  else
	    {
	      if (x * x + y * y > COLORTRIANGLERADIUS * COLORTRIANGLERADIUS)
		{
		  atn = atan2 (x, y);
		  if (atn < 0)
		    atn = atn + 2 * G_PI;
		  gimp_hsv_to_rgb4 (buf + k, atn / (2 * G_PI), 1, 1);
		}
	      else
		{
		  colorsel_xy_to_triangle_buf (x, y, hue, buf + k, hx, hy, sx, sy, vx, vy);
		}
	    }

	  k += 3;
	}

      gtk_preview_draw_row (GTK_PREVIEW (preview), buf,
			    COLORWHEELRADIUS + x0 - 4,
			    COLORWHEELRADIUS - y, 9);
    }

  gtk_widget_draw (preview, NULL);
}

static void
colorsel_xy_to_triangle_buf (const gint x, const gint y,
			     const gdouble hue, guchar *buf,
			     const gint hx, const gint hy, /* colored point */
			     const gint sx, const gint sy, /* black point */
			     const gint vx, const gint vy) /* white point */
{
  gdouble sat, val;

  /*
   * The value is 1 - (the distance from the H->V line).
   * I forgot the linear algebra behind it...
   */
  val = (gdouble) ( (x - sx) * (hy - vy) -  (y - sy) * (hx - vx)) /
	(gdouble) ((vx - sx) * (hy - vy) - (vy - sy) * (hx - vx));

  if (val >= 0 && val<= 1)
    {
      if (abs (hy - vy) < abs (hx - vx))
	{
	  sat = (val == 0 ? 0: ((gdouble) (x - sx - val * (vx - sx)) /
					  (val * (hx - vx))));
	}
      else
	{
	  sat = (val == 0 ? 0: ((gdouble) (y - sy - val * (vy - sy)) /
					  (val * (hy - vy))));
	}

      /* Yes, this ugly 1.00*01 fixes some subtle rounding errors... */
      if (sat >= 0 && sat <= 1.000000000000001) 
	gimp_hsv_to_rgb4 (buf, hue / 360, sat, val);
    } 
}

/*
 * Color Preview
 */

static gint
color_selection_callback (GtkWidget *widget, 
			  GdkEvent  *event)
{
  ColorSelect *coldata;
  gint         x,y, angle, mousex, mousey;
  gdouble      r;
  gdouble      hue, sat, val;
  gint         hx,hy, sx,sy, vx,vy;

  coldata = g_object_get_data (G_OBJECT (widget), "colorselect");

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      gtk_grab_add (widget);
      x = event->button.x - COLORWHEELRADIUS - 1;
      y = - event->button.y + COLORWHEELRADIUS + 1;
      r = sqrt ((gdouble) (x * x + y * y));
      angle = ((gint) RINT (atan2 (x, y) / G_PI * 180) + 360 ) % 360;
      if ( /* r <= COLORWHEELRADIUS  && */ r > COLORTRIANGLERADIUS) 
        coldata->mode = 1;  /* Dragging in the Ring */
      else
        coldata->mode = 2;  /* Dragging in the Triangle */
      break;

    case GDK_MOTION_NOTIFY:
      x = event->motion.x - COLORWHEELRADIUS - 1;
      y = - event->motion.y + COLORWHEELRADIUS + 1;
      r = sqrt ((gdouble) (x * x + y * y));
      angle = ((gint) RINT (atan2 (x, y) / G_PI * 180) + 360 ) % 360;
      break;

    case GDK_BUTTON_RELEASE:
      coldata->mode = 0;
      gtk_grab_remove (widget);

      /* callback the user */
      (* coldata->callback) (coldata->data,
			     &coldata->hsv,
			     &coldata->rgb);
      
      return FALSE;
      break;

    default:
      gtk_widget_get_pointer (widget, &x, &y);
      x = x - COLORWHEELRADIUS - 1;
      y = - y + COLORWHEELRADIUS + 1;
      r = sqrt ((gdouble) (x * x + y * y));
      angle = ((gint) RINT (atan2 (x, y) / G_PI * 180) + 360 ) % 360;
      break;
    }

  gtk_widget_get_pointer (widget, &mousex, &mousey);
  if ((event->type == GDK_MOTION_NOTIFY &&
      (mousex != event->motion.x || mousey != event->motion.y)))
     return FALSE;

  if (coldata->mode == 1 ||
      (r > COLORWHEELRADIUS &&
       (abs (angle - coldata->hsv.h * 360.0) < 30 ||
	abs (abs (angle - coldata->hsv.h * 360.0) - 360) < 30)))
    {
      coldata->hsv.h = angle / 360.0;
      gimp_hsv_to_rgb (&coldata->hsv, &coldata->rgb);
      update_previews (coldata, TRUE);
    }
  else
    {
      hue = coldata->hsv.h * 2 * G_PI;
      hx = sin (hue) * COLORTRIANGLERADIUS;
      hy = cos (hue) * COLORTRIANGLERADIUS;
      sx = sin (hue - 2 * G_PI / 3) * COLORTRIANGLERADIUS;
      sy = cos (hue - 2 * G_PI / 3) * COLORTRIANGLERADIUS;
      vx = sin (hue + 2 * G_PI / 3) * COLORTRIANGLERADIUS;
      vy = cos (hue + 2 * G_PI / 3) * COLORTRIANGLERADIUS;
      hue = coldata->hsv.h * 360.0;

      if ((x - sx) * vx + (y - sy) * vy < 0)
	{
	  sat = 1;
	  val = ((gdouble) ( (x - sx) * (hx - sx) +  (y - sy) * (hy - sy)))
	                 / ((hx - sx) * (hx - sx) + (hy - sy) * (hy - sy));
	  if (val < 0)
	    val = 0;
	  else if (val > 1)
	    val = 1;
	}
      else if ((x - sx) * hx + (y - sy) * hy < 0)
	{
	  sat = 0;
	  val = ((gdouble) ( (x - sx) * (vx - sx) +  (y - sy) * (vy - sy)))
	                 / ((vx - sx) * (vx - sx) + (vy - sy) * (vy - sy));
	  if (val < 0)
	    val = 0;
	  else if (val > 1)
	    val = 1;
	}
      else if ((x - hx) * sx + (y - hy) * sy < 0)
	{
	  val = 1;
	  sat = ((gdouble) ( (x - vx) * (hx - vx) +  (y - vy) * (hy - vy)))
	                 / ((hx - vx) * (hx - vx) + (hy - vy) * (hy - vy));
	  if (sat < 0)
	    sat = 0;
	  else if (sat > 1)
	    sat = 1;
	}
      else
	{
	  val =   (gdouble) ( (x - sx) * (hy - vy) -  (y - sy) * (hx - vx))
	        / (gdouble) ((vx - sx) * (hy - vy) - (vy - sy) * (hx - vx));
	  if (val <= 0)
	    {
	      val = 0;
	      sat = 0;
	    }
	  else
	    {
	      if (val > 1)
		val = 1;
	      if (hy == vy)
		sat = (gdouble) (x - sx - val * (vx - sx)) / (val * (gdouble) (hx - vx));
	      else
		sat = (gdouble) (y - sy - val * (vy - sy)) / (val * (gdouble) (hy - vy));
	      if (sat < 0)
		sat = 0;
	      else if (sat > 1)
		sat = 1;
	    }
	}

    coldata->hsv.s = sat;
    coldata->hsv.v = val;
    gimp_hsv_to_rgb (&coldata->hsv, &coldata->rgb);
    update_previews (coldata, FALSE);
  }

  /* callback the user */
  (* coldata->callback) (coldata->data,
			 &coldata->hsv,
			 &coldata->rgb);

  return FALSE;
}

static GtkWidget *
create_preview (ColorSelect *coldata)
{
  GtkWidget *preview;
  guchar     buf[3 * PREVIEWSIZE];
  gint       i;

  preview = gtk_preview_new (GTK_PREVIEW_COLOR);
  gtk_preview_set_dither (GTK_PREVIEW (preview), GDK_RGB_DITHER_MAX);
  gtk_widget_set_events (GTK_WIDGET (preview), PREVIEW_MASK );
  gtk_preview_size (GTK_PREVIEW (preview), PREVIEWSIZE, PREVIEWSIZE);

  g_object_set_data (G_OBJECT (preview), "colorselect", coldata);

  g_signal_connect (G_OBJECT (preview), "motion_notify_event",
                    G_CALLBACK (color_selection_callback),
                    NULL);
  g_signal_connect (G_OBJECT (preview), "button_press_event",
                    G_CALLBACK (color_selection_callback),
                    NULL);
  g_signal_connect (G_OBJECT (preview), "button_release_event",
                    G_CALLBACK (color_selection_callback),
                    NULL);

  for (i=0; i < 3 * PREVIEWSIZE; i += 3)
    buf[i] = buf[i+1] = buf[i+2] = BGCOLOR;
  for (i=0; i < PREVIEWSIZE; i++) 
    gtk_preview_draw_row (GTK_PREVIEW (preview), buf, 0, i, PREVIEWSIZE);

  gtk_widget_draw (preview, NULL);

  return preview;
}
