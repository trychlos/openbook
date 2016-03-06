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
 * @short_description: An interface to manage our dialogs.
 * @include: openbook/my-idialog.h
 *
 * This interface manages for the application:
 * - the dialog buttons.
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
}
	myIDialogInterface;

GType      my_idialog_get_type                  ( void );

guint      my_idialog_get_interface_last_version( void );

guint      my_idialog_get_interface_version     ( const myIDialog *instance );

void       my_idialog_init_dialog               ( myIDialog *instance );

void       my_idialog_widget_click_to_close     ( myIDialog *instance,
														GtkWidget *button );

GtkWidget *my_idialog_set_close_button          ( myIDialog *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_IDIALOG_H__ */
