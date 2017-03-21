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

#include "my/my-date.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"

#include "tva/ofa-tva-record-store.h"
#include "tva/ofo-tva-form.h"
#include "tva/ofo-tva-record.h"

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
	ofaTVARecordStorePrivate;

static GType st_col_types[TVA_RECORD_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* mnemo, label, correspondence */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* begin, end, is_validated */
		G_TYPE_STRING,									/* dope */
		G_TYPE_STRING, 0,								/* notes, notes_png */
		G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
		G_TYPE_OBJECT, G_TYPE_OBJECT					/* the #ofoTVARecord itself, the #ofoTVAForm */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVARecordStore *self );
static void     load_dataset( ofaTVARecordStore *self );
static void     insert_row( ofaTVARecordStore *self, const ofoTVARecord *record );
static void     set_row_by_iter( ofaTVARecordStore *self, const ofoTVARecord *record, GtkTreeIter *iter );
static gboolean find_record_by_key( ofaTVARecordStore *self, const gchar *mnemo, const GDate *end, GtkTreeIter *iter );
static gboolean find_record_by_ptr( ofaTVARecordStore *self, const ofoTVARecord *record, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaTVARecordStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaTVARecordStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaTVARecordStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaTVARecordStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaTVARecordStore *self );

G_DEFINE_TYPE_EXTENDED( ofaTVARecordStore, ofa_tva_record_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaTVARecordStore ))

static void
tva_record_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_store_parent_class )->finalize( instance );
}

static void
tva_record_store_dispose( GObject *instance )
{
	ofaTVARecordStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_STORE( instance ));

	priv = ofa_tva_record_store_get_instance_private( OFA_TVA_RECORD_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_store_parent_class )->dispose( instance );
}

static void
ofa_tva_record_store_init( ofaTVARecordStore *self )
{
	static const gchar *thisfn = "ofa_tva_record_store_init";
	ofaTVARecordStorePrivate *priv;

	g_return_if_fail( OFA_IS_TVA_RECORD_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tva_record_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_tva_record_store_class_init( ofaTVARecordStoreClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_store_finalize;
}

/**
 * ofa_tva_record_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaTVARecordStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaTVARecordStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 *
 * Returns: a new reference to the #ofaTVARecordStore object.
 */
ofaTVARecordStore *
ofa_tva_record_store_new( ofaIGetter *getter )
{
	ofaTVARecordStore *store;
	ofaTVARecordStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaTVARecordStore * ) my_icollector_single_get_object( collector, OFA_TYPE_TVA_RECORD_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_TVA_RECORD_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_TVA_RECORD_STORE, NULL );

		priv = ofa_tva_record_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[TVA_RECORD_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), TVA_RECORD_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( store );
}

/*
 * sorting the self per record code
 * we are sorting by mnemo asc, end date desc
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVARecordStore *self )
{
	ofoTVARecord *aobj, *bobj;
	gchar *aend, *bend;
	gint cmp;

	gtk_tree_model_get( tmodel, a, TVA_RECORD_COL_OBJECT, &aobj, -1 );
	g_object_unref( aobj );

	gtk_tree_model_get( tmodel, b, TVA_RECORD_COL_OBJECT, &bobj, -1 );
	g_object_unref( bobj );

	cmp = my_collate( ofo_tva_record_get_mnemo( aobj ), ofo_tva_record_get_mnemo( bobj ));

	if( cmp == 0 ){
		aend = my_date_to_str( ofo_tva_record_get_end( aobj ), MY_DATE_SQL );
		bend = my_date_to_str( ofo_tva_record_get_end( bobj ), MY_DATE_SQL );
		cmp = -1 * my_collate( aend, bend );
		g_free( aend );
		g_free( bend );
	}

	return( cmp );
}

static void
load_dataset( ofaTVARecordStore *self )
{
	ofaTVARecordStorePrivate *priv;
	const GList *dataset, *it;
	ofoTVARecord *record;

	priv = ofa_tva_record_store_get_instance_private( self );

	dataset = ofo_tva_record_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		record = OFO_TVA_RECORD( it->data );
		insert_row( self, record );
	}
}

static void
insert_row( ofaTVARecordStore *self, const ofoTVARecord *record )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, record, &iter );
}

static void
set_row_by_iter( ofaTVARecordStore *self, const ofoTVARecord *record, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_tva_record_store_set_row_by_iter";
	ofaTVARecordStorePrivate *priv;
	const gchar *cvalidated;
	gchar *sbegin, *send, *sdope, *stamp;
	const gchar *notes;
	ofoTVAForm *form;
	GError *error;
	GdkPixbuf *notes_png;

	priv = ofa_tva_record_store_get_instance_private( self );

	form = ofo_tva_form_get_by_mnemo( priv->getter, ofo_tva_record_get_mnemo( record ));
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	sbegin = my_date_to_str( ofo_tva_record_get_begin( record ), ofa_prefs_date_display( priv->getter ));
	send = my_date_to_str( ofo_tva_record_get_end( record ), ofa_prefs_date_display( priv->getter ));
	cvalidated = ofo_tva_record_get_is_validated( record ) ? _( "Yes" ) : _( "No" );
	sdope = my_date_to_str( ofo_tva_record_get_dope( record ), ofa_prefs_date_display( priv->getter ));

	notes = ofo_tva_record_get_notes( record );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	stamp  = my_stamp_to_str( ofo_tva_form_get_upd_stamp( form ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			TVA_RECORD_COL_MNEMO,          ofo_tva_record_get_mnemo( record ),
			TVA_RECORD_COL_LABEL,          ofo_tva_record_get_label( record ),
			TVA_RECORD_COL_CORRESPONDENCE, ofo_tva_record_get_correspondence( record ),
			TVA_RECORD_COL_BEGIN,          sbegin,
			TVA_RECORD_COL_END,            send,
			TVA_RECORD_COL_IS_VALIDATED,   cvalidated,
			TVA_RECORD_COL_DOPE,           sdope,
			TVA_RECORD_COL_NOTES,          notes,
			TVA_RECORD_COL_NOTES_PNG,      notes_png,
			TVA_RECORD_COL_UPD_USER,       ofo_tva_form_get_upd_user( form ),
			TVA_RECORD_COL_UPD_STAMP,      stamp,
			TVA_RECORD_COL_OBJECT,         record,
			TVA_RECORD_COL_FORM,           form,
			-1 );

	g_free( sbegin );
	g_free( send );
	g_free( sdope );
	g_free( stamp );
}

static gboolean
find_record_by_key( ofaTVARecordStore *self, const gchar *mnemo, const GDate *end, GtkTreeIter *iter )
{
	ofoTVARecord *iter_obj;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, TVA_RECORD_COL_OBJECT, &iter_obj, -1 );
			g_object_unref( iter_obj );
			cmp = ofo_tva_record_compare_by_key( iter_obj, mnemo, end );
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

static gboolean
find_record_by_ptr( ofaTVARecordStore *self, const ofoTVARecord *record, GtkTreeIter *iter )
{
	ofoTVARecord *iter_obj;
	const gchar *mnemo;
	const GDate *dend;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		mnemo = ofo_tva_record_get_mnemo( record );
		dend = ofo_tva_record_get_end( record );
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, TVA_RECORD_COL_OBJECT, &iter_obj, -1 );
			g_object_unref( iter_obj );
			cmp = ofo_tva_record_compare_by_key( iter_obj, mnemo, dend );
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
signaler_connect_to_signaling_system( ofaTVARecordStore *self )
{
	ofaTVARecordStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_tva_record_store_get_instance_private( self );

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
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaTVARecordStore *self )
{
	static const gchar *thisfn = "ofa_tva_record_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_TVA_RECORD( object )){
		insert_row( self, OFO_TVA_RECORD( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaTVARecordStore *self )
{
	static const gchar *thisfn = "ofa_tva_record_store_signaler_on_updated_base";
	GtkTreeIter iter;
	const gchar *mnemo;
	const GDate *dend;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_TVA_RECORD( object )){
		mnemo = ofo_tva_record_get_mnemo( OFO_TVA_RECORD( object ));
		dend = ofo_tva_record_get_end( OFO_TVA_RECORD( object ));
		if( find_record_by_key( self, mnemo, dend, &iter )){
			set_row_by_iter( self, OFO_TVA_RECORD( object ), &iter);
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaTVARecordStore *self )
{
	static const gchar *thisfn = "ofa_tva_record_store_signaler_on_deleted_base";
	GtkTreeIter iter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_TVA_RECORD( object )){
		if( find_record_by_ptr( self, OFO_TVA_RECORD( object ), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaTVARecordStore *self )
{
	static const gchar *thisfn = "ofa_tva_record_store_signaler_on_reload_collection";

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	if( type == OFO_TYPE_TVA_RECORD ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
