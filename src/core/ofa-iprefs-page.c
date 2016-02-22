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

#include "api/ofa-iprefs-page.h"
#include "api/ofa-iprefs-provider.h"

/* some data attached to each IDBMeta instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIPrefsProvider *prov_instance;
}
	sIPrefsPage;

#define IPREFS_PAGE_LAST_VERSION        1
#define IPREFS_PAGE_DATA                "idbmeta-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIPrefsPageInterface *klass );
static void         interface_base_finalize( ofaIPrefsPageInterface *klass );
static sIPrefsPage *get_iprefs_page_data( const ofaIPrefsPage *page );
static void         on_page_finalized( sIPrefsPage *data, GObject *finalized_page );

/**
 * ofa_iprefs_page_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iprefs_page_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iprefs_page_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iprefs_page_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIPrefsPageInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIPrefsPage", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIPrefsPageInterface *klass )
{
	static const gchar *thisfn = "ofa_iprefs_page_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/*klass->get_interface_version = ipreferences_get_interface_version;*/
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIPrefsPageInterface *klass )
{
	static const gchar *thisfn = "ofa_iprefs_page_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iprefs_page_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iprefs_page_get_interface_last_version( void )
{
	return( IPREFS_PAGE_LAST_VERSION );
}

/**
 * ofa_iprefs_page_get_interface_version:
 * @instance: this #ofaIPrefsPage instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_iprefs_page_get_interface_version( const ofaIPrefsPage *instance )
{
	static const gchar *thisfn = "ofa_iprefs_page_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PAGE( instance ), 0 );

	if( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIPrefsPage instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_iprefs_page_get_provider:
 * @instance: this #ofaIPrefsPage instance.
 *
 * Returns: a new reference to the provider instance which should be
 * g_object_unref() by the caller.
 */
ofaIPrefsProvider *
ofa_iprefs_page_get_provider( const ofaIPrefsPage *instance )
{
	sIPrefsPage *data;

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PAGE( instance ), NULL );

	data = get_iprefs_page_data( instance );
	return( g_object_ref( data->prov_instance ));
}

/**
 * ofa_iprefs_page_set_provider:
 * @instance: this #ofaIPrefsPage instance.
 * @provider: the #ofaIPrefsProvider which manages the page.
 *
 * The interface takes a reference on the @instance object, to make
 * sure it stays available. This reference will be automatically
 * released on @page finalization.
 */
void
ofa_iprefs_page_set_provider( ofaIPrefsPage *instance, ofaIPrefsProvider *provider )
{
	sIPrefsPage *data;

	g_return_if_fail( instance && OFA_IS_IPREFS_PAGE( instance ));
	g_return_if_fail( provider && OFA_IS_IPREFS_PROVIDER( provider ));

	data = get_iprefs_page_data( instance );
	g_clear_object( &data->prov_instance );
	data->prov_instance = g_object_ref( provider );
}

/**
 * ofa_iprefs_page_init:
 * @importer: this #ofaIPrefsPage instance.
 * @settings: the #myISettings instance which manages the user
 *  preferences settings file.
 * @label: [allow-none][out]: the label to be set as the notebook page
 *  tab title.
 * @msgerr: [allow-none][out]: an error message to be returned.
 *
 * Initialize the page.
 *
 * Returns: %TRUE if the page has been successfully initialized,
 * %FALSE else.
 */
gboolean
ofa_iprefs_page_init( const ofaIPrefsPage *instance, myISettings *settings, gchar **label, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_iprefs_page_init";

	g_debug( "%s: instance=%p (%s), settings=%p, label=%p, msgerr=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) settings, ( void * ) label, ( void * ) msgerr );

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PAGE( instance ), FALSE );
	g_return_val_if_fail( settings && MY_IS_ISETTINGS( settings ), FALSE );

	if( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->init ){
		return( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->init( instance, settings, label, msgerr ));
	}

	g_info( "%s: ofaIPrefsPage instance %p does not provide 'init()' method",
			thisfn, ( void * ) instance );
	return( TRUE );
}

/**
 * ofa_iprefs_page_get_valid:
 * @instance: this #ofaIPrefsPage instance.
 * @msgerr: [allow-none][out]: an error message to be returned.
 *
 * Returns: %TRUE if the page is valid, %FALSE else.
 */
gboolean
ofa_iprefs_page_get_valid( const ofaIPrefsPage *instance, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_iprefs_page_get_valild";

	g_debug( "%s: instance=%p (%s), msgerr=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) msgerr );

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PAGE( instance ), FALSE );

	if( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->get_valid ){
		return( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->get_valid( instance, msgerr ));
	}

	g_info( "%s: ofaIPrefsPage instance %p does not provide 'get_valid()' method",
			thisfn, ( void * ) instance );
	return( TRUE );
}

/**
 * ofa_iprefs_page_apply:
 * @instance: this #ofaIPrefsPage instance.
 * @msgerr: [allow-none][out]: an error message to be returned.
 *
 * Saves the user preferences.
 *
 * Returns: %TRUE if the updates have been successfully applied,
 * %FALSE else.
 */
gboolean
ofa_iprefs_page_apply( const ofaIPrefsPage *instance, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_iprefs_page_apply";

	g_debug( "%s: instance=%p (%s), msgerr=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) msgerr );

	g_return_val_if_fail( instance && OFA_IS_IPREFS_PAGE( instance ), FALSE );

	if( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->apply ){
		return( OFA_IPREFS_PAGE_GET_INTERFACE( instance )->apply( instance, msgerr ));
	}

	g_info( "%s: ofaIPrefsPage instance %p does not provide 'apply()' method",
			thisfn, ( void * ) instance );
	return( TRUE );
}

static sIPrefsPage *
get_iprefs_page_data( const ofaIPrefsPage *page )
{
	sIPrefsPage *data;

	data = ( sIPrefsPage * ) g_object_get_data( G_OBJECT( page ), IPREFS_PAGE_DATA );

	if( !data ){
		data = g_new0( sIPrefsPage, 1 );
		g_object_set_data( G_OBJECT( page ), IPREFS_PAGE_DATA, data );
		g_object_weak_ref( G_OBJECT( page ), ( GWeakNotify ) on_page_finalized, data );
	}

	return( data );
}

static void
on_page_finalized( sIPrefsPage *data, GObject *finalized_page )
{
	static const gchar *thisfn = "ofa_iprefs_page_on_page_finalized";

	g_debug( "%s: data=%p, finalized_page=%p", thisfn, ( void * ) data, ( void * ) finalized_page );

	g_clear_object( &data->prov_instance );
	g_free( data );
}
