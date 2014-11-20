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

#ifndef __OFA_PRINT_GENERAL_BOOKS_H__
#define __OFA_PRINT_GENERAL_BOOKS_H__

/**
 * SECTION: ofa_print_general_books
 * @short_description: #ofaPrintGeneralBooks class definition.
 * @include: ui/ofa-print-gen-ledger.h
 *
 * Print the General Ledger summary.
 *
 * This is a convenience class around a GtkPrintOperation.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PRINT_GENERAL_BOOKS                ( ofa_print_general_books_get_type())
#define OFA_PRINT_GENERAL_BOOKS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PRINT_GENERAL_BOOKS, ofaPrintGeneralBooks ))
#define OFA_PRINT_GENERAL_BOOKS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PRINT_GENERAL_BOOKS, ofaPrintGeneralBooksClass ))
#define OFA_IS_PRINT_GENERAL_BOOKS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PRINT_GENERAL_BOOKS ))
#define OFA_IS_PRINT_GENERAL_BOOKS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PRINT_GENERAL_BOOKS ))
#define OFA_PRINT_GENERAL_BOOKS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PRINT_GENERAL_BOOKS, ofaPrintGeneralBooksClass ))

typedef struct _ofaPrintGeneralBooksPrivate         ofaPrintGeneralBooksPrivate;

typedef struct {
	/*< public members >*/
	GObject                  parent;

	/*< private members >*/
	ofaPrintGeneralBooksPrivate *priv;
}
	ofaPrintGeneralBooks;

typedef struct {
	/*< public members >*/
	GObjectClass              parent;
}
	ofaPrintGeneralBooksClass;

GType    ofa_print_general_books_get_type( void ) G_GNUC_CONST;

gboolean ofa_print_general_books_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_PRINT_GENERAL_BOOKS_H__ */
