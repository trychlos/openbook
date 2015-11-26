/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-idbconnect.h"

/* some data attached to each IDBConnect instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIFileMeta   *meta;
	ofaIFilePeriod *period;
	gchar          *account;
}
	sIDBConnect;

#define IDBCONNECT_LAST_VERSION         1
#define IDBCONNECT_DATA                 "idbconnect-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIDBConnectInterface *klass );
static void         interface_base_finalize( ofaIDBConnectInterface *klass );
static sIDBConnect *get_idbconnect_data( const ofaIDBConnect *connect );
static void         on_dbconnect_finalized( sIDBConnect *data, GObject *finalized_dbconnect );

/**
 * ofa_idbconnect_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbconnect_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbconnect_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbconnect_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBConnectInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBConnect", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBConnectInterface *klass )
{
	static const gchar *thisfn = "ofa_idbconnect_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBConnectInterface *klass )
{
	static const gchar *thisfn = "ofa_idbconnect_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbconnect_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbconnect_get_interface_last_version( void )
{
	return( IDBCONNECT_LAST_VERSION );
}

/**
 * ofa_idbconnect_get_interface_version:
 * @instance: this #ofaIDBConnect instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_idbconnect_get_interface_version( const ofaIDBConnect *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IDBCONNECT( instance ), 0 );

	if( OFA_IDBCONNECT_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IDBCONNECT_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	return( 1 );
}

/**
 * ofa_idbconnect_get_meta:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: a new reference to the target #ofaIFileMeta dossier, which
 * should be g_object_unref() by the caller.
 */
ofaIFileMeta *
ofa_idbconnect_get_meta( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_object_ref( data->meta ));
}

/**
 * ofa_idbconnect_set_meta:
 * @connect: this #ofaIDBConnect instance.
 * @meta: the #ofaIFileMeta object which manages the dossier.
 *
 * The interface takes a reference on the @meta object, to make
 * sure it stays available. This reference will be automatically
 * released on @connect finalization. It is so important to not call
 * this method more than once.
 */
void
ofa_idbconnect_set_meta( ofaIDBConnect *connect, const ofaIFileMeta *meta )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_clear_object( &data->meta );
	data->meta = g_object_ref(( gpointer ) meta );
}

/**
 * ofa_idbconnect_get_period:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: a new reference to the target #ofaIFilePeriod dossier, which
 * should be g_object_unref() by the caller.
 */
ofaIFilePeriod *
ofa_idbconnect_get_period( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_object_ref( data->period ));
}

/**
 * ofa_idbconnect_set_period:
 * @connect: this #ofaIDBConnect instance.
 * @period: the #ofaIFilePeriod object which manages the exercice.
 *
 * The interface takes a reference on the @period object, to make
 * sure it stays available. This reference will be automatically
 * released on @connect finalization. It is so important to not call
 * this method more than once.
 */
void
ofa_idbconnect_set_period( ofaIDBConnect *connect, const ofaIFilePeriod *period )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_clear_object( &data->period );
	data->period = g_object_ref(( gpointer ) period );
}

/**
 * ofa_idbconnect_get_account:
 * @connect: this #ofaIDBConnect instance.
 *
 * Returns: the account used to open the connection, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_idbconnect_get_account( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	data = get_idbconnect_data( connect );
	return( g_strdup( data->account ));
}

/**
 * ofa_idbconnect_set_account:
 * @connect: this #ofaIDBConnect instance.
 * @account: the account which holds the connection.
 */
void
ofa_idbconnect_set_account( ofaIDBConnect *connect, const gchar *account )
{
	sIDBConnect *data;

	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	data = get_idbconnect_data( connect );
	g_free( data->account );
	data->account = g_strdup( account );
}

static sIDBConnect *
get_idbconnect_data( const ofaIDBConnect *connect )
{
	sIDBConnect *data;

	data = ( sIDBConnect * ) g_object_get_data( G_OBJECT( connect ), IDBCONNECT_DATA );

	if( !data ){
		data = g_new0( sIDBConnect, 1 );
		g_object_set_data( G_OBJECT( connect ), IDBCONNECT_DATA, data );
		g_object_weak_ref( G_OBJECT( connect ), ( GWeakNotify ) on_dbconnect_finalized, data );
	}

	return( data );
}

static void
on_dbconnect_finalized( sIDBConnect *data, GObject *finalized_connect )
{
	static const gchar *thisfn = "ofa_idbconnect_on_dbconnect_finalized";

	g_debug( "%s: data=%p, finalized_connect=%p",
			thisfn, ( void * ) data, ( void * ) finalized_connect );

	g_clear_object( &data->meta );
	g_clear_object( &data->period );
	g_free( data->account );
	g_free( data );
}
