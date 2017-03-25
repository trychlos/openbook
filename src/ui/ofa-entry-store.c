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

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-concil-id.h"

#include "ofa-entry-store.h"
#include "core/ofa-iconcil.h"

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
	ofaEntryStorePrivate;

/* store data types
 */
static GType st_col_types[ENTRY_N_COLUMNS] = {
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* dope, deffect, label */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ref, currency, ledger */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ope_template, account, debit */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* credit, ope_number, stlmt_number */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* stlmt_user, stlmt_stamp, ent_number_str */
	G_TYPE_ULONG,										/* ent_number_int */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* upd_user, upd_stamp, concil_number */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,			/* concil_date, status_str, status_int */
	G_TYPE_OBJECT,										/* the #ofoEntry itself */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, 		/* msgerr, msgwarn, dope_set */
	G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,						/* deffect_set, currency_set */
	G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, 0			/* rule_int, rule, notes, notes_png */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     load_dataset( ofaEntryStore *store );
static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaEntryStore *self );
static void     insert_row( ofaEntryStore *self, const ofoEntry *entry );
static void     set_row_by_iter( ofaEntryStore *self, const ofoEntry *entry, GtkTreeIter *iter );
static void     set_row_concil( ofaEntryStore *self, ofoConcil *concil, GtkTreeIter *iter );
static gboolean find_row_by_number( ofaEntryStore *self, ofxCounter number, GtkTreeIter *iter );
static void     do_update_concil( ofaEntryStore *self, ofoConcil *concil, gboolean is_deleted );
static void     set_account_new_id( ofaEntryStore *self, const gchar *prev, const gchar *number );
static void     set_currency_new_id( ofaEntryStore *self, const gchar *prev, const gchar *code );
static void     set_ledger_new_id( ofaEntryStore *self, const gchar *prev, const gchar *mnemo );
static void     set_ope_template_new_id( ofaEntryStore *self, const gchar *prev, const gchar *mnemo );
static void     signaler_connect_to_signaling_system( ofaEntryStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaEntryStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaEntryStore *self );
static void     signaler_on_updated_entry( ofaEntryStore *self, ofoEntry *entry );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaEntryStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaEntryStore *self );

G_DEFINE_TYPE_EXTENDED( ofaEntryStore, ofa_entry_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaEntryStore ))

static void
entry_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ENTRY_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_store_parent_class )->finalize( instance );
}

static void
entry_store_dispose( GObject *instance )
{
	ofaEntryStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_ENTRY_STORE( instance ));

	priv = ofa_entry_store_get_instance_private( OFA_ENTRY_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_store_parent_class )->dispose( instance );
}

static void
ofa_entry_store_init( ofaEntryStore *self )
{
	static const gchar *thisfn = "ofa_entry_store_init";
	ofaEntryStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ENTRY_STORE( self ));

	priv = ofa_entry_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_entry_store_class_init( ofaEntryStoreClass *klass )
{
	static const gchar *thisfn = "ofa_entry_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_store_finalize;

	/**
	 * ofaEntryStore::ofa-changed:
	 *
	 * #ofaEntryStore sends a 'ofa-changed' signal after having treated
	 * an ofaISignaler update. It is time for the view to update itself.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryStore *store,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ENTRY_STORE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_entry_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new reference to the #ofaEntryStore, which should be
 * released by the caller.
 */
ofaEntryStore *
ofa_entry_store_new( ofaIGetter *getter )
{
	ofaEntryStore *store;
	ofaEntryStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaEntryStore * ) my_icollector_single_get_object( collector, OFA_TYPE_ENTRY_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_ENTRY_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_ENTRY_STORE, NULL );

		priv = ofa_entry_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[ENTRY_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), ENTRY_N_COLUMNS, st_col_types );

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
 * sorting the store per entry number ascending
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaEntryStore *self )
{
	ofxCounter numa, numb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, ENTRY_COL_ENT_NUMBER_I, &numa, -1 );
	gtk_tree_model_get( tmodel, b, ENTRY_COL_ENT_NUMBER_I, &numb, -1 );

	cmp = numa < numb ? -1 : ( numa > numb ? 1 : 0 );

	return( cmp );
}

/*
 * Loads the dataset.
 */
static void
load_dataset( ofaEntryStore *store )
{
	ofaEntryStorePrivate *priv;
	GList *dataset, *it;
	ofoEntry *entry;

	priv = ofa_entry_store_get_instance_private( store );

	dataset = ofo_entry_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		insert_row( store, entry );
	}
}

static void
insert_row( ofaEntryStore *self, const ofoEntry *entry )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( self ),
			&iter,
			-1,
			ENTRY_COL_ENT_NUMBER_I, ofo_entry_get_number( entry ),
			ENTRY_COL_OBJECT,       entry ,
			-1 );

	set_row_by_iter( self, entry, &iter );
}

static void
set_row_by_iter( ofaEntryStore *self, const ofoEntry *entry, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_entry_store_set_row_by_iter";
	ofaEntryStorePrivate *priv;
	gchar *sdope, *sdeff, *sdeb, *scre, *sopenum, *ssetnum, *ssetstamp, *sentnum, *supdstamp;
	const gchar *cstr, *cref, *cur_code, *csetuser, *cupduser, *notes;
	ofoCurrency *cur_obj;
	ofxAmount amount;
	ofxCounter counter;
	ofoConcil *concil;
	GdkPixbuf *notes_png;
	GError *error;
	ofeEntryStatus status;
	ofeEntryRule rule;

	priv = ofa_entry_store_get_instance_private( self );

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display( priv->getter ));
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_display( priv->getter ));

	cstr = ofo_entry_get_ref( entry );
	cref = cstr ? cstr : "";

	cur_code = ofo_entry_get_currency( entry );
	g_return_if_fail( my_strlen( cur_code ));
	cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
	g_return_if_fail( cur_obj && OFO_IS_CURRENCY( cur_obj ));

	amount = ofo_entry_get_debit( entry );
	sdeb = amount ? ofa_amount_to_str( amount, cur_obj, priv->getter ) : g_strdup( "" );
	amount = ofo_entry_get_credit( entry );
	scre = amount ? ofa_amount_to_str( amount, cur_obj, priv->getter ) : g_strdup( "" );

	counter = ofo_entry_get_ope_number( entry );
	sopenum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	counter = ofo_entry_get_settlement_number( entry );
	ssetnum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	cstr = ofo_entry_get_settlement_user( entry );
	csetuser = cstr ? cstr : "";
	ssetstamp = my_stamp_to_str( ofo_entry_get_settlement_stamp( entry ), MY_STAMP_DMYYHM );

	sentnum = g_strdup_printf( "%lu", ofo_entry_get_number( entry ));

	cstr = ofo_entry_get_upd_user( entry );
	cupduser = cstr ? cstr : "";
	supdstamp = my_stamp_to_str( ofo_entry_get_upd_stamp( entry ), MY_STAMP_DMYYHM );

	status = ofo_entry_get_status( entry );
	rule = ofo_entry_get_rule( entry );

	error = NULL;
	notes = ofo_entry_get_notes( entry );
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
				GTK_LIST_STORE( self ),
				iter,
				ENTRY_COL_DOPE,          sdope,
				ENTRY_COL_DEFFECT,       sdeff,
				ENTRY_COL_LABEL,         ofo_entry_get_label( entry ),
				ENTRY_COL_REF,           cref,
				ENTRY_COL_CURRENCY,      cur_code,
				ENTRY_COL_LEDGER,        ofo_entry_get_ledger( entry ),
				ENTRY_COL_OPE_TEMPLATE,  ofo_entry_get_ope_template( entry ),
				ENTRY_COL_ACCOUNT,       ofo_entry_get_account( entry ),
				ENTRY_COL_DEBIT,         sdeb,
				ENTRY_COL_CREDIT,        scre,
				ENTRY_COL_OPE_NUMBER,    sopenum,
				ENTRY_COL_STLMT_NUMBER,  ssetnum,
				ENTRY_COL_STLMT_USER,    csetuser,
				ENTRY_COL_STLMT_STAMP,   ssetstamp,
				ENTRY_COL_ENT_NUMBER,    sentnum,
				ENTRY_COL_UPD_USER,      cupduser,
				ENTRY_COL_UPD_STAMP,     supdstamp,
				ENTRY_COL_CONCIL_NUMBER, "",
				ENTRY_COL_CONCIL_DATE,   "",
				ENTRY_COL_STATUS,        ofo_entry_status_get_abr( status ),
				ENTRY_COL_STATUS_I,      status,
				ENTRY_COL_OBJECT,        entry,
				ENTRY_COL_MSGERR,        "",
				ENTRY_COL_MSGWARN,       "",
				ENTRY_COL_DOPE_SET,      FALSE,
				ENTRY_COL_DEFFECT_SET,   FALSE,
				ENTRY_COL_CURRENCY_SET,  FALSE,
				ENTRY_COL_RULE_I,        rule,
				ENTRY_COL_RULE,          ofo_entry_rule_get_abr( rule ),
				ENTRY_COL_NOTES,         notes,
				ENTRY_COL_NOTES_PNG,     notes_png,
				-1 );

	g_free( supdstamp );
	g_free( sentnum );
	g_free( ssetstamp );
	g_free( ssetnum );
	g_free( sopenum );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	if( concil ){
		set_row_concil( self, concil, iter );
	}
}

/*
 * iter is on the list store
 */
static void
set_row_concil( ofaEntryStore *self, ofoConcil *concil, GtkTreeIter *iter )
{
	ofaEntryStorePrivate *priv;
	gchar *srappro, *snum;

	priv = ofa_entry_store_get_instance_private( self );

	srappro = concil ?
				my_date_to_str( ofo_concil_get_dval( concil ), ofa_prefs_date_display( priv->getter )) :
				g_strdup( "" );
	snum = concil ?
				g_strdup_printf( "%lu", ofo_concil_get_id( concil )) :
				g_strdup( "" );

	gtk_list_store_set(
				GTK_LIST_STORE( self ),
				iter,
				ENTRY_COL_CONCIL_NUMBER, snum,
				ENTRY_COL_CONCIL_DATE,   srappro,
				-1 );

	g_free( snum );
	g_free( srappro );
}

/*
 * find_row_by_number:
 * @store: this #ofaEntryStore
 * @number: [in]: the entry number we are searching for.
 * @iter: [out]: the found iter if %TRUE.
 *
 * Rows are sorted by entry number.
 * We exit the search as soon as we get a number greater than the
 * searched one, or the end of the list.
 *
 * Returns TRUE if we have found an exact match, and @iter addresses
 * this exact match.
 */
static gboolean
find_row_by_number( ofaEntryStore *self, ofxCounter number, GtkTreeIter *iter )
{
	ofxCounter row_number;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, ENTRY_COL_ENT_NUMBER_I, &row_number, -1 );
			if( row_number == number ){
				return( TRUE );
			}
			if( row_number > number ){
				break;
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * a conciliation group is updated
 * -> update the entry row if needed
 */
static void
do_update_concil( ofaEntryStore *self, ofoConcil *concil, gboolean is_deleted )
{
	GList *ids, *it;
	ofsConcilId *sid;
	GtkTreeIter iter;

	ids = ofo_concil_get_ids( concil );
	for( it=ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( !g_strcmp0( sid->type, CONCIL_TYPE_ENTRY ) &&
				find_row_by_number( self, sid->other_id, &iter )){
			set_row_concil( self, is_deleted ? NULL : concil, &iter );
		}
	}
}

static void
set_account_new_id( ofaEntryStore *self, const gchar *prev, const gchar *number )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, ENTRY_COL_ACCOUNT, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( self ), &iter, ENTRY_COL_ACCOUNT, number, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

static void
set_currency_new_id( ofaEntryStore *self, const gchar *prev, const gchar *code )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, ENTRY_COL_CURRENCY, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( self ), &iter, ENTRY_COL_CURRENCY, code, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

static void
set_ledger_new_id( ofaEntryStore *self, const gchar *prev, const gchar *mnemo )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, ENTRY_COL_LEDGER, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( self ), &iter, ENTRY_COL_LEDGER, mnemo, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

static void
set_ope_template_new_id( ofaEntryStore *self, const gchar *prev, const gchar *mnemo )
{
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, ENTRY_COL_OPE_TEMPLATE, &str, -1 );
			cmp = g_utf8_collate( str, prev );
			g_free( str );
			if( cmp == 0 ){
				gtk_list_store_set( GTK_LIST_STORE( self ), &iter, ENTRY_COL_OPE_TEMPLATE, mnemo, -1 );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaEntryStore *self )
{
	ofaEntryStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_entry_store_get_instance_private( self );

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
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaEntryStore *self )
{
	static const gchar *thisfn = "ofa_entry_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ENTRY( object )){
		insert_row( self, OFO_ENTRY( object ));
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaEntryStore *self )
{
	static const gchar *thisfn = "ofa_entry_store_signaler_on_updated_base";

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( prev_id ){
		if( OFO_IS_ACCOUNT( object )){
			set_account_new_id( self, prev_id, ofo_account_get_number( OFO_ACCOUNT( object )));

		} else if( OFO_IS_CURRENCY( object )){
			set_currency_new_id( self, prev_id, ofo_currency_get_code( OFO_CURRENCY( object )));

		} else if( OFO_IS_LEDGER( object )){
			set_ledger_new_id( self, prev_id, ofo_ledger_get_mnemo( OFO_LEDGER( object )));

		} else if( OFO_IS_OPE_TEMPLATE( object )){
			set_ope_template_new_id( self, prev_id, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
		}
	} else if( OFO_IS_CONCIL( object )){
		do_update_concil( self, OFO_CONCIL( object ), FALSE );

	} else if( OFO_IS_ENTRY( object )){
		signaler_on_updated_entry( self, OFO_ENTRY( object ));
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
signaler_on_updated_entry( ofaEntryStore *self, ofoEntry *entry )
{
	GtkTreeIter iter;

	if( find_row_by_number( self, ofo_entry_get_number( entry ), &iter )){
		set_row_by_iter( self, entry, &iter );
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaEntryStore *self )
{
	static const gchar *thisfn = "ofa_entry_store_signaler_on_deleted_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CONCIL( object )){
		do_update_concil( self, OFO_CONCIL( object ), TRUE );

	} else if( OFO_IS_ENTRY( object )){
		signaler_on_updated_entry( self, OFO_ENTRY( object ));
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaEntryStore *self )
{
	static const gchar *thisfn = "ofa_entry_store_signaler_on_reload_collection";

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn,
			( void * ) signaler, type, ( void * ) self );

	g_signal_emit_by_name( self, "ofa-changed" );
}
