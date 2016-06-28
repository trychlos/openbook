/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 * Note on 'hide_on_close' indicator:
 * Some windows do not want to be destroyed when user closes them, but
 * just be hidden (most of time because of a big initialization and a
 * layout which would gain to be re-used).
 * When the 'hide_on_close' indicator is set, these windows are hidden
 * instead of being destroyed at close time.
 *
 * Note on non-modal windows.
 * 1/ They cannot have a return code
 * 2/ this myIWindow interface takes care of having at most one instance
 *    of each IWindow identifier (usually the dialog class name + object
 *    identifier)
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

	/**
	 * get_default_size:
	 * @instance: the #myIWindow instance.
	 * @x: [out]:
	 * @y: [out]:
	 * @cx: [out]:
	 * @cy: [out]:
	 *
	 * Let the user provide its own default size and position when no
	 * size and position have been recorded from a previous use.
	 *
	 * Since: version 1.
	 */
	void     ( *get_default_size )     ( myIWindow *instance,
												gint *x,
												gint *y,
												gint *cx,
												gint *cy );

	/**
	 * read_settings:
	 * @instance: the #myIWindow instance.
	 * @settings: the #myISettings implementation provided by the
	 *  application.
	 * @keyname: this is the identifier, plus the '-settings' suffix.
	 *
	 * Called at initialization time, after window creation and restore
	 * of size and position, to let the application read its own settings.
	 *
	 * In particular, this #read_settings() method is called before the
	 * #idialog_init() one.
	 *
	 * Since: version 1.
	 */
	void     ( *read_settings )        ( myIWindow *instance,
												myISettings *settings,
												const gchar *keyname );

	/**
	 * write_settings:
	 * @instance: the #myIWindow instance.
	 * @settings: the #myISettings implementation provided by the
	 *  application.
	 * @keyname: this is the identifier, plus the '-settings' suffix.
	 *
	 * Called at dispose time, before save of window size and position,
	 * to let the application write its own settings.
	 *
	 * Since: version 1.
	 */
	void     ( *write_settings )       ( myIWindow *instance,
												myISettings *settings,
												const gchar *keyname );

	/**
	 * is_destroy_allowed:
	 * @instance: the #myIWindow instance.
	 *
	 * Returns: %TRUE if the @instance accepts to be destroyed.
	 *
	 * Since: version 1.
	 */
	gboolean ( *is_destroy_allowed )   ( const myIWindow *instance );
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

myISettings *my_iwindow_get_settings              ( const myIWindow *instance );

void         my_iwindow_set_settings              ( myIWindow *instance,
														myISettings *settings );

gchar       *my_iwindow_get_keyname               ( const myIWindow *instance );

void         my_iwindow_set_restore_pos           ( myIWindow *instance,
														gboolean restore_pos );

void         my_iwindow_set_restore_size          ( myIWindow *instance,
														gboolean restore_size );

void         my_iwindow_init                      ( myIWindow *instance );

myIWindow   *my_iwindow_present                   ( myIWindow *instance );

void         my_iwindow_close                     ( myIWindow *instance );

void         my_iwindow_close_all                 ( void );

void         my_iwindow_msg_dialog                ( myIWindow *instance,
														GtkMessageType type,
														const gchar *msg );

G_END_DECLS

#endif /* __MY_API_MY_IWINDOW_H__ */
