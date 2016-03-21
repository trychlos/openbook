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

#ifndef __OPENBOOK_API_OFA_IIMPORTER_H__
#define __OPENBOOK_API_OFA_IIMPORTER_H__

/**
 * SECTION: iimporter
 * @title: ofaIImporter
 * @short_description: The Import Interface
 * @include: openbook/ofa-iimporter.h
 *
 * The #ofaIImporter interface is called by the application in order to
 * try to import objects from an external stream.
 *
 * The #ofaIImporter lets the importable object communicate with the
 * importer code. Two signals are defined:
 * - "progress" to visually render the progress of the import (resp.
 *   the insertion in the DBMS)
 * - "message" to display a standard, warning or error message during
 *   the import (resp. the DBMS insertion).
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IIMPORTER                      ( ofa_iimporter_get_type())
#define OFA_IIMPORTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IIMPORTER, ofaIImporter ))
#define OFA_IS_IIMPORTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IIMPORTER ))
#define OFA_IIMPORTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IIMPORTER, ofaIImporterInterface ))

typedef struct _ofaIImporter                    ofaIImporter;
typedef struct _ofaIImporterParms               ofaIImporterParms;

/**
 * ofaIImporterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @import_from_uri:       [should] imports a file.
 *
 * This defines the interface that an #ofaIImporter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIImporter provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIImporter interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIImporter *instance );
}
	ofaIImporterInterface;

GType ofa_iimporter_get_type       ( void );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IIMPORTER_H__ */
