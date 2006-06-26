/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpselectbutton.c
 * Copyright (C) 2003  Sven Neumann  <sven@gimp.org>
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

#include "gimp.h"
#include "gimpuitypes.h"
#include "gimpselectbutton.h"


/*  local function prototypes  */

static void   gimp_select_button_destroy      (GtkObject    *object);


G_DEFINE_TYPE (GimpSelectButton, gimp_select_button, GTK_TYPE_HBOX)

static void
gimp_select_button_class_init (GimpSelectButtonClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

  object_class->destroy = gimp_select_button_destroy;
}

static void
gimp_select_button_init (GimpSelectButton *select_button)
{
  select_button->temp_callback = NULL;
}

/**
 * gimp_select_button_close_popup:
 * @select_button: A #GimpSelectButton
 *
 * Closes the popup window associated with @select_button.
 *
 * Since: GIMP 2.4
 */
void
gimp_select_button_close_popup (GimpSelectButton *select_button)
{
  g_return_if_fail (GIMP_IS_SELECT_BUTTON (select_button));
 
  if (select_button->temp_callback)
    {
      GIMP_SELECT_BUTTON_GET_CLASS (select_button)->select_destroy
        (select_button->temp_callback);
      select_button->temp_callback = NULL;
    }
}


/*  private functions  */

static void
gimp_select_button_destroy (GtkObject *object)
{
  GimpSelectButton *select_button = GIMP_SELECT_BUTTON (object);

  gimp_select_button_close_popup (select_button);

  GTK_OBJECT_CLASS (gimp_select_button_parent_class)->destroy (object);
}
