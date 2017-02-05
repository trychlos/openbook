/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofo-paimean.h"

#include "core/ofa-paimean-store.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	GList      *signaler_handlers;
}
	ofaPaimeanStorePrivate;

static GType st_col_types[PAM_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING,					/* code, label */
		G_TYPE_STRING, G_TYPE_STRING,					/* account, notes */
		0, G_TYPE_STRING, G_TYPE_STRING,				/* notes_png, upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoPaimean itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaPaimeanStore *self );
static void     load_dataset( ofaPaimeanStore *self );
static void     insert_row( ofaPaimeanStore *self, const ofoPaimean *paimean );
static void     set_row_by_iter( ofaPaimeanStore *self, const ofoPaimean *paimean, GtkTreeIter *iter );
static gboolean find_paimean_by_code( ofaPaimeanStore *self, const gchar *code, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaPaimeanStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaPaimeanStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaPaimeanStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaPaimeanStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaPaimeanStore *self );

G_DEFINE_TYPE_EXTENDED( ofaPaimeanStore, ofa_paimean_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaPaimeanStore ))

static void
paimean_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_paimean_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAIMEAN_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_store_parent_class )->finalize( instance );
}

static void
paimean_store_dispose( GObject *instance )
{
	ofaPaimeanStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_PAIMEAN_STORE( instance ));

	priv = ofa_paimean_store_get_instance_private( OFA_PAIMEAN_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_store_parent_class )->dispose( instance );
}

static void
ofa_paimean_store_init( ofaPaimeanStore *self )
{
	static const gchar *thisfn = "ofa_paimean_store_init";
	ofaPaimeanStorePrivate *priv;

	g_return_if_fail( OFA_IS_PAIMEAN_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_paimean_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_paimean_store_class_init( ofaPaimeanStoreClass *klass )
{
	static const gchar *thisfn = "ofa_paimean_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_store_finalize;
}

/**
 * ofa_paimean_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaPaimeanStore and attached it to the
 * #myICollector if not already done. Else get the already allocated
 * #ofaPaimeanStore from this same #myICollector.
 *
 * Returns: a new reference to the #ofaPaimeanStore object.
 */
ofaPaimeanStore *
ofa_paimean_store_new( ofaIGetter *getter )
{
	ofaPaimeanStore *store;
	ofaPaimeanStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaPaimeanStore * ) my_icollector_single_get_object( collector, OFA_TYPE_PAIMEAN_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_PAIMEAN_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_PAIMEAN_STORE, NULL );

		priv = ofa_paimean_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[PAM_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), PAM_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the self by code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaPaimeanStore *self )
{
	gchar *codea, *codeb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, PAM_COL_CODE, &codea, -1 );
	gtk_tree_model_get( tmodel, b, PAM_COL_CODE, &codeb, -1 );

	cmp = my_collate( codea, codeb );

	g_free( codea );
	g_free( codeb );

	return( cmp );
}

static void
load_dataset( ofaPaimeanStore *self )
{
	ofaPaimeanStorePrivate *priv;
	const GList *dataset, *it;
	ofoPaimean *paimean;

	priv = ofa_paimean_store_get_instance_private( self );

	dataset = ofo_paimean_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		paimean = OFO_PAIMEAN( it->data );
		insert_row( self, paimean );
	}
}

static void
insert_row( ofaPaimeanStore *self, const ofoPaimean *paimean )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, paimean, &iter );
}

static void
set_row_by_iter( ofaPaimeanStore *self, const ofoPaimean *paimean, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_paimean_store_set_row";
	gchar *stamp;
	const gchar *notes, *label, *account;
	GError *error;
	GdkPixbuf *notes_png;

	stamp  = my_stamp_to_str( ofo_paimean_get_upd_stamp( paimean ), MY_STAMP_DMYYHM );
	label = ofo_paimean_get_label( paimean );
	account = ofo_paimean_get_account( paimean );

	notes = ofo_paimean_get_notes( paimean );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			PAM_COL_CODE,       ofo_paimean_get_code( paimean ),
			PAM_COL_LABEL,      label ? label : "",
			PAM_COL_ACCOUNT,    account ? account : "",
			PAM_COL_NOTES,      notes,
			PAM_COL_NOTES_PNG,  notes_png,
			PAM_COL_UPD_USER,   ofo_paimean_get_upd_user( paimean ),
			PAM_COL_UPD_STAMP,  stamp,
			PAM_COL_OBJECT,     paimean,
			-1 );

	g_object_unref( notes_png );
	g_free( stamp );
}

static gboolean
find_paimean_by_code( ofaPaimeanStore *self, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, PAM_COL_CODE, &str, -1 );
			cmp = my_collate( str, code );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaPaimeanStore *self )
{
	ofaPaimeanStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_paimean_store_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_DELETED, G_CALLBACK( signaler_on_deleted_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_COLLECTION_RELOAD, G_CALLBACK( signaler_on_reload_collection ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaPaimeanStore *self )
{
	static const gchar *thisfn = "ofa_paimean_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_PAIMEAN( object )){
		insert_row( self, OFO_PAIMEAN( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaPaimeanStore *self )
{
	static const gchar *thisfn = "ofa_paimean_store_signaler_on_updated_base";
	GtkTreeIter iter;
	const gchar *code, *new_code;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_PAIMEAN( object )){
		new_code = ofo_paimean_get_code( OFO_PAIMEAN( object ));
		code = prev_id ? prev_id : new_code;
		if( find_paimean_by_code( self, code, &iter )){
			set_row_by_iter( self, OFO_PAIMEAN( object ), &iter);
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaPaimeanStore *self )
{
	static const gchar *thisfn = "ofa_paimean_store_signaler_on_deleted_base";
	GtkTreeIter iter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_PAIMEAN( object )){
		if( find_paimean_by_code( self, ofo_paimean_get_code( OFO_PAIMEAN( object )), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaPaimeanStore *self )
{
	static const gchar *thisfn = "ofa_paimean_store_signaler_on_reload_collection";

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	if( type == OFO_TYPE_PAIMEAN ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
