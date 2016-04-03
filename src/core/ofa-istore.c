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

#include <gtk/gtk.h>

#include "api/ofa-hub.h"
#include "api/ofa-istore.h"
#include "api/ofo-dossier.h"

/* data associated to each implementor object
 */
typedef struct {

	/* static data
	 * to be set at initialization time
	 */
	ofoDossier *dossier;
	ofaHub     *hub;
}
	sIStore;

#define ISTORE_LAST_VERSION             1
#define ISTORE_DATA                     "ofa-istore-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaIStoreInterface *klass );
static void     interface_base_finalize( ofaIStoreInterface *klass );
static void     on_hub_finalized( ofaIStore *istore, GObject *finalized_hub );
static void     on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaIStore *istore );
static void     simulate_dataset_load_rec( GtkTreeModel *tmodel, GtkTreeIter *parent_iter );

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
ofa_istore_init( ofaIStore *istore, ofaHub *hub )
{
	static const gchar *thisfn = "ofa_istore_init";
	sIStore *sdata;

	g_return_if_fail( G_IS_OBJECT( istore ));
	g_return_if_fail( OFA_IS_ISTORE( istore ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( istore ), ISTORE_DATA );
	if( sdata ){
		g_warning( "%s: already initialized ofaIStore=%p", thisfn, ( void * ) sdata );
	}

	sdata = g_new0( sIStore, 1 );
	g_object_set_data( G_OBJECT( istore ), ISTORE_DATA, sdata );

	g_object_weak_ref( G_OBJECT( hub ), ( GWeakNotify ) on_hub_finalized, istore );

	g_signal_connect( istore, "row-inserted", G_CALLBACK( on_row_inserted ), istore );
}

static void
on_hub_finalized( ofaIStore *istore, GObject *finalized_hub )
{
	static const gchar *thisfn = "ofa_istore_on_hub_finalized";
	sIStore *sdata;

	/* at this time, even the IStore implementation is no more an object
	 * if it takes advantage of ofaISingleKeeper interface */
	g_debug( "%s: istore=%p, finalized_hub=%p",
			thisfn, ( void * ) istore, ( void * ) finalized_hub );

	if( istore && G_IS_OBJECT( istore )){
		sdata = ( sIStore * ) g_object_get_data( G_OBJECT( istore ), ISTORE_DATA );

		g_free( sdata );
		g_object_set_data( G_OBJECT( istore ), ISTORE_DATA, NULL );

		g_object_unref( istore );
	}
}

static void
on_row_inserted( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, ofaIStore *istore )
{
	g_signal_emit_by_name( istore, "ofa-row-inserted", path, iter );
}

/**
 * ofa_istore_simulate_dataset_load:
 */
void
ofa_istore_simulate_dataset_load( const ofaIStore *istore )
{
	static const gchar *thisfn = "ofa_istore_simulate_dataset_load";

	g_debug( "%s: store=%p", thisfn, ( void * ) istore );

	g_return_if_fail( istore && OFA_IS_ISTORE( istore ));

	simulate_dataset_load_rec( GTK_TREE_MODEL( istore ), NULL );
}

/*
 * enter with iter=NULL the first time
 * enter with iter=parent when recursing in sub-trees
 */
static void
simulate_dataset_load_rec( GtkTreeModel *tmodel, GtkTreeIter *parent_iter )
{
	GtkTreeIter iter, child_iter;
	GtkTreePath *path;

	if( gtk_tree_model_iter_children( tmodel, &iter, parent_iter )){
		while( TRUE ){
			path = gtk_tree_model_get_path( tmodel, &iter );
			g_signal_emit_by_name( tmodel, "ofa-row-inserted", path, &iter );
			gtk_tree_path_free( path );

			if( gtk_tree_model_iter_has_child( tmodel, &iter ) &&
					gtk_tree_model_iter_children( tmodel, &child_iter, &iter )){
				simulate_dataset_load_rec( tmodel, &child_iter );
			}

			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}
