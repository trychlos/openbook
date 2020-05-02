/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-igetter.h"
#include "api/ofa-istore.h"
#include "api/ofa-itree-adder.h"
#include "api/ofo-dossier.h"

/* data associated to each implementor object
 */
typedef struct {

	/* initialization
	 */
	ofaIGetter *getter;
}
	sIStore;

#define ISTORE_LAST_VERSION               1

#define ISTORE_DATA                      "ofa-istore-data"

/* signals defined here
 */
enum {
	INSERT = 0,
	NEED_REFILTER,
	N_SIGNALS
};

static gint  st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIStoreInterface *klass );
static void  interface_base_finalize( ofaIStoreInterface *klass );
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

		/**
		 * ofaiStore::ofa-row-inserted:
		 *
		 * The signal is emitted on the store for each row insertion,
		 * either because this row has actually been inserted, or because
		 * the insertion is simulated.
		 *
		 * This may be trapped by frames which want create the treeview
		 * 'on the fly'.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaIStore    *store,
		 * 							GtkTreeIter *iter,
		 * 							gpointer     user_data );
		 */
		st_signals[ INSERT ] = g_signal_new_class_handler(
					"ofa-row-inserted",
					OFA_TYPE_ISTORE,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_POINTER );

		/**
		 * ofaiStore::ofa-istore-need-refilter:
		 *
		 * The signal may be emitted on the store when it wants the
		 * treeview to refilter itself.
		 *
		 * Handler is of type:
		 * 		void user_handler( ofaIStore    *store,
		 * 							gpointer     user_data );
		 */
		st_signals[ NEED_REFILTER ] = g_signal_new_class_handler(
					"ofa-istore-need-refilter",
					OFA_TYPE_ISTORE,
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
 *
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
 * ofa_istore_set_column_types:
 * @store: this #ofaIStore instance.
 * @getter: a #ofaIGetter instance.
 * @columns_count: the initial count of columns.
 * @columns_type: the initial GType's array.
 *
 * Initialize the underlying #GtkListStore / #GtkTreeStore with the
 * specified columns + the columns added by plugins.
 *
 * This method must be called once per instance.
 */
void
ofa_istore_set_column_types( ofaIStore *store, ofaIGetter *getter, guint columns_count, GType *columns_type )
{
	static const gchar *thisfn = "ofa_istore_set_column_types";
	sIStore *sdata;
	GType *final_array;
	guint final_count;

	g_debug( "%s: store=%p, getter=%p, columns_count=%u, columns_type=%p",
			thisfn, ( void * ) store, ( void * ) getter, columns_count, ( void * ) columns_type );

	g_return_if_fail( store && OFA_IS_ISTORE( store ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( store ), ISTORE_DATA );
	if( sdata ){
		sdata->getter = getter;

		final_array = ofa_itree_adder_get_column_types( getter, store, columns_count, columns_type, &final_count );

		if( GTK_IS_LIST_STORE( store )){
			gtk_list_store_set_column_types( GTK_LIST_STORE( store ), final_count, final_array );
		} else {
			gtk_tree_store_set_column_types( GTK_TREE_STORE( store ), final_count, final_array );
		}

		g_free( final_array );
	}
}

/**
 * ofa_istore_set_values:
 * @store: this #ofaIStore instance.
 * @iter: the current #GtkTreeIter.
 * @object: related user data.
 *
 * Let the plugins set its own row data.
 */
void
ofa_istore_set_values( ofaIStore *store, GtkTreeIter *iter, void *object )
{
	sIStore *sdata;

	g_return_if_fail( store && OFA_IS_ISTORE( store ));

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( store ), ISTORE_DATA );
	if( sdata ){
		ofa_itree_adder_set_values( sdata->getter, store, iter, object );
	}
}

/**
 * ofa_istore_sort:
 * @store: this #ofaIStore instance.
 * @model: the model to be sorted (usually a filter model)
 * @a: one of the #GtkTreeIter to be compared.
 * @b: the second #GtkTreeIter to be compared.
 * @column_id: the identifier of the column.
 * @cmp: [out]: the result.
 *
 * Compare the two items.
 *
 * Returns: %TRUE if the @column_id is managed here, %FALSE else.
 */
gboolean
ofa_istore_sort( ofaIStore *store, GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, gint *cmp )
{
	sIStore *sdata;

	g_return_val_if_fail( store && OFA_IS_ISTORE( store ), FALSE );
	g_return_val_if_fail( model && GTK_IS_TREE_MODEL( model ), FALSE );
	g_return_val_if_fail( cmp, FALSE );

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( store ), ISTORE_DATA );
	if( sdata ){
		return( ofa_itree_adder_sort( sdata->getter, store, model, a, b, column_id, cmp ));
	}

	return( FALSE );
}

/**
 * ofa_istore_add_columns:
 * @store: this #ofaIStore instance.
 * @bin: the #ofaTVBin which manages the treeview.
 *
 * Add #GtkTreeViewColumn columns to the @bin treeview.
 */
void
ofa_istore_add_columns( ofaIStore *store, ofaTVBin *bin )
{
	static const gchar *thisfn = "ofa_istore_add_columns";
	sIStore *sdata;

	g_debug( "%s: store=%p, bin=%p", thisfn, ( void * ) store, ( void * ) bin );

	g_return_if_fail( store && OFA_IS_ISTORE( store ));
	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( store ), ISTORE_DATA );
	if( sdata ){
		ofa_itree_adder_add_columns( sdata->getter, store, bin );
	}
}

static void
on_store_finalized( sIStore *sdata, GObject *finalized_store )
{
	static const gchar *thisfn = "ofa_istore_on_store_finalized";

	g_debug( "%s: sdata=%p, finalized_store=%p (%s)",
			thisfn, ( void * ) sdata, ( void * ) finalized_store, G_OBJECT_TYPE_NAME( finalized_store ));

	g_free( sdata );
}
