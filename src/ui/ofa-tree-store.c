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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofo-dossier-def.h"

#include "ui/ofa-tree-store.h"

/* private instance data
 */
struct _ofaTreeStorePrivate {
	gboolean    dispose_has_run;
	gboolean    from_dossier_finalized;

	/* properties
	 */
	ofoDossier *dossier;
};

/* class properties
 */
enum {
	OFA_PROP_DOSSIER_ID = 1,
};

G_DEFINE_TYPE( ofaTreeStore, ofa_tree_store, GTK_TYPE_TREE_STORE )

static void on_dossier_finalized( ofaTreeStore *store, gpointer finalized_dossier );

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

	priv = OFA_TREE_STORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( !priv->from_dossier_finalized ){
			g_object_weak_unref(
					G_OBJECT( priv->dossier ), ( GWeakNotify ) on_dossier_finalized, instance );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tree_store_parent_class )->dispose( instance );
}

static void
tree_store_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaTreeStorePrivate *priv;

	g_return_if_fail( object && OFA_IS_TREE_STORE( object ));

	priv = OFA_TREE_STORE( object )->priv;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_DOSSIER_ID:
				g_value_set_pointer( value, priv->dossier );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
tree_store_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaTreeStorePrivate *priv;

	g_return_if_fail( OFA_IS_TREE_STORE( object ));

	priv = OFA_TREE_STORE( object )->priv;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_DOSSIER_ID:
				priv->dossier = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
tree_store_constructed( GObject *instance )
{
	ofaTreeStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_TREE_STORE( instance ));

	/* first, chain up to the parent class */
	G_OBJECT_CLASS( ofa_tree_store_parent_class )->constructed( instance );

	/* weak ref the dossier (which must have been set at instanciation
	 * time of the derived class), so that we will be auto-unreffed at
	 * dossier finalization
	 */
	priv = OFA_TREE_STORE( instance )->priv;
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	g_object_weak_ref( G_OBJECT( priv->dossier ), ( GWeakNotify ) on_dossier_finalized, instance );
}

static void
ofa_tree_store_init( ofaTreeStore *self )
{
	static const gchar *thisfn = "ofa_tree_store_init";

	g_return_if_fail( OFA_IS_TREE_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TREE_STORE, ofaTreeStorePrivate );
}

static void
ofa_tree_store_class_init( ofaTreeStoreClass *klass )
{
	static const gchar *thisfn = "ofa_tree_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = tree_store_constructed;
	G_OBJECT_CLASS( klass )->get_property = tree_store_get_property;
	G_OBJECT_CLASS( klass )->set_property = tree_store_set_property;
	G_OBJECT_CLASS( klass )->dispose = tree_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = tree_store_finalize;

	g_type_class_add_private( klass, sizeof( ofaTreeStorePrivate ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_DOSSIER_ID,
			g_param_spec_pointer(
					OFA_PROP_DOSSIER,
					"Dossier",
					"The currently opened dossier",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

static void
on_dossier_finalized( ofaTreeStore *store, gpointer finalized_dossier )
{
	static const gchar *thisfn = "ofa_tree_store_on_dossier_finalized";
	ofaTreeStorePrivate *priv;

	g_debug( "%s: store=%p, finalized_dossier=%p",
			thisfn, ( void * ) store, ( void * ) finalized_dossier );

	g_return_if_fail( store && OFA_IS_TREE_STORE( store ));

	priv = store->priv;
	priv->from_dossier_finalized = TRUE;

	g_object_unref( store );
}
