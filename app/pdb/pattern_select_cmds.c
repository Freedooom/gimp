/* The GIMP -- an image manipulation program
 * Copyright (C) 1995-2000 Spencer Kimball and Peter Mattis
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

/* NOTE: This file is autogenerated by pdbgen.pl. */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "apptypes.h"
#include "procedural_db.h"

#include "gimpcontext.h"
#include "gimplist.h"
#include "gimppattern.h"
#include "pattern_select.h"
#include "patterns.h"

static ProcRecord patterns_popup_proc;
static ProcRecord patterns_close_popup_proc;
static ProcRecord patterns_set_popup_proc;

void
register_pattern_select_procs (void)
{
  procedural_db_register (&patterns_popup_proc);
  procedural_db_register (&patterns_close_popup_proc);
  procedural_db_register (&patterns_set_popup_proc);
}

static PatternSelect *
pattern_get_patternselect (gchar *name)
{
  GSList *list;
  PatternSelect *psp;

  for (list = pattern_active_dialogs; list; list = g_slist_next (list))
    {
      psp = (PatternSelect *) list->data;
      
      if (psp->callback_name && !strcmp (name, psp->callback_name))
	return psp;
    }

  return NULL;
}

static Argument *
patterns_popup_invoker (Argument *args)
{
  gboolean success = TRUE;
  gchar *name;
  gchar *title;
  gchar *pattern;
  ProcRecord *prec;
  PatternSelect *newdialog;

  name = (gchar *) args[0].value.pdb_pointer;
  if (name == NULL)
    success = FALSE;

  title = (gchar *) args[1].value.pdb_pointer;
  if (title == NULL)
    success = FALSE;

  pattern = (gchar *) args[2].value.pdb_pointer;

  if (success)
    {
      if ((prec = procedural_db_lookup (name)))
	{
	  if (pattern && strlen (pattern))
	    newdialog = pattern_select_new (title, pattern);
	  else
	    newdialog = pattern_select_new (title, NULL);
    
	  /* The callback procedure to run when pattern changes */
	  newdialog->callback_name = g_strdup (name);
	}
      else
	success = FALSE;
    }

  return procedural_db_return_args (&patterns_popup_proc, success);
}

static ProcArg patterns_popup_inargs[] =
{
  {
    PDB_STRING,
    "pattern_callback",
    "The callback PDB proc to call when pattern selection is made"
  },
  {
    PDB_STRING,
    "popup_title",
    "Title to give the pattern popup window"
  },
  {
    PDB_STRING,
    "initial_pattern",
    "The name of the pattern to set as the first selected"
  }
};

static ProcRecord patterns_popup_proc =
{
  "gimp_patterns_popup",
  "Invokes the Gimp pattern selection.",
  "This procedure popups the pattern selection dialog.",
  "Andy Thomas",
  "Andy Thomas",
  "1998",
  PDB_INTERNAL,
  3,
  patterns_popup_inargs,
  0,
  NULL,
  { { patterns_popup_invoker } }
};

static Argument *
patterns_close_popup_invoker (Argument *args)
{
  gboolean success = TRUE;
  gchar *name;
  ProcRecord *prec;
  PatternSelect *psp;

  name = (gchar *) args[0].value.pdb_pointer;
  if (name == NULL)
    success = FALSE;

  if (success)
    {
      if ((prec = procedural_db_lookup (name)) &&
	  (psp = pattern_get_patternselect (name)))
	{
	  if (GTK_WIDGET_VISIBLE (psp->shell))
	    gtk_widget_hide (psp->shell);
    
	  /* Free memory if poping down dialog which is not the main one */
	  if (psp != pattern_select_dialog)
	    {
	      gtk_widget_destroy (psp->shell);
	      pattern_select_free (psp);
	    }
	}
      else
	success = FALSE;
    }

  return procedural_db_return_args (&patterns_close_popup_proc, success);
}

static ProcArg patterns_close_popup_inargs[] =
{
  {
    PDB_STRING,
    "pattern_callback",
    "The name of the callback registered for this popup"
  }
};

static ProcRecord patterns_close_popup_proc =
{
  "gimp_patterns_close_popup",
  "Popdown the Gimp pattern selection.",
  "This procedure closes an opened pattern selection dialog.",
  "Andy Thomas",
  "Andy Thomas",
  "1998",
  PDB_INTERNAL,
  1,
  patterns_close_popup_inargs,
  0,
  NULL,
  { { patterns_close_popup_invoker } }
};

static Argument *
patterns_set_popup_invoker (Argument *args)
{
  gboolean success = TRUE;
  gchar *name;
  gchar *pattern_name;
  ProcRecord *prec;
  PatternSelect *psp;

  name = (gchar *) args[0].value.pdb_pointer;
  if (name == NULL)
    success = FALSE;

  pattern_name = (gchar *) args[1].value.pdb_pointer;
  if (pattern_name == NULL)
    success = FALSE;

  if (success)
    {
      if ((prec = procedural_db_lookup (name)) &&
	  (psp = pattern_get_patternselect (name)))
	{
	  GimpPattern *active =
	    (GimpPattern *) gimp_container_get_child_by_name (global_pattern_list,
							      pattern_name);
    
	  if (active)
	    {
	      /* Must alter the wigdets on screen as well */
	      gimp_context_set_pattern (psp->context, active);
	    }
	  else
	    success = FALSE;
	}
      else
	success = FALSE;
    }

  return procedural_db_return_args (&patterns_set_popup_proc, success);
}

static ProcArg patterns_set_popup_inargs[] =
{
  {
    PDB_STRING,
    "pattern_callback",
    "The name of the callback registered for this popup"
  },
  {
    PDB_STRING,
    "pattern_name",
    "The name of the pattern to set as selected"
  }
};

static ProcRecord patterns_set_popup_proc =
{
  "gimp_patterns_set_popup",
  "Sets the current pattern selection in a popup.",
  "Sets the current pattern selection in a popup.",
  "Andy Thomas",
  "Andy Thomas",
  "1998",
  PDB_INTERNAL,
  2,
  patterns_set_popup_inargs,
  0,
  NULL,
  { { patterns_set_popup_invoker } }
};
