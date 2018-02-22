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

#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irecover.h"

#define IRECOVER_LAST_VERSION             1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIRecoverInterface *klass );
static void  interface_base_finalize( ofaIRecoverInterface *klass );
static void  recover_file_free( ofsRecoverFile *sfile );

/**
 * ofa_irecover_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_irecover_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_irecover_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_irecover_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIRecoverInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIRecover", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIRecoverInterface *klass )
{
	static const gchar *thisfn = "ofa_irecover_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIRecoverInterface *klass )
{
	static const gchar *thisfn = "ofa_irecover_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_irecover_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_irecover_get_interface_last_version( void )
{
	return( IRECOVER_LAST_VERSION );
}

/**
 * ofa_irecover_get_interface_version:
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
ofa_irecover_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IRECOVER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIRecoverInterface * ) iface )->get_interface_version ){
		version = (( ofaIRecoverInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIRecover::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_irecover_import_uris:
 * @instance: this #ofaIRecover instance.
 * @getter: a #ofaIGetter instance.
 * @uris: a #GList of #ofsRecoverFile structures to be imported.
 * @format: the input stream format.
 * @connect: the target connection.
 * @msg_cb: a message callback.
 * @msg_data: user data to be passed to @msg_cb.
 *
 * Import the specified @uris into the @connect target.
 *
 * Returns: %TRUE if the recovery was successfull, %FALSE else.
 */
gboolean
ofa_irecover_import_uris( ofaIRecover *instance, ofaIGetter *getter, GList *uris, ofaStreamFormat *format, ofaIDBConnect *connect, ofaMsgCb msg_cb, void *msg_data )
{
	static const gchar *thisfn = "ofa_irecover_import_uris";

	g_debug( "%s: instance=%p, getter=%p, uris=%p, format=%p, connect=%p, msg_cb=%p, msg_data=%p",
			thisfn, ( void * ) instance, ( void * ) getter,
			( void * ) uris, ( void * ) format, ( void * ) connect, ( void * ) msg_cb, msg_data );

	g_return_val_if_fail( instance && OFA_IS_IRECOVER( instance ), FALSE );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( format && OFA_IS_STREAM_FORMAT( format ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IRECOVER_GET_INTERFACE( instance )->import_uris ){
		return( OFA_IRECOVER_GET_INTERFACE( instance )->import_uris( instance, getter, uris, format, connect, msg_cb, msg_data ));
	}

	g_info( "%s: ofaIRecover's %s implementation does not provide 'import_uris()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * ofa_irecover_add_file:
 * @uris: the current list of URIs.
 * @nature: the nature of the URI to be added.
 * @uri: the URI.
 *
 * Returns: the new list of URIs.
 */
GList *
ofa_irecover_add_file( GList *uris, guint nature, const gchar *uri )
{
	ofsRecoverFile *sfile;
	GList *head;

	sfile = g_new0( ofsRecoverFile, 1 );
	sfile->nature = nature;
	sfile->uri = g_strdup( uri );

	head = g_list_append( uris, sfile );

	return( head );
}

/**
 * ofa_irecover_free_files:
 * @uris: a pointer to the list of URIs as returned by #ofa_irecover_add_file.
 *
 * Free the ressources associated to the @uris list.
 * Set the @uris pointer to %NULL.
 */
void
ofa_irecover_free_files( GList **uris )
{
	g_list_free_full( *uris, ( GDestroyNotify ) recover_file_free );
	*uris = NULL;
}

static void
recover_file_free( ofsRecoverFile *sfile )
{
	g_free( sfile->uri );
	g_free( sfile );
}
