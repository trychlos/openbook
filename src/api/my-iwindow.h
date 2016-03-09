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

#ifndef __OPENBOOK_API_MY_IWINDOW_H__
#define __OPENBOOK_API_MY_IWINDOW_H__

/**
 * SECTION: idialog
 * @title: myIWindow
 * @short_description: An interface to manage our GtkWindow-derived windows.
 * @include: openbook/my-iwindow.h
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
 * @get_identifier: [should]: returns the identifier of this window.
 * @init: [should]: one-time initialization.
 * @quit_on_escape: [should]: let ask for a user confirmation.
 *
 * This defines the interface that an #myIWindow may/should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #myIWindow instance.
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
	guint    ( *get_interface_version )( const myIWindow *instance );

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
	 * Since: version 1
	 */
	gchar *  ( *get_identifier )       ( const myIWindow *instance );

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
	 * Since: version 1
	 */
	void     ( *get_default_size )     ( myIWindow *instance,
												guint *x,
												guint *y,
												guint *cx,
												guint *cy );

	/**
	 * quit_on_escape:
	 * @instance: the #myIWindow instance.
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
	gboolean ( *quit_on_escape )       ( const myIWindow *instance );
}
	myIWindowInterface;

GType                 my_iwindow_get_type                  ( void );

guint                 my_iwindow_get_interface_last_version( void );

guint                 my_iwindow_get_interface_version     ( const myIWindow *instance );

GtkApplicationWindow *my_iwindow_get_main_window           ( const myIWindow *instance );

void                  my_iwindow_set_main_window           ( myIWindow *instance,
																	GtkApplicationWindow *main_window );

void                  my_iwindow_set_parent                ( myIWindow *instance,
																	GtkWindow *parent );

void                  my_iwindow_set_hide_on_close         ( myIWindow *instance,
																	gboolean hide_on_close );

void                  my_iwindow_init                      ( myIWindow *instance );

myIWindow            *my_iwindow_present                   ( myIWindow *instance );

void                  my_iwindow_close                     ( myIWindow *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_IWINDOW_H__ */
