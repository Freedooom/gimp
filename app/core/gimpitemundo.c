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

#include "gimpimage.h"
#include "gimpitem.h"
#include "gimpitemundo.h"


enum
{
  PROP_0,
  PROP_ITEM
};


static void      gimp_item_undo_class_init   (GimpItemUndoClass     *klass);

static GObject * gimp_item_undo_constructor  (GType                  type,
                                              guint                  n_params,
                                              GObjectConstructParam *params);
static void      gimp_item_undo_set_property (GObject               *object,
                                              guint                  property_id,
                                              const GValue          *value,
                                              GParamSpec            *pspec);
static void      gimp_item_undo_get_property (GObject               *object,
                                              guint                  property_id,
                                              GValue                *value,
                                              GParamSpec            *pspec);

static void      gimp_item_undo_free         (GimpUndo              *undo,
                                              GimpUndoMode           undo_mode);


static GimpUndoClass *parent_class = NULL;


GType
gimp_item_undo_get_type (void)
{
  static GType undo_type = 0;

  if (! undo_type)
    {
      static const GTypeInfo undo_info =
      {
        sizeof (GimpItemUndoClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gimp_item_undo_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data     */
        sizeof (GimpItemUndo),
        0,              /* n_preallocs    */
        NULL            /* instance_init  */
      };

      undo_type = g_type_register_static (GIMP_TYPE_UNDO,
                                          "GimpItemUndo",
                                          &undo_info, 0);
  }

  return undo_type;
}

static void
gimp_item_undo_class_init (GimpItemUndoClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GimpUndoClass *undo_class   = GIMP_UNDO_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->constructor  = gimp_item_undo_constructor;
  object_class->set_property = gimp_item_undo_set_property;
  object_class->get_property = gimp_item_undo_get_property;

  undo_class->free           = gimp_item_undo_free;

  g_object_class_install_property (object_class, PROP_ITEM,
                                   g_param_spec_object ("item", NULL, NULL,
                                                        GIMP_TYPE_ITEM,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static GObject *
gimp_item_undo_constructor (GType                  type,
                            guint                  n_params,
                            GObjectConstructParam *params)
{
  GObject      *object;
  GimpItemUndo *item_undo;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  item_undo = GIMP_ITEM_UNDO (object);

  g_assert (GIMP_IS_ITEM (item_undo->item));

  return object;
}

static void
gimp_item_undo_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GimpItemUndo *item_undo = GIMP_ITEM_UNDO (object);

  switch (property_id)
    {
    case PROP_ITEM:
      item_undo->item = (GimpItem *) g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_item_undo_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GimpItemUndo *item_undo = GIMP_ITEM_UNDO (object);

  switch (property_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, item_undo->item);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_item_undo_free (GimpUndo     *undo,
                     GimpUndoMode  undo_mode)
{
  GimpItemUndo *item_undo = GIMP_ITEM_UNDO (undo);

  GIMP_UNDO_CLASS (parent_class)->free (undo, undo_mode);

  if (item_undo->item)
    {
      g_object_unref (item_undo->item);
      item_undo->item = NULL;
    }
}
