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

#ifndef __OFA_BASE_DIALOG_H__
#define __OFA_BASE_DIALOG_H__

/**
 * SECTION: ofa_base_dialog
 * @short_description: #ofaBaseDialog class definition.
 * @include: ui/ofa-base-dialog.h
 *
 * Base class for application dialog boxes.
 */

#include "ui/ofa-main-window-def.h"
#include "ui/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BASE_DIALOG                ( ofa_base_dialog_get_type())
#define OFA_BASE_DIALOG( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BASE_DIALOG, ofaBaseDialog ))
#define OFA_BASE_DIALOG_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BASE_DIALOG, ofaBaseDialogClass ))
#define OFA_IS_BASE_DIALOG( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BASE_DIALOG ))
#define OFA_IS_BASE_DIALOG_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BASE_DIALOG ))
#define OFA_BASE_DIALOG_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BASE_DIALOG, ofaBaseDialogClass ))

typedef struct _ofaBaseDialogPrivate        ofaBaseDialogPrivate;
typedef struct _ofaBaseDialogProtected      ofaBaseDialogProtected;

typedef struct {
	/*< private >*/
	GObject                 parent;
	ofaBaseDialogPrivate   *priv;
	ofaBaseDialogProtected *prot;
}
	ofaBaseDialog;

typedef struct {
	/*< private >*/
	GObjectClass parent;

	/*< virtual functions >*/
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
	void     ( *init_dialog )         ( ofaBaseDialog *dialog );

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
	gint     ( *run_dialog )          ( ofaBaseDialog *dialog );

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
	gboolean ( *quit_on_delete_event )( ofaBaseDialog *dialog );

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
	gboolean ( *quit_on_cancel )      ( ofaBaseDialog *dialog );

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
	gboolean ( *quit_on_close )       ( ofaBaseDialog *dialog );

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
	gboolean ( *quit_on_ok )          ( ofaBaseDialog *dialog );
}
	ofaBaseDialogClass;

/**
 * Properties defined by the ofaBaseDialog class.
 *
 * @OFA_PROP_MAIN_WINDOW:      main window of the application
 * @OFA_PROP_DIALOG_XML:       path to the xml file which contains the UI description
 * @OFA_PROP_DIALOG_NAME:      dialog box name
 */
#define OFA_PROP_MAIN_WINDOW                  "ofa-dialog-prop-main-window"
#define OFA_PROP_DIALOG_XML                   "ofa-dialog-prop-xml"
#define OFA_PROP_DIALOG_NAME                  "ofa-dialog-prop-name"

GType          ofa_base_dialog_get_type       ( void );

gboolean       ofa_base_dialog_init_dialog    ( ofaBaseDialog *dialog );

gint           ofa_base_dialog_run_dialog     ( ofaBaseDialog *dialog );

ofoDossier    *ofa_base_dialog_get_dossier    ( const ofaBaseDialog *dialog );

ofaMainWindow *ofa_base_dialog_get_main_window( const ofaBaseDialog *dialog );

G_END_DECLS

#endif /* __OFA_BASE_DIALOG_H__ */
