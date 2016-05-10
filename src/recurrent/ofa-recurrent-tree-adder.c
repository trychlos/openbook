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

#include <glib/gi18n.h>

#include "api/ofa-istore.h"
#include "api/ofa-itree-adder.h"
#include "api/ofa-ope-template-store.h"

#include "ofa-recurrent-tree-adder.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
	GList   *ids;
}
	ofaRecurrentTreeAdderPrivate;

/* the columns managed here
 */
enum {
	OPE_TEMPLATE_STORE_COL_RECURRENT = 1,
};

typedef struct {
	guint adder_id;
	guint store_id;
}
	sIDs;

static void itree_adder_iface_init( ofaITreeAdderInterface *iface );
static void itree_adder_get_types( ofaITreeAdder *instance, ofaIStore *store, TreeAdderTypeCb cb, void * cb_data );
static void add_column_id( ofaRecurrentTreeAdder *self, guint adder_id, guint store_id );
static void free_ids( sIDs *sids );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentTreeAdder, ofa_recurrent_tree_adder, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaRecurrentTreeAdder )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREE_ADDER, itree_adder_iface_init ))

static void
recurrent_tree_adder_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_finalize";
	ofaRecurrentTreeAdderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_TREE_ADDER( instance ));

	/* free data members here */
	priv = ofa_recurrent_tree_adder_get_instance_private( OFA_RECURRENT_TREE_ADDER( instance ));

	g_list_free_full( priv->ids, ( GDestroyNotify ) free_ids );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_tree_adder_parent_class )->finalize( instance );
}

static void
recurrent_tree_adder_dispose( GObject *instance )
{
	ofaRecurrentTreeAdderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_TREE_ADDER( instance ));

	priv = ofa_recurrent_tree_adder_get_instance_private( OFA_RECURRENT_TREE_ADDER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_tree_adder_parent_class )->dispose( instance );
}

static void
ofa_recurrent_tree_adder_init( ofaRecurrentTreeAdder *self )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_init";
	ofaRecurrentTreeAdderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_TREE_ADDER( self ));

	priv = ofa_recurrent_tree_adder_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_recurrent_tree_adder_class_init( ofaRecurrentTreeAdderClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_tree_adder_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_tree_adder_finalize;
}

/*
 * #ofaITreeAdder interface management
 */
static void
itree_adder_iface_init( ofaITreeAdderInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_types = itree_adder_get_types;
}

static void
itree_adder_get_types( ofaITreeAdder *instance, ofaIStore *store, TreeAdderTypeCb cb, void *cb_data )
{
	guint col_id;

	if( OFA_IS_OPE_TEMPLATE_STORE( store )){
		col_id = ( *cb )( store, GDK_TYPE_PIXBUF, cb_data );
		add_column_id( OFA_RECURRENT_TREE_ADDER( instance ), OPE_TEMPLATE_STORE_COL_RECURRENT, col_id );
	}
}

/*
 * @adder_id: our own column identifier
 * @store_id: the column identifier returned by the store
 */
static void
add_column_id( ofaRecurrentTreeAdder *self, guint adder_id, guint store_id )
{
	ofaRecurrentTreeAdderPrivate *priv;
	sIDs *sdata;

	priv = ofa_recurrent_tree_adder_get_instance_private( self );

	sdata = g_new0( sIDs, 1 );
	sdata->adder_id = adder_id;
	sdata->store_id = store_id;

	priv->ids = g_list_prepend( priv->ids, sdata );
}

static void
free_ids( sIDs *sids )
{
	g_free( sids );
}
