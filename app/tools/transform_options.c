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

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "config/gimpcoreconfig.h"

#include "core/gimp.h"
#include "core/gimptoolinfo.h"

#include "widgets/gimpenummenu.h"
#include "widgets/gimpwidgets-utils.h"

#include "gimprotatetool.h"
#include "gimpscaletool.h"
#include "gimptransformtool.h"
#include "transform_options.h"
#include "tool_manager.h"

#include "libgimp/gimpintl.h"


/*  local function prototypes  */

static void   gimp_transform_options_init       (GimpTransformOptions      *options);
static void   gimp_transform_options_class_init (GimpTransformOptionsClass *options_class);

static void gimp_transform_tool_grid_type_update    (GtkWidget            *widget,
                                                     GimpTransformOptions *options);
static void gimp_transform_tool_grid_density_update (GtkAdjustment        *adj,
                                                     GimpTransformOptions *options);
static void gimp_transform_tool_show_path_update    (GtkWidget            *widget,
                                                     GimpTransformOptions *options);


GType
gimp_transform_options_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      static const GTypeInfo info =
      {
        sizeof (GimpTransformOptionsClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_transform_options_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpTransformOptions),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_transform_options_init,
      };

      type = g_type_register_static (GIMP_TYPE_TOOL_OPTIONS,
                                     "GimpTransformOptions",
                                     &info, 0);
    }

  return type;
}

static void 
gimp_transform_options_class_init (GimpTransformOptionsClass *klass)
{
}

static void
gimp_transform_options_init (GimpTransformOptions *options)
{
  options->direction     = options->direction_d    = GIMP_TRANSFORM_FORWARD;
  options->show_path     = options->show_path_d    = TRUE;
  options->clip          = options->clip_d         = FALSE;
  options->grid_type     = options->grid_type_d    = TRANSFORM_GRID_TYPE_N_LINES;
  options->grid_size     = options->grid_size_d    = 15;
  options->constrain_1   = options->constrain_1_d  = FALSE;
  options->constrain_2   = options->constrain_2_d  = FALSE;
}

void
gimp_transform_options_gui (GimpToolOptions *tool_options)
{
  GimpTransformOptions *options;
  GtkWidget            *vbox;
  GtkWidget            *hbox;
  GtkWidget            *label;
  GtkWidget            *frame;
  GtkWidget            *table;
  GtkWidget            *grid_density;

  options = GIMP_TRANSFORM_OPTIONS (tool_options);

  tool_options->reset_func = gimp_transform_options_reset;

  vbox = tool_options->main_vbox;

  options->interpolation =
    tool_options->tool_info->gimp->config->interpolation_type;

  frame = gimp_enum_radio_frame_new (GIMP_TYPE_TRANSFORM_DIRECTION,
                                     gtk_label_new (_("Transform Direction")),
                                     2,
                                     G_CALLBACK (gimp_radio_button_update),
                                     &options->direction,
                                     &options->direction_w);
  gimp_radio_group_set_active (GTK_RADIO_BUTTON (options->direction_w),
                               GINT_TO_POINTER (options->direction));

  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /*  the interpolation menu  */
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new (_("Interpolation:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  options->interpolation_w = 
    gimp_enum_option_menu_new (GIMP_TYPE_INTERPOLATION_TYPE,
                               G_CALLBACK (gimp_menu_item_update),
                               &options->interpolation);
  gimp_option_menu_set_history (GTK_OPTION_MENU (options->interpolation_w),
                                GINT_TO_POINTER (options->interpolation));
  gtk_box_pack_start (GTK_BOX (hbox), 
                      options->interpolation_w, FALSE, FALSE, 0);
  gtk_widget_show (options->interpolation_w);

  /*  the clip resulting image toggle button  */
  options->clip_w = gtk_check_button_new_with_label (_("Clip Result"));
  gtk_box_pack_start (GTK_BOX (vbox), options->clip_w, FALSE, FALSE, 0);
  gtk_widget_show (options->clip_w);

  g_signal_connect (options->clip_w, "toggled",
                    G_CALLBACK (gimp_toggle_button_update),
                    &options->clip);

  /*  the grid frame  */
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  /*  the grid type menu  */
  options->grid_type_w =
    gimp_option_menu_new2 (FALSE,
                           G_CALLBACK (gimp_transform_tool_grid_type_update),
                           options,
                           GINT_TO_POINTER (options->grid_type_d),

                           _("Don't Show Grid"),
                           GINT_TO_POINTER (TRANSFORM_GRID_TYPE_NONE), NULL,

                           _("Number of Grid Lines"),
                           GINT_TO_POINTER (TRANSFORM_GRID_TYPE_N_LINES), NULL,

                           _("Grid Line Spacing"),
                           GINT_TO_POINTER (TRANSFORM_GRID_TYPE_SPACING), NULL,

                           NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), options->grid_type_w);
  gtk_widget_show (options->grid_type_w);
  
  /*  the grid density scale  */
  table = gtk_table_new (1, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_widget_show (table);

  options->grid_size_w = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                                               _("Density:"), -1, -1,
                                               options->grid_size,
                                               1.0, 128.0, 1.0, 8.0, 0,
                                               TRUE, 0.0, 0.0,
                                               NULL, NULL);
  grid_density = GIMP_SCALE_ENTRY_SPINBUTTON (options->grid_size_w);

  g_signal_connect (options->grid_size_w, "value_changed",
                    G_CALLBACK (gimp_transform_tool_grid_density_update),
                    options);

  /*  the show_path toggle button  */
  options->show_path_w = gtk_check_button_new_with_label (_("Show Path"));
  gtk_box_pack_start (GTK_BOX (vbox), options->show_path_w, FALSE, FALSE, 0);
  gtk_widget_show (options->show_path_w);

  g_signal_connect (options->show_path_w, "toggled",
                    G_CALLBACK (gimp_transform_tool_show_path_update),
                    options);

  if (tool_options->tool_info->tool_type == GIMP_TYPE_ROTATE_TOOL ||
      tool_options->tool_info->tool_type == GIMP_TYPE_SCALE_TOOL)
    {
      GtkWidget *vbox2;
      gchar     *str;

      /*  the constraints frame  */
      frame = gtk_frame_new (_("Constraints"));
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
      gtk_widget_show (frame);

      vbox2 = gtk_vbox_new (FALSE, 2);
      gtk_container_set_border_width (GTK_CONTAINER (vbox2), 2);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);
      gtk_widget_show (vbox2);

      if (tool_options->tool_info->tool_type == GIMP_TYPE_ROTATE_TOOL)
        {
          str = g_strdup_printf (_("15 Degrees  %s"),
                                 gimp_get_mod_name_control ());

          options->constrain_1_w = gtk_check_button_new_with_label (str);
          gtk_box_pack_start (GTK_BOX (vbox2), options->constrain_1_w,
                              FALSE, FALSE, 0);
          gtk_widget_show (options->constrain_1_w);

          g_free (str);

          g_signal_connect (options->constrain_1_w, "toggled",
                            G_CALLBACK (gimp_toggle_button_update),
                            &options->constrain_1);
        }
      else if (tool_options->tool_info->tool_type == GIMP_TYPE_SCALE_TOOL)
        {
          str = g_strdup_printf (_("Keep Height  %s"),
                                 gimp_get_mod_name_control ());

          options->constrain_1_w = gtk_check_button_new_with_label (str);
          gtk_box_pack_start (GTK_BOX (vbox2), options->constrain_1_w,
                              FALSE, FALSE, 0);
          gtk_widget_show (options->constrain_1_w);

          g_free (str);

          gimp_help_set_help_data (options->constrain_1_w,
                                   _("Activate both the \"Keep Height\" and\n"
                                     "\"Keep Width\" toggles to constrain\n"
                                     "the aspect ratio"), NULL);

          g_signal_connect (options->constrain_1_w, "toggled",
                            G_CALLBACK (gimp_toggle_button_update),
                            &options->constrain_1);

          str = g_strdup_printf (_("Keep Width  %s"),
                                 gimp_get_mod_name_alt ());

          options->constrain_2_w = gtk_check_button_new_with_label (str);
          gtk_box_pack_start (GTK_BOX (vbox2), options->constrain_2_w,
                              FALSE, FALSE, 0);
          gtk_widget_show (options->constrain_2_w);

          g_free (str);

          gimp_help_set_help_data (options->constrain_2_w,
                                   _("Activate both the \"Keep Height\" and\n"
                                     "\"Keep Width\" toggles to constrain\n"
                                     "the aspect ratio"), NULL);

          g_signal_connect (options->constrain_2_w, "toggled",
                            G_CALLBACK (gimp_toggle_button_update),
                            &options->constrain_2);
        }
    }

  /* Set options to default values */
  gimp_transform_options_reset (tool_options);
}

void
gimp_transform_options_reset (GimpToolOptions *tool_options)
{
  GimpTransformOptions *options;

  options = GIMP_TRANSFORM_OPTIONS (tool_options);

  gimp_radio_group_set_active (GTK_RADIO_BUTTON (options->direction_w),
                               GINT_TO_POINTER (options->direction_d));

  options->interpolation =
    tool_options->tool_info->gimp->config->interpolation_type;
  gimp_option_menu_set_history (GTK_OPTION_MENU (options->interpolation_w),
				GINT_TO_POINTER (options->interpolation));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options->clip_w),
				options->clip_d);

  gimp_option_menu_set_history (GTK_OPTION_MENU (options->grid_type_w),
                                GINT_TO_POINTER (options->grid_type_d));
  options->grid_type = options->grid_type_d;

  {
    GimpToolOptions *tool_options;
    GimpTool        *active_tool;

    tool_options = (GimpToolOptions *) options;

    active_tool = tool_manager_get_active (tool_options->tool_info->gimp);

    if (GIMP_IS_TRANSFORM_TOOL (active_tool))
      gimp_transform_tool_grid_density_changed (GIMP_TRANSFORM_TOOL (active_tool));

    gtk_widget_set_sensitive (GTK_BIN (options->grid_type_w->parent)->child,
                              options->grid_type != TRANSFORM_GRID_TYPE_NONE);
  }

  gtk_adjustment_set_value (GTK_ADJUSTMENT (options->grid_size_w),
			    options->grid_size_d);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options->show_path_w),
				options->show_path_d);

  if (options->constrain_1_w)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options->constrain_1_w),
                                  options->constrain_1_d);

  if (options->constrain_2_w)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options->constrain_2_w),
                                  options->constrain_2_d);
}


/*  private functions  */

static void
gimp_transform_tool_grid_type_update (GtkWidget            *widget,
        	 		      GimpTransformOptions *options)
{
  GimpToolOptions *tool_options;
  GimpTool        *active_tool;

  tool_options = GIMP_TOOL_OPTIONS (options);

  gimp_menu_item_update (widget, &options->grid_type);

  active_tool = tool_manager_get_active (tool_options->tool_info->gimp);

  if (GIMP_IS_TRANSFORM_TOOL (active_tool))
    gimp_transform_tool_grid_density_changed (GIMP_TRANSFORM_TOOL (active_tool));

  gtk_widget_set_sensitive (GTK_BIN (options->grid_type_w->parent)->child,
                            options->grid_type != TRANSFORM_GRID_TYPE_NONE);
}

static void
gimp_transform_tool_grid_density_update (GtkAdjustment        *adj,
                                         GimpTransformOptions *options)
{
  GimpToolOptions *tool_options;
  GimpTool        *active_tool;

  tool_options = GIMP_TOOL_OPTIONS (options);

  options->grid_size = (gint) (adj->value + 0.5);

  active_tool = tool_manager_get_active (tool_options->tool_info->gimp);

  if (GIMP_IS_TRANSFORM_TOOL (active_tool))
    gimp_transform_tool_grid_density_changed (GIMP_TRANSFORM_TOOL (active_tool));
}

static void
gimp_transform_tool_show_path_update (GtkWidget            *widget,
				      GimpTransformOptions *options)
{
  GimpToolOptions *tool_options;
  GimpTool        *active_tool;

  static gboolean first_call = TRUE;  /* eek, this hack avoids a segfault */

  if (first_call)
    {
      first_call = FALSE;
      return;
    }

  tool_options = GIMP_TOOL_OPTIONS (options);

  active_tool = tool_manager_get_active (tool_options->tool_info->gimp);

  if (GIMP_IS_TRANSFORM_TOOL (active_tool))
    {
      GimpTransformTool *transform_tool;

      transform_tool = GIMP_TRANSFORM_TOOL (active_tool);

      gimp_transform_tool_show_path_changed (transform_tool, 1); /* pause */

      gimp_toggle_button_update (widget, &options->show_path);

      gimp_transform_tool_show_path_changed (transform_tool, 0); /* resume */
    }
  else
    {
      gimp_toggle_button_update (widget, &options->show_path);
    }
}
