/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Gradient Map plug-in
 * Copyright (C) 1997 Eiichi Takamori <taka@ma1.seikyou.ne.jp>
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
 * version 1.06
 * This plug-in requires GIMP v0.99.10 or above.
 *
 * This plug-in maps the image using active gradient. (See help_string
 * in query ()).
 *
 *	Eiichi Takamori <taka@ma1.seikyou.ne.jp>
 *	http://ha1.seikyou.ne.jp/home/taka/gimp/
 *
 * Changes from version 1.05 to version 1.06:
 * - Fixed bug that completely white pixel (= grayscale 255) was not
 *   mapped properly.  (Thanks to Dov Grobgeld)
 *
 * Changes from version 1.04 to version 1.05:
 * - Now it uses gimp_gradients_sample_uniform () instead of blend
 *   tool. Maybe right thing.
 *
 * Changes from revision 1.1 to version 1.04:
 * - Fix bug that it didn't work with alpha channel.
 * - Changed calling `gimp_blend' so that it works in v0.99.9.
 * - Changed calling `gimp_blend' so that it works with Quartic's
 *   asupsample patch.
 * - Fix the way to calculate luminosity of RGB image.
 *   (Thanks to Michael Lamertz)
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <libgimp/gimp.h>

#include "libgimp/stdplugins-intl.h"


#ifdef RCSID
static char rcsid[] = "$Id$";
#endif

/* Some useful macros */

#define NSAMPLES	256
#define TILE_CACHE_SIZE 32
#define LUMINOSITY(X)	(INTENSITY (X[0], X[1], X[2]))

/* Declare a local function.
 */
static void	 query		(void);
static void	 run		(gchar	 *name,
				 gint	 nparams,
				 GimpParam	 *param,
				 gint	 *nreturn_vals,
				 GimpParam	 **return_vals);

static void	 gradmap	(GimpDrawable *drawable);
static guchar *	 get_samples	(GimpDrawable *drawable );


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,	 /* init_proc  */
  NULL,	 /* quit_proc  */
  query, /* query_proc */
  run,	 /* run_proc   */
};

MAIN ()

static void
query()
{
  static GimpParamDef args[]=
    {
      { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
      { GIMP_PDB_IMAGE, "image", "Input image (unused)" },
      { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" }
   };
  static gint nargs = sizeof (args) / sizeof (args[0]);

  gimp_install_procedure ("plug_in_gradmap",
                          "Map the contents of the specified drawable with "
			  "active gradient",
                          " This plug-in maps the contents of the specified "
			  "drawable with active gradient. It calculates "
			  "luminosity of each pixel and replaces the pixel "
			  "by the sample of active gradient at the position "
			  "proportional to that luminosity. Complete black "
			  "pixel becomes the leftmost color of the gradient, "
			  "and complete white becomes the rightmost. Works on "
			  "both Grayscale and RGB image with/without alpha "
			  "channel.",
			  "Eiichi Takamori",
			  "Eiichi Takamori",
			  "1997",
			  N_("<Image>/Filters/Colors/Map/Gradient Map"),
			  "RGB*, GRAY*",
			  GIMP_PLUGIN,
			  nargs, 0,
			  args, NULL);
}

static void
run (gchar   *name,
     gint    nparams,
     GimpParam  *param,
     gint    *nreturn_vals,
     GimpParam  **return_vals)
{
  static GimpParam values[1];
  GimpDrawable *drawable;
  GimpRunModeType run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  run_mode = param[0].data.d_int32;

  INIT_I18N();

  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /*  Get the specified drawable  */
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  /*  Make sure that the drawable is gray or RGB color	*/
  if (gimp_drawable_is_rgb (drawable->drawable_id) ||
      gimp_drawable_is_gray (drawable->drawable_id))
    {
      gimp_progress_init ( _("Gradient Map..."));
      gimp_tile_cache_ntiles (TILE_CACHE_SIZE);

      gradmap (drawable);

      if (run_mode != GIMP_RUN_NONINTERACTIVE)
	gimp_displays_flush ();
    }
  else
    {
      /* g_message ("gradmap: cannot operate on indexed color images"); */
      status = GIMP_PDB_EXECUTION_ERROR;
    }

  values[0].data.d_status = status;

  gimp_drawable_detach (drawable);
}

static void
gradmap (GimpDrawable *drawable)
{
  GimpPixelRgn	src_rgn, dest_rgn;
  gpointer	pr;
  guchar	*src_row, *dest_row;
  guchar	*src, *dest;
  gint		progress, max_progress;
  gint		x1, y1, x2, y2;
  gint		row, col;
  gint		bpp, color, has_alpha, alpha;
  guchar	*samples, *samp;
  gint		lum;				/* luminosity */
  gint		b;

  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  bpp = alpha = gimp_drawable_bpp( drawable->drawable_id );
  color = gimp_drawable_is_rgb( drawable->drawable_id );
  has_alpha = gimp_drawable_has_alpha( drawable->drawable_id );
  if( has_alpha )
    alpha--;

  samples = get_samples (drawable);

  gimp_pixel_rgn_init (&src_rgn, drawable, x1, y1, x2-x1, y2-y1, FALSE, FALSE);
  gimp_pixel_rgn_init (&dest_rgn, drawable, x1, y1, x2-x1, y2-y1, TRUE, TRUE);

  /* Initialize progress */
  progress = 0;
  max_progress = (x2 - x1) * (y2 - y1);

  for (pr = gimp_pixel_rgns_register (2, &src_rgn, &dest_rgn);
       pr != NULL; pr = gimp_pixel_rgns_process (pr))
    {
      src_row = src_rgn.data;
      dest_row = dest_rgn.data;

      for ( row = 0; row < src_rgn.h; row++ )
	{
	  src = src_row;
	  dest = dest_row;

	  for ( col = 0; col < src_rgn.w; col++ )
	    {
	      if( color )
		{
		  lum = LUMINOSITY (src);
		  lum = CLAMP (lum, 0, 255);	/* to make sure */
		}
	      else
		lum = src[0];

	      samp = &samples[lum * bpp];
	      for( b = 0; b < bpp; b++ )
		dest[b] = samp[b];

	      src += src_rgn.bpp;
	      dest += dest_rgn.bpp;
	    }
	  src_row += src_rgn.rowstride;
	  dest_row += dest_rgn.rowstride;
	}
      /* Update progress */
      progress += src_rgn.w * src_rgn.h;
      gimp_progress_update ((double) progress / (double) max_progress);
    }

  g_free (samples);

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id, x1, y1, (x2 - x1), (y2 - y1));
}

/*
  Returns 256 samples of active gradient.
  Each sample has (gimp_drawable_bpp (drawable->drawable_id)) bytes.
 */
static guchar *
get_samples (GimpDrawable *drawable)
{
  gdouble	*f_samples, *f_samp;	/* float samples */
  guchar	*b_samples, *b_samp;	/* byte samples */
  gint		bpp, color, has_alpha, alpha;
  gint		i, j;

  f_samples = gimp_gradients_sample_uniform (NSAMPLES);

  bpp	    = gimp_drawable_bpp (drawable->drawable_id);
  color	    = gimp_drawable_is_rgb (drawable->drawable_id);
  has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);
  alpha	    = (has_alpha ? bpp - 1 : bpp);

  b_samples = g_new (guchar, NSAMPLES * bpp);

  for (i = 0; i < NSAMPLES; i++)
    {
      b_samp = &b_samples[i * bpp];
      f_samp = &f_samples[i * 4];
      if (color)
	for (j = 0; j < 3; j++)
	  b_samp[j] = f_samp[j] * 255;
      else
	b_samp[0] = LUMINOSITY (f_samp) * 255;

      if (has_alpha)
	b_samp[alpha] = f_samp[3] * 255;
    }

  g_free (f_samples);
  return b_samples;
}
