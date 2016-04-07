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

#ifndef __OFA_IMPORTER_PDF_LCL_H__
#define __OFA_IMPORTER_PDF_LCL_H__

/**
 * SECTION: ofa_importer_pdf_lcl
 * @short_description: #ofaImporterPdfLcl class definition.
 * @include: importers/ofa-importer-pdf-bourso.h
 *
 * This is a base class to handle import of Boursorama PDF files.
 */

#include "importers/ofa-importer-pdf.h"

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTER_PDF_LCL                ( ofa_importer_pdf_lcl_get_type())
#define OFA_IMPORTER_PDF_LCL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTER_PDF_LCL, ofaImporterPdfLcl ))
#define OFA_IMPORTER_PDF_LCL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTER_PDF_LCL, ofaImporterPdfLclClass ))
#define OFA_IS_IMPORTER_PDF_LCL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTER_PDF_LCL ))
#define OFA_IS_IMPORTER_PDF_LCL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTER_PDF_LCL ))
#define OFA_IMPORTER_PDF_LCL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTER_PDF_LCL, ofaImporterPdfLclClass ))

typedef struct {
	/*< public members >*/
	ofaImporterPdf      parent;
}
	ofaImporterPdfLcl;

typedef struct {
	/*< public members >*/
	ofaImporterPdfClass parent;
}
	ofaImporterPdfLclClass;

GType ofa_importer_pdf_lcl_get_type( void );

G_END_DECLS

#endif /* __OFA_IMPORTER_PDF_LCL_H__ */
