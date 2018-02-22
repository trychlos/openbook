/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"

/* data set against the exported object
 */
typedef struct {

	/* initialization
	 */

	/* runtime data
	 */
}
	sIExporter;

#define IEXPORTER_LAST_VERSION          1

static guint st_initializations = 0;	/* interface initialization count */

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
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_iexporter_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IEXPORTER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIExporterInterface * ) iface )->get_interface_version ){
		version = (( ofaIExporterInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIExporter::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iexporter_get_formats:
 * @exporter: this #ofaIExporter instance.
 * @type: the target class to export;
 *  the corresponding class must implement #ofaIExportable interface.
 * @getter: the #ofaIGetter of the application.
 *
 * Returns: %NULL, or a null-terminated array of #ofsIExporterFormat
 * structures.
 */
ofsIExporterFormat *
ofa_iexporter_get_formats( ofaIExporter *exporter, GType type, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_iexporter_get_formats";

	g_return_val_if_fail( exporter && OFA_IS_IEXPORTER( exporter ), NULL );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( OFA_IEXPORTER_GET_INTERFACE( exporter )->get_formats ){
		return( OFA_IEXPORTER_GET_INTERFACE( exporter )->get_formats( exporter, type, getter ));
	}

	g_info( "%s: ofaIExporter's %s implementation does not provide 'get_formats()' method",
			thisfn, G_OBJECT_TYPE_NAME( exporter ));
	return( NULL );
}

/**
 * ofa_iexporter_export:
 * @exporter: this #ofaIExporter instance.
 * @exportable: the #ofaIExportable instance to export.
 * @format_id: the export format.
 *
 * Returns: %TRUE if the export has been successful.
 */
gboolean
ofa_iexporter_export( ofaIExporter *exporter, ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofa_iexporter_export";

	g_return_val_if_fail( exporter && OFA_IS_IEXPORTER( exporter ), FALSE );
	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );

	if( OFA_IEXPORTER_GET_INTERFACE( exporter )->export ){
		return( OFA_IEXPORTER_GET_INTERFACE( exporter )->export( exporter, exportable, format_id ));
	}

	g_info( "%s: ofaIExporter's %s implementation does not provide 'export()' method",
			thisfn, G_OBJECT_TYPE_NAME( exporter ));
	return( FALSE );
}
