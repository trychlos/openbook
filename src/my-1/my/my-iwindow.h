/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __MY_API_MY_IWINDOW_H__
#define __MY_API_MY_IWINDOW_H__

/**
 * SECTION: idialog
 * @title: myIWindow
 * @short_description: An interface to manage our GtkWindow-derived windows.
 * @include: my/my-iwindow.h
 *
 * This is a GtkWindow extension:
 * - the implementation must derives from GtkWindow.
 *
 * This interface manages for the application:
 * - the size and the position of windows, including dialogs and assistants
 * - the modal dialogs
 * - the non-modal windows.
 *
 * Note on myIWindow identifier.
 * #my_iwindow_present() method makes sure at most one instance of each
 * #myIWindow identifier is opened as every time. This identifier defaults
 * to the class name.
 *
 * Note on size and position.
 * Size and position of the window default to be:
 * - restored from the user settings at one-time window initialization,
 * - saved to user settings at close time.
 * The key of the size/position settings defaults to the #myIWindow
 * identifier, plus a '-pos' suffix added by my_utils.
 *
 * Note on #my_iwindow_close().
 * The application should call #my_iwindow_close() to close a #myIWindow
 * instance. Its default behavior is to #gtk_widget_destroy() the window
 * unless the 'close_allowed' indicator has been previously cleared.
 */

#include <gtk/gtk.h>

#include "my/my-isettings.h"

G_BEGIN_DECLS

#define MY_TYPE_IWINDOW                      ( my_iwindow_get_type())
#define MY_IWINDOW( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IWINDOW, myIWindow ))
#define MY_IS_IWINDOW( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IWINDOW ))
#define MY_IWINDOW_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IWINDOW, myIWindowInterface ))

typedef struct _myIWindow                    myIWindow;

/**
 * myIWindowInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_identifier: [may]: returns the identifier of this window.
 * @init: [may]: one-time initialization.
 * @get_default_size: [may]: returns default size.
 * @quit_on_escape: [may]: let ask for a user confirmation.
 * @read_settings: [may]: read user preferences.
 * @write_settings: [may]: write user preferences.
 *
 * This defines the interface that an #myIWindow may/should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_identifier:
	 * @instance: the #myIWindow instance.
	 *
	 * Returns: The identifier of this window, as a newly allocated
	 * string which will be #g_free() by the interface code.
	 *
	 * This is the default for the settings name.
	 * This is also used on non-modal windows management, in order to
	 * make sure there is only one instance for a given 'identifier'.
	 *
	 * Defaults to class name.
	 *
	 * Since: version 1.
	 */
	gchar *  ( *get_identifier )       ( const myIWindow *instance );

	/**
	 * get_key_prefix:
	 * @instance: the #myIWindow instance.
	 *
	 * Returns: The prefix of the keys in settings file.
	 *
	 * Defaults to identifier.
	 *
	 * Since: version 1.
	 */
	gchar *  ( *get_key_prefix )       ( const myIWindow *instance );

	/**
	 * init:
	 * @instance: the #myIWindow instance.
	 *
	 * This is called once, before the first presentation.
	 *
	 * Since: version 1
	 */
	void     ( *init )                 ( myIWindow *instance );
}
	myIWindowInterface;

/*
 * Interface-wide
 */
GType        my_iwindow_get_type                  ( void );

guint        my_iwindow_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint        my_iwindow_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GtkWindow   *my_iwindow_get_parent                ( const myIWindow *instance );

void         my_iwindow_set_parent                ( myIWindow *instance,
														GtkWindow *parent );

void         my_iwindow_set_geometry_settings     ( myIWindow *instance,
														myISettings *settings );

void         my_iwindow_set_restore_pos           ( myIWindow *instance,
														gboolean restore_pos );

void         my_iwindow_set_restore_size          ( myIWindow *instance,
														gboolean restore_size );

void         my_iwindow_set_close_allowed         ( myIWindow *instance,
														gboolean allowed );

void         my_iwindow_init                      ( myIWindow *instance );

myIWindow   *my_iwindow_present                   ( myIWindow *instance );

void         my_iwindow_close                     ( myIWindow *instance );

void         my_iwindow_close_all                 ( void );

G_END_DECLS

#endif /* __MY_API_MY_IWINDOW_H__ */
