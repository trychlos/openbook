/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-isignaler.h"
#include "api/ofa-itree-adder.h"
#include "api/ofa-ope-template-store.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime data
	 */
	GList      *signaler_handlers;
	gboolean    dataset_is_loaded;
}
	ofaOpeTemplateStorePrivate;

static GType st_col_types[OPE_TEMPLATE_N_COLUMNS] = {
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* mnemo, cre_user, cre_stamp */
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* label, ledger, ledger_locked */
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* ref, ref_locked, ref_mandatory */
		G_TYPE_STRING,										/* pam_row */
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* have_tiers, tiers, tiers_locked */
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* have_qppro, qppro, qppro_locked */
		G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* have_rule, rule, rule_locked */
		G_TYPE_STRING,  0,              G_TYPE_STRING,		/* notes, notes_png, upd_user */
		G_TYPE_STRING,										/* upd_stamp */
		G_TYPE_OBJECT										/* the #ofoOpeTemplate itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplateStore *self );
static void     list_store_v_load_dataset( ofaListStore *self );
static void     insert_row( ofaOpeTemplateStore *self, const ofoOpeTemplate *ope );
static void     set_row_by_iter( ofaOpeTemplateStore *self, const ofoOpeTemplate *ope, GtkTreeIter *iter );
static gboolean find_row_by_mnemo( ofaOpeTemplateStore *self, const gchar *mnemo, GtkTreeIter *iter, gboolean *bvalid );
static void     remove_row_by_mnemo( ofaOpeTemplateStore *self, const gchar *mnemo );
static void     set_account_new_id( ofaOpeTemplateStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_ledger_new_id( ofaOpeTemplateStore *self, const gchar *prev_id, const gchar *new_id );
static void     signaler_connect_to_signaling_system( ofaOpeTemplateStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaOpeTemplateStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaOpeTemplateStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaOpeTemplateStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaOpeTemplateStore *self );

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
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_STORE( instance ));

	priv = ofa_ope_template_store_get_instance_private( OFA_OPE_TEMPLATE_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
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
	priv->signaler_handlers = NULL;
	priv->dataset_is_loaded = FALSE;
}

static void
ofa_ope_template_store_class_init( ofaOpeTemplateStoreClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_store_finalize;

	OFA_LIST_STORE_CLASS( klass )->load_dataset = list_store_v_load_dataset;
}

/**
 * ofa_ope_template_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaOpeTemplateStore and attached it to the
 * #myICollector if not already done. Else get the already allocated
 * #ofaOpeTemplateStore from this same #myICollector.
 *
 * Returns: a new reference to the store, which should be unreffed by
 * the caller.
 */
ofaOpeTemplateStore *
ofa_ope_template_store_new( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_ope_template_store_new";
	ofaOpeTemplateStore *store;
	ofaOpeTemplateStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaOpeTemplateStore * ) my_icollector_single_get_object( collector, OFA_TYPE_OPE_TEMPLATE_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_OPE_TEMPLATE_STORE( store ), NULL );
		g_debug( "%s: returning existing store=%p", thisfn, ( void * ) store );

	} else {
		store = g_object_new( OFA_TYPE_OPE_TEMPLATE_STORE, NULL );
		g_debug( "%s: returning newly allocated store=%p", thisfn, ( void * ) store );

		priv = ofa_ope_template_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[OPE_TEMPLATE_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		ofa_istore_set_column_types( OFA_ISTORE( store ), getter, OPE_TEMPLATE_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ),
				( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
	}

	return( g_object_ref( store ));
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
 * Load the dataset.
 *
 * The #ofaListStore::load_dataset() virtual method is just a redirection
 * of the #ofaIStore::load_dataset() interface method, which is itself
 * triggered from the #ofa_istore_load_dataset() public method.
 */
static void
list_store_v_load_dataset( ofaListStore *store )
{
	ofaOpeTemplateStorePrivate *priv;
	const GList *dataset, *it;
	ofoOpeTemplate *ope;

	priv = ofa_ope_template_store_get_instance_private( OFA_OPE_TEMPLATE_STORE( store ));

	if( priv->dataset_is_loaded ){
		ofa_list_store_loading_simulate( store );

	} else {
		dataset = ofo_ope_template_get_dataset( priv->getter );

		for( it=dataset ; it ; it=it->next ){
			ope = OFO_OPE_TEMPLATE( it->data );
			insert_row( OFA_OPE_TEMPLATE_STORE( store ), ope );
		}

		priv->dataset_is_loaded = TRUE;
	}
}

static void
insert_row( ofaOpeTemplateStore *self, const ofoOpeTemplate *ope )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( self ),
			&iter,
			-1,
			OPE_TEMPLATE_COL_MNEMO, ofo_ope_template_get_mnemo( ope ),
			OPE_TEMPLATE_COL_OBJECT, ope,
			-1 );

	set_row_by_iter( self, ope, &iter );
}

static void
set_row_by_iter( ofaOpeTemplateStore *self, const ofoOpeTemplate *ope, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_ope_template_store_set_row";
	gint pamrow;
	gchar *crestamp, *updstamp, *spamrow;
	const gchar *notes;
	GError *error;
	GdkPixbuf *notes_png;

	crestamp  = my_stamp_to_str( ofo_ope_template_get_cre_stamp( ope ), MY_STAMP_DMYYHM );
	updstamp  = my_stamp_to_str( ofo_ope_template_get_upd_stamp( ope ), MY_STAMP_DMYYHM );

	pamrow = ofo_ope_template_get_pam_row( ope );
	spamrow = pamrow >= 0 ? g_strdup_printf( "%d", pamrow ) : g_strdup( "" );

	notes = ofo_ope_template_get_notes( ope );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			OPE_TEMPLATE_COL_CRE_USER,      ofo_ope_template_get_cre_user( ope ),
			OPE_TEMPLATE_COL_CRE_STAMP,     crestamp,
			OPE_TEMPLATE_COL_LABEL,         ofo_ope_template_get_label( ope ),
			OPE_TEMPLATE_COL_LEDGER,        ofo_ope_template_get_ledger( ope ),
			OPE_TEMPLATE_COL_LEDGER_LOCKED, ofo_ope_template_get_ledger_locked( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_REF,           ofo_ope_template_get_ref( ope ),
			OPE_TEMPLATE_COL_REF_LOCKED,    ofo_ope_template_get_ref_locked( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_REF_MANDATORY, ofo_ope_template_get_ref_mandatory( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_PAM_ROW,       spamrow,
			OPE_TEMPLATE_COL_HAVE_TIERS,    ofo_ope_template_get_have_tiers( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_TIERS,         ofo_ope_template_get_tiers( ope ),
			OPE_TEMPLATE_COL_TIERS_LOCKED,  ofo_ope_template_get_tiers_locked( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_HAVE_QPPRO,    ofo_ope_template_get_have_qppro( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_QPPRO,         ofo_ope_template_get_qppro( ope ),
			OPE_TEMPLATE_COL_QPPRO_LOCKED,  ofo_ope_template_get_qppro_locked( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_HAVE_RULE,     ofo_ope_template_get_have_rule( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_RULE,          ofo_ope_template_get_rule( ope ),
			OPE_TEMPLATE_COL_RULE_LOCKED,   ofo_ope_template_get_rule_locked( ope ) ? _( "Yes" ):_( "No" ),
			OPE_TEMPLATE_COL_NOTES,         notes,
			OPE_TEMPLATE_COL_NOTES_PNG,     notes_png,
			OPE_TEMPLATE_COL_UPD_USER,      ofo_ope_template_get_upd_user( ope ),
			OPE_TEMPLATE_COL_UPD_STAMP,     updstamp,
			-1 );

	g_object_unref( notes_png );
	g_free( spamrow );
	g_free( crestamp );
	g_free( updstamp );

	ofa_istore_set_values( OFA_ISTORE( self ), iter, ( void * ) ope );
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

static void
set_account_new_id( ofaOpeTemplateStore *self, const gchar *prev_id, const gchar *new_id )
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

static void
set_ledger_new_id( ofaOpeTemplateStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;
	ofoOpeTemplate *template;
	const gchar *model_ledger;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, OPE_TEMPLATE_COL_OBJECT, &template, -1 );
			g_return_if_fail( template && OFO_IS_OPE_TEMPLATE( template ));
			g_object_unref( template );

			model_ledger = ofo_ope_template_get_ledger( template );
			if( my_strlen( model_ledger ) && !my_collate( model_ledger, prev_id )){
				ofo_ope_template_set_ledger( template, new_id );
			}

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
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

/*
 * Connect to ofaISignaler signaling system
 *
 * Note that the need to keep trace of the signal handlers may be
 * questionable as the lifetime of this store is not longer than those
 * of the dossier.
 */
static void
signaler_connect_to_signaling_system( ofaOpeTemplateStore *self )
{
	ofaOpeTemplateStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_ope_template_store_get_instance_private( self );

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
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		insert_row( self, OFO_OPE_TEMPLATE( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_signaler_on_updated_base";
	GtkTreeIter iter;
	const gchar *mnemo, *new_id;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));

		if( prev_id && g_utf8_collate( prev_id, mnemo )){
			remove_row_by_mnemo( self, prev_id );
			insert_row( self, OFO_OPE_TEMPLATE( object ));

		} else if( find_row_by_mnemo( self, mnemo, &iter, NULL )){
			set_row_by_iter( self, OFO_OPE_TEMPLATE( object ), &iter);

		} else {
			g_debug( "%s: not found: mnemo=%s", thisfn, mnemo );
		}

	} else if( OFO_IS_ACCOUNT( object )){
		new_id = ofo_account_get_number( OFO_ACCOUNT( object ));
		if( prev_id && g_utf8_collate( prev_id, new_id )){
			set_account_new_id( self, prev_id, new_id );
		}

	} else if( OFO_IS_LEDGER( object )){
		new_id = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
		if( prev_id && g_utf8_collate( prev_id, new_id )){
			set_ledger_new_id( self, prev_id, new_id );
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_signaler_on_deleted_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		remove_row_by_mnemo( self, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
	}
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_signaler_on_reload_collection";
	ofaOpeTemplateStorePrivate *priv;

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	priv = ofa_ope_template_store_get_instance_private( self );

	if( type == OFO_TYPE_OPE_TEMPLATE ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		priv->dataset_is_loaded = FALSE;
		list_store_v_load_dataset( OFA_LIST_STORE( self ));
	}
}
