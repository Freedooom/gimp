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

#include "core/gimpimage.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimphelp-ids.h"

#include "actions.h"
#include "colormap-editor-actions.h"
#include "colormap-editor-commands.h"

#include "gimp-intl.h"


static GimpActionEntry colormap_editor_actions[] =
{
  { "colormap-editor-popup", GIMP_STOCK_INDEXED_PALETTE,
    N_("Indexed Palette Menu"), NULL, NULL, NULL,
    GIMP_HELP_INDEXED_PALETTE_DIALOG },

  { "colormap-editor-edit-color", GIMP_STOCK_EDIT,
    N_("_Edit Color..."), NULL, NULL,
    G_CALLBACK (colormap_editor_edit_color_cmd_callback),
    GIMP_HELP_INDEXED_PALETTE_EDIT }
};

static GimpEnumActionEntry colormap_editor_add_color_actions[] =
{
  { "colormap-editor-add-color-from-fg", GTK_STOCK_ADD,
    N_("_Add Color from FG"), "", NULL,
    FALSE,
    GIMP_HELP_INDEXED_PALETTE_ADD },

  { "colormap-editor-add-color-from-bg", GTK_STOCK_ADD,
    N_("_Add Color from BG"), "", NULL,
    TRUE,
    GIMP_HELP_INDEXED_PALETTE_ADD }
};


void
colormap_editor_actions_setup (GimpActionGroup *group)
{
  gimp_action_group_add_actions (group,
                                 colormap_editor_actions,
                                 G_N_ELEMENTS (colormap_editor_actions));

  gimp_action_group_add_enum_actions (group,
                                      colormap_editor_add_color_actions,
                                      G_N_ELEMENTS (colormap_editor_add_color_actions),
                                      G_CALLBACK (colormap_editor_add_color_cmd_callback));
}

void
colormap_editor_actions_update (GimpActionGroup *group,
                                gpointer         data)
{
  GimpImage *gimage;
  gboolean   indexed    = FALSE;
  gint       num_colors = 0;

  gimage = action_data_get_image (data);

  if (gimage)
    {
      indexed    = gimp_image_base_type (gimage) == GIMP_INDEXED;
      num_colors = gimage->num_cols;
    }

#define SET_SENSITIVE(action,condition) \
        gimp_action_group_set_action_sensitive (group, action, (condition) != 0)

  SET_SENSITIVE ("colormap-editor-edit-color",
                 gimage && indexed);
  SET_SENSITIVE ("colormap-editor-add-color-from-fg",
                 gimage && indexed && num_colors < 256);
  SET_SENSITIVE ("colormap-editor-add-color-from-bg",
                 gimage && indexed && num_colors < 256);

#undef SET_SENSITIVE
}
