/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * vectors-types.h
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

#ifndef __VECTORS_TYPES_H__
#define __VECTORS_TYPES_H__

#include "core/core-types.h"


typedef enum
{
  GIMP_ANCHOR_ANCHOR,
  GIMP_ANCHOR_CONTROL
} GimpAnchorType;

typedef enum
{
  GIMP_ANCHOR_FEATURE_NONE,
  GIMP_ANCHOR_FEATURE_EDGE,
  GIMP_ANCHOR_FEATURE_ALIGNED,
  GIMP_ANCHOR_FEATURE_SYMMETRIC
} GimpAnchorFeatureType;

typedef enum
{
  EXTEND_SIMPLE,
  EXTEND_EDITABLE
} GimpVectorExtendMode;


typedef struct _GimpAnchor       GimpAnchor;

typedef struct _GimpVectors      GimpVectors;
typedef struct _GimpStroke       GimpStroke;
typedef struct _GimpBezierStroke GimpBezierStroke;


#endif /* __VECTORS_TYPES_H__ */
