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

#include "gui-types.h"

#include "core/gimp.h"
#include "core/gimpchannel.h"
#include "core/gimpdrawable.h"
#include "core/gimpdrawable-offset.h"
#include "core/gimplayer.h"
#include "core/gimplayermask.h"
#include "core/gimpimage.h"

#include "widgets/gimphelp-ids.h"
#include "widgets/gimpviewabledialog.h"

#include "offset-dialog.h"

#include "gimp-intl.h"


#define ENTRY_WIDTH  60

#define WRAP_AROUND  (1 << 3)
#define FILL_MASK    (GIMP_OFFSET_BACKGROUND | GIMP_OFFSET_TRANSPARENT)


typedef struct _OffsetDialog OffsetDialog;

struct _OffsetDialog
{
  GtkWidget      *dlg;
  GtkWidget      *off_se;

  GimpOffsetType  fill_type;

  GimpImage      *gimage;
};


/*  local function prototypes  */

static void  offset_response            (GtkWidget    *widget,
                                         gint          response_id,
					 OffsetDialog *off_d);
static void  offset_halfheight_callback (GtkWidget    *widget,
					 OffsetDialog *off_d);


/*  public functions  */

GtkWidget *
offset_dialog_new (GimpDrawable *drawable,
                   GtkWidget    *parent)
{
  OffsetDialog *off_d;
  GtkWidget    *main_vbox;
  GtkWidget    *vbox;
  GtkWidget    *hbox;
  GtkWidget    *button;
  GtkWidget    *spinbutton;
  GtkWidget    *frame;
  GtkWidget    *radio_button;
  GtkObject    *adjustment;
  const gchar  *title = NULL;

  g_return_val_if_fail (GIMP_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);

  off_d = g_new0 (OffsetDialog, 1);

  off_d->fill_type = gimp_drawable_has_alpha (drawable) | WRAP_AROUND;
  off_d->gimage    = gimp_item_get_image (GIMP_ITEM (drawable));

  if (GIMP_IS_LAYER (drawable))
    title = _("Offset Layer");
  else if (GIMP_IS_LAYER_MASK (drawable))
    title = _("Offset Layer Mask");
  else if (GIMP_IS_CHANNEL (drawable))
    title = _("Offset Channel");
  else
    g_warning ("%s: unexpected drawable type", G_STRLOC);

  off_d->dlg = gimp_viewable_dialog_new (GIMP_VIEWABLE (drawable),
                                         _("Offset"), "gimp-drawable-offset",
                                         GIMP_STOCK_TOOL_MOVE,
                                         title,
                                         parent,
                                         gimp_standard_help_func,
                                         GIMP_HELP_LAYER_OFFSET,

                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,     GTK_RESPONSE_OK,

                                         NULL);

  gtk_window_set_resizable (GTK_WINDOW (off_d->dlg), FALSE);

  g_object_weak_ref (G_OBJECT (off_d->dlg), (GWeakNotify) g_free, off_d);

  g_signal_connect (off_d->dlg, "response",
                    G_CALLBACK (offset_response),
                    off_d);

  main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (off_d->dlg)->vbox), main_vbox);
  gtk_widget_show (main_vbox);

  /*  The offset frame  */
  frame = gimp_frame_new (_("Offset"));
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  hbox = gtk_hbox_new (0, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  vbox = gtk_vbox_new (6, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);

  spinbutton = gimp_spin_button_new (&adjustment,
                                     1, 1, 1, 1, 10, 1,
                                     1, 2);
  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton), 10);

  off_d->off_se = gimp_size_entry_new (1, off_d->gimage->unit, "%a",
				       TRUE, TRUE, FALSE, 10,
				       GIMP_SIZE_ENTRY_UPDATE_SIZE);
  gtk_table_set_col_spacing (GTK_TABLE (off_d->off_se), 0, 4);
  gtk_table_set_col_spacing (GTK_TABLE (off_d->off_se), 1, 4);
  gtk_table_set_row_spacing (GTK_TABLE (off_d->off_se), 0, 2);

  gimp_size_entry_add_field (GIMP_SIZE_ENTRY (off_d->off_se),
                             GTK_SPIN_BUTTON (spinbutton), NULL);
  gtk_table_attach_defaults (GTK_TABLE (off_d->off_se), spinbutton,
                             1, 2, 0, 1);
  gtk_widget_show (spinbutton);

  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (off_d->off_se),
                                _("_X:"), 0, 0, 1.0);
  gimp_size_entry_attach_label (GIMP_SIZE_ENTRY (off_d->off_se),
                                _("_Y:"), 1, 0, 1.0);

  gtk_box_pack_start (GTK_BOX (vbox), off_d->off_se, FALSE, FALSE, 0);
  gtk_widget_show (off_d->off_se);

  gimp_size_entry_set_unit (GIMP_SIZE_ENTRY (off_d->off_se), GIMP_UNIT_PIXEL);

  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (off_d->off_se), 0,
                                  off_d->gimage->xresolution, FALSE);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (off_d->off_se), 1,
                                  off_d->gimage->yresolution, FALSE);

  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (off_d->off_se), 0,
                                         -off_d->gimage->width,
					 off_d->gimage->width);
  gimp_size_entry_set_refval_boundaries (GIMP_SIZE_ENTRY (off_d->off_se), 1,
					 -off_d->gimage->height,
					 off_d->gimage->height);

  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (off_d->off_se), 0,
                            0, off_d->gimage->width);
  gimp_size_entry_set_size (GIMP_SIZE_ENTRY (off_d->off_se), 1,
                            0, off_d->gimage->height);

  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (off_d->off_se), 0, 0);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (off_d->off_se), 1, 0);

  button = gtk_button_new_with_mnemonic (_("Offset by (x/_2),(y/2)"));
  gtk_container_set_border_width (GTK_CONTAINER (button), 2);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (button, "clicked",
		    G_CALLBACK (offset_halfheight_callback),
		    off_d);

  /*  The edge behaviour frame  */
  frame = gimp_int_radio_group_new (TRUE, _("Edge Behaviour"),
                                    G_CALLBACK (gimp_radio_button_update),
                                    &off_d->fill_type, off_d->fill_type,

                                    _("_Wrap Around"),
                                    WRAP_AROUND, NULL,

                                    _("Fill with _Background Color"),
                                    GIMP_OFFSET_BACKGROUND, NULL,

                                    _("Make _Transparent"),
                                    GIMP_OFFSET_TRANSPARENT, &radio_button,
                                    NULL);

  if (! gimp_drawable_has_alpha (drawable))
    gtk_widget_set_sensitive (radio_button, FALSE);

  gtk_box_pack_start (GTK_BOX (main_vbox), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  return off_d->dlg;
}


/*  private functions  */

static void
offset_response (GtkWidget    *widget,
                 gint          response_id,
                 OffsetDialog *off_d)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      GimpImage    *gimage;
      GimpDrawable *drawable;
      gint          offset_x;
      gint          offset_y;

      if ((gimage = off_d->gimage) != NULL)
        {
          drawable = gimp_image_active_drawable (gimage);

          offset_x =
            RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (off_d->off_se),
                                              0));
          offset_y =
            RINT (gimp_size_entry_get_refval (GIMP_SIZE_ENTRY (off_d->off_se),
                                              1));

          gimp_drawable_offset (drawable,
                                gimp_get_user_context (gimage->gimp),
                                off_d->fill_type & WRAP_AROUND ? TRUE : FALSE,
                                off_d->fill_type & FILL_MASK,
                                offset_x, offset_y);
          gimp_image_flush (gimage);
        }
    }

  gtk_widget_destroy (off_d->dlg);
}

static void
offset_halfheight_callback (GtkWidget    *widget,
                            OffsetDialog *off_d)
{
  GimpImage *gimage = off_d->gimage;

  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (off_d->off_se),
			      0, gimage->width / 2);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY (off_d->off_se),
			      1, gimage->height / 2);
}
