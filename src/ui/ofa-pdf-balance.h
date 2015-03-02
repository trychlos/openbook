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
 *
 */

#ifndef __OFA_PDF_BALANCE_H__
#define __OFA_PDF_BALANCE_H__

/**
 * SECTION: ofa_pdf_balance
 * @short_description: #ofaPDFBalance class definition.
 * @include: ui/ofa-print-reconcil.h
 *
 * Print the reconciliation summary.
 *
 * This is a convenience class around a GtkPrintOperation.
 */

#include "core/ofa-main-window-def.h"

#include "ui/ofa-pdf-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_PDF_BALANCE                ( ofa_pdf_balance_get_type())
#define OFA_PDF_BALANCE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PDF_BALANCE, ofaPDFBalance ))
#define OFA_PDF_BALANCE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PDF_BALANCE, ofaPDFBalanceClass ))
#define OFA_IS_PDF_BALANCE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PDF_BALANCE ))
#define OFA_IS_PDF_BALANCE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PDF_BALANCE ))
#define OFA_PDF_BALANCE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PDF_BALANCE, ofaPDFBalanceClass ))

typedef struct _ofaPDFBalancePrivate        ofaPDFBalancePrivate;

typedef struct {
	/*< public members >*/
	ofaPDFDialog          parent;

	/*< private members >*/
	ofaPDFBalancePrivate *priv;
}
	ofaPDFBalance;

typedef struct {
	/*< public members >*/
	ofaPDFDialogClass     parent;
}
	ofaPDFBalanceClass;

GType    ofa_pdf_balance_get_type( void ) G_GNUC_CONST;

gboolean ofa_pdf_balance_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_PDF_BALANCE_H__ */
