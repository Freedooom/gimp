/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpmiscui.c
 * Contains all kinds of miscellaneous routines factored out from different
 * plug-ins. They stay here until their API has crystalized a bit and we can
 * put them into the file where they belong (Maurits Rijk 
 * <lpeek.mrijk@consunet.nl> if you want to blame someone for this mess)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef __GNUC__
#warning GTK_DISABLE_DEPRECATED
#endif
#undef GTK_DISABLE_DEPRECATED

#include <string.h>

#include <gtk/gtk.h>

#include "config.h"

#include "gimp.h"
#include "gimpintl.h"
#include "gimpmiscui.h"

#define PREVIEW_SIZE  128 

GimpFixMePreview*
gimp_fixme_preview_new (GimpDrawable *drawable, gboolean has_frame)
{
  GimpFixMePreview *preview = g_new0 (GimpFixMePreview, 1);

  preview->widget = gtk_preview_new (GTK_PREVIEW_COLOR);

  if (drawable)
    gimp_fixme_preview_fill_with_thumb (preview, drawable->drawable_id);

  if (has_frame)
    {
      GtkWidget *frame, *abox;

      preview->frame = gtk_frame_new (_("Preview"));
      gtk_widget_show (preview->frame);

      abox = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      gtk_container_set_border_width (GTK_CONTAINER (abox), 4);
      gtk_container_add (GTK_CONTAINER (preview->frame), abox);
      gtk_widget_show (abox);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_container_add (GTK_CONTAINER (abox), frame);
      gtk_widget_show (frame);

      gtk_container_add (GTK_CONTAINER (frame), preview->widget);
    }

  return preview;
}

void 
gimp_fixme_preview_free (GimpFixMePreview *preview)
{
  g_free (preview->cmap);
  g_free (preview->even);
  g_free (preview->odd);
  g_free (preview->cache);
  g_free (preview);
}

void
gimp_fixme_preview_do_row (GimpFixMePreview *preview,
			   gint    row,
			   gint    width,
			   guchar *src)
{
  gint    x;
  guchar *p0 = preview->even;
  guchar *p1 = preview->odd;
  gint bpp = preview->bpp;
  gdouble r, g, b, a;
  gdouble c0, c1;

  for (x = 0; x < width; x++) 
    {
      if (bpp == 4)
	{
	  r = ((gdouble) src[x*4 + 0]) / 255.0;
	  g = ((gdouble) src[x*4 + 1]) / 255.0;
	  b = ((gdouble) src[x*4 + 2]) / 255.0;
	  a = ((gdouble) src[x*4 + 3]) / 255.0;
	}
      else if (bpp == 3)
	{
	  r = ((gdouble) src[x*3 + 0]) / 255.0;
	  g = ((gdouble) src[x*3 + 1]) / 255.0;
	  b = ((gdouble) src[x*3 + 2]) / 255.0;
	  a = 1.0;
	}
      else
	{
	  if (preview->cmap)
	    {
	      gint index = MIN (src[x*bpp], preview->ncolors - 1);
	      
	      r = ((gdouble)preview->cmap[index * 3 + 0]) / 255.0;
	      g = ((gdouble)preview->cmap[index * 3 + 1]) / 255.0;
	      b = ((gdouble)preview->cmap[index * 3 + 2]) / 255.0;
	    }
	  else
	    {
	      r = ((gdouble)src[x*bpp + 0]) / 255.0;
	      g = b = r;
	    }
	  
	  if (bpp == 2)
	    a = ((gdouble)src[x*2 + 1]) / 255.0;
	  else
	    a = 1.0;
	}
      
      if ((x / GIMP_CHECK_SIZE_SM) & 1) 
	{
	  c0 = GIMP_CHECK_LIGHT;
	  c1 = GIMP_CHECK_DARK;
	} 
      else 
	{
	  c0 = GIMP_CHECK_DARK;
	  c1 = GIMP_CHECK_LIGHT;
	}
      
      *p0++ = (c0 + (r - c0) * a) * 255.0;
      *p0++ = (c0 + (g - c0) * a) * 255.0;
      *p0++ = (c0 + (b - c0) * a) * 255.0;
      
      *p1++ = (c1 + (r - c1) * a) * 255.0;
      *p1++ = (c1 + (g - c1) * a) * 255.0;
      *p1++ = (c1 + (b - c1) * a) * 255.0; 
    }

  if ((row / GIMP_CHECK_SIZE_SM) & 1)
    {
      gtk_preview_draw_row (GTK_PREVIEW (preview->widget), 
                            (guchar *) preview->odd,  0, row, width); 
    }
  else
    {
      gtk_preview_draw_row (GTK_PREVIEW (preview->widget),
                            (guchar *) preview->even, 0, row, width); 
    }
}

void 
gimp_fixme_preview_fill_with_thumb (GimpFixMePreview *preview,
				    gint32     drawable_ID)
{
  gint    bpp;
  gint    y;
  gint    width  = PREVIEW_SIZE;
  gint    height = PREVIEW_SIZE;
  guchar *src;

  preview->cache = 
    gimp_drawable_get_thumbnail_data (drawable_ID, &width, &height, &bpp);

  if (width < 1 || height < 1)
    return;

  preview->rowstride = width * bpp;
  preview->bpp       = bpp;

  if (gimp_drawable_is_indexed (drawable_ID))
    {
      gint32 image_ID = gimp_drawable_image (drawable_ID);
      preview->cmap = gimp_image_get_cmap (image_ID, &preview->ncolors);
    }
  else
    {
      preview->cmap = NULL;
    }

  gtk_preview_size (GTK_PREVIEW (preview->widget), width, height);

  preview->scale_x = 
    (gdouble) width / (gdouble) gimp_drawable_width (drawable_ID);
  preview->scale_y = 
    (gdouble) height / (gdouble) gimp_drawable_height (drawable_ID);

  src  = preview->cache;
  preview->even = g_malloc (width * 3);
  preview->odd  = g_malloc (width * 3);

  for (y = 0; y < height; y++)
    {
      gimp_fixme_preview_do_row (preview, y, width, src);
      src += width * bpp;
    }

  preview->buffer = GTK_PREVIEW (preview->widget)->buffer;
  preview->width = GTK_PREVIEW (preview->widget)->buffer_width;
  preview->height = GTK_PREVIEW (preview->widget)->buffer_height;
}

void 
gimp_fixme_preview_fill (GimpFixMePreview *preview, 
			 GimpDrawable *drawable)
{
  GimpPixelRgn  srcPR;
  gint       width;
  gint       height;
  gint       x1, x2, y1, y2;
  gint       bpp;
  gint       y;
  guchar    *src;
  
  gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);

  if (x2 - x1 > PREVIEW_SIZE)
    x2 = x1 + PREVIEW_SIZE;
  
  if (y2 - y1 > PREVIEW_SIZE)
    y2 = y1 + PREVIEW_SIZE;
  
  width  = x2 - x1;
  height = y2 - y1;
  bpp    = gimp_drawable_bpp (drawable->drawable_id);

  if (gimp_drawable_is_indexed (drawable->drawable_id))
    {
      gint32 image_ID = gimp_drawable_image (drawable->drawable_id);
      preview->cmap = gimp_image_get_cmap (image_ID, &preview->ncolors);
    }
  else
    {
      preview->cmap = NULL;
    }

  gtk_preview_size (GTK_PREVIEW (preview->widget), width, height);

  gimp_pixel_rgn_init (&srcPR, drawable, x1, y1, x2, y2, FALSE, FALSE);

  preview->even = g_malloc (width * 3);
  preview->odd  = g_malloc (width * 3);
  src  = g_malloc (width * bpp);
  preview->cache = g_malloc(width * bpp * height);
  preview->rowstride = width * bpp;
  preview->bpp = bpp;

  for (y = 0; y < height; y++)
    {
      gimp_pixel_rgn_get_row (&srcPR, src, x1, y + y1, width);
      memcpy(preview->cache + (y * width * bpp), src, width * bpp);
    }
  
  for (y = 0; y < height; y++)
    {
      gimp_fixme_preview_do_row(preview, y, width, 
				preview->cache + (y * width * bpp));
    }

  preview->buffer = GTK_PREVIEW (preview->widget)->buffer;
  preview->width = GTK_PREVIEW (preview->widget)->buffer_width;
  preview->height = GTK_PREVIEW (preview->widget)->buffer_height;

  g_free (src);
}

GList*
gimp_plug_in_parse_path (gchar *path_name, const gchar *dir_name)
{
  GList *path_list = NULL;
  GList *fail_list = NULL;
  GList *list;
  gchar *path;

  path = gimp_gimprc_query (path_name);

  if (!path)
    {
      gchar *gimprc = gimp_personal_rc_file ("gimprc");
      gchar *full_path;
      gchar *esc_path;

      full_path = g_strconcat 
	("${gimp_dir}", G_DIR_SEPARATOR_S, dir_name,
	 G_SEARCHPATH_SEPARATOR_S,
	 "${gimp_data_dir}", G_DIR_SEPARATOR_S, dir_name,
	 NULL);
      esc_path = g_strescape (full_path, NULL);

      g_message (_("No %s in gimprc:\n"
		   "You need to add an entry like\n"
		   "(%s \"%s\")\n"
		   "to your %s file."), path_name, path_name, esc_path, 
		 gimprc);

      g_free (gimprc);
      g_free (full_path);
      g_free (esc_path);
      return NULL;
    }

  path_list = gimp_path_parse (path, 16, TRUE, &fail_list);

  g_free (path);

  if (fail_list)
    {
      GString *err = g_string_new (path_name);
      g_string_append (err, _(" misconfigured - "
			      "the following folders were not found:"));

      for (list = fail_list; list; list = g_list_next (list))
        {
          g_string_append_c (err, '\n');
          g_string_append (err, (gchar *) list->data);
        }

      g_message (err->str);

      g_string_free (err, TRUE);
      gimp_path_free (fail_list);
    }

  return path_list;
}

