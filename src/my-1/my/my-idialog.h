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

#ifndef __MY_API_MY_IDIALOG_H__
#define __MY_API_MY_IDIALOG_H__

/**
 * SECTION: idialog
 * @title: myIDialog
 * @short_description: An interface to manage our dialogs.
 * @include: my/my-idialog.h
 *
 * This is a GtkDialog extension:
 * - the implementation must derives from GtkDialog
 * - it is expected that the implementation also implements myIWindow
 *   (though this cannot be forced by the code).
 *
 * This interface manages for the application:
 * - the dialog buttons
 * - the modal and non-modal dialogs.
 *
 * Response codes are taken from /usr/include/gtk-3.0/gtk/gtkdialog.h.
 * Most often used are:
 *   GTK_RESPONSE_DELETE_EVENT = -4,
 *   GTK_RESPONSE_OK           = -5,
 *   GTK_RESPONSE_CANCEL       = -6,
 *   GTK_RESPONSE_CLOSE        = -7
 *
 * A #myIDialog dialog may be run under three modes:
 * - as a non-modal dialog: see #my_iwindow_present()
 * - as a modal dialog: see #my_idialog_run()
 * - as a modal or non-modal dialog depending of the parent modality:
 *   > if the parent is modal, then run modal,
 *   > if the parent is non-modal (or not set), run non modal
 *   see #my_idialog_run_maybe_modal().
 *
 * As a reminder, only a modal dialog (i.e. called with #my_idialog_run())
 * is able to return some valuable and meaningful information.
 */

#include <gtk/gtk.h>

#include "my/my-iwindow.h"

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
 * @init: [should]: one-time initialization.
 *
 * This defines the interface that an #myIDialog may/should/must
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
	 * init:
	 * @instance: the #myIDialog instance.
	 *
	 * This is called once, before the first presentation, when running
	 * a modal dialog.
	 *
	 * Please note that the #myIDialog interface takes care of calling
	 * gtk_widget_show_all() on the @instance after returned from this
	 * method.
	 *
	 * Since: version 1.
	 */
	void     ( *init )                 ( myIDialog *instance );

	/**
	 * quit_on_ok:
	 * @instance: the #myIDialog instance.
	 *
	 * Returns: %TRUE if the implementation is OK to terminate the
	 * dialog loop.
	 *
	 * This method is only used for modal dialogs which are run from
	 * #my_idialog_run() function.
	 *
	 * Note that returning from the dialog loop does not mean
	 * terminating the window. The caller has still to explicitely
	 * close the #myIWindow himself after having got the needed datas.
	 *
	 * Defaults to %TRUE.
	 *
	 * Since: version 1.
	 */
	gboolean ( *quit_on_ok )           ( myIDialog *instance );

	/**
	 * quit_on_code:
	 * @instance: the #myIDialog instance.
	 * @response_code: the response code.
	 *
	 * Returns: %TRUE if the implementation is OK to terminate the dialog.
	 *
	 * Since: version 1.
	 */
	gboolean ( *quit_on_code )         ( myIDialog *instance,
												gint response_code );
}
	myIDialogInterface;

/*
 * Interface-wide
 */
GType      my_idialog_get_type                  ( void );

guint      my_idialog_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint      my_idialog_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void       my_idialog_init                      ( myIDialog *instance );

GtkWidget *my_idialog_set_close_button          ( myIDialog *instance );

gint       my_idialog_run                       ( myIDialog *instance );

myIWindow *my_idialog_run_maybe_modal           ( myIDialog *instance );

G_END_DECLS

#endif /* __MY_API_MY_IDIALOG_H__ */
