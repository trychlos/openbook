/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <gtk/gtk.h>
#include <string.h>

#include "api/ofa-hub.h"
#include "api/ofa-istore.h"
#include "api/ofa-itree-adder.h"
#include "api/ofo-dossier.h"

/* data associated to each implementor object
 */
typedef struct {
	guint  cols_count;
	GType *cols_type;
}
	sIStore;

#define ISTORE_LAST_VERSION               1

#define ISTORE_DATA                      "ofa-istore-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIStoreInterface *klass );
static void  interface_base_finalize( ofaIStoreInterface *klass );
static guint on_column_type_added( ofaIStore *store, GType type, sIStore *sdata );
static void  on_store_finalized( sIStore *sdata, GObject *finalized_store );

/**
 * ofa_istore_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_istore_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_istore_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_istore_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIStoreInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIStore", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_istore_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_istore_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_istore_get_interface_last_version:
 * @instance: this #ofaIStore instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_istore_get_interface_last_version( const ofaIStore *instance )
{
	return( ISTORE_LAST_VERSION );
}

/**
 * ofa_istore_get_interface_version:
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
ofa_istore_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISTORE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIStoreInterface * ) iface )->get_interface_version ){
		version = (( ofaIStoreInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIStore::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_istore_init:
 * @istore: this #ofaIStore instance.
 *
 * Initialize a structure attached to the implementor #GObject.
 * This should be done as soon as possible in order to let the
 * implementation take benefit of the interface.
 */
void
ofa_istore_init( ofaIStore *istore )
{
	static const gchar *thisfn = "ofa_istore_init";
	sIStore *sdata;

	g_debug( "%s: istore=%p", thisfn, ( void * ) istore );

	g_return_if_fail( istore && G_IS_OBJECT( istore ) && OFA_IS_ISTORE( istore ));

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( istore ), ISTORE_DATA );
	if( sdata ){
		g_warning( "%s: already initialized ofaIStore=%p", thisfn, ( void * ) sdata );
	}

	sdata = g_new0( sIStore, 1 );
	sdata->cols_count = 0;
	sdata->cols_type = NULL;
	g_object_set_data( G_OBJECT( istore ), ISTORE_DATA, sdata );
	g_object_weak_ref( G_OBJECT( istore ), ( GWeakNotify ) on_store_finalized, sdata );
}

/**
 * ofa_istore_load_dataset:
 * @istore: this #ofaIStore instance.
 *
 * Asks the implementation to load its datas from DBMS.
 */
void
ofa_istore_load_dataset( ofaIStore *istore )
{
	static const gchar *thisfn = "ofa_istore_load_dataset";

	g_debug( "%s: istore=%p", thisfn, ( void * ) istore );

	g_return_if_fail( istore && OFA_IS_ISTORE( istore ));

	if( OFA_ISTORE_GET_INTERFACE( istore )->load_dataset ){
		OFA_ISTORE_GET_INTERFACE( istore )->load_dataset( istore );
		return;
	}

	g_info( "%s: ofaIStore's %s implementation does not provide 'load_dataset()' method",
			thisfn, G_OBJECT_TYPE_NAME( istore ));
}

/**
 * ofa_istore_set_columns_type:
 * @store: this #ofaIStore instance.
 * @hub: the #ofaHub object of the application.
 * @column_object: the column number of the stored object.
 * @columns_count: the initial count of columns.
 * @columns_type: the initial GType's array.
 *
 * Initialize the underlying #GtkListStore / #GtkTreeStore with the
 * specified columns + the columns added by plugins.
 */
void
ofa_istore_set_columns_type( ofaIStore *store, ofaHub *hub, guint column_object, guint columns_count, GType *columns_type )
{
	static const gchar *thisfn = "ofa_istore_set_columns_type";
	sIStore *sdata;

	g_debug( "%s: store=%p, hub=%p, columnn_object=%u, columns_count=%u, columns_type=%p",
			thisfn, ( void * ) store, ( void * ) hub, column_object, columns_count, ( void * ) columns_type );

	g_return_if_fail( store && OFA_IS_ISTORE( store ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( store ), ISTORE_DATA );

	sdata->cols_count = columns_count;
	sdata->cols_type = g_new0( GType, 1+columns_count );
	memcpy( sdata->cols_type, columns_type, sizeof( GType ) * columns_count );

	ofa_itree_adder_add_types( hub, store, column_object, ( TreeAdderTypeCb ) on_column_type_added, sdata );

	if( GTK_IS_LIST_STORE( store )){
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), sdata->cols_count, sdata->cols_type );
	} else {
		gtk_tree_store_set_column_types(
				GTK_TREE_STORE( store ), sdata->cols_count, sdata->cols_type );
	}
}

/*
 * starting with N columns, numbered from 0 to N-1
 * priv->types is an NULL-terminated array of N GType's, so is N+1 size
 * adding a GType means
 * N -> N+1
 * last numbered
 */
static guint
on_column_type_added( ofaIStore *store, GType type, sIStore *sdata )
{
	GType *types;
	guint i, count;

	if( 0 ){
		g_debug( "before" );
		for( i=0 ; i<sdata->cols_count ; ++i ){
			g_debug( "i=%u, type=%ld", i, sdata->cols_type[i] );
		}
	}

	sdata->cols_count += 1;									/* new columns count */
	count = sdata->cols_count;
	types = g_new0( GType, count+1 );
	memcpy( types, sdata->cols_type, sizeof( GType ) * ( count-1 ));
	types[count-1] = type;
	g_free( sdata->cols_type );
	sdata->cols_type = types;

	if( 0 ){
		g_debug( "after" );
		for( i=0 ; i<sdata->cols_count ; ++i ){
			g_debug( "i=%u, type=%ld", i, sdata->cols_type[i] );
		}
	}

	return( sdata->cols_count-1 );
}

static void
on_store_finalized( sIStore *sdata, GObject *finalized_store )
{
	static const gchar *thisfn = "ofa_istore_on_store_finalized";

	g_debug( "%s: sdata=%p, finalized_store=%p (%s)",
			thisfn, ( void * ) sdata, ( void * ) finalized_store, G_OBJECT_TYPE_NAME( finalized_store ));

	g_free( sdata );
}
