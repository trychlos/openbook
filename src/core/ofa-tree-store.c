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

#include "api/ofa-hub.h"
#include "api/ofa-istore.h"
#include "api/ofa-tree-store.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime
	 */
	gboolean dataset_loaded;
}
	ofaTreeStorePrivate;

/* signals defined here
 */
enum {
	ROW_INSERTED = 0,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

static void   istore_iface_init( ofaIStoreInterface *iface );
static guint  istore_get_interface_version( void );

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
}

static void
ofa_tree_store_class_init( ofaTreeStoreClass *klass )
{
	static const gchar *thisfn = "ofa_tree_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tree_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = tree_store_finalize;

	/**
	 * ofaTreeStore::ofa-row-inserted:
	 *
	 * The signal is emitted either because a new row has been
	 * inserted into the #GtkTreeModel implementor, or when we are
	 * trying to load an already previously loaded dataset.
	 * This later is typically useful when the build of the display
	 * is event-based.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofaTreeStore *store,
	 * 								GtkTreePath *path,
	 * 								GtkTreeIter *iter
	 * 								gpointer     user_data );
	 *
	 * It appears that an interface <I> is only able to send a message
	 * defined in this same interface <I> to an instance of <I> if the
	 * client <A> class directly implements the interface <I>.
	 * In other terms, a class <B> which derives from <A>, is not a
	 * valid dest for a message defined in <I>.
	 *
	 * As only #ofaListStore implements #ofaIStore interface, and not
	 * its derived class, #ofaIStore would be unable to send this
	 * message when defined to, e.g. #ofaOpeTemplateStore, if this
	 * definition had stayed in interface itself.
	 */
	st_signals[ ROW_INSERTED ] = g_signal_new_class_handler(
				"ofa-row-inserted",
				OFA_TYPE_TREE_STORE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_POINTER );
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
}

static guint
istore_get_interface_version( void )
{
	return( 1 );
}

/**
 * ofa_tree_store_load_dataset:
 * @store: the #ofaTreeStore-derived object.
 * @hub: the #ofaHub hub of the application.
 *
 * The first time, loads the dataset.
 * The other times, only simulates this load, just emitting one signal
 * per row.
 */
void
ofa_tree_store_load_dataset( ofaTreeStore *store, ofaHub *hub )
{
	ofaTreeStorePrivate *priv;

	g_return_if_fail( store && OFA_IS_TREE_STORE( store ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_tree_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	if( !priv->dataset_loaded ){
		if( OFA_TREE_STORE_GET_CLASS( store )->load_dataset ){
			OFA_TREE_STORE_GET_CLASS( store )->load_dataset( store, hub );
		}
		priv->dataset_loaded = TRUE;

	} else {
		ofa_istore_simulate_dataset_load( OFA_ISTORE( store ));
	}
}
