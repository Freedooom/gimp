/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdialogfactory.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpcontext.h"

#include "config/gimpconfigwriter.h"

#include "gimpcontainerview.h"
#include "gimpcontainerview-utils.h"
#include "gimpcursor.h"
#include "gimpdialogfactory.h"
#include "gimpdock.h"
#include "gimpdockbook.h"
#include "gimpdockable.h"
#include "gimpimagedock.h"
#include "gimpmenufactory.h"


/* #define DEBUG_FACTORY */

#ifdef DEBUG_FACTORY
#define D(stmnt) stmnt
#else
#define D(stmnt)
#endif


typedef enum
{
  GIMP_DIALOG_VISIBILITY_UNKNOWN = 0,
  GIMP_DIALOG_VISIBILITY_INVISIBLE,
  GIMP_DIALOG_VISIBILITY_VISIBLE
} GimpDialogVisibilityState;

typedef enum
{
  GIMP_DIALOG_SHOW_ALL,
  GIMP_DIALOG_HIDE_ALL,
  GIMP_DIALOG_SHOW_TOOLBOX
} GimpDialogShowState;


static void   gimp_dialog_factory_class_init (GimpDialogFactoryClass *klass);
static void   gimp_dialog_factory_init       (GimpDialogFactory      *factory);

static void   gimp_dialog_factory_dispose             (GObject           *object);
static void   gimp_dialog_factory_finalize            (GObject           *object);

static gboolean gimp_dialog_factory_set_user_pos      (GtkWidget         *dialog,
                                                       GdkEventConfigure *cevent,
                                                       gpointer           data);
static gboolean gimp_dialog_factory_dialog_configure  (GtkWidget         *dialog,
                                                       GdkEventConfigure *cevent,
                                                       GimpDialogFactory *factory);
static void   gimp_dialog_factories_save_foreach      (gchar             *name,
						       GimpDialogFactory *factory,
						       GimpConfigWriter  *writer);
static void   gimp_dialog_factories_restore_foreach   (gchar             *name,
						       GimpDialogFactory *factory,
						       gpointer           data);
static void   gimp_dialog_factories_clear_foreach     (gchar             *name,
						       GimpDialogFactory *factory,
						       gpointer           data);
static void   gimp_dialog_factory_get_window_info     (GtkWidget         *window, 
						       GimpSessionInfo   *info);
static void   gimp_dialog_factory_set_window_geometry (GtkWidget         *window,
						       GimpSessionInfo   *info);
static void   gimp_dialog_factory_get_aux_info        (GtkWidget         *dialog,
						       GimpSessionInfo   *info);
static void   gimp_dialog_factory_set_aux_info        (GtkWidget         *dialog,
						       GimpSessionInfo   *info);
static void   gimp_dialog_factories_hide_foreach      (gchar             *name,
						       GimpDialogFactory *factory,
						       gpointer           data);
static void   gimp_dialog_factories_show_foreach      (gchar             *name,
						       GimpDialogFactory *factory,
						       gpointer           data);
static void   gimp_dialog_factories_idle_foreach      (gchar             *name,
						       GimpDialogFactory *factory,
						       gpointer           data);
static void   gimp_dialog_factories_unidle_foreach    (gchar             *name,
						       GimpDialogFactory *factory,
						       gpointer           data);


static GimpObjectClass *parent_class = NULL;


GType
gimp_dialog_factory_get_type (void)
{
  static GType factory_type = 0;

  if (! factory_type)
    {
      static const GTypeInfo factory_info =
      {
        sizeof (GimpDialogFactoryClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gimp_dialog_factory_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GimpDialogFactory),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gimp_dialog_factory_init,
      };

      factory_type = g_type_register_static (GIMP_TYPE_OBJECT,
					     "GimpDialogFactory",
					     &factory_info, 0);
    }

  return factory_type;
}

static void
gimp_dialog_factory_class_init (GimpDialogFactoryClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose  = gimp_dialog_factory_dispose;
  object_class->finalize = gimp_dialog_factory_finalize;

  klass->factories = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gimp_dialog_factory_init (GimpDialogFactory *factory)
{
  factory->menu_factory       = NULL;
  factory->new_dock_func      = NULL;
  factory->registered_dialogs = NULL;
  factory->session_infos      = NULL;
  factory->open_dialogs       = NULL;
}

static void
gimp_dialog_factory_dispose (GObject *object)
{
  GimpDialogFactory *factory;
  GList             *list;

  factory = GIMP_DIALOG_FACTORY (object);

  /*  start iterating from the beginning each time we destroyed a
   *  toplevel because destroying a dock may cause lots of items
   *  to be removed from factory->open_dialogs
   */
  while (factory->open_dialogs)
    {
      for (list = factory->open_dialogs; list; list = g_list_next (list))
        {
          if (GTK_WIDGET_TOPLEVEL (list->data))
            {
              gtk_widget_destroy (GTK_WIDGET (list->data));
              break;
            }
        }

      /*  the list being non-empty without any toplevel is an error,
       *  so eek and chain up
       */
      if (! list)
        {
          g_warning ("%s: stale non-toplevel entries in factory->open_dialogs",
                     G_GNUC_FUNCTION);
          break;
        }
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_dialog_factory_finalize (GObject *object)
{
  GimpDialogFactory *factory;
  GList             *list;

  factory = GIMP_DIALOG_FACTORY (object);

  g_hash_table_remove (GIMP_DIALOG_FACTORY_GET_CLASS (object)->factories,
		       GIMP_OBJECT (factory)->name);

  for (list = factory->registered_dialogs; list; list = g_list_next (list))
    {
      GimpDialogFactoryEntry *entry;

      entry = (GimpDialogFactoryEntry *) list->data;

      g_free (entry->identifier);
      g_free (entry);
    }

  if (factory->registered_dialogs)
    {
      g_list_free (factory->registered_dialogs);
      factory->registered_dialogs = NULL;
    }
  if (factory->open_dialogs)
    {
      g_list_free (factory->open_dialogs);
      factory->open_dialogs = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GimpDialogFactory *
gimp_dialog_factory_new (const gchar       *name,
			 GimpContext       *context,
			 GimpMenuFactory   *menu_factory,
			 GimpDialogNewFunc  new_dock_func)
{
  GimpDialogFactoryClass *factory_class;
  GimpDialogFactory      *factory;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (! menu_factory || GIMP_IS_MENU_FACTORY (menu_factory),
			NULL);

  /* EEK */
  factory_class = g_type_class_ref (GIMP_TYPE_DIALOG_FACTORY);

  if (g_hash_table_lookup (factory_class->factories, name))
    {
      g_warning ("%s: dialog factory \"%s\" already exists",
		 G_GNUC_FUNCTION, name);
      return NULL;
    }

  factory = g_object_new (GIMP_TYPE_DIALOG_FACTORY, NULL);

  gimp_object_set_name (GIMP_OBJECT (factory), name);

  g_hash_table_insert (factory_class->factories,
		       GIMP_OBJECT (factory)->name, factory);

  factory->context       = context;
  factory->menu_factory  = menu_factory;
  factory->new_dock_func = new_dock_func;

  return factory;
}

GimpDialogFactory *
gimp_dialog_factory_from_name (const gchar *name)
{
  GimpDialogFactoryClass *factory_class;
  GimpDialogFactory      *factory;

  g_return_val_if_fail (name != NULL, NULL);

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  factory = (GimpDialogFactory *)
    g_hash_table_lookup (factory_class->factories, name);

  return factory;
}

void
gimp_dialog_factory_register_entry (GimpDialogFactory *factory,
				    const gchar       *identifier,
				    GimpDialogNewFunc  new_func,
                                    gint               preview_size,
				    gboolean           singleton,
				    gboolean           session_managed,
				    gboolean           remember_size,
				    gboolean           remember_if_open)
{
  GimpDialogFactoryEntry *entry;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (identifier != NULL);

  entry = g_new0 (GimpDialogFactoryEntry, 1);

  entry->identifier       = g_strdup (identifier);
  entry->new_func         = new_func;
  entry->preview_size     = preview_size;
  entry->singleton        = singleton ? TRUE : FALSE;
  entry->session_managed  = session_managed ? TRUE : FALSE;
  entry->remember_size    = remember_size ? TRUE : FALSE;
  entry->remember_if_open = remember_if_open ? TRUE : FALSE;

  factory->registered_dialogs = g_list_prepend (factory->registered_dialogs,
						entry);
}

GimpDialogFactoryEntry *
gimp_dialog_factory_find_entry (GimpDialogFactory *factory,
				const gchar       *identifier)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  for (list = factory->registered_dialogs; list; list = g_list_next (list))
    {
      GimpDialogFactoryEntry *entry;

      entry = (GimpDialogFactoryEntry *) list->data;

      if (! strcmp (identifier, entry->identifier))
	{
	  return entry;
	}
    }

  return NULL;
}

GimpSessionInfo *
gimp_dialog_factory_find_session_info (GimpDialogFactory *factory,
				       const gchar       *identifier)
{
  GList *list;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  for (list = factory->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *info;

      info = (GimpSessionInfo *) list->data;

      if ((info->toplevel_entry &&
	   ! strcmp (identifier, info->toplevel_entry->identifier)) ||
	  (info->dockable_entry &&
	   ! strcmp (identifier, info->dockable_entry->identifier)))
	{
	  return info;
	}
    }

  return NULL;
}

static GtkWidget *
gimp_dialog_factory_dialog_new_internal (GimpDialogFactory *factory,
					 GimpContext       *context,
					 const gchar       *identifier,
                                         gint               preview_size,
					 gboolean           raise_if_found)
{
  GimpDialogFactoryEntry *entry;
  GtkWidget              *dialog = NULL;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  entry = gimp_dialog_factory_find_entry (factory, identifier);

  if (! entry)
    {
      g_warning ("%s: no entry registered for \"%s\"",
		 G_GNUC_FUNCTION, identifier);
      return NULL;
    }

  if (! entry->new_func)
    {
      g_warning ("%s: entry for \"%s\" has no constructor",
		 G_GNUC_FUNCTION, identifier);
      return NULL;
    }

  /*  a singleton dialog is always only raised if it already exisits  */
  if (raise_if_found || entry->singleton)
    {
      GimpSessionInfo *info;

      info = gimp_dialog_factory_find_session_info (factory, identifier);

      if (info)
	dialog = info->widget;
    }

  /*  create the dialog if it was not found  */
  if (! dialog)
    {
      GtkWidget *dock = NULL;

      /*  If the dialog will be a dockable (factory->new_dock_func) and
       *  we are called from gimp_dialog_factory_dialog_raise() (! context),
       *  create a new dock _before_ creating the dialog.
       *  We do this because the new dockable needs to be created in it's
       *  dock's context.
       */
      if (factory->new_dock_func && ! context)
	{
          GtkWidget *dockbook;

	  dock     = gimp_dialog_factory_dock_new (factory);
          dockbook = gimp_dockbook_new (factory->menu_factory);

	  gimp_dock_add_book (GIMP_DOCK (dock),
			      GIMP_DOCKBOOK (dockbook),
                              0);
	}

      /*  Create the new dialog in the appropriate context which is
       *  - the passed context if not NULL
       *  - the newly created dock's context if we just created it
       *  - the factory's context, which happens when raising a toplevel
       *    dialog was the original request.
       */
      if (preview_size < GIMP_PREVIEW_SIZE_TINY)
        preview_size = entry->preview_size;

      if (context)
	dialog = entry->new_func (factory,
                                  context,
                                  preview_size);
      else if (dock)
	dialog = entry->new_func (factory,
                                  GIMP_DOCK (dock)->context,
                                  preview_size);
      else
	dialog = entry->new_func (factory,
                                  factory->context,
                                  preview_size);

      if (dialog)
	{
	  g_object_set_data (G_OBJECT (dialog), "gimp-dialog-factory", 
                             factory);
	  g_object_set_data (G_OBJECT (dialog), "gimp-dialog-factory-entry",
                             entry);

	  /*  If we created a dock before, the newly created dialog is
	   *  supposed to be a GimpDockable.
	   */
	  if (dock)
	    {
	      if (GIMP_IS_DOCKABLE (dialog))
		{
		  gimp_dock_add (GIMP_DOCK (dock), GIMP_DOCKABLE (dialog),
				 0, 0);

		  gtk_widget_show (dock);
		}
	      else
		{
		  g_warning ("%s: GimpDialogFactory is a dockable factory "
			     "but constructor for \"%s\" did not return a "
			     "GimpDockable",
			     G_GNUC_FUNCTION, identifier);

		  gtk_widget_destroy (dialog);
                  gtk_widget_destroy (dock);

		  dialog = NULL;
                  dock   = NULL;
		}
	    }
	}
      else if (dock)
	{
	  g_warning ("%s: constructor for \"%s\" returned NULL",
		     G_GNUC_FUNCTION, identifier);

	  gtk_widget_destroy (dock);

	  dock = NULL;
	}

      if (dialog)
	gimp_dialog_factory_add_dialog (factory, dialog);
    }

  /*  finally, if we found an existing dialog or created a new one,
   *  raise it
   */
  if (dialog)
    {
      if (GTK_WIDGET_TOPLEVEL (dialog))
	{
          gtk_window_present (GTK_WINDOW (dialog));
	}
      else if (GIMP_IS_DOCKABLE (dialog))
	{
	  GimpDockable *dockable;
	  gint          page_num;

	  dockable = GIMP_DOCKABLE (dialog);

	  if (dockable->dockbook && dockable->dockbook->dock)
	    {
	      page_num =
		gtk_notebook_page_num (GTK_NOTEBOOK (dockable->dockbook),
				       dialog);

	      if (page_num != -1)
		{
		  GtkWidget *toplevel;

		  gtk_notebook_set_current_page (GTK_NOTEBOOK (dockable->dockbook),
                                                 page_num);

		  toplevel = gtk_widget_get_toplevel (dialog);

                  gtk_window_present (GTK_WINDOW (toplevel));
		}
	    }
	}
    }

  return dialog;
}

/**
 * gimp_dialog_factory_dialog_new:
 * @factory:    a #GimpDialogFactory
 * @identifier: the identifier of the dialog as registered with
 *              gimp_dialog_factory_register_entry()
 * 
 * Creates a new toplevel dialog or a #GimpDockable, depending on whether
 * %factory is a toplevel of dockable factory.
 *
 * Implicitly raises singleton dialogs.
 * 
 * Return value: the newly created dialog or an already existing singleton
 *               dialog.
 **/
GtkWidget *
gimp_dialog_factory_dialog_new (GimpDialogFactory *factory,
				const gchar       *identifier,
                                gint               preview_size)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  return gimp_dialog_factory_dialog_new_internal (factory,
						  factory->context,
						  identifier,
                                                  preview_size,
						  FALSE);
}

/**
 * gimp_dialog_factory_dialog_raise:
 * @factory   : a #GimpDialogFactory
 * @identifier: the identifier of the dialog as registered with
 *              gimp_dialog_factory_register_entry()
 * 
 * Raises an already existing toplevel dialog or #GimpDockable if it was
 * already created by this %facory.
 *
 * Implicitly creates a new dialog if it was not found.
 * 
 * Return value: the raised or newly created dialog.
 **/
GtkWidget *
gimp_dialog_factory_dialog_raise (GimpDialogFactory *factory,
				  const gchar       *identifier,
                                  gint               preview_size)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  return gimp_dialog_factory_dialog_new_internal (factory,
						  NULL,
						  identifier,
                                                  preview_size,
						  TRUE);
}

/**
 * gimp_dialog_factory_dockable_new:
 * @factory   : a #GimpDialogFactory
 * @dock      : a #GimpDock crated by this %factory.
 * @identifier: the identifier of the dialog as registered with
 *              gimp_dialog_factory_register_entry()
 * 
 * Creates a new #GimpDockable in the context of the #GimpDock it will be
 * added to.
 *
 * Implicitly raises & returns an already existing singleton dockable,
 * so callers should check that dockable->dockbook is NULL before trying
 * to add it to it's #GimpDockbook.
 * 
 * Return value: the newly created #GimpDockable or an already existing
 *               singleton dockable.
 **/
GtkWidget *
gimp_dialog_factory_dockable_new (GimpDialogFactory *factory,
				  GimpDock          *dock,
				  const gchar       *identifier,
                                  gint               preview_size)
{
  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);
  g_return_val_if_fail (identifier != NULL, NULL);

  return gimp_dialog_factory_dialog_new_internal (factory,
						  dock->context,
						  identifier,
                                                  preview_size,
						  FALSE);
}

/**
 * gimp_dialog_factory_dock_new:
 * @factory: a #GimpDialogFacotry
 * 
 * Returns a new #GimpDock in this %factory's context. We use a function
 * pointer passed to this %factory's constructor instead of simply
 * gimp_dock_new() because we may want different instances of
 * #GimpDialogFactory create different subclasses of #GimpDock.
 * 
 * Return value: the newly created #GimpDock.
 **/
GtkWidget *
gimp_dialog_factory_dock_new (GimpDialogFactory *factory)
{
  GtkWidget *dock;

  g_return_val_if_fail (GIMP_IS_DIALOG_FACTORY (factory), NULL);
  g_return_val_if_fail (factory->new_dock_func != NULL, NULL);

  dock = factory->new_dock_func (factory, factory->context, 0);

  if (dock)
    {
      g_object_set_data (G_OBJECT (dock), "gimp-dialog-factory", factory);

      gimp_dialog_factory_add_dialog (factory, dock);
    }

  return dock;
}

void
gimp_dialog_factory_add_dialog (GimpDialogFactory *factory,
				GtkWidget         *dialog)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;
  GimpSessionInfo        *info;
  GList                  *list;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (GTK_IS_WIDGET (dialog));

  if (g_list_find (factory->open_dialogs, dialog))
    {
      g_warning ("%s: dialog already registered", G_GNUC_FUNCTION);
      return;
    }

  dialog_factory = g_object_get_data (G_OBJECT (dialog), 
                                      "gimp-dialog-factory");
  entry = g_object_get_data (G_OBJECT (dialog), "gimp-dialog-factory-entry");

  if (! (dialog_factory && (entry || GIMP_IS_DOCK (dialog))))
    {
      g_warning ("%s: dialog was not created by a GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return;
    }

  if (dialog_factory != factory)
    {
      g_warning ("%s: dialog was created by a different GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return;
    }

  if (entry) /* dialog is a toplevel (but not a GimpDock) or a GimpDockable */
    {
      gboolean toplevel;

      toplevel = GTK_WIDGET_TOPLEVEL (dialog);

      D (g_print ("%s: adding %s \"%s\"\n",
                  G_GNUC_FUNCTION,
                  toplevel ? "toplevel" : "dockable",
                  entry->identifier));

      for (list = factory->session_infos; list; list = g_list_next (list))
	{
	  info = (GimpSessionInfo *) list->data;

	  if ((info->toplevel_entry == entry) ||
	      (info->dockable_entry == entry))
	    {
	      if (info->widget)
		{
		  if (entry->singleton)
		    {
		      g_warning ("%s: singleton dialog \"%s\" created twice",
				 G_GNUC_FUNCTION, entry->identifier);

                      D (g_print ("%s: corrupt session info: %p (widget %p)\n",
                                  G_GNUC_FUNCTION,
                                  info, info->widget));

		      return;
		    }

		  continue;
		}

              info->widget = dialog;

              D (g_print ("%s: updating session info %p (widget %p) for %s \"%s\"\n",
                          G_GNUC_FUNCTION,
                          info, info->widget,
                          toplevel ? "toplevel" : "dockable",
                          entry->identifier));

	      if (entry->session_managed)
                gimp_dialog_factory_set_window_geometry (info->widget, info);

	      break;
	    }
	}

      if (! list) /*  didn't find a session info  */
	{
	  info = g_new0 (GimpSessionInfo, 1);

	  info->widget = dialog;

          D (g_print  ("%s: creating session info %p (widget %p) for %s \"%s\"\n",
                       G_GNUC_FUNCTION,
                       info, info->widget,
                       toplevel ? "toplevel" : "dockable",
                       entry->identifier));

	  if (toplevel)
            {
              info->toplevel_entry = entry;

              /*  if we create a new session info, we never call
               *  gimp_dialog_factory_set_window_geometry(), but still
               *  the dialog needs GDK_HINT_USER_POS so it keeps its
               *  position when hidden/shown within this(!) session.
               */
              if (entry->session_managed)
                g_signal_connect (dialog, "configure_event",
                                  G_CALLBACK (gimp_dialog_factory_set_user_pos),
                                  NULL);
            }
	  else
            {
              info->dockable_entry = entry;
            }

	  factory->session_infos = g_list_append (factory->session_infos, info);
	}
    }
  else /*  dialog is a GimpDock  */
    {
      D (g_print ("%s: adding dock\n", G_GNUC_FUNCTION));

      for (list = factory->session_infos; list; list = g_list_next (list))
	{
	  info = (GimpSessionInfo *) list->data;

          /*  take the first empty slot  */
          if (! info->toplevel_entry &&
              ! info->dockable_entry &&
              ! info->widget)
	    {
	      info->widget = dialog;

              D (g_print ("%s: updating session info %p (widget %p) for dock\n",
                          G_GNUC_FUNCTION,
                          info, info->widget));

	      gimp_dialog_factory_set_window_geometry (info->widget, info);

	      break;
	    }
	}

      if (! list) /*  didn't find a session info  */
	{
	  info = g_new0 (GimpSessionInfo, 1);

	  info->widget = dialog;

          D (g_print ("%s: creating session info %p (widget %p) for dock\n",
                      G_GNUC_FUNCTION,
                      info, info->widget));

	  factory->session_infos = g_list_append (factory->session_infos, info);
	}
    }

  factory->open_dialogs = g_list_prepend (factory->open_dialogs, dialog);

  g_signal_connect_object (dialog, "destroy",
                           G_CALLBACK (gimp_dialog_factory_remove_dialog),
                           factory,
                           G_CONNECT_SWAPPED);

  if (entry                  &&
      entry->session_managed &&
      GTK_WIDGET_TOPLEVEL (dialog))
    {
      g_signal_connect_object (dialog, "configure_event",
                               G_CALLBACK (gimp_dialog_factory_dialog_configure),
                               factory,
                               0);
    }
}

void
gimp_dialog_factory_add_foreign (GimpDialogFactory *factory,
                                 const gchar       *identifier,
                                 GtkWidget         *dialog)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (identifier != NULL);
  g_return_if_fail (GTK_IS_WIDGET (dialog));
  g_return_if_fail (GTK_WIDGET_TOPLEVEL (dialog));

  dialog_factory = g_object_get_data (G_OBJECT (dialog), 
                                      "gimp-dialog-factory");
  entry = g_object_get_data (G_OBJECT (dialog), "gimp-dialog-factory-entry");

  if (dialog_factory || entry)
    {
      g_warning ("%s: dialog was created by a GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return;
    }

  entry = gimp_dialog_factory_find_entry (factory, identifier);

  if (! entry)
    {
      g_warning ("%s: no entry registered for \"%s\"",
		 G_GNUC_FUNCTION, identifier);
      return;
    }

  if (entry->new_func)
    {
      g_warning ("%s: entry for \"%s\" has a constructor (is not foreign)",
		 G_GNUC_FUNCTION, identifier);
      return;
    }

  g_object_set_data (G_OBJECT (dialog), "gimp-dialog-factory", 
                     factory);
  g_object_set_data (G_OBJECT (dialog), "gimp-dialog-factory-entry",
                     entry);

  gimp_dialog_factory_add_dialog (factory, dialog);
}

void
gimp_dialog_factory_remove_dialog (GimpDialogFactory *factory,
				   GtkWidget         *dialog)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;
  GimpSessionInfo        *session_info;
  GList                  *list;

  g_return_if_fail (GIMP_IS_DIALOG_FACTORY (factory));
  g_return_if_fail (GTK_IS_WIDGET (dialog));

  if (! g_list_find (factory->open_dialogs, dialog))
    {
      g_warning ("%s: dialog not registered", G_GNUC_FUNCTION);
      return;
    }

  factory->open_dialogs = g_list_remove (factory->open_dialogs, dialog);

  dialog_factory = g_object_get_data (G_OBJECT (dialog),
                                      "gimp-dialog-factory");
  entry = g_object_get_data (G_OBJECT (dialog), "gimp-dialog-factory-entry");

  if (! (dialog_factory && (entry || GIMP_IS_DOCK (dialog))))
    {
      g_warning ("%s: dialog was not created by a GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return;
    }

  if (dialog_factory != factory)
    {
      g_warning ("%s: dialog was created by a different GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return;
    }

  D (g_print ("%s: removing \"%s\"\n",
              G_GNUC_FUNCTION,
              entry ? entry->identifier : "dock"));

  for (list = factory->session_infos; list; list = g_list_next (list))
    {
      session_info = (GimpSessionInfo *) list->data;

      if (session_info->widget == dialog)
	{
          D (g_print ("%s: clearing session info %p (widget %p) for \"%s\"\n",
                      G_GNUC_FUNCTION,
                      session_info, session_info->widget,
                      entry ? entry->identifier : "dock"));

	  session_info->widget = NULL;

	  /*  don't save session info for empty docks  */
	  if (GIMP_IS_DOCK (dialog))
	    {
	      factory->session_infos = g_list_remove (factory->session_infos,
						      session_info);

	      g_free (session_info);
	    }

	  break;
	}
    }
}

void
gimp_dialog_factories_session_save (GimpConfigWriter *writer)
{
  GimpDialogFactoryClass *factory_class;

  g_return_if_fail (writer != NULL);

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
			(GHFunc) gimp_dialog_factories_save_foreach,
			writer);
}

void
gimp_dialog_factories_session_restore (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
			(GHFunc) gimp_dialog_factories_restore_foreach,
			NULL);
}

void
gimp_dialog_factories_session_clear (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
			(GHFunc) gimp_dialog_factories_clear_foreach,
			NULL);
}

void
gimp_dialog_factories_toggle (GimpDialogFactory *toolbox_factory)
{
  static GimpDialogShowState toggle_state = GIMP_DIALOG_SHOW_ALL;
  static gboolean            doing_update = FALSE;

  GimpDialogFactoryClass *factory_class;

  if (doing_update)
    return;

  doing_update = TRUE;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  switch (toggle_state)
    {
    case GIMP_DIALOG_SHOW_ALL:
      toggle_state = GIMP_DIALOG_HIDE_ALL;

      g_hash_table_foreach (factory_class->factories,
			    (GHFunc) gimp_dialog_factories_hide_foreach,
			    NULL);
      break;

    case GIMP_DIALOG_HIDE_ALL:
      toggle_state = GIMP_DIALOG_SHOW_TOOLBOX;

      gimp_dialog_factories_show_foreach (GIMP_OBJECT (toolbox_factory)->name,
                                          toolbox_factory,
                                          NULL);
      break;

    case GIMP_DIALOG_SHOW_TOOLBOX:
      toggle_state = GIMP_DIALOG_SHOW_ALL;

      g_hash_table_foreach (factory_class->factories,
			    (GHFunc) gimp_dialog_factories_show_foreach,
			    NULL);
    default:
      break;
    }

  doing_update = FALSE;
}

void
gimp_dialog_factories_idle (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
			(GHFunc) gimp_dialog_factories_idle_foreach,
			NULL);
}

void
gimp_dialog_factories_unidle (void)
{
  GimpDialogFactoryClass *factory_class;

  factory_class = g_type_class_peek (GIMP_TYPE_DIALOG_FACTORY);

  g_hash_table_foreach (factory_class->factories,
			(GHFunc) gimp_dialog_factories_unidle_foreach,
			NULL);
}


/*  private functions  */

static gboolean
gimp_dialog_factory_set_user_pos (GtkWidget         *dialog,
                                  GdkEventConfigure *cevent,
                                  gpointer           data)
{
  g_signal_handlers_disconnect_by_func (dialog,
                                        gimp_dialog_factory_set_user_pos,
                                        data);

  gtk_window_set_geometry_hints (GTK_WINDOW (dialog), NULL, NULL,
                                 GDK_HINT_USER_POS);

  return FALSE;
}

static gboolean
gimp_dialog_factory_dialog_configure (GtkWidget         *dialog,
                                      GdkEventConfigure *cevent,
                                      GimpDialogFactory *factory)
{
  GimpDialogFactory      *dialog_factory;
  GimpDialogFactoryEntry *entry;
  GimpSessionInfo        *session_info;
  GList                  *list;

  if (! g_list_find (factory->open_dialogs, dialog))
    {
      g_warning ("%s: dialog not registered", G_GNUC_FUNCTION);
      return FALSE;
    }

  dialog_factory = g_object_get_data (G_OBJECT (dialog),
                                      "gimp-dialog-factory");
  entry = g_object_get_data (G_OBJECT (dialog), "gimp-dialog-factory-entry");

  if (! dialog_factory || ! entry)
    {
      g_warning ("%s: dialog was not created by a GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return FALSE;
    }

  if (dialog_factory != factory)
    {
      g_warning ("%s: dialog was created by a different GimpDialogFactory",
		 G_GNUC_FUNCTION);
      return FALSE;
    }

  for (list = factory->session_infos; list; list = g_list_next (list))
    {
      session_info = (GimpSessionInfo *) list->data;

      if (session_info->widget == dialog)
        {
          D (g_print ("%s: updating session info for \"%s\"\n",
                      G_GNUC_FUNCTION, entry->identifier));

          gimp_dialog_factory_get_window_info (dialog, session_info);

          break;
	}
    }

  return FALSE;
}

static void
gimp_dialog_factories_save_foreach (gchar             *name,
				    GimpDialogFactory *factory,
				    GimpConfigWriter  *writer)
{
  GList *list;

  for (list = factory->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *info;
      const gchar     *dialog_name;

      info = (GimpSessionInfo *) list->data;

      /*  we keep session info entries for all toplevel dialogs created
       *  by the factory but don't save them if they don't want to be
       *  managed
       */
      if (info->dockable_entry ||
	  (info->toplevel_entry && ! info->toplevel_entry->session_managed))
	continue;

      if (info->widget)
	gimp_dialog_factory_get_window_info (info->widget, info);

      if (info->toplevel_entry)
        dialog_name = info->toplevel_entry->identifier;
      else
        dialog_name = "dock";

      gimp_config_writer_open (writer, "session-info");
      gimp_config_writer_string (writer, name);
      gimp_config_writer_string (writer, dialog_name);

      gimp_config_writer_open (writer, "position");
      gimp_config_writer_printf (writer, "%d %d", info->x, info->y);
      gimp_config_writer_close (writer);

      if (info->width > 0 && info->height > 0)
        {
          gimp_config_writer_open (writer, "size");
          gimp_config_writer_printf (writer, "%d %d",
                                     info->width, info->height);
          gimp_config_writer_close (writer);
        }

      if (info->open)
        {
          gimp_config_writer_open (writer, "open-on-exit");
          gimp_config_writer_close (writer);
        }

      /*  save aux-info  */
      if (info->widget)
	{
	  gimp_dialog_factory_get_aux_info (info->widget, info);

	  if (info->aux_info)
	    {
	      GList *aux;

              gimp_config_writer_open (writer, "aux-info");

	      for (aux = info->aux_info; aux; aux = g_list_next (aux))
                gimp_config_writer_string (writer, (gchar *) aux->data);

              gimp_config_writer_close (writer);

	      g_list_foreach (info->aux_info, (GFunc) g_free, NULL);
	      g_list_free (info->aux_info);
	      info->aux_info = NULL;
	    }
	}

      if (! info->toplevel_entry && info->widget)
	{
	  GimpDock *dock;
	  GList    *books;

	  dock = GIMP_DOCK (info->widget);

	  gimp_config_writer_open (writer, "dock");

	  for (books = dock->dockbooks; books; books = g_list_next (books))
	    {
	      GimpDockbook *dockbook;
	      GList        *children;
	      GList        *pages;

	      dockbook = (GimpDockbook *) books->data;

	      gimp_config_writer_open (writer, "book");

	      children = gtk_container_get_children (GTK_CONTAINER (dockbook));

	      for (pages = children; pages; pages = g_list_next (pages))
		{
		  GimpDockable           *dockable;
		  GimpDialogFactoryEntry *entry;

		  dockable = (GimpDockable *) pages->data;

		  entry = g_object_get_data (G_OBJECT (dockable),
                                             "gimp-dialog-factory-entry");

		  if (entry)
                    {
                      GimpContainerView *view;
                      gint               preview_size = -1;

                      gimp_config_writer_linefeed (writer);

                      view = gimp_container_view_get_by_dockable (dockable);

                      if (view && view->preview_size >= GIMP_PREVIEW_SIZE_TINY)
                        {
                          preview_size = view->preview_size;
                        }

                      if (preview_size > 0 &&
                          preview_size != entry->preview_size)
                        {
                          gimp_config_writer_printf (writer, "\"%s@%d\"",
                                                     entry->identifier,
                                                     preview_size);
                        }
                      else
                        {
                          gimp_config_writer_printf (writer, "\"%s\"",
                                                     entry->identifier);
                        }
                    }
		}

	      g_list_free (children);

	      gimp_config_writer_close (writer);  /* book */
	    }

	  gimp_config_writer_close (writer);  /* dock */
	}

      gimp_config_writer_close (writer);  /* session-info */
    }
}

static void
gimp_dialog_factories_restore_foreach (gchar             *name,
				       GimpDialogFactory *factory,
				       gpointer           data)
{
  GList *list;

  for (list = factory->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *info;

      info = (GimpSessionInfo *) list->data;

      if (! info->open)
	continue;

      info->open = FALSE;

      if (info->toplevel_entry)
	{
	  GtkWidget *dialog;

	  dialog =
	    gimp_dialog_factory_dialog_new (factory,
					    info->toplevel_entry->identifier,
                                            info->toplevel_entry->preview_size);

	  if (dialog && info->aux_info)
	    gimp_dialog_factory_set_aux_info (dialog, info);
	}
      else
	{
	  GimpDock *dock;
	  GList    *books;

	  dock = GIMP_DOCK (gimp_dialog_factory_dock_new (factory));

	  if (dock && info->aux_info)
	    gimp_dialog_factory_set_aux_info (GTK_WIDGET (dock), info);

	  for (books = info->sub_dialogs; books; books = g_list_next (books))
	    {
	      GtkWidget *dockbook;
	      GList     *pages;

	      dockbook = gimp_dockbook_new (factory->menu_factory);

	      gimp_dock_add_book (dock, GIMP_DOCKBOOK (dockbook), -1);

	      for (pages = books->data; pages; pages = g_list_next (pages))
		{
		  GtkWidget *dockable;
		  gchar     *identifier;
                  gchar     *substring;
                  gint       preview_size = -1;

		  identifier = (gchar *) pages->data;

                  if ((substring = strstr (identifier, "@")))
                    {
                      *substring = '\0';

                      preview_size = atoi (substring + 1);

                      if (preview_size < GIMP_PREVIEW_SIZE_TINY ||
			  preview_size > GIMP_PREVIEW_SIZE_GIGANTIC)
                        preview_size = -1;
                    }

                  /*  use the new dock's dialog factory to create dockables
                   *  because it may be different from the dialog factory
                   *  the dock was created from.
                   */
		  dockable =
                    gimp_dialog_factory_dockable_new (dock->dialog_factory,
                                                      dock,
                                                      identifier,
                                                      preview_size);

		  if (dockable)
		    gimp_dockbook_add (GIMP_DOCKBOOK (dockbook),
                                       GIMP_DOCKABLE (dockable), -1);

		  g_free (identifier);
		}

	      g_list_free (books->data);
	    }

	  g_list_free (info->sub_dialogs);
	  info->sub_dialogs = NULL;

	  gtk_widget_show (GTK_WIDGET (dock));
	}

      g_list_foreach (info->aux_info, (GFunc) g_free, NULL);
      g_list_free (info->aux_info);
      info->aux_info = NULL;
    }
}

static void
gimp_dialog_factories_clear_foreach (gchar             *name,
                                     GimpDialogFactory *factory,
                                     gpointer           data)
{
  GList *list;

  for (list = factory->session_infos; list; list = g_list_next (list))
    {
      GimpSessionInfo *info;

      info = (GimpSessionInfo *) list->data;

      if (info->widget)
        continue;

#ifdef __GNUC__
#warning FIXME: implement session info deletion
#endif
    }
}

static void
gimp_dialog_factory_get_window_info (GtkWidget       *window,
				     GimpSessionInfo *info)
{
  g_return_if_fail (GTK_IS_WIDGET (window));
  g_return_if_fail (GTK_WIDGET_TOPLEVEL (window));
  g_return_if_fail (info != NULL);

  if (window->window)
    {
      gdk_window_get_root_origin (window->window, &info->x, &info->y);

      if (! info->toplevel_entry || info->toplevel_entry->remember_size)
	{
	  info->width  = window->allocation.width;
	  info->height = window->allocation.height;
	}
      else
	{
	  info->width  = 0;
	  info->height = 0;
	}
    }

  if (! info->toplevel_entry || info->toplevel_entry->remember_if_open)
    info->open = GTK_WIDGET_VISIBLE (window);
  else
    info->open = FALSE;
}

static void
gimp_dialog_factory_set_window_geometry (GtkWidget       *window,
					 GimpSessionInfo *info)
{
  static gint screen_width  = 0;
  static gint screen_height = 0;

  gchar geom[32];

  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_WIDGET_TOPLEVEL (window));
  g_return_if_fail (info != NULL);

  if (screen_width == 0 || screen_height == 0)
    {
      screen_width  = gdk_screen_width ();
      screen_height = gdk_screen_height ();
    }

  info->x = CLAMP (info->x, 0, screen_width  - 128);
  info->y = CLAMP (info->y, 0, screen_height - 128);

  g_snprintf (geom, sizeof (geom), "+%d+%d", info->x, info->y);

  gtk_window_parse_geometry (GTK_WINDOW (window), geom);

  if (! info->toplevel_entry || info->toplevel_entry->remember_size)
    {
      if (info->width > 0 && info->height > 0)
        gtk_window_set_default_size (GTK_WINDOW (window),
                                     info->width, info->height);
    }
}

static void
gimp_dialog_factory_get_aux_info (GtkWidget       *dialog,
				  GimpSessionInfo *info)
{
  /* FIXME: make the aux-info stuff generic */

  if (GIMP_IS_IMAGE_DOCK (dialog))
    {
      GimpImageDock *dock;

      dock = GIMP_IMAGE_DOCK (dialog);

      info->aux_info = g_list_append (info->aux_info,
				      dock->show_image_menu ?
				      g_strdup ("menu-shown") :
				      g_strdup ("menu-hidden"));

      info->aux_info = g_list_append (info->aux_info,
				      dock->auto_follow_active ?
				      g_strdup ("follow-active-image") :
				      g_strdup ("dont-follow-active-image"));
    }
}

static void
gimp_dialog_factory_set_aux_info (GtkWidget       *dialog,
				  GimpSessionInfo *info)
{
  GList *aux;

  /* FIXME: make the aux-info stuff generic */

  if (GIMP_IS_IMAGE_DOCK (dialog))
    {
      gboolean menu_shown  = TRUE;
      gboolean auto_follow = TRUE;

      for (aux = info->aux_info; aux; aux = g_list_next (aux))
	{
	  gchar *str;

	  str = (gchar *) aux->data;

	  if (! strcmp (str, "menu-shown"))
	    menu_shown = TRUE;
	  else if (! strcmp (str, "menu-hidden"))
	    menu_shown = FALSE;

	  else if (! strcmp (str, "follow-active-image"))
	    auto_follow = TRUE;
	  else if (! strcmp (str, "dont-follow-active-image"))
	    auto_follow = FALSE;
	}

      gimp_image_dock_set_show_image_menu (GIMP_IMAGE_DOCK (dialog),
					   menu_shown);
      gimp_image_dock_set_auto_follow_active (GIMP_IMAGE_DOCK (dialog),
                                              auto_follow);
    }
}

static void
gimp_dialog_factories_hide_foreach (gchar             *name,
				    GimpDialogFactory *factory,
				    gpointer           data)
{
  GList *list;

  for (list = factory->open_dialogs; list; list = g_list_next (list))
    {
      if (GTK_IS_WIDGET (list->data) && GTK_WIDGET_TOPLEVEL (list->data))
	{
	  GimpDialogVisibilityState visibility = GIMP_DIALOG_VISIBILITY_UNKNOWN;

	  if (GTK_WIDGET_VISIBLE (list->data))
	    {
	      visibility = GIMP_DIALOG_VISIBILITY_VISIBLE;

	      gtk_widget_hide (GTK_WIDGET (list->data));
	    }
	  else
	    {
	      visibility = GIMP_DIALOG_VISIBILITY_INVISIBLE;
	    }

	  g_object_set_data (G_OBJECT (list->data),
                             "gimp-dialog-visibility",
                             GINT_TO_POINTER (visibility));
	}
    }
}

static void
gimp_dialog_factories_show_foreach (gchar             *name,
				    GimpDialogFactory *factory,
				    gpointer           data)
{
  GList *list;

  for (list = factory->open_dialogs; list; list = g_list_next (list))
    {
      if (GTK_IS_WIDGET (list->data) && GTK_WIDGET_TOPLEVEL (list->data))
	{
	  GimpDialogVisibilityState visibility;

	  visibility =
	    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data),
                                                "gimp-dialog-visibility"));

	  if (! GTK_WIDGET_VISIBLE (list->data) &&
	      visibility == GIMP_DIALOG_VISIBILITY_VISIBLE)
	    {
	      gtk_widget_show (GTK_WIDGET (list->data));
	    }
	}
    }
}

static void
gimp_dialog_factories_idle_foreach (gchar             *name,
				    GimpDialogFactory *factory,
				    gpointer           data)
{
  GdkCursor *cursor;
  GList     *list;

  cursor = gimp_cursor_new (GDK_WATCH,
			    GIMP_TOOL_CURSOR_NONE,
			    GIMP_CURSOR_MODIFIER_NONE);

  for (list = factory->open_dialogs; list; list = g_list_next (list))
    {
      if (GTK_IS_WIDGET (list->data)       &&
	  GTK_WIDGET_TOPLEVEL (list->data) &&
	  GTK_WIDGET_VISIBLE (list->data))
	{
	  gdk_window_set_cursor (GTK_WIDGET (list->data)->window, cursor);
	}
    }

  gdk_cursor_unref (cursor);
}

static void
gimp_dialog_factories_unidle_foreach (gchar             *name,
				      GimpDialogFactory *factory,
				      gpointer           data)
{
  GList *list;

  for (list = factory->open_dialogs; list; list = g_list_next (list))
    {
      if (GTK_IS_WIDGET (list->data)       &&
	  GTK_WIDGET_TOPLEVEL (list->data) &&
	  GTK_WIDGET_VISIBLE (list->data))
	{
	  gdk_window_set_cursor (GTK_WIDGET (list->data)->window, NULL);
	}
    }
}
