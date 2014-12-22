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

#ifndef __OFA_IMPORTER_H__
#define __OFA_IMPORTER_H__

/**
 * SECTION: ofa_importer
 * @short_description: #ofaImporter class definition.
 * @include: ofa-importer.h
 *
 * Import Bank Account Transaction (BAT) files in tabulated text format.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTER                ( ofa_importer_get_type())
#define OFA_IMPORTER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTER, ofaImporter ))
#define OFA_IMPORTER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTER, ofaImporterClass ))
#define OFA_IS_IMPORTER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTER ))
#define OFA_IS_IMPORTER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTER ))
#define OFA_IMPORTER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTER, ofaImporterClass ))

typedef struct _ofaImporterPrivate       ofaImporterPrivate;

typedef struct {
	/*< public members >*/
	GObject             parent;

	/*< private members >*/
	ofaImporterPrivate *priv;
}
	ofaImporter;

typedef struct {
	/*< public members >*/
	GObjectClass        parent;
}
	ofaImporterClass;

GType ofa_importer_get_type     ( void );

void  ofa_importer_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OFA_IMPORTER_H__ */
