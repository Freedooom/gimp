#ifndef __file_dialog_new_h__
#define __file_dialog_new_h__

#include "gtk/gtk.h"

void file_new_cmd_callback (GtkWidget           *widget,
			    gpointer             callback_data,
			    guint                callback_action);

void file_new_reset_current_cut_buffer();

#endif /* __FILE_DIALOG_NEW_H__ */
