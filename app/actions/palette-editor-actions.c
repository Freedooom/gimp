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

#include "libgimpwidgets/gimpwidgets.h"

#include "actions-types.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimphelp-ids.h"
#include "widgets/gimppaletteeditor.h"

#include "palette-editor-actions.h"
#include "palette-editor-commands.h"

#include "gimp-intl.h"


static GimpActionEntry palette_editor_actions[] =
{
  { "palette-editor-edit-color", GIMP_STOCK_EDIT,
    N_("_Edit Color..."), "", NULL,
    G_CALLBACK (palette_editor_edit_color_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_EDIT },

  { "palette-editor-new-color-fg", GTK_STOCK_NEW,
    N_("New Color from _FG"), "", NULL,
    G_CALLBACK (palette_editor_new_color_fg_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_NEW },

  { "palette-editor-new-color-bg", GTK_STOCK_NEW,
    N_("New Color from _BG"), "", NULL,
    G_CALLBACK (palette_editor_new_color_bg_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_NEW },

  { "palette-editor-delete-color", GTK_STOCK_DELETE,
    N_("_Delete Color"), "", NULL,
    G_CALLBACK (palette_editor_delete_color_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_DELETE },

  { "palette-editor-zoom-out", GTK_STOCK_ZOOM_OUT,
    N_("Zoom _Out"), "", NULL,
    G_CALLBACK (palette_editor_zoom_out_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_ZOOM_OUT },

  { "palette-editor-zoom-in", GTK_STOCK_ZOOM_IN,
    N_("Zoom _In"), "", NULL,
    G_CALLBACK (palette_editor_zoom_in_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_ZOOM_IN },

  { "palette-editor-zoom-all", GTK_STOCK_ZOOM_FIT,
    N_("Zoom _All"), "", NULL,
    G_CALLBACK (palette_editor_zoom_all_cmd_callback),
    GIMP_HELP_PALETTE_EDITOR_ZOOM_ALL }
};


void
palette_editor_actions_setup (GimpActionGroup *group,
                              gpointer         data)
{
  gimp_action_group_add_actions (group,
                                 palette_editor_actions,
                                 G_N_ELEMENTS (palette_editor_actions),
                                 data);
}

void
palette_editor_actions_update (GimpActionGroup *group,
                               gpointer         data)
{
  GimpPaletteEditor *editor;
  GimpDataEditor    *data_editor;
  gboolean           editable = FALSE;

  editor      = GIMP_PALETTE_EDITOR (data);
  data_editor = GIMP_DATA_EDITOR (data);

  if (data_editor->data && data_editor->data_editable)
    editable = TRUE;

#define SET_SENSITIVE(action,condition) \
        gimp_action_group_set_action_sensitive (group, action, (condition) != 0)

  SET_SENSITIVE ("palette-editor-edit-color",   editable && editor->color);
  SET_SENSITIVE ("palette-editor-new-color-fg", editable);
  SET_SENSITIVE ("palette-editor-new-color-bg", editable);
  SET_SENSITIVE ("palette-editor-delete-color", editable && editor->color);

  SET_SENSITIVE ("palette-editor-zoom-out", data_editor->data);
  SET_SENSITIVE ("palette-editor-zoom-in",  data_editor->data);
  SET_SENSITIVE ("palette-editor-zoom-all", data_editor->data);

#undef SET_SENSITIVE
}
