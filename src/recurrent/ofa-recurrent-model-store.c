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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofo-ope-template.h"

#include "recurrent/ofa-recurrent-model-store.h"
#include "recurrent/ofo-rec-period.h"
#include "recurrent/ofo-recurrent-model.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* runtime
	 */
	ofaHub   *hub;
	GList    *hub_handlers;
}
	ofaRecurrentModelStorePrivate;

static GType st_col_types[REC_MODEL_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* mnemo, label, ope_template */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_LONG,		/* periodicity, detail, detail_i */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* def_amount1,def_amount2,def_amount3 */
		G_TYPE_STRING, G_TYPE_BOOLEAN,					/* enabled_str, enabled_bool */
		G_TYPE_STRING, 0, G_TYPE_STRING,				/* notes, notes_png, upd_user */
		G_TYPE_STRING,									/* upd_stamp */
		G_TYPE_OBJECT									/* the #ofoRecurrentModel itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/recurrent/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/recurrent/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentModelStore *self );
static void     load_dataset( ofaRecurrentModelStore *self );
static void     insert_row( ofaRecurrentModelStore *self, const ofoRecurrentModel *model );
static void     set_row_by_iter( ofaRecurrentModelStore *self, const ofoRecurrentModel *model, GtkTreeIter *iter );
static gboolean model_find_by_mnemo( ofaRecurrentModelStore *self, const gchar *code, GtkTreeIter *iter );
static void     remove_row_by_mnemo( ofaRecurrentModelStore *self, const gchar *mnemo );
static void     hub_connect_to_signaling_system( ofaRecurrentModelStore *self );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentModelStore *self );
static void     hub_on_updated_ope_template_mnemo( ofaRecurrentModelStore *self, const gchar *prev_mnemo, const gchar *new_mnemo );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentModelStore *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelStore, ofa_recurrent_model_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaRecurrentModelStore ))

static void
recurrent_model_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_store_parent_class )->finalize( instance );
}

static void
recurrent_model_store_dispose( GObject *instance )
{
	ofaRecurrentModelStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_STORE( instance ));

	priv = ofa_recurrent_model_store_get_instance_private( OFA_RECURRENT_MODEL_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_store_parent_class )->dispose( instance );
}

static void
ofa_recurrent_model_store_init( ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_init";
	ofaRecurrentModelStorePrivate *priv;

	g_return_if_fail( OFA_IS_RECURRENT_MODEL_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_recurrent_model_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
}

static void
ofa_recurrent_model_store_class_init( ofaRecurrentModelStoreClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_model_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_model_store_finalize;
}

/**
 * ofa_recurrent_model_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaRecurrentModelStore and attached it to the @hub
 * if not already done. Else get the already allocated #ofaRecurrentModelStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 *
 * Note that the #myICollector associated to the @hub maintains its own
 * reference to the #ofaRecurrentModelStore object, reference which will
 * be freed on @hub finalization.
 *
 * Returns: a new reference to the #ofaRecurrentStore object.
 */
ofaRecurrentModelStore *
ofa_recurrent_model_store_new( ofaHub *hub )
{
	ofaRecurrentModelStore *store;
	ofaRecurrentModelStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	collector = ofa_hub_get_collector( hub );
	store = ( ofaRecurrentModelStore * ) my_icollector_single_get_object( collector, OFA_TYPE_RECURRENT_MODEL_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_RECURRENT_MODEL_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_RECURRENT_MODEL_STORE, NULL );

		priv = ofa_recurrent_model_store_get_instance_private( store );
		priv->hub = hub;

		st_col_types[REC_MODEL_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), REC_MODEL_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		hub_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( store );
}

/*
 * sorting the store per model code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentModelStore *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, REC_MODEL_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, REC_MODEL_COL_MNEMO, &bmnemo, -1 );

	cmp = my_collate( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

static void
load_dataset( ofaRecurrentModelStore *self )
{
	ofaRecurrentModelStorePrivate *priv;
	const GList *dataset, *it;
	ofoRecurrentModel *model;

	priv = ofa_recurrent_model_store_get_instance_private( self );

	dataset = ofo_recurrent_model_get_dataset( priv->hub );

	for( it=dataset ; it ; it=it->next ){
		model = OFO_RECURRENT_MODEL( it->data );
		insert_row( self, model );
	}
}

static void
insert_row( ofaRecurrentModelStore *self, const ofoRecurrentModel *model )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, model, &iter );
}

static void
set_row_by_iter( ofaRecurrentModelStore *self, const ofoRecurrentModel *model, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_set_row";
	ofaRecurrentModelStorePrivate *priv;
	gchar *stamp;
	const gchar *csper, *csdet, *csdef1, *csdef2, *csdef3, *cenabled;
	const gchar *notes;
	GError *error;
	GdkPixbuf *notes_png;
	gboolean is_enabled;
	ofxCounter perdetid;
	ofoRecPeriod *period;
	gint idx;

	priv = ofa_recurrent_model_store_get_instance_private( self );

	stamp  = my_utils_stamp_to_str( ofo_recurrent_model_get_upd_stamp( model ), MY_STAMP_DMYYHM );

	csper = ofo_recurrent_model_get_periodicity( model );
	period = ofo_rec_period_get_by_id( priv->hub, csper );

	csdet = "";
	perdetid = 0;
	if( period && ofo_rec_period_get_details_count( period ) > 0 ){
		perdetid = ofo_recurrent_model_get_periodicity_detail( model );
		idx = ofo_rec_period_detail_get_by_id( period, perdetid );
		if( idx >= 0 ){
			csdet = ofo_rec_period_detail_get_label( period, idx );
		}
	}

	csdef1 = ofo_recurrent_model_get_def_amount1( model );
	csdef2 = ofo_recurrent_model_get_def_amount2( model );
	csdef3 = ofo_recurrent_model_get_def_amount3( model );
	is_enabled = ofo_recurrent_model_get_is_enabled( model );
	cenabled = is_enabled ? _( "Yes" ) : _( "No" );

	notes = ofo_recurrent_model_get_notes( model );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			REC_MODEL_COL_MNEMO,              ofo_recurrent_model_get_mnemo( model ),
			REC_MODEL_COL_LABEL,              ofo_recurrent_model_get_label( model ),
			REC_MODEL_COL_OPE_TEMPLATE,       ofo_recurrent_model_get_ope_template( model ),
			REC_MODEL_COL_PERIODICITY,        csper,
			REC_MODEL_COL_PERIODICITY_DETAIL, csdet,
			REC_MODEL_COL_PERIOD_DETAIL_I,    perdetid,
			REC_MODEL_COL_DEF_AMOUNT1,        csdef1 ? csdef1 : "",
			REC_MODEL_COL_DEF_AMOUNT2,        csdef2 ? csdef2 : "",
			REC_MODEL_COL_DEF_AMOUNT3,        csdef3 ? csdef3 : "",
			REC_MODEL_COL_ENABLED,            cenabled,
			REC_MODEL_COL_ENABLED_B,          is_enabled,
			REC_MODEL_COL_NOTES,              notes,
			REC_MODEL_COL_NOTES_PNG,          notes_png,
			REC_MODEL_COL_UPD_USER,           ofo_recurrent_model_get_upd_user( model ),
			REC_MODEL_COL_UPD_STAMP,          stamp,
			REC_MODEL_COL_OBJECT,             model,
			-1 );

	g_object_unref( notes_png );
	g_free( stamp );
}

static gboolean
model_find_by_mnemo( ofaRecurrentModelStore *self, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, REC_MODEL_COL_MNEMO, &str, -1 );
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

static void
remove_row_by_mnemo( ofaRecurrentModelStore *self, const gchar *mnemo )
{
	GtkTreeIter iter;

	if( model_find_by_mnemo( self, mnemo, &iter )){
		gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
	}
}

/**
 * ofa_recurrent_model_store_get_iter:
 * @store: this #ofaRecurrentModelStore instance.
 * @model: the searched #ofoRecurrentModel object.
 * @iter: [out]: the iterator.
 *
 * Returns: %TRUE if the @model has been found in the store, and @iter
 * is set to the corresponding row.
 */
gboolean
ofa_recurrent_model_store_get_iter( ofaRecurrentModelStore *store, ofoRecurrentModel *model, GtkTreeIter *iter )
{
	ofaRecurrentModelStorePrivate *priv;

	g_return_val_if_fail( store && OFA_IS_RECURRENT_MODEL_STORE( store ), FALSE );
	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), FALSE );

	priv = ofa_recurrent_model_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( model_find_by_mnemo( store, ofo_recurrent_model_get_mnemo( model ), iter ));
}

/*
 * connect to the hub signaling system
 */
static void
hub_connect_to_signaling_system( ofaRecurrentModelStore *self )
{
	ofaRecurrentModelStorePrivate *priv;
	gulong handler;

	priv = ofa_recurrent_model_store_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		insert_row( self, OFO_RECURRENT_MODEL( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_updated_object";
	GtkTreeIter iter;
	const gchar *code, *new_code, *new_mnemo;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		new_code = ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object ));
		code = prev_id ? prev_id : new_code;
		if( model_find_by_mnemo( self, code, &iter )){
			set_row_by_iter( self, OFO_RECURRENT_MODEL( object ), &iter);
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		new_mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
		if( my_strlen( prev_id ) && my_collate( prev_id, new_mnemo )){
			hub_on_updated_ope_template_mnemo( self, prev_id, new_mnemo );
		}
	}
}

/*
 * Update all models to the new ope template mnemo
 * Which means updating the store + updating the corresponding object
 * Iter on all rows because several models may share same ope template
 */
static void
hub_on_updated_ope_template_mnemo( ofaRecurrentModelStore *self, const gchar *prev_mnemo, const gchar *new_mnemo )
{
	GtkTreeIter iter;
	ofoRecurrentModel *model;
	gchar *stored_mnemo;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter,
					REC_MODEL_COL_OPE_TEMPLATE, &stored_mnemo, REC_MODEL_COL_OBJECT, &model, -1 );

			g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));
			g_object_unref( model );
			cmp = g_utf8_collate( stored_mnemo, prev_mnemo );
			g_free( stored_mnemo );

			if( cmp == 0 ){
				ofo_recurrent_model_set_ope_template( model, new_mnemo );
				gtk_list_store_set( GTK_LIST_STORE( self ), &iter, REC_MODEL_COL_OPE_TEMPLATE, new_mnemo, -1 );
			}

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
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RECURRENT_MODEL( object )){
		remove_row_by_mnemo( self, ofo_recurrent_model_get_mnemo( OFO_RECURRENT_MODEL( object )));
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecurrentModelStore *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_store_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_RECURRENT_MODEL ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
