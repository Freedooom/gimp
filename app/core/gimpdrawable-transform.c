/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2001 Spencer Kimball, Peter Mattis, and others
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

/* FIXME */
#define TRANSFORM_CORRECTIVE 1 


#include "config.h"

#include <stdlib.h>

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "apptypes.h"

#include "cursorutil.h"
#include "drawable.h"
#include "floating_sel.h"
#include "gdisplay.h"
#include "gimage_mask.h"
#include "gimpchannel.h"
#include "gimpimage.h"
#include "gimplayer.h"
#include "gimpmarshal.h"
#include "gimprc.h"
#include "gimpui.h"
#include "info_dialog.h"
#include "path_transform.h"
#include "paint_funcs.h"
#include "pixel_region.h"
#include "undo.h"
#include "tile_manager.h"
#include "tile.h"

#include "tools/gimptool.h"
#include "tools/tool_manager.h"
#include "tools/gimpdrawtool.h"
#include "tools/gimptransformtool.h"
/*#include "transform_tool.h"*/

#include "libgimp/gimpintl.h"

enum
{
  TRANSFORM,
  LAST_SIGNAL
};

/* This should be migrated to pixel_region or similar... */
/* PixelSurround describes a (read-only)
 *  region around a pixel in a tile manager
 */

typedef struct _PixelSurround
{
  Tile        *tile;
  TileManager *mgr;
  guchar      *buff;
  gint         buff_size;
  gint         bpp;
  gint         w;
  gint         h;
  guchar       bg[MAX_CHANNELS];
  gint         row_stride;
} PixelSurround;

#define BILINEAR(jk,j1k,jk1,j1k1,dx,dy) \
                ((1-dy) * (jk + dx * (j1k - jk)) + \
		    dy  * (jk1 + dx * (j1k1 - jk1)))

/* access interleaved pixels */
#define CUBIC_ROW(dx, row, step) \
  gimp_transform_tool_cubic(dx, (row)[0], (row)[step], (row)[step+step], (row)[step+step+step])
#define CUBIC_SCALED_ROW(dx, row, step, i) \
  gimp_transform_tool_cubic(dx, (row)[0] * (row)[i], \
            (row)[step] * (row)[step + i], \
            (row)[step+step]* (row)[step+step + i], \
            (row)[step+step+step] * (row)[step+step+step + i])

#define REF_TILE(i,x,y) \
     tile[i] = tile_manager_get_tile (float_tiles, x, y, TRUE, FALSE); \
     src[i] = tile_data_pointer (tile[i], (x) % TILE_WIDTH, (y) % TILE_HEIGHT);

/*  forward function declarations  */
static void      gimp_transform_tool_bounds    (GimpTransformTool      *tool,
                                                GDisplay               *gdisp);
static void      gimp_transform_tool_recalc    (GimpTransformTool      *tool,
					        GDisplay               *gdisp);
static void      gimp_transform_tool_doit      (GimpTransformTool      *tool,
					        GDisplay               *gdisp);
static gdouble   gimp_transform_tool_cubic     (gdouble                 dx,
					        gint                    jm1,
					        gint                    j,
					        gint                    jp1,
					        gint                    jp2);
static void    gimp_transform_tool_setup_grid  (GimpTransformTool      *tool);
static void    gimp_transform_tool_grid_recalc (GimpTransformTool      *gimp_transform_tool);
static void    gimp_transform_tool_init        (GimpTransformTool      *tool);
static void    gimp_transform_tool_class_init  (GimpTransformToolClass *tool);

void          gimp_transform_tool_button_press (GimpTool               *tool,
                                                GdkEventButton         *bevent,
			                        GDisplay               *gdisp);
			          
void        gimp_transform_tool_button_release (GimpTool               *tool,
			                        GdkEventButton         *bevent,
			                        GDisplay               *gdisp);
			                        
void           gimp_transform_tool_motion      (GimpTool               *tool,
		                                GdkEventMotion         *mevent,
		                                GDisplay               *gdisp);
		                                
void         gimp_transform_tool_cursor_update (GimpTool               *tool,
	                 		        GdkEventMotion         *mevent,
			                        GDisplay               *gdisp);
			                        
void           gimp_transform_tool_control     (GimpTool               *tool,
			                        ToolAction              action,
			                        GDisplay               *gdisp);

/*  variables  */
static TranInfo           old_trans_info;
InfoDialog               *transform_info        = NULL;
static gboolean           transform_info_inited = FALSE;
static GimpDrawToolClass *parent_class          = NULL;

static guint gimp_transform_tool_signals[LAST_SIGNAL] = { 0 };

GtkType
gimp_transform_tool_get_type (void)
{
  static GtkType tool_type = 0;

  if (! tool_type)
    {
      GtkTypeInfo tool_info =
      {
        "GimpTransformTool",
        sizeof (GimpTransformTool),
        sizeof (GimpTransformToolClass),
        (GtkClassInitFunc) gimp_transform_tool_class_init,
        (GtkObjectInitFunc) gimp_transform_tool_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        NULL /* (GtkClassInitFunc) gimp_tool_class_init, */
      };

      tool_type = gtk_type_unique (GIMP_TYPE_DRAW_TOOL, &tool_info);
    }

  return tool_type;
}


static void
gimp_transform_tool_class_init (GimpTransformToolClass *klass)
{
  GtkObjectClass    *object_class;
  GimpToolClass     *tool_class;
  GimpDrawToolClass *draw_class;

  object_class = (GtkObjectClass *) klass;
  tool_class   = (GimpToolClass *) klass;
  draw_class   = (GimpDrawToolClass *) klass;

  parent_class = gtk_type_class (GIMP_TYPE_DRAW_TOOL);

  gimp_transform_tool_signals[TRANSFORM] =
    gtk_signal_new ("transform",
    		    GTK_RUN_FIRST,
    		    object_class->type,
    		    GTK_SIGNAL_OFFSET (GimpTransformToolClass,
    		    		       transform),
    		    gimp_marshal_POINTER__POINTER_INT,
    		    GTK_TYPE_POINTER, 2,
    		    GTK_TYPE_POINTER,
    		    GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, gimp_transform_tool_signals,
				LAST_SIGNAL);

  object_class->destroy      = gimp_transform_tool_destroy;

  tool_class->button_press   = gimp_transform_tool_button_press;
  tool_class->button_release = gimp_transform_tool_button_release;
  tool_class->motion         = gimp_transform_tool_motion;
  tool_class->cursor_update  = gimp_transform_tool_cursor_update;
  tool_class->control        = gimp_transform_tool_control;

  /* FIXME if (interactive) */
  draw_class->draw           = gimp_transform_tool_draw;
  /* else
    private->core = gimp_draw_tool_new (gimp_transform_tool_no_draw); */


}

static void
gimp_transform_tool_init (GimpTransformTool *tr_tool)
{
  GimpTool      *tool = GIMP_TOOL(tr_tool);
  gint           i;

  tr_tool->function = TRANSFORM_CREATING;
  tr_tool->original = NULL;

  tr_tool->bpressed = FALSE;

  for (i = 0; i < TRAN_INFO_SIZE; i++)
    tr_tool->trans_info[i] = 0;

  tr_tool->grid_coords = tr_tool->tgrid_coords = NULL;

  tool->scroll_lock = TRUE;   /*  Disallow scrolling  */
  tool->preserve    = FALSE;  /*  Don't preserve on drawable change  */
}

static void
pixel_surround_init (PixelSurround *ps,
		     TileManager   *tm,
		     gint           w,
		     gint           h,
		     guchar         bg[MAX_CHANNELS])
{
  gint i;

  for (i = 0; i < MAX_CHANNELS; ++i)
    {
      ps->bg[i] = bg[i];
    }

  ps->tile       = NULL;
  ps->mgr        = tm;
  ps->bpp        = tile_manager_bpp (tm);
  ps->w          = w;
  ps->h          = h;
  /* make sure buffer is big enough */
  ps->buff_size  = w * h * ps->bpp;
  ps->buff       = g_malloc (ps->buff_size);
  ps->row_stride = 0;
}

/* return a pointer to a buffer which contains all the surrounding pixels */
/* strategy: if we are in the middle of a tile, use the tile storage */
/* otherwise just copy into our own malloced buffer and return that */

static guchar *
pixel_surround_lock (PixelSurround *ps,
		     gint           x,
		     gint           y)
{
  gint    i, j;
  guchar *k;
  guchar *ptr;

  ps->tile = tile_manager_get_tile (ps->mgr, x, y, TRUE, FALSE);

  i = x % TILE_WIDTH;
  j = y % TILE_HEIGHT;

  /* do we have the whole region? */
  if (ps->tile &&
      (i < (tile_ewidth(ps->tile) - ps->w)) &&
      (j < (tile_eheight(ps->tile) - ps->h)))
    {
      ps->row_stride = tile_ewidth (ps->tile) * ps->bpp;
      /* is this really the correct way? */
      return tile_data_pointer (ps->tile, i, j);
    }

  /* nope, do this the hard way (for now) */
  if (ps->tile)
    {
      tile_release (ps->tile, FALSE);
      ps->tile = 0;
    }

  /* copy pixels, one by one */
  /* no, this is not the best way, but it's much better than before */
  ptr = ps->buff;
  for (j = y; j < y+ps->h; ++j)
    {
      for (i = x; i < x+ps->w; ++i)
	{
	  Tile *tile = tile_manager_get_tile (ps->mgr, i, j, TRUE, FALSE);

	  if (tile)
	    {
	      guchar *buff = tile_data_pointer (tile,
						i % TILE_WIDTH,
						j % TILE_HEIGHT);

	      for (k = buff; k < buff+ps->bpp; ++k, ++ptr)
		{
		  *ptr = *k;
		}
	      tile_release (tile, FALSE);
	    }
	  else
	    {
	      for (k = ps->bg; k < ps->bg+ps->bpp; ++k, ++ptr)
		{
		  *ptr = *k;
		}
	    }
	}
    }
  ps->row_stride = ps->w * ps->bpp;

  return ps->buff;
}

static gint
pixel_surround_rowstride (PixelSurround *ps)
{
  return ps->row_stride;
}

static void
pixel_surround_release (PixelSurround *ps)
{
  /* always get new tile (for now), so release the old one */
  if (ps->tile)
    {
      tile_release (ps->tile, FALSE);
      ps->tile = 0;
    }
}

static void
pixel_surround_clear (PixelSurround *ps)
{
  if (ps->buff)
    {
      g_free (ps->buff);
      ps->buff = 0;
      ps->buff_size = 0;
    }
}

static void
transform_ok_callback (GtkWidget *widget,
		       gpointer   data)
{
  GimpTool *tool;

  tool = GIMP_TOOL(data);
  gimp_transform_tool_doit (GIMP_TRANSFORM_TOOL(tool), tool->gdisp);
}

static void
transform_reset_callback (GtkWidget *widget,
			  gpointer   data)
{
  GimpTransformTool   *tool;
  GimpDrawTool        *dr_tool;
  gint                 i;

  tool    = GIMP_TRANSFORM_TOOL(data);
  dr_tool = GIMP_DRAW_TOOL(data);

  /*  stop the current tool drawing process  */
  gimp_draw_tool_pause (dr_tool);

  /*  Restore the previous transformation info  */
  for (i = 0; i < TRAN_INFO_SIZE; i++)
    tool->trans_info [i] = old_trans_info [i];

  /*  recalculate the tool's transformation matrix  */
  gimp_transform_tool_recalc (tool, GIMP_TOOL(tool)->gdisp);

  /*  resume drawing the current tool  */
  gimp_draw_tool_resume (dr_tool);
}

static const gchar *action_labels[] =
{
  N_("Rotate"),
  N_("Scale"),
  N_("Shear"),
  N_("Transform")
};

void
gimp_transform_tool_button_press (GimpTool           *tool,
                                  GdkEventButton     *bevent,
			          GDisplay           *gdisp)
{
  GimpTransformTool  *gt_tool;
  GimpDrawable       *drawable;
  gint                dist;
  gint                closest_dist;
  gint                x, y;
  gint                i;
  gint                off_x, off_y;

  gt_tool = GIMP_TRANSFORM_TOOL(tool);

  gt_tool->bpressed = TRUE; /* ALT */

  drawable = gimp_image_active_drawable (gdisp->gimage);

  if (gt_tool->function == TRANSFORM_CREATING && tool->state == ACTIVE)
    {
      /*  Save the current transformation info  */
      for (i = 0; i < TRAN_INFO_SIZE; i++)
	old_trans_info [i] = gt_tool->trans_info [i];
    }

  /*  if we have already displayed the bounding box and handles,
   *  check to make sure that the display which currently owns the
   *  tool is the one which just received the button pressed event
   */
  if ((gdisp == tool->gdisp) && gt_tool->interactive)
    {
      /*  start drawing the bounding box and handles...  */
      gimp_draw_tool_start (GIMP_DRAW_TOOL(gt_tool), gdisp->canvas->window);
	
      x = bevent->x;
      y = bevent->y;

      closest_dist = SQR (x - gt_tool->sx1) + SQR (y - gt_tool->sy1);
      gt_tool->function = TRANSFORM_HANDLE_1;

      dist = SQR (x - gt_tool->sx2) + SQR (y - gt_tool->sy2);
      if (dist < closest_dist)
	{
	  closest_dist = dist;
	  gt_tool->function = TRANSFORM_HANDLE_2;
	}

      dist = SQR (x - gt_tool->sx3) + SQR (y - gt_tool->sy3);
      if (dist < closest_dist)
	{
	  closest_dist = dist;
	  gt_tool->function = TRANSFORM_HANDLE_3;
	}

      dist = SQR (x - gt_tool->sx4) + SQR (y - gt_tool->sy4);
      if (dist < closest_dist)
	{
	  closest_dist = dist;
	  gt_tool->function = TRANSFORM_HANDLE_4;
	}

#ifdef FIXME_I_DO_NOT_BELONG_HERE
      if (tool->type == ROTATE
	  && (SQR (x - gt_tool->scx) +
	      SQR (y - gt_tool->scy)) <= 100)
	{
	  gt_tool->function = TRANSFORM_HANDLE_CENTER;
	}
#endif

      /*  Save the current pointer position  */
      gdisplay_untransform_coords (gdisp, bevent->x, bevent->y,
				   &gt_tool->startx,
				   &gt_tool->starty, TRUE, 0);
      gt_tool->lastx = gt_tool->startx;
      gt_tool->lasty = gt_tool->starty;

      gdk_pointer_grab (gdisp->canvas->window, FALSE,
			GDK_POINTER_MOTION_HINT_MASK |
			GDK_BUTTON1_MOTION_MASK |
			GDK_BUTTON_RELEASE_MASK,
			NULL, NULL, bevent->time);

      tool->state = ACTIVE;
      return;
    }


  /*  Initialisation stuff: if the cursor is clicked inside the current
   *  selection, show the bounding box and handles...
   */
  gdisplay_untransform_coords (gdisp, bevent->x, bevent->y, &x, &y,
			       FALSE, FALSE);

  gimp_drawable_offsets (drawable, &off_x, &off_y);
  if (x >= off_x && y >= off_y &&
      x < (off_x + gimp_drawable_width (drawable)) &&
      y < (off_y + gimp_drawable_height (drawable)))
    if (gimage_mask_is_empty (gdisp->gimage) ||
	gimage_mask_value (gdisp->gimage, x, y))
      {
	if (GIMP_IS_LAYER (drawable) &&
	    gimp_layer_get_mask (GIMP_LAYER (drawable)))
	  {
	    g_message (_("Transformations do not work on\n"
			 "layers that contain layer masks."));
	    tool->state = INACTIVE;
	    return;
	  }

	/*  If the tool is already active, clear the current state
	 *  and reset
	 */
	if (tool->state == ACTIVE)
	  gimp_transform_tool_reset (gt_tool, gdisp);

	/*  Set the pointer to the active display  */
	tool->gdisp    = gdisp;
	tool->drawable = drawable;
	tool->state    = ACTIVE;

	/*  Grab the pointer if we're in non-interactive mode  */
	if (!gt_tool->interactive)
	  gdk_pointer_grab (gdisp->canvas->window, FALSE,
			    (GDK_POINTER_MOTION_HINT_MASK |
			     GDK_BUTTON1_MOTION_MASK |
			     GDK_BUTTON_RELEASE_MASK),
			    NULL, NULL, bevent->time);

	/*  Find the transform bounds for some tools (like scale,
	 *  perspective) that actually need the bounds for
	 *  initializing
	 */
	gimp_transform_tool_bounds (gt_tool, gdisp);

	/*  Calculate the grid line endpoints  */
	if (gimp_transform_tool_show_grid ())
	  gimp_transform_tool_setup_grid (tool);

	/*  Initialize the transform tool */
	gimp_transform_tool_transform (gt_tool, gdisp, TRANSFORM_INIT);

	if (transform_info && !transform_info_inited)
	  {
	    gimp_dialog_create_action_area
	      (GTK_DIALOG (transform_info->shell),

	       /* FIXME!!! gettext (action_labels[tool->type - ROTATE])*/ "I need to be fixed badly.",
	       transform_ok_callback,
	       tool, NULL, NULL, TRUE, FALSE,
	       _("Reset"), transform_reset_callback,
	       tool, NULL, NULL, FALSE, FALSE,

	       NULL);

	    transform_info_inited = TRUE;
	  }

	/*  Recalculate the transform tool  */
	gimp_transform_tool_recalc (gt_tool, gdisp);

	/*  recall this function to find which handle we're dragging  */
	if (gt_tool->interactive)
	  gimp_transform_tool_button_press (gt_tool, bevent, gdisp);
      }
}

void
gimp_transform_tool_button_release (GimpTool         *tool,
			            GdkEventButton   *bevent,
			            GDisplay         *gdisp)
{
  GimpTransformTool *gt_tool;
  gint               i;

  gt_tool = GIMP_TRANSFORM_TOOL(tool);

  gt_tool->bpressed = FALSE; /* ALT */

  /*  if we are creating, there is nothing to be done...exit  */
  if (gt_tool->function == TRANSFORM_CREATING &&
      gt_tool->interactive)
    return;

  /*  release of the pointer grab  */
  gdk_pointer_ungrab (bevent->time);
  gdk_flush ();

  /*  if the 3rd button isn't pressed, transform the selected mask  */
  if (! (bevent->state & GDK_BUTTON3_MASK))
    {
      /* Shift-clicking is another way to approve the transform  */
      if ((bevent->state & GDK_SHIFT_MASK) /* FIXME || (tool->type == FLIP) */)
	{
	  gimp_transform_tool_doit (gt_tool, gdisp);
	}
      else
	{
	  /*  Only update the paths preview */
	  path_transform_current_path (gdisp->gimage,
				       gt_tool->transform, TRUE);
	}
    }
  else
    {
      /*  stop the current tool drawing process  */
      gimp_draw_tool_pause (GIMP_DRAW_TOOL(gt_tool));

      /*  Restore the previous transformation info  */
      for (i = 0; i < TRAN_INFO_SIZE; i++)
	gt_tool->trans_info [i] = old_trans_info [i];

      /*  recalculate the tool's transformation matrix  */
      gimp_transform_tool_recalc (gt_tool, gdisp);

      /*  resume drawing the current tool  */
      gimp_draw_tool_resume (GIMP_DRAW_TOOL(gt_tool));

      /* Update the paths preview */
      path_transform_current_path (gdisp->gimage,
				   gt_tool->transform, TRUE);
    }

  /*  if this tool is non-interactive, make it inactive after use  */
  if (!gt_tool->interactive)
    tool->state = INACTIVE;
}

void
gimp_transform_tool_doit (GimpTransformTool  *gt_tool,
		          GDisplay           *gdisp)
{
  GimpTool      *tool;
  TileManager   *new_tiles;
  TransformUndo *tu;
  PathUndo      *pundo;
  gboolean       new_layer;
  gint           i, x, y;

  gimp_add_busy_cursors ();

  tool = GIMP_TOOL(gt_tool);

  /* undraw the tool before we muck around with the transform matrix */
  gimp_draw_tool_pause (GIMP_DRAW_TOOL(gt_tool));

  /*  We're going to dirty this image, but we want to keep the tool
   *  around
   */
  tool->preserve = TRUE;

  /*  Start a transform undo group  */
  undo_push_group_start (gdisp->gimage, TRANSFORM_CORE_UNDO);

  /*  With the old UI, if original is NULL, then this is the
      first transformation. In the new UI, it is always so, right?  */
  g_assert (gt_tool->original == NULL);

  /* If we're in interactive mode, we need to copy the current
   *  selection to the transform tool's private selection pointer, so
   *  that the original source can be repeatedly modified.
   */
  tool->drawable = gimp_image_active_drawable (gdisp->gimage);

  gt_tool->original = gimp_transform_tool_cut (gdisp->gimage,
					       tool->drawable,
					       &new_layer);

  pundo = path_transform_start_undo (gdisp->gimage);

  /*  Send the request for the transformation to the tool...
   */
  new_tiles = gimp_transform_tool_transform (gt_tool, gdisp,
					      TRANSFORM_FINISH);

  gimp_transform_tool_transform (gt_tool, gdisp, TRANSFORM_INIT);

  gimp_transform_tool_recalc (gt_tool, gdisp);

  if (new_tiles)
    {
      /*  paste the new transformed image to the gimage...also implement
       *  undo...
       */
      /*  FIXME: we should check if the drawable is still valid  */
      gimp_transform_tool_paste (gdisp->gimage, tool->drawable,
			    new_tiles, new_layer);

      /*  create and initialize the transform_undo structure  */
      tu = g_new (TransformUndo, 1);
      tu->tool = gt_tool;

      for (i = 0; i < TRAN_INFO_SIZE; i++)
	tu->trans_info[i] = old_trans_info[i];
      tu->original = NULL;
      tu->path_undo = pundo;

      /*  Make a note of the new current drawable (since we may have
       *  a floating selection, etc now.
       */
      tool->drawable = gimp_image_active_drawable (gdisp->gimage);

      undo_push_transform (gdisp->gimage, (void *) tu);
    }

  /*  push the undo group end  */
  undo_push_group_end (gdisp->gimage);

  /*  We're done dirtying the image, and would like to be restarted
   *  if the image gets dirty while the tool exists
   */
  tool->preserve = FALSE;

  /*  Flush the gdisplays  */
  if (gdisp->disp_xoffset || gdisp->disp_yoffset)
    {
      gdk_window_get_size (gdisp->canvas->window, &x, &y);
      if (gdisp->disp_yoffset)
	{
	  gdisplay_expose_area (gdisp, 0, 0, gdisp->disp_width,
				gdisp->disp_yoffset);
	  gdisplay_expose_area (gdisp, 0, gdisp->disp_yoffset + y,
				gdisp->disp_width, gdisp->disp_height);
	}
      if (gdisp->disp_xoffset)
	{
	  gdisplay_expose_area (gdisp, 0, 0, gdisp->disp_xoffset,
				gdisp->disp_height);
	  gdisplay_expose_area (gdisp, gdisp->disp_xoffset + x, 0,
				gdisp->disp_width, gdisp->disp_height);
	}
    }

  gimp_remove_busy_cursors (NULL);

  gdisplays_flush ();

  gimp_transform_tool_reset (tool, gdisp);

  /*  if this tool is non-interactive, make it inactive after use  */
  if (!gt_tool->interactive)
    tool->state = INACTIVE;
}


void
gimp_transform_tool_motion (GimpTool          *tool,
		            GdkEventMotion    *mevent,
		            GDisplay          *gdisp)
{
  GimpTransformTool *tr_tool;

  tr_tool = GIMP_TRANSFORM_TOOL(tool);

  if (tr_tool->bpressed == FALSE)
  {
    /*  hey we have not got the button press yet
     *  so go away.
     */
    return;
  }

  /*  if we are creating or this tool is non-interactive, there is
   *  nothing to be done so exit.
   */
  if (tr_tool->function == TRANSFORM_CREATING ||
      !tr_tool->interactive)
    return;

  /*  stop the current tool drawing process  */
  gimp_draw_tool_pause (GIMP_DRAW_TOOL(tool));

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y,
			       &tr_tool->curx,
			       &tr_tool->cury, TRUE, 0);
  tr_tool->state = mevent->state;

  /*  recalculate the tool's transformation matrix  */
  gimp_transform_tool_transform (tr_tool, gdisp, TRANSFORM_MOTION);

  tr_tool->lastx = tr_tool->curx;
  tr_tool->lasty = tr_tool->cury;

  /*  resume drawing the current tool  */
  gimp_draw_tool_resume (GIMP_DRAW_TOOL(tool));
}

void
gimp_transform_tool_cursor_update (GimpTool           *tool,
			           GdkEventMotion     *mevent,
			           GDisplay           *gdisp)
{
  GimpTransformTool  *tr_tool;
  GimpDrawable       *drawable;
  GdkCursorType       ctype = GDK_TOP_LEFT_ARROW;
  gint                x, y;

  tool = GIMP_TRANSFORM_TOOL(tool);

  gdisplay_untransform_coords (gdisp, mevent->x, mevent->y, &x, &y,
			       FALSE, FALSE);

  if ((drawable = gimp_image_active_drawable (gdisp->gimage)))
    {
      if (GIMP_IS_LAYER (drawable) &&
	  gimp_layer_get_mask (GIMP_LAYER (drawable)))
	{
	  ctype = GIMP_BAD_CURSOR;
	}
      else if (x >= drawable->offset_x &&
	       y >= drawable->offset_y &&
	       x < (drawable->offset_x + drawable->width) &&
	       y < (drawable->offset_y + drawable->height))
	{
	  if (gimage_mask_is_empty (gdisp->gimage) ||
	      gimage_mask_value (gdisp->gimage, x, y))
	    {
	      ctype = GIMP_MOUSE_CURSOR;
	    }
	}
    }

  gdisplay_install_tool_cursor (gdisp,
				ctype,
				tool->tool_cursor,
				GIMP_CURSOR_MODIFIER_NONE);
}

void
gimp_transform_tool_control (GimpTool           *tool,
			     ToolAction          action,
			     GDisplay           *gdisp)
{
  GimpDrawTool       *dr_tool;
  GimpTransformTool  *tr_tool;

  dr_tool = GIMP_DRAW_TOOL(tool);
  tr_tool = GIMP_TRANSFORM_TOOL(tool);

  switch (action)
    {
    case PAUSE:
      gimp_draw_tool_pause (dr_tool);
      break;

    case RESUME:
      gimp_transform_tool_recalc (tr_tool, gdisp);
      gimp_draw_tool_resume (dr_tool);
      break;

    case HALT:
      gimp_transform_tool_reset (tr_tool, gdisp);
      break;

    default:
      break;
    }
}

void
gimp_transform_tool_no_draw (GimpDrawTool *tool)
{
  return;
}

void
gimp_transform_tool_draw (GimpDrawTool *dr_tool)
{
  GDisplay           *gdisp;
  GimpTransformTool  *tr_tool;
  GimpTool           *tool;
  gint                x1, y1, x2, y2, x3, y3, x4, y4;
  gint                srw, srh;
  gint                i, k, gci;
  gint                xa, ya, xb, yb;

  tr_tool        = GIMP_TRANSFORM_TOOL(dr_tool);
  tool           = GIMP_TOOL(tool);

  gdisp          = tool->gdisp;

  gdisplay_transform_coords (gdisp, tr_tool->tx1, tr_tool->ty1,
			     &tr_tool->sx1, &tr_tool->sy1, FALSE);
  gdisplay_transform_coords (gdisp, tr_tool->tx2, tr_tool->ty2,
			     &tr_tool->sx2, &tr_tool->sy2, FALSE);
  gdisplay_transform_coords (gdisp, tr_tool->tx3, tr_tool->ty3,
			     &tr_tool->sx3, &tr_tool->sy3, FALSE);
  gdisplay_transform_coords (gdisp, tr_tool->tx4, tr_tool->ty4,
			     &tr_tool->sx4, &tr_tool->sy4, FALSE);

  x1 = tr_tool->sx1;  y1 = tr_tool->sy1;
  x2 = tr_tool->sx2;  y2 = tr_tool->sy2;
  x3 = tr_tool->sx3;  y3 = tr_tool->sy3;
  x4 = tr_tool->sx4;  y4 = tr_tool->sy4;

  /*  find the handles' width and height  */
  srw = 10;
  srh = 10;

  /*  draw the bounding box  */
  gdk_draw_line (dr_tool->win, dr_tool->gc,
		 x1, y1, x2, y2);
  gdk_draw_line (dr_tool->win, dr_tool->gc,
		 x2, y2, x4, y4);
  gdk_draw_line (dr_tool->win, dr_tool->gc,
		 x3, y3, x4, y4);
  gdk_draw_line (dr_tool->win, dr_tool->gc,
		 x3, y3, x1, y1);

  /*  Draw the grid */

  if ((tr_tool->grid_coords != NULL) &&
      (tr_tool->tgrid_coords != NULL) /* FIXME!!! this doesn't belong here &&
      ((tool->type != PERSPECTIVE)  ||
       ((tr_tool->transform[0][0] >=0.0) &&
	(tr_tool->transform[1][1] >=0.0)) */ ) 
    {

      gci = 0;
      k = tr_tool->ngx + tr_tool->ngy;
      for (i = 0; i < k; i++)
	{
	  gdisplay_transform_coords (gdisp, tr_tool->tgrid_coords[gci],
				     tr_tool->tgrid_coords[gci+1],
				     &xa, &ya, FALSE);
	  gdisplay_transform_coords (gdisp, tr_tool->tgrid_coords[gci+2],
				     tr_tool->tgrid_coords[gci+3],
				     &xb, &yb, FALSE);

	  gdk_draw_line (dr_tool->win, dr_tool->gc,
			 xa, ya, xb, yb);
	  gci += 4;
	}
    }

  /*  draw the tool handles  */
  gdk_draw_rectangle (dr_tool->win, dr_tool->gc, 0,
		      x1 - (srw >> 1), y1 - (srh >> 1), srw, srh);
  gdk_draw_rectangle (dr_tool->win, dr_tool->gc, 0,
		      x2 - (srw >> 1), y2 - (srh >> 1), srw, srh);
  gdk_draw_rectangle (dr_tool->win, dr_tool->gc, 0,
		      x3 - (srw >> 1), y3 - (srh >> 1), srw, srh);
  gdk_draw_rectangle (dr_tool->win, dr_tool->gc, 0,
		      x4 - (srw >> 1), y4 - (srh >> 1), srw, srh);

#ifdef FIXME_I_DO_NOT_BELONG_HERE
  /*  draw the center  */
  if (tool->type == ROTATE)
    {
      gdisplay_transform_coords (gdisp, gimp_transform_tool->tcx, gimp_transform_tool->tcy,
				 &gimp_transform_tool->scx, &gimp_transform_tool->scy, FALSE);

      gdk_draw_arc (gimp_transform_tool->core->win, gimp_transform_tool->core->gc, 1,
		    gimp_transform_tool->scx - (srw >> 1),
		    gimp_transform_tool->scy - (srh >> 1),
		    srw, srh, 0, 23040);
    }
#endif

  if (transform_tool_showpath ())
    {
      GimpMatrix3 tmp_matrix;

      if (transform_tool_direction () == TRANSFORM_CORRECTIVE)
	{
	  gimp_matrix3_invert (tr_tool->transform, tmp_matrix);
	}
      else
	{
	  gimp_matrix3_duplicate (tr_tool->transform, tmp_matrix);
	}

      path_transform_draw_current (gdisp, dr_tool, tmp_matrix);
    }
}

void
gimp_transform_tool_destroy (GtkObject *object)
{
  GimpTransformTool *tr_tool = GIMP_TRANSFORM_TOOL (object);
  GimpDrawTool      *dr_tool = GIMP_DRAW_TOOL      (tr_tool);
  GimpTool          *tool    = GIMP_TOOL           (tr_tool);

  /*  Make sure the selection core is not visible  */
  if (tool->state == ACTIVE)
    gimp_draw_tool_stop (dr_tool);


  /*  Free up the original selection if it exists  */
  if (tr_tool->original)
    tile_manager_destroy (tr_tool->original);

  /*  If there is an information dialog, free it up  */
  if (transform_info)
    info_dialog_free (transform_info);

  transform_info        = NULL;
  transform_info_inited = FALSE;

  /*  Free the grid line endpoint arrays if they exist */
  if (tr_tool->grid_coords != NULL)
    g_free (tr_tool->grid_coords);

  if (tr_tool->tgrid_coords != NULL)
    g_free (tr_tool->tgrid_coords);

}

void
gimp_transform_tool_transform_bounding_box (GimpTransformTool *tr_tool)
{
  GimpTool  *tool;
  gint       i, k;
  gint       gci;

  tool = GIMP_TOOL(tr_tool);

  gimp_matrix3_transform_point (tr_tool->transform,
				tr_tool->x1, tr_tool->y1,
				&tr_tool->tx1, &tr_tool->ty1);
  gimp_matrix3_transform_point (tr_tool->transform,
				tr_tool->x2, tr_tool->y1,
				&tr_tool->tx2, &tr_tool->ty2);
  gimp_matrix3_transform_point (tr_tool->transform,
				tr_tool->x1, tr_tool->y2,
				&tr_tool->tx3, &tr_tool->ty3);
  gimp_matrix3_transform_point (tr_tool->transform,
				tr_tool->x2, tr_tool->y2,
				&tr_tool->tx4, &tr_tool->ty4);

/* FIXME  if (tool->type == ROTATE)
    gimp_matrix3_transform_point (tr_tool->transform,
				  tr_tool->cx, tr_tool->cy,
				  &tr_tool->tcx, &tr_tool->tcy); */

  if (tr_tool->grid_coords != NULL &&
      tr_tool->tgrid_coords != NULL)
    {
      gci = 0;
      k  = (tr_tool->ngx + tr_tool->ngy) * 2;
      for (i = 0; i < k; i++)
	{
	  gimp_matrix3_transform_point (tr_tool->transform,
					tr_tool->grid_coords[gci],
					tr_tool->grid_coords[gci+1],
					&(tr_tool->tgrid_coords[gci]),
					&(tr_tool->tgrid_coords[gci+1]));
	  gci += 2;
	}
    }
}

void
gimp_transform_tool_reset (GimpTransformTool   *tr_tool,
		           GDisplay            *gdisp)
{
  GimpTool *tool;

  tool = GIMP_TOOL(tr_tool);

  if (tr_tool->original)
    tile_manager_destroy (tr_tool->original);
  tr_tool->original = NULL;

  /*  inactivate the tool  */
  tr_tool->function = TRANSFORM_CREATING;
  gimp_draw_tool_stop (GIMP_DRAW_TOOL(tr_tool));
  info_dialog_popdown (transform_info);

  tool->state    = INACTIVE;
  tool->gdisp    = NULL;
  tool->drawable = NULL;
}

static void
gimp_transform_tool_bounds (GimpTransformTool   *tr_tool,
		            GDisplay            *gdisp)
{
  GimpTool      *tool;
  TileManager   *tiles;
  GimpDrawable  *drawable;
  gint           offset_x, offset_y;

  tiles      = tr_tool->original;
  drawable   = gimp_image_active_drawable (gdisp->gimage);

  /*  find the boundaries  */
  if (tiles)
    {
      tile_manager_get_offsets (tiles,
				&tr_tool->x1, &tr_tool->y1);
				
      tr_tool->x2 = tr_tool->x1 + tile_manager_width (tiles);
      tr_tool->y2 = tr_tool->y1 + tile_manager_height (tiles);
    }
  else
    {
      gimp_drawable_offsets (drawable, &offset_x, &offset_y);
      gimp_drawable_mask_bounds (drawable,
				 &tr_tool->x1, &tr_tool->y1,
				 &tr_tool->x2, &tr_tool->y2);
      tr_tool->x1 += offset_x;
      tr_tool->y1 += offset_y;
      tr_tool->x2 += offset_x;
      tr_tool->y2 += offset_y;
    }

  tr_tool->cx = (tr_tool->x1 + tr_tool->x2) / 2;
  tr_tool->cy = (tr_tool->y1 + tr_tool->y2) / 2;

  /*  changing the bounds invalidates any grid we may have  */
  gimp_transform_tool_grid_recalc (tr_tool);
}

void
gimp_transform_tool_grid_density_changed (void)
{
  GimpTransformTool *tr_tool;
  GimpDrawTool      *dr_tool;

  /* EEEEEEEK!!! */
  tr_tool = GIMP_TRANSFORM_TOOL(active_tool);
  dr_tool = GIMP_DRAW_TOOL(tr_tool);

  if (tr_tool->function == TRANSFORM_CREATING)
    return;

  gimp_draw_tool_pause (dr_tool);
  gimp_transform_tool_grid_recalc (tr_tool);
  gimp_transform_tool_transform_bounding_box (tr_tool);
  gimp_draw_tool_resume (dr_tool);
}

void
gimp_transform_tool_showpath_changed (gint type /* a truly undescriptive name */)
{
  GimpTransformTool *tr_tool;

  /* EEEEEEEK!!! */
  tr_tool = (GimpTransformTool *) active_tool;

  if (tr_tool->function == TRANSFORM_CREATING)
    return;

  if (type)
    gimp_draw_tool_pause (GIMP_DRAW_TOOL(tr_tool));
  else
    gimp_draw_tool_resume (GIMP_DRAW_TOOL(tr_tool));
}

static void
gimp_transform_tool_grid_recalc (GimpTransformTool *tr_tool)
{
  if (tr_tool->grid_coords != NULL)
    {
      g_free (tr_tool->grid_coords);
      tr_tool->grid_coords = NULL;
    }
  if (tr_tool->tgrid_coords != NULL)
    {
      g_free (tr_tool->tgrid_coords);
      tr_tool->tgrid_coords = NULL;
    }
  if (transform_tool_show_grid ())
    gimp_transform_tool_setup_grid (/* EEEEEEK!!! */ GIMP_TRANSFORM_TOOL(active_tool));
}

static void
gimp_transform_tool_setup_grid (GimpTransformTool *tr_tool)
{
  GimpTool   *tool;
  gint        i, gci;
  gdouble    *coords;

  tool = GIMP_TOOL(tr_tool);

  /*  We use the transform_tool_grid_size function only here, even
   *  if the user changes the grid size in the middle of an
   *  operation, nothing happens.
   */
  tr_tool->ngx =
    (tr_tool->x2 - tr_tool->x1) / transform_tool_grid_size ();
  if (tr_tool->ngx > 0)
    tr_tool->ngx--;

 tr_tool->ngy =
    (tr_tool->y2 - tr_tool->y1) / transform_tool_grid_size ();
  if (tr_tool->ngy > 0)
    tr_tool->ngy--;

  tr_tool->grid_coords = coords =
    g_new (double, (tr_tool->ngx + tr_tool->ngy) * 4);

  tr_tool->tgrid_coords =
    g_new (double, (tr_tool->ngx + tr_tool->ngy) * 4);

  gci = 0;
  for (i = 1; i <= tr_tool->ngx; i++)
    {
      coords[gci] = tr_tool->x1 +
	((double) i)/(tr_tool->ngx + 1) *
	(tr_tool->x2 - tr_tool->x1);
      coords[gci+1] = tr_tool->y1;
      coords[gci+2] = coords[gci];
      coords[gci+3] = tr_tool->y2;
      gci += 4;
    }
  for (i = 1; i <= tr_tool->ngy; i++)
    {
      coords[gci] = tr_tool->x1;
      coords[gci+1] = tr_tool->y1 +
	((double) i)/(tr_tool->ngy + 1) *
	(tr_tool->y2 - tr_tool->y1);
      coords[gci+2] = tr_tool->x2;
      coords[gci+3] = coords[gci+1];
      gci += 4;
    }
}

static void
gimp_transform_tool_recalc (GimpTransformTool   *tr_tool,
		            GDisplay            *gdisp)
{
  gimp_transform_tool_bounds (tr_tool, gdisp);

   gimp_transform_tool_transform (tr_tool, gdisp, TRANSFORM_RECALC);
}

/*  Actually carry out a transformation  */
TileManager *
gimp_transform_tool_do (GimpImage           *gimage,
                        GimpDrawable     *drawable,
                        TileManager      *float_tiles,
                        gboolean          interpolation,
                        GimpMatrix3       matrix,
                        GimpProgressFunc  progress_callback,
                        gpointer          progress_data)
{
  PixelRegion  destPR;
  TileManager *tiles;
  GimpMatrix3  m;
  GimpMatrix3  im;
  gint         itx, ity;
  gint         tx1, ty1, tx2, ty2;
  gint         width, height;
  gint         alpha;
  gint         bytes, b;
  gint         x, y;
  gint         sx, sy;
  gint         x1, y1, x2, y2;
  gdouble      xinc, yinc, winc;
  gdouble      tx, ty, tw;
  gdouble      ttx = 0.0, tty = 0.0;
  guchar      *dest;
  guchar      *d;
  guchar      *src[16];
  Tile        *tile[16];
  guchar       bg_col[MAX_CHANNELS];
  gint         i;
  gdouble      a_val, a_recip;
  gint         newval;

  PixelSurround surround;

  alpha = 0;

  /*  turn interpolation off for simple transformations (e.g. rot90)  */
  if (gimp_matrix3_is_simple (matrix) ||
      interpolation_type == NEAREST_NEIGHBOR_INTERPOLATION)
    interpolation = FALSE;

  /*  Get the background color  */
  gimp_image_get_background (gimage, drawable, bg_col);

  switch (gimp_drawable_type (drawable))
    {
    case RGB_GIMAGE: case RGBA_GIMAGE:
      bg_col[ALPHA_PIX] = TRANSPARENT_OPACITY;
      alpha = ALPHA_PIX;
      break;
    case GRAY_GIMAGE: case GRAYA_GIMAGE:
      bg_col[ALPHA_G_PIX] = TRANSPARENT_OPACITY;
      alpha = ALPHA_G_PIX;
      break;
    case INDEXED_GIMAGE: case INDEXEDA_GIMAGE:
      bg_col[ALPHA_I_PIX] = TRANSPARENT_OPACITY;
      alpha = ALPHA_I_PIX;
      /*  If the gimage is indexed color, ignore smoothing value  */
      interpolation = FALSE;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  /*  enable rotating un-floated non-layers  */
  if (tile_manager_bpp (float_tiles) == 1)
    {
      bg_col[0] = OPAQUE_OPACITY;

      /*  setting alpha = 0 will cause the channel's value to be treated
       *  as alpha and the color channel loops never to be entered
       */
      alpha = 0;
    }

  if (transform_tool_direction () == TRANSFORM_CORRECTIVE)
    {
      /*  keep the original matrix here, so we dont need to recalculate
	  the inverse later  */
      gimp_matrix3_duplicate (matrix, m);
      gimp_matrix3_invert (matrix, im);
      matrix = im;
    }
  else
    {
      /*  Find the inverse of the transformation matrix  */
      gimp_matrix3_invert (matrix, m);
    }

  path_transform_current_path (gimage, matrix, FALSE);

  tile_manager_get_offsets (float_tiles, &x1, &y1);
  x2 = x1 + tile_manager_width (float_tiles);
  y2 = y1 + tile_manager_height (float_tiles);

  /*  Find the bounding coordinates  */
  if (alpha == 0 || (active_tool && transform_tool_clip ()))
    {
      tx1 = x1;
      ty1 = y1;
      tx2 = x2;
      ty2 = y2;
    }
  else
    {
      gdouble dx1, dy1, dx2, dy2, dx3, dy3, dx4, dy4;

      gimp_matrix3_transform_point (matrix, x1, y1, &dx1, &dy1);
      gimp_matrix3_transform_point (matrix, x2, y1, &dx2, &dy2);
      gimp_matrix3_transform_point (matrix, x1, y2, &dx3, &dy3);
      gimp_matrix3_transform_point (matrix, x2, y2, &dx4, &dy4);

      tx1 = MIN (dx1, dx2);
      tx1 = MIN (tx1, dx3);
      tx1 = MIN (tx1, dx4);
      ty1 = MIN (dy1, dy2);
      ty1 = MIN (ty1, dy3);
      ty1 = MIN (ty1, dy4);
      tx2 = MAX (dx1, dx2);
      tx2 = MAX (tx2, dx3);
      tx2 = MAX (tx2, dx4);
      ty2 = MAX (dy1, dy2);
      ty2 = MAX (ty2, dy3);
      ty2 = MAX (ty2, dy4);
    }

  /*  Get the new temporary buffer for the transformed result  */
  tiles = tile_manager_new ((tx2 - tx1), (ty2 - ty1),
			    tile_manager_bpp (float_tiles));
  pixel_region_init (&destPR, tiles, 0, 0, (tx2 - tx1), (ty2 - ty1), TRUE);
  tile_manager_set_offsets (tiles, tx1, ty1);

  /* initialise the pixel_surround accessor */
  if (interpolation)
    {
      if (interpolation_type == CUBIC_INTERPOLATION)
	{
	  pixel_surround_init (&surround, float_tiles, 4, 4, bg_col);
	}
      else
	{
	  pixel_surround_init (&surround, float_tiles, 2, 2, bg_col);
	}
    }
  else
    {
      /* not actually useful, keeps the code cleaner */
      pixel_surround_init (&surround, float_tiles, 1, 1, bg_col);
    }

  width  = tile_manager_width (tiles);
  height = tile_manager_height (tiles);
  bytes  = tile_manager_bpp (tiles);

  dest = g_new (guchar, width * bytes);

  xinc = m[0][0];
  yinc = m[1][0];
  winc = m[2][0];

  /* these loops could be rearranged, depending on which bit of code
   * you'd most like to write more than once.
   */

  for (y = ty1; y < ty2; y++)
    {
      if (progress_callback && !(y & 0xf))
	(* progress_callback) (ty1, ty2, y, progress_data);

      /* set up inverse transform steps */
      tx = xinc * tx1 + m[0][1] * y + m[0][2];
      ty = yinc * tx1 + m[1][1] * y + m[1][2];
      tw = winc * tx1 + m[2][1] * y + m[2][2];

      d = dest;
      for (x = tx1; x < tx2; x++)
	{
	  /*  normalize homogeneous coords  */
	  if (tw == 0.0)
	    {
	      g_warning ("homogeneous coordinate = 0...\n");
	    }
	  else if (tw != 1.0)
	    {
	      ttx = tx / tw;
	      tty = ty / tw;
	    }
	  else
	    {
	      ttx = tx;
	      tty = ty;
	    }

          /*  Set the destination pixels  */

          if (interpolation)
       	    {
              if (interpolation_type == CUBIC_INTERPOLATION)
       	        {
                  /*  ttx & tty are the subpixel coordinates of the point in
		   *  the original selection's floating buffer.
		   *  We need the four integer pixel coords around them:
		   *  itx to itx + 3, ity to ity + 3
                   */
                  itx = floor (ttx);
                  ity = floor (tty);

		  /* check if any part of our region overlaps the buffer */

                  if ((itx + 2) >= x1 && (itx - 1) < x2 &&
                      (ity + 2) >= y1 && (ity - 1) < y2 )
                    {
                      guchar  *data;
                      gint     row;
                      gdouble  dx, dy;
                      guchar  *start;

		      /* lock the pixel surround */
                      data = pixel_surround_lock (&surround,
						  itx - 1 - x1, ity - 1 - y1);

                      row = pixel_surround_rowstride (&surround);

                      /* the fractional error */
                      dx = ttx - itx;
                      dy = tty - ity;

		      /* calculate alpha of result */
		      start = &data[alpha];
		      a_val = gimp_transform_tool_cubic (dy,
				     CUBIC_ROW (dx, start, bytes),
				     CUBIC_ROW (dx, start + row, bytes),
				     CUBIC_ROW (dx, start + row + row, bytes),
				     CUBIC_ROW (dx, start + row + row + row, bytes));

		      if (a_val <= 0.0)
			{
			  a_recip = 0.0;
			  d[alpha] = 0;
			}
		      else if (a_val > 255.0)
			{
			  a_recip = 1.0 / a_val;
			  d[alpha] = 255;
			}
		      else
			{
			  a_recip = 1.0 / a_val;
			  d[alpha] = RINT(a_val);
			}

		      /*  for colour channels c,
		       *  result = bicubic (c * alpha) / bicubic (alpha)
		       *
		       *  never entered for alpha == 0
		       */
		      for (i = -alpha; i < 0; ++i)
			{
			  start = &data[alpha];
			  newval =
			    RINT (a_recip *
				  gimp_transform_tool_cubic (dy,
					 CUBIC_SCALED_ROW (dx, start, bytes, i),
					 CUBIC_SCALED_ROW (dx, start + row, bytes, i),
					 CUBIC_SCALED_ROW (dx, start + row + row, bytes, i),
					 CUBIC_SCALED_ROW (dx, start + row + row + row, bytes, i)));
			  if (newval <= 0)
			    {
			      *d++ = 0;
			    }
			  else if (newval > 255)
			    {
			      *d++ = 255;
			    }
			  else
			    {
			      *d++ = newval;
			    }
			}

		      /*  alpha already done  */
		      d++;

		      pixel_surround_release (&surround);
		    }
                  else /* not in source range */
                    {
                      /*  increment the destination pointers  */
                      for (b = 0; b < bytes; b++)
                        *d++ = bg_col[b];
                    }
                }

       	      else  /*  linear  */
                {
                  itx = floor (ttx);
                  ity = floor (tty);

		  /*  expand source area to cover interpolation region
		   *  (which runs from itx to itx + 1, same in y)
		   */
                  if ((itx + 1) >= x1 && itx < x2 &&
                      (ity + 1) >= y1 && ity < y2 )
                    {
                      guchar  *data;
                      gint     row;
                      double   dx, dy;
                      guchar  *chan;

		      /* lock the pixel surround */
                      data = pixel_surround_lock (&surround, itx - x1, ity - y1);

                      row = pixel_surround_rowstride (&surround);

                      /* the fractional error */
                      dx = ttx - itx;
                      dy = tty - ity;

		      /* calculate alpha value of result pixel */
		      chan = &data[alpha];
		      a_val = BILINEAR (chan[0], chan[bytes], chan[row],
					chan[row+bytes], dx, dy);
		      if (a_val <= 0.0)
			{
			  a_recip = 0.0;
			  d[alpha] = 0.0;
			}
		      else if (a_val >= 255.0)
			{
			  a_recip = 1.0 / a_val;
			  d[alpha] = 255;
			}
		      else
			{
			  a_recip = 1.0 / a_val;
			  d[alpha] = RINT (a_val);
			}

		      /*  for colour channels c,
		       *  result = bilinear (c * alpha) / bilinear (alpha)
		       *
		       *  never entered for alpha == 0
		       */
		      for (i = -alpha; i < 0; ++i)
			{
			  chan = &data[alpha];
			  newval =
			    RINT (a_recip *
				  BILINEAR (chan[0] * chan[i],
					    chan[bytes] * chan[bytes+i],
					    chan[row] * chan[row+i],
					    chan[row+bytes] * chan[row+bytes+i],
					    dx, dy));
			  if (newval <= 0)
			    {
			      *d++ = 0;
			    }
			  else if (newval > 255)
			    {
			      *d++ = 255;
			    }
			  else
			    {
			      *d++ = newval;
			    }
			}

		      /*  alpha already done  */
		      d++;

                      pixel_surround_release (&surround);
		    }

                  else /* not in source range */
                    {
                      /*  increment the destination pointers  */
                      for (b = 0; b < bytes; b++)
                        *d++ = bg_col[b];
                    }
		}
	    }
          else  /*  no interpolation  */
            {
              itx = floor (ttx);
              ity = floor (tty);

              if (itx >= x1 && itx < x2 &&
                  ity >= y1 && ity < y2 )
                {
                  /*  x, y coordinates into source tiles  */
                  sx = itx - x1;
                  sy = ity - y1;

                  REF_TILE (0, sx, sy);

                  for (b = 0; b < bytes; b++)
                    *d++ = src[0][b];

                  tile_release (tile[0], FALSE);
		}
              else /* not in source range */
                {
                  /*  increment the destination pointers  */
                  for (b = 0; b < bytes; b++)
                    *d++ = bg_col[b];
                }
	    }
	  /*  increment the transformed coordinates  */
	  tx += xinc;
	  ty += yinc;
	  tw += winc;
	}

      /*  set the pixel region row  */
      pixel_region_set_row (&destPR, 0, (y - ty1), width, dest);
    }

  pixel_surround_clear (&surround);

  g_free (dest);
  return tiles;
}

TileManager *
gimp_transform_tool_cut (GimpImage       *gimage,
		         GimpDrawable *drawable,
		         gboolean     *new_layer)
{
  TileManager *tiles;

  /*  extract the selected mask if there is a selection  */
  if (! gimage_mask_is_empty (gimage))
    {
      /* set the keep_indexed flag to FALSE here, since we use
	 gimp_layer_new_from_tiles() later which assumes that the tiles
	 are either RGB or GRAY.  Eeek!!!              (Sven)
       */
      tiles = gimage_mask_extract (gimage, drawable, TRUE, FALSE, TRUE);
      *new_layer = TRUE;
    }
  /*  otherwise, just copy the layer  */
  else
    {
      if (GIMP_IS_LAYER (drawable))
	tiles = gimage_mask_extract (gimage, drawable, FALSE, TRUE, TRUE);
      else
	tiles = gimage_mask_extract (gimage, drawable, FALSE, TRUE, FALSE);
      *new_layer = FALSE;
    }

  return tiles;
}


/*  Paste a transform to the gdisplay  */
gboolean
gimp_transform_tool_paste (GimpImage       *gimage,
		           GimpDrawable *drawable,
		           TileManager  *tiles,
		           gboolean      new_layer)
{
  GimpLayer   *layer   = NULL;
  GimpChannel *channel = NULL;
  GimpLayer   *floating_layer;

  if (new_layer)
    {
      layer =
	gimp_layer_new_from_tiles (gimage,
				   gimp_drawable_type_with_alpha (drawable),
				   tiles,
				   _("Transformation"),
				   OPAQUE_OPACITY, NORMAL_MODE);
      if (! layer)
        {
          g_warning ("gimp_transform_tool_paste: gimp_layer_new_frome_tiles() failed");
          return FALSE;
        }

      tile_manager_get_offsets (tiles, 
				&(GIMP_DRAWABLE (layer)->offset_x),
				&(GIMP_DRAWABLE (layer)->offset_y));

      /*  Start a group undo  */
      undo_push_group_start (gimage, EDIT_PASTE_UNDO);

      floating_sel_attach (layer, drawable);

      /*  End the group undo  */
      undo_push_group_end (gimage);

      /*  Free the tiles  */
      tile_manager_destroy (tiles);

      return TRUE;
    }
  else
    {
      if (GIMP_IS_LAYER (drawable))
	layer = GIMP_LAYER (drawable);
      else if (GIMP_IS_CHANNEL (drawable))
	channel = GIMP_CHANNEL (drawable);
      else
	return FALSE;

      if (layer)
	gimp_layer_add_alpha (layer);

      floating_layer = gimp_image_floating_sel (gimage);

      if (floating_layer)
	floating_sel_relax (floating_layer, TRUE);

      gdisplays_update_area (gimage,
			     drawable->offset_x,
			     drawable->offset_y,
			     drawable->width,
			     drawable->height);

      /*  Push an undo  */
      if (layer)
	undo_push_layer_mod (gimage, layer);
      else if (channel)
	undo_push_channel_mod (gimage, channel);

      /*  set the current layer's data  */
      drawable->tiles = tiles;

      /*  Fill in the new layer's attributes  */
      drawable->width    = tile_manager_width (tiles);
      drawable->height   = tile_manager_height (tiles);
      drawable->bytes    = tile_manager_bpp (tiles);
      tile_manager_get_offsets (tiles, 
				&drawable->offset_x, &drawable->offset_y);

      if (floating_layer)
	floating_sel_rigor (floating_layer, TRUE);

      drawable_update (drawable,
		       0, 0,
		       gimp_drawable_width (drawable),
		       gimp_drawable_height (drawable));

      /*  if we were operating on the floating selection, then it's boundary 
       *  and previews need invalidating
       */
      if (drawable == (GimpDrawable *) floating_layer)
	floating_sel_invalidate (floating_layer);

      return TRUE;
    }
}

/* Note: cubic function no longer clips result */
static gdouble
gimp_transform_tool_cubic (gdouble dx,
       gint    jm1,
       gint    j,
       gint    jp1,
       gint    jp2)
{
  gdouble result;

#if 0
  /* Equivalent to Gimp 1.1.1 and earlier - some ringing */
  result = ((( ( - jm1 + j - jp1 + jp2 ) * dx +
               ( jm1 + jm1 - j - j + jp1 - jp2 ) ) * dx +
               ( - jm1 + jp1 ) ) * dx + j );
  /* Recommended by Mitchell and Netravali - too blurred? */
  result = ((( ( - 7 * jm1 + 21 * j - 21 * jp1 + 7 * jp2 ) * dx +
               ( 15 * jm1 - 36 * j + 27 * jp1 - 6 * jp2 ) ) * dx +
               ( - 9 * jm1 + 9 * jp1 ) ) * dx + (jm1 + 16 * j + jp1) ) / 18.0;
#else

  /* Catmull-Rom - not bad */
  result = ((( ( - jm1 + 3 * j - 3 * jp1 + jp2 ) * dx +
               ( 2 * jm1 - 5 * j + 4 * jp1 - jp2 ) ) * dx +
               ( - jm1 + jp1 ) ) * dx + (j + j) ) / 2.0;

#endif

  return result;
}
