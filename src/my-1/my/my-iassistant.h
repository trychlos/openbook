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

#ifndef __MY_API_MY_IASSISTANT_H__
#define __MY_API_MY_IASSISTANT_H__

/**
 * SECTION: idialog
 * @title: myIAssistant
 * @short_description: An interface to manage our GtkAssistant-derived assistants.
 * @include: my/my-iassistant.h
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
	/**
	 * get_interface_version:
	 * @instance: the #myIAssistant instance.
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
	guint    ( *get_interface_version )( const myIAssistant *instance );

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
	 * Since: version 1
	 */
	gboolean ( *is_willing_to_quit )   ( myIAssistant *instance,
												guint keyval );
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

GType    my_iassistant_get_type                  ( void );

guint    my_iassistant_get_interface_last_version( void );

guint    my_iassistant_get_interface_version     ( const myIAssistant *instance );

void     my_iassistant_set_callbacks             ( myIAssistant *instance,
														const ofsIAssistant *cbs );

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