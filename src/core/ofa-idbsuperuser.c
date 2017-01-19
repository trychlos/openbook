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

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"

/* some data attached to each IDBSuperuser instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIDBProvider    *provider;
	ofaIDBDossierMeta *dossier_meta;
}
	sIDBSUser;

#define IDBSUPERUSER_LAST_VERSION              1
#define IDBSUPERUSER_DATA                     "idbsuperuser-data"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };
static guint st_initializations         =   0;	/* interface initialization count */

static GType      register_type( void );
static void       interface_base_init( ofaIDBSuperuserInterface *klass );
static void       interface_base_finalize( ofaIDBSuperuserInterface *klass );
static sIDBSUser *get_instance_data( const ofaIDBSuperuser *editor );
static void       on_instance_finalized( sIDBSUser *data, GObject *finalized_instance );

/**
 * ofa_idbsuperuser_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbsuperuser_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbsuperuser_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbsuperuser_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBSuperuserInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBSuperuser", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBSuperuserInterface *klass )
{
	static const gchar *thisfn = "ofa_idbsuperuser_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIDBSuperuser::ofa-changed:
		 *
		 * This signal is sent on the #ofaIDBSuperuser widget when any of
		 * the content changes.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIDBSuperuser *widget,
		 * 						gpointer       user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-changed",
					OFA_TYPE_IDBSUPERUSER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBSuperuserInterface *klass )
{
	static const gchar *thisfn = "ofa_idbsuperuser_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbsuperuser_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbsuperuser_get_interface_last_version( void )
{
	return( IDBSUPERUSER_LAST_VERSION );
}

/**
 * ofa_idbsuperuser_get_interface_version:
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
ofa_idbsuperuser_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBSUPERUSER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBSuperuserInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBSuperuserInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBSuperuser::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbsuperuser_get_provider:
 * @instance: this #ofaIDBSuperuser instance.
 *
 * Returns: the #ofaIDBProvider instance to which the @instance is
 * attached.
 *
 * The returned reference is owned by the @instance, and should not be
 * released by the caller.
 */
ofaIDBProvider *
ofa_idbsuperuser_get_provider( const ofaIDBSuperuser *instance )
{
	sIDBSUser *data;

	g_return_val_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ), NULL );

	data = get_instance_data( instance );

	return( data->provider );
}

/**
 * ofa_idbsuperuser_set_provider:
 * @instance: this #ofaIDBSuperuser instance.
 * @provider: the #ofaIDBProvider instance which manages this @instance.
 *
 * Set the @provider.
 */
void
ofa_idbsuperuser_set_provider( ofaIDBSuperuser *instance, ofaIDBProvider *provider )
{
	sIDBSUser *data;

	g_return_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ));
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));

	data = get_instance_data( instance );

	data->provider = provider;
}

/**
 * ofa_idbsuperuser_get_dossier_meta:
 * @instance: this #ofaIDBSuperuser instance.
 *
 * Returns: the #ofaIDBDossierMeta attached to this @instance.
 *
 * The returned reference is owned by the @instance, and should not be
 * released by the caller.
 */
ofaIDBDossierMeta *
ofa_idbsuperuser_get_dossier_meta( const ofaIDBSuperuser *instance )
{
	sIDBSUser *data;

	g_return_val_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ), NULL );

	data = get_instance_data( instance );

	return( data->dossier_meta );
}

/**
 * ofa_idbsuperuser_set_dossier_meta:
 * @instance: this #ofaIDBSuperuser instance.
 * @dossier_meta: a #ofaIDBDossierMeta to be attached to @instance.
 *
 * Set the @dossier_meta.
 */
void
ofa_idbsuperuser_set_dossier_meta( ofaIDBSuperuser *instance, ofaIDBDossierMeta *dossier_meta )
{
	static const gchar *thisfn = "ofa_idbsuperuser_set_dossier_meta";
	sIDBSUser *data;

	g_return_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ));
	g_return_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ));

	data = get_instance_data( instance );

	data->dossier_meta = dossier_meta;

	if( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->set_dossier_meta ){
		OFA_IDBSUPERUSER_GET_INTERFACE( instance )->set_dossier_meta( instance, dossier_meta );

	} else {
		g_info( "%s: ofaIDBSuperuser's %s implementation does not provide 'set_dossier_meta()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}
}

/**
 * ofa_idbsuperuser_get_size_group:
 * @instance: this #ofaIDBSuperuser instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup of the specified @column.
 */
GtkSizeGroup *
ofa_idbsuperuser_get_size_group( const ofaIDBSuperuser *instance, guint column )
{
	static const gchar *thisfn = "ofa_idbsuperuser_get_size_group";

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ), NULL );

	if( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->get_size_group ){
		return( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->get_size_group( instance, column ));
	}

	g_info( "%s: ofaIDBSuperuser's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_idbsuperuser_is_valid:
 * @instance: this #ofaIDBSuperuser instance.
 * @message: [allow-none][out]: a message to be set.
 *
 * Returns: %TRUE if the entered connection informations are valid.
 */
gboolean
ofa_idbsuperuser_is_valid( const ofaIDBSuperuser *instance, gchar **message )
{
	static const gchar *thisfn = "ofa_idbsuperuser_is_valid";

	g_debug( "%s: instance=%p, message=%p", thisfn, ( void * ) instance, ( void * ) message );

	g_return_val_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ), FALSE );

	if( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->is_valid ){
		return( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->is_valid( instance, message ));
	}

	g_info( "%s: ofaIDBSuperuser's %s implementation does not provide 'is_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * ofa_idbsuperuser_set_valid:
 * @instance: this #ofaIDBSuperuser instance.
 * @valid: the validity status.
 *
 * Set the validity status.
 */
void
ofa_idbsuperuser_set_valid( ofaIDBSuperuser *instance, gboolean valid )
{
	static const gchar *thisfn = "ofa_idbsuperuser_set_valid";

	g_debug( "%s: instance=%p, valid=%s",
			thisfn, ( void * ) instance, valid ? "True":"False" );

	g_return_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ));

	if( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->set_valid ){
		OFA_IDBSUPERUSER_GET_INTERFACE( instance )->set_valid( instance, valid );
		return;
	}

	g_info( "%s: ofaIDBSuperuser's %s implementation does not provide 'set_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * ofa_idbsuperuser_set_credentials_from_connect:
 * @instance: this #ofaIDBSuperuser instance.
 * @connect: an #ofaIDBConnect.
 *
 * Set the credentials from @connect.
 */
void
ofa_idbsuperuser_set_credentials_from_connect( ofaIDBSuperuser *instance, ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_idbsuperuser_set_credentials_from_connect";

	g_debug( "%s: instance=%p, connect=%p",
			thisfn, ( void * ) instance, ( void * ) connect );

	g_return_if_fail( instance && OFA_IS_IDBSUPERUSER( instance ));
	g_return_if_fail( connect && OFA_IS_IDBCONNECT( connect ));

	if( OFA_IDBSUPERUSER_GET_INTERFACE( instance )->set_credentials_from_connect ){
		OFA_IDBSUPERUSER_GET_INTERFACE( instance )->set_credentials_from_connect( instance, connect );
		return;
	}

	g_info( "%s: ofaIDBSuperuser's %s implementation does not provide 'set_credentials_from_connect()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

static sIDBSUser *
get_instance_data( const ofaIDBSuperuser *editor )
{
	sIDBSUser *data;

	data = ( sIDBSUser * ) g_object_get_data( G_OBJECT( editor ), IDBSUPERUSER_DATA );

	if( !data ){
		data = g_new0( sIDBSUser, 1 );
		g_object_set_data( G_OBJECT( editor ), IDBSUPERUSER_DATA, data );
		g_object_weak_ref( G_OBJECT( editor ), ( GWeakNotify ) on_instance_finalized, data );
	}

	return( data );
}

static void
on_instance_finalized( sIDBSUser *data, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_idbsuperuser_on_instance_finalized";

	g_debug( "%s: data=%p, finalized_instance=%p", thisfn, ( void * ) data, ( void * ) finalized_instance );

	g_free( data );
}
