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

#include "api/ofa-hub.h"
#include "api/ofa-istore.h"
#include "api/ofo-dossier.h"

#include "api/ofa-list-store.h"

/* private instance data
 */
struct _ofaListStorePrivate {
	gboolean    dispose_has_run;

	/* properties
	 */
	ofaHub     *hub;
	gboolean    dataset_loaded;
};

/* class properties
 */
enum {
	OFA_PROP_HUB_ID = 1,
};

/* signals defined here
 */
enum {
	ROW_INSERTED = 0,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

static void   istore_iface_init( ofaIStoreInterface *iface );
static guint  istore_get_interface_version( const ofaIStore *instance );

G_DEFINE_TYPE_EXTENDED( ofaListStore, ofa_list_store, GTK_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaListStore )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISTORE, istore_iface_init ))

static void
list_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_list_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LIST_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_list_store_parent_class )->finalize( instance );
}

static void
list_store_dispose( GObject *instance )
{
	ofaListStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_LIST_STORE( instance ));

	priv = ofa_list_store_get_instance_private( OFA_LIST_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_list_store_parent_class )->dispose( instance );
}

static void
list_store_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaListStorePrivate *priv;

	g_return_if_fail( object && OFA_IS_LIST_STORE( object ));

	priv = ofa_list_store_get_instance_private( OFA_LIST_STORE( object ));
	g_return_if_fail( !priv->dispose_has_run );

	switch( property_id ){
		case OFA_PROP_HUB_ID:
			g_value_set_object( value, priv->hub );
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
			break;
	}
}

static void
list_store_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaListStorePrivate *priv;

	g_return_if_fail( object && OFA_IS_LIST_STORE( object ));

	priv = ofa_list_store_get_instance_private( OFA_LIST_STORE( object ));
	g_return_if_fail( !priv->dispose_has_run );

	switch( property_id ){
		case OFA_PROP_HUB_ID:
			priv->hub = g_value_get_object( value );
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
			break;
	}
}

static void
list_store_constructed( GObject *instance )
{
	ofaListStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_LIST_STORE( instance ));

	/* first, chain up to the parent class */
	G_OBJECT_CLASS( ofa_list_store_parent_class )->constructed( instance );

	/* weak ref the dossier (which must have been set at instanciation
	 * time of the derived class), so that we will be auto-unreffed at
	 * dossier finalization
	 */
	priv = ofa_list_store_get_instance_private( OFA_LIST_STORE( instance ));

	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	ofa_istore_init( OFA_ISTORE( instance ), priv->hub );
}

static void
ofa_list_store_init( ofaListStore *self )
{
	static const gchar *thisfn = "ofa_list_store_init";
	ofaListStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LIST_STORE( self ));

	priv = ofa_list_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_list_store_class_init( ofaListStoreClass *klass )
{
	static const gchar *thisfn = "ofa_list_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = list_store_constructed;
	G_OBJECT_CLASS( klass )->get_property = list_store_get_property;
	G_OBJECT_CLASS( klass )->set_property = list_store_set_property;
	G_OBJECT_CLASS( klass )->dispose = list_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = list_store_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_HUB_ID,
			g_param_spec_object(
					OFA_PROP_HUB,
					"Hub",
					"The current ofaHub object",
					OFA_TYPE_HUB,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	/**
	 * ofaListStore::ofa-row-inserted:
	 *
	 * The signal is emitted either because a new row has been
	 * inserted into the #GtkTreeModel implementor, or when we are
	 * trying to load an already previously loaded dataset.
	 * This later is typically useful when the build of the display
	 * is event-based.
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofaListStore *store,
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
				OFA_TYPE_LIST_STORE,
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
	static const gchar *thisfn = "ofa_list_store_istore_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = istore_get_interface_version;
}

static guint
istore_get_interface_version( const ofaIStore *instance )
{
	return( 1 );
}

/**
 * ofa_list_store_load_dataset:
 * @store: this #ofaListStore instance.
 */
void
ofa_list_store_load_dataset( ofaListStore *store )
{
	ofaListStorePrivate *priv;

	g_return_if_fail( store && OFA_IS_LIST_STORE( store ));

	priv = ofa_list_store_get_instance_private( store );
	g_return_if_fail( !priv->dispose_has_run );

	if( !priv->dataset_loaded ){
		if( OFA_LIST_STORE_GET_CLASS( store )->load_dataset ){
			OFA_LIST_STORE_GET_CLASS( store )->load_dataset( store );
		}
		priv->dataset_loaded = TRUE;

	} else {
		ofa_istore_simulate_dataset_load( OFA_ISTORE( store ));
	}
}

/**
 * ofa_list_store_get_hub:
 * @store: this #ofaListStore instance.
 *
 * Returns: the #ofaHub object provided at initialization time.
 *
 * The returned instance is owned by @store object, and should not be
 * released by the caller.
 */
ofaHub *
ofa_list_store_get_hub( const ofaListStore *store )
{
	ofaListStorePrivate *priv;

	g_return_val_if_fail( store && OFA_IS_LIST_STORE( store ), NULL );

	priv = ofa_list_store_get_instance_private( store );
	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->hub );
}
