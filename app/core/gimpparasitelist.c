/* parasitelist.c: Copyright 1998 Jay Cox <jaycox@earthlink.net>
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

#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-object.h>

#include "libgimpbase/gimpbase.h"

#include "core-types.h"

#include "config/gimpconfig.h"
#include "config/gimpscanner.h"

#include "gimpmarshal.h"
#include "gimpparasitelist.h"

#include "libgimp/gimpintl.h"


enum
{
  ADD,
  REMOVE,
  LAST_SIGNAL
};


static void     gimp_parasite_list_class_init  (GimpParasiteListClass *klass);
static void     gimp_parasite_list_init        (GimpParasiteList      *list);
static void     gimp_parasite_list_finalize          (GObject     *object);
static gsize    gimp_parasite_list_get_memsize       (GimpObject  *object);

static void     gimp_parasite_list_config_iface_init (gpointer     iface,
                                                      gpointer     iface_data);
static gboolean gimp_parasite_list_serialize         (GObject     *object,
                                                      gint         fd);
static gboolean gimp_parasite_list_deserialize       (GObject     *object,
                                                      GScanner    *scanner);

static void     parasite_serialize           (const gchar      *key,
                                              GimpParasite     *parasite,
                                              gint             *fd_ptr);
static void     parasite_copy                (const gchar      *key,
                                              GimpParasite     *parasite,
                                              GimpParasiteList *list);
static gboolean parasite_free                (const gchar      *key,
                                              GimpParasite     *parasite,
                                              gpointer          unused);
static void     parasite_count_if_persistent (const gchar      *key, 
                                              GimpParasite     *parasite, 
                                              gint             *count);


static guint parasite_list_signals[LAST_SIGNAL] = { 0 };

static GimpObjectClass *parent_class = NULL;


GType
gimp_parasite_list_get_type (void)
{
  static GType list_type = 0;

  if (! list_type)
    {
      static const GTypeInfo list_info =
      {
        sizeof (GimpParasiteListClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) gimp_parasite_list_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data     */
	sizeof (GimpParasiteList),
	0,              /* n_preallocs    */
	(GInstanceInitFunc) gimp_parasite_list_init,
      };
      static const GInterfaceInfo list_iface_info = 
      { 
        gimp_parasite_list_config_iface_init,
        NULL,           /* iface_finalize */ 
        NULL            /* iface_data     */
      };

      list_type = g_type_register_static (GIMP_TYPE_OBJECT,
					  "GimpParasiteList", 
					  &list_info, 0);
      g_type_add_interface_static (list_type,
                                   GIMP_TYPE_CONFIG_INTERFACE,
                                   &list_iface_info);
    }

  return list_type;
}

static void
gimp_parasite_list_class_init (GimpParasiteListClass *klass)
{
  GObjectClass    *object_class;
  GimpObjectClass *gimp_object_class;

  object_class      = G_OBJECT_CLASS (klass);
  gimp_object_class = GIMP_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  parasite_list_signals[ADD] =
    g_signal_new ("add",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpParasiteListClass, add),
		  NULL, NULL,
		  gimp_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1,
		  G_TYPE_POINTER);

  parasite_list_signals[REMOVE] = 
    g_signal_new ("remove",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GimpParasiteListClass, remove),
		  NULL, NULL,
		  gimp_marshal_VOID__POINTER,
		  G_TYPE_NONE, 1,
		  G_TYPE_POINTER);

  object_class->finalize         = gimp_parasite_list_finalize;

  gimp_object_class->get_memsize = gimp_parasite_list_get_memsize;

  klass->add                     = NULL;
  klass->remove                  = NULL;
}

static void
gimp_parasite_list_config_iface_init (gpointer  iface,
                                      gpointer  iface_data)
{
  GimpConfigInterface *config_iface = (GimpConfigInterface *) iface;

  config_iface->serialize   = gimp_parasite_list_serialize;
  config_iface->deserialize = gimp_parasite_list_deserialize;
}

static void
gimp_parasite_list_init (GimpParasiteList *list)
{
  list->table = NULL;
}

static void
gimp_parasite_list_finalize (GObject *object)
{
  GimpParasiteList *list;

  g_return_if_fail (GIMP_IS_PARASITE_LIST (object));

  list = GIMP_PARASITE_LIST (object);

  if (list->table)
    {
      g_hash_table_foreach_remove (list->table, (GHRFunc) parasite_free, NULL);
      g_hash_table_destroy (list->table);
      list->table = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_parasite_list_get_memsize_foreach (gpointer key,
                                        gpointer p,
                                        gpointer m)
{
  GimpParasite *parasite;
  gsize        *memsize;

  parasite = (GimpParasite *) p;
  memsize  = (gsize *) m;

  *memsize += (sizeof (GimpParasite) +
               strlen (parasite->name) + 1 +
               parasite->size);
}

static gsize
gimp_parasite_list_get_memsize (GimpObject *object)
{
  GimpParasiteList *list;
  gsize             memsize = 0;

  list = GIMP_PARASITE_LIST (object);

  if (list->table)
    {
      memsize += (g_hash_table_size (list->table) *
                  3 * sizeof (gpointer)); /* FIXME */

      g_hash_table_foreach (list->table,
                            gimp_parasite_list_get_memsize_foreach,
                            &memsize);
    }

  return memsize + GIMP_OBJECT_CLASS (parent_class)->get_memsize (object);
}

static gboolean
gimp_parasite_list_serialize (GObject *list,
                              gint     fd)
{
  if (GIMP_PARASITE_LIST (list)->table)
    g_hash_table_foreach (GIMP_PARASITE_LIST (list)->table,
                          (GHFunc) parasite_serialize, &fd);

  return (fd != -1);
}

static gboolean
gimp_parasite_list_deserialize (GObject  *list,
                                GScanner *scanner)
{
  GTokenType token;

  g_scanner_scope_add_symbol (scanner, 0, "parasite", GINT_TO_POINTER (1));

  token = G_TOKEN_LEFT_PAREN;

  do
    {
      if (g_scanner_peek_next_token (scanner) != token)
        break;

      token = g_scanner_get_next_token (scanner);

      switch (token)
        {
        case G_TOKEN_LEFT_PAREN:
          token = G_TOKEN_SYMBOL;
          break;

        case G_TOKEN_SYMBOL:
          if (scanner->value.v_symbol == GINT_TO_POINTER (1))
            {
              gchar        *parasite_name  = NULL;
              gint          parasite_flags = 0;
              gchar        *parasite_data  = NULL;
              GimpParasite *parasite;

              token = G_TOKEN_STRING;

              if (g_scanner_peek_next_token (scanner) != token)
                break;

              if (! gimp_scanner_parse_string (scanner, &parasite_name))
                break;

             token = G_TOKEN_INT;

              if (g_scanner_peek_next_token (scanner) != token)
                break;

              if (! gimp_scanner_parse_int (scanner, &parasite_flags))
                break;

              token = G_TOKEN_STRING;

              if (g_scanner_peek_next_token (scanner) != token)
                break;

              if (! gimp_scanner_parse_string (scanner, &parasite_data))
                break;

              parasite = gimp_parasite_new (parasite_name,
                                            parasite_flags,
                                            strlen (parasite_data),
                                            parasite_data);
              gimp_parasite_list_add (GIMP_PARASITE_LIST (list),
                                      parasite);  /* adds a copy */
              gimp_parasite_free (parasite);

              token = G_TOKEN_RIGHT_PAREN;
            }
          break;

        case G_TOKEN_RIGHT_PAREN:
          token = G_TOKEN_LEFT_PAREN;
          break;

        default: /* do nothing */
          break;
        }
    }
  while (token != G_TOKEN_EOF);

  if (token != G_TOKEN_LEFT_PAREN)
    {
      g_scanner_get_next_token (scanner);
      g_scanner_unexp_token (scanner, token, NULL, "`parasite'", NULL, 
                             _("fatal parse error"), TRUE);
      return FALSE;
    }

  return TRUE;
}

GimpParasiteList *
gimp_parasite_list_new (void)
{
  GimpParasiteList *list;

  list = g_object_new (GIMP_TYPE_PARASITE_LIST, NULL);

  return list;
}

GimpParasiteList *
gimp_parasite_list_copy (const GimpParasiteList *list)
{
  GimpParasiteList *newlist;

  g_return_val_if_fail (GIMP_IS_PARASITE_LIST (list), NULL);

  newlist = gimp_parasite_list_new ();

  if (list->table)
    g_hash_table_foreach (list->table, (GHFunc) parasite_copy, newlist);

  return newlist;
}

void
gimp_parasite_list_add (GimpParasiteList *list, 
			GimpParasite     *parasite)
{
  g_return_if_fail (GIMP_IS_PARASITE_LIST (list));
  g_return_if_fail (parasite != NULL);
  g_return_if_fail (parasite->name != NULL);

  if (list->table == NULL)
    list->table = g_hash_table_new (g_str_hash, g_str_equal);

  gimp_parasite_list_remove (list, parasite->name);
  parasite = gimp_parasite_copy (parasite);
  g_hash_table_insert (list->table, parasite->name, parasite);

  g_signal_emit (G_OBJECT (list), parasite_list_signals[ADD], 0,
		 parasite);
}

void
gimp_parasite_list_remove (GimpParasiteList *list, 
			   const gchar      *name)
{
  GimpParasite *parasite;

  g_return_if_fail (GIMP_IS_PARASITE_LIST (list));

  if (list->table)
    {
      parasite = gimp_parasite_list_find (list, name);

      if (parasite)
	{
	  g_hash_table_remove (list->table, name);

	  g_signal_emit (G_OBJECT (list), parasite_list_signals[REMOVE], 0,
			 parasite);

	  gimp_parasite_free (parasite);
	}
    }
}

gint
gimp_parasite_list_length (GimpParasiteList *list)
{
  g_return_val_if_fail (GIMP_IS_PARASITE_LIST (list), 0);

  if (! list->table)
    return 0;

  return g_hash_table_size (list->table);
}

gint
gimp_parasite_list_persistent_length (GimpParasiteList *list)
{
  gint len = 0;

  g_return_val_if_fail (GIMP_IS_PARASITE_LIST (list), 0);

  if (!list->table)
    return 0;

  gimp_parasite_list_foreach (list,
                              (GHFunc) parasite_count_if_persistent, &len);

  return len;
}

void
gimp_parasite_list_foreach (GimpParasiteList *list, 
			    GHFunc            function, 
			    gpointer          user_data)
{
  g_return_if_fail (GIMP_IS_PARASITE_LIST (list));

  if (!list->table)
    return;

  g_hash_table_foreach (list->table, function, user_data);
}

GimpParasite *
gimp_parasite_list_find (GimpParasiteList *list, 
			 const gchar      *name)
{
  g_return_val_if_fail (GIMP_IS_PARASITE_LIST (list), NULL);

  if (list->table)
    return (GimpParasite *) g_hash_table_lookup (list->table, name);
  else
    return NULL;
}


static void
parasite_serialize (const gchar  *key,
                    GimpParasite *parasite,
                    gint         *fd_ptr)
{
  GString     *str;
  const gchar *data;
  guint32      len;

  /* return if write failed earlier */
  if (*fd_ptr == -1)
    return;

  if (! gimp_parasite_is_persistent (parasite))
    return;

  str = g_string_sized_new (64);
      
  g_string_printf (str, "(parasite \"%s\" %lu \"",
                   gimp_parasite_name (parasite),
                   gimp_parasite_flags (parasite));

  /*
   * the current methodology is: never move the parasiterc from one
   * system to another. If you want to do this you should probably
   * write out parasites which contain any non-alphanumeric(+some)
   * characters as \xHH sequences altogether.
   */

  data = (const gchar *) gimp_parasite_data (parasite);

  for (len = gimp_parasite_data_size (parasite); len > 0; len--, data++)
    {
      switch (*data)
        {
        case '\\': g_string_append_len (str, "\\\\", 2); break;
        case '\0': g_string_append_len (str, "\\0" , 2); break;
        case '"' : g_string_append_len (str, "\\\"", 2); break;
          /* disabled, not portable!  */
          /*              case '\n': fputs ("\\n", fp); break;*/
          /*              case '\r': fputs ("\\r", fp); break;*/
        case 26  : g_string_append_len (str, "\\z",  2); break;

        default  : g_string_append_c (str, *data);
          break;
        }
    }

  g_string_append (str, "\")\n\n");

  if (write (*fd_ptr, str->str, str->len) == -1)
    *fd_ptr = -1;

  g_string_free (str, TRUE);
}

static void
parasite_copy (const gchar      *key,
               GimpParasite     *parasite,
               GimpParasiteList *list)
{
  gimp_parasite_list_add (list, parasite);
}

static gboolean
parasite_free (const gchar  *key,
               GimpParasite *parasite,
               gpointer     unused)
{
  gimp_parasite_free (parasite);

  return TRUE;
}

static void 
parasite_count_if_persistent (const gchar  *key, 
                              GimpParasite *parasite, 
                              gint         *count)
{
  if (gimp_parasite_is_persistent (parasite))
    *count = *count + 1;
}
