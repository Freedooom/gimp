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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"

#include "tools/tools-types.h"

#include "config/gimpcoreconfig.h"

#include "base/pixel-region.h"
#include "base/tile-manager.h"
#include "base/tile.h"

#include "paint-funcs/paint-funcs.h"

#include "core/gimp.h"
#include "core/gimp-parasites.h"
#include "core/gimpchannel.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-guides.h"
#include "core/gimpimage-mask.h"
#include "core/gimpimage-projection.h"
#include "core/gimplayer.h"
#include "core/gimplayer-floating-sel.h"
#include "core/gimplayermask.h"
#include "core/gimplist.h"
#include "core/gimpparasitelist.h"

#include "paint/gimppaintcore.h"

#include "tools/gimpbycolorselecttool.h"
#include "tools/gimppainttool.h"
#include "tools/gimptransformtool.h"
#include "tools/tool_manager.h"

#include "path_transform.h"
#include "undo.h"
#include "undo_types.h"

#include "libgimp/gimpintl.h"


/*#define DEBUG*/

#ifdef DEBUG
#define TRC(x) printf x
#else
#define TRC(x)
#endif


typedef enum
{
  UNDO = 0,
  REDO = 1
} UndoState;


typedef gboolean (* UndoPopFunc)  (GimpImage *gimage,
                                   UndoState  state,
                                   UndoType   type,
                                   gpointer   data);
typedef void     (* UndoFreeFunc) (UndoState  state,
                                   UndoType   type,
                                   gpointer   data);


typedef struct _Undo Undo;

struct _Undo
{
  UndoType      type;           /* undo type                                  */
  gpointer      data;           /* data to implement the undo, NULL for group */
  gsize         bytes;          /* size of undo item                          */
  gboolean      dirties_image;  /* TRUE if undo mutates image                 */
  gboolean      group_boundary; /* TRUE if this is the start/end of a group   */

  UndoPopFunc   pop_func;       /* function pointer to undo pop proc          */
  UndoFreeFunc  free_func;      /* function with specifics for freeing        */
};


static const gchar * undo_type_to_name  (UndoType     undo_type);

static Undo        * undo_new           (UndoType     undo_type, 
					 gsize        size, 
					 gboolean     dirties_image);


static gboolean mode_changed       = FALSE;
static gboolean size_changed       = FALSE;
static gboolean resolution_changed = FALSE;
static gboolean unit_changed       = FALSE;
static gboolean mask_changed       = FALSE;
static gboolean qmask_changed      = FALSE;
static gboolean alpha_changed      = FALSE;


static void
undo_free_list (GimpImage *gimage,
		UndoState  state,
		GSList    *list)
{
  GSList *orig;
  Undo   *undo;

  orig = list;

  while (list)
    {
      undo = (Undo *) list->data;
      if (undo)
	{
	  if (undo->free_func)
	    (*undo->free_func) (state, undo->type, undo->data);
	  gimage->undo_bytes -= undo->bytes;
	  g_free (undo);
	}
      list = g_slist_next (list);
    }

  g_slist_free (orig);
}


static GSList *
remove_stack_bottom (GimpImage *gimage)
{
  GSList   *list;
  GSList   *last;
  Undo     *object;
  gboolean  in_group = FALSE;

  list = gimage->undo_stack;

  last = NULL;
  while (list)
    {
      if (list->next == NULL)
	{
	  if (last)
	    undo_free_list (gimage, UNDO, last->next);
	  else
	    undo_free_list (gimage, UNDO, list);

	  gimage->undo_levels --;
	  if (last)
	    last->next = NULL;
	  list = NULL;

	  gimp_image_undo_event (gimage, UNDO_EXPIRED);
	}
      else
	{
	  object = (Undo *) list->data;
	  if (object->group_boundary)
	    in_group = (in_group) ? FALSE : TRUE;

	  /*  Make sure last points to only a single item, or the
	   *  tail of an aggregate undo item
	   */
	  if (!in_group)
	    last = list;

	  list = g_slist_next (list);
	}
    }

  if (last)
    return gimage->undo_stack;
  else
    return NULL;
}


/*  Allocate and initialise a new Undo.  Leaves data and function
 *  pointers zeroed ready to be filled in by caller.
 */
static Undo *
undo_new (UndoType type, 
	  gsize    size, 
	  gboolean dirties_image)
{
  Undo *new;

  new = g_new0 (Undo, 1);

  new->type          = type;
  new->bytes         = size;
  new->dirties_image = dirties_image;

  return new;
}


static gboolean
undo_free_up_space (GimpImage *gimage)
{
  /* If there are 0 levels of undo return FALSE.  */
  if (gimage->gimp->config->levels_of_undo == 0)
    return FALSE;

  /*  Delete the item on the bottom of the stack if we have the maximum
   *  levels of undo already
   */
  while (gimage->undo_levels >= gimage->gimp->config->levels_of_undo)
    gimage->undo_stack = remove_stack_bottom (gimage);

  return TRUE;
}


static Undo *
undo_push (GimpImage *gimage,
	   gsize      size,
	   UndoType   type,
	   gboolean   dirties_image)
{
  Undo *new;

  g_return_val_if_fail (type > LAST_UNDO_GROUP, NULL);

  /* Does this undo dirty the image?  If so, we always want to mark
   * image dirty, even if we can't actually push the undo.
   */
  if (dirties_image)
    gimp_image_dirty (gimage);

  if (! gimage->undo_on)
    return NULL;

  size += sizeof (Undo);

  if (gimage->redo_stack)
    {
      undo_free_list (gimage, REDO, gimage->redo_stack);
      gimage->redo_stack = NULL;

      /* If the image was dirty, but could become clean by redo-ing
       * some actions, then it should now become 'infinitely' dirty.
       * This is because we've just nuked the actions that would allow
       * the image to become clean again.  The only hope for salvation
       * is to save the image now!  -- austin */
      if (gimage->dirty < 0)
	gimage->dirty = 10000;
    }

  if (gimage->pushing_undo_group == NO_UNDO_GROUP)
    if (! undo_free_up_space (gimage))
      return NULL;

  new = undo_new (type, size, dirties_image);

  gimage->undo_bytes += size;

  /*  only increment levels if not in a group  */
  if (gimage->pushing_undo_group == NO_UNDO_GROUP)
      gimage->undo_levels ++;

  gimage->undo_stack = g_slist_prepend (gimage->undo_stack, (gpointer) new);
  TRC (("undo_push: %s\n", undo_type_to_name (type)));

  /* lastly, tell people about the newly pushed undo (must come after
   * modification of undo_stack).  */
  if (gimage->pushing_undo_group == NO_UNDO_GROUP)
    gimp_image_undo_event (gimage, UNDO_PUSHED);

  return new;
}


static gboolean
pop_stack (GimpImage  *gimage,
	   GSList    **stack_ptr,
	   GSList    **unstack_ptr,
	   UndoState   state)
{
  Undo     *object;
  GSList   *stack;
  GSList   *tmp;
  gboolean  status   = FALSE;
  gboolean  in_group = FALSE;

  /*  Keep popping until we pop a valid object
   *  or get to the end of a group if we're in one
   */
  while (*stack_ptr)
    {
      stack = *stack_ptr;

      object = (Undo *) stack->data;

      if (object->group_boundary)
	{
	  in_group = ! in_group;

	  if (in_group)
	    gimage->undo_levels += (state == UNDO) ? -1 : 1;

	  if (status && ! in_group)
	    status = TRUE;
	  else
	    status = FALSE;
	}
      else
	{
	  TRC (("undo_pop: %s\n", undo_type_to_name (object->type)));

	  status = (* object->pop_func) (gimage,
                                         state,
                                         object->type,
					 object->data);

	  if (object->dirties_image)
	    {
	      switch (state)
		{
		case UNDO:
		  gimp_image_clean (gimage);
		  break;
		case REDO:
		  gimp_image_dirty (gimage);
		  break;
		}
	    }

	  if (! in_group)
	    gimage->undo_levels += (state == UNDO) ? -1 : 1;
	}

      *unstack_ptr = g_slist_prepend (*unstack_ptr, object);

      tmp = stack;
      *stack_ptr = g_slist_next (*stack_ptr);
      tmp->next = NULL;
      g_slist_free (tmp);

      if (status && ! in_group)
	{
	  /*  If the mode_changed flag was set  */
	  if (mode_changed)
	    {
	      gimp_image_mode_changed (gimage);

	      mode_changed = FALSE;
	    }

	  /*  If the size_changed flag was set  */
	  if (size_changed)
	    {
	      gimp_viewable_size_changed (GIMP_VIEWABLE (gimage));

	      size_changed = FALSE;
	    }

	  /*  If the resolution_changed flag was set  */
	  if (resolution_changed)
	    {
	      gimp_image_resolution_changed (gimage);

	      resolution_changed = FALSE;
	    }

	  /*  If the unit_changed flag was set  */
	  if (unit_changed)
	    {
	      gimp_image_unit_changed (gimage);

	      unit_changed = FALSE;
	    }

	  /*  If the mask_changed flag was set  */
	  if (mask_changed)
	    {
	      gimp_image_mask_changed (gimage);

	      mask_changed = FALSE;
	    }

	  /*  If the qmask_changed flag was set  */
	  if (qmask_changed)
	    {
	      gimp_image_qmask_changed (gimage);

	      qmask_changed = FALSE;
	    }

	  /*  If the alpha_changed flag was set  */
	  if (alpha_changed)
	    {
	      gimp_image_alpha_changed (gimage);

	      alpha_changed = FALSE;
	    }

	  /* let others know that we just popped an action */
	  gimp_image_undo_event (gimage,
				 (state == UNDO)? UNDO_POPPED : UNDO_REDO);

	  return TRUE;
	}
    }

  return FALSE;
}


gboolean
undo_pop (GimpImage *gimage)
{
  /*  Very bad idea to pop an action off the undo stack if we're in the
   *  middle of a group, since the whole group won't be popped.  Might
   *  leave unbalanced group start marker earlier in the stack too,
   *  causing much confusion when it's later reached and
   *  mis-interpreted as a group start.
   */
  g_return_val_if_fail (gimage->pushing_undo_group == NO_UNDO_GROUP, FALSE);

  return pop_stack (gimage, &gimage->undo_stack, &gimage->redo_stack, UNDO);
}


gboolean
undo_redo (GimpImage *gimage)
{
  /* ditto for redo stack */
  g_return_val_if_fail (gimage->pushing_undo_group == NO_UNDO_GROUP, FALSE);

  return pop_stack (gimage, &gimage->redo_stack, &gimage->undo_stack, REDO);
}



/* Return the name of the action that would be used if undo_pop or
 * undo_redo is called, or NULL if there are no actions pushed on the
 * stack.
 */
static const gchar *
undo_get_topitem_name (GSList *stack)
{
  Undo *object;

  if (!stack)
    return NULL;

  object = stack->data;

  /* For group boundaries, the type of the boundary marker is the
   * type of the whole group (but each individual action in the
   * group retains its own type so layer/channel unrefs work
   * correctly).
   */

  return undo_type_to_name (object->type);
}

/* Return the type of the top undo action */
static UndoType
undo_get_topitem_type (GSList *stack)
{
  Undo *object;

  if (!stack)
    return NO_UNDO_GROUP;

  object = stack->data;

  /* For group boundaries, the type of the boundary marker is the
   * type of the whole group (but each individual action in the
   * group retains its own type so layer/channel unrefs work
   * correctly).
   */

  return object->type;
}


const gchar *
undo_get_undo_name (GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NULL);

  /* don't want to encourage undo while a group is open */
  if (gimage->pushing_undo_group != NO_UNDO_GROUP)
    return NULL;

  return undo_get_topitem_name (gimage->undo_stack);
}

UndoType
undo_get_undo_top_type (GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NO_UNDO_GROUP);

  /* don't want to encourage undo while a group is open */
  if (gimage->pushing_undo_group != NO_UNDO_GROUP)
    return NO_UNDO_GROUP;

  return undo_get_topitem_type (gimage->undo_stack);
}


const gchar *
undo_get_redo_name (GimpImage *gimage)
{
  g_return_val_if_fail (gimage != NULL, NULL);

  /* don't want to encourage redo while a group is open */
  if (gimage->pushing_undo_group != NO_UNDO_GROUP)
    return NULL;

  return undo_get_topitem_name (gimage->redo_stack);
}


static void
undo_map_over_stack (GSList      *stack, 
		     undo_map_fn  fn, 
		     gpointer     data)
{
  gboolean  in_group = FALSE;
  gint      count = 0;
  Undo     *object;

  while (stack)
    {
      object = (Undo *) stack->data;
      if (object->group_boundary)
	in_group = !in_group;

      /* Keep track of group length.  0 means not in group (or group
       * end marker), 1 is group start marker, >= 2 are group
       * members.
       */
      if (in_group)
	count++;
      else
	count = 0;

      /* Is this a group end marker or non-grouped action? */
      if (count == 0)
	{
	  if (fn (undo_type_to_name (object->type), data))
	    return; /* early termination option exercised */
	}

      stack = g_slist_next (stack);
    }
}

void
undo_map_over_undo_stack (GimpImage   *gimage, 
			  undo_map_fn  fn, 
			  gpointer     data)
{
  /* shouldn't have group open */
  g_return_if_fail (gimage->pushing_undo_group == NO_UNDO_GROUP);

  undo_map_over_stack (gimage->undo_stack, fn, data);
}

void
undo_map_over_redo_stack (GimpImage   *gimage, 
			  undo_map_fn  fn, 
			  gpointer     data)
{
  /* shouldn't have group open */
  g_return_if_fail (gimage->pushing_undo_group == NO_UNDO_GROUP);

  undo_map_over_stack (gimage->redo_stack, fn, data);
}


void
undo_free (GimpImage *gimage)
{
  undo_free_list (gimage, UNDO, gimage->undo_stack);
  undo_free_list (gimage, REDO, gimage->redo_stack);

  gimage->undo_stack  = NULL;
  gimage->redo_stack  = NULL;
  gimage->undo_bytes  = 0;
  gimage->undo_levels = 0;

  /* If the image was dirty, but could become clean by redo-ing
   * some actions, then it should now become 'infinitely' dirty.
   * This is because we've just nuked the actions that would allow
   * the image to become clean again.  The only hope for salvation
   * is to save the image now!  -- austin
   */
  if (gimage->dirty < 0)
    gimage->dirty = 10000;

  /* The same applies to the case where the image would become clean
   * due to undo actions, but since user can't undo without an undo
   * stack, that's not so much a problem.
   */
  gimp_image_undo_event (gimage, UNDO_FREE);
}


/**********************/
/*  group Undo funcs  */

gboolean
undo_push_group_start (GimpImage *gimage,
		       UndoType   type)
{
  Undo *boundary_marker;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  /* Bad idea to push NO_UNDO_GROUP as the group type, since that
   * won't be recognized as the start of the group later.
   */
  g_return_val_if_fail (type >  FIRST_UNDO_GROUP &&
                        type <= LAST_UNDO_GROUP, FALSE);

  /* Notify listeners that the image will be modified */
  gimp_image_undo_start (gimage);

  if (! gimage->undo_on)
    return FALSE;

  gimage->group_count ++;
  TRC (("group_start (%s)\n", undo_type_to_name (type)));

  /*  If we're already in a group...ignore  */
  if (gimage->group_count > 1)
    return TRUE;

  if (gimage->redo_stack)
    {
      undo_free_list (gimage, REDO, gimage->redo_stack);
      gimage->redo_stack = NULL;
      /* If the image was dirty, but could become clean by redo-ing
       * some actions, then it should now become 'infinitely' dirty.
       * This is because we've just nuked the actions that would allow
       * the image to become clean again.  The only hope for salvation
       * is to save the image now!  -- austin
       */
      if (gimage->dirty < 0)
	gimage->dirty = 10000;
    }

  if (! undo_free_up_space (gimage))
    return FALSE;

  boundary_marker = undo_new (type, sizeof (Undo), FALSE);
  gimage->undo_bytes += boundary_marker->bytes;
  boundary_marker->group_boundary = TRUE;

  gimage->pushing_undo_group = type;
  gimage->undo_stack = g_slist_prepend (gimage->undo_stack, boundary_marker);
  gimage->undo_levels++;

  return TRUE;
}

gboolean
undo_push_group_end (GimpImage *gimage)
{
  Undo *boundary_marker;

  g_return_val_if_fail (GIMP_IS_IMAGE (gimage), FALSE);

  if (! gimage->undo_on)
    return FALSE;

  gimage->group_count --;
  TRC (("group end\n"));

  if (gimage->group_count == 0)
    {
      boundary_marker = undo_new (gimage->pushing_undo_group,
				  sizeof (Undo), FALSE);
      boundary_marker->group_boundary = TRUE;
      gimage->undo_stack = g_slist_prepend (gimage->undo_stack,
					    boundary_marker);
      gimage->undo_bytes += boundary_marker->bytes;

      gimage->pushing_undo_group = NO_UNDO_GROUP;

      /* Do it here, since undo_push doesn't emit this event while in the
       * middle of a group */
      gimp_image_undo_event (gimage, UNDO_PUSHED);
    }

  return TRUE;
}


/****************/
/*  Image Undo  */

typedef struct _ImageUndo ImageUndo;

struct _ImageUndo
{
  TileManager  *tiles;
  GimpDrawable *drawable;
  gint          x1, y1, x2, y2;
  gboolean      sparse;
};

static gboolean undo_pop_image  (GimpImage *,
                                 UndoState, UndoType, gpointer);
static void     undo_free_image (UndoState, UndoType, gpointer);

gboolean
undo_push_image (GimpImage    *gimage,
		 GimpDrawable *drawable,
		 gint          x1,
		 gint          y1,
		 gint          x2,
		 gint          y2)
{
  Undo  *new;
  gsize  size;

  x1 = CLAMP (x1, 0, gimp_drawable_width (drawable));
  y1 = CLAMP (y1, 0, gimp_drawable_height (drawable));
  x2 = CLAMP (x2, 0, gimp_drawable_width (drawable));
  y2 = CLAMP (y2, 0, gimp_drawable_height (drawable));

  size = sizeof (ImageUndo) + ((x2 - x1) * (y2 - y1) *
                               gimp_drawable_bytes (drawable));

  if ((new = undo_push (gimage, size, IMAGE_UNDO, TRUE)))
    {
      ImageUndo   *image_undo;
      TileManager *tiles;
      PixelRegion  srcPR, destPR;

      image_undo = g_new0 (ImageUndo, 1);

      new->data      = image_undo;
      new->pop_func  = undo_pop_image;
      new->free_func = undo_free_image;

      /*  If we cannot create a new temp buf--either because our parameters are
       *  degenerate or something else failed, simply return an unsuccessful push.
       */
      tiles = tile_manager_new ((x2 - x1), (y2 - y1),
				gimp_drawable_bytes (drawable));
      pixel_region_init (&srcPR, gimp_drawable_data (drawable),
			 x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&destPR, tiles,
			 0, 0, (x2 - x1), (y2 - y1), TRUE);
      copy_region (&srcPR, &destPR);

      /*  set the image undo structure  */
      image_undo->tiles    = tiles;
      image_undo->drawable = drawable;
      image_undo->x1       = x1;
      image_undo->y1       = y1;
      image_undo->x2       = x2;
      image_undo->y2       = y2;
      image_undo->sparse   = FALSE;

      return TRUE;
    }

  return FALSE;
}

gboolean
undo_push_image_mod (GimpImage    *gimage,
		     GimpDrawable *drawable,
		     gint          x1,
		     gint          y1,
		     gint          x2,
		     gint          y2,
		     TileManager  *tiles,
		     gboolean      sparse)
{
  Undo  *new;
  gsize  size;
  gint   dwidth, dheight;

  if (! tiles)
    return FALSE;

  dwidth  = gimp_drawable_width (drawable);
  dheight = gimp_drawable_height (drawable);

  x1 = CLAMP (x1, 0, dwidth);
  y1 = CLAMP (y1, 0, dheight);
  x2 = CLAMP (x2, 0, dwidth);
  y2 = CLAMP (y2, 0, dheight);

  size = sizeof (ImageUndo) + tile_manager_get_memsize (tiles);

  if ((new = undo_push (gimage, size, IMAGE_MOD_UNDO, TRUE)))
    {
      ImageUndo *image_undo;

      image_undo = g_new0 (ImageUndo, 1);

      new->data      = image_undo;
      new->pop_func  = undo_pop_image;
      new->free_func = undo_free_image;

      image_undo->tiles    = tiles;
      image_undo->drawable = drawable;
      image_undo->x1       = x1;
      image_undo->y1       = y1;
      image_undo->x2       = x2;
      image_undo->y2       = y2;
      image_undo->sparse   = sparse;

      return TRUE;
    }

  tile_manager_destroy (tiles);

  return FALSE;
}

static gboolean
undo_pop_image (GimpImage *gimage,
		UndoState  state,
		UndoType   type,
		gpointer   image_undo_ptr)
{
  ImageUndo   *image_undo;
  TileManager *tiles;
  PixelRegion  PR1, PR2;
  gint x, y;
  gint w, h;

  image_undo = (ImageUndo *) image_undo_ptr;
  tiles = image_undo->tiles;

  /*  some useful values  */
  x = image_undo->x1;
  y = image_undo->y1;

  if (image_undo->sparse == FALSE)
    {
      w = tile_manager_width (tiles);
      h = tile_manager_height (tiles);

      pixel_region_init (&PR1, tiles,
			 0, 0, w, h, TRUE);
      pixel_region_init (&PR2, gimp_drawable_data (image_undo->drawable),
			 x, y, w, h, TRUE);

      /*  swap the regions  */
      swap_region (&PR1, &PR2);
    }
  else
    {
      int i, j;
      Tile *src_tile;
      Tile *dest_tile;

      w = image_undo->x2 - image_undo->x1;
      h = image_undo->y2 - image_undo->y1;

      for (i = y; i < image_undo->y2; i += (TILE_HEIGHT - (i % TILE_HEIGHT)))
	{
	  for (j = x; j < image_undo->x2; j += (TILE_WIDTH - (j % TILE_WIDTH)))
	    {
	      src_tile = tile_manager_get_tile (tiles, j, i, FALSE, FALSE);
	      if (tile_is_valid (src_tile) == TRUE)
		{
		  /* swap tiles, not pixels! */

		  src_tile = tile_manager_get_tile (tiles, j, i, TRUE, FALSE /* TRUE */);
		  dest_tile = tile_manager_get_tile (gimp_drawable_data (image_undo->drawable), j, i, TRUE, FALSE /* TRUE */);

		  tile_manager_map_tile (tiles, j, i, dest_tile);
		  tile_manager_map_tile (gimp_drawable_data (image_undo->drawable), j, i, src_tile);
#if 0
		  swap_pixels (tile_data_pointer (src_tile, 0, 0),
			       tile_data_pointer (dest_tile, 0, 0),
			       tile_size (src_tile));
#endif
		  
		  tile_release (dest_tile, FALSE /* TRUE */);
		  tile_release (src_tile, FALSE /* TRUE */);
		}
	    }
	}
    }

  gimp_drawable_update (image_undo->drawable, x, y, w, h);

  return TRUE;
}

static void
undo_free_image (UndoState  state,
		 UndoType   type,
		 gpointer   image_undo_ptr)
{
  ImageUndo *image_undo;

  image_undo = (ImageUndo *) image_undo_ptr;

  tile_manager_destroy (image_undo->tiles);
  g_free (image_undo);
}


/*********************/
/*  Image Type Undo  */

typedef struct _ImageTypeUndo ImageTypeUndo;

struct _ImageTypeUndo
{
  GimpImageBaseType base_type;
};

static gboolean undo_pop_image_type  (GimpImage *,
                                      UndoState, UndoType, gpointer);
static void     undo_free_image_type (UndoState, UndoType, gpointer);

gboolean
undo_push_image_type (GimpImage *gimage)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ImageTypeUndo),
                        IMAGE_TYPE_UNDO, TRUE)))
    {
      ImageTypeUndo *itu;

      itu = g_new0 (ImageTypeUndo, 1);

      new->data      = itu;
      new->pop_func  = undo_pop_image_type;
      new->free_func = undo_free_image_type;

      itu->base_type = gimage->base_type;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_type (GimpImage *gimage,
		     UndoState  state,
		     UndoType   type,
		     gpointer   data_ptr)
{
  ImageTypeUndo     *itu;
  GimpImageBaseType  tmp;

  itu = (ImageTypeUndo *) data_ptr;

  tmp = itu->base_type;
  itu->base_type = gimage->base_type;
  gimage->base_type = tmp;

  gimp_image_projection_allocate (gimage);

  gimp_image_colormap_changed (gimage, -1);

  if (itu->base_type != gimage->base_type)
    mode_changed = TRUE;

  return TRUE;
}

static void
undo_free_image_type (UndoState  state,
		      UndoType   type,
		      gpointer   data_ptr)
{
  g_free (data_ptr);
}


/*********************/
/*  Image Size Undo  */

typedef struct _ImageSizeUndo ImageSizeUndo;

struct _ImageSizeUndo
{
  gint width;
  gint height;
};

static gboolean undo_pop_image_size  (GimpImage *,
                                      UndoState, UndoType, gpointer);
static void     undo_free_image_size (UndoState, UndoType, gpointer);

gboolean
undo_push_image_size (GimpImage *gimage)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ImageSizeUndo),
                        IMAGE_SIZE_UNDO, TRUE)))
    {
      ImageSizeUndo *isu;

      isu = g_new0 (ImageSizeUndo, 1);

      new->data      = isu;
      new->pop_func  = undo_pop_image_size;
      new->free_func = undo_free_image_size;

      isu->width  = gimage->width;
      isu->height = gimage->height;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_size (GimpImage *gimage,
		     UndoState  state,
		     UndoType   type,
		     gpointer   data_ptr)
{
  ImageSizeUndo *isu;
  gint           tmp;

  isu = (ImageSizeUndo *) data_ptr;

  tmp = isu->width;
  isu->width = gimage->width;
  gimage->width = tmp;

  tmp = isu->height;
  isu->height = gimage->height;
  gimage->height = tmp;

  gimp_image_projection_allocate (gimage);

  gimp_image_mask_invalidate (gimage);

  if (gimage->width  != isu->width ||
      gimage->height != isu->height)
    size_changed = TRUE;

  return TRUE;
}

static void
undo_free_image_size (UndoState  state,
		      UndoType   type,
		      gpointer   data_ptr)
{
  g_free (data_ptr);
}


/***************************/
/*  Image Resolution Undo  */

typedef struct _ResolutionUndo ResolutionUndo;

struct _ResolutionUndo
{
  gdouble  xres;
  gdouble  yres;
  GimpUnit unit;
};

static gboolean undo_pop_image_resolution  (GimpImage *,
                                            UndoState, UndoType, gpointer);
static void     undo_free_image_resolution (UndoState, UndoType, gpointer);

gboolean
undo_push_image_resolution (GimpImage *gimage)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ResolutionUndo),
                        IMAGE_RESOLUTION_UNDO, TRUE)))
    {
      ResolutionUndo *ru;

      ru = g_new (ResolutionUndo, 1);

      new->data      = ru;
      new->pop_func  = undo_pop_image_resolution;
      new->free_func = undo_free_image_resolution;

      ru->xres = gimage->xresolution;
      ru->yres = gimage->yresolution;
      ru->unit = gimage->unit;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_resolution (GimpImage *gimage,
                           UndoState  state,
                           UndoType   type,
                           gpointer   data_ptr)
{
  ResolutionUndo *ru;

  ru = data_ptr;

  if (ABS (ru->xres - gimage->xresolution) >= 1e-5 ||
      ABS (ru->yres - gimage->yresolution) >= 1e-5)
    {
      gdouble old_xres;
      gdouble old_yres;

      old_xres = gimage->xresolution;
      old_yres = gimage->yresolution;

      gimage->xresolution = ru->xres;
      gimage->yresolution = ru->yres;

      ru->xres = old_xres;
      ru->yres = old_yres;

      resolution_changed = TRUE;
    }

  if (ru->unit != gimage->unit)
    {
      GimpUnit old_unit;

      old_unit = gimage->unit;
      gimage->unit = ru->unit;
      ru->unit = old_unit;

      unit_changed = TRUE;
    }

  return TRUE;
}

static void
undo_free_image_resolution (UndoState  state,
                            UndoType   type,
                            gpointer   data_ptr)
{
  g_free (data_ptr);
}


/****************/
/*  Qmask Undo  */

typedef struct _QmaskUndo QmaskUndo;

struct _QmaskUndo
{
  GimpImage *gimage;
  gboolean   qmask;
};

static gboolean undo_pop_image_qmask  (GimpImage *,
                                       UndoState, UndoType, gpointer);
static void     undo_free_image_qmask (UndoState, UndoType, gpointer);

gboolean
undo_push_image_qmask (GimpImage *gimage)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (QmaskUndo),
                        IMAGE_QMASK_UNDO, TRUE)))
    {
      QmaskUndo *qu;

      qu = g_new (QmaskUndo, 1);

      new->data      = qu;
      new->pop_func  = undo_pop_image_qmask;
      new->free_func = undo_free_image_qmask;

      qu->gimage = gimage;
      qu->qmask  = gimage->qmask_state;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_qmask (GimpImage *gimage,
                      UndoState  state,
                      UndoType   type,
                      gpointer   data_ptr)
{
  QmaskUndo *data;
  gboolean   tmp;

  data = (QmaskUndo *) data_ptr;
  
  tmp                 = gimage->qmask_state;
  gimage->qmask_state = data->qmask;
  data->qmask         = tmp;

  qmask_changed = TRUE;

  return TRUE;
}

static void
undo_free_image_qmask (UndoState  state,
                       UndoType   type,
                       gpointer   data_ptr)
{
  g_free (data_ptr);
}


/****************/
/*  Guide Undo  */

typedef struct _GuideUndo GuideUndo;

struct _GuideUndo
{
  GimpImage *gimage;
  GimpGuide *guide;
  GimpGuide  orig;
};

static gboolean undo_pop_image_guide  (GimpImage *,
                                       UndoState, UndoType, gpointer);
static void     undo_free_image_guide (UndoState, UndoType, gpointer);

gboolean
undo_push_image_guide (GimpImage *gimage,
                       GimpGuide *guide)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (GuideUndo),
                        IMAGE_GUIDE_UNDO, TRUE)))
    {
      GuideUndo *gu;

      gu = g_new (GuideUndo, 1);

      new->data      = gu;
      new->pop_func  = undo_pop_image_guide;
      new->free_func = undo_free_image_guide;

      guide->ref_count++;

      gu->gimage = gimage;
      gu->guide  = guide;
      gu->orig   = *guide;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_image_guide (GimpImage *gimage,
                      UndoState  state,
                      UndoType   type,
                      gpointer   data_ptr)
{
  GuideUndo *data;
  GimpGuide  tmp;
  gint       tmp_ref;

  data = data_ptr;

  gimp_image_update_guide (gimage, data->guide);

  tmp_ref = data->guide->ref_count;
  tmp = *(data->guide);
  *(data->guide) = data->orig;
  data->guide->ref_count = tmp_ref;
  data->orig = tmp;

  gimp_image_update_guide (gimage, data->guide);

  return TRUE;
}

static void
undo_free_image_guide (UndoState state,
                       UndoType  type,
                       gpointer  data_ptr)
{
  GuideUndo *data;

  data = (GuideUndo *) data_ptr;

  data->guide->ref_count--;
  if (data->guide->position < 0 && data->guide->ref_count <= 0)
    {
      gimp_image_remove_guide (data->gimage, data->guide);
      g_free (data->guide);
    }
  g_free (data);
}


/***************/
/*  Mask Undo  */

typedef struct _MaskUndo MaskUndo;

struct _MaskUndo
{
  GimpChannel *channel;  /*  the channel this undo is for  */
  TileManager *tiles;    /*  the actual mask               */
  gint         x, y;     /*  offsets                       */
};

static gboolean undo_pop_mask  (GimpImage *,
                                UndoState, UndoType, gpointer);
static void     undo_free_mask (UndoState, UndoType, gpointer);

gboolean
undo_push_mask (GimpImage   *gimage,
                GimpChannel *mask)
{
  TileManager *undo_tiles;
  gint         x1, y1, x2, y2;
  Undo        *new;
  gsize        size;

  if (gimp_channel_bounds (mask, &x1, &y1, &x2, &y2))
    {
      PixelRegion srcPR, destPR;

      undo_tiles = tile_manager_new ((x2 - x1), (y2 - y1), 1);

      pixel_region_init (&srcPR, GIMP_DRAWABLE (mask)->tiles,
                         x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&destPR, undo_tiles,
                         0, 0, (x2 - x1), (y2 - y1), TRUE);

      copy_region (&srcPR, &destPR);
    }
  else
    undo_tiles = NULL;

  size = sizeof (MaskUndo);

  if (undo_tiles)
    size += tile_manager_get_memsize (undo_tiles);

  if ((new = undo_push (gimage, size, MASK_UNDO, FALSE)))
    {
      MaskUndo *mask_undo;

      mask_undo = g_new0 (MaskUndo, 1);

      new->data      = mask_undo;
      new->pop_func  = undo_pop_mask;
      new->free_func = undo_free_mask;

      mask_undo->channel = mask;
      mask_undo->tiles   = undo_tiles;
      mask_undo->x       = x1;
      mask_undo->y       = y1;

      return TRUE;
    }

  if (undo_tiles)
    tile_manager_destroy (undo_tiles);

  return FALSE;
}

static gboolean
undo_pop_mask (GimpImage *gimage,
               UndoState  state,
               UndoType   type,
               gpointer   mask_ptr)
{
  MaskUndo    *mask_undo;
  TileManager *new_tiles;
  PixelRegion  srcPR, destPR;
  gint         x1, y1, x2, y2;
  gint         width, height;
  guchar       empty = 0;

  width = height = 0;

  mask_undo = (MaskUndo *) mask_ptr;

  if (gimp_channel_bounds (mask_undo->channel, &x1, &y1, &x2, &y2))
    {
      new_tiles = tile_manager_new ((x2 - x1), (y2 - y1), 1);

      pixel_region_init (&srcPR, GIMP_DRAWABLE (mask_undo->channel)->tiles,
                         x1, y1, (x2 - x1), (y2 - y1), FALSE);
      pixel_region_init (&destPR, new_tiles,
			 0, 0, (x2 - x1), (y2 - y1), TRUE);

      copy_region (&srcPR, &destPR);

      pixel_region_init (&srcPR, GIMP_DRAWABLE (mask_undo->channel)->tiles,
			 x1, y1, (x2 - x1), (y2 - y1), TRUE);

      color_region (&srcPR, &empty);
    }
  else
    new_tiles = NULL;

  if (mask_undo->tiles)
    {
      width  = tile_manager_width (mask_undo->tiles);
      height = tile_manager_height (mask_undo->tiles);

      pixel_region_init (&srcPR, mask_undo->tiles,
			 0, 0, width, height, FALSE);
      pixel_region_init (&destPR, GIMP_DRAWABLE (mask_undo->channel)->tiles,
			 mask_undo->x, mask_undo->y, width, height, TRUE);

      copy_region (&srcPR, &destPR);

      tile_manager_destroy (mask_undo->tiles);
    }

  if (mask_undo->channel == gimp_image_get_mask (gimage))
    {
      /* invalidate the current bounds and boundary of the mask */
      gimp_image_mask_invalidate (gimage);
    }
  else
    {
      mask_undo->channel->boundary_known = FALSE;
      GIMP_DRAWABLE (mask_undo->channel)->preview_valid = FALSE;
    }

  if (mask_undo->tiles)
    {
      mask_undo->channel->empty = FALSE;
      mask_undo->channel->x1    = mask_undo->x;
      mask_undo->channel->y1    = mask_undo->y;
      mask_undo->channel->x2    = mask_undo->x + width;
      mask_undo->channel->y2    = mask_undo->y + height;
    }
  else
    {
      mask_undo->channel->empty = TRUE;
      mask_undo->channel->x1    = 0;
      mask_undo->channel->y1    = 0;
      mask_undo->channel->x2    = GIMP_DRAWABLE (mask_undo->channel)->width;
      mask_undo->channel->y2    = GIMP_DRAWABLE (mask_undo->channel)->height;
    }

  /* we know the bounds */
  mask_undo->channel->bounds_known = TRUE;

  /*  set the new mask undo parameters  */
  mask_undo->tiles = new_tiles;
  mask_undo->x     = x1;
  mask_undo->y     = y1;

  if (mask_undo->channel == gimp_image_get_mask (gimage))
    {
      mask_changed = TRUE;
    }
  else
    {
      gimp_drawable_update (GIMP_DRAWABLE (mask_undo->channel),
                            0, 0,
                            GIMP_DRAWABLE (mask_undo->channel)->width,
                            GIMP_DRAWABLE (mask_undo->channel)->height);
    }

  return TRUE;
}

static void
undo_free_mask (UndoState  state,
                UndoType   type,
                gpointer   mask_ptr)
{
  MaskUndo *mask_undo;

  mask_undo = (MaskUndo *) mask_ptr;
  if (mask_undo->tiles)
    tile_manager_destroy (mask_undo->tiles);
  g_free (mask_undo);
}


/**********************/
/*  Item Rename Undo  */

typedef struct _ItemRenameUndo ItemRenameUndo;

struct _ItemRenameUndo
{
  GimpItem *item;
  gchar    *old_name;
};

static gboolean undo_pop_item_rename  (GimpImage *,
                                       UndoState, UndoType, gpointer);
static void     undo_free_item_rename (UndoState, UndoType, gpointer);

gboolean
undo_push_item_rename (GimpImage *gimage, 
                       GimpItem  *item)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ItemRenameUndo),
                        ITEM_RENAME_UNDO, TRUE)))
    {
      ItemRenameUndo *iru;

      iru = g_new0 (ItemRenameUndo, 1);

      new->data      = iru;
      new->pop_func  = undo_pop_item_rename;
      new->free_func = undo_free_item_rename;

      iru->item     = item;
      iru->old_name = g_strdup (gimp_object_get_name (GIMP_OBJECT (item)));

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_item_rename (GimpImage *gimage,
                      UndoState  state,
                      UndoType   type,
                      gpointer   iru_ptr)
{
  ItemRenameUndo *iru;
  gchar          *tmp;

  iru = (ItemRenameUndo *) iru_ptr;

  tmp = g_strdup (gimp_object_get_name (GIMP_OBJECT (iru->item)));
  gimp_object_set_name (GIMP_OBJECT (iru->item), iru->old_name);
  g_free (iru->old_name);
  iru->old_name = tmp;

  return TRUE;
}

static void
undo_free_item_rename (UndoState  state,
                       UndoType   type,
                       gpointer   iru_ptr)
{
  ItemRenameUndo *iru;

  iru = (ItemRenameUndo *) iru_ptr;

  g_free (iru->old_name);
  g_free (iru);
}


/***************************/
/*  Layer Add/Remove Undo  */

typedef struct _LayerUndo LayerUndo;

struct _LayerUndo
{
  GimpLayer *layer;           /*  the actual layer         */
  gint       prev_position;   /*  former position in list  */
  GimpLayer *prev_layer;      /*  previous active layer    */
};

static gboolean undo_push_layer (GimpImage *gimage,
                                 UndoType   type,
                                 GimpLayer *layer,
                                 gint       prev_position,
                                 GimpLayer *prev_layer);
static gboolean undo_pop_layer  (GimpImage *,
                                 UndoState, UndoType, gpointer);
static void     undo_free_layer (UndoState, UndoType, gpointer);

gboolean
undo_push_layer_add (GimpImage *gimage,
                     GimpLayer *layer,
                     gint       prev_position,
                     GimpLayer *prev_layer)
{
  return undo_push_layer (gimage, LAYER_ADD_UNDO,
                          layer, prev_position, prev_layer);
}

gboolean
undo_push_layer_remove (GimpImage *gimage,
                        GimpLayer *layer,
                        gint       prev_position,
                        GimpLayer *prev_layer)
{
  return undo_push_layer (gimage, LAYER_REMOVE_UNDO,
                          layer, prev_position, prev_layer);
}

static gboolean
undo_push_layer (GimpImage *gimage,
		 UndoType   type,
                 GimpLayer *layer,
                 gint       prev_position,
                 GimpLayer *prev_layer)
{
  Undo  *new;
  gsize  size;

  g_return_val_if_fail (type == LAYER_ADD_UNDO ||
			type == LAYER_REMOVE_UNDO,
			FALSE);

  size = sizeof (LayerUndo) + gimp_object_get_memsize (GIMP_OBJECT (layer));

  if ((new = undo_push (gimage, size, type, TRUE)))
    {
      LayerUndo *lu;

      lu = g_new0 (LayerUndo, 1);

      new->data      = lu;
      new->pop_func  = undo_pop_layer;
      new->free_func = undo_free_layer;

      g_object_ref (G_OBJECT (layer));

      lu->layer         = layer;
      lu->prev_position = prev_position;
      lu->prev_layer    = prev_layer;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer (GimpImage *gimage,
		UndoState  state,
		UndoType   type,
		gpointer   lu_ptr)
{
  LayerUndo *lu;

  lu = (LayerUndo *) lu_ptr;

  if ((state == UNDO && type == LAYER_ADD_UNDO) ||
      (state == REDO && type == LAYER_REMOVE_UNDO))
    {
      /*  remove layer  */

      /*  record the current position  */
      lu->prev_position = gimp_image_get_layer_index (gimage, lu->layer);

      /*  if exists, set the previous layer                                           */
      /* (counterexample: result layer added after a merge op has no previous layer.) */

      if(GIMP_IS_LAYER (lu->prev_layer))
	gimp_image_set_active_layer (gimage, lu->prev_layer);

      /*  remove the layer  */
      gimp_container_remove (gimage->layers, GIMP_OBJECT (lu->layer));
      gimage->layer_stack = g_slist_remove (gimage->layer_stack, lu->layer);

      /*  reset the gimage values  */
      if (gimp_layer_is_floating_sel (lu->layer))
	{
	  gimage->floating_sel = NULL;
	  /*  reset the old drawable  */
	  floating_sel_reset (lu->layer);

	  gimp_image_floating_selection_changed (gimage);
	}

      gimp_drawable_update (GIMP_DRAWABLE (lu->layer),
			    0, 0,
			    GIMP_DRAWABLE (lu->layer)->width,
			    GIMP_DRAWABLE (lu->layer)->height);

      if (gimp_container_num_children (gimage->layers) == 1 &&
          ! gimp_drawable_has_alpha (GIMP_LIST (gimage->layers)->list->data))
        {
          alpha_changed = TRUE;
        }
    }
  else
    {
      /*  restore layer  */

      /*  record the active layer  */
      lu->prev_layer = gimp_image_get_active_layer (gimage);

      /*  hide the current selection--for all views  */
      if (gimp_image_get_active_layer (gimage))
	gimp_layer_invalidate_boundary (gimp_image_get_active_layer (gimage));

      /*  if this is a floating selection, set the fs pointer  */
      if (gimp_layer_is_floating_sel (lu->layer))
	gimage->floating_sel = lu->layer;

      if (gimp_container_num_children (gimage->layers) == 1 &&
          ! gimp_drawable_has_alpha (GIMP_LIST (gimage->layers)->list->data))
        {
          alpha_changed = TRUE;
        }

      /*  add the new layer  */
      gimp_container_insert (gimage->layers, 
			     GIMP_OBJECT (lu->layer), lu->prev_position);
      gimp_image_set_active_layer (gimage, lu->layer);

      if (gimp_layer_is_floating_sel (lu->layer))
	gimp_image_floating_selection_changed (gimage);

      gimp_drawable_update (GIMP_DRAWABLE (lu->layer),
			    0, 0,
			    GIMP_DRAWABLE (lu->layer)->width,
			    GIMP_DRAWABLE (lu->layer)->height);
    }

  return TRUE;
}

static void
undo_free_layer (UndoState  state,
		 UndoType   type,
		 gpointer   lu_ptr)
{
  LayerUndo *lu;

  lu = (LayerUndo *) lu_ptr;

  g_object_unref (G_OBJECT (lu->layer));
  g_free (lu);
}


/********************/
/*  Layer Mod Undo  */

typedef struct _LayerModUndo LayerModUndo;

struct _LayerModUndo
{
  GimpLayer    *layer;
  TileManager  *tiles;
  GimpImageType type;
  gint          offset_x;
  gint		offset_y;
};

static gboolean undo_pop_layer_mod  (GimpImage *,
                                     UndoState, UndoType, gpointer);
static void     undo_free_layer_mod (UndoState, UndoType, gpointer);

gboolean
undo_push_layer_mod (GimpImage *gimage,
		     GimpLayer *layer)
{
  Undo         *new;
  TileManager  *tiles;
  gsize         size;

  tiles = GIMP_DRAWABLE (layer)->tiles;

  size = sizeof (LayerModUndo) + tile_manager_get_memsize (tiles);

  if ((new = undo_push (gimage, size, LAYER_MOD_UNDO, TRUE)))
    {
      LayerModUndo *lmu;

      lmu = g_new0 (LayerModUndo, 1);

      new->data      = lmu;
      new->pop_func  = undo_pop_layer_mod;
      new->free_func = undo_free_layer_mod;

      lmu->layer     = layer;
      lmu->tiles     = tiles;
      lmu->type      = GIMP_DRAWABLE (layer)->type;
      lmu->offset_x  = GIMP_DRAWABLE (layer)->offset_x;
      lmu->offset_y  = GIMP_DRAWABLE (layer)->offset_y;

      return TRUE;
    }

  tile_manager_destroy (tiles);

  return FALSE;
}

static gboolean
undo_pop_layer_mod (GimpImage *gimage,
		    UndoState  state,
		    UndoType   type,
		    gpointer   lmu_ptr)
{
  LayerModUndo *lmu;
  gint          layer_type;
  gint          offset_x, offset_y;
  TileManager  *tiles;
  TileManager  *temp;
  GimpLayer    *layer;
  gboolean      old_has_alpha;

  lmu = (LayerModUndo *) lmu_ptr;

  layer    = lmu->layer;
  tiles    = lmu->tiles;
  offset_x = lmu->offset_x;
  offset_y = lmu->offset_y;

  /*  Issue the first update  */
  gimp_image_update (gimage,
                     GIMP_DRAWABLE (layer)->offset_x,
                     GIMP_DRAWABLE (layer)->offset_y,
                     GIMP_DRAWABLE (layer)->width,
                     GIMP_DRAWABLE (layer)->height);

  /*  Create a tile manager to store the current layer contents  */
  temp       = GIMP_DRAWABLE (layer)->tiles;
  
  lmu->offset_x = GIMP_DRAWABLE (layer)->offset_x;
  lmu->offset_y = GIMP_DRAWABLE (layer)->offset_y;

  layer_type = lmu->type;
  lmu->type = GIMP_DRAWABLE (layer)->type;

  old_has_alpha = GIMP_DRAWABLE (layer)->has_alpha;

  /*  restore the layer's data  */
  GIMP_DRAWABLE (layer)->tiles     = tiles;
  GIMP_DRAWABLE (layer)->width     = tile_manager_width (tiles);
  GIMP_DRAWABLE (layer)->height    = tile_manager_height (tiles);
  GIMP_DRAWABLE (layer)->bytes     = tile_manager_bpp (tiles);
  GIMP_DRAWABLE (layer)->type      = layer_type;
  GIMP_DRAWABLE (layer)->has_alpha = GIMP_IMAGE_TYPE_HAS_ALPHA (layer_type);
  GIMP_DRAWABLE (layer)->offset_x  = offset_x;
  GIMP_DRAWABLE (layer)->offset_y  = offset_y;
  if (layer->mask) 
    {
      GIMP_DRAWABLE (layer->mask)->offset_x = offset_x;
      GIMP_DRAWABLE (layer->mask)->offset_y = offset_y;
    }

  if (GIMP_DRAWABLE (layer)->has_alpha != old_has_alpha &&
      GIMP_ITEM (layer)->gimage->layers->num_children == 1)
    {
      gimp_image_alpha_changed (GIMP_ITEM (layer)->gimage);
    }

  /*  Set the new tile manager  */
  lmu->tiles = temp;

  /*  Issue the second update  */
  gimp_drawable_update (GIMP_DRAWABLE (layer),
			0, 0, 
			GIMP_DRAWABLE (layer)->width,
			GIMP_DRAWABLE (layer)->height);

  return TRUE;
}

static void
undo_free_layer_mod (UndoState  state,
		     UndoType   type,
		     gpointer   lmu_ptr)
{
  LayerModUndo *lmu;

  lmu = (LayerModUndo *) lmu_ptr;

  tile_manager_destroy (lmu->tiles);
  g_free (lmu);
}


/********************************/
/*  Layer Mask Add/Remove Undo  */

typedef struct _LayerMaskUndo LayerMaskUndo;

struct _LayerMaskUndo
{
  GimpLayer     *layer;    /*  the layer       */
  GimpLayerMask *mask;     /*  the layer mask  */
};

static gboolean undo_push_layer_mask (GimpImage     *gimage,
                                      UndoType       type,
                                      GimpLayer     *layer,
                                      GimpLayerMask *mask);
static gboolean undo_pop_layer_mask  (GimpImage *,
                                      UndoState, UndoType, gpointer);
static void     undo_free_layer_mask (UndoState, UndoType, gpointer);

gboolean
undo_push_layer_mask_add (GimpImage     *gimage,
                          GimpLayer     *layer,
                          GimpLayerMask *mask)
{
  return undo_push_layer_mask (gimage, LAYER_MASK_ADD_UNDO,
                               layer, mask);
}

gboolean
undo_push_layer_mask_remove (GimpImage     *gimage,
                             GimpLayer     *layer,
                             GimpLayerMask *mask)
{
  return undo_push_layer_mask (gimage, LAYER_MASK_REMOVE_UNDO,
                               layer, mask);
}

static gboolean
undo_push_layer_mask (GimpImage     *gimage,
		      UndoType       type,
                      GimpLayer     *layer,
                      GimpLayerMask *mask)
{
  Undo  *new;
  gsize  size;

  g_return_val_if_fail (type == LAYER_MASK_ADD_UNDO ||
			type == LAYER_MASK_REMOVE_UNDO,
			FALSE);

  size = sizeof (LayerMaskUndo) + gimp_object_get_memsize (GIMP_OBJECT (mask));

  if ((new = undo_push (gimage, size, type, TRUE)))
    {
      LayerMaskUndo *lmu;

      lmu = g_new0 (LayerMaskUndo, 1);

      new->data      = lmu;
      new->pop_func  = undo_pop_layer_mask;
      new->free_func = undo_free_layer_mask;

      g_object_ref (G_OBJECT (mask));

      lmu->layer = layer;
      lmu->mask  = mask;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer_mask (GimpImage *gimage,
		     UndoState  state,
		     UndoType   type,
		     gpointer   lmu_ptr)
{
  LayerMaskUndo *lmu;

  lmu = (LayerMaskUndo *) lmu_ptr;

  if ((state == UNDO && type == LAYER_MASK_ADD_UNDO) ||
      (state == REDO && type == LAYER_MASK_REMOVE_UNDO))
    {
      /*  remove layer mask  */

      gimp_layer_apply_mask (lmu->layer, GIMP_MASK_DISCARD, FALSE);
    }
  else
    {
      /*  restore layer  */

      gimp_layer_add_mask (lmu->layer, lmu->mask, FALSE);
    }

  return TRUE;
}

static void
undo_free_layer_mask (UndoState  state,
		      UndoType   type,
		      gpointer   lmu_ptr)
{
  LayerMaskUndo *lmu;

  lmu = (LayerMaskUndo *) lmu_ptr;

  g_object_unref (G_OBJECT (lmu->mask));
  g_free (lmu);
}


/***************************/
/* Layer re-position Undo  */

typedef struct _LayerRepositionUndo LayerRepositionUndo;

struct _LayerRepositionUndo
{
  GimpLayer *layer;
  gint       old_position;
};

static gboolean undo_pop_layer_reposition  (GimpImage *,
                                            UndoState, UndoType, gpointer);
static void     undo_free_layer_reposition (UndoState, UndoType, gpointer);

gboolean
undo_push_layer_reposition (GimpImage *gimage, 
			    GimpLayer *layer)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (LayerRepositionUndo),
                        LAYER_REPOSITION_UNDO, TRUE)))
    {
      LayerRepositionUndo *lru;

      lru = g_new0 (LayerRepositionUndo, 1);

      new->data      = lru;
      new->pop_func  = undo_pop_layer_reposition;
      new->free_func = undo_free_layer_reposition;

      lru->layer        = layer;
      lru->old_position = gimp_image_get_layer_index (gimage, layer);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_layer_reposition (GimpImage *gimage,
			   UndoState  state,
			   UndoType   type,
			   void      *data_ptr)
{
  LayerRepositionUndo *data = data_ptr;
  gint                 pos;

  /* what's the layer's current index? */
  pos = gimp_image_get_layer_index (gimage, data->layer);
  gimp_image_position_layer (gimage, data->layer, data->old_position, FALSE);

  data->old_position = pos;

  return TRUE;
}

static void
undo_free_layer_reposition (UndoState  state,
			    UndoType   type,
			    void      *data_ptr)
{
  g_free (data_ptr);
}


/*****************************/
/*  Layer displacement Undo  */

typedef struct _LayerDisplaceUndo LayerDisplaceUndo;

struct _LayerDisplaceUndo
{
  gint    info[3];
  GSList *path_undo;
};

static gboolean undo_pop_layer_displace  (GimpImage *,
                                          UndoState, UndoType, gpointer);
static void     undo_free_layer_displace (UndoState, UndoType, gpointer);

gboolean
undo_push_layer_displace (GimpImage *gimage,
			  GimpLayer *layer)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (LayerDisplaceUndo),
                        LAYER_DISPLACE_UNDO, TRUE)))
    {
      LayerDisplaceUndo *ldu;

      ldu = g_new (LayerDisplaceUndo, 1);

      new->data      = ldu;
      new->pop_func  = undo_pop_layer_displace;
      new->free_func = undo_free_layer_displace;

      ldu->info[0]   = gimp_item_get_ID (GIMP_ITEM (layer));
      ldu->info[1]   = GIMP_DRAWABLE (layer)->offset_x;
      ldu->info[2]   = GIMP_DRAWABLE (layer)->offset_y;
      ldu->path_undo = path_transform_start_undo (gimage);

      return TRUE;
    }

  return FALSE;
}


static gboolean
undo_pop_layer_displace (GimpImage *gimage,
			 UndoState  state,
			 UndoType   type,
			 gpointer   info_ptr)
{
  GimpLayer         *layer;
  gint               old_offsets[2];
  LayerDisplaceUndo *ldu;

  ldu = (LayerDisplaceUndo *) info_ptr;
  layer = (GimpLayer *) gimp_item_get_by_ID (gimage->gimp, ldu->info[0]);
  if (layer)
    {
      old_offsets[0] = GIMP_DRAWABLE (layer)->offset_x;
      old_offsets[1] = GIMP_DRAWABLE (layer)->offset_y;
      gimp_drawable_update (GIMP_DRAWABLE (layer),
			    0, 0,
			    GIMP_DRAWABLE (layer)->width,
			    GIMP_DRAWABLE (layer)->height);

      GIMP_DRAWABLE (layer)->offset_x = ldu->info[1];
      GIMP_DRAWABLE (layer)->offset_y = ldu->info[2];
      gimp_drawable_update (GIMP_DRAWABLE (layer),
			    0, 0,
			    GIMP_DRAWABLE (layer)->width,
			    GIMP_DRAWABLE (layer)->height);
      
      if (layer->mask) 
	{
	  GIMP_DRAWABLE (layer->mask)->offset_x = ldu->info[1];
	  GIMP_DRAWABLE (layer->mask)->offset_y = ldu->info[2];
	  gimp_drawable_update (GIMP_DRAWABLE (layer->mask),
				0, 0,
				GIMP_DRAWABLE (layer->mask)->width,
				GIMP_DRAWABLE (layer->mask)->height);
	}


      /*  invalidate the selection boundary because of a layer modification  */
      gimp_layer_invalidate_boundary (layer);

      ldu->info[1] = old_offsets[0];
      ldu->info[2] = old_offsets[1];

      /* Now undo paths bits */
      if (ldu->path_undo)
	path_transform_do_undo (gimage, ldu->path_undo);

      return TRUE;
    }
  else
    return FALSE;
}


static void
undo_free_layer_displace (UndoState  state,
			  UndoType   type,
			  gpointer   info_ptr)
{
  LayerDisplaceUndo * ldu;

  ldu = (LayerDisplaceUndo *) info_ptr;

  /* Free mem held for paths undo stuff */
  if (ldu->path_undo)
    path_transform_free_undo (ldu->path_undo);  

  g_free (info_ptr);
}


/*****************************/
/*  Add/Remove Channel Undo  */

typedef struct _ChannelUndo ChannelUndo;

struct _ChannelUndo
{
  GimpChannel *channel;         /*  the actual channel          */
  gint         prev_position;   /*  former position in list     */
  GimpChannel *prev_channel;    /*  previous active channel     */
};

static gboolean undo_push_channel (GimpImage   *gimage,
                                   UndoType     type,
                                   GimpChannel *channel,
                                   gint         prev_position,
                                   GimpChannel *prev_channel);
static gboolean undo_pop_channel  (GimpImage *,
                                   UndoState, UndoType, gpointer);
static void     undo_free_channel (UndoState, UndoType, gpointer);

gboolean
undo_push_channel_add (GimpImage   *gimage,
                       GimpChannel *channel,
                       gint         prev_position,
                       GimpChannel *prev_channel)
{
  return undo_push_channel (gimage, CHANNEL_ADD_UNDO,
                            channel, prev_position, prev_channel);
}

gboolean
undo_push_channel_remove (GimpImage   *gimage,
                          GimpChannel *channel,
                          gint         prev_position,
                          GimpChannel *prev_channel)
{
  return undo_push_channel (gimage, CHANNEL_REMOVE_UNDO,
                            channel, prev_position, prev_channel);
}

static gboolean
undo_push_channel (GimpImage   *gimage,
		   UndoType     type,
                   GimpChannel *channel,
                   gint         prev_position,
                   GimpChannel *prev_channel)
{
  Undo  *new;
  gsize  size;

  g_return_val_if_fail (type == CHANNEL_ADD_UNDO ||
			type == CHANNEL_REMOVE_UNDO,
			FALSE);

  size = sizeof (ChannelUndo) + gimp_object_get_memsize (GIMP_OBJECT (channel));

  if ((new = undo_push (gimage, size, type, TRUE)))
    {
      ChannelUndo *cu;

      cu = g_new0 (ChannelUndo, 1);

      new->data      = cu;
      new->pop_func  = undo_pop_channel;
      new->free_func = undo_free_channel;

      g_object_ref (G_OBJECT (channel));

      cu->channel       = channel;
      cu->prev_position = prev_position;
      cu->prev_channel  = prev_channel;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_channel (GimpImage *gimage,
		  UndoState  state,
		  UndoType   type,
		  gpointer   cu_ptr)
{
  ChannelUndo *cu;

  cu = (ChannelUndo *) cu_ptr;

  if ((state == UNDO && type == CHANNEL_ADD_UNDO) ||
      (state == REDO && type == CHANNEL_REMOVE_UNDO))
    {
      /*  remove channel  */

      /*  record the current position  */
      cu->prev_position = gimp_image_get_channel_index (gimage, cu->channel);

      /*  remove the channel  */
      gimp_container_remove (gimage->channels, GIMP_OBJECT (cu->channel));

      if (cu->prev_channel)
        {
          /*  set the previous channel  */
          gimp_image_set_active_channel (gimage, cu->prev_channel);
        }

      /*  update the area  */
      gimp_drawable_update (GIMP_DRAWABLE (cu->channel),
			    0, 0, 
			    GIMP_DRAWABLE (cu->channel)->width,
			    GIMP_DRAWABLE (cu->channel)->height);
    }
  else
    {
      /*  restore channel  */

      /*  record the active channel  */
      cu->prev_channel = gimp_image_get_active_channel (gimage);

      /*  add the new channel  */
      gimp_container_insert (gimage->channels, 
			     GIMP_OBJECT (cu->channel),	cu->prev_position);
 
      /*  set the new channel  */
      gimp_image_set_active_channel (gimage, cu->channel);

      /*  update the area  */
      gimp_drawable_update (GIMP_DRAWABLE (cu->channel),
			    0, 0, 
			    GIMP_DRAWABLE (cu->channel)->width,
			    GIMP_DRAWABLE (cu->channel)->height);
    }

  return TRUE;
}

static void
undo_free_channel (UndoState  state,
		   UndoType   type,
		   gpointer   cu_ptr)
{
  ChannelUndo *cu;

  cu = (ChannelUndo *) cu_ptr;

  g_object_unref (G_OBJECT (cu->channel));

  g_free (cu);
}


/**********************/
/*  Channel Mod Undo  */

typedef struct _ChannelModUndo ChannelModUndo;

struct _ChannelModUndo
{
  GimpChannel *channel;
  TileManager *tiles;
};

static gboolean undo_pop_channel_mod  (GimpImage *,
                                       UndoState, UndoType, gpointer);
static void     undo_free_channel_mod (UndoState, UndoType, gpointer);

gboolean
undo_push_channel_mod (GimpImage   *gimage,
		       GimpChannel *channel)
{
  TileManager *tiles;
  Undo        *new;
  gsize        size;

  tiles = GIMP_DRAWABLE (channel)->tiles;

  size  = sizeof (ChannelModUndo) + tile_manager_get_memsize (tiles);

  if ((new = undo_push (gimage, size, CHANNEL_MOD_UNDO, TRUE)))
    {
      ChannelModUndo *cmu;

      cmu = g_new0 (ChannelModUndo, 1);

      new->data      = cmu;
      new->pop_func  = undo_pop_channel_mod;
      new->free_func = undo_free_channel_mod;

      cmu->channel = channel;
      cmu->tiles   = tiles;

      return TRUE;
    }

  tile_manager_destroy (tiles);

  return FALSE;
}

static gboolean
undo_pop_channel_mod (GimpImage *gimage,
		      UndoState  state,
		      UndoType   type,
		      gpointer   cmu_ptr)
{
  ChannelModUndo *cmu;
  TileManager    *tiles;
  TileManager    *temp;
  GimpChannel    *channel;

  cmu = (ChannelModUndo *) cmu_ptr;

  channel = cmu->channel;
  tiles   = cmu->tiles;

  /*  Issue the first update  */
  gimp_drawable_update (GIMP_DRAWABLE (channel),
			0, 0, 
			GIMP_DRAWABLE (channel)->width,
			GIMP_DRAWABLE (channel)->height);

  temp = GIMP_DRAWABLE (channel)->tiles;

  GIMP_DRAWABLE (channel)->tiles       = tiles;
  GIMP_DRAWABLE (channel)->width       = tile_manager_width (tiles);
  GIMP_DRAWABLE (channel)->height      = tile_manager_height (tiles);
  GIMP_CHANNEL (channel)->bounds_known = FALSE; 

  /*  Set the new buffer  */
  cmu->tiles = temp;

  /*  Issue the second update  */
  gimp_drawable_update (GIMP_DRAWABLE (channel),
			0, 0, 
			GIMP_DRAWABLE (channel)->width,
			GIMP_DRAWABLE (channel)->height);

  return TRUE;
}

static void
undo_free_channel_mod (UndoState state,
		       UndoType  type,
		       gpointer  cmu_ptr)
{
  ChannelModUndo *cmu;

  cmu = (ChannelModUndo *) cmu_ptr;

  tile_manager_destroy (cmu->tiles);

  g_free (cmu);
}


/******************************/
/*  Channel re-position Undo  */

typedef struct _ChannelRepositionUndo ChannelRepositionUndo;

struct _ChannelRepositionUndo
{
  GimpChannel *channel;
  gint         old_position;
};

static gboolean undo_pop_channel_reposition  (GimpImage *,
                                              UndoState, UndoType, gpointer);
static void     undo_free_channel_reposition (UndoState, UndoType, gpointer);

gboolean
undo_push_channel_reposition (GimpImage   *gimage, 
                              GimpChannel *channel)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ChannelRepositionUndo),
                        CHANNEL_REPOSITION_UNDO, TRUE)))
    {
      ChannelRepositionUndo *cru;

      cru = g_new0 (ChannelRepositionUndo, 1);

      new->data      = cru;
      new->pop_func  = undo_pop_channel_reposition;
      new->free_func = undo_free_channel_reposition;

      cru->channel      = channel;
      cru->old_position = gimp_image_get_channel_index (gimage, channel);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_channel_reposition (GimpImage *gimage,
                             UndoState  state,
                             UndoType   type,
                             gpointer   cru_ptr)
{
  ChannelRepositionUndo *cru;
  gint                   pos;

  cru = (ChannelRepositionUndo *) cru_ptr;

  /* what's the channel's current index? */
  pos = gimp_image_get_channel_index (gimage, cru->channel);
  gimp_image_position_channel (gimage, cru->channel, cru->old_position, FALSE);

  cru->old_position = pos;

  return TRUE;
}

static void
undo_free_channel_reposition (UndoState  state,
                              UndoType   type,
                              gpointer   cru_ptr)
{
  g_free (cru_ptr);
}


/*****************************/
/*  Add/Remove Vectors Undo  */

typedef struct _VectorsUndo VectorsUndo;

struct _VectorsUndo
{
  GimpVectors *vectors;         /*  the actual vectors          */
  gint         prev_position;   /*  former position in list     */
  GimpVectors *prev_vectors;    /*  previous active vectors     */
};

static gboolean undo_push_vectors (GimpImage   *gimage,
                                   UndoType     type,
                                   GimpVectors *vectors,
                                   gint         prev_position,
                                   GimpVectors *prev_vectors);
static gboolean undo_pop_vectors  (GimpImage *,
                                   UndoState, UndoType, gpointer);
static void     undo_free_vectors (UndoState, UndoType, gpointer);

gboolean
undo_push_vectors_add (GimpImage   *gimage,
                       GimpVectors *vectors,
                       gint         prev_position,
                       GimpVectors *prev_vectors)
{
  return undo_push_vectors (gimage, VECTORS_ADD_UNDO,
                            vectors, prev_position, prev_vectors);
}

gboolean
undo_push_vectors_remove (GimpImage   *gimage,
                          GimpVectors *vectors,
                          gint         prev_position,
                          GimpVectors *prev_vectors)
{
  return undo_push_vectors (gimage, VECTORS_REMOVE_UNDO,
                            vectors, prev_position, prev_vectors);
}

static gboolean
undo_push_vectors (GimpImage   *gimage,
		   UndoType     type,
                   GimpVectors *vectors,
                   gint         prev_position,
                   GimpVectors *prev_vectors)
{
  Undo  *new;
  gsize  size;

  g_return_val_if_fail (type == VECTORS_ADD_UNDO ||
			type == VECTORS_REMOVE_UNDO,
			FALSE);

  size = sizeof (VectorsUndo) + gimp_object_get_memsize (GIMP_OBJECT (vectors));

  if ((new = undo_push (gimage, size, type, TRUE)))
    {
      VectorsUndo *vu;

      vu = g_new0 (VectorsUndo, 1);

      new->data      = vu;
      new->pop_func  = undo_pop_vectors;
      new->free_func = undo_free_vectors;

      g_object_ref (G_OBJECT (vectors));

      vu->vectors       = vectors;
      vu->prev_position = prev_position;
      vu->prev_vectors  = prev_vectors;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_vectors (GimpImage *gimage,
		  UndoState  state,
		  UndoType   type,
		  gpointer   vu_ptr)
{
  VectorsUndo *vu;

  vu = (VectorsUndo *) vu_ptr;

  if ((state == UNDO && type == VECTORS_ADD_UNDO) ||
      (state == REDO && type == VECTORS_REMOVE_UNDO))
    {
      /*  remove vectors  */

      /*  record the current position  */
      vu->prev_position = gimp_image_get_vectors_index (gimage, vu->vectors);

      /*  remove the vectors  */
      gimp_container_remove (gimage->vectors, GIMP_OBJECT (vu->vectors));

      if (vu->prev_vectors)
        {
          /*  set the previous vectors  */
          gimp_image_set_active_vectors (gimage, vu->prev_vectors);
        }
    }
  else
    {
      /*  restore vectors  */

      /*  record the active vectors  */
      vu->prev_vectors = gimp_image_get_active_vectors (gimage);

      /*  add the new vectors  */
      gimp_container_insert (gimage->vectors, 
			     GIMP_OBJECT (vu->vectors),	vu->prev_position);
 
      /*  set the new vectors  */
      gimp_image_set_active_vectors (gimage, vu->vectors);
    }

  return TRUE;
}

static void
undo_free_vectors (UndoState  state,
		   UndoType   type,
		   gpointer   vu_ptr)
{
  VectorsUndo *vu;

  vu = (VectorsUndo *) vu_ptr;

  g_object_unref (G_OBJECT (vu->vectors));

  g_free (vu);
}


/**********************/
/*  Vectors Mod Undo  */

typedef struct _VectorsModUndo VectorsModUndo;

struct _VectorsModUndo
{
  GimpVectors *vectors;
  GimpVectors *undo_vectors;
};

static gboolean undo_pop_vectors_mod  (GimpImage *,
                                       UndoState, UndoType, gpointer);
static void     undo_free_vectors_mod (UndoState, UndoType, gpointer);

gboolean
undo_push_vectors_mod (GimpImage   *gimage,
		       GimpVectors *vectors)
{
  Undo  *new;
  gsize  size;

  size = (sizeof (VectorsModUndo) +
          gimp_object_get_memsize (GIMP_OBJECT (vectors)));

  if ((new = undo_push (gimage, size, VECTORS_MOD_UNDO, TRUE)))
    {
      VectorsModUndo *vmu;

      vmu = g_new0 (VectorsModUndo, 1);

      new->data      = vmu;
      new->pop_func  = undo_pop_vectors_mod;
      new->free_func = undo_free_vectors_mod;

      vmu->vectors      = vectors;
      vmu->undo_vectors = NULL; /* gimp_vectors_duplicate (vectors); */

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_vectors_mod (GimpImage *gimage,
		      UndoState  state,
		      UndoType   type,
		      gpointer   vmu_ptr)
{
  VectorsModUndo *vmu;
  GimpVectors    *temp;

  vmu = (VectorsModUndo *) vmu_ptr;

  temp = vmu->undo_vectors;

  vmu->undo_vectors = NULL; /* gimp_vectors_duplicate (vmu->vectors); */

  /* gimp_vectors_copy_strokes (temp, vmu->vectors); */

  /* g_object_unref (G_OBJECT (temp)); */

  return TRUE;
}

static void
undo_free_vectors_mod (UndoState state,
		       UndoType  type,
		       gpointer  vmu_ptr)
{
  VectorsModUndo *vmu;

  vmu = (VectorsModUndo *) vmu_ptr;

  /* g_object_unref (G_OBJECT (vmu->undo_vectors)); */

  g_free (vmu);
}


/******************************/
/*  Vectors re-position Undo  */

typedef struct _VectorsRepositionUndo VectorsRepositionUndo;

struct _VectorsRepositionUndo
{
  GimpVectors *vectors;
  gint         old_position;
};

static gboolean undo_pop_vectors_reposition  (GimpImage *,
                                              UndoState, UndoType, gpointer);
static void     undo_free_vectors_reposition (UndoState, UndoType, gpointer);

gboolean
undo_push_vectors_reposition (GimpImage   *gimage, 
                              GimpVectors *vectors)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (VectorsRepositionUndo),
                        VECTORS_REPOSITION_UNDO, TRUE)))
    {
      VectorsRepositionUndo *vru;

      vru = g_new0 (VectorsRepositionUndo, 1);

      new->data      = vru;
      new->pop_func  = undo_pop_vectors_reposition;
      new->free_func = undo_free_vectors_reposition;

      vru->vectors      = vectors;
      vru->old_position = gimp_image_get_vectors_index (gimage, vectors);

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_vectors_reposition (GimpImage *gimage,
                             UndoState  state,
                             UndoType   type,
                             gpointer   vru_ptr)
{
  VectorsRepositionUndo *vru;
  gint                   pos;

  vru = (VectorsRepositionUndo *) vru_ptr;

  /* what's the vectors's current index? */
  pos = gimp_image_get_vectors_index (gimage, vru->vectors);
  gimp_image_position_vectors (gimage, vru->vectors, vru->old_position, FALSE);

  vru->old_position = pos;

  return TRUE;
}

static void
undo_free_vectors_reposition (UndoState  state,
                              UndoType   type,
                              gpointer   vru_ptr)
{
  g_free (vru_ptr);
}


/**************************************/
/*  Floating Selection to Layer Undo  */

typedef struct _FStoLayerUndo FStoLayerUndo;

struct _FStoLayerUndo
{
  GimpLayer    *floating_layer; /*  the floating layer        */
  GimpDrawable *drawable;       /*  drawable of floating sel  */
};

static gboolean undo_pop_fs_to_layer  (GimpImage *,
                                       UndoState, UndoType, gpointer);
static void     undo_free_fs_to_layer (UndoState, UndoType, gpointer);

gboolean
undo_push_fs_to_layer (GimpImage    *gimage,
                       GimpLayer    *floating_layer,
                       GimpDrawable *drawable)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (FStoLayerUndo),
                        FS_TO_LAYER_UNDO, TRUE)))
    {
      FStoLayerUndo *fsu;

      fsu = g_new0 (FStoLayerUndo, 1);

      new->data      = fsu;
      new->pop_func  = undo_pop_fs_to_layer;
      new->free_func = undo_free_fs_to_layer;

      fsu->floating_layer = floating_layer;
      fsu->drawable       = drawable;

      return TRUE;
    }

  tile_manager_destroy (floating_layer->fs.backing_store);
  floating_layer->fs.backing_store = NULL;

  return FALSE;
}

static gboolean
undo_pop_fs_to_layer (GimpImage *gimage,
		      UndoState  state,
		      UndoType   type,
		      gpointer   fsu_ptr)
{
  FStoLayerUndo *fsu;

  fsu = (FStoLayerUndo *) fsu_ptr;

  switch (state)
    {
    case UNDO:
      /*  Update the preview for the floating sel  */
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (fsu->floating_layer));

      fsu->floating_layer->fs.drawable = fsu->drawable;
      gimp_image_set_active_layer (gimage, fsu->floating_layer);
      gimage->floating_sel = fsu->floating_layer;

      /*  restore the contents of the drawable  */
      floating_sel_store (fsu->floating_layer, 
			  GIMP_DRAWABLE (fsu->floating_layer)->offset_x, 
			  GIMP_DRAWABLE (fsu->floating_layer)->offset_y,
			  GIMP_DRAWABLE (fsu->floating_layer)->width, 
			  GIMP_DRAWABLE (fsu->floating_layer)->height);
      fsu->floating_layer->fs.initial = TRUE;

      /*  clear the selection  */
      gimp_layer_invalidate_boundary (fsu->floating_layer);

      /*  Update the preview for the gimage and underlying drawable  */
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (fsu->floating_layer));
      break;

    case REDO:
      /*  restore the contents of the drawable  */
      floating_sel_restore (fsu->floating_layer, 
			    GIMP_DRAWABLE (fsu->floating_layer)->offset_x, 
			    GIMP_DRAWABLE (fsu->floating_layer)->offset_y,
			    GIMP_DRAWABLE (fsu->floating_layer)->width, 
			    GIMP_DRAWABLE (fsu->floating_layer)->height);

      /*  Update the preview for the gimage and underlying drawable  */
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (fsu->floating_layer));

      /*  clear the selection  */
      gimp_layer_invalidate_boundary (fsu->floating_layer);

      /*  update the pointers  */
      fsu->floating_layer->fs.drawable = NULL;
      gimage->floating_sel = NULL;

      /*  Update the fs drawable  */
      gimp_viewable_invalidate_preview (GIMP_VIEWABLE (fsu->floating_layer));
      break;
    }

  gimp_image_floating_selection_changed (gimage);

  return TRUE;
}

static void
undo_free_fs_to_layer (UndoState  state,
		       UndoType   type,
		       gpointer   fsu_ptr)
{
  FStoLayerUndo *fsu;

  fsu = (FStoLayerUndo *) fsu_ptr;

  if (state == UNDO)
    {
      tile_manager_destroy (fsu->floating_layer->fs.backing_store);
      fsu->floating_layer->fs.backing_store = NULL;
    }

  g_free (fsu);
}


/***********************************/
/*  Floating Selection Rigor Undo  */

static gboolean undo_pop_fs_rigor  (GimpImage *,
                                    UndoState, UndoType, gpointer);
static void     undo_free_fs_rigor (UndoState, UndoType, gpointer);

gboolean
undo_push_fs_rigor (GimpImage *gimage,
		    gint       layer_ID)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (gint32),
                        FS_RIGOR_UNDO, FALSE)))
    {
      new->data      = g_new (gint32, 1);
      new->pop_func  = undo_pop_fs_rigor;
      new->free_func = undo_free_fs_rigor;

      *((gint32 *) new->data) = layer_ID;

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
undo_pop_fs_rigor (GimpImage *gimage,
		   UndoState  state,
		   UndoType   type,
		   gpointer   layer_ptr)
{
  gint32     layer_ID;
  GimpLayer *floating_layer;

  layer_ID = *((gint32 *) layer_ptr);

  if ((floating_layer = (GimpLayer *) gimp_item_get_by_ID (gimage->gimp, layer_ID)) == NULL)
    return FALSE;

  if (! gimp_layer_is_floating_sel (floating_layer))
    return FALSE;

  switch (state)
    {
    case UNDO:
      /*  restore the contents of drawable the floating layer is attached to  */
      if (floating_layer->fs.initial == FALSE)
	floating_sel_restore (floating_layer,
			      GIMP_DRAWABLE (floating_layer)->offset_x, 
			      GIMP_DRAWABLE (floating_layer)->offset_y,
			      GIMP_DRAWABLE (floating_layer)->width, 
			      GIMP_DRAWABLE (floating_layer)->height);
      floating_layer->fs.initial = TRUE;
      break;

    case REDO:
      /*  store the affected area from the drawable in the backing store  */
      floating_sel_store (floating_layer,
			  GIMP_DRAWABLE (floating_layer)->offset_x, 
			  GIMP_DRAWABLE (floating_layer)->offset_y,
			  GIMP_DRAWABLE (floating_layer)->width, 
			  GIMP_DRAWABLE (floating_layer)->height);
      floating_layer->fs.initial = TRUE;
      break;
    }

  gimp_image_floating_selection_changed (gimage);

  return TRUE;
}

static void
undo_free_fs_rigor (UndoState  state,
		    UndoType   type,
		    gpointer   layer_ptr)
{
  g_free (layer_ptr);
}


/***********************************/
/*  Floating Selection Relax Undo  */

static gboolean undo_pop_fs_relax  (GimpImage *,
                                    UndoState, UndoType, gpointer);
static void     undo_free_fs_relax (UndoState, UndoType, gpointer);

gboolean
undo_push_fs_relax (GimpImage *gimage,
		    gint32     layer_ID)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (gint32),
                        FS_RELAX_UNDO, FALSE)))
    {
      new->data      = g_new (gint32, 1);
      new->pop_func  = undo_pop_fs_relax;
      new->free_func = undo_free_fs_relax;

      *((gint32 *) new->data) = layer_ID;

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
undo_pop_fs_relax (GimpImage *gimage,
		   UndoState  state,
		   UndoType   type,
		   gpointer   layer_ptr)
{
  gint32    layer_ID;
  GimpLayer *floating_layer;

  layer_ID = *((gint32 *) layer_ptr);

  if ((floating_layer = (GimpLayer *) gimp_item_get_by_ID (gimage->gimp, layer_ID)) == NULL)
    return FALSE;

  if (! gimp_layer_is_floating_sel (floating_layer))
    return FALSE;

  switch (state)
    {
    case UNDO:
      /*  store the affected area from the drawable in the backing store  */
      floating_sel_store (floating_layer,
			  GIMP_DRAWABLE (floating_layer)->offset_x, 
			  GIMP_DRAWABLE (floating_layer)->offset_y,
			  GIMP_DRAWABLE (floating_layer)->width, 
			  GIMP_DRAWABLE (floating_layer)->height);
      floating_layer->fs.initial = TRUE;
      break;

    case REDO:
      /*  restore the contents of drawable the floating layer is attached to  */
      if (floating_layer->fs.initial == FALSE)
	floating_sel_restore (floating_layer,
			      GIMP_DRAWABLE (floating_layer)->offset_x, 
			      GIMP_DRAWABLE (floating_layer)->offset_y,
			      GIMP_DRAWABLE (floating_layer)->width, 
			      GIMP_DRAWABLE (floating_layer)->height);
      floating_layer->fs.initial = TRUE;
      break;
    }

  gimp_image_floating_selection_changed (gimage);

  return TRUE;
}

static void
undo_free_fs_relax (UndoState  state,
		    UndoType   type,
		    gpointer   layer_ptr)
{
  g_free (layer_ptr);
}


/********************/
/*  Transform Undo  */

typedef struct _TransformUndo TransformUndo;

struct _TransformUndo
{
  gint         tool_ID;
  GType        tool_type;

  TransInfo    trans_info;
  TileManager *original;
  gpointer     path_undo;
};

static gboolean undo_pop_transform  (GimpImage *,
                                     UndoState, UndoType, gpointer);
static void     undo_free_transform (UndoState, UndoType, gpointer);

gboolean
undo_push_transform (GimpImage   *gimage,
                     gint         tool_ID,
                     GType        tool_type,
                     gdouble     *trans_info,
                     TileManager *original,
                     GSList      *path_undo)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (TransformUndo),
                        TRANSFORM_UNDO, FALSE)))
    {
      TransformUndo *tu;
      gint           i;

      tu = g_new0 (TransformUndo, 1);

      new->data      = tu;
      new->pop_func  = undo_pop_transform;
      new->free_func = undo_free_transform;

      tu->tool_ID   = tool_ID;
      tu->tool_type = tool_type;

      for (i = 0; i < TRAN_INFO_SIZE; i++)
	tu->trans_info[i] = trans_info[i];

      tu->original  = original;
      tu->path_undo = path_undo;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_transform (GimpImage *gimage,
		    UndoState  state,
		    UndoType   type,
		    gpointer   tu_ptr)
{
  GimpTool *active_tool;

  active_tool = tool_manager_get_active (gimage->gimp);

  if (GIMP_IS_TRANSFORM_TOOL (active_tool))
    {
      GimpTransformTool *tt;
      TransformUndo     *tu;

      tt = GIMP_TRANSFORM_TOOL (active_tool);
      tu = (TransformUndo *) tu_ptr;

      path_transform_do_undo (gimage, tu->path_undo);

      /*  only pop if the active tool is the tool that pushed this undo  */
      if (tu->tool_ID == active_tool->ID)
        {
          TileManager *temp;
          gdouble      d;
          gint         i;

          /*  swap the transformation information arrays  */
          for (i = 0; i < TRAN_INFO_SIZE; i++)
            {
              d                 = tu->trans_info[i];
              tu->trans_info[i] = tt->trans_info[i];
              tt->trans_info[i] = d;
            }

          /*  swap the original buffer--the source buffer for repeated transforms
           */
          temp         = tu->original;
          tu->original = tt->original;
          tt->original = temp;

          /*  If we're re-implementing the first transform, reactivate tool  */
          if (state == REDO && tt->original)
            {
              gimp_tool_control_activate(active_tool->control);

              gimp_draw_tool_resume (GIMP_DRAW_TOOL (tt));
            }
        }
    }

  return TRUE;
}

static void
undo_free_transform (UndoState   state,
		     UndoType    type,
		     gpointer    tu_ptr)
{
  TransformUndo * tu;

  tu = (TransformUndo *) tu_ptr;
  if (tu->original)
    tile_manager_destroy (tu->original);
  path_transform_free_undo (tu->path_undo);
  g_free (tu);
}


/****************/
/*  Paint Undo  */

typedef struct _PaintUndo PaintUndo;

struct _PaintUndo
{
  gint        core_ID;
  GType       core_type;

  GimpCoords  last_coords;
};

static gboolean undo_pop_paint  (GimpImage *,
                                 UndoState, UndoType, gpointer);
static void     undo_free_paint (UndoState, UndoType, gpointer);

gboolean
undo_push_paint (GimpImage  *gimage,
                 gint        core_ID,
                 GType       core_type,
                 GimpCoords *last_coords)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (PaintUndo),
                        PAINT_UNDO, FALSE)))
    {
      PaintUndo *pu;

      pu = g_new0 (PaintUndo, 1);

      new->data      = pu;
      new->pop_func  = undo_pop_paint;
      new->free_func = undo_free_paint;

      pu->core_ID     = core_ID;
      pu->core_type   = core_type;
      pu->last_coords = *last_coords;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_paint (GimpImage *gimage,
		UndoState  state,
		UndoType   type,
		gpointer   pu_ptr)
{
  GimpTool *active_tool;

  active_tool = tool_manager_get_active (gimage->gimp);

  if (GIMP_IS_PAINT_TOOL (active_tool))
    {
      GimpPaintTool *pt;
      PaintUndo     *pu;

      pt = GIMP_PAINT_TOOL (active_tool);
      pu = (PaintUndo *) pu_ptr;

      /*  only pop if the active paint core is the one that pushed this undo  */
      if (pu->core_ID == pt->core->ID)
        {
          GimpCoords tmp_coords;

          /*  swap the paint core information  */
          tmp_coords            = pt->core->last_coords;
          pt->core->last_coords = pu->last_coords;
          pu->last_coords       = tmp_coords;
        }
    }

  return TRUE;
}

static void
undo_free_paint (UndoState  state,
		 UndoType   type,
		 gpointer   pu_ptr)
{
  PaintUndo *pu;

  pu = (PaintUndo *) pu_ptr;
  g_free (pu);
}


/*******************/
/*  Parasite Undo  */

typedef struct _ParasiteUndo ParasiteUndo;

struct _ParasiteUndo
{
  GimpImage    *gimage;
  GimpItem     *item;
  GimpParasite *parasite;
  gchar        *name;
};

static gboolean undo_pop_parasite  (GimpImage *,
                                    UndoState, UndoType, gpointer);
static void     undo_free_parasite (UndoState, UndoType, gpointer);

gboolean
undo_push_image_parasite (GimpImage *gimage,
			  gpointer   parasite)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ParasiteUndo),
                        PARASITE_ATTACH_UNDO, TRUE)))
    {
      ParasiteUndo *pu;

      pu = g_new0 (ParasiteUndo, 1);

      new->data      = pu;
      new->pop_func  = undo_pop_parasite;
      new->free_func = undo_free_parasite;

      pu->gimage   = gimage;
      pu->item     = NULL;
      pu->name     = g_strdup (gimp_parasite_name (parasite));
      pu->parasite = gimp_parasite_copy (gimp_image_parasite_find (gimage,
                                                                   pu->name));

      return TRUE;
    }

  return FALSE;
}

gboolean
undo_push_image_parasite_remove (GimpImage   *gimage,
				 const gchar *name)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ParasiteUndo),
                        PARASITE_REMOVE_UNDO, TRUE)))
    {
      ParasiteUndo *pu;

      pu = g_new0 (ParasiteUndo, 1);

      new->data      = pu;
      new->pop_func  = undo_pop_parasite;
      new->free_func = undo_free_parasite;

      pu->gimage   = gimage;
      pu->item     = NULL;
      pu->name     = g_strdup (name);
      pu->parasite = gimp_parasite_copy (gimp_image_parasite_find (gimage,
                                                                   pu->name));

      return TRUE;
    }

  return FALSE;
}

gboolean
undo_push_item_parasite (GimpImage *gimage,
                         GimpItem  *item,
                         gpointer   parasite)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ParasiteUndo),
                        PARASITE_ATTACH_UNDO, TRUE)))
    {
      ParasiteUndo *pu;

      pu = g_new0 (ParasiteUndo, 1);

      new->data      = pu;
      new->pop_func  = undo_pop_parasite;
      new->free_func = undo_free_parasite;

      pu->gimage   = NULL;
      pu->item     = item;
      pu->name     = g_strdup (gimp_parasite_name (parasite));
      pu->parasite = gimp_parasite_copy (gimp_item_parasite_find (item,
                                                                  pu->name));
      return TRUE;
    }

  return FALSE;
}

gboolean
undo_push_item_parasite_remove (GimpImage   *gimage,
                                GimpItem    *item,
                                const gchar *name)
{
  Undo *new;

  if ((new = undo_push (gimage, sizeof (ParasiteUndo),
                        PARASITE_REMOVE_UNDO, TRUE)))
    {
      ParasiteUndo *pu;

      pu = g_new0 (ParasiteUndo, 1);

      new->data      = pu;
      new->pop_func  = undo_pop_parasite;
      new->free_func = undo_free_parasite;

      pu->gimage   = NULL;
      pu->item     = item;
      pu->name     = g_strdup (name);
      pu->parasite = gimp_parasite_copy (gimp_item_parasite_find (item,
                                                                  pu->name));
      return TRUE;
    }

  return FALSE;
}


static gboolean
undo_pop_parasite (GimpImage *gimage,
		   UndoState  state,
		   UndoType   type,
		   gpointer   data_ptr)
{
  ParasiteUndo *data;
  GimpParasite *tmp;

  data = data_ptr;

  tmp = data->parasite;
  
  if (data->gimage)
    {
      data->parasite =
	gimp_parasite_copy (gimp_image_parasite_find (gimage, data->name));

      if (tmp)
	gimp_parasite_list_add (data->gimage->parasites, tmp);
      else
	gimp_parasite_list_remove (data->gimage->parasites, data->name);
    }
  else if (data->item)
    {
      data->parasite =
	gimp_parasite_copy (gimp_item_parasite_find (data->item,
                                                     data->name));
      if (tmp)
	gimp_parasite_list_add (data->item->parasites, tmp);
      else
	gimp_parasite_list_remove (data->item->parasites, data->name);
    }
  else
    {
      data->parasite = gimp_parasite_copy (gimp_parasite_find (gimage->gimp,
							       data->name));
      if (tmp)
	gimp_parasite_attach (gimage->gimp, tmp);
      else
	gimp_parasite_detach (gimage->gimp, data->name);
    }
    
  if (tmp)
    gimp_parasite_free (tmp);

  return TRUE;
}


static void
undo_free_parasite (UndoState  state,
		    UndoType   type,
		    gpointer   data_ptr)
{
  ParasiteUndo *data;

  data = data_ptr;

  if (data->parasite)
    gimp_parasite_free (data->parasite);
  if (data->name)
    g_free (data->name);

  g_free (data_ptr);
}


/******************************************************************************/
/*  Something for which programmer is too lazy to write an undo function for  */

static gboolean undo_pop_cantundo  (GimpImage *,
                                    UndoState, UndoType, gpointer);
static void     undo_free_cantundo (UndoState, UndoType, gpointer);

gboolean
undo_push_cantundo (GimpImage   *gimage,
		    const gchar *action)
{
  Undo *new;

  /* This is the sole purpose of this type of undo: the ability to
   * mark an image as having been mutated, without really providing
   * any adequate undo facility.
   */

  if ((new = undo_push (gimage, 0, CANT_UNDO, TRUE)))
    {
      new->data      = (gpointer) action;
      new->pop_func  = undo_pop_cantundo;
      new->free_func = undo_free_cantundo;

      return TRUE;
    }

  return FALSE;
}

static gboolean
undo_pop_cantundo (GimpImage *gimage,
		   UndoState  state,
		   UndoType   type,
		   gpointer   data_ptr)
{
  gchar *action = data_ptr;

  switch (state)
    {
    case UNDO:
      g_message (_("Can't undo %s"), action);
      break;

    case REDO:
      break;
    }

  return TRUE;
}

static void
undo_free_cantundo (UndoState  state,
		    UndoType   type,
		    gpointer   data_ptr)
{
  /* nothing to free */
}


static struct undo_name_t
{
  UndoType     type;
  const gchar *name;
}
undo_name[] =
{
  { NO_UNDO_GROUP,                 N_("<<invalid>>")               },
  { IMAGE_SCALE_UNDO_GROUP,        N_("Scale Image")               },
  { IMAGE_RESIZE_UNDO_GROUP,       N_("Resize Image")              },
  { IMAGE_CONVERT_UNDO_GROUP,      N_("Convert Image")             },
  { IMAGE_CROP_UNDO_GROUP,         N_("Crop Image")                },
  { IMAGE_LAYERS_MERGE_UNDO_GROUP, N_("Merge Layers")              },
  { IMAGE_QMASK_UNDO_GROUP,        N_("QuickMask")                 },
  { IMAGE_GUIDE_UNDO_GROUP,        N_("Guide")                     },
  { LAYER_PROPERTIES_UNDO_GROUP,   N_("Layer Properties")          },
  { LAYER_SCALE_UNDO_GROUP,        N_("Scale Layer")               },
  { LAYER_RESIZE_UNDO_GROUP,       N_("Resize Layer")              },
  { LAYER_DISPLACE_UNDO_GROUP,     N_("Move Layer")                },
  { LAYER_APPLY_MASK_UNDO_GROUP,   N_("Apply Layer Mask")          },
  { LAYER_LINKED_UNDO_GROUP,       N_("Linked Layer")              },
  { FS_FLOAT_UNDO_GROUP,           N_("Float Selection")           },
  { FS_ANCHOR_UNDO_GROUP,          N_("Anchor Floating Selection") },
  { EDIT_PASTE_UNDO_GROUP,         N_("Paste")                     },
  { EDIT_CUT_UNDO_GROUP,           N_("Cut")                       },
  { EDIT_COPY_UNDO_GROUP,          N_("Copy")                      },
  { TEXT_UNDO_GROUP,               N_("Text")                      },
  { TRANSFORM_UNDO_GROUP,          N_("Transform")                 },
  { PAINT_UNDO_GROUP,              N_("Paint")                     },
  { PARASITE_ATTACH_UNDO_GROUP,    N_("Attach Parasite")           },
  { PARASITE_REMOVE_UNDO_GROUP,    N_("Remove Parasite")           },
  { MISC_UNDO_GROUP,               N_("Plug-In")                   },

  { IMAGE_UNDO,	                   N_("Image")                     },
  { IMAGE_MOD_UNDO,                N_("Image Mod")                 },
  { IMAGE_TYPE_UNDO,               N_("Image Type")                },
  { IMAGE_SIZE_UNDO,               N_("Image Size")                },
  { IMAGE_RESOLUTION_UNDO,         N_("Resolution Change")         },
  { IMAGE_QMASK_UNDO,              N_("QuickMask")                 },
  { IMAGE_GUIDE_UNDO,              N_("Guide")                     },
  { MASK_UNDO,                     N_("Selection Mask")            },
  { ITEM_RENAME_UNDO,              N_("Rename Item")               },
  { LAYER_ADD_UNDO,                N_("New Layer")                 },
  { LAYER_REMOVE_UNDO,             N_("Delete Layer")              },
  { LAYER_MOD_UNDO,                N_("Layer Mod")                 },
  { LAYER_MASK_ADD_UNDO,           N_("Add Layer Mask")            },
  { LAYER_MASK_REMOVE_UNDO,        N_("Delete Layer Mask")         },
  { LAYER_REPOSITION_UNDO,         N_("Layer Reposition")          },
  { LAYER_DISPLACE_UNDO,           N_("Layer Move")                },
  { CHANNEL_ADD_UNDO,              N_("New Channel")               },
  { CHANNEL_REMOVE_UNDO,           N_("Delete Channel")            },
  { CHANNEL_MOD_UNDO,              N_("Channel Mod")               },
  { CHANNEL_REPOSITION_UNDO,       N_("Channel Reposition")        },
  { VECTORS_ADD_UNDO,              N_("New Vectors")               },
  { VECTORS_REMOVE_UNDO,           N_("Delete Vectors")            },
  { VECTORS_MOD_UNDO,              N_("Vectors Mod")               },
  { VECTORS_REPOSITION_UNDO,       N_("Vectors Reposition")        },
  { FS_TO_LAYER_UNDO,              N_("FS to Layer")               },
  { FS_RIGOR_UNDO,                 N_("FS Rigor")                  },
  { FS_RELAX_UNDO,                 N_("FS Relax")                  },
  { TRANSFORM_UNDO,                N_("Transform")                 },
  { PAINT_UNDO,		           N_("Paint")                     },
  { PARASITE_ATTACH_UNDO,          N_("Attach Parasite")           },
  { PARASITE_REMOVE_UNDO,          N_("Remove Parasite")           },

  { CANT_UNDO,                     "EEK: can't undo"               }
};

static const gchar *
undo_type_to_name (UndoType type)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (undo_name); i++)
    if (undo_name[i].type == type)
      return gettext (undo_name[i].name);

  /* no name found */
  return "";
}
