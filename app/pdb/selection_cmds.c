/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2000 Spencer Kimball and Peter Mattis
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

/* NOTE: This file is autogenerated by pdbgen.pl. */

#include "config.h"


#include <gtk/gtk.h>

#include "apptypes.h"
#include "procedural_db.h"

#include "channel.h"
#include "drawable.h"
#include "gimage_mask.h"
#include "gimpimage.h"
#include "layer.h"

static ProcRecord selection_bounds_proc;
static ProcRecord selection_value_proc;
static ProcRecord selection_is_empty_proc;
static ProcRecord selection_translate_proc;
static ProcRecord selection_float_proc;
static ProcRecord selection_clear_proc;
static ProcRecord selection_invert_proc;
static ProcRecord selection_sharpen_proc;
static ProcRecord selection_all_proc;
static ProcRecord selection_none_proc;
static ProcRecord selection_feather_proc;
static ProcRecord selection_border_proc;
static ProcRecord selection_grow_proc;
static ProcRecord selection_shrink_proc;
static ProcRecord selection_layer_alpha_proc;
static ProcRecord selection_load_proc;
static ProcRecord selection_save_proc;
static ProcRecord selection_combine_proc;

void
register_selection_procs (void)
{
  procedural_db_register (&selection_bounds_proc);
  procedural_db_register (&selection_value_proc);
  procedural_db_register (&selection_is_empty_proc);
  procedural_db_register (&selection_translate_proc);
  procedural_db_register (&selection_float_proc);
  procedural_db_register (&selection_clear_proc);
  procedural_db_register (&selection_invert_proc);
  procedural_db_register (&selection_sharpen_proc);
  procedural_db_register (&selection_all_proc);
  procedural_db_register (&selection_none_proc);
  procedural_db_register (&selection_feather_proc);
  procedural_db_register (&selection_border_proc);
  procedural_db_register (&selection_grow_proc);
  procedural_db_register (&selection_shrink_proc);
  procedural_db_register (&selection_layer_alpha_proc);
  procedural_db_register (&selection_load_proc);
  procedural_db_register (&selection_save_proc);
  procedural_db_register (&selection_combine_proc);
}

static Argument *
selection_bounds_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;
  gboolean non_empty = FALSE;
  gint32 x1;
  gint32 y1;
  gint32 x2;
  gint32 y2;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    non_empty = gimage_mask_bounds (gimage, &x1, &y1, &x2, &y2);

  return_args = procedural_db_return_args (&selection_bounds_proc, success);

  if (success)
    {
      return_args[1].value.pdb_int = non_empty;
      return_args[2].value.pdb_int = x1;
      return_args[3].value.pdb_int = y1;
      return_args[4].value.pdb_int = x2;
      return_args[5].value.pdb_int = y2;
    }

  return return_args;
}

static ProcArg selection_bounds_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcArg selection_bounds_outargs[] =
{
  {
    PDB_INT32,
    "non_empty",
    "True if there is a selection"
  },
  {
    PDB_INT32,
    "x1",
    "x coordinate of upper left corner of selection bounds"
  },
  {
    PDB_INT32,
    "y1",
    "y coordinate of upper left corner of selection bounds"
  },
  {
    PDB_INT32,
    "x2",
    "x coordinate of lower right corner of selection bounds"
  },
  {
    PDB_INT32,
    "y2",
    "y coordinate of lower right corner of selection bounds"
  }
};

static ProcRecord selection_bounds_proc =
{
  "gimp_selection_bounds",
  "Find the bounding box of the current selection.",
  "This procedure returns whether there is a selection for the specified image. If there is one, the upper left and lower right corners of the bounding box are returned. These coordinates are relative to the image.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_bounds_inargs,
  5,
  selection_bounds_outargs,
  { { selection_bounds_invoker } }
};

static Argument *
selection_value_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;
  gint32 x;
  gint32 y;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  x = args[1].value.pdb_int;

  y = args[2].value.pdb_int;

  return_args = procedural_db_return_args (&selection_value_proc, success);

  if (success)
    return_args[1].value.pdb_int = gimage_mask_value (gimage, x, y);

  return return_args;
}

static ProcArg selection_value_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_INT32,
    "x",
    "x coordinate of value"
  },
  {
    PDB_INT32,
    "y",
    "y coordinate of value"
  }
};

static ProcArg selection_value_outargs[] =
{
  {
    PDB_INT32,
    "value",
    "Value of the selection: (0 <= value <= 255)"
  }
};

static ProcRecord selection_value_proc =
{
  "gimp_selection_value",
  "Find the value of the selection at the specified coordinates.",
  "This procedure returns the value of the selection at the specified coordinates. If the coordinates lie out of bounds, 0 is returned.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  3,
  selection_value_inargs,
  1,
  selection_value_outargs,
  { { selection_value_invoker } }
};

static Argument *
selection_is_empty_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  return_args = procedural_db_return_args (&selection_is_empty_proc, success);

  if (success)
    return_args[1].value.pdb_int = gimage_mask_is_empty (gimage);

  return return_args;
}

static ProcArg selection_is_empty_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcArg selection_is_empty_outargs[] =
{
  {
    PDB_INT32,
    "is_empty",
    "Is the selection empty?"
  }
};

static ProcRecord selection_is_empty_proc =
{
  "gimp_selection_is_empty",
  "Determine whether the selection is empty.",
  "This procedure returns non-zero if the selection for the specified image is not empty.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_is_empty_inargs,
  1,
  selection_is_empty_outargs,
  { { selection_is_empty_invoker } }
};

static Argument *
selection_translate_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;
  gint32 offx;
  gint32 offy;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  offx = args[1].value.pdb_int;

  offy = args[2].value.pdb_int;

  if (success)
    gimage_mask_translate (gimage, offx, offy);

  return procedural_db_return_args (&selection_translate_proc, success);
}

static ProcArg selection_translate_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_INT32,
    "offx",
    "x offset for translation"
  },
  {
    PDB_INT32,
    "offy",
    "y offset for translation"
  }
};

static ProcRecord selection_translate_proc =
{
  "gimp_selection_translate",
  "Translate the selection by the specified offsets.",
  "This procedure actually translates the selection for the specified image by the specified offsets. Regions that are translated from beyond the bounds of the image are set to empty. Valid regions of the selection which are translated beyond the bounds of the image because of this call are lost.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  3,
  selection_translate_inargs,
  0,
  NULL,
  { { selection_translate_invoker } }
};

static Argument *
selection_float_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpDrawable *drawable;
  gint32 offx;
  gint32 offy;
  GimpLayer *layer = NULL;
  GimpImage *gimage;

  drawable = gimp_drawable_get_by_ID (args[0].value.pdb_int);
  if (drawable == NULL)
    success = FALSE;

  offx = args[1].value.pdb_int;

  offy = args[2].value.pdb_int;

  if (success)
    {
      gimage = gimp_drawable_gimage (drawable);
      layer = gimage_mask_float (gimage, drawable, offx, offy);
      success = layer != NULL;
    }

  return_args = procedural_db_return_args (&selection_float_proc, success);

  if (success)
    return_args[1].value.pdb_int = gimp_drawable_get_ID (GIMP_DRAWABLE (layer));

  return return_args;
}

static ProcArg selection_float_inargs[] =
{
  {
    PDB_DRAWABLE,
    "drawable",
    "The drawable from which to float selection"
  },
  {
    PDB_INT32,
    "offx",
    "x offset for translation"
  },
  {
    PDB_INT32,
    "offy",
    "y offset for translation"
  }
};

static ProcArg selection_float_outargs[] =
{
  {
    PDB_LAYER,
    "layer",
    "The floated layer"
  }
};

static ProcRecord selection_float_proc =
{
  "gimp_selection_float",
  "Float the selection from the specified drawable with initial offsets as specified.",
  "This procedure determines the region of the specified drawable that lies beneath the current selection. The region is then cut from the drawable and the resulting data is made into a new layer which is instantiated as a floating selection. The offsets allow initial positioning of the new floating selection.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  3,
  selection_float_inargs,
  1,
  selection_float_outargs,
  { { selection_float_invoker } }
};

static Argument *
selection_clear_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    gimage_mask_clear (gimage);

  return procedural_db_return_args (&selection_clear_proc, success);
}

static ProcArg selection_clear_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcRecord selection_clear_proc =
{
  "gimp_selection_clear",
  "Set the selection to none, clearing all previous content.",
  "This procedure sets the selection mask to empty, assigning the value 0 to every pixel in the selection channel.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_clear_inargs,
  0,
  NULL,
  { { selection_clear_invoker } }
};

static Argument *
selection_invert_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    gimage_mask_invert (gimage);

  return procedural_db_return_args (&selection_invert_proc, success);
}

static ProcArg selection_invert_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcRecord selection_invert_proc =
{
  "gimp_selection_invert",
  "Invert the selection mask.",
  "This procedure inverts the selection mask. For every pixel in the selection channel, its new value is calculated as (255 - old_value).",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_invert_inargs,
  0,
  NULL,
  { { selection_invert_invoker } }
};

static Argument *
selection_sharpen_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    gimage_mask_sharpen (gimage);

  return procedural_db_return_args (&selection_sharpen_proc, success);
}

static ProcArg selection_sharpen_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcRecord selection_sharpen_proc =
{
  "gimp_selection_sharpen",
  "Sharpen the selection mask.",
  "This procedure sharpens the selection mask. For every pixel in the selection channel, if the value is > 0, the new pixel is assigned a value of 255. This removes any \"anti-aliasing\" that might exist in the selection mask's boundary.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_sharpen_inargs,
  0,
  NULL,
  { { selection_sharpen_invoker } }
};

static Argument *
selection_all_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    gimage_mask_all (gimage);

  return procedural_db_return_args (&selection_all_proc, success);
}

static ProcArg selection_all_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcRecord selection_all_proc =
{
  "gimp_selection_all",
  "Select all of the image.",
  "This procedure sets the selection mask to completely encompass the image. Every pixel in the selection channel is set to 255.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_all_inargs,
  0,
  NULL,
  { { selection_all_invoker } }
};

static Argument *
selection_none_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    gimage_mask_none (gimage);

  return procedural_db_return_args (&selection_none_proc, success);
}

static ProcArg selection_none_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcRecord selection_none_proc =
{
  "gimp_selection_none",
  "Deselect the entire image.",
  "This procedure deselects the entire image. Every pixel in the selection channel is set to 0.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_none_inargs,
  0,
  NULL,
  { { selection_none_invoker } }
};

static Argument *
selection_feather_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;
  gdouble radius;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  radius = args[1].value.pdb_float;
  if (radius < 0.0)
    success = FALSE;

  if (success)
    gimage_mask_feather (gimage, radius, radius);

  return procedural_db_return_args (&selection_feather_proc, success);
}

static ProcArg selection_feather_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_FLOAT,
    "radius",
    "Radius of feather (in pixels)"
  }
};

static ProcRecord selection_feather_proc =
{
  "gimp_selection_feather",
  "Feather the image's selection",
  "This procedure feathers the selection. Feathering is implemented using a gaussian blur.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  2,
  selection_feather_inargs,
  0,
  NULL,
  { { selection_feather_invoker } }
};

static Argument *
selection_border_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;
  gint32 radius;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  radius = args[1].value.pdb_int;
  if (radius < 0)
    success = FALSE;

  if (success)
    gimage_mask_border (gimage, radius, radius);

  return procedural_db_return_args (&selection_border_proc, success);
}

static ProcArg selection_border_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_INT32,
    "radius",
    "Radius of border (in pixels)"
  }
};

static ProcRecord selection_border_proc =
{
  "gimp_selection_border",
  "Border the image's selection",
  "This procedure borders the selection. Bordering creates a new selection which is defined along the boundary of the previous selection at every point within the specified radius.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  2,
  selection_border_inargs,
  0,
  NULL,
  { { selection_border_invoker } }
};

static Argument *
selection_grow_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;
  gint32 steps;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  steps = args[1].value.pdb_int;
  if (steps < 0)
    success = FALSE;

  if (success)
    gimage_mask_grow (gimage, steps, steps);

  return procedural_db_return_args (&selection_grow_proc, success);
}

static ProcArg selection_grow_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_INT32,
    "steps",
    "Steps of grow (in pixels)"
  }
};

static ProcRecord selection_grow_proc =
{
  "gimp_selection_grow",
  "Grow the image's selection",
  "This procedure grows the selection. Growing involves expanding the boundary in all directions by the specified pixel amount.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  2,
  selection_grow_inargs,
  0,
  NULL,
  { { selection_grow_invoker } }
};

static Argument *
selection_shrink_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpImage *gimage;
  gint32 radius;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  radius = args[1].value.pdb_int;
  if (radius < 0)
    success = FALSE;

  if (success)
    gimage_mask_shrink (gimage, radius, radius, FALSE);

  return procedural_db_return_args (&selection_shrink_proc, success);
}

static ProcArg selection_shrink_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  },
  {
    PDB_INT32,
    "radius",
    "Radius of shrink (in pixels)"
  }
};

static ProcRecord selection_shrink_proc =
{
  "gimp_selection_shrink",
  "Shrink the image's selection",
  "This procedure shrinks the selection. Shrinking invovles trimming the existing selection boundary on all sides by the specified number of pixels.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  2,
  selection_shrink_inargs,
  0,
  NULL,
  { { selection_shrink_invoker } }
};

static Argument *
selection_layer_alpha_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpLayer *layer;
  GimpImage *gimage;

  layer = (GimpLayer *) gimp_drawable_get_by_ID (args[0].value.pdb_int);
  if (layer == NULL)
    success = FALSE;

  if (success)
    {
      gimage = gimp_drawable_gimage (GIMP_DRAWABLE (layer));
      gimage_mask_layer_alpha (gimage, layer);
    }

  return procedural_db_return_args (&selection_layer_alpha_proc, success);
}

static ProcArg selection_layer_alpha_inargs[] =
{
  {
    PDB_LAYER,
    "layer",
    "Layer with alpha"
  }
};

static ProcRecord selection_layer_alpha_proc =
{
  "gimp_selection_layer_alpha",
  "Transfer the specified layer's alpha channel to the selection mask.",
  "This procedure requires a layer with an alpha channel. The alpha channel information is used to create a selection mask such that for any pixel in the image defined in the specified layer, that layer pixel's alpha value is transferred to the selection mask. If the layer is undefined at a particular image pixel, the associated selection mask value is set to 0.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_layer_alpha_inargs,
  0,
  NULL,
  { { selection_layer_alpha_invoker } }
};

static Argument *
selection_load_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpChannel *channel;
  GimpImage *gimage;

  channel = (GimpChannel *) gimp_drawable_get_by_ID (args[0].value.pdb_int);
  if (channel == NULL)
    success = FALSE;

  if (success)
    {
      gimage = gimp_drawable_gimage (GIMP_DRAWABLE (channel));
       
      if (gimp_drawable_width  (GIMP_DRAWABLE (channel)) == gimage->width &&
	  gimp_drawable_height (GIMP_DRAWABLE (channel)) == gimage->height)
	gimage_mask_load (gimage, channel);
      else
	success = FALSE;
    }

  return procedural_db_return_args (&selection_load_proc, success);
}

static ProcArg selection_load_inargs[] =
{
  {
    PDB_CHANNEL,
    "channel",
    "The channel"
  }
};

static ProcRecord selection_load_proc =
{
  "gimp_selection_load",
  "Transfer the specified channel to the selection mask.",
  "This procedure loads the specified channel into the selection mask. This essentially involves a copy of the channel's content in to the selection mask. Therefore, the channel must have the same width and height of the image, or an error is returned.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_load_inargs,
  0,
  NULL,
  { { selection_load_invoker } }
};

static Argument *
selection_save_invoker (Argument *args)
{
  gboolean success = TRUE;
  Argument *return_args;
  GimpImage *gimage;
  GimpChannel *channel = NULL;

  gimage = pdb_id_to_image (args[0].value.pdb_int);
  if (gimage == NULL)
    success = FALSE;

  if (success)
    success = (channel = gimage_mask_save (gimage)) != NULL;

  return_args = procedural_db_return_args (&selection_save_proc, success);

  if (success)
    return_args[1].value.pdb_int = gimp_drawable_get_ID (GIMP_DRAWABLE (channel));

  return return_args;
}

static ProcArg selection_save_inargs[] =
{
  {
    PDB_IMAGE,
    "image",
    "The image"
  }
};

static ProcArg selection_save_outargs[] =
{
  {
    PDB_CHANNEL,
    "channel",
    "The new channel"
  }
};

static ProcRecord selection_save_proc =
{
  "gimp_selection_save",
  "Copy the selection mask to a new channel.",
  "This procedure copies the selection mask and stores the content in a new channel. The new channel is automatically inserted into the image's list of channels.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  1,
  selection_save_inargs,
  1,
  selection_save_outargs,
  { { selection_save_invoker } }
};

static Argument *
selection_combine_invoker (Argument *args)
{
  gboolean success = TRUE;
  GimpChannel *channel;
  gint32 operation;
  GimpImage *gimage;
  GimpChannel *new_channel;

  channel = (GimpChannel *) gimp_drawable_get_by_ID (args[0].value.pdb_int);
  if (channel == NULL)
    success = FALSE;

  operation = args[1].value.pdb_int;
  if (operation < CHANNEL_OP_ADD || operation > CHANNEL_OP_INTERSECT)
    success = FALSE;

  if (success)
    {
      gimage = gimp_drawable_gimage (GIMP_DRAWABLE (channel));
       
      if (gimp_drawable_width  (GIMP_DRAWABLE (channel)) == gimage->width &&
	  gimp_drawable_height (GIMP_DRAWABLE (channel)) == gimage->height)
	{
	  new_channel = gimp_channel_copy (gimp_image_get_mask (gimage));
	  gimp_channel_combine_mask (new_channel,
				     channel,
				     operation, 
				     0, 0);  /* off x/y */
	  gimage_mask_load (gimage, new_channel);
	  gtk_object_unref (GTK_OBJECT (new_channel));
	}
      else
	success = FALSE;
    }

  return procedural_db_return_args (&selection_combine_proc, success);
}

static ProcArg selection_combine_inargs[] =
{
  {
    PDB_CHANNEL,
    "channel",
    "The channel"
  },
  {
    PDB_INT32,
    "operation",
    "The selection operation: { ADD (0), SUB (1), REPLACE (2), INTERSECT (3) }"
  }
};

static ProcRecord selection_combine_proc =
{
  "gimp_selection_combine",
  "Combines the specified channel with the selection mask.",
  "This procedure combines the specified channel into the selection mask. It essentially involves a transfer of the channel's content into the selection mask. Therefore, the channel must have the same width and height of the image, or an error is returned.",
  "Spencer Kimball & Peter Mattis",
  "Spencer Kimball & Peter Mattis",
  "1995-1996",
  PDB_INTERNAL,
  2,
  selection_combine_inargs,
  0,
  NULL,
  { { selection_combine_invoker } }
};
