/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OPENBOOK_API_MY_IDIALOG_H__
#define __OPENBOOK_API_MY_IDIALOG_H__

/**
 * SECTION: idialog
 * @title: myIDialog
 * @short_description: An interface to manage the dialog boxes.
 * @include: openbook/my-idialog.h
 *
 * This interface manages for the application:
 * - the size and the position of windows and dialogs
 * - the modal dialogs
 * - the non-modal dialogs.
 *
 * Notes on non-modal dialogs.
 * 1/ They cannot have a return code
 * 2/ this myIDialog interface takes care of having at most one instance
 *    of each dialog class + object identifier
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_IDIALOG                      ( my_idialog_get_type())
#define MY_IDIALOG( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IDIALOG, myIDialog ))
#define MY_IS_IDIALOG( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IDIALOG ))
#define MY_IDIALOG_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IDIALOG, myIDialogInterface ))

typedef struct _myIDialog                    myIDialog;

/**
 * myIDialogInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_identifier: [should]: returns the identifier of this window.
 * @init: [should]: one-time initialization.
 * @quit_on_escape: [should]: let ask for a user confirmation.
 *
 * This defines the interface that an #myIDialog may/should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #myIDialog instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implemented by the instance.
	 *
	 * Returns: the version number of this interface that the @instance
	 *  is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint    ( *get_interface_version )( const myIDialog *instance );

	/**
	 * get_identifier:
	 * @instance: the #myIDialog instance.
	 *
	 * Returns: The identifier of this window, as a newly allocated
	 * string which will be #g_free() by the interface code.
	 *
	 * This is mostly used on non-modal windows management, in order to
	 * make sure there is only one instance for a given 'identifier'.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_identifier )       ( const myIDialog *instance );

	/**
	 * init:
	 * @instance: the #myIDialog instance.
	 *
	 * This is called once, before the first presentation.
	 *
	 * Since: version 1
	 */
	void     ( *init )                 ( myIDialog *instance );

	/**
	 * quit_on_escape:
	 * @instance: the #myIDialog instance.
	 *
	 * Called when the user asks for quitting the dialog by hitting
	 * the 'Escape' key, or destroy the window.
	 *
	 * Returns: %TRUE if the user confirms that he wants quit the
	 * dialog.
	 *
	 * Defaults to %TRUE.
	 *
	 * Since: version 1
	 */
	gboolean ( *quit_on_escape )       ( const myIDialog *instance );
}
	myIDialogInterface;

GType                 my_idialog_get_type                  ( void );

guint                 my_idialog_get_interface_last_version( void );

guint                 my_idialog_get_interface_version     ( const myIDialog *instance );

GtkApplicationWindow *my_idialog_get_main_window           ( const myIDialog *instance );

void                  my_idialog_set_main_window           ( myIDialog *instance,
																	GtkApplicationWindow *main_window );

void                  my_idialog_set_ui_from_file          ( myIDialog *instance,
																	const gchar *xml_fname,
																	const gchar *toplevel_name );

void                  my_idialog_present                   ( myIDialog *instance );

void                  my_idialog_close                     ( myIDialog *instance );

GtkWidget            *my_idialog_set_close_button          ( myIDialog *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_IDIALOG_H__ */
