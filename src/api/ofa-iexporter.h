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

#ifndef __OPENBOOK_API_OFA_IEXPORTER_H__
#define __OPENBOOK_API_OFA_IEXPORTER_H__

/**
 * SECTION: iexporter
 * @title: ofaIExporter
 * @short_description: The Export Interface
 * @include: openbook/ofa-iexporter.h
 *
 * The #ofaIExporter interface exports items to the outside world.
 * It should be implemented by the code which exports the item
 * (cf. eg. #ofaExportAssistant), or by any code which interfaces to
 * an #ofaIExportable item.
 *
 * The #ofaIEXporter interface expects that exportable items all
 * implement the #ofaIExportable interface.
 */

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXPORTER                      ( ofa_iexporter_get_type())
#define OFA_IEXPORTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXPORTER, ofaIExporter ))
#define OFA_IS_IEXPORTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXPORTER ))
#define OFA_IEXPORTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXPORTER, ofaIExporterInterface ))

typedef struct _ofaIExporter                    ofaIExporter;

/**
 * ofaIExporterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @export:                [should] exports a dataset.
 *
 * This defines the interface that an #ofaIExporter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIExporter provider.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIExporter *instance );

	/**
	 * get_exportables:
	 * @instance: the #ofaIExporter provider.
	 *
	 * Return: a list of newly allocated #ofaIExportable objects.
	 */
	GList *  ( *get_exportables )      ( ofaIExporter *instance );
}
	ofaIExporterInterface;

GType    ofa_iexporter_get_type                  ( void );

guint    ofa_iexporter_get_interface_last_version( void );

guint    ofa_iexporter_get_interface_version     ( const ofaIExporter *instance );

GList   *ofa_iexporter_get_exportables           ( ofaIExporter *instance );

GList   *ofa_iexporter_get_exportables_all       ( ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXPORTER_H__ */
