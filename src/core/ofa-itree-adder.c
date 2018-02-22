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

#include <string.h>

#include "api/ofa-extender-collection.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itree-adder.h"

#define ITREE_ADDER_LAST_VERSION          1

static guint st_initializations         = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaITreeAdderInterface *klass );
static void     interface_base_finalize( ofaITreeAdderInterface *klass );
static GType   *add_column_types( guint orig_cols_count, GType *orig_col_types, guint add_cols, GType *add_types );
static GType   *itree_adder_get_column_types( ofaITreeAdder *instance, ofaIStore *store, guint cols_count, guint *add_cols );
static void     itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaIGetter *getter, GtkTreeIter *iter, void *object );
static gboolean itree_adder_sort( ofaITreeAdder *instance, ofaIStore *store, ofaIGetter *getter, GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, gint *cmp );
static void     itree_adder_add_columns( ofaITreeAdder *instance, ofaIStore *store, ofaTVBin *bin );

/**
 * ofa_itree_adder_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itree_adder_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itree_adder_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itree_adder_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaITreeAdderInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITreeAdder", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaITreeAdderInterface *klass )
{
	static const gchar *thisfn = "ofa_itree_adder_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaITreeAdderInterface *klass )
{
	static const gchar *thisfn = "ofa_itree_adder_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_itree_adder_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itree_adder_get_interface_last_version( void )
{
	return( ITREE_ADDER_LAST_VERSION );
}

/**
 * ofa_itree_adder_get_interface_version:
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
ofa_itree_adder_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ITREE_ADDER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaITreeAdderInterface * ) iface )->get_interface_version ){
		version = (( ofaITreeAdderInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaITreeAdder::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_itree_adder_get_column_types:
 * @getter: a #ofaIGetter instance.
 * @store: the target #ofaIStore.
 * @orig_cols_count: the original count of columns in the @store.
 * @orig_col_types: the original array of defined GType's.
 * @cols_count: [out]: the final count of columns in the @store.
 *
 * Propose each #ofaITreeAdder implementation to add some columns in the
 * @store.
 *
 * Returns: the final array of GType's for the @store.
 *
 * The returned array should be g_free() by the caller.
 */
GType *
ofa_itree_adder_get_column_types( ofaIGetter *getter, ofaIStore *store,
			guint orig_cols_count, GType *orig_col_types, guint *cols_count )
{
	ofaExtenderCollection *collection;
	GList *modules, *it;
	GType *col_types, *add_types, *temp_types;
	guint add_cols;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( store && OFA_IS_ISTORE( store ), NULL );
	g_return_val_if_fail( cols_count, NULL );

	collection = ofa_igetter_get_extender_collection( getter );
	modules = ofa_extender_collection_get_for_type( collection, OFA_TYPE_ITREE_ADDER );
	col_types = add_column_types( 0, NULL, orig_cols_count, orig_col_types );
	*cols_count = orig_cols_count;

	for( it=modules ; it ; it=it->next ){
		add_types = itree_adder_get_column_types(
				OFA_ITREE_ADDER( it->data ), store, *cols_count, &add_cols );
		if( add_cols && add_types ){
			temp_types = col_types;
			col_types = add_column_types( *cols_count, temp_types, add_cols, add_types );
			*cols_count += add_cols;
			g_free( temp_types );
		}
	}

	g_list_free( modules );

	return( col_types );
}

static GType *
add_column_types( guint orig_cols_count, GType *orig_col_types, guint add_cols, GType *add_types )
{
	GType *final_types;
	guint i;

	final_types = g_new0( GType, 1+orig_cols_count+add_cols );
	for( i=0 ; i<orig_cols_count ; ++i ){
		final_types[i] = orig_col_types[i];
	}
	for( i=0 ; i<add_cols ; ++i ){
		final_types[i+orig_cols_count] = add_types[i];
	}

	return( final_types );
}

static GType *
itree_adder_get_column_types( ofaITreeAdder *instance, ofaIStore *store, guint cols_count, guint *add_cols )
{
	static const gchar *thisfn = "ofa_itree_adder_get_column_types";

	if( OFA_ITREE_ADDER_GET_INTERFACE( instance )->get_column_types ){
		return( OFA_ITREE_ADDER_GET_INTERFACE( instance )->get_column_types( instance, store, cols_count, add_cols ));
	}

	g_info( "%s: ofaITreeAdder's %s implementation does not provide 'get_column_types()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_itree_adder_set_values:
 * @getter: a #ofaIGetter instance.
 * @store: the target #ofaIStore.
 * @iter: the current #GtkTreeIter.
 * @object: the current object.
 *
 * Let an implementation set its values in the current row.
 */
void
ofa_itree_adder_set_values( ofaIGetter *getter, ofaIStore *store, GtkTreeIter *iter, void *object )
{
	ofaExtenderCollection *collection;
	GList *modules, *it;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( store && OFA_IS_ISTORE( store ));

	collection = ofa_igetter_get_extender_collection( getter );
	modules = ofa_extender_collection_get_for_type( collection, OFA_TYPE_ITREE_ADDER );
	for( it=modules ; it ; it=it->next ){
		itree_adder_set_values( OFA_ITREE_ADDER( it->data ), store, getter, iter, object );
	}
	g_list_free( modules );
}

static void
itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaIGetter *getter, GtkTreeIter *iter, void *object )
{
	static const gchar *thisfn = "ofa_itree_adder_set_values";

	if( OFA_ITREE_ADDER_GET_INTERFACE( instance )->set_values ){
		OFA_ITREE_ADDER_GET_INTERFACE( instance )->set_values( instance, store, getter, iter, object );
		return;
	}

	g_info( "%s: ofaITreeAdder's %s implementation does not provide 'set_values()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * ofa_itree_adder_sort:
 * @getter: a #ofaIGetter instance.
 * @store: the target #ofaIStore.
 * @model: a #GtkTreeModel model.
 * @a: a row to be compared.
 * @b: another row to compare to @a.
 * @column_id: the index of the column to be compared.
 * @cmp: [out]: the result of the comparison.
 *
 * Returns: %TRUE if the column is managed here, %FALSE else.
 */
gboolean
ofa_itree_adder_sort( ofaIGetter *getter, ofaIStore *store, GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, gint *cmp )
{
	ofaExtenderCollection *collection;
	GList *modules, *it;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( store && OFA_IS_ISTORE( store ), FALSE );
	g_return_val_if_fail( cmp, FALSE );

	collection = ofa_igetter_get_extender_collection( getter );
	modules = ofa_extender_collection_get_for_type( collection, OFA_TYPE_ITREE_ADDER );
	for( it=modules ; it ; it=it->next ){
		if( itree_adder_sort( OFA_ITREE_ADDER( it->data ), store, getter, model, a, b, column_id, cmp )){
			return( TRUE );
		}
	}
	g_list_free( modules );

	return( FALSE );
}

static gboolean
itree_adder_sort( ofaITreeAdder *instance, ofaIStore *store, ofaIGetter *getter, GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, gint *cmp )
{
	static const gchar *thisfn = "ofa_itree_adder_set_values";

	if( OFA_ITREE_ADDER_GET_INTERFACE( instance )->sort ){
		return( OFA_ITREE_ADDER_GET_INTERFACE( instance )->sort( instance, store, getter, model, a, b, column_id, cmp ));
	}

	g_info( "%s: ofaITreeAdder's %s implementation does not provide 'set_values()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * ofa_itree_adder_add_columns:
 * @getter: a #ofaIGetter instance.
 * @store: the source #ofaIStore.
 * @bin: the #ofaTVBin which manages the #GtkTreeView treeview.
 *
 * Let an implementation add its columns to a treeview.
 */
void
ofa_itree_adder_add_columns( ofaIGetter *getter, ofaIStore *store, ofaTVBin *bin )
{
	ofaExtenderCollection *collection;
	GList *modules, *it;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( store && OFA_IS_ISTORE( store ));
	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	collection = ofa_igetter_get_extender_collection( getter );
	modules = ofa_extender_collection_get_for_type( collection, OFA_TYPE_ITREE_ADDER );
	for( it=modules ; it ; it=it->next ){
		itree_adder_add_columns( OFA_ITREE_ADDER( it->data ), store, bin );
	}
	g_list_free( modules );
}

/*
 * itree_adder_add_columns:
 * @instance: the #ofaITreeAdder instance.
 * @store:
 * @treeview:
 *
 * Add columns to a @treeview.
 */
static void
itree_adder_add_columns( ofaITreeAdder *instance, ofaIStore *store, ofaTVBin *bin )
{
	static const gchar *thisfn = "ofa_itree_adder_add_columns";

	if( OFA_ITREE_ADDER_GET_INTERFACE( instance )->add_columns ){
		OFA_ITREE_ADDER_GET_INTERFACE( instance )->add_columns( instance, store, bin );
		return;
	}

	g_info( "%s: ofaITreeAdder's %s implementation does not provide 'add_columns()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}
