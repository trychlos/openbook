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

#include "api/ofa-istore.h"
#include "api/ofa-tree-store.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaTreeStorePrivate;

static void   istore_iface_init( ofaIStoreInterface *iface );
static guint  istore_get_interface_version( void );
static void   istore_load_dataset( ofaIStore *istore );
static void   loading_simulate_rec( ofaTreeStore *self, GtkTreeIter *iter );
static void   on_row_inserted( GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, ofaTreeStore *self );

G_DEFINE_TYPE_EXTENDED( ofaTreeStore, ofa_tree_store, GTK_TYPE_TREE_STORE, 0,
		G_ADD_PRIVATE( ofaTreeStore )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISTORE, istore_iface_init ))

static void
tree_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tree_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TREE_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tree_store_parent_class )->finalize( instance );
}

static void
tree_store_dispose( GObject *instance )
{
	ofaTreeStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_TREE_STORE( instance ));

	priv = ofa_tree_store_get_instance_private( OFA_TREE_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tree_store_parent_class )->dispose( instance );
}

static void
ofa_tree_store_init( ofaTreeStore *self )
{
	static const gchar *thisfn = "ofa_tree_store_init";
	ofaTreeStorePrivate *priv;

	g_return_if_fail( OFA_IS_TREE_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tree_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	ofa_istore_init( OFA_ISTORE( self ));
	g_signal_connect( self, "row-inserted", G_CALLBACK( on_row_inserted ), self );
}

static void
ofa_tree_store_class_init( ofaTreeStoreClass *klass )
{
	static const gchar *thisfn = "ofa_tree_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tree_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = tree_store_finalize;
}

/*
 * ofaIStore interface management
 */
static void
istore_iface_init( ofaIStoreInterface *iface )
{
	static const gchar *thisfn = "ofo_tree_store_istore_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = istore_get_interface_version;
	iface->load_dataset = istore_load_dataset;
}

static guint
istore_get_interface_version( void )
{
	return( 1 );
}

static void
istore_load_dataset( ofaIStore *istore )
{
	if( OFA_TREE_STORE_GET_CLASS( istore )->load_dataset ){
		OFA_TREE_STORE_GET_CLASS( istore )->load_dataset( OFA_TREE_STORE( istore ));
	}
}

/**
 * ofa_tree_store_loading_simulate:
 * @store: this #ofaTreeStore instance.
 *
 * Simulate the reload of the current dataset, just by sending
 * 'ofa-row-inserted' messages on the store.
 */
void
ofa_tree_store_loading_simulate( ofaTreeStore *store )
{
	ofaTreeStorePrivate *priv;
	GtkTreeIter iter;

	g_return_if_fail( store && OFA_IS_TREE_STORE( store ));

	priv = ofa_tree_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter )){
		loading_simulate_rec( store, &iter );
	}
}

static void
loading_simulate_rec( ofaTreeStore *self, GtkTreeIter *iter )
{
	GtkTreeIter child_iter;

	while( TRUE ){
		g_signal_emit_by_name( G_OBJECT( self ), "ofa-row-inserted", iter );
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			loading_simulate_rec( self, &child_iter );
		}
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}
}

/*
 * proxy of Gtk 'row-inserted' signal to 'ofa-row-inserted'
 */
static void
on_row_inserted( GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, ofaTreeStore *self )
{
	g_signal_emit_by_name( G_OBJECT( self ), "ofa-row-inserted", iter );
}
