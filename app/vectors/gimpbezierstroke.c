/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpstroke.c
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

#include <glib-object.h>

#include "vectors-types.h"

#include "gimpanchor.h"
#include "gimpbezierstroke.h"


#define INPUT_RESOLUTION 256


/* local prototypes */

static void gimp_bezier_stroke_class_init (GimpBezierStrokeClass *klass);
static void gimp_bezier_stroke_init       (GimpBezierStroke      *bezier_stroke);
static void gimp_bezier_stroke_anchor_move_relative (GimpStroke        *stroke,
                                                     GimpAnchor        *anchor,
                                                     const GimpCoords  *deltacoord,
                                                     GimpAnchorFeatureType feature);
static void gimp_bezier_stroke_anchor_move_absolute (GimpStroke        *stroke,
                                                     GimpAnchor        *anchor,
                                                     const GimpCoords  *coord,
                                                     GimpAnchorFeatureType feature);

static void gimp_bezier_stroke_anchor_convert (GimpStroke            *stroke,
                                               GimpAnchor            *anchor,
                                               GimpAnchorFeatureType  feature);
static GArray *   gimp_bezier_stroke_interpolate (const GimpStroke  *stroke,
                                                  const gdouble      precision,
                                                  gboolean          *closed);

static void gimp_bezier_stroke_finalize   (GObject               *object);

static void gimp_bezier_coords_mix        (const gdouble          amul,
                                           const GimpCoords      *a,
                                           const gdouble          bmul,
                                           const GimpCoords      *b,
                                           GimpCoords            *ret_val);
static void gimp_bezier_coords_average    (const GimpCoords      *a,
                                           const GimpCoords      *b,
                                           GimpCoords            *ret_average);
static void gimp_bezier_coords_add        (const GimpCoords      *a,
                                           const GimpCoords      *b,
                                           GimpCoords            *ret_add);
static void gimp_bezier_coords_difference (const GimpCoords      *a,
                                           const GimpCoords      *b,
                                           GimpCoords            *difference);
static void gimp_bezier_coords_scale      (const gdouble          f,
                                           const GimpCoords      *a,
                                           GimpCoords            *ret_multiply);
static void gimp_bezier_coords_subdivide  (const GimpCoords      *beziercoords,
                                           const gdouble          precision,
                                           GArray               **ret_coords);
static void gimp_bezier_coords_subdivide2 (const GimpCoords      *beziercoords,
                                           const gdouble          precision,
                                           GArray               **ret_coords,
                                           gint                   depth);

static gdouble  gimp_bezier_coords_scalarprod  (const GimpCoords *a,
                                                const GimpCoords *b);
static gdouble  gimp_bezier_coords_length      (const GimpCoords *a);
static gdouble  gimp_bezier_coords_length2     (const GimpCoords *a);
static gboolean gimp_bezier_coords_is_straight (const GimpCoords *beziercoords,
                                                const gdouble     precision);


/*  private variables  */

static GimpStrokeClass *parent_class = NULL;


GType
gimp_bezier_stroke_get_type (void)
{
  static GType bezier_stroke_type = 0;

  if (! bezier_stroke_type)
    {
      static const GTypeInfo bezier_stroke_info =
      {
        sizeof (GimpBezierStrokeClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_bezier_stroke_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpBezierStroke),
        0,              /* n_preallocs    */
        (GInstanceInitFunc) gimp_bezier_stroke_init,
      };

      bezier_stroke_type = g_type_register_static (GIMP_TYPE_STROKE,
                                                   "GimpBezierStroke", 
                                                   &bezier_stroke_info, 0);
    }

  return bezier_stroke_type;
}

static void
gimp_bezier_stroke_class_init (GimpBezierStrokeClass *klass)
{
  GObjectClass    *object_class;
  GimpStrokeClass *stroke_class;

  object_class = G_OBJECT_CLASS (klass);
  stroke_class = GIMP_STROKE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize             = gimp_bezier_stroke_finalize;

  stroke_class->anchor_move_relative = gimp_bezier_stroke_anchor_move_relative;
  stroke_class->anchor_move_absolute = gimp_bezier_stroke_anchor_move_absolute;
  stroke_class->anchor_convert       = gimp_bezier_stroke_anchor_convert;
  stroke_class->interpolate          = gimp_bezier_stroke_interpolate;
}

static void
gimp_bezier_stroke_init (GimpBezierStroke *bezier_stroke)
{
  /* pass */
}

static void
gimp_bezier_stroke_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* Bezier specific functions */

GimpStroke *
gimp_bezier_stroke_new (void)
{
  GimpBezierStroke *bezier_stroke;
  GimpStroke       *stroke;

  bezier_stroke = g_object_new (GIMP_TYPE_BEZIER_STROKE, NULL);

  stroke = GIMP_STROKE (bezier_stroke);

  stroke->anchors = NULL;
  stroke->closed = FALSE;
  return stroke;
}


GimpStroke *
gimp_bezier_stroke_new_from_coords (const GimpCoords *coords,
                                    gint              n_coords,
                                    gboolean          closed)
{
  GimpBezierStroke *bezier_stroke;
  GimpStroke       *stroke;
  GimpAnchor       *last_anchor;
  gint              count;

  g_return_val_if_fail (coords != NULL, NULL);
  g_return_val_if_fail (n_coords >= 3, NULL);
  g_return_val_if_fail ((n_coords % 3) == 0, NULL);

  stroke = gimp_bezier_stroke_new ();

  bezier_stroke = GIMP_BEZIER_STROKE (stroke);

  last_anchor = NULL;

  count = 0;

  while (count < n_coords)
    last_anchor = gimp_bezier_stroke_extend (bezier_stroke,
                                             &coords[count++],
                                             last_anchor,
                                             EXTEND_SIMPLE);

  stroke->closed = closed ? TRUE : FALSE;

  return stroke;
}


GimpAnchor *
gimp_bezier_stroke_extend (GimpBezierStroke     *bezier_stroke,
                           const GimpCoords     *coords,
                           GimpAnchor           *neighbor,
                           GimpVectorExtendMode  extend_mode)
{
  GimpAnchor *anchor = NULL;
  GimpStroke *stroke;
  GList      *listneighbor;
  gint        loose_end, control_count;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (bezier_stroke), NULL);

  stroke = GIMP_STROKE (bezier_stroke);

  if (stroke->anchors == NULL)
    {
      /* assure that there is no neighbor specified */
      g_return_val_if_fail (neighbor == NULL, NULL);

      anchor = g_new0 (GimpAnchor, 1);
      anchor->position = *coords;
      anchor->type = GIMP_ANCHOR_CONTROL;

      stroke->anchors = g_list_append (stroke->anchors, anchor);

      switch (extend_mode)
        {
        case EXTEND_SIMPLE:
          break;

        case EXTEND_EDITABLE:
          anchor = gimp_bezier_stroke_extend (bezier_stroke,
                                              coords, anchor,
                                              EXTEND_SIMPLE);

          gimp_stroke_anchor_select (stroke, anchor, TRUE);

          anchor = gimp_bezier_stroke_extend (bezier_stroke,
                                              coords, anchor,
                                              EXTEND_SIMPLE);

          break;
        default:
          anchor = NULL;
        }
      return anchor;
    }
  else
    {
      /* assure that there is a neighbor specified */
      g_return_val_if_fail (neighbor != NULL, NULL);

      loose_end = 0;
      listneighbor = g_list_last (stroke->anchors);

      /* Check if the neighbor is at an end of the control points */
      if (listneighbor->data == neighbor)
        {
          loose_end = 1;
        }
      else
        {
          listneighbor = g_list_first (stroke->anchors);
          if (listneighbor->data == neighbor)
            {
              loose_end = -1;
            }
          else
            {
              /*
               * it isnt. if we are on a handle go to the nearest
               * anchor and see if we can find an end from it.
               * Yes, this is tedious.
               */

              listneighbor = g_list_find (stroke->anchors, neighbor);

              if (listneighbor && neighbor->type == GIMP_ANCHOR_CONTROL)
                {
                  if (listneighbor->prev &&
                      ((GimpAnchor *) listneighbor->prev->data)->type == GIMP_ANCHOR_ANCHOR)
                    {
                      listneighbor = listneighbor->prev;
                    }
                  else if (listneighbor->next &&
                           ((GimpAnchor *) listneighbor->next->data)->type == GIMP_ANCHOR_ANCHOR)
                    {
                      listneighbor = listneighbor->next;
                    }
                  else
                    {
                      loose_end = 0;
                      listneighbor = NULL;
                    }
                }

              if (listneighbor)
                /* we found a suitable ANCHOR_ANCHOR now, lets
                 * search for its loose end.
                 */
                {
                  if (listneighbor->prev &&
                      listneighbor->prev->prev == NULL)
                    {
                      loose_end = -1;
                      listneighbor = listneighbor->prev;
                    }
                  else if (listneighbor->next &&
                           listneighbor->next->next == NULL)
                    {
                      loose_end = 1;
                      listneighbor = listneighbor->next;
                    }
                }
            }
        }

      if (loose_end)
        {
          /* We have to detect the type of the point to add... */

          control_count = 0;

          if (loose_end == 1)
            {
              while (listneighbor &&
                     ((GimpAnchor *) listneighbor->data)->type == GIMP_ANCHOR_CONTROL)
                {
                  control_count++;
                  listneighbor = listneighbor->prev;
                }
            }
          else
            {
              while (listneighbor &&
                     ((GimpAnchor *) listneighbor->data)->type == GIMP_ANCHOR_CONTROL)
                {
                  control_count++;
                  listneighbor = listneighbor->next;
                }
            }

          switch (extend_mode)
            {
            case EXTEND_SIMPLE:
              anchor = g_new0 (GimpAnchor, 1);
              anchor->position = *coords;

              switch (control_count)
                {
                case 0:
                  anchor->type = GIMP_ANCHOR_CONTROL;
                  break;
                case 1:
                  if (listneighbor)  /* only one handle in the path? */
                    anchor->type = GIMP_ANCHOR_CONTROL;
                  else
                    anchor->type = GIMP_ANCHOR_ANCHOR;
                  break;
                case 2:
                  anchor->type = GIMP_ANCHOR_ANCHOR;
                  break;
                default:
                  g_printerr ("inconsistent bezier curve: "
                              "%d successive control handles", control_count);
                }

              if (loose_end == 1)
                stroke->anchors = g_list_append (stroke->anchors, anchor);

              if (loose_end == -1)
                stroke->anchors = g_list_prepend (stroke->anchors, anchor);
              break;

            case EXTEND_EDITABLE:
              switch (control_count)
                {
                case 0:
                  neighbor = gimp_bezier_stroke_extend (bezier_stroke,
                                                        &(neighbor->position),
                                                        neighbor,
                                                        EXTEND_SIMPLE);
                case 1:
                  neighbor = gimp_bezier_stroke_extend (bezier_stroke,
                                                        coords,
                                                        neighbor,
                                                        EXTEND_SIMPLE);
                case 2:
                  neighbor = gimp_bezier_stroke_extend (bezier_stroke,
                                                        coords,
                                                        neighbor,
                                                        EXTEND_SIMPLE);

                  gimp_stroke_anchor_select (stroke, neighbor, TRUE);

                  anchor = gimp_bezier_stroke_extend (bezier_stroke,
                                                      coords,
                                                      neighbor,
                                                      EXTEND_SIMPLE);
                  break;
                default:
                  g_printerr ("inconsistent bezier curve: "
                              "%d successive control handles", control_count);
                }
            }

          return anchor;
        }
      return NULL;
    }
}


static void
gimp_bezier_stroke_anchor_move_relative (GimpStroke            *stroke,
                                         GimpAnchor            *anchor,
                                         const GimpCoords      *deltacoord,
                                         GimpAnchorFeatureType  feature)
{
  GimpCoords  delta, coord1, coord2;
  GList      *anchor_list;

  delta = *deltacoord;
  delta.pressure = 0;
  delta.xtilt    = 0;
  delta.ytilt    = 0;
  delta.wheel    = 0;

  gimp_bezier_coords_add (&(anchor->position), &delta, &coord1);
  anchor->position = coord1;

  anchor_list = g_list_find (stroke->anchors, anchor);
  g_return_if_fail (anchor_list != NULL);

  if (anchor->type == GIMP_ANCHOR_ANCHOR)
    {
      if (g_list_previous (anchor_list))
        {
          coord2 = ((GimpAnchor *) g_list_previous (anchor_list)->data)->position;
          gimp_bezier_coords_add (&coord2, &delta, &coord1);
          ((GimpAnchor *) g_list_previous (anchor_list)->data)->position =
              coord1;
        }

      if (g_list_next (anchor_list))
        {
          coord2 = ((GimpAnchor *) g_list_next (anchor_list)->data)->position;
          gimp_bezier_coords_add (&coord2, &delta, &coord1);
          ((GimpAnchor *) g_list_next (anchor_list)->data)->position = coord1;
        }
    }
  else
    {
      if (feature == GIMP_ANCHOR_FEATURE_SYMMETRIC)
        {
          GList *neighbour = NULL, *opposite = NULL;

          /* search for opposite control point. Sigh. */
          neighbour = g_list_previous (anchor_list);
          if (neighbour &&
              ((GimpAnchor *) neighbour->data)->type == GIMP_ANCHOR_ANCHOR)
            {
              opposite = g_list_previous (neighbour);
            }
          else
            {
              neighbour = g_list_next (anchor_list);
              if (neighbour &&
                  ((GimpAnchor *) neighbour->data)->type == GIMP_ANCHOR_ANCHOR)
                {
                  opposite = g_list_next (neighbour);
                }
            }
          if (opposite &&
              ((GimpAnchor *) opposite->data)->type == GIMP_ANCHOR_CONTROL)
            {
              gimp_bezier_coords_difference (&(((GimpAnchor *) neighbour->data)->position),
                                             &(anchor->position), &delta);
              gimp_bezier_coords_add (&(((GimpAnchor *) neighbour->data)->position),
                                      &delta, &coord1);
              ((GimpAnchor *) opposite->data)->position = coord1;
            }
        }
    }
}


static void
gimp_bezier_stroke_anchor_move_absolute (GimpStroke            *stroke,
                                         GimpAnchor            *anchor,
                                         const GimpCoords      *coord,
                                         GimpAnchorFeatureType  feature)
{
  GimpCoords deltacoord;

  gimp_bezier_coords_difference (coord, &anchor->position, &deltacoord);
  gimp_bezier_stroke_anchor_move_relative (stroke, anchor,
                                           &deltacoord, feature);
}

static void
gimp_bezier_stroke_anchor_convert (GimpStroke            *stroke,
                                   GimpAnchor            *anchor,
                                   GimpAnchorFeatureType  feature)
{
  GList *anchor_list;

  anchor_list = g_list_find (stroke->anchors, anchor);

  g_return_if_fail (anchor_list != NULL);

  switch (feature)
    {
    case GIMP_ANCHOR_FEATURE_EDGE:
      if (anchor->type == GIMP_ANCHOR_ANCHOR)
        {
          if (g_list_previous (anchor_list))
            ((GimpAnchor *) g_list_previous (anchor_list)->data)->position =
              anchor->position;

          if (g_list_next (anchor_list))
            ((GimpAnchor *) g_list_next (anchor_list)->data)->position =
              anchor->position;
        }
      else
        {
          if (g_list_previous (anchor_list) &&
              ((GimpAnchor *) g_list_previous (anchor_list)->data)->type == GIMP_ANCHOR_ANCHOR)
            anchor->position = ((GimpAnchor *) g_list_previous (anchor_list)->data)->position;
          if (g_list_next (anchor_list) &&
              ((GimpAnchor *) g_list_next (anchor_list)->data)->type == GIMP_ANCHOR_ANCHOR)
            anchor->position = ((GimpAnchor *) g_list_next (anchor_list)->data)->position;
        }

      break;

    default:
      g_printerr ("gimp_bezier_stroke_anchor_convert: "
                  "unimplemented anchor conversion %d\n", feature);
    }
}

static GArray *
gimp_bezier_stroke_interpolate (const GimpStroke  *stroke,
                                gdouble            precision,
                                gboolean          *ret_closed)
{
  GArray     *ret_coords;
  GimpAnchor *anchor;
  GList      *anchorlist;
  GimpCoords  segmentcoords[4];
  gint        count;

  g_return_val_if_fail (GIMP_IS_BEZIER_STROKE (stroke), NULL);
  g_return_val_if_fail (ret_closed != NULL, NULL);
  
  if (!stroke->anchors)
    {
      *ret_closed = FALSE;
      return NULL;
    }

  ret_coords = g_array_new (FALSE, FALSE, sizeof (GimpCoords));

  count = 0;

  for (anchorlist = stroke->anchors;
       anchorlist && ((GimpAnchor *) anchorlist->data)->type != GIMP_ANCHOR_ANCHOR;
       anchorlist = g_list_next (anchorlist));

  for ( ; anchorlist; anchorlist = g_list_next (anchorlist))
    {
      anchor = anchorlist->data;

      segmentcoords[count] = anchor->position;
      count++;

      if (count == 4)
        {
          gimp_bezier_coords_subdivide (segmentcoords, precision, &ret_coords);
          segmentcoords[0] = segmentcoords[3];
          count = 1;
        }
    }

  if (stroke->closed && stroke->anchors)
    {
      anchorlist = stroke->anchors;

      while (count < 3)
        {
          segmentcoords[count] = ((GimpAnchor *) anchorlist->data)->position;
          count++;
        }
      anchorlist = g_list_next (anchorlist);
      if (anchorlist)
        segmentcoords[3] = ((GimpAnchor *) anchorlist->data)->position;

      gimp_bezier_coords_subdivide (segmentcoords, precision, &ret_coords);
      
    }

  ret_coords = g_array_append_val (ret_coords, segmentcoords[3]);

  *ret_closed = stroke->closed;

  return ret_coords;
}


/* local helper functions for bezier subdivision */

/*   amul * a + bmul * b   */

static void
gimp_bezier_coords_mix (const gdouble     amul,
                        const GimpCoords *a,
                        const gdouble     bmul,
                        const GimpCoords *b,
                        GimpCoords       *ret_val)
{
  if (b)
    {
      ret_val->x        = amul * a->x        + bmul * b->x ;
      ret_val->y        = amul * a->y        + bmul * b->y ;
      ret_val->pressure = amul * a->pressure + bmul * b->pressure ;
      ret_val->xtilt    = amul * a->xtilt    + bmul * b->xtilt ;
      ret_val->ytilt    = amul * a->ytilt    + bmul * b->ytilt ;
      ret_val->wheel    = amul * a->wheel    + bmul * b->wheel ;
    }
  else
    {
      ret_val->x        = amul * a->x;
      ret_val->y        = amul * a->y;
      ret_val->pressure = amul * a->pressure;
      ret_val->xtilt    = amul * a->xtilt;
      ret_val->ytilt    = amul * a->ytilt;
      ret_val->wheel    = amul * a->wheel;
    }
}

                        
/*    (a+b)/2   */

static void
gimp_bezier_coords_average (const GimpCoords *a,
                            const GimpCoords *b,
                            GimpCoords       *ret_average)
{
  gimp_bezier_coords_mix (0.5, a, 0.5, b, ret_average);
}


/* a + b */

static void
gimp_bezier_coords_add (const GimpCoords *a,
                        const GimpCoords *b,
                        GimpCoords       *ret_add)
{
  gimp_bezier_coords_mix (1.0, a, 1.0, b, ret_add);
}


/* a - b */

static void
gimp_bezier_coords_difference (const GimpCoords *a,
                               const GimpCoords *b,
                               GimpCoords       *ret_difference)
{
  gimp_bezier_coords_mix (1.0, a, -1.0, b, ret_difference);
}


/* a * f = ret_product */

static void
gimp_bezier_coords_scale (const gdouble     f,
                          const GimpCoords *a,
                          GimpCoords       *ret_multiply)
{
  gimp_bezier_coords_mix (f, a, 0.0, NULL, ret_multiply);
}


/* local helper for measuring the scalarproduct of two gimpcoords. */

static gdouble
gimp_bezier_coords_scalarprod (const GimpCoords *a,
                               const GimpCoords *b)
{
  return (a->x        * b->x        +
          a->y        * b->y        +
          a->pressure * b->pressure +
          a->xtilt    * b->xtilt    +
          a->ytilt    * b->ytilt    +
          a->wheel    * b->wheel   );
}


/*
 * The "lenght" of the gimpcoord.
 * Applies a metric that increases the weight on the
 * pressure/xtilt/ytilt/wheel to ensure proper interpolation
 */

static gdouble
gimp_bezier_coords_length2 (const GimpCoords *a)
{
  GimpCoords upscaled_a;

  upscaled_a.x        = a->x;
  upscaled_a.y        = a->y;
  upscaled_a.pressure = a->pressure * INPUT_RESOLUTION;
  upscaled_a.xtilt    = a->xtilt    * INPUT_RESOLUTION;
  upscaled_a.ytilt    = a->ytilt    * INPUT_RESOLUTION;
  upscaled_a.wheel    = a->wheel    * INPUT_RESOLUTION;

  return gimp_bezier_coords_scalarprod (&upscaled_a, &upscaled_a);
}


static gdouble
gimp_bezier_coords_length (const GimpCoords *a)
{
  return sqrt (gimp_bezier_coords_length2 (a));
}


/*
 * a helper function that determines if a bezier segment is "straight
 * enough" to be approximated by a line.
 * 
 * Needs four GimpCoords in an array.
 */

static gboolean
gimp_bezier_coords_is_straight (const GimpCoords *beziercoords,
                                gdouble           precision)
{
  GimpCoords line, tan1, tan2, d1, d2;
  gdouble    l2, s1, s2;

  gimp_bezier_coords_difference (&(beziercoords[3]),
                                 &(beziercoords[0]),
                                 &line);

  if (gimp_bezier_coords_length2 (&line) < precision * precision)
    {
      gimp_bezier_coords_difference (&(beziercoords[1]),
                                     &(beziercoords[0]),
                                     &tan1);
      gimp_bezier_coords_difference (&(beziercoords[2]),
                                     &(beziercoords[3]),
                                     &tan2);
      if ((gimp_bezier_coords_length2 (&tan1) < precision * precision) &&
          (gimp_bezier_coords_length2 (&tan2) < precision * precision))
        {
          return 1;
        }
      else
        {
          /* Tangents are too big for the small baseline */
          /* g_printerr ("Zu grosse Tangenten bei zu kleiner Basislinie\n"); */
          return 0;
        }
    }
  else
    {
      gimp_bezier_coords_difference (&(beziercoords[1]),
                                     &(beziercoords[0]),
                                     &tan1);
      gimp_bezier_coords_difference (&(beziercoords[2]),
                                     &(beziercoords[0]),
                                     &tan2);

      l2 = gimp_bezier_coords_scalarprod (&line, &line);
      s1 = gimp_bezier_coords_scalarprod (&line, &tan1) / l2;
      s2 = gimp_bezier_coords_scalarprod (&line, &tan2) / l2;

      if (s1 < 0 || s1 > 1 || s2 < 0 || s2 > 1 || s2 < s1)
        {
          /* The tangents get projected outside the baseline */
          /* g_printerr ("Tangenten projezieren sich ausserhalb der Basisline\n"); */
          return 0;
        }

      gimp_bezier_coords_mix (1.0, &tan1, - s1, &line, &d1);
      gimp_bezier_coords_mix (1.0, &tan2, - s2, &line, &d2);

      if ((gimp_bezier_coords_length2 (&d1) > precision * precision) ||
          (gimp_bezier_coords_length2 (&d2) > precision * precision))
        {
          /* The control points are too far away from the baseline */
          /* g_printerr ("Zu grosser Abstand zur Basislinie\n"); */
          return 0;
        }

      return 1;
    }
}


static void
gimp_bezier_coords_subdivide2 (const GimpCoords *beziercoords,
                               const gdouble     precision,
                               GArray          **ret_coords,
                               gint              depth)
{
  /*
   * beziercoords has to contain four GimpCoords with the four control points
   * of the bezier segment. We subdivide it at the parameter 0.5.
   */

  GimpCoords subdivided[8];

  subdivided[0] = beziercoords[0];
  subdivided[6] = beziercoords[3];

  if (!depth) g_printerr ("Hit rekursion depth limit!\n");

  gimp_bezier_coords_average (&(beziercoords[0]), &(beziercoords[1]),
                              &(subdivided[1]));

  gimp_bezier_coords_average (&(beziercoords[1]), &(beziercoords[2]),
                              &(subdivided[7]));

  gimp_bezier_coords_average (&(beziercoords[2]), &(beziercoords[3]),
                              &(subdivided[5]));

  gimp_bezier_coords_average (&(subdivided[1]), &(subdivided[7]),
                              &(subdivided[2]));

  gimp_bezier_coords_average (&(subdivided[7]), &(subdivided[5]),
                              &(subdivided[4]));

  gimp_bezier_coords_average (&(subdivided[2]), &(subdivided[4]),
                              &(subdivided[3]));

  /*
   * We now have the coordinates of the two bezier segments in
   * subdivided [0-3] and subdivided [3-6]
   */

  /*
   * Here we need to check, if we have sufficiently subdivided, i.e.
   * if the stroke is sufficiently close to a straight line.
   */

  if (!depth || gimp_bezier_coords_is_straight (&(subdivided[0]),
                                                precision)) /* 1st half */
    {
      *ret_coords = g_array_append_vals (*ret_coords, &(subdivided[0]), 3);
    }
  else
    {
      gimp_bezier_coords_subdivide2 (&(subdivided[0]), precision,
                                     ret_coords, depth-1);
    }

  if (!depth || gimp_bezier_coords_is_straight (&(subdivided[3]),
                                                precision)) /* 2nd half */
    {
      *ret_coords = g_array_append_vals (*ret_coords, &(subdivided[3]), 3);
    }
  else
    {
      gimp_bezier_coords_subdivide2 (&(subdivided[3]), precision,
                                     ret_coords, depth-1);
    }
  
  /* g_printerr ("gimp_bezier_coords_subdivide end: %d entries\n", (*ret_coords)->len); */
}


static void
gimp_bezier_coords_subdivide (const GimpCoords  *beziercoords,
                              const gdouble      precision,
                              GArray           **ret_coords)
{
  gimp_bezier_coords_subdivide2 (beziercoords, precision, ret_coords, 10);
}
