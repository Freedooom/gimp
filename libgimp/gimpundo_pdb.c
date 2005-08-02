/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2003 Peter Mattis and Spencer Kimball
 *
 * gimpundo_pdb.c
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
 * gimp_image_undo_group_start:
 * @image_ID: The ID of the image in which to open an undo group.
 *
 * Starts a group undo.
 *
 * This function is used to start a group undo--necessary for logically
 * combining two or more undo operations into a single operation. This
 * call must be used in conjunction with a 'gimp-image-undo-group-end'
 * call.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_image_undo_group_start (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp-image-undo-group-start",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_image_undo_group_end:
 * @image_ID: The ID of the image in which to close an undo group.
 *
 * Finish a group undo.
 *
 * This function must be called once for each
 * 'gimp-image-undo-group-start' call that is made.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_image_undo_group_end (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp-image-undo-group-end",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == GIMP_PDB_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_image_undo_is_enabled:
 * @image_ID: The image.
 *
 * Check if the image's undo stack is enabled.
 *
 * This procedure checks if the image's undo stack is currently enabled
 * or disabled. This is useful when several plugins or scripts call
 * each other and want to check if their caller has already used
 * 'gimp_image_undo_disable' or 'gimp_image_undo_freeze'.
 *
 * Returns: True if undo is enabled for this image.
 */
gboolean
gimp_image_undo_is_enabled (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean enabled = FALSE;

  return_vals = gimp_run_procedure ("gimp-image-undo-is-enabled",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    enabled = return_vals[1].data.d_int32;

  gimp_destroy_params (return_vals, nreturn_vals);

  return enabled;
}

/**
 * gimp_image_undo_disable:
 * @image_ID: The image.
 *
 * Disable the image's undo stack.
 *
 * This procedure disables the image's undo stack, allowing subsequent
 * operations to ignore their undo steps. This is generally called in
 * conjunction with 'gimp_image_undo_enable' to temporarily disable an
 * image undo stack. This is advantageous because saving undo steps can
 * be time and memory intensive.
 *
 * Returns: True if the image undo has been disabled.
 */
gboolean
gimp_image_undo_disable (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean disabled = FALSE;

  return_vals = gimp_run_procedure ("gimp-image-undo-disable",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    disabled = return_vals[1].data.d_int32;

  gimp_destroy_params (return_vals, nreturn_vals);

  return disabled;
}

/**
 * gimp_image_undo_enable:
 * @image_ID: The image.
 *
 * Enable the image's undo stack.
 *
 * This procedure enables the image's undo stack, allowing subsequent
 * operations to store their undo steps. This is generally called in
 * conjunction with 'gimp_image_undo_disable' to temporarily disable an
 * image undo stack.
 *
 * Returns: True if the image undo has been enabled.
 */
gboolean
gimp_image_undo_enable (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean enabled = FALSE;

  return_vals = gimp_run_procedure ("gimp-image-undo-enable",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    enabled = return_vals[1].data.d_int32;

  gimp_destroy_params (return_vals, nreturn_vals);

  return enabled;
}

/**
 * gimp_image_undo_freeze:
 * @image_ID: The image.
 *
 * Freeze the image's undo stack.
 *
 * This procedure freezes the image's undo stack, allowing subsequent
 * operations to ignore their undo steps. This is generally called in
 * conjunction with 'gimp_image_undo_thaw' to temporarily disable an
 * image undo stack. This is advantageous because saving undo steps can
 * be time and memory intensive. 'gimp_image_undo_{freeze,thaw}' and
 * 'gimp_image_undo_{disable,enable}' differ in that the former does
 * not free up all undo steps when undo is thawed, so is more suited to
 * interactive in-situ previews. It is important in this case that the
 * image is back to the same state it was frozen in before thawing,
 * else 'undo' behaviour is undefined.
 *
 * Returns: True if the image undo has been frozen.
 */
gboolean
gimp_image_undo_freeze (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean frozen = FALSE;

  return_vals = gimp_run_procedure ("gimp-image-undo-freeze",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    frozen = return_vals[1].data.d_int32;

  gimp_destroy_params (return_vals, nreturn_vals);

  return frozen;
}

/**
 * gimp_image_undo_thaw:
 * @image_ID: The image.
 *
 * Thaw the image's undo stack.
 *
 * This procedure thaws the image's undo stack, allowing subsequent
 * operations to store their undo steps. This is generally called in
 * conjunction with 'gimp_image_undo_freeze' to temporarily freeze an
 * image undo stack. 'gimp_image_undo_thaw' does NOT free the undo
 * stack as 'gimp_image_undo_enable' does, so is suited for situations
 * where one wishes to leave the undo stack in the same state in which
 * one found it despite non-destructively playing with the image in the
 * meantime. An example would be in-situ plugin previews. Balancing
 * freezes and thaws and ensuring image consistancy is the
 * responsibility of the caller.
 *
 * Returns: True if the image undo has been thawed.
 */
gboolean
gimp_image_undo_thaw (gint32 image_ID)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean thawed = FALSE;

  return_vals = gimp_run_procedure ("gimp-image-undo-thaw",
				    &nreturn_vals,
				    GIMP_PDB_IMAGE, image_ID,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == GIMP_PDB_SUCCESS)
    thawed = return_vals[1].data.d_int32;

  gimp_destroy_params (return_vals, nreturn_vals);

  return thawed;
}
