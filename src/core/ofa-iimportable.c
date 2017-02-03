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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"

#define IIMPORTABLE_LAST_VERSION        1

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIImportableInterface *klass );
static void          interface_base_finalize( ofaIImportableInterface *klass );

/**
 * ofa_iimportable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iimportable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iimportable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iimportable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIImportableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIImportable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIImportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iimportable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;

	if( st_initializations == 1 ){

		/* declare here the default implementations */
	}
}

static void
interface_base_finalize( ofaIImportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iimportable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iimportable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iimportable_get_interface_last_version( void )
{
	return( IIMPORTABLE_LAST_VERSION );
}

/**
 * ofa_iimportable_get_interface_version:
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
ofa_iimportable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IIMPORTABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIImportableInterface * ) iface )->get_interface_version ){
		version = (( ofaIImportableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIImportable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_iimportable_import:
 * @type: the GType of the target class.
 * @importer: the #ofaIImporter instance.
 * @parms: the #ofsImporterParms arguments.
 * @lines: the lines to be imported.
 *
 * Returns: the total count of errors.
 */
guint
ofa_iimportable_import( GType type, ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	gpointer klass, iface;
	guint error_count;
	gchar *msgerr;

	g_return_val_if_fail( importer && OFA_IS_IIMPORTER( importer ), 1 );

	error_count = 0;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IIMPORTABLE );
	g_return_val_if_fail( iface, 1 );

	if((( ofaIImportableInterface * ) iface )->import ){
		error_count = (( ofaIImportableInterface * ) iface )->import( importer, parms, lines );

	} else {
		error_count += 1;
		msgerr = g_strdup_printf(
						_( "%s implementation does not provide 'ofaIImportable::import()' method" ),
						g_type_name( type ));
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, importer, msgerr );
		} else {
			g_info( "%s", msgerr );
		}
		g_free( msgerr );
	}

	g_type_class_unref( klass );

	return( error_count );
}

/**
 * ofa_iimportable_get_label:
 * @instance: this #ofaIImportable instance.
 *
 * Returns: the label to be associated to the @instance's class, as a
 * newly allocated string which should be g_free() by the caller.
 */
gchar *
ofa_iimportable_get_label( const ofaIImportable *importable )
{
	static const gchar *thisfn = "ofa_iimportable_get_label";

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), NULL );

	if( OFA_IIMPORTABLE_GET_INTERFACE( importable )->get_label ){
		return( OFA_IIMPORTABLE_GET_INTERFACE( importable )->get_label( importable ));
	}

	g_info( "%s: ofaIImportable's %s implementation does not provide 'get_label()' method",
			thisfn, G_OBJECT_TYPE_NAME( importable ));
	return( NULL );
}
