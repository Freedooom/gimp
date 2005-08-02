/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpdrawabletransform_pdb.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* NOTE: This file is autogenerated by pdbgen.pl */

#include "config.h"

#include "gimp.h"

/**
 * gimp_drawable_transform_flip_simple:
 * @drawable_ID: The affected drawable.
 * @flip_type: Type of flip.
 * @auto_center: Whether to automatically position the axis in the selection center.
 * @axis: coord. of flip axis.
 * @clip_result: Whether to clip results.
 *
 * Flip the specified drawable either vertically or horizontally.
 *
 * This procedure flips the specified drawable if no selection exists.
 * If a selection exists, the portion of the drawable which lies under
 * the selection is cut from the drawable and made into a floating
 * selection which is then flipped. If auto_center is set to true, the
 * flip is around the selection's center. Otherwise, the coordinate of
 * the axis needs to be specified. The return value is the ID of the
 * flipped drawable. If there was no selection, this will be equal to
 * the drawable ID supplied as input. Otherwise, this will be the newly
 * created and flipped drawable.
 *
 * Returns: The flipped drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_flip_simple (gint32              drawable_ID,
				     GimpOrientationType flip_type,
				     gboolean            auto_center,
				     gdouble             axis,
				     gboolean            clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-flip-simple",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_INT32, flip_type,
				    GIMP_PDB_INT32, auto_center,
				    GIMP_PDB_FLOAT, axis,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_flip:
 * @drawable_ID: The affected drawable.
 * @x0: horz. coord. of one end of axis.
 * @y0: vert. coord. of one end of axis.
 * @x1: horz. coord. of other end of axis.
 * @y1: vert. coord. of other end of axis.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Flip the specified drawable around a given line.
 *
 * This procedure flips the specified drawable if no selection exists.
 * If a selection exists, the portion of the drawable which lies under
 * the selection is cut from the drawable and made into a floating
 * selection which is then flipped. The axis to flip around is
 * specified by specifying two points from that line. The return value
 * is the ID of the flipped drawable. If there was no selection, this
 * will be equal to the drawable ID supplied as input. Otherwise, this
 * will be the newly created and flipped drawable. The clip results
 * parameter specifies wheter current selection will affect the
 * transform.
 *
 * Returns: The flipped drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_flip (gint32                 drawable_ID,
			      gdouble                x0,
			      gdouble                y0,
			      gdouble                x1,
			      gdouble                y1,
			      GimpTransformDirection transform_direction,
			      GimpInterpolationType  interpolation,
			      gboolean               supersample,
			      gint                   recursion_level,
			      gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-flip",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, x0,
				    GIMP_PDB_FLOAT, y0,
				    GIMP_PDB_FLOAT, x1,
				    GIMP_PDB_FLOAT, y1,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_flip_default:
 * @drawable_ID: The affected drawable.
 * @x0: horz. coord. of one end of axis.
 * @y0: vert. coord. of one end of axis.
 * @x1: horz. coord. of other end of axis.
 * @y1: vert. coord. of other end of axis.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Flip the specified drawable around a given line.
 *
 * This procedure is a variant of gimp_drawable_transform_flip() which
 * uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The flipped drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_flip_default (gint32   drawable_ID,
				      gdouble  x0,
				      gdouble  y0,
				      gdouble  x1,
				      gdouble  y1,
				      gboolean interpolate,
				      gboolean clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-flip-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, x0,
				    GIMP_PDB_FLOAT, y0,
				    GIMP_PDB_FLOAT, x1,
				    GIMP_PDB_FLOAT, y1,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_perspective:
 * @drawable_ID: The affected drawable.
 * @x0: The new x coordinate of upper-left corner of original bounding box.
 * @y0: The new y coordinate of upper-left corner of original bounding box.
 * @x1: The new x coordinate of upper-right corner of original bounding box.
 * @y1: The new y coordinate of upper-right corner of original bounding box.
 * @x2: The new x coordinate of lower-left corner of original bounding box.
 * @y2: The new y coordinate of lower-left corner of original bounding box.
 * @x3: The new x coordinate of lower-right corner of original bounding box.
 * @y3: The new y coordinate of lower-right corner of original bounding box.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Perform a possibly non-affine transformation on the specified
 * drawable, with extra parameters.
 *
 * This procedure performs a possibly non-affine transformation on the
 * specified drawable by allowing the corners of the original bounding
 * box to be arbitrarily remapped to any values. The specified drawable
 * is remapped if no selection exists. However, if a selection exists,
 * the portion of the drawable which lies under the selection is cut
 * from the drawable and made into a floating selection which is then
 * remapped as specified. The return value is the ID of the remapped
 * drawable. If there was no selection, this will be equal to the
 * drawable ID supplied as input. Otherwise, this will be the newly
 * created and remapped drawable. The 4 coordinates specify the new
 * locations of each corner of the original bounding box. By specifying
 * these values, any affine transformation (rotation, scaling,
 * translation) can be affected. Additionally, these values can be
 * specified such that the resulting transformed drawable will appear
 * to have been projected via a perspective transform.
 *
 * Returns: The newly mapped drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_perspective (gint32                 drawable_ID,
				     gdouble                x0,
				     gdouble                y0,
				     gdouble                x1,
				     gdouble                y1,
				     gdouble                x2,
				     gdouble                y2,
				     gdouble                x3,
				     gdouble                y3,
				     GimpTransformDirection transform_direction,
				     GimpInterpolationType  interpolation,
				     gboolean               supersample,
				     gint                   recursion_level,
				     gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-perspective",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, x0,
				    GIMP_PDB_FLOAT, y0,
				    GIMP_PDB_FLOAT, x1,
				    GIMP_PDB_FLOAT, y1,
				    GIMP_PDB_FLOAT, x2,
				    GIMP_PDB_FLOAT, y2,
				    GIMP_PDB_FLOAT, x3,
				    GIMP_PDB_FLOAT, y3,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_perspective_default:
 * @drawable_ID: The affected drawable.
 * @x0: The new x coordinate of upper-left corner of original bounding box.
 * @y0: The new y coordinate of upper-left corner of original bounding box.
 * @x1: The new x coordinate of upper-right corner of original bounding box.
 * @y1: The new y coordinate of upper-right corner of original bounding box.
 * @x2: The new x coordinate of lower-left corner of original bounding box.
 * @y2: The new y coordinate of lower-left corner of original bounding box.
 * @x3: The new x coordinate of lower-right corner of original bounding box.
 * @y3: The new y coordinate of lower-right corner of original bounding box.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Perform a possibly non-affine transformation on the specified
 * drawable, with extra parameters.
 *
 * This procedure is a variant of gimp_drawable_transform_perspective()
 * which uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The newly mapped drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_perspective_default (gint32   drawable_ID,
					     gdouble  x0,
					     gdouble  y0,
					     gdouble  x1,
					     gdouble  y1,
					     gdouble  x2,
					     gdouble  y2,
					     gdouble  x3,
					     gdouble  y3,
					     gboolean interpolate,
					     gboolean clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-perspective-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, x0,
				    GIMP_PDB_FLOAT, y0,
				    GIMP_PDB_FLOAT, x1,
				    GIMP_PDB_FLOAT, y1,
				    GIMP_PDB_FLOAT, x2,
				    GIMP_PDB_FLOAT, y2,
				    GIMP_PDB_FLOAT, x3,
				    GIMP_PDB_FLOAT, y3,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_rotate_simple:
 * @drawable_ID: The affected drawable.
 * @rotate_type: Type of rotation.
 * @auto_center: Whether to automatically rotate around the selection center.
 * @center_x: The hor. coordinate of the center of rotation.
 * @center_y: The vert. coordinate of the center of rotation.
 * @clip_result: Whether to clip results.
 *
 * Rotate the specified drawable about given coordinates through the
 * specified angle.
 *
 * This function rotates the specified drawable if no selection exists.
 * If a selection exists, the portion of the drawable which lies under
 * the selection is cut from the drawable and made into a floating
 * selection which is then rotated by the specified amount. The return
 * value is the ID of the rotated drawable. If there was no selection,
 * this will be equal to the drawable ID supplied as input. Otherwise,
 * this will be the newly created and rotated drawable.
 *
 * Returns: The rotated drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_rotate_simple (gint32           drawable_ID,
				       GimpRotationType rotate_type,
				       gboolean         auto_center,
				       gint             center_x,
				       gint             center_y,
				       gboolean         clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-rotate-simple",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_INT32, rotate_type,
				    GIMP_PDB_INT32, auto_center,
				    GIMP_PDB_INT32, center_x,
				    GIMP_PDB_INT32, center_y,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_rotate:
 * @drawable_ID: The affected drawable.
 * @angle: The angle of rotation (radians).
 * @auto_center: Whether to automatically rotate around the selection center.
 * @center_x: The hor. coordinate of the center of rotation.
 * @center_y: The vert. coordinate of the center of rotation.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Rotate the specified drawable about given coordinates through the
 * specified angle.
 *
 * This function rotates the specified drawable if no selection exists.
 * If a selection exists, the portion of the drawable which lies under
 * the selection is cut from the drawable and made into a floating
 * selection which is then rotated by the specified amount. The return
 * value is the ID of the rotated drawable. If there was no selection,
 * this will be equal to the drawable ID supplied as input. Otherwise,
 * this will be the newly created and rotated drawable.
 *
 * Returns: The rotated drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_rotate (gint32                 drawable_ID,
				gdouble                angle,
				gboolean               auto_center,
				gint                   center_x,
				gint                   center_y,
				GimpTransformDirection transform_direction,
				GimpInterpolationType  interpolation,
				gboolean               supersample,
				gint                   recursion_level,
				gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-rotate",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, angle,
				    GIMP_PDB_INT32, auto_center,
				    GIMP_PDB_INT32, center_x,
				    GIMP_PDB_INT32, center_y,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_rotate_default:
 * @drawable_ID: The affected drawable.
 * @angle: The angle of rotation (radians).
 * @auto_center: Whether to automatically rotate around the selection center.
 * @center_x: The hor. coordinate of the center of rotation.
 * @center_y: The vert. coordinate of the center of rotation.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Rotate the specified drawable about given coordinates through the
 * specified angle.
 *
 * This procedure is a variant of gimp_drawable_transform_rotate()
 * which uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The rotated drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_rotate_default (gint32   drawable_ID,
					gdouble  angle,
					gboolean auto_center,
					gint     center_x,
					gint     center_y,
					gboolean interpolate,
					gboolean clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-rotate-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, angle,
				    GIMP_PDB_INT32, auto_center,
				    GIMP_PDB_INT32, center_x,
				    GIMP_PDB_INT32, center_y,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_scale:
 * @drawable_ID: The affected drawable.
 * @x0: The new x coordinate of upper-left corner of newly scaled region.
 * @y0: The new y coordinate of upper-left corner of newly scaled region.
 * @x1: The new x coordinate of lower-right corner of newly scaled region.
 * @y1: The new y coordinate of lower-right corner of newly scaled region.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Scale the specified drawable with extra parameters
 *
 * This procedure scales the specified drawable if no selection exists.
 * If a selection exists, the portion of the drawable which lies under
 * the selection is cut from the drawable and made into a floating
 * selection which is then scaled by the specified amount. The return
 * value is the ID of the scaled drawable. If there was no selection,
 * this will be equal to the drawable ID supplied as input. Otherwise,
 * this will be the newly created and scaled drawable.
 *
 * Returns: The scaled drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_scale (gint32                 drawable_ID,
			       gdouble                x0,
			       gdouble                y0,
			       gdouble                x1,
			       gdouble                y1,
			       GimpTransformDirection transform_direction,
			       GimpInterpolationType  interpolation,
			       gboolean               supersample,
			       gint                   recursion_level,
			       gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-scale",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, x0,
				    GIMP_PDB_FLOAT, y0,
				    GIMP_PDB_FLOAT, x1,
				    GIMP_PDB_FLOAT, y1,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_scale_default:
 * @drawable_ID: The affected drawable.
 * @x0: The new x coordinate of upper-left corner of newly scaled region.
 * @y0: The new y coordinate of upper-left corner of newly scaled region.
 * @x1: The new x coordinate of lower-right corner of newly scaled region.
 * @y1: The new y coordinate of lower-right corner of newly scaled region.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Scale the specified drawable with extra parameters
 *
 * This procedure is a variant of gimp_drawable_transform_scale() which
 * uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The scaled drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_scale_default (gint32   drawable_ID,
				       gdouble  x0,
				       gdouble  y0,
				       gdouble  x1,
				       gdouble  y1,
				       gboolean interpolate,
				       gboolean clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-scale-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, x0,
				    GIMP_PDB_FLOAT, y0,
				    GIMP_PDB_FLOAT, x1,
				    GIMP_PDB_FLOAT, y1,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_shear:
 * @drawable_ID: The affected drawable.
 * @shear_type: Type of shear.
 * @magnitude: The magnitude of the shear.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Shear the specified drawable about its center by the specified
 * magnitude, with extra parameters.
 *
 * This procedure shears the specified drawable if no selection exists.
 * If a selection exists, the portion of the drawable which lies under
 * the selection is cut from the drawable and made into a floating
 * selection which is then sheard by the specified amount. The return
 * value is the ID of the sheard drawable. If there was no selection,
 * this will be equal to the drawable ID supplied as input. Otherwise,
 * this will be the newly created and sheard drawable. The shear type
 * parameter indicates whether the shear will be applied horizontally
 * or vertically. The magnitude can be either positive or negative and
 * indicates the extent (in pixels) to shear by.
 *
 * Returns: The sheared drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_shear (gint32                 drawable_ID,
			       GimpOrientationType    shear_type,
			       gdouble                magnitude,
			       GimpTransformDirection transform_direction,
			       GimpInterpolationType  interpolation,
			       gboolean               supersample,
			       gint                   recursion_level,
			       gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-shear",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_INT32, shear_type,
				    GIMP_PDB_FLOAT, magnitude,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_shear_default:
 * @drawable_ID: The affected drawable.
 * @shear_type: Type of shear.
 * @magnitude: The magnitude of the shear.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Shear the specified drawable about its center by the specified
 * magnitude, with extra parameters.
 *
 * This procedure is a variant of gimp_drawable_transform_shear() which
 * uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The sheared drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_shear_default (gint32              drawable_ID,
				       GimpOrientationType shear_type,
				       gdouble             magnitude,
				       gboolean            interpolate,
				       gboolean            clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-shear-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_INT32, shear_type,
				    GIMP_PDB_FLOAT, magnitude,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_2d:
 * @drawable_ID: The affected drawable.
 * @source_x: X coordinate of the transformation center.
 * @source_y: Y coordinate of the transformation center.
 * @scale_x: Amount to scale in x direction.
 * @scale_y: Amount to scale in y direction.
 * @angle: The angle of rotation (radians).
 * @dest_x: X coordinate of where the center goes.
 * @dest_y: Y coordinate of where the center goes.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Transform the specified drawable in 2d, with extra parameters.
 *
 * This procedure transforms the specified drawable if no selection
 * exists. If a selection exists, the portion of the drawable which
 * lies under the selection is cut from the drawable and made into a
 * floating selection which is then transformed. The transformation is
 * done by scaling the image by the x and y scale factors about the
 * point (source_x, source_y), then rotating around the same point,
 * then translating that point to the new position (dest_x, dest_y).
 * The return value is the ID of the rotated drawable. If there was no
 * selection, this will be equal to the drawable ID supplied as input.
 * Otherwise, this will be the newly created and transformed drawable.
 *
 * Returns: The transformed drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_2d (gint32                 drawable_ID,
			    gdouble                source_x,
			    gdouble                source_y,
			    gdouble                scale_x,
			    gdouble                scale_y,
			    gdouble                angle,
			    gdouble                dest_x,
			    gdouble                dest_y,
			    GimpTransformDirection transform_direction,
			    GimpInterpolationType  interpolation,
			    gboolean               supersample,
			    gint                   recursion_level,
			    gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-2d",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, source_x,
				    GIMP_PDB_FLOAT, source_y,
				    GIMP_PDB_FLOAT, scale_x,
				    GIMP_PDB_FLOAT, scale_y,
				    GIMP_PDB_FLOAT, angle,
				    GIMP_PDB_FLOAT, dest_x,
				    GIMP_PDB_FLOAT, dest_y,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_2d_default:
 * @drawable_ID: The affected drawable.
 * @source_x: X coordinate of the transformation center.
 * @source_y: Y coordinate of the transformation center.
 * @scale_x: Amount to scale in x direction.
 * @scale_y: Amount to scale in y direction.
 * @angle: The angle of rotation (radians).
 * @dest_x: X coordinate of where the center goes.
 * @dest_y: Y coordinate of where the center goes.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Transform the specified drawable in 2d, with extra parameters.
 *
 * This procedure is a variant of gimp_drawable_transform_2d() which
 * uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The transformed drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_2d_default (gint32   drawable_ID,
				    gdouble  source_x,
				    gdouble  source_y,
				    gdouble  scale_x,
				    gdouble  scale_y,
				    gdouble  angle,
				    gdouble  dest_x,
				    gdouble  dest_y,
				    gboolean interpolate,
				    gboolean clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-2d-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, source_x,
				    GIMP_PDB_FLOAT, source_y,
				    GIMP_PDB_FLOAT, scale_x,
				    GIMP_PDB_FLOAT, scale_y,
				    GIMP_PDB_FLOAT, angle,
				    GIMP_PDB_FLOAT, dest_x,
				    GIMP_PDB_FLOAT, dest_y,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_matrix:
 * @drawable_ID: The affected drawable.
 * @coeff_0_0: coefficient (0,0) of the transformation matrix.
 * @coeff_0_1: coefficient (0,1) of the transformation matrix.
 * @coeff_0_2: coefficient (0,2) of the transformation matrix.
 * @coeff_1_0: coefficient (1,0) of the transformation matrix.
 * @coeff_1_1: coefficient (1,1) of the transformation matrix.
 * @coeff_1_2: coefficient (1,2) of the transformation matrix.
 * @coeff_2_0: coefficient (2,0) of the transformation matrix.
 * @coeff_2_1: coefficient (2,1) of the transformation matrix.
 * @coeff_2_2: coefficient (2,2) of the transformation matrix.
 * @transform_direction: Direction of Transformation.
 * @interpolation: Type of interpolation.
 * @supersample: Whether to perform supersample.
 * @recursion_level: Level of recursion (3 is a nice default).
 * @clip_result: Whether to clip results.
 *
 * Transform the specified drawable in 2d, with extra parameters.
 *
 * This procedure transforms the specified drawable if no selection
 * exists. If a selection exists, the portion of the drawable which
 * lies under the selection is cut from the drawable and made into a
 * floating selection which is then transformed. The transformation is
 * done by assembling a 3x3 matrix from the coefficients passed. The
 * return value is the ID of the rotated drawable. If there was no
 * selection, this will be equal to the drawable ID supplied as input.
 * Otherwise, this will be the newly created and transformed drawable.
 *
 * Returns: The transformed drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_matrix (gint32                 drawable_ID,
				gdouble                coeff_0_0,
				gdouble                coeff_0_1,
				gdouble                coeff_0_2,
				gdouble                coeff_1_0,
				gdouble                coeff_1_1,
				gdouble                coeff_1_2,
				gdouble                coeff_2_0,
				gdouble                coeff_2_1,
				gdouble                coeff_2_2,
				GimpTransformDirection transform_direction,
				GimpInterpolationType  interpolation,
				gboolean               supersample,
				gint                   recursion_level,
				gboolean               clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-matrix",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, coeff_0_0,
				    GIMP_PDB_FLOAT, coeff_0_1,
				    GIMP_PDB_FLOAT, coeff_0_2,
				    GIMP_PDB_FLOAT, coeff_1_0,
				    GIMP_PDB_FLOAT, coeff_1_1,
				    GIMP_PDB_FLOAT, coeff_1_2,
				    GIMP_PDB_FLOAT, coeff_2_0,
				    GIMP_PDB_FLOAT, coeff_2_1,
				    GIMP_PDB_FLOAT, coeff_2_2,
				    GIMP_PDB_INT32, transform_direction,
				    GIMP_PDB_INT32, interpolation,
				    GIMP_PDB_INT32, supersample,
				    GIMP_PDB_INT32, recursion_level,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}

/**
 * gimp_drawable_transform_matrix_default:
 * @drawable_ID: The affected drawable.
 * @coeff_0_0: coefficient (0,0) of the transformation matrix.
 * @coeff_0_1: coefficient (0,1) of the transformation matrix.
 * @coeff_0_2: coefficient (0,2) of the transformation matrix.
 * @coeff_1_0: coefficient (1,0) of the transformation matrix.
 * @coeff_1_1: coefficient (1,1) of the transformation matrix.
 * @coeff_1_2: coefficient (1,2) of the transformation matrix.
 * @coeff_2_0: coefficient (2,0) of the transformation matrix.
 * @coeff_2_1: coefficient (2,1) of the transformation matrix.
 * @coeff_2_2: coefficient (2,2) of the transformation matrix.
 * @interpolate: Whether to use interpolation and supersampling.
 * @clip_result: Whether to clip results.
 *
 * Transform the specified drawable in 2d, with extra parameters.
 *
 * This procedure is a variant of gimp_drawable_transform_matrix()
 * which uses no interpolation/supersampling at all, or default values
 * (depending on the 'interpolate' parameter).
 *
 * Returns: The transformed drawable.
 *
 * Since: GIMP 2.2
 */
gint32
gimp_drawable_transform_matrix_default (gint32   drawable_ID,
					gdouble  coeff_0_0,
					gdouble  coeff_0_1,
					gdouble  coeff_0_2,
					gdouble  coeff_1_0,
					gdouble  coeff_1_1,
					gdouble  coeff_1_2,
					gdouble  coeff_2_0,
					gdouble  coeff_2_1,
					gdouble  coeff_2_2,
					gboolean interpolate,
					gboolean clip_result)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gint32 ret_drawable_ID = -1;

  return_vals = gimp_run_procedure ("gimp-drawable-transform-matrix-default",
				    &nreturn_vals,
				    GIMP_PDB_DRAWABLE, drawable_ID,
				    GIMP_PDB_FLOAT, coeff_0_0,
				    GIMP_PDB_FLOAT, coeff_0_1,
				    GIMP_PDB_FLOAT, coeff_0_2,
				    GIMP_PDB_FLOAT, coeff_1_0,
				    GIMP_PDB_FLOAT, coeff_1_1,
				    GIMP_PDB_FLOAT, coeff_1_2,
				    GIMP_PDB_FLOAT, coeff_2_0,
				    GIMP_PDB_FLOAT, coeff_2_1,
				    GIMP_PDB_FLOAT, coeff_2_2,
				    GIMP_PDB_INT32, interpolate,
				    GIMP_PDB_INT32, clip_result,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    ret_drawable_ID = return_vals[1].data.d_drawable;

  gimp_destroy_params (return_vals, nreturn_vals);

  return ret_drawable_ID;
}
