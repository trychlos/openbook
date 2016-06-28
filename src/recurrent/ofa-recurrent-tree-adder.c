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

#include <glib/gi18n.h>

#include "api/ofa-hub.h"
#include "api/ofa-istore.h"
#include "api/ofa-itree-adder.h"
#include "api/ofa-ope-template-store.h"
#include "api/ofo-ope-template.h"

#include "ofa-recurrent-tree-adder.h"
#include "ofo-recurrent-model.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
	ofaHub  *hub;
	GList   *stores;
}
	ofaRecurrentTreeAdderPrivate;

/* the managed stores
 */
typedef struct {
	ofaIStore *store;
	guint      col_object;
	GList     *ids;
}
	sStore;

/* The columns managed here
 * These are the 'adder_id': our own column identifier
 */
enum {
	OPE_TEMPLATE_STORE_COL_RECURRENT = 1,
};

/* Associates our own column identifier with the column identifier
 * provided by the store itself.
 * Only the store identifier is valid when interacting with the store.
 */
typedef struct {
	guint adder_id;
	guint store_id;
}
	sIDs;

static const gchar *st_resource_filler_png    = "/org/trychlos/openbook/recurrent/filler.png";
static const gchar *st_resource_recurrent_png = "/org/trychlos/openbook/recurrent/ofa-recurrent-icon-16x16.png";

static void    itree_adder_iface_init( ofaITreeAdderInterface *iface );
static void    itree_adder_add_types( ofaITreeAdder *instance, ofaIStore *store, guint column_object, TreeAdderTypeCb cb, void * cb_data );
static void    add_column_id( ofaRecurrentTreeAdder *self, sStore *store_data, guint adder_id, GType type, TreeAdderTypeCb cb, void * cb_data );
static void    itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeIter *iter, void *object );
static void    ope_template_store_set_values( ofaRecurrentTreeAdder *self, sStore *store_data, ofaHub *hub, GtkTreeIter *iter, ofoOpeTemplate *template );
static void    ope_template_set_is_recurrent( ofaRecurrentTreeAdder *self, sStore *store_data, ofaHub *hub, GtkTreeIter *iter, guint col_id, ofoOpeTemplate *template );
static void    itree_adder_add_columns( ofaITreeAdder *instance, ofaIStore *store, GtkWidget *treeview );
static void    ope_template_store_add_columns( ofaRecurrentTreeAdder *self, sStore *store_data, GtkWidget *treeview );
static sStore *get_store_data( ofaRecurrentTreeAdder *self, void *store, gboolean create );
static void    on_store_finalized( ofaRecurrentTreeAdder *self, GObject *finalized_store );
static void    free_store( sStore *sdata );
static void    free_ids( sIDs *sids );
static void    connect_to_hub_signaling_system( ofaRecurrentTreeAdder *self, ofaHub *hub );
static void    hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentTreeAdder *self );
static void    hub_on_new_recurrent_model( ofaRecurrentTreeAdder *self, ofaHub *hub, ofoRecurrentModel *model );
static void    ope_template_store_set_recurrent_model( ofaRecurrentTreeAdder *self, ofaHub *hub, ofoRecurrentModel *model, sStore *store_data );
static void    ope_template_store_update_recurrent_model_rec( ofaRecurrentTreeAdder *self, ofaHub *hub, sStore *store_data, GtkTreeIter *iter );
static void    hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentTreeAdder *self );
static void    hub_on_updated_recurrent_model( ofaRecurrentTreeAdder *self, ofaHub *hub, ofoRecurrentModel *model );

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

	g_list_free_full( priv->stores, ( GDestroyNotify ) free_store );

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
	static const gchar *thisfn = "ofa_recurrent_tree_adder_itree_adder_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->add_types = itree_adder_add_types;
	iface->set_values = itree_adder_set_values;
	iface->add_columns = itree_adder_add_columns;
}

static void
itree_adder_add_types( ofaITreeAdder *instance, ofaIStore *store, guint column_object, TreeAdderTypeCb cb, void *cb_data )
{
	sStore *store_data;

	store_data = get_store_data( OFA_RECURRENT_TREE_ADDER( instance ), store, TRUE );
	store_data->col_object = column_object;

	if( OFA_IS_OPE_TEMPLATE_STORE( store )){
		add_column_id( OFA_RECURRENT_TREE_ADDER( instance ),
				store_data, OPE_TEMPLATE_STORE_COL_RECURRENT, GDK_TYPE_PIXBUF, cb, cb_data );
	}
}

/*
 * @adder_id: our own column identifier
 * @store_id: the column identifier returned by the store
 */
static void
add_column_id( ofaRecurrentTreeAdder *self, sStore *store_data, guint adder_id, GType type, TreeAdderTypeCb cb, void *cb_data )
{
	sIDs *sdata;
	guint col_id;

	col_id = ( *cb )( store_data->store, type, cb_data );

	sdata = g_new0( sIDs, 1 );
	sdata->adder_id = adder_id;
	sdata->store_id = col_id;

	store_data->ids = g_list_prepend( store_data->ids, sdata );
}

static void
itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeIter *iter, void *object )
{
	ofaRecurrentTreeAdderPrivate *priv;
	sStore *store_data;

	priv = ofa_recurrent_tree_adder_get_instance_private( OFA_RECURRENT_TREE_ADDER( instance ));

	if( !priv->hub ){
		priv->hub = hub;
		connect_to_hub_signaling_system( OFA_RECURRENT_TREE_ADDER( instance ), hub );
	}

	store_data = get_store_data( OFA_RECURRENT_TREE_ADDER( instance ), store, TRUE );

	if( OFA_IS_OPE_TEMPLATE_STORE( store )){
		ope_template_store_set_values( OFA_RECURRENT_TREE_ADDER( instance ), store_data, hub, iter, OFO_OPE_TEMPLATE( object ));
	}
}

static void
ope_template_store_set_values( ofaRecurrentTreeAdder *self, sStore *store_data, ofaHub *hub, GtkTreeIter *iter, ofoOpeTemplate *template )
{
	GList *it;
	sIDs *sdata;

	for( it=store_data->ids ; it ; it=it->next ){
		sdata = ( sIDs * ) it->data;
		switch( sdata->adder_id ){
			case OPE_TEMPLATE_STORE_COL_RECURRENT:
				ope_template_set_is_recurrent( self, store_data, hub, iter, sdata->store_id, template );
				break;
			default:
				break;
		}
	}
}

/*
 * set a small graphic indicator if the operation template is used as
 * a recurrent model
 */
static void
ope_template_set_is_recurrent( ofaRecurrentTreeAdder *self, sStore *store_data, ofaHub *hub, GtkTreeIter *iter, guint col_id, ofoOpeTemplate *template )
{
	gboolean is_recurrent;
	GdkPixbuf *png;
	const gchar *mnemo;

	mnemo = ofo_ope_template_get_mnemo( template );
	is_recurrent = ofo_recurrent_model_use_ope_template( hub, mnemo );
	png = gdk_pixbuf_new_from_resource( is_recurrent ? st_resource_recurrent_png : st_resource_filler_png, NULL );

	if( GTK_IS_LIST_STORE( store_data->store )){
		gtk_list_store_set( GTK_LIST_STORE( store_data->store ), iter, col_id, png, -1 );
	} else {
		gtk_tree_store_set( GTK_TREE_STORE( store_data->store ), iter, col_id, png, -1 );
	}
}

static void
itree_adder_add_columns( ofaITreeAdder *instance, ofaIStore *store, GtkWidget *treeview )
{
	sStore *store_data;

	store_data = get_store_data( OFA_RECURRENT_TREE_ADDER( instance ), store, TRUE );

	if( OFA_IS_OPE_TEMPLATE_STORE( store )){
		ope_template_store_add_columns( OFA_RECURRENT_TREE_ADDER( instance ), store_data, treeview );
	}
}

static void
ope_template_store_add_columns( ofaRecurrentTreeAdder *self, sStore *store_data, GtkWidget *treeview )
{
	GList *it;
	sIDs *sdata;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	for( it=store_data->ids ; it ; it=it->next ){
		sdata = ( sIDs * ) it->data;
		switch( sdata->adder_id ){
			case OPE_TEMPLATE_STORE_COL_RECURRENT:
				cell = gtk_cell_renderer_pixbuf_new();
				column = gtk_tree_view_column_new_with_attributes(
							"", cell, "pixbuf", sdata->store_id, NULL );
				gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), column );
				break;
			default:
				break;
		}
	}
}

static sStore *
get_store_data( ofaRecurrentTreeAdder *self, void *store, gboolean create )
{
	ofaRecurrentTreeAdderPrivate *priv;
	GList *it;
	sStore *sdata;

	priv = ofa_recurrent_tree_adder_get_instance_private( self );

	if( priv->dispose_has_run ){
		return( NULL );
	}

	for( it=priv->stores ; it ; it=it->next ){
		sdata = ( sStore * ) it->data;
		if( sdata->store == store ){
			return( sdata );
		}
	}

	sdata = NULL;

	if( create ){
		sdata = g_new0( sStore, 1 );
		sdata->store = store;
		priv->stores = g_list_prepend( priv->stores, sdata );
		g_object_weak_ref( G_OBJECT( store ), ( GWeakNotify ) on_store_finalized, self );
	}

	return( sdata );
}

static void
on_store_finalized( ofaRecurrentTreeAdder *self, GObject *finalized_store )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_on_store_finalized";
	ofaRecurrentTreeAdderPrivate *priv;
	sStore *sdata;

	g_debug( "%s: self=%p, finalized_store=%p (%s)",
			thisfn, ( void * ) self, ( void * ) finalized_store, G_OBJECT_TYPE_NAME( finalized_store ));

	priv = ofa_recurrent_tree_adder_get_instance_private( self );

	sdata = get_store_data( self, finalized_store, FALSE );

	if( sdata ){
		priv->stores = g_list_remove( priv->stores, sdata );
		free_store( sdata );
	}
}

static void
free_store( sStore *sdata )
{
	g_list_free_full( sdata->ids, ( GDestroyNotify ) free_ids );
	g_free( sdata );
}

static void
free_ids( sIDs *sids )
{
	g_free( sids );
}

/*
 * Hub signaling system
 */
static void
connect_to_hub_signaling_system( ofaRecurrentTreeAdder *self, ofaHub *hub )
{
	g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentTreeAdder *self )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		hub_on_new_recurrent_model( self, hub, OFO_RECURRENT_MODEL( object ));
	}
}

static void
hub_on_new_recurrent_model( ofaRecurrentTreeAdder *self, ofaHub *hub, ofoRecurrentModel *model )
{
	hub_on_updated_recurrent_model( self, hub, model );
}

static void
ope_template_store_set_recurrent_model( ofaRecurrentTreeAdder *self, ofaHub *hub, ofoRecurrentModel *model, sStore *store_data )
{
	GtkTreeIter iter;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store_data->store ), &iter )){
		ope_template_store_update_recurrent_model_rec( self, hub, store_data, &iter );
	}
}

static void
ope_template_store_update_recurrent_model_rec( ofaRecurrentTreeAdder *self, ofaHub *hub, sStore *store_data, GtkTreeIter *iter )
{
	GtkTreeIter child_iter;
	ofoOpeTemplate *template;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( store_data->store ), &child_iter, iter )){
			ope_template_store_update_recurrent_model_rec( self, hub, store_data, &child_iter );
		}

		gtk_tree_model_get( GTK_TREE_MODEL( store_data->store ), iter, store_data->col_object, &template, -1 );
		g_return_if_fail( template && OFO_IS_OPE_TEMPLATE( template ));
		g_object_unref( template );

		ope_template_store_set_values( self, store_data, hub, iter, template );

		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store_data->store ), iter )){
			break;
		}
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentTreeAdder *self )
{
	static const gchar *thisfn = "ofa_recurrent_tree_adder_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		hub_on_updated_recurrent_model( self, hub, OFO_RECURRENT_MODEL( object ));
	}
}

static void
hub_on_updated_recurrent_model( ofaRecurrentTreeAdder *self, ofaHub *hub, ofoRecurrentModel *model )
{
	ofaRecurrentTreeAdderPrivate *priv;
	GList *it;
	sStore *store_data;

	priv = ofa_recurrent_tree_adder_get_instance_private( self );

	for( it=priv->stores ; it ; it=it->next ){
		store_data = ( sStore * ) it->data;
		if( OFA_IS_OPE_TEMPLATE_STORE( store_data->store )){
			ope_template_store_set_recurrent_model( self, hub, model, store_data );
		}
	}
}
