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

#ifndef __OFA_BOURSO_PDF_IMPORTER_H__
#define __OFA_BOURSO_PDF_IMPORTER_H__

/**
 * SECTION: ofa_bourso_pdf_importer
 * @short_description: #ofaBoursoPdfImporter class definition.
 * @include: import-bourso-pdf/ofa-importer.h
 *
 * BOURSO Import Bank Account Transaction (BAT) files in PDF format.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_BOURSO_PDF_IMPORTER                ( ofa_bourso_pdf_importer_get_type())
#define OFA_BOURSO_PDF_IMPORTER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BOURSO_PDF_IMPORTER, ofaBoursoPdfImporter ))
#define OFA_BOURSO_PDF_IMPORTER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BOURSO_PDF_IMPORTER, ofaBoursoPdfImporterClass ))
#define OFA_IS_BOURSO_PDF_IMPORTER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BOURSO_PDF_IMPORTER ))
#define OFA_IS_BOURSO_PDF_IMPORTER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BOURSO_PDF_IMPORTER ))
#define OFA_BOURSO_PDF_IMPORTER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BOURSO_PDF_IMPORTER, ofaBoursoPdfImporterClass ))

typedef struct _ofaBoursoPdfImporterPrivate         ofaBoursoPdfImporterPrivate;

typedef struct {
	/*< public members >*/
	GObject                      parent;

	/*< private members >*/
	ofaBoursoPdfImporterPrivate *priv;
}
	ofaBoursoPdfImporter;

typedef struct {
	/*< public members >*/
	GObjectClass                 parent;
}
	ofaBoursoPdfImporterClass;

GType ofa_bourso_pdf_importer_get_type     ( void );

void  ofa_bourso_pdf_importer_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OFA_BOURSO_PDF_IMPORTER_H__ */
