/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __MY_DIALOG_H__
#define __MY_DIALOG_H__

/**
 * SECTION: my_dialog
 * @short_description: #myDialog class definition.
 * @include: core/my-dialog.h
 *
 * The base class for application dialog boxes.
 */

#include "core/my-window.h"

G_BEGIN_DECLS

#define MY_TYPE_DIALOG                ( my_dialog_get_type())
#define MY_DIALOG( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_DIALOG, myDialog ))
#define MY_DIALOG_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_DIALOG, myDialogClass ))
#define MY_IS_DIALOG( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_DIALOG ))
#define MY_IS_DIALOG_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_DIALOG ))
#define MY_DIALOG_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_DIALOG, myDialogClass ))

typedef struct _myDialogPrivate       myDialogPrivate;

typedef struct {
	/*< public members >*/
	myWindow           parent;

	/*< private members >*/
	myDialogPrivate   *priv;
}
	myDialog;

typedef struct {
	/*< public members >*/
	myWindowClass parent;

	/*< protected virtual functions >*/
	/**
	 * init_dialog:
	 * @dialog:
	 *
	 * Initializes the dialog box before being first displayed.
	 *
	 * The base class takes care of loading the widgets hierarchy from
	 * the construction-time-provided XML definition file before calling
	 * this method.
	 *
	 * The user should implement this method as the base class is not
	 * able to do anything in this matter.
	 *
	 * The base class will take care of showing all widgets after this
	 * method returns.
	 */
	void     ( *init_dialog )         ( myDialog *dialog );

	/**
	 * run_dialog:
	 * @dialog:
	 *
	 * Run the dialog box until the loop quits (see below).
	 * The function returns the gtk_dialog_run() code, provided that
	 * the corresponding quit_on_xxxx() method below has returned
	 * %TRUE.
	 *
	 * The base class default implementation just run the loop.
	 */
	gint     ( *run_dialog )          ( myDialog *dialog );

	/**
	 * quit_on_delete_event:
	 * @dialog:
	 *
	 * Ask whether the dialog box should quit on 'delete-event' event.
	 *
	 * The implementation should returns %TRUE in order to allow to
	 * terminate the dialog box, %FALSE else.
	 *
	 * The base class default implementation returns %TRUE.
	 */
	gboolean ( *quit_on_delete_event )( myDialog *dialog );

	/**
	 * quit_on_cancel:
	 * @dialog:
	 *
	 * Ask whether the dialog box should quit on 'cancel' event.
	 *
	 * The implementation should returns %TRUE in order to allow to
	 * terminate the dialog box, %FALSE else.
	 *
	 * The base class default implementation returns %TRUE.
	 */
	gboolean ( *quit_on_cancel )      ( myDialog *dialog );

	/**
	 * quit_on_close:
	 * @dialog:
	 *
	 * Ask whether the dialog box should quit on 'close' event.
	 *
	 * The implementation should returns %TRUE in order to allow to
	 * terminate the dialog box, %FALSE else.
	 *
	 * The base class default implementation returns %TRUE.
	 */
	gboolean ( *quit_on_close )       ( myDialog *dialog );

	/**
	 * quit_on_ok:
	 * @dialog:
	 *
	 * Ask whether the dialog box should quit on 'OK' event.
	 *
	 * The implementation should returns %TRUE in order to allow to
	 * terminate the dialog box, %FALSE else.
	 *
	 * The base class default implementation returns %TRUE.
	 */
	gboolean ( *quit_on_ok )          ( myDialog *dialog );

	/**
	 * quit_on_code:
	 * @dialog:
	 *
	 * Ask whether the dialog box should quit on the specified response
	 * code.
	 *
	 * The implementation should returns %TRUE in order to allow to
	 * terminate the dialog box, %FALSE else.
	 *
	 * The base class default implementation returns %FALSE.
	 */
	gboolean ( *quit_on_code )        ( myDialog *dialog, gint code );
}
	myDialogClass;

GType          my_dialog_get_type   ( void ) G_GNUC_CONST;

gboolean       my_dialog_init_dialog( myDialog *dialog );

gint           my_dialog_run_dialog ( myDialog *dialog );

G_END_DECLS

#endif /* __MY_DIALOG_H__ */
