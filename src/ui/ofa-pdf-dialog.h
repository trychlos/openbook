/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_PDF_DIALOG_H__
#define __OFA_PDF_DIALOG_H__

/**
 * SECTION: ofa_pdf_dialog
 * @short_description: #ofaPDFDialog class definition.
 * @include: ui/ofa-pdf-dialog.h
 *
 * The #ofaPDFDialog is a base class for the dialogs which manage the
 * 'Export as PDF' functions.
 *
 * The #ofaPDFDialog base class adds a tab at the end of the notebook
 * provided by the child class, in order to let the user manage the
 * exported filename.
 */

#include "api/my-dialog.h"

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PDF_DIALOG                ( ofa_pdf_dialog_get_type())
#define OFA_PDF_DIALOG( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PDF_DIALOG, ofaPDFDialog ))
#define OFA_PDF_DIALOG_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PDF_DIALOG, ofaPDFDialogClass ))
#define OFA_IS_PDF_DIALOG( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PDF_DIALOG ))
#define OFA_IS_PDF_DIALOG_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PDF_DIALOG ))
#define OFA_PDF_DIALOG_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PDF_DIALOG, ofaPDFDialogClass ))

typedef struct _ofaPDFDialogPrivate        ofaPDFDialogPrivate;

typedef struct {
	/*< public members >*/
	myDialog             parent;

	/*< private members >*/
	ofaPDFDialogPrivate *priv;
}
	ofaPDFDialog;

typedef struct {
	/*< public members >*/
	myDialogClass        parent;
}
	ofaPDFDialogClass;

/**
 * ofaPDFDialog properties:
 *
 * @PDF_PROP_LABEL:      the label displayed in the #GtkFileChooser
 *                       added tab.
 *                       Default to localized string: 'Export as'.
 * @PDF_PROP_DEF_NAME:   the default name displayed in the entry of the
 *                       #GtkFileChooser added tab.
 *                       Default to localized string: 'Untitled'.
 * @PDF_PROP_PREF_NAME:  the name of the user preference which stores
 *                       the last selected filename.
 *                       No default (implies no user preference).
 */
#define PDF_PROP_LABEL                  "ofa-pdf-prop-label"
#define PDF_PROP_DEF_NAME               "ofa-pdf-prop-def-name"
#define PDF_PROP_PREF_NAME              "ofa-pdf-prop-pref-name"

GType        ofa_pdf_dialog_get_type    ( void ) G_GNUC_CONST;

gchar       *ofa_pdf_dialog_get_filename( const ofaPDFDialog *dialog );

const gchar *ofa_pdf_dialog_get_uri     ( const ofaPDFDialog *dialog );

G_END_DECLS

#endif /* __OFA_PDF_DIALOG_H__ */
