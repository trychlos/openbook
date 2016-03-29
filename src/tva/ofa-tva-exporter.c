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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-iexportable.h"

#include "ofa-tva.h"
#include "ofa-tva-exporter.h"

static guint  iexporter_get_interface_version( const ofaIExporter *instance );
static GList *iexporter_get_exportables( ofaIExporter *instance );

/*
 * #ofaIExporter interface setup
 */
void
ofa_tva_exporter_iface_init( ofaIExporterInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_exporter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexporter_get_interface_version;
	iface->get_exportables = iexporter_get_exportables;
}

/*
 * the version of the #ofaIExporter interface implemented by the module
 */
static guint
iexporter_get_interface_version( const ofaIExporter *instance )
{
	return( 1 );
}

static GList *
iexporter_get_exportables( ofaIExporter *instance )
{
	GList *vat_objects, *exportables, *it;

	vat_objects = ofa_tva_get_registered_types( OFA_TVA( instance ));
	exportables = NULL;

	for( it=vat_objects ; it ; it=it->next ){
		if( OFA_IS_IEXPORTABLE( it->data )){
			exportables = g_list_prepend( exportables, g_object_ref( it->data ));
		}
	}

	return( exportables );
}
