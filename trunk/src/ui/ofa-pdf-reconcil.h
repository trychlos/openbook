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
 * $Id$
 */

#ifndef __OFA_PDF_RECONCIL_H__
#define __OFA_PDF_RECONCIL_H__

/**
 * SECTION: ofa_pdf_reconcil
 * @short_description: #ofaPDFReconcil class definition.
 * @include: ui/ofa-pdf-reconcil.h
 *
 * Print the reconciliation summary.
 *
 * This is a convenience class around a GtkPrintOperation.
 */

#include "core/ofa-main-window-def.h"

#include "ui/ofa-pdf-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_PDF_RECONCIL                ( ofa_pdf_reconcil_get_type())
#define OFA_PDF_RECONCIL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PDF_RECONCIL, ofaPDFReconcil ))
#define OFA_PDF_RECONCIL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PDF_RECONCIL, ofaPDFReconcilClass ))
#define OFA_IS_PDF_RECONCIL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PDF_RECONCIL ))
#define OFA_IS_PDF_RECONCIL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PDF_RECONCIL ))
#define OFA_PDF_RECONCIL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PDF_RECONCIL, ofaPDFReconcilClass ))

typedef struct _ofaPDFReconcilPrivate        ofaPDFReconcilPrivate;

typedef struct {
	/*< public members >*/
	ofaPDFDialog           parent;

	/*< private members >*/
	ofaPDFReconcilPrivate *priv;
}
	ofaPDFReconcil;

typedef struct {
	/*< public members >*/
	ofaPDFDialogClass      parent;
}
	ofaPDFReconcilClass;

GType    ofa_pdf_reconcil_get_type( void ) G_GNUC_CONST;

gboolean ofa_pdf_reconcil_run     ( ofaMainWindow *parent, const gchar *account );

G_END_DECLS

#endif /* __OFA_PDF_RECONCIL_H__ */
