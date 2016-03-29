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

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"

#define IEXPORTER_LAST_VERSION            1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIExporterInterface *klass );
static void  interface_base_finalize( ofaIExporterInterface *klass );

/**
 * ofa_iexporter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iexporter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iexporter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iexporter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIExporterInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIExporter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIExporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iexporter_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIExporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iexporter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iexporter_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iexporter_get_interface_last_version( void )
{
	return( IEXPORTER_LAST_VERSION );
}

/**
 * ofa_iexporter_get_interface_version:
 * @instance: this #ofaIExporter instance.
 *
 * Returns: the version number of this interface implemented by the
 * application.
 */
guint
ofa_iexporter_get_interface_version( const ofaIExporter *instance )
{
	static const gchar *thisfn = "ofa_iexporter_get_interface_version";

	g_return_val_if_fail( instance && OFA_IS_IEXPORTER( instance ), 0 );

	if( OFA_IEXPORTER_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IEXPORTER_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIExporter's %s implementation does not provide 'get_interface_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 1 );
}

/**
 * ofa_iexporter_get_exportables:
 * @instance: this #ofaIExporter instance.
 *
 * Returns: a list of newly allocated #ofaIExportable objects.
 *
 * This method is directly meants for the plugins, so that they are able
 * to advertize their ofaIExportable objects.
 *
 * The interface will take ownership of the returned list, and will
 * #g_list_free_full( list, ( GDestroyNotify ) g_object_unref ) after
 * use.
 */
GList *
ofa_iexporter_get_exportables( ofaIExporter *instance )
{
	static const gchar *thisfn = "ofa_iexporter_get_exportables";

	g_return_val_if_fail( instance && OFA_IS_IEXPORTER( instance ), NULL );

	if( OFA_IEXPORTER_GET_INTERFACE( instance )->get_exportables ){
		return( OFA_IEXPORTER_GET_INTERFACE( instance )->get_exportables( instance ));
	}

	g_info( "%s: ofaIExporter's %s implementation does not provide 'get_exportables()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_iexporter_get_exportables_all:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a list of #ofaIExportable 'fake' objects, concatenating
 * both those from the core library, and those advertized by the
 * plugins.
 *
 * It is expected that the caller takes ownership of the returned list,
 * and #g_list_free_full( list, ( GDestroyNotify ) g_object_unref )
 * after use.
 */
GList *
ofa_iexporter_get_exportables_all( ofaHub *hub )
{
	GList *exportables, *exporters;
	GList *it, *it_objects;
	ofaExtenderCollection *extenders;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	/* requests the ofaIExportables from core library
	 * from ofaHub as ofaIExporter */
	exportables = ofa_iexporter_get_exportables( OFA_IEXPORTER( hub ));

	/* requests the ofaIExportables from ofaIExporter modules */
	extenders = ofa_hub_get_extender_collection( hub );
	exporters = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IEXPORTER );

	for( it=exporters ; it ; it=it->next ){
		it_objects = ofa_iexporter_get_exportables( OFA_IEXPORTER( it->data ));
		exportables = g_list_concat( exportables, it_objects );
	}

	ofa_extender_collection_free_types( exporters );

	return( exportables );
}
