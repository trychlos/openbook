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

#ifndef __OFA_PDF_ACCOUNTS_BALANCE_H__
#define __OFA_PDF_ACCOUNTS_BALANCE_H__

/**
 * SECTION: ofa_pdf_accounts_balance
 * @short_description: #ofaPDFAccountsBalance class definition.
 * @include: ui/ofa-print-reconcil.h
 *
 * Print the balances of entries between two dates.
 *
 * The print displays the balance of entries whose effect date is
 * included in the specified period.
 */

#include "core/ofa-main-window-def.h"

#include "ui/ofa-pdf-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_PDF_ACCOUNTS_BALANCE                ( ofa_pdf_accounts_balance_get_type())
#define OFA_PDF_ACCOUNTS_BALANCE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PDF_ACCOUNTS_BALANCE, ofaPDFAccountsBalance ))
#define OFA_PDF_ACCOUNTS_BALANCE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PDF_ACCOUNTS_BALANCE, ofaPDFAccountsBalanceClass ))
#define OFA_IS_PDF_ACCOUNTS_BALANCE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PDF_ACCOUNTS_BALANCE ))
#define OFA_IS_PDF_ACCOUNTS_BALANCE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PDF_ACCOUNTS_BALANCE ))
#define OFA_PDF_ACCOUNTS_BALANCE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PDF_ACCOUNTS_BALANCE, ofaPDFAccountsBalanceClass ))

typedef struct _ofaPDFAccountsBalancePrivate         ofaPDFAccountsBalancePrivate;

typedef struct {
	/*< public members >*/
	ofaPDFDialog          parent;

	/*< private members >*/
	ofaPDFAccountsBalancePrivate *priv;
}
	ofaPDFAccountsBalance;

typedef struct {
	/*< public members >*/
	ofaPDFDialogClass     parent;
}
	ofaPDFAccountsBalanceClass;

GType    ofa_pdf_accounts_balance_get_type( void ) G_GNUC_CONST;

gboolean ofa_pdf_accounts_balance_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_PDF_ACCOUNTS_BALANCE_H__ */
