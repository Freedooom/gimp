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

#include <gtk/gtk.h>

#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#ifdef __GNUC__
#warning #include "tools/tools-types.h"
#endif

#include "tools/tools-types.h"

#include "config/gimpcoreconfig.h"

#include "core/gimp.h"
#include "core/gimpchannel-select.h"
#include "core/gimpcontainer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-pick-color.h"
#include "core/gimpselection.h"
#include "core/gimptoolinfo.h"

#include "tools/gimpselectionoptions.h"

#include "gimpselectioneditor.h"
#include "gimpdnd.h"
#include "gimphelp-ids.h"
#include "gimpmenufactory.h"
#include "gimppreview.h"
#include "gimppreviewrenderer.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"


static void   gimp_selection_editor_class_init (GimpSelectionEditorClass *klass);
static void   gimp_selection_editor_init       (GimpSelectionEditor      *selection_editor);

static GObject * gimp_selection_editor_constructor (GType                type,
                                                    guint                n_params,
                                                    GObjectConstructParam *params);

static void   gimp_selection_editor_set_image      (GimpImageEditor     *editor,
                                                    GimpImage           *gimage);

static gboolean gimp_selection_preview_button_press(GtkWidget           *widget,
                                                    GdkEventButton      *bevent,
                                                    GimpSelectionEditor *editor);
static void   gimp_selection_editor_drop_color     (GtkWidget           *widget,
                                                    const GimpRGB       *color,
                                                    gpointer             data);

static void   gimp_selection_editor_mask_changed   (GimpImage           *gimage,
                                                    GimpSelectionEditor *editor);


static GimpImageEditorClass *parent_class = NULL;


GType
gimp_selection_editor_get_type (void)
{
  static GType editor_type = 0;

  if (! editor_type)
    {
      static const GTypeInfo editor_info =
      {
        sizeof (GimpSelectionEditorClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_selection_editor_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpSelectionEditor),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_selection_editor_init,
      };

      editor_type = g_type_register_static (GIMP_TYPE_IMAGE_EDITOR,
                                            "GimpSelectionEditor",
                                            &editor_info, 0);
    }

  return editor_type;
}

static void
gimp_selection_editor_class_init (GimpSelectionEditorClass* klass)
{
  GObjectClass         *object_class       = G_OBJECT_CLASS (klass);
  GimpImageEditorClass *image_editor_class = GIMP_IMAGE_EDITOR_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->constructor     = gimp_selection_editor_constructor;

  image_editor_class->set_image = gimp_selection_editor_set_image;
}

static void
gimp_selection_editor_init (GimpSelectionEditor *editor)
{
  GtkWidget *frame;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (editor), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame);

  editor->preview = gimp_preview_new_by_types (GIMP_TYPE_PREVIEW,
                                               GIMP_TYPE_SELECTION,
                                               GIMP_PREVIEW_SIZE_HUGE,
                                               0, TRUE);
  gimp_preview_renderer_set_background (GIMP_PREVIEW (editor->preview)->renderer,
                                        GIMP_STOCK_TEXTURE);
  gtk_widget_set_size_request (editor->preview,
                               GIMP_PREVIEW_SIZE_HUGE, GIMP_PREVIEW_SIZE_HUGE);
  gimp_preview_set_expand (GIMP_PREVIEW (editor->preview), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), editor->preview);
  gtk_widget_show (editor->preview);

  g_signal_connect (editor->preview, "button_press_event",
                    G_CALLBACK (gimp_selection_preview_button_press),
                    editor);

  gimp_dnd_color_dest_add (editor->preview,
                           gimp_selection_editor_drop_color,
                           editor);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);
}

static GObject *
gimp_selection_editor_constructor (GType                  type,
                                   guint                  n_params,
                                   GObjectConstructParam *params)
{
  GObject             *object;
  GimpSelectionEditor *editor;
  gchar               *str;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  editor = GIMP_SELECTION_EDITOR (object);

  editor->all_button =
    gimp_editor_add_action_button (GIMP_EDITOR (editor), "select",
                                   "select-all", NULL);

  editor->none_button =
    gimp_editor_add_action_button (GIMP_EDITOR (editor), "select",
                                   "select-none", NULL);

  editor->invert_button =
    gimp_editor_add_action_button (GIMP_EDITOR (editor), "select",
                                   "select-invert", NULL);

  editor->save_button =
    gimp_editor_add_action_button (GIMP_EDITOR (editor), "select",
                                   "select-save", NULL);

  str = g_strdup_printf (_("Selection to path\n"
                           "%s  Advanced options"),
                         gimp_get_mod_string (GDK_SHIFT_MASK));

  editor->path_button =
    gimp_editor_add_action_button (GIMP_EDITOR (editor), "vectors",
                                   "vectors-selection-to-vectors",
                                   "vectors-selection-to-vectors-advanced",
                                   GDK_SHIFT_MASK,
                                   NULL);
  gimp_help_set_help_data (editor->path_button, str,
                           GIMP_HELP_SELECTION_TO_PATH);

  g_free (str);

  editor->stroke_button =
    gimp_editor_add_action_button (GIMP_EDITOR (editor), "select",
                                   "select-stroke", NULL);

  return object;
}

static void
gimp_selection_editor_set_image (GimpImageEditor *image_editor,
                                 GimpImage       *gimage)
{
  GimpSelectionEditor *editor = GIMP_SELECTION_EDITOR (image_editor);

  if (image_editor->gimage)
    {
      g_signal_handlers_disconnect_by_func (image_editor->gimage,
					    gimp_selection_editor_mask_changed,
					    editor);
    }

  GIMP_IMAGE_EDITOR_CLASS (parent_class)->set_image (image_editor, gimage);

  if (gimage)
    {
      g_signal_connect (gimage, "mask_changed",
			G_CALLBACK (gimp_selection_editor_mask_changed),
			editor);

      gimp_preview_set_viewable (GIMP_PREVIEW (editor->preview),
                                 GIMP_VIEWABLE (gimp_image_get_mask (gimage)));
    }
  else
    {
      gimp_preview_set_viewable (GIMP_PREVIEW (editor->preview), NULL);
    }
}


/*  public functions  */

GtkWidget *
gimp_selection_editor_new (GimpImage       *gimage,
                           GimpMenuFactory *menu_factory)
{
  GimpSelectionEditor *editor;

  g_return_val_if_fail (gimage == NULL || GIMP_IS_IMAGE (gimage), NULL);
  g_return_val_if_fail (GIMP_IS_MENU_FACTORY (menu_factory), NULL);

  editor = g_object_new (GIMP_TYPE_SELECTION_EDITOR,
                         "menu-factory",    menu_factory,
                         "menu-identifier", "<SelectionEditor>",
                         "ui-path",         "/selection-editor-popup",
                         NULL);

  if (gimage)
    gimp_image_editor_set_image (GIMP_IMAGE_EDITOR (editor), gimage);

  return GTK_WIDGET (editor);
}

static gboolean
gimp_selection_preview_button_press (GtkWidget           *widget,
                                     GdkEventButton      *bevent,
                                     GimpSelectionEditor *editor)
{
  GimpImageEditor      *image_editor = GIMP_IMAGE_EDITOR (editor);
  GimpPreviewRenderer  *renderer;
  GimpToolInfo         *tool_info;
  GimpSelectionOptions *options;
  GimpDrawable         *drawable;
  SelectOps             operation = SELECTION_REPLACE;
  gint                  x, y;
  GimpRGB               color;

  if (! image_editor->gimage)
    return TRUE;

  renderer = GIMP_PREVIEW (editor->preview)->renderer;

  tool_info = (GimpToolInfo *)
    gimp_container_get_child_by_name (image_editor->gimage->gimp->tool_info_list,
                                      "gimp-by-color-select-tool");

  if (! tool_info)
    return TRUE;

  options = GIMP_SELECTION_OPTIONS (tool_info->tool_options);

  drawable = gimp_image_active_drawable (image_editor->gimage);

  if (! drawable)
    return TRUE;

  if (bevent->state & GDK_SHIFT_MASK)
    {
      if (bevent->state & GDK_CONTROL_MASK)
        {
          operation = SELECTION_INTERSECT;
        }
      else
        {
          operation = SELECTION_ADD;
        }
    }
  else if (bevent->state & GDK_CONTROL_MASK)
    {
      operation = SELECTION_SUBTRACT;
    }

  x = image_editor->gimage->width  * bevent->x / renderer->width;
  y = image_editor->gimage->height * bevent->y / renderer->height;

  if (gimp_image_pick_color (image_editor->gimage, drawable, x, y,
                             options->sample_merged,
                             FALSE, 0.0,
                             NULL,
                             &color, NULL))
    {
      gimp_channel_select_by_color (gimp_image_get_mask (image_editor->gimage),
                                    drawable,
                                    options->sample_merged,
                                    &color,
                                    options->threshold,
                                    options->select_transparent,
                                    operation,
                                    options->antialias,
                                    options->feather,
                                    options->feather_radius,
                                    options->feather_radius);
      gimp_image_flush (image_editor->gimage);
    }

  return TRUE;
}

static void
gimp_selection_editor_drop_color (GtkWidget     *widget,
                                  const GimpRGB *color,
                                  gpointer       data)
{
  GimpImageEditor      *editor = GIMP_IMAGE_EDITOR (data);
  GimpToolInfo         *tool_info;
  GimpSelectionOptions *options;
  GimpDrawable         *drawable;

  if (! editor->gimage)
    return;

  tool_info = (GimpToolInfo *)
    gimp_container_get_child_by_name (editor->gimage->gimp->tool_info_list,
                                      "gimp-by-color-select-tool");

  if (! tool_info)
    return;

  options = GIMP_SELECTION_OPTIONS (tool_info->tool_options);

  drawable = gimp_image_active_drawable (editor->gimage);

  if (! drawable)
    return;

  gimp_channel_select_by_color (gimp_image_get_mask (editor->gimage),
                                drawable,
                                options->sample_merged,
                                color,
                                options->threshold,
                                options->select_transparent,
                                options->operation,
                                options->antialias,
                                options->feather,
                                options->feather_radius,
                                options->feather_radius);
  gimp_image_flush (editor->gimage);
}

static void
gimp_selection_editor_mask_changed (GimpImage           *gimage,
                                    GimpSelectionEditor *editor)
{
  gimp_preview_renderer_invalidate (GIMP_PREVIEW (editor->preview)->renderer);
}
