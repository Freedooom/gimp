/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Vector tool
 * Copyright (C) 2002 Simon Budig  <simon@gimp.org>
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

#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplist.h"
#include "core/gimptoolinfo.h"

#include "vectors/gimpanchor.h"
#include "vectors/gimpvectors.h"
#include "vectors/gimpbezierstroke.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplay-foreach.h"

#include "gimptoolcontrol.h"
#include "gimpvectoroptions.h"
#include "gimpvectortool.h"

#include "gimp-intl.h"


#define TARGET 9


/*  local function prototypes  */

static void   gimp_vector_tool_class_init      (GimpVectorToolClass *klass);
static void   gimp_vector_tool_init            (GimpVectorTool      *tool);

static void   gimp_vector_tool_control         (GimpTool        *tool,
                                                GimpToolAction   action,
                                                GimpDisplay     *gdisp);
static void   gimp_vector_tool_button_press    (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                guint32          time,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);
static void   gimp_vector_tool_button_release  (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                guint32          time,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);
static void   gimp_vector_tool_motion          (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                guint32          time,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);
static void   gimp_vector_tool_cursor_update   (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                GdkModifierType  state,
                                                GimpDisplay     *gdisp);

static gboolean gimp_vector_tool_on_handle     (GimpTool        *tool,
                                                GimpCoords      *coords,
                                                GimpAnchorType   preferred,
                                                GimpDisplay     *gdisp,
                                                GimpAnchor     **ret_anchor,
                                                GimpStroke     **ret_stroke);

static void   gimp_vector_tool_draw            (GimpDrawTool    *draw_tool);

static void   gimp_vector_tool_clear_vectors   (GimpVectorTool  *vector_tool);

static void   gimp_vector_tool_vectors_freeze  (GimpVectors     *vectors,
                                                GimpVectorTool  *vector_tool);
static void   gimp_vector_tool_vectors_thaw    (GimpVectors     *vectors,
                                                GimpVectorTool  *vector_tool);


static GimpSelectionToolClass *parent_class = NULL;


void
gimp_vector_tool_register (GimpToolRegisterCallback  callback,
                           gpointer                  data)
{
  (* callback) (GIMP_TYPE_VECTOR_TOOL,
                GIMP_TYPE_VECTOR_OPTIONS,
                gimp_vector_options_gui,
                0,
                "gimp-vector-tool",
                _("Vectors"),
                _("the most promising path tool prototype... :-)"),
                N_("/Tools/_Vectors"), NULL,
                NULL, "tools/vector.html",
                GIMP_STOCK_TOOL_PATH,
                data);
}

GType
gimp_vector_tool_get_type (void)
{
  static GType tool_type = 0;

  if (! tool_type)
    {
      static const GTypeInfo tool_info =
      {
        sizeof (GimpVectorToolClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_vector_tool_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpVectorTool),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_vector_tool_init,
      };

      tool_type = g_type_register_static (GIMP_TYPE_SELECTION_TOOL,
					  "GimpVectorTool", 
                                          &tool_info, 0);
    }

  return tool_type;
}

static void
gimp_vector_tool_class_init (GimpVectorToolClass *klass)
{
  GObjectClass      *object_class;
  GimpToolClass     *tool_class;
  GimpDrawToolClass *draw_tool_class;

  object_class    = G_OBJECT_CLASS (klass);
  tool_class      = GIMP_TOOL_CLASS (klass);
  draw_tool_class = GIMP_DRAW_TOOL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  tool_class->control        = gimp_vector_tool_control;
  tool_class->button_press   = gimp_vector_tool_button_press;
  tool_class->button_release = gimp_vector_tool_button_release;
  tool_class->motion         = gimp_vector_tool_motion;
  tool_class->cursor_update  = gimp_vector_tool_cursor_update;

  draw_tool_class->draw      = gimp_vector_tool_draw;
}

static void
gimp_vector_tool_init (GimpVectorTool *vector_tool)
{
  GimpTool *tool;

  tool = GIMP_TOOL (vector_tool);

  gimp_tool_control_set_scroll_lock (tool->control, TRUE);
  
  vector_tool->function       = VECTORS_CREATING;
  vector_tool->last_x         = 0;
  vector_tool->last_y         = 0;

  vector_tool->cur_anchor     = NULL;
  vector_tool->cur_stroke     = NULL;
  vector_tool->vectors        = NULL;
  vector_tool->active_anchors = NULL;
}


static void
gimp_vector_tool_control (GimpTool       *tool,
                          GimpToolAction  action,
                          GimpDisplay    *gdisp)
{
  GimpVectorTool *vector_tool;

  vector_tool = GIMP_VECTOR_TOOL (tool);

  switch (action)
    {
    case PAUSE:
      break;

    case RESUME:
      break;

    case HALT:
      /* gimp_tool_pop_status (tool); */
      gimp_tool_control_halt (tool->control);
      gimp_vector_tool_clear_vectors (vector_tool);
      break;

    default:
      break;
    }

  GIMP_TOOL_CLASS (parent_class)->control (tool, action, gdisp);
}

static void
gimp_vector_tool_button_press (GimpTool        *tool,
                               GimpCoords      *coords,
                               guint32          time,
                               GdkModifierType  state,
                               GimpDisplay     *gdisp)
{
  GimpVectorTool    *vector_tool;
  GimpVectorOptions *options;
  GimpAnchor        *anchor = NULL;
  GimpStroke        *stroke = NULL;
  GimpAnchorType     preferred = GIMP_ANCHOR_ANCHOR;

  vector_tool = GIMP_VECTOR_TOOL (tool);
  options     = GIMP_VECTOR_OPTIONS (tool->tool_info->tool_options);

  /* when pressing mouse down
   *
   * Anchor: (NONE) -> Regular Movement 
   *         (SHFT) -> multiple selection
   *         (CTRL) -> Drag out control point
   *         (CTRL+SHFT) -> Convert to corner
   *         
   * Handle: (NONE) -> Regular Movement
   *         (SHFT) -> (Handle) Move opposite handle symmetrically
   *         (CTRL+SHFT) -> move handle to its anchor
   */

  /*  if we are changing displays, pop the statusbar of the old one  */ 
  if (gimp_tool_control_is_active (tool->control) && gdisp != tool->gdisp)
    {
      /* gimp_tool_pop_status (tool); */
    }

  vector_tool->restriction = GIMP_ANCHOR_FEATURE_NONE;

  if (vector_tool->vectors &&
      gdisp->gimage != GIMP_ITEM (vector_tool->vectors)->gimage)
    gimp_vector_tool_clear_vectors (vector_tool);

  if (gimp_tool_control_is_active (tool->control))
    {
      /* reset everything */
      gimp_draw_tool_stop (GIMP_DRAW_TOOL (vector_tool));
    }

  tool->gdisp = gdisp;

  if (! vector_tool->vectors || vector_tool->function == VECTORS_CREATING)
    {
      if (! vector_tool->vectors)
        {
          GimpVectors *vectors;

          vectors = gimp_vectors_new (gdisp->gimage, _("Unnamed"));

          gimp_image_add_vectors (gdisp->gimage, vectors, -1);

          vector_tool->vectors = g_object_ref (vectors);

          g_signal_connect_object (vectors, "removed", 
                                   G_CALLBACK (gimp_vector_tool_clear_vectors),
                                   vector_tool,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (vectors, "freeze",
                                   G_CALLBACK (gimp_vector_tool_vectors_freeze),
                                   vector_tool,
                                   0);
          g_signal_connect_object (vectors, "thaw",
                                   G_CALLBACK (gimp_vector_tool_vectors_thaw),
                                   vector_tool,
                                   0);
        }

      stroke = gimp_bezier_stroke_new ();
      anchor = gimp_bezier_stroke_extend (GIMP_BEZIER_STROKE (stroke), coords,
                                          NULL, EXTEND_EDITABLE);
      gimp_vectors_stroke_add (vector_tool->vectors, stroke);

      vector_tool->function = VECTORS_MOVING;
      
      /* We want to drag out the control point later */
      state |= GDK_CONTROL_MASK;
      vector_tool->restriction = GIMP_ANCHOR_FEATURE_SYMMETRIC;

      /*  start drawing the vector tool  */
      gimp_draw_tool_start (GIMP_DRAW_TOOL (tool), gdisp);
    }

  else if (!gimp_vector_tool_on_handle (tool, coords, GIMP_ANCHOR_CONTROL,
                                        gdisp, NULL, NULL))
    {
      gimp_draw_tool_pause (GIMP_DRAW_TOOL (vector_tool));

      /* not on an anchor, we go into ADDING mode */
      vector_tool->function = VECTORS_ADDING;
      anchor = gimp_bezier_stroke_extend (GIMP_BEZIER_STROKE (vector_tool->cur_stroke),
                                          coords, vector_tool->cur_anchor,
                                          EXTEND_EDITABLE);
      if (anchor)
        vector_tool->cur_anchor = anchor;

      state |= GDK_CONTROL_MASK;
      vector_tool->restriction = GIMP_ANCHOR_FEATURE_SYMMETRIC;

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (vector_tool));
    }

  if (state & GDK_CONTROL_MASK && !(state & GDK_SHIFT_MASK))
    preferred = GIMP_ANCHOR_CONTROL;

  anchor = vector_tool->cur_anchor;

  if (gimp_vector_tool_on_handle (tool, coords,
                                  preferred, gdisp, &anchor, &stroke))
    {
      gimp_draw_tool_pause (GIMP_DRAW_TOOL (vector_tool));

      if (anchor->type == GIMP_ANCHOR_ANCHOR)
        {
          if (state & GDK_SHIFT_MASK && !(state & GDK_CONTROL_MASK))
            gimp_stroke_anchor_select (stroke, anchor, FALSE);
          else
            gimp_stroke_anchor_select (stroke, anchor, TRUE);
      
          /* Maybe convert to edge */
          if (state & GDK_CONTROL_MASK && state & GDK_SHIFT_MASK)
            {
              gimp_stroke_anchor_convert (stroke, anchor,
                                          GIMP_ANCHOR_FEATURE_EDGE);
              vector_tool->restriction = GIMP_ANCHOR_FEATURE_SYMMETRIC;
            }

          /* if the selected anchor changed, the visible control
           * points might have changed too */
          if (state & GDK_CONTROL_MASK)
            gimp_vector_tool_on_handle (tool, coords, GIMP_ANCHOR_CONTROL,
                                        gdisp, &anchor, &stroke);
        }
      else
        {
          if (state & GDK_SHIFT_MASK && state & GDK_CONTROL_MASK)
            {
              gimp_stroke_anchor_convert (stroke, anchor,
                                          GIMP_ANCHOR_FEATURE_EDGE);
              vector_tool->cur_stroke = NULL;
              vector_tool->cur_anchor = NULL;
            }
        }

      vector_tool->function   = VECTORS_MOVING;
      vector_tool->cur_stroke = stroke;
      vector_tool->cur_anchor = anchor;

      gimp_draw_tool_resume (GIMP_DRAW_TOOL (vector_tool));
    }

  gimp_tool_control_activate (tool->control);
}

static void
gimp_vector_tool_button_release (GimpTool        *tool,
                                 GimpCoords      *coords,
                                 guint32          time,
                                 GdkModifierType  state,
                                 GimpDisplay     *gdisp)
{
  GimpVectorTool *vector_tool;
  GimpViewable   *viewable;

  vector_tool = GIMP_VECTOR_TOOL (tool);

  vector_tool->function = VECTORS_ADDING;

  /* THIS DOES NOT BELONG HERE! */
  if (vector_tool->vectors)
    {
      viewable = GIMP_VIEWABLE (vector_tool->vectors);
      gimp_viewable_invalidate_preview (viewable);
    }

  gimp_tool_control_halt (tool->control);
}

static void
gimp_vector_tool_motion (GimpTool        *tool,
                         GimpCoords      *coords,
                         guint32          time,
                         GdkModifierType  state,
                         GimpDisplay     *gdisp)
{
  GimpVectorTool    *vector_tool;
  GimpVectorOptions *options;
  GimpAnchor        *anchor;

  /* While moving:
   *         (SHFT) -> restrict movement
   */

  vector_tool = GIMP_VECTOR_TOOL (tool);
  options     = GIMP_VECTOR_OPTIONS (tool->tool_info->tool_options);

  gimp_vectors_freeze (vector_tool->vectors);

  if (state & GDK_SHIFT_MASK)
    {
      vector_tool->restriction = GIMP_ANCHOR_FEATURE_SYMMETRIC;
    }
  else
    {
      vector_tool->restriction = GIMP_ANCHOR_FEATURE_NONE;
    }

  switch (vector_tool->function)
    {
    case VECTORS_MOVING:
      anchor = vector_tool->cur_anchor;

      if (anchor)
        gimp_stroke_anchor_move_absolute (vector_tool->cur_stroke,
                                          vector_tool->cur_anchor,
                                          coords, vector_tool->restriction);

    default:
      break;
    }

  gimp_vectors_thaw (vector_tool->vectors);
}

static gboolean
gimp_vector_tool_on_handle (GimpTool        *tool,
                            GimpCoords      *coords,
                            GimpAnchorType   preferred,
                            GimpDisplay     *gdisp,
                            GimpAnchor     **ret_anchor,
                            GimpStroke     **ret_stroke)
{
  GimpVectorTool *vector_tool;
  GimpStroke     *stroke = NULL;
  GimpStroke     *pref_stroke = NULL;
  GimpAnchor     *anchor = NULL;
  GimpAnchor     *pref_anchor = NULL;
  GList          *list, *anchor_list = NULL;
  gdouble         dx, dy, pref_mindist = -1, mindist = -1;

  vector_tool = GIMP_VECTOR_TOOL (tool);

  if (!vector_tool->vectors)
    {
      if (ret_anchor)
        *ret_anchor = NULL;

      if (ret_stroke)
        *ret_stroke = NULL;

      return FALSE;
    }

  while ((stroke = gimp_vectors_stroke_get_next (vector_tool->vectors, stroke))
         != NULL)
    {
      anchor_list = gimp_stroke_get_draw_anchors (stroke);

      list = gimp_stroke_get_draw_controls (stroke);
      anchor_list = g_list_concat (anchor_list, list);

      while (anchor_list)
        {
          dx = coords->x - ((GimpAnchor *) anchor_list->data)->position.x;
          dy = coords->y - ((GimpAnchor *) anchor_list->data)->position.y;

          if (mindist < 0 || mindist > dx * dx + dy * dy)
            {
              mindist = dx * dx + dy * dy;
              anchor = (GimpAnchor *) anchor_list->data;
              if (ret_stroke)
                *ret_stroke = stroke;
            }

          if ((pref_mindist < 0 || pref_mindist > dx * dx + dy * dy) &&
              ((GimpAnchor *) anchor_list->data)->type == preferred)
            {
              pref_mindist = dx * dx + dy * dy;
              pref_anchor = (GimpAnchor *) anchor_list->data;
              pref_stroke = stroke;
            }

          anchor_list = anchor_list->next;
        }

      g_list_free (anchor_list);
    }

  /* If the data passed into ret_anchor is a preferred anchor, return
   * it.
   */
  if (ret_anchor && *ret_anchor
      && gimp_draw_tool_on_handle (GIMP_DRAW_TOOL (tool), gdisp,
                                   coords->x,
                                   coords->y,
                                   GIMP_HANDLE_CIRCLE,
                                   (*ret_anchor)->position.x,
                                   (*ret_anchor)->position.y,
                                   TARGET,
                                   TARGET,
                                   GTK_ANCHOR_CENTER,
                                   FALSE)
      && (*ret_anchor)->type == preferred)
    {
      if (ret_stroke)
        *ret_stroke = pref_stroke;
      return TRUE;
    }

  if (pref_anchor && gimp_draw_tool_on_handle (GIMP_DRAW_TOOL (tool), gdisp,
                                               coords->x,
                                               coords->y,
                                               GIMP_HANDLE_CIRCLE,
                                               pref_anchor->position.x,
                                               pref_anchor->position.y,
                                               TARGET,
                                               TARGET,
                                               GTK_ANCHOR_CENTER,
                                               FALSE))
    {
      if (ret_anchor)
        *ret_anchor = pref_anchor;
      if (ret_stroke)
        *ret_stroke = pref_stroke;
      return TRUE;
    }
  else if (anchor && gimp_draw_tool_on_handle (GIMP_DRAW_TOOL (tool), gdisp,
                                               coords->x,
                                               coords->y,
                                               GIMP_HANDLE_CIRCLE,
                                               anchor->position.x,
                                               anchor->position.y,
                                               TARGET,
                                               TARGET,
                                               GTK_ANCHOR_CENTER,
                                               FALSE))
    {
      if (ret_anchor)
        *ret_anchor = anchor;
        
      /* *ret_stroke already set correctly. */
      return TRUE;
    }
  else
    {
      if (ret_anchor)
        *ret_anchor = NULL;
      if (ret_stroke)
        *ret_stroke = NULL;
      return FALSE;
    }
}

static void
gimp_vector_tool_cursor_update (GimpTool        *tool,
                                GimpCoords      *coords,
                                GdkModifierType  state,
                                GimpDisplay     *gdisp)
{
  GimpVectorTool   *vector_tool;
  gboolean           in_handle = FALSE;
  GdkCursorType      ctype     = GIMP_MOUSE_CURSOR;
  GimpCursorModifier cmodifier = GIMP_CURSOR_MODIFIER_NONE;
  GimpAnchor        *anchor;

  vector_tool = GIMP_VECTOR_TOOL (tool);

  if (vector_tool->vectors &&
      gimp_tool_control_is_active (tool->control) && tool->gdisp == gdisp)
    {
      anchor = gimp_vectors_anchor_get (vector_tool->vectors, coords, NULL);

      if (anchor && gimp_draw_tool_on_handle (GIMP_DRAW_TOOL (tool), gdisp,
                                              coords->x,
                                              coords->y,
                                              GIMP_HANDLE_CIRCLE,
                                              anchor->position.x,
                                              anchor->position.y,
                                              TARGET,
                                              TARGET,
                                              GTK_ANCHOR_CENTER,
                                              FALSE))
        {
          in_handle = TRUE;
          cmodifier = GIMP_CURSOR_MODIFIER_MOVE;
        }
    }

  gimp_tool_control_set_cursor (tool->control, ctype);
  gimp_tool_control_set_cursor_modifier (tool->control, cmodifier);

  GIMP_TOOL_CLASS (parent_class)->cursor_update (tool, coords, state, gdisp);
}

static void
gimp_vector_tool_draw (GimpDrawTool *draw_tool)
{
  GimpVectorTool  *vector_tool;
  GimpTool        *tool;
  GimpAnchor      *cur_anchor = NULL;
  GimpStroke      *cur_stroke = NULL;
  GimpVectors     *vectors;
  GArray          *coords;
  gboolean         closed;
  GList           *draw_anchors;
  GList           *list;

  vector_tool = GIMP_VECTOR_TOOL (draw_tool);
  tool        = GIMP_TOOL (draw_tool);

  vectors = vector_tool->vectors;

  if (!vectors)
    return;

  while ((cur_stroke = gimp_vectors_stroke_get_next (vectors, cur_stroke)))
    {
      /* anchor handles */
      draw_anchors = gimp_stroke_get_draw_anchors (cur_stroke);

      for (list = draw_anchors; list; list = g_list_next (list))
        {
          cur_anchor = (GimpAnchor *) list->data;

          if (cur_anchor->type == GIMP_ANCHOR_ANCHOR)
            {
              gimp_draw_tool_draw_handle (draw_tool,
                                          cur_anchor->selected ?
                                          GIMP_HANDLE_CIRCLE :
                                          GIMP_HANDLE_FILLED_CIRCLE,
                                          cur_anchor->position.x,
                                          cur_anchor->position.y,
                                          TARGET,
                                          TARGET,
                                          GTK_ANCHOR_CENTER,
                                          FALSE);
            }
        }

      g_list_free (draw_anchors);

      /* control handles */
      draw_anchors = gimp_stroke_get_draw_controls (cur_stroke);

      for (list = draw_anchors; list; list = g_list_next (list))
        {
          cur_anchor = (GimpAnchor *) list->data;

          gimp_draw_tool_draw_handle (draw_tool,
                                      GIMP_HANDLE_SQUARE,
                                      cur_anchor->position.x,
                                      cur_anchor->position.y,
                                      TARGET - 3,
                                      TARGET - 3,
                                      GTK_ANCHOR_CENTER,
                                      FALSE);
        }

      g_list_free (draw_anchors);

      /* the lines to the control handles */
      coords = gimp_stroke_get_draw_lines (cur_stroke);
      
      if (coords->len % 2 == 0)
        {
          gint i;

          for (i = 0; i < coords->len; i += 2)
            gimp_draw_tool_draw_strokes (draw_tool,
                                         &g_array_index (coords,
                                                         GimpCoords, i),
                                         2, FALSE, FALSE);
        }

      g_array_free (coords, TRUE);

      /* the stroke itself */
      coords = gimp_stroke_interpolate (cur_stroke, 1.0, &closed);

      if (coords->len)
        gimp_draw_tool_draw_strokes (draw_tool,
                                     &g_array_index (coords, GimpCoords, 0),
                                     coords->len, FALSE, FALSE);

      g_array_free (coords, TRUE);
    }
}

static void
gimp_vector_tool_clear_vectors (GimpVectorTool *vector_tool)
{
  g_return_if_fail (GIMP_IS_VECTOR_TOOL (vector_tool));

  gimp_vector_tool_set_vectors (vector_tool, NULL);
}

static void
gimp_vector_tool_vectors_freeze (GimpVectors    *vectors,
                                 GimpVectorTool *vector_tool)
{
  GimpDrawTool *draw_tool;

  draw_tool = GIMP_DRAW_TOOL (vector_tool);

  g_print ("vectors freeze\n");

  if (draw_tool->gdisp)
    gimp_draw_tool_pause (draw_tool);
}

static void
gimp_vector_tool_vectors_thaw (GimpVectors    *vectors,
                               GimpVectorTool *vector_tool)
{
  GimpDrawTool *draw_tool;

  draw_tool = GIMP_DRAW_TOOL (vector_tool);

  g_print ("vectors thaw\n");

  if (draw_tool->gdisp)
    gimp_draw_tool_resume (draw_tool);
}

void
gimp_vector_tool_set_vectors (GimpVectorTool *vector_tool,
                              GimpVectors    *vectors)
{
  GimpDrawTool *draw_tool;
  GimpTool     *tool;
  GimpItem     *item = NULL;

  g_return_if_fail (GIMP_IS_VECTOR_TOOL (vector_tool));
  g_return_if_fail (vectors == NULL || GIMP_IS_VECTORS (vectors));

  draw_tool = GIMP_DRAW_TOOL (vector_tool);
  tool      = GIMP_TOOL (vector_tool);

  if (vectors)
    item = GIMP_ITEM (vectors);

  if (vectors == vector_tool->vectors)
    return;

  if (draw_tool->gdisp)
    {
      if (vectors && draw_tool->gdisp->gimage == item->gimage)
        gimp_draw_tool_pause (draw_tool);
      else
        gimp_draw_tool_stop (draw_tool);
    }

  if (vector_tool->vectors)
    {
      g_signal_handlers_disconnect_by_func (vector_tool->vectors,
                                            gimp_vector_tool_clear_vectors,
                                            vector_tool);
      g_signal_handlers_disconnect_by_func (vector_tool->vectors,
                                            gimp_vector_tool_vectors_freeze,
                                            vector_tool);
      g_signal_handlers_disconnect_by_func (vector_tool->vectors,
                                            gimp_vector_tool_vectors_thaw,
                                            vector_tool);
      g_object_unref (vector_tool->vectors);
    }

  vector_tool->vectors        = vectors;
  vector_tool->cur_stroke     = NULL;
  vector_tool->cur_anchor     = NULL;
  vector_tool->active_anchors = NULL;
  vector_tool->function       = VECTORS_CREATING;

  if (! vector_tool->vectors)
    {
      tool->gdisp = NULL;
      gimp_tool_control_halt (tool->control);

      return;
    }

  g_object_ref (vectors);

  g_signal_connect_object (vectors, "removed", 
                           G_CALLBACK (gimp_vector_tool_clear_vectors),
                           vector_tool,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (vectors, "freeze",
                           G_CALLBACK (gimp_vector_tool_vectors_freeze),
                           vector_tool,
                           0);
  g_signal_connect_object (vectors, "thaw",
                           G_CALLBACK (gimp_vector_tool_vectors_thaw),
                           vector_tool,
                           0);

  if (draw_tool->gdisp && draw_tool->gdisp->gimage == item->gimage)
    {
      gimp_draw_tool_resume (draw_tool);
    }
  else if (tool->gdisp && tool->gdisp->gimage == item->gimage)
    {
      gimp_draw_tool_start (draw_tool, tool->gdisp);
    }
  else
    {
      GimpContext *context;
      GimpDisplay *gdisp;

      context = gimp_get_current_context (tool->tool_info->gimp);
      gdisp   = gimp_context_get_display (context);

      if (! gdisp || gdisp->gimage != item->gimage)
        {
          GList *list;

          gdisp = NULL;

          for (list = GIMP_LIST (item->gimage->gimp->displays)->list;
               list;
               list = g_list_next (list))
            {
              if (((GimpDisplay *) list->data)->gimage == item->gimage)
                {
                  gimp_context_set_display (context,
                                            (GimpDisplay *) list->data);

                  gdisp = gimp_context_get_display (context);
                  break;
                }
            }
        }

      if (gdisp)
        {
          tool->gdisp = gdisp;
          gimp_tool_control_activate (tool->control);

          gimp_draw_tool_start (draw_tool, tool->gdisp);
        }
    }
}
