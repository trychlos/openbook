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

#ifndef __OFA_IMPORTER_PDF_H__
#define __OFA_IMPORTER_PDF_H__

/**
 * SECTION: ofa_importer_pdf
 * @short_description: #ofaImporterPdf class definition.
 * @include: importers/ofa-importer-pdf.h
 *
 * This is a base class to handle import of pdf files.
 */

#include <glib-object.h>
#include <poppler.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTER_PDF                ( ofa_importer_pdf_get_type())
#define OFA_IMPORTER_PDF( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTER_PDF, ofaImporterPdf ))
#define OFA_IMPORTER_PDF_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTER_PDF, ofaImporterPdfClass ))
#define OFA_IS_IMPORTER_PDF( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTER_PDF ))
#define OFA_IS_IMPORTER_PDF_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTER_PDF ))
#define OFA_IMPORTER_PDF_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTER_PDF, ofaImporterPdfClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaImporterPdf;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaImporterPdfClass;

/**
 * ofsPdfRC:
 * @page_num: the page number, counted from zero.
 * @count: the count of PopplerRectangle provided for this text.
 * @x1: x coordinate of lower left corner.
 * @y1: y coordinate of lower left corner.
 * @x2: x coordinate of upper right corner.
 * @y2: y coordinate of upper right corner.
 * @text: the text inside of the box.
 *
 * A layout rectangle with its text.
 */
typedef struct {
	guint   page_num;
	guint   count;
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
	gchar  *text;
}
	ofsPdfRC;

GType    ofa_importer_pdf_get_type           ( void );

gboolean ofa_importer_pdf_is_willing_to      ( ofaImporterPdf *instance,
													ofaIGetter *getter,
													const gchar *uri,
													const GList *accepted_contents );

GList   *ofa_importer_pdf_get_layout         ( ofaImporterPdf *instance,
													PopplerDocument *doc,
													guint page_num,
													const gchar *charset );

gdouble  ofa_importer_pdf_get_acceptable_diff( void );

void     ofa_importer_pdf_free_layout        ( GList *layout );

void     ofa_importer_pdf_dump_rc            ( const ofsPdfRC *rc, const gchar *label );

void     ofa_importer_pdf_free_rc            ( ofsPdfRC *rc );

G_END_DECLS

#endif /* __OFA_IMPORTER_PDF_H__ */
