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

#include <glib-object.h>

#include "core-types.h"

#include "gimp.h"
#include "gimppaintinfo.h"


static void   gimp_paint_info_class_init (GimpPaintInfoClass *klass);
static void   gimp_paint_info_init       (GimpPaintInfo      *paint_info);

static void   gimp_paint_info_finalize   (GObject            *object);


static GimpObjectClass *parent_class = NULL;


GType
gimp_paint_info_get_type (void)
{
  static GType paint_info_type = 0;

  if (! paint_info_type)
    {
      static const GTypeInfo paint_info_info =
      {
        sizeof (GimpPaintInfoClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_paint_info_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpPaintInfo),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_paint_info_init,
      };

      paint_info_type = g_type_register_static (GIMP_TYPE_OBJECT,
					       "GimpPaintInfo",
					       &paint_info_info, 0);
    }

  return paint_info_type;
}

static void
gimp_paint_info_class_init (GimpPaintInfoClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gimp_paint_info_finalize;
}

static void
gimp_paint_info_init (GimpPaintInfo *paint_info)
{
  paint_info->gimp          = NULL;
  paint_info->paint_type    = G_TYPE_NONE;
  paint_info->pdb_string    = NULL;
  paint_info->paint_options = NULL;
}

static void
gimp_paint_info_finalize (GObject *object)
{
  GimpPaintInfo *paint_info;

  paint_info = GIMP_PAINT_INFO (object);

  if (paint_info->pdb_string)
    {
      g_free (paint_info->pdb_string);
      paint_info->pdb_string = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GimpPaintInfo *
gimp_paint_info_new (Gimp        *gimp,
                     GType        paint_type,
                     GType        paint_options_type,
                     const gchar *pdb_string)
{
  GimpPaintInfo *paint_info;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (pdb_string != NULL, NULL);

  paint_info = g_object_new (GIMP_TYPE_PAINT_INFO,
                             "name", g_type_name (paint_type),
                             NULL);

  paint_info->gimp               = gimp;
  paint_info->paint_type         = paint_type;
  paint_info->paint_options_type = paint_options_type;
  paint_info->pdb_string         = g_strdup (pdb_string);

  return paint_info;
}
