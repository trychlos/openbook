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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <api/ofa-iimporter.h>

static guint st_initializations = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIImporterInterface *klass );
static void  interface_base_finalize( ofaIImporterInterface *klass );
static guint iimporter_get_interface_version( const ofaIImporter *instance );

/**
 * ofa_iimporter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iimporter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iimporter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iimporter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIImporterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIImporter", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIImporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iimporter_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->get_interface_version = iimporter_get_interface_version;
		klass->import_from_uri = NULL;
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIImporterInterface *klass )
{
	static const gchar *thisfn = "ofa_iimporter_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

static guint
iimporter_get_interface_version( const ofaIImporter *instance )
{
	return( 1 );
}

/**
 * ofa_iimporter_import_from_uri:
 * @importer: this #ofaIImporter instance.
 * @parms: a #ofaIImporterParms structure.
 *
 * Tries to import data from the URI specified in @parms, returning
 * the result in <structfield>@parms->imported</structfield>.
 *
 * Returns: the return code of the operation.
 */

guint
ofa_iimporter_import_from_uri( const ofaIImporter *importer, ofaIImporterParms *parms )
{
	static const gchar *thisfn = "ofa_iimporter_import_from_uri";
	guint code;

	g_return_val_if_fail( OFA_IS_IIMPORTER( importer ), IMPORTER_CODE_PROGRAM_ERROR );
	g_return_val_if_fail( parms, IMPORTER_CODE_PROGRAM_ERROR );

	code = IMPORTER_CODE_NOT_WILLING_TO;

	g_debug( "%s: importer=%p (%s), parms=%p", thisfn,
			( void * ) importer, G_OBJECT_TYPE_NAME( importer), ( void * ) parms );

	if( OFA_IIMPORTER_GET_INTERFACE( importer )->import_from_uri ){
		code = OFA_IIMPORTER_GET_INTERFACE( importer )->import_from_uri( importer, parms );
	}

	return( code );
}

static void
free_output_sbatv1( ofaIImporterSBatv1 *bat )
{
	g_free( bat->currency );
	g_free( bat->label );
	g_free( bat->ref );
	g_free( bat );
}

/**
 * ofa_iimporter_free_output:
 *
 * Free the structure which handles a bank account transaction
 */
void
ofa_iimporter_free_output( ofaIImporterParms *parms )
{
	static const gchar *thisfn = "ofa_iimporter_free_output";

	switch( parms->type ){

		case IMPORTER_TYPE_BAT1:
			g_free( parms->batv1.format );
			g_free( parms->batv1.rib );
			g_free( parms->batv1.currency );
			g_list_free_full( parms->batv1.results, ( GDestroyNotify ) free_output_sbatv1 );
			memset( &parms->batv1, '\0', sizeof( ofaIImporterSBatv1 ));
			g_date_clear( &parms->batv1.begin, 1 );
			g_date_clear( &parms->batv1.end, 1 );
			break;

		default:
			g_debug( "%s: TO BE WRITTEN", thisfn );
	}
}
