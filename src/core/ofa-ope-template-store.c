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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-ope-template-store.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
	ofaHub  *hub;
	GList   *hub_handlers;
}
	ofaOpeTemplateStorePrivate;

static GType st_col_types[OPE_TEMPLATE_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* mnemo, label, ledger */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* notes, upd_user, upd_stamp */
		G_TYPE_STRING,									/* is_recurrent_model */
		G_TYPE_OBJECT									/* the #ofoOpeTemplate itself */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplateStore *self );
static void     list_store_load_dataset( ofaListStore *self, ofaHub *hub );
static void     insert_row( ofaOpeTemplateStore *self, ofaHub *hub, const ofoOpeTemplate *ope );
static void     set_row( ofaOpeTemplateStore *self, ofaHub *hub, const ofoOpeTemplate *ope, GtkTreeIter *iter );
static gboolean find_row_by_mnemo( ofaOpeTemplateStore *self, const gchar *mnemo, GtkTreeIter *iter, gboolean *bvalid );
static void     remove_row_by_mnemo( ofaOpeTemplateStore *self, const gchar *mnemo );
static void     connect_to_hub_signaling_system( ofaOpeTemplateStore *self, ofaHub *hub );
static void     on_hub_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateStore *self );
static void     on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateStore *self );
static void     hub_on_updated_account( ofaOpeTemplateStore *self, const gchar *prev_id, const gchar *new_id );
static void     on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateStore *self );
static void     on_hub_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateStore *self );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateStore, ofa_ope_template_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaOpeTemplateStore ))

static void
ope_template_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_store_parent_class )->finalize( instance );
}

static void
ope_template_store_dispose( GObject *instance )
{
	ofaOpeTemplateStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_STORE( instance ));

	priv = ofa_ope_template_store_get_instance_private( OFA_OPE_TEMPLATE_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_store_parent_class )->dispose( instance );
}

static void
ofa_ope_template_store_init( ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_init";
	ofaOpeTemplateStorePrivate *priv;

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ope_template_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
}

static void
ofa_ope_template_store_class_init( ofaOpeTemplateStoreClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_store_finalize;

	OFA_LIST_STORE_CLASS( klass )->load_dataset = list_store_load_dataset;
}

/**
 * ofa_ope_template_store_new:
 * @hub: the current #ofaHub.
 *
 * Instanciates a new #ofaOpeTemplateStore and attached it to the
 * @hub if not already done. Else get the already allocated
 * #ofaOpeTemplateStore from the @hub collectioner.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * unique instance will be unreffed when the @hub is destroyed.
 */
ofaOpeTemplateStore *
ofa_ope_template_store_new( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_ope_template_store_new";
	ofaOpeTemplateStore *store;
	myICollector *collector;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	collector = ofa_hub_get_collector( hub );
	store = ( ofaOpeTemplateStore * ) my_icollector_single_get_object( collector, OFA_TYPE_OPE_TEMPLATE_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_OPE_TEMPLATE_STORE( store ), NULL );
		g_debug( "%s: returning existing store=%p", thisfn, ( void * ) store );

	} else {
		store = g_object_new( OFA_TYPE_OPE_TEMPLATE_STORE, NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), OPE_TEMPLATE_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );

		connect_to_hub_signaling_system( store, hub );

		g_debug( "%s: returning newly allocated store=%p", thisfn, ( void * ) store );
	}

	return( store );
}

/*
 * sorting the self per account number
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplateStore *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, OPE_TEMPLATE_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, OPE_TEMPLATE_COL_MNEMO, &bmnemo, -1 );

	cmp = g_utf8_collate( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

/*
 * load the dataset when columns and dossier have been both set
 */
static void
list_store_load_dataset( ofaListStore *self, ofaHub *hub )
{
	const GList *dataset, *it;
	ofoOpeTemplate *ope;

	dataset = ofo_ope_template_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		ope = OFO_OPE_TEMPLATE( it->data );
		insert_row( OFA_OPE_TEMPLATE_STORE( self ), hub, ope );
	}
}

static void
insert_row( ofaOpeTemplateStore *self, ofaHub *hub, const ofoOpeTemplate *ope )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( self ),
			&iter,
			-1,
			OPE_TEMPLATE_COL_MNEMO, ofo_ope_template_get_mnemo( ope ),
			OPE_TEMPLATE_COL_OBJECT, ope,
			-1 );

	set_row( self, hub, ope, &iter );
}

static void
set_row( ofaOpeTemplateStore *self, ofaHub *hub, const ofoOpeTemplate *ope, GtkTreeIter *iter )
{
	gchar *stamp;

	stamp  = my_utils_stamp_to_str( ofo_ope_template_get_upd_stamp( ope ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			OPE_TEMPLATE_COL_LABEL,     ofo_ope_template_get_label( ope ),
			OPE_TEMPLATE_COL_LEDGER,    ofo_ope_template_get_ledger( ope ),
			OPE_TEMPLATE_COL_NOTES,     ofo_ope_template_get_notes( ope ),
			OPE_TEMPLATE_COL_UPD_USER,  ofo_ope_template_get_upd_user( ope ),
			OPE_TEMPLATE_COL_UPD_STAMP, stamp,
			OPE_TEMPLATE_COL_RECURRENT, "",
			-1 );

	g_free( stamp );
}

/*
 * find_row_by_mnemo:
 * @self: this #ofaOpeTemplateStore
 * @number: [in]: the account number we are searching for.
 * @iter: [out]: the last iter equal or immediately greater than the
 *  searched value.
 * @bvalid: [allow-none][out]: set to TRUE if the returned iter is
 *  valid.
 *
 * rows are sorted by mnemonic
 * we exit the search as soon as we get a number greater than the
 * searched one
 *
 * Returns TRUE if we have found an exact match, and @iter addresses
 * this exact match.
 *
 * Returns FALSE if we do not have found a match, and @iter addresses
 * the first number greater than the searched value.
 */
static gboolean
find_row_by_mnemo( ofaOpeTemplateStore *self, const gchar *mnemo, GtkTreeIter *iter, gboolean *bvalid )
{
	gchar *cmp_mnemo;
	gint cmp;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		if( bvalid ){
			*bvalid = TRUE;
		}
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, OPE_TEMPLATE_COL_MNEMO, &cmp_mnemo, -1 );
			cmp = g_utf8_collate( cmp_mnemo, mnemo );
			g_free( cmp_mnemo );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( cmp > 0 ){
				return( FALSE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				if( bvalid ){
					*bvalid = FALSE;
				}
				return( FALSE );
			}
		}
	}

	return( FALSE );
}

static void
remove_row_by_mnemo( ofaOpeTemplateStore *self, const gchar *number )
{
	GtkTreeIter iter;

	if( find_row_by_mnemo( self, number, &iter, NULL )){
		gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
	}
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this self is equal to those of the dossier
 */
static void
connect_to_hub_signaling_system( ofaOpeTemplateStore *self, ofaHub *hub )
{
	ofaOpeTemplateStorePrivate *priv;
	gulong handler;

	priv = ofa_ope_template_store_get_instance_private( self );

	priv->hub = hub;

	handler = g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		insert_row( self, hub, OFO_OPE_TEMPLATE( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_hub_updated_object";
	GtkTreeIter iter;
	const gchar *mnemo, *new_id;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));

		if( prev_id && g_utf8_collate( prev_id, mnemo )){
			remove_row_by_mnemo( self, prev_id );
			insert_row( self, hub, OFO_OPE_TEMPLATE( object ));

		} else if( find_row_by_mnemo( self, mnemo, &iter, NULL )){
			set_row( self, hub, OFO_OPE_TEMPLATE( object ), &iter);

		} else {
			g_debug( "%s: not found: mnemo=%s", thisfn, mnemo );
		}

	} else if( OFO_IS_ACCOUNT( object )){
		new_id = ofo_account_get_number( OFO_ACCOUNT( object ));
		if( prev_id && g_utf8_collate( prev_id, new_id )){
			hub_on_updated_account( self, prev_id, new_id );
		}
	}
}

static void
hub_on_updated_account( ofaOpeTemplateStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;
	ofoOpeTemplate *template;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, OPE_TEMPLATE_COL_OBJECT, &template, -1 );
			g_return_if_fail( template && OFO_IS_OPE_TEMPLATE( template ));
			g_object_unref( template );

			ofo_ope_template_update_account( template, prev_id, new_id );

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_hub_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		remove_row_by_mnemo( self, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_hub_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_OPE_TEMPLATE ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		list_store_load_dataset( OFA_LIST_STORE( self ), hub );
	}
}

/**
 * ofa_ope_template_store_get_by_mnemo:
 * @store:
 * @mnemo:
 * @iter: [out]:
 *
 * Set the iter to the specified row.
 *
 * Returns: %TRUE if found.
 */
gboolean
ofa_ope_template_store_get_by_mnemo( ofaOpeTemplateStore *store, const gchar *mnemo, GtkTreeIter *iter )
{
	ofaOpeTemplateStorePrivate *priv;
	gboolean found;

	g_return_val_if_fail( store && OFA_IS_OPE_TEMPLATE_STORE( store ), FALSE );

	priv = ofa_ope_template_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	found = find_row_by_mnemo( store, mnemo, iter, NULL );

	return( found );
}
