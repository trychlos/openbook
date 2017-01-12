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

#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"

#include "recurrent/ofa-rec-period-store.h"
#include "recurrent/ofo-rec-period.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* runtime
	 */
	ofaHub   *hub;
	GList    *hub_handlers;
}
	ofaRecPeriodStorePrivate;

static GType st_col_types[PER_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,		/* id, order, order_i */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,		/* label, details_count, details_count_i */
		G_TYPE_STRING, 0,								/* notes, notes_png */
		G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoRecPeriod itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/recurrent/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/recurrent/notes.png";

static void     load_dataset( ofaRecPeriodStore *self );
static gint     on_sort_model( GtkTreeModel *tperiod, GtkTreeIter *a, GtkTreeIter *b, ofaRecPeriodStore *self );
static void     do_insert_dataset( ofaRecPeriodStore *self, const GList *dataset );
static void     insert_row( ofaRecPeriodStore *self, ofoRecPeriod *period );
static void     set_row_by_iter( ofaRecPeriodStore *self, ofoRecPeriod *period, GtkTreeIter *iter );
static gboolean find_row_by_object( ofaRecPeriodStore *self, ofoRecPeriod *period, GtkTreeIter *iter );
static void     hub_connect_to_signaling_system( ofaRecPeriodStore *self );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecPeriodStore *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecPeriodStore *self );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecPeriodStore *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecPeriodStore *self );

G_DEFINE_TYPE_EXTENDED( ofaRecPeriodStore, ofa_rec_period_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaRecPeriodStore ))

static void
rec_period_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rec_period_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_store_parent_class )->finalize( instance );
}

static void
rec_period_store_dispose( GObject *instance )
{
	ofaRecPeriodStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_STORE( instance ));

	priv = ofa_rec_period_store_get_instance_private( OFA_REC_PERIOD_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_store_parent_class )->dispose( instance );
}

static void
ofa_rec_period_store_init( ofaRecPeriodStore *self )
{
	static const gchar *thisfn = "ofa_rec_period_store_init";
	ofaRecPeriodStorePrivate *priv;

	g_return_if_fail( OFA_IS_REC_PERIOD_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_rec_period_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
}

static void
ofa_rec_period_store_class_init( ofaRecPeriodStoreClass *klass )
{
	static const gchar *thisfn = "ofa_rec_period_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rec_period_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = rec_period_store_finalize;
}

/**
 * ofa_rec_period_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaRecPeriodStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaRecPeriodStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 *
 * Note that the #myICollector associated to the @hub maintains its own
 * reference to the #ofaRecPeriodStore object, reference which will be
 * freed on @hub finalization.
 *
 * Returns: a new reference to the #ofaRecPeriodStore object.
 */
ofaRecPeriodStore *
ofa_rec_period_store_new( ofaHub *hub )
{
	ofaRecPeriodStore *store;
	ofaRecPeriodStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	collector = ofa_hub_get_collector( hub );
	store = ( ofaRecPeriodStore * ) my_icollector_single_get_object( collector, OFA_TYPE_REC_PERIOD_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_REC_PERIOD_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_REC_PERIOD_STORE, NULL );

		priv = ofa_rec_period_store_get_instance_private( store );
		priv->hub = hub;

		st_col_types[PER_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), PER_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		hub_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the store per code
 */
static gint
on_sort_model( GtkTreeModel *trun, GtkTreeIter *a, GtkTreeIter *b, ofaRecPeriodStore *self )
{
	gchar *ida, *idb;
	gint cmp;

	gtk_tree_model_get( trun, a, PER_COL_ID, &ida, -1 );
	gtk_tree_model_get( trun, b, PER_COL_ID, &idb, -1 );

	cmp = my_collate( ida, idb );

	g_free( ida );
	g_free( idb );

	return( cmp );
}

static void
load_dataset( ofaRecPeriodStore *self )
{
	ofaRecPeriodStorePrivate *priv;
	GList *dataset;

	priv = ofa_rec_period_store_get_instance_private( self );

	dataset = ofo_rec_period_get_dataset( priv->hub );
	do_insert_dataset( self, dataset );
}

static void
do_insert_dataset( ofaRecPeriodStore *self, const GList *dataset )
{
	const GList *it;
	ofoRecPeriod *period;

	for( it=dataset ; it ; it=it->next ){
		period = OFO_REC_PERIOD( it->data );
		insert_row( self, period );
	}
}

static void
insert_row( ofaRecPeriodStore *self, ofoRecPeriod *period )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, period, &iter );
}

static void
set_row_by_iter( ofaRecPeriodStore *self, ofoRecPeriod *period, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_rec_period_store_set_row_by_iter";
	const gchar *id, *label, *notes;
	guint order, count;
	gchar *sorder, *scount, *stamp;
	GError *error;
	GdkPixbuf *notes_png;

	id = ofo_rec_period_get_id( period );
	order = ofo_rec_period_get_order( period );
	sorder = g_strdup_printf( "%u", order );
	label = ofo_rec_period_get_label( period );
	count = ofo_rec_period_get_details_count( period );
	scount = g_strdup_printf( "%u", count );

	stamp  = my_stamp_to_str( ofo_rec_period_get_upd_stamp( period ), MY_STAMP_DMYYHM );

	notes = ofo_rec_period_get_notes( period );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			PER_COL_ID,              id,
			PER_COL_ORDER,           sorder,
			PER_COL_ORDER_I,         order,
			PER_COL_LABEL,		     label ? label : "",
			PER_COL_DETAILS_COUNT,   scount,
			PER_COL_DETAILS_COUNT_I, count,
			PER_COL_NOTES,           notes,
			PER_COL_NOTES_PNG,       notes_png,
			PER_COL_UPD_USER,        ofo_rec_period_get_upd_user( period ),
			PER_COL_UPD_STAMP,       stamp,
			PER_COL_OBJECT,          period,
			-1 );

	g_free( sorder );
	g_free( scount );
	g_free( stamp );
}

static gboolean
find_row_by_object( ofaRecPeriodStore *self, ofoRecPeriod *period, GtkTreeIter *iter )
{
	const gchar *period_id;
	gchar *row_id;
	gint cmp;

	period_id = ofo_rec_period_get_id( period );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, PER_COL_ID, &row_id, -1 );
			cmp = my_collate( period_id, row_id );
			g_free( row_id );
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
 * connect to the hub signaling system
 */
static void
hub_connect_to_signaling_system( ofaRecPeriodStore *self )
{
	ofaRecPeriodStorePrivate *priv;
	gulong handler;

	priv = ofa_rec_period_store_get_instance_private( self );

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
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaRecPeriodStore *self )
{
	static const gchar *thisfn = "ofa_rec_period_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_REC_PERIOD( object )){
		insert_row( self, OFO_REC_PERIOD( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaRecPeriodStore *self )
{
	static const gchar *thisfn = "ofa_rec_period_store_hub_on_updated_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_REC_PERIOD( object )){
		if( find_row_by_object( self, OFO_REC_PERIOD( object ), &iter )){
			set_row_by_iter( self, OFO_REC_PERIOD( object ), &iter );
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 *
 * ofaRecPeriod is not expected to be deletable after having been
 * recorded in the DBMS.
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaRecPeriodStore *self )
{
	static const gchar *thisfn = "ofa_rec_period_store_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaRecPeriodStore *self )
{
	static const gchar *thisfn = "ofa_rec_period_store_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_REC_PERIOD ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
