/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpcolorhexentry.c
 * Copyright (C) 2004  Sven Neumann <sven@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "libgimpcolor/gimpcolor.h"

#include "gimpwidgetstypes.h"

#include "gimpcellrenderercolor.h"
#include "gimpcolorhexentry.h"


enum
{
  COLOR_CHANGED,
  LAST_SIGNAL
};

enum
{
  COLUMN_NAME,
  COLUMN_COLOR,
  NUM_COLUMNS
};


static void      gimp_color_hex_entry_class_init (GimpColorHexEntryClass *klass);
static void      gimp_color_hex_entry_init       (GimpColorHexEntry  *entry);
static gboolean  gimp_color_hex_entry_events     (GtkWidget          *widget,
                                                  GdkEvent           *event);

static gboolean  gimp_color_hex_entry_matched    (GtkEntryCompletion *completion,
                                                  GtkTreeModel       *model,
                                                  GtkTreeIter        *iter,
                                                  GimpColorHexEntry  *entry);


static guint  entry_signals[LAST_SIGNAL] = { 0 };


GType
gimp_color_hex_entry_get_type (void)
{
  static GType entry_type = 0;

  if (! entry_type)
    {
      static const GTypeInfo entry_info =
      {
        sizeof (GimpColorHexEntryClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_color_hex_entry_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpColorHexEntry),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_color_hex_entry_init,
      };

      entry_type = g_type_register_static (GTK_TYPE_ENTRY,
                                           "GimpColorHexEntry",
                                           &entry_info, 0);
    }

  return entry_type;
}

static void
gimp_color_hex_entry_class_init (GimpColorHexEntryClass *klass)
{
  entry_signals[COLOR_CHANGED] =
    g_signal_new ("color_changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpColorHexEntryClass, color_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  klass->color_changed = NULL;
}

static void
gimp_color_hex_entry_init (GimpColorHexEntry *entry)
{
  GtkEntryCompletion  *completion;
  GtkCellRenderer     *cell;
  GtkListStore        *store;
  GimpRGB             *colors;
  const gchar        **names;
  gint                 num_colors;
  gint                 i;

  gimp_rgba_set (&entry->color, 0.0, 0.0, 0.0, 1.0);

  store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, GIMP_TYPE_RGB);

  num_colors = gimp_rgb_list_names (&names, &colors);

  for (i = 0; i < num_colors; i++)
    {
      GtkTreeIter  iter;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_NAME,  names[i],
                          COLUMN_COLOR, colors + i,
                          -1);
    }

  g_free (colors);
  g_free (names);

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION,
                             "model", store,
                             NULL);
  g_object_unref (store);

  cell = gimp_cell_renderer_color_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (completion), cell,
                                  "color", COLUMN_COLOR,
                                  NULL);

  gtk_entry_completion_set_text_column (completion, COLUMN_NAME);

  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);

  gtk_entry_set_text (GTK_ENTRY (entry), "000000");

  g_signal_connect (entry, "focus_out_event",
		    G_CALLBACK (gimp_color_hex_entry_events),
		    NULL);
  g_signal_connect (entry, "key_press_event",
		    G_CALLBACK (gimp_color_hex_entry_events),
		    NULL);

  g_signal_connect (completion, "match_selected",
                    G_CALLBACK (gimp_color_hex_entry_matched),
                    entry);
}

/**
 * gimp_color_hex_entry_new:
 *
 * Return value: a new #GimpColorHexEntry widget
 *
 * Since: GIMP 2.2
 **/
GtkWidget *
gimp_color_hex_entry_new (void)
{
  return g_object_new (GIMP_TYPE_COLOR_HEX_ENTRY, NULL);
}

/**
 * gimp_color_hex_entry_set_color:
 * @entry: a #GimpColorHexEntry widget
 * @color: pointer to a #GimpRGB
 *
 * Sets the color displayed by a #GimpColorHexEntry. If the new color
 * is different to the previously set color, the "color_changed"
 * signal is emitted.
 *
 * Since: GIMP 2.2
 **/
void
gimp_color_hex_entry_set_color (GimpColorHexEntry *entry,
                                const GimpRGB     *color)
{
  g_return_if_fail (GIMP_IS_COLOR_HEX_ENTRY (entry));
  g_return_if_fail (color != NULL);

  if (gimp_rgb_distance (&entry->color, color) > 0.0)
    {
      gchar   buffer[8];
      guchar  r, g, b;

      gimp_rgb_set (&entry->color, color->r, color->g, color->b);
      gimp_rgb_clamp (&entry->color);

      gimp_rgb_get_uchar (&entry->color, &r, &g, &b);
      g_snprintf (buffer, sizeof (buffer), "%.2x%.2x%.2x", r, g, b);

      gtk_entry_set_text (GTK_ENTRY (entry), buffer);

      /* move cursor to the end */
      gtk_editable_set_position (GTK_EDITABLE (entry), -1);

     g_signal_emit (entry, entry_signals[COLOR_CHANGED], 0);
    }
}

/**
 * gimp_color_hex_entry_get_color:
 * @entry: a #GimpColorHexEntry widget
 * @color: pointer to a #GimpRGB
 *
 * Retrieves the color value displayed by a #GimpColorHexEntry.
 *
 * Since: GIMP 2.2
 **/
void
gimp_color_hex_entry_get_color (GimpColorHexEntry *entry,
                                GimpRGB           *color)
{
  g_return_if_fail (GIMP_IS_COLOR_HEX_ENTRY (entry));
  g_return_if_fail (color != NULL);

  *color = entry->color;
}

static gboolean
gimp_color_hex_entry_events (GtkWidget *widget,
                             GdkEvent  *event)
{
  GimpColorHexEntry *entry = GIMP_COLOR_HEX_ENTRY (widget);
  const gchar       *text;
  gchar              buffer[8];
  guchar             r, g, b;

  switch (event->type)
    {
    case GDK_KEY_PRESS:
      if (((GdkEventKey *) event)->keyval != GDK_Return)
        break;
      /*  else fall through  */

    case GDK_FOCUS_CHANGE:
      text = gtk_entry_get_text (GTK_ENTRY (widget));

      gimp_rgb_get_uchar (&entry->color, &r, &g, &b);
      g_snprintf (buffer, sizeof (buffer), "%.2x%.2x%.2x", r, g, b);

      if (g_ascii_strcasecmp (buffer, text) != 0)
        {
          GimpRGB  color;
          gsize    len = strlen (text);

          if (len > 0&&
              ((len % 3 == 0 && gimp_rgb_parse_hex (&color, text, len)) ||
               (gimp_rgb_parse_name (&color, text, -1))))
	    {
              gimp_color_hex_entry_set_color (entry, &color);
	    }
          else
            {
              gtk_entry_set_text (GTK_ENTRY (entry), buffer);
            }
        }
      break;

    default:
      /*  do nothing  */
      break;
    }

  return FALSE;
}

static gboolean
gimp_color_hex_entry_matched (GtkEntryCompletion *completion,
                              GtkTreeModel       *model,
                              GtkTreeIter        *iter,
                              GimpColorHexEntry  *entry)
{
  gchar   *name;
  GimpRGB  color;

  gtk_tree_model_get (model, iter,
                      COLUMN_NAME, &name,
                      -1);

  if (gimp_rgb_parse_name (&color, name, -1))
    gimp_color_hex_entry_set_color (entry, &color);

  g_free (name);

  return TRUE;
}
