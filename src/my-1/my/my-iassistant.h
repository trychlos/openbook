/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __MY_API_MY_IASSISTANT_H__
#define __MY_API_MY_IASSISTANT_H__

/**
 * SECTION: idialog
 * @title: myIAssistant
 * @short_description: An interface to manage our GtkAssistant-derived assistants.
 * @include: my/my-iassistant.h
 *
 * This is a GtkAssistant extension:
 * - the implementation must derives from GtkAssistant
 * - it is expected that the implementation also implements myIWindow
 *   (though this cannot be forced by the code).
 *
 * This interface provides to the implementation a callback for each
 * state of each page:
 * - init: before the page be displayed for the first time; this is a
 *   once-time initialization
 * - forward: after the user has clicked on 'next', and before the next
 *   page is displayed
 * - display: before the page be displayed, every time, whether it is
 *   because the user has clicked 'back' or 'next'.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_IASSISTANT                      ( my_iassistant_get_type())
#define MY_IASSISTANT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_IASSISTANT, myIAssistant ))
#define MY_IS_IASSISTANT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_IASSISTANT ))
#define MY_IASSISTANT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_IASSISTANT, myIAssistantInterface ))

typedef struct _myIAssistant                    myIAssistant;

/**
 * myIAssistantInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @is_willing_to_quit: [may]: returns %TRUE if the user is willing to quit.
 *
 * This defines the interface that an #myIAssistant may/should/must
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
	 * is_willing_to_quit:
	 * @instance: the #myIAssistant instance.
	 * @keyval: the key pressed;
	 *  this value is from gdkkeysyms.h, and may take the values:
	 *  - GDK_KEY_Escape
	 *  - GDK_KEY_Cancel.
	 *
	 * When the user hits the 'Cancel' or the 'Escape' key, the interface
	 * request this method to know if the user is actually willing to
	 * quit.
	 *
	 * Returns: %TRUE to actuallly quit, %FALSE else.
	 *
	 * Defaults to %TRUE.
	 *
	 * Since: version 1.
	 */
	gboolean ( *is_willing_to_quit )   ( myIAssistant *instance,
												guint keyval );
	/**
	 * on_prepare:
	 * @instance: the #myIAssistant instance.
	 * @page: the page to be prepared.
	 *
	 * The interface calls this method before each page be displayed so that
	 * the implementation may decide or not to run the default tasks.
	 *
	 * If this method is not implemented, then the interface executes the default
	 * my_iassistant_do_prepare() code:
	 * - if this is the first time, then call the 'init' cb
	 * - only then, call the 'display' cb
	 *
	 * Most of the time, it is not needed for the implementation to implement
	 * this method as the default traitement already takes care of calling
	 * 'init' and 'dsplay' callbacks.
	 *
	 * The "prepare" signal for the first page (usually Introduction) is
	 * sent during GtkAssistant construction, so before the derived class
	 * has any chance to connect to it.
	 *
	 * Since: version 2.
	 */
	void     ( *on_prepare )           ( myIAssistant *instance,
												GtkWidget *page );
	/**
	 * on_cancel:
	 * @instance: the #myIAssistant instance.
	 * @keyval: the hit key, which may be the 'Cancel' button or the 'Esc' key.
	 *
	 * This method is called when the user clicks on the "Cancel"
	 * button, or if he hits the 'Escape' key (and the 'Quit on escape'
	 * preference is set).
	 *
	 * If this method is not implemented, then the interface executes the default
	 * my_iassistant_do_cancel() code:
	 * - if the user confirms he is willing to quit, then closes the window.
	 *
	 * Since: version 2.
	 */
	void     ( *on_cancel )            ( myIAssistant *instance,
												guint keyval );

	/**
	 * on_close:
	 * @instance: the #myIAssistant instance.
	 *
	 * This method is called when the user hits the 'Close' key after the
	 * assistant is complete.
	 *
	 * If this method is not implemented, then the interface executes the default
	 * my_iassistant_do_close() code:
	 * - close the window.
	 *
	 * Since: version 2.
	 */
	void     ( *on_close )             ( myIAssistant *instance );
}
	myIAssistantInterface;

/**
 * myIAssistantCb:
 * @assistant: this #myIAssistant instance.
 * @page_num: counted from zero.
 * @page_widget: the page.
 */
typedef void ( *myIAssistantCb )( myIAssistant *instance, gint page_num, GtkWidget *page_widget );

/**
 * ofsIAssistant:
 */
typedef struct {
	gint           page_num;
	myIAssistantCb init_cb;
	myIAssistantCb display_cb;
	myIAssistantCb forward_cb;
}
	ofsIAssistant;

/*
 * Interface-wide
 */
GType    my_iassistant_get_type                  ( void );

guint    my_iassistant_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint    my_iassistant_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void     my_iassistant_set_callbacks             ( myIAssistant *instance,
														const ofsIAssistant *cbs );

void     my_iassistant_do_cancel                 ( myIAssistant *instance,
														guint keyval );

void     my_iassistant_do_close                  ( myIAssistant *instance );

void     my_iassistant_do_prepare                ( myIAssistant *instance,
														GtkWidget *page );

gboolean my_iassistant_get_page_complete         ( myIAssistant *instance,
														gint page_num );

gboolean my_iassistant_has_been_cancelled        ( myIAssistant *instance );

gboolean my_iassistant_is_page_initialized       ( const myIAssistant *instance,
														GtkWidget *page );

void     my_iassistant_set_page_initialized      ( myIAssistant *instance,
														GtkWidget *page,
														gboolean initialized );

void     my_iassistant_set_current_page_complete ( myIAssistant *instance,
														gboolean complete );

void     my_iassistant_set_current_page_type     ( myIAssistant *instance,
														GtkAssistantPageType type );

G_END_DECLS

#endif /* __MY_API_MY_IASSISTANT_H__ */
