/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpDisplayOptions
 * Copyright (C) 2003  Sven Neumann <sven@gimp.org>
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

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpcolor/gimpcolor.h"

#include "core/core-types.h"
#include "display-enums.h"

#include "config/gimpconfig.h"
#include "config/gimpconfig.h"
#include "config/gimpconfig-params.h"
#include "config/gimprc-blurbs.h"

#include "gimpdisplayoptions.h"

#include "gimp-intl.h"


enum
{
  PROP_0,
  PROP_SHOW_MENUBAR,
  PROP_SHOW_RULERS,
  PROP_SHOW_SCROLLBARS,
  PROP_SHOW_STATUSBAR,
  PROP_SHOW_SELECTION,
  PROP_SHOW_LAYER_BOUNDARY,
  PROP_SHOW_GUIDES,
  PROP_SHOW_GRID,
  PROP_PADDING_MODE,
  PROP_PADDING_COLOR
};


static void  gimp_display_options_class_init    (GimpDisplayOptionsClass *klass);
static void  gimp_display_options_fs_class_init (GimpDisplayOptionsClass *klass);
static void  gimp_display_options_init          (GimpDisplayOptions      *options);

static void  gimp_display_options_set_property  (GObject      *object,
                                                guint         property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec);
static void  gimp_display_options_get_property  (GObject      *object,
                                                guint         property_id,
                                                GValue       *value,
                                                GParamSpec   *pspec);


GType
gimp_display_options_get_type (void)
{
  static GType options_type = 0;

  if (! options_type)
    {
      static const GTypeInfo options_info =
      {
        sizeof (GimpDisplayOptionsClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_display_options_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpDisplayOptions),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_display_options_init
      };
      static const GInterfaceInfo config_iface_info =
      {
        NULL,           /* iface_init     */
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      options_type = g_type_register_static (G_TYPE_OBJECT,
                                             "GimpDisplayOptions",
                                             &options_info, 0);

      g_type_add_interface_static (options_type, GIMP_TYPE_CONFIG,
                                   &config_iface_info);
    }

  return options_type;
}

GType
gimp_display_options_fullscreen_get_type (void)
{
  static GType options_type = 0;

  if (! options_type)
    {
      static const GTypeInfo options_info =
      {
        sizeof (GimpDisplayOptionsClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_display_options_fs_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data     */
	sizeof (GimpDisplayOptions),
	0,              /* n_preallocs    */
	NULL            /* instance_init  */
      };
      static const GInterfaceInfo config_iface_info =
      {
        NULL,           /* iface_init     */
        NULL,           /* iface_finalize */
        NULL            /* iface_data     */
      };

      options_type = g_type_register_static (GIMP_TYPE_DISPLAY_OPTIONS,
                                             "GimpDisplayOptionsFullscreen",
                                             &options_info, 0);

      g_type_add_interface_static (options_type, GIMP_TYPE_CONFIG,
                                   &config_iface_info);
    }

  return options_type;
}

static void
gimp_display_options_class_init (GimpDisplayOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GimpRGB       white;

  gimp_rgba_set (&white, 1.0, 1.0, 1.0, GIMP_OPACITY_OPAQUE);

  object_class->set_property = gimp_display_options_set_property;
  object_class->get_property = gimp_display_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_MENUBAR,
                                    "show-menubar", SHOW_MENUBAR_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_RULERS,
                                    "show-rulers", SHOW_RULERS_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_SCROLLBARS,
                                    "show-scrollbars", SHOW_SCROLLBARS_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_STATUSBAR,
                                    "show-statusbar", SHOW_STATUSBAR_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_SELECTION,
                                    "show-selection", SHOW_SELECTION_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_LAYER_BOUNDARY,
                                    "show-layer-boundary", SHOW_LAYER_BOUNDARY_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_GUIDES,
                                    "show-guides", SHOW_GUIDES_BLURB,
                                    TRUE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_GRID,
                                    "show-grid", SHOW_GRID_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_PADDING_MODE,
                                 "padding-mode", CANVAS_PADDING_MODE_BLURB,
                                 GIMP_TYPE_CANVAS_PADDING_MODE,
                                 GIMP_CANVAS_PADDING_MODE_DEFAULT,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_COLOR (object_class, PROP_PADDING_COLOR,
                                  "padding-color", CANVAS_PADDING_COLOR_BLURB,
                                  &white,
                                  0);
}

static void
gimp_display_options_fs_class_init (GimpDisplayOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GimpRGB       black;

  gimp_rgba_set (&black, 0.0, 0.0, 0.0, GIMP_OPACITY_OPAQUE);

  object_class->set_property = gimp_display_options_set_property;
  object_class->get_property = gimp_display_options_get_property;

  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_MENUBAR,
                                    "show-menubar", SHOW_MENUBAR_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_RULERS,
                                    "show-rulers", SHOW_RULERS_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_SCROLLBARS,
                                    "show-scrollbars", SHOW_SCROLLBARS_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_STATUSBAR,
                                    "show-statusbar", SHOW_STATUSBAR_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_SELECTION,
                                    "show-selection", SHOW_SELECTION_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_LAYER_BOUNDARY,
                                    "show-layer-boundary", SHOW_LAYER_BOUNDARY_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_GUIDES,
                                    "show-guides", SHOW_GUIDES_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, PROP_SHOW_GRID,
                                    "show-grid", SHOW_GRID_BLURB,
                                    FALSE,
                                    0);
  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_PADDING_MODE,
                                 "padding-mode", CANVAS_PADDING_MODE_BLURB,
                                 GIMP_TYPE_CANVAS_PADDING_MODE,
                                 GIMP_CANVAS_PADDING_MODE_CUSTOM,
                                 0);
  GIMP_CONFIG_INSTALL_PROP_COLOR (object_class, PROP_PADDING_COLOR,
                                  "padding-color", CANVAS_PADDING_COLOR_BLURB,
                                  &black,
                                  0);
}

static void
gimp_display_options_init (GimpDisplayOptions *options)
{
  options->padding_mode_set = FALSE;
}

static void
gimp_display_options_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GimpDisplayOptions *options = GIMP_DISPLAY_OPTIONS (object);

  switch (property_id)
    {
    case PROP_SHOW_MENUBAR:
      options->show_menubar = g_value_get_boolean (value);
      break;
    case PROP_SHOW_RULERS:
      options->show_rulers = g_value_get_boolean (value);
      break;
    case PROP_SHOW_SCROLLBARS:
      options->show_scrollbars = g_value_get_boolean (value);
      break;
    case PROP_SHOW_STATUSBAR:
      options->show_statusbar = g_value_get_boolean (value);
      break;
    case PROP_SHOW_SELECTION:
      options->show_selection = g_value_get_boolean (value);
      break;
    case PROP_SHOW_LAYER_BOUNDARY:
      options->show_layer_boundary = g_value_get_boolean (value);
      break;
    case PROP_SHOW_GUIDES:
      options->show_guides = g_value_get_boolean (value);
      break;
    case PROP_SHOW_GRID:
      options->show_grid = g_value_get_boolean (value);
      break;
    case PROP_PADDING_MODE:
      options->padding_mode = g_value_get_enum (value);
      break;
    case PROP_PADDING_COLOR:
      options->padding_color = *(GimpRGB *) g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_display_options_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GimpDisplayOptions *options = GIMP_DISPLAY_OPTIONS (object);

  switch (property_id)
    {
    case PROP_SHOW_MENUBAR:
      g_value_set_boolean (value, options->show_menubar);
      break;
    case PROP_SHOW_RULERS:
      g_value_set_boolean (value, options->show_rulers);
      break;
    case PROP_SHOW_SCROLLBARS:
      g_value_set_boolean (value, options->show_scrollbars);
      break;
    case PROP_SHOW_STATUSBAR:
      g_value_set_boolean (value, options->show_statusbar);
      break;
    case PROP_SHOW_SELECTION:
      g_value_set_boolean (value, options->show_selection);
      break;
    case PROP_SHOW_LAYER_BOUNDARY:
      g_value_set_boolean (value, options->show_layer_boundary);
      break;
    case PROP_SHOW_GUIDES:
      g_value_set_boolean (value, options->show_guides);
      break;
    case PROP_SHOW_GRID:
      g_value_set_boolean (value, options->show_grid);
      break;
    case PROP_PADDING_MODE:
      g_value_set_enum (value, options->padding_mode);
      break;
    case PROP_PADDING_COLOR:
      g_value_set_boxed (value, &options->padding_color);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
