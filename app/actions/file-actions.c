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

#include "config/gimpguiconfig.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpimage.h"

#include "file/file-utils.h"

#include "widgets/gimpactiongroup.h"
#include "widgets/gimphelp-ids.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"

#include "file-actions.h"
#include "file-commands.h"

#include "gimp-intl.h"


/*  local function prototypes  */

static void   file_actions_last_opened_update  (GimpContainer   *container,
                                                GimpImagefile   *unused,
                                                GimpActionGroup *group);
static void   file_actions_last_opened_reorder (GimpContainer   *container,
                                                GimpImagefile   *unused1,
                                                gint             unused2,
                                                GimpActionGroup *group);


static GimpActionEntry file_actions[] =
{
  { "file-menu", NULL,
    N_("_File") },

  { "file-open-recent-menu", NULL,
    N_("Open _Recent") },

  { "file-open-recent-empty", NULL,
    N_("(Empty)") },

  { "file-new", GTK_STOCK_NEW,
    N_("_New..."), "<control>N", NULL,
    G_CALLBACK (file_new_cmd_callback),
    GIMP_HELP_FILE_NEW },

  { "file-open", GTK_STOCK_OPEN,
    N_("_Open..."), "<control>O", NULL,
    G_CALLBACK (file_open_cmd_callback),
    GIMP_HELP_FILE_OPEN },

  { "file-save", GTK_STOCK_SAVE,
    N_("_Save"), "<control>S", NULL,
    G_CALLBACK (file_save_cmd_callback),
    GIMP_HELP_FILE_SAVE },

  { "file-save-as", GTK_STOCK_SAVE_AS,
    N_("Save _as..."), "<control><shift>S", NULL,
    G_CALLBACK (file_save_as_cmd_callback),
    GIMP_HELP_FILE_SAVE_AS },

  { "file-save-a-copy", NULL,
    N_("Save a Cop_y..."), NULL, NULL,
    G_CALLBACK (file_save_a_copy_cmd_callback),
    GIMP_HELP_FILE_SAVE_A_COPY },

  { "file-save-as-template", NULL,
    N_("Save as _Template..."), NULL, NULL,
    G_CALLBACK (file_save_template_cmd_callback),
    GIMP_HELP_FILE_SAVE_AS_TEMPLATE },

  { "file-revert", GTK_STOCK_REVERT_TO_SAVED,
    N_("Re_vert"), NULL, NULL,
    G_CALLBACK (file_revert_cmd_callback),
    GIMP_HELP_FILE_REVERT },

  { "file-close", GTK_STOCK_CLOSE,
    N_( "_Close"), "<control>W", NULL,
    G_CALLBACK (file_close_cmd_callback),
    GIMP_HELP_FILE_CLOSE },

  { "file-quit", GTK_STOCK_QUIT,
    N_("_Quit"), "<control>Q", NULL,
    G_CALLBACK (file_quit_cmd_callback),
    GIMP_HELP_FILE_QUIT }
};


void
file_actions_setup (GimpActionGroup *group,
                    gpointer         data)
{
  GimpEnumActionEntry *entries;
  gint                 n_entries;
  gint                 i;

  gimp_action_group_add_actions (group,
                                 file_actions,
                                 G_N_ELEMENTS (file_actions),
                                 data);

  n_entries = GIMP_GUI_CONFIG (group->gimp->config)->last_opened_size;

  entries = g_new0 (GimpEnumActionEntry, n_entries);

  for (i = 0; i < n_entries; i++)
    {
      entries[i].name     = g_strdup_printf ("file-last-opened-%02d", i + 1);
      entries[i].stock_id = GTK_STOCK_OPEN;
      entries[i].label    = NULL;
      entries[i].tooltip  = NULL;
      entries[i].value    = i;
      entries[i].help_id  = GIMP_HELP_FILE_OPEN_RECENT;

      if (i < 9)
        entries[i].accelerator = g_strdup_printf ("<control>%d", i + 1);
      else if (i == 9)
        entries[i].accelerator = "<control>0";
      else
        entries[i].accelerator = "";
    }

  gimp_action_group_add_enum_actions (group, entries, n_entries,
                                      G_CALLBACK (file_last_opened_cmd_callback),
                                      data);

  gimp_action_group_set_action_sensitive (group, "file-open-recent-empty",
                                          FALSE);

  for (i = 0; i < n_entries; i++)
    {
      gimp_action_group_set_action_visible (group, entries[i].name, FALSE);

      g_free ((gchar *) entries[i].name);

      if (i < 9)
        g_free ((gchar *) entries[i].accelerator);
    }

  g_free (entries);

  g_signal_connect_object (group->gimp->documents, "add",
                           G_CALLBACK (file_actions_last_opened_update),
                           group, 0);
  g_signal_connect_object (group->gimp->documents, "remove",
                           G_CALLBACK (file_actions_last_opened_update),
                           group, 0);
  g_signal_connect_object (group->gimp->documents, "reorder",
                           G_CALLBACK (file_actions_last_opened_reorder),
                           group, 0);

  file_actions_last_opened_update (group->gimp->documents, NULL, group);
}

void
file_actions_update (GimpActionGroup *group,
                     gpointer         data)
{
  GimpDisplay      *gdisp    = NULL;
  GimpDisplayShell *shell    = NULL;
  GimpImage        *gimage   = NULL;
  GimpDrawable     *drawable = NULL;

  if (GIMP_IS_DISPLAY_SHELL (data))
    {
      shell = GIMP_DISPLAY_SHELL (data);
      gdisp = shell->gdisp;
    }
  else if (GIMP_IS_DISPLAY (data))
    {
      gdisp = GIMP_DISPLAY (data);
      shell = GIMP_DISPLAY_SHELL (gdisp->shell);
    }

  if (gdisp)
    {
      gimage = gdisp->gimage;

      drawable = gimp_image_active_drawable (gimage);
    }

#define SET_SENSITIVE(action,condition) \
        gimp_action_group_set_action_sensitive (group, action, (condition) != 0)

  SET_SENSITIVE ("file-save",             gdisp && drawable);
  SET_SENSITIVE ("file-save-as",          gdisp && drawable);
  SET_SENSITIVE ("file-save-a-copy",      gdisp && drawable);
  SET_SENSITIVE ("file-save-as-template", gdisp);
  SET_SENSITIVE ("file-revert",           gdisp && GIMP_OBJECT (gimage)->name);
  SET_SENSITIVE ("file-close",            gdisp);

#undef SET_SENSITIVE
}


/*  private functions  */

static void
file_actions_last_opened_update (GimpContainer   *container,
                                 GimpImagefile   *unused,
                                 GimpActionGroup *group)
{
  gint num_documents;
  gint i;
  gint n = GIMP_GUI_CONFIG (group->gimp->config)->last_opened_size;

  num_documents = gimp_container_num_children (container);

  gimp_action_group_set_action_visible (group,
                                        "file-open-recent-empty",
                                        num_documents == 0);

  for (i = 0; i < n; i++)
    {
      GtkAction *action;
      gchar     *name = g_strdup_printf ("file-open-recent-%02d", i + 1);

      action = gtk_action_group_get_action (GTK_ACTION_GROUP (group), name);

      if (i < num_documents)
        {
          GimpImagefile *imagefile = (GimpImagefile *)
            gimp_container_get_child_by_index (container, i);

          if (g_object_get_data (G_OBJECT (action), "gimp-imagefile") !=
              (gpointer) imagefile)
            {
              const gchar *uri;
              gchar       *filename;
              gchar       *basename;

              uri = gimp_object_get_name (GIMP_OBJECT (imagefile));

              filename = file_utils_uri_to_utf8_filename (uri);
              basename = file_utils_uri_to_utf8_basename (uri);

              g_object_set (G_OBJECT (action),
                            "label",   basename,
                            "tooltip", filename,
                            "visible", TRUE,
                            NULL);

#if 0
              gimp_help_set_help_data (widget, filename, NULL);
#endif

              g_free (filename);
              g_free (basename);

              g_object_set_data (G_OBJECT (action),
				 "gimp-imagefile", imagefile);
            }
        }
      else
        {
          g_object_set_data (G_OBJECT (action), "gimp-imagefile", NULL);
          g_object_set (G_OBJECT (action), "visible", FALSE, NULL);
        }

      g_free (name);
    }
}

static void
file_actions_last_opened_reorder (GimpContainer   *container,
                                  GimpImagefile   *unused1,
                                  gint             unused2,
                                  GimpActionGroup *group)
{
  file_actions_last_opened_update (container, unused1, group);
}
