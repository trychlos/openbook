/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-istore.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"

#include "core/ofa-account-store.h"

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
	gboolean    dataset_is_loaded;
}
	ofaAccountStorePrivate;

#define ACCOUNT_SETTLEABLE_STR            _( "S" )
#define ACCOUNT_RECONCILIABLE_STR         _( "R" )
#define ACCOUNT_FORWARDABLE_STR           _( "F" )
#define ACCOUNT_CLOSED_STR                _( "C" )
#define ACCOUNT_KEEP_UNSETTLED_STR        _( "Y" )
#define ACCOUNT_KEEP_UNRECONCILIATED_STR  _( "Y" )

/* store data types
 * GDK_PIXBUF is not a constant, and has thus to be set at runtime
 */
static GType st_col_types[ACCOUNT_N_COLUMNS] = {
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* number, cre_user, cre_stamp */
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_BOOLEAN,		/* label, currency, root */
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* settleable, keep_unsettled, reconciliable */
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* keep_unreconciliated, forwardable, closed */
	G_TYPE_STRING,  0,              G_TYPE_STRING,		/* notes, notes_png, upd_user */
	G_TYPE_STRING,										/* upd_stamp */
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* cur_rough_debit, cur_rough_credit, cur_val_debit */
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING,		/* cur_val_credit, fut_rough_debit, fut_rough_credit */
	G_TYPE_STRING,  G_TYPE_STRING,						/* fut_val_debit, fut_val_credit */
	G_TYPE_STRING,  G_TYPE_STRING,  G_TYPE_STRING, 		/* exe_debit, exe_credit, exe_solde */
	G_TYPE_OBJECT										/* the #ofoAccount itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountStore *self );
static void     tree_store_v_load_dataset( ofaTreeStore *store );
static void     insert_row( ofaAccountStore *self, const ofoAccount *account );
static void     set_row_by_iter( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *iter );
static gboolean find_parent_iter( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *parent_iter );
static gboolean find_row_by_number( ofaAccountStore *self, const gchar *number, GtkTreeIter *iter, gboolean *bvalid );
static gboolean find_row_by_number_rec( ofaAccountStore *self, const gchar *number, GtkTreeIter *iter, GtkTreeIter *smaller, gint *last );
static GList   *get_children_iter( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *iter );
static void     realign_children( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *parent_iter );
static GList   *remove_rows_by_number( ofaAccountStore *self, const gchar *number );
static GList   *remove_rows_rec( ofaAccountStore *self, GtkTreeIter *iter, GList *list );
static gint     cmp_account_by_number( const ofoAccount *a, const ofoAccount *b );
static void     set_account_new_id( ofaAccountStore *self, ofoAccount *account, const gchar *prev_id );
static void     set_currency_new_id( ofaAccountStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_currency_new_id_rec( ofaAccountStore *self, const gchar *prev_id, const gchar *new_id, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaAccountStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaAccountStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaAccountStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaAccountStore *self );
static void     signaler_on_deleted_account( ofaAccountStore *self, ofoAccount *account );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaAccountStore *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountStore, ofa_account_store, OFA_TYPE_TREE_STORE, 0,
		G_ADD_PRIVATE( ofaAccountStore ))

static void
account_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_store_parent_class )->finalize( instance );
}

static void
account_store_dispose( GObject *instance )
{
	ofaAccountStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_STORE( instance ));

	priv = ofa_account_store_get_instance_private( OFA_ACCOUNT_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_store_parent_class )->dispose( instance );
}

static void
ofa_account_store_init( ofaAccountStore *self )
{
	static const gchar *thisfn = "ofa_account_store_init";
	ofaAccountStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_STORE( self ));

	priv = ofa_account_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
	priv->dataset_is_loaded = FALSE;
}

static void
ofa_account_store_class_init( ofaAccountStoreClass *klass )
{
	static const gchar *thisfn = "ofa_account_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_store_finalize;

	OFA_TREE_STORE_CLASS( klass )->load_dataset = tree_store_v_load_dataset;
}

/**
 * ofa_account_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaAccountStore and attached it to the collector
 * if not already done. Else get the already allocated #ofaAccountStore
 * from the @dossier.
 *
 * Returns: a new reference to the store, which should be unreffed by
 * the caller.
 */
ofaAccountStore *
ofa_account_store_new( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_account_store_new";
	ofaAccountStore *store;
	ofaAccountStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaAccountStore * ) my_icollector_single_get_object( collector, OFA_TYPE_ACCOUNT_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_ACCOUNT_STORE( store ), NULL );
		g_debug( "%s: returning existing store=%p", thisfn, ( void * ) store );

	} else {
		store = g_object_new( OFA_TYPE_ACCOUNT_STORE, NULL );
		g_debug( "%s: returning newly allocated store=%p", thisfn, ( void * ) store );

		priv = ofa_account_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[ACCOUNT_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		ofa_istore_set_column_types( OFA_ISTORE( store ), getter, ACCOUNT_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );

		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the store per account number
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountStore *self )
{
	gchar *anumber, *bnumber;
	gint cmp;

	gtk_tree_model_get( tmodel, a, ACCOUNT_COL_NUMBER, &anumber, -1 );
	gtk_tree_model_get( tmodel, b, ACCOUNT_COL_NUMBER, &bnumber, -1 );

	cmp = my_collate( anumber, bnumber );

	g_free( anumber );
	g_free( bnumber );

	return( cmp );
}

/*
 * Load the dataset.
 *
 * The #ofaTreeStore::load_dataset() virtual method is just a redirection
 * of the #ofaIStore::load_dataset() interface method, which is itself
 * triggered from the #ofa_istore_load_dataset() public method.
 */
static void
tree_store_v_load_dataset( ofaTreeStore *store )
{
	ofaAccountStorePrivate *priv;
	const GList *dataset, *it;
	ofoAccount *account;

	priv = ofa_account_store_get_instance_private( OFA_ACCOUNT_STORE( store ));

	if( priv->dataset_is_loaded ){
		ofa_tree_store_loading_simulate( store );

	} else {
		dataset = ofo_account_get_dataset( priv->getter );

		for( it=dataset ; it ; it=it->next ){
			account = OFO_ACCOUNT( it->data );
			insert_row( OFA_ACCOUNT_STORE( store ), account );
		}

		priv->dataset_is_loaded = TRUE;
	}
}

/*
 * If we do not found a valid parent for the being-inserted account,
 * then it will be inserted at level 0 of the tree, whatever be its
 * actual account level.
 */
static void
insert_row( ofaAccountStore *self, const ofoAccount *account )
{
	static const gchar *thisfn = "ofa_account_store_insert_row";
	GtkTreeIter parent_iter, iter;
	gboolean parent_found;

	parent_found = find_parent_iter( self, account, &parent_iter );

	g_debug( "%s: number=%s", thisfn, ofo_account_get_number( account ));

	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( self ),
			&iter,
			parent_found ? &parent_iter : NULL,
			-1,
			ACCOUNT_COL_NUMBER, ofo_account_get_number( account ),
			ACCOUNT_COL_OBJECT, account,
			-1 );

	set_row_by_iter( self, account, &iter );
	realign_children( self, account, &iter );
}

static void
set_row_by_iter( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_account_store_set_row_by_iter";
	ofaAccountStorePrivate *priv;
	const gchar *currency_code, *notes;
	ofoCurrency *currency_obj;
	gchar *crestamp, *updstamp;
	gchar *scrdeb, *scrcre, *scvdeb, *scvcre, *sfrdeb, *sfrcre, *sfvdeb, *sfvcre, *sedeb, *secre, *sesol;
	gchar *str;
	ofxAmount crough_debit, crough_credit, cval_debit, cval_credit;
	ofxAmount frough_debit, frough_credit, fval_debit, fval_credit;
	ofxAmount exe_debit, exe_credit, exe_solde;
	GdkPixbuf *notes_png;
	GError *error;

	priv = ofa_account_store_get_instance_private( self );

	currency_code = ofo_account_get_currency( account );
	if( !ofo_account_is_root( account )){
		currency_obj = ofo_currency_get_by_code( priv->getter, currency_code );
		g_return_if_fail( currency_obj && OFO_IS_CURRENCY( currency_obj ));

		crough_debit = ofo_account_get_current_rough_debit( account );
		crough_credit = ofo_account_get_current_rough_credit( account );
		cval_debit = ofo_account_get_current_val_debit( account );
		cval_credit = ofo_account_get_current_val_credit( account );
		frough_debit = ofo_account_get_futur_rough_debit( account );
		frough_credit = ofo_account_get_futur_rough_credit( account );
		fval_debit = ofo_account_get_futur_val_debit( account );
		fval_credit = ofo_account_get_futur_val_credit( account );

		scrdeb = ofa_amount_to_str( crough_debit, currency_obj, priv->getter );
		scrcre = ofa_amount_to_str( crough_credit, currency_obj, priv->getter );
		scvdeb = ofa_amount_to_str( cval_debit, currency_obj, priv->getter );
		scvcre = ofa_amount_to_str( cval_credit, currency_obj, priv->getter );
		sfrdeb = ofa_amount_to_str( frough_debit, currency_obj, priv->getter );
		sfrcre = ofa_amount_to_str( frough_credit, currency_obj, priv->getter );
		sfvdeb = ofa_amount_to_str( fval_debit, currency_obj, priv->getter );
		sfvcre = ofa_amount_to_str( fval_credit, currency_obj, priv->getter );

		exe_debit = crough_debit + cval_debit + frough_debit + fval_debit;
		exe_credit = crough_credit + cval_credit + frough_credit + fval_credit;

		sedeb = ofa_amount_to_str( exe_debit, currency_obj, priv->getter );
		secre = ofa_amount_to_str( exe_credit, currency_obj, priv->getter );
		exe_solde = exe_debit - exe_credit;

		if( exe_solde >= 0 ){
			str = ofa_amount_to_str( exe_solde, currency_obj, priv->getter );
			sesol = g_strdup_printf( _( "%s DB" ), str );
		} else {
			str = ofa_amount_to_str( -exe_solde, currency_obj, priv->getter );
			sesol = g_strdup_printf( _( "%s CR" ), str );
		}
		g_free( str );

	} else {
		scrdeb = g_strdup( "" );
		scrcre = g_strdup( "" );
		scvdeb = g_strdup( "" );
		scvcre = g_strdup( "" );
		sfrdeb = g_strdup( "" );
		sfrcre = g_strdup( "" );
		sfvdeb = g_strdup( "" );
		sfvcre = g_strdup( "" );
		sedeb = g_strdup( "" );
		secre = g_strdup( "" );
		sesol = g_strdup( "" );
	}

	crestamp = my_stamp_to_str( ofo_account_get_cre_stamp( account ), MY_STAMP_DMYYHM );
	updstamp = my_stamp_to_str( ofo_account_get_upd_stamp( account ), MY_STAMP_DMYYHM );
	notes = ofo_account_get_notes( account );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_tree_store_set(
			GTK_TREE_STORE( self ),
			iter,
			ACCOUNT_COL_CRE_USER,             ofo_account_get_cre_user( account ),
			ACCOUNT_COL_CRE_STAMP,            crestamp,
			ACCOUNT_COL_LABEL,                ofo_account_get_label( account ),
			ACCOUNT_COL_CURRENCY,             currency_code,
			ACCOUNT_COL_ROOT,                 ofo_account_is_root( account ),
			ACCOUNT_COL_SETTLEABLE,           ofo_account_is_settleable( account ) ? ACCOUNT_SETTLEABLE_STR : "",
			ACCOUNT_COL_KEEP_UNSETTLED,       ofo_account_get_keep_unsettled( account ) ? ACCOUNT_KEEP_UNSETTLED_STR : "",
			ACCOUNT_COL_RECONCILIABLE,        ofo_account_is_reconciliable( account ) ? ACCOUNT_RECONCILIABLE_STR : "",
			ACCOUNT_COL_KEEP_UNRECONCILIATED, ofo_account_get_keep_unreconciliated( account ) ? ACCOUNT_KEEP_UNRECONCILIATED_STR : "",
			ACCOUNT_COL_FORWARDABLE,          ofo_account_is_forwardable( account ) ? ACCOUNT_FORWARDABLE_STR : "",
			ACCOUNT_COL_CLOSED,               ofo_account_is_closed( account ) ? ACCOUNT_CLOSED_STR : "",
			ACCOUNT_COL_NOTES,                notes,
			ACCOUNT_COL_NOTES_PNG,            notes_png,
			ACCOUNT_COL_UPD_USER,             ofo_account_get_upd_user( account ),
			ACCOUNT_COL_UPD_STAMP,            updstamp,
			ACCOUNT_COL_CROUGH_DEBIT,         scrdeb,
			ACCOUNT_COL_CROUGH_CREDIT,        scrcre,
			ACCOUNT_COL_CVAL_DEBIT,           scvdeb,
			ACCOUNT_COL_CVAL_CREDIT,          scvcre,
			ACCOUNT_COL_FROUGH_DEBIT,         sfrdeb,
			ACCOUNT_COL_FROUGH_CREDIT,        sfrcre,
			ACCOUNT_COL_FVAL_DEBIT,           sfvdeb,
			ACCOUNT_COL_FVAL_CREDIT,          sfvcre,
			ACCOUNT_COL_EXE_DEBIT,            sedeb,
			ACCOUNT_COL_EXE_CREDIT,           secre,
			ACCOUNT_COL_EXE_SOLDE,            sesol,
			-1 );

	g_object_unref( notes_png );
	g_free( scrdeb );
	g_free( scrcre );
	g_free( scvdeb );
	g_free( scvcre );
	g_free( sfrdeb );
	g_free( sfrcre );
	g_free( sfvdeb );
	g_free( sfvcre );
	g_free( sedeb );
	g_free( secre );
	g_free( sesol );
	g_free( updstamp );
	g_free( crestamp );

	ofa_istore_set_values( OFA_ISTORE( self ), iter, ( void * ) account );
}

/*
 * @self: this #ofaAccountStore
 * @account: [in]: the account for which we are searching the closest
 *  parent
 * @parent_iter: [out]: the GtkTreeIter of the closes parent if found.
 *
 * Search for the GtkTreeIter corresponding to the closest parent of
 * this account
 *
 * Returns %TRUE if a parent has been found, and @parent_iter adresses
 * this parent, else @parent_iter is undefined.
 */
static gboolean
find_parent_iter( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *parent_iter )
{
	const gchar *number;
	gchar *candidate_number;
	gboolean found;
	glong len;

	number = ofo_account_get_number( account );
	candidate_number = g_strdup( number );
	found = FALSE;

	while( !found && ( len = my_strlen( candidate_number )) > 1 ){
		candidate_number[len-1] = '\0';
		//g_debug( "find_parent_iter: candidate_number='%s'", candidate_number );
		found = find_row_by_number( self, candidate_number, parent_iter, NULL );
	}

	g_free( candidate_number );

	return( found );
}

/*
 * find_row_by_number:
 * @self: this #ofaAccountStore
 * @number: [in]: the account number we are searching for.
 * @iter: [out]: the last iter equal or immediately greater than the
 *  searched value.
 * @bvalid: [allow-none][out]: set to TRUE if the returned iter is
 *  valid.
 *
 * Rows are sorted by account number
 * We exit the search as soon as we get a number greater than the
 * searched one, or the end of the tree.
 *
 * Returns TRUE if we have found an exact match, and @iter addresses
 * this exact match.
 *
 * Returns FALSE if we do not have found a match:
 * - @iter addresses
 * the first number greater than the searched value, or lesser if at
 * the end of the list, or nothing at all if the self is empty (and
 * @bvalid will be %FALSE if this later case).
 */
static gboolean
find_row_by_number( ofaAccountStore *self, const gchar *number, GtkTreeIter *iter, gboolean *bvalid )
{
	gint last_cmp;
	gboolean found;
	GtkTreeIter smaller;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		if( bvalid ){
			*bvalid = TRUE;
		}
		smaller = *iter;
		found = find_row_by_number_rec( self, number, iter, &smaller, &last_cmp );
		return( found );
	}

	return( FALSE );
}

/*
 * enter here with a valid @iter (the first of the tree)
 * start by compare this @iter with the searched value
 * returns TRUE (and exit the recursion) if the @iter is equal to
 *   the searched value
 * returns FALSE (and exit the recursion) if the @iter is greater than
 *   the searched value
 * else if have children
 *   get the first child and recurse
 * else get next iter
 *
 * when exiting this function, iter is set to the last number which is
 * less or equal to the searched value
 */
static gboolean
find_row_by_number_rec( ofaAccountStore *self, const gchar *number, GtkTreeIter *iter, GtkTreeIter *smaller, gint *last_cmp )
{
	gchar *cmp_number;
	gint searched_class, cmp_class;
	GtkTreeIter cmp_iter, child_iter, last_iter;

	cmp_iter = *iter;
	searched_class = ofo_account_get_class_from_number( number );

	while( TRUE ){
		gtk_tree_model_get( GTK_TREE_MODEL( self ), &cmp_iter, ACCOUNT_COL_NUMBER, &cmp_number, -1 );
		*last_cmp = g_utf8_collate( cmp_number, number );
		cmp_class = ofo_account_get_class_from_number( cmp_number );
		g_free( cmp_number );
		if( *last_cmp == 0 ){
			*iter = cmp_iter;
			return( TRUE );
		}
		if( *last_cmp > 0 ){
			*iter = cmp_class > searched_class ? *smaller : cmp_iter;
			return( FALSE );
		}
		*smaller = cmp_iter;
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, &cmp_iter )){
			find_row_by_number_rec( self, number, &child_iter, smaller, last_cmp );
			if( *last_cmp == 0 ){
				*iter = child_iter;
				return( TRUE );
			}
			if( *last_cmp > 0 ){
				*iter = *smaller;
				return( FALSE );
			}
			*smaller = child_iter;
		}
		last_iter = cmp_iter;
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &cmp_iter )){
			break;
		}
	}

	*iter = last_iter;
	return( FALSE );
}

/*
 * The @account has just come inserted at @parent_iter.
 * Its possible children have to be reinserted under it.
 * When entering here, @parent_iter should not have yet any child iter
 * (because it is newly inserted)
 *
 * This function relies on the fact that the self is sorted.
 * Starting from the newly inserted account (@parent_iter), we
 * successively iterate on children while these are actual child
 * of the account.
 */
static void
realign_children( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *parent_iter )
{
	static const gchar *thisfn = "ofa_account_store_realign_children";
	GList *children, *it;
	GtkTreeIter iter;

	if( gtk_tree_model_iter_has_child( GTK_TREE_MODEL( self ), parent_iter )){
		g_warning( "%s: newly inserted row, but already has one child", thisfn );

	} else {
		children = NULL;
		iter = *parent_iter;
		if( gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
			children = get_children_iter( self, account, &iter );
			for( it=children ; it ; it=it->next ){
				//g_debug( "realign_children: re-inserting %s", ofo_account_get_number( OFO_ACCOUNT( it->data )));
				insert_row( self, OFO_ACCOUNT( it->data ));
			}
			g_list_free_full( children, ( GDestroyNotify ) g_object_unref );
		}
	}
}

/*
 * get_children_iter:
 * copy into 'children' GList all children accounts, along with their
 * row reference - it will so be easy to remove them from the model,
 * then reinsert these same accounts
 */
static GList *
get_children_iter( ofaAccountStore *self, const ofoAccount *account, GtkTreeIter *iter )
{
	GList *children;
	ofoAccount *candidate;
	gboolean is_child, has_next;
	GtkTreePath *path;
	GtkTreeRowReference *ref;
	GtkTreeIter next_iter;

	children = NULL;

	while( TRUE ){
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, ACCOUNT_COL_OBJECT, &candidate, -1 );
		g_object_unref( candidate );

		is_child = ofo_account_is_child_of( account, ofo_account_get_number( candidate ));
		if( 0 ){
			g_debug( "get_children_iter: account=%s, candidate=%s, is_child=%s",
					ofo_account_get_number( account ),
					ofo_account_get_number( candidate ),
					is_child ? "True":"False" );
		}
		if( is_child ){
			next_iter = *iter;
			has_next = gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &next_iter );
			if( has_next ){
				path = gtk_tree_model_get_path( GTK_TREE_MODEL( self ), &next_iter );
				ref = gtk_tree_row_reference_new( GTK_TREE_MODEL( self ), path );
				gtk_tree_path_free( path );
			}
			children = remove_rows_rec( self, iter, children );
			if( has_next ){
				path = gtk_tree_row_reference_get_path( ref );
				gtk_tree_model_get_iter( GTK_TREE_MODEL( self ), iter, path );
				gtk_tree_row_reference_free( ref );

			} else {
				break;
			}

		} else {
			break;
		}
	}

	return( children );
}

/*
 * Recursively removes the 'number' row and all its children.
 * Does nothing if 'number' does not exist.
 * Returns: a #GList of ofoAccount references
 */
static GList *
remove_rows_by_number( ofaAccountStore *self, const gchar *number )
{
	static const gchar *thisfn = "ofa_account_store_remove_rows_by_number";
	GList *list;
	GtkTreeIter iter;
	gboolean is_valid;

	g_debug( "%s: number=%s", thisfn, number );

	/* warning is admitted here because this function is expected to be
	 * called only on existing accounts */
	if( !find_row_by_number( self, number, &iter, &is_valid )){
		g_warning( "%s: '%s': account not found", thisfn, number );
		return( NULL );
	}
	if( !is_valid ){
		g_warning( "%s: '%s': account was found, but returned GtkTreeIter is not valid", thisfn, number );
		return( NULL );
	}

	list = remove_rows_rec( self, &iter, NULL );

	return( list );
}

/*
 * Enter here on an item to be removed.
 * If item has children, then start by removing the children
 * Then remove the row, adding it to the list
 *
 * Returns: a list by account number.
 */
static GList *
remove_rows_rec( ofaAccountStore *self, GtkTreeIter *iter, GList *list )
{
	GtkTreeIter child_iter;
	ofoAccount *account;

	if( 0 ){
		g_debug( "remove_rows_rec: iter=%p, list=%p (count=%d)",
				( void * ) iter, ( void * ) list, g_list_length( list ));
	}

	while( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
		list = remove_rows_rec( self, &child_iter, list );
	}

	gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, ACCOUNT_COL_OBJECT, &account, -1 );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );
	list = g_list_insert_sorted( list, account, ( GCompareFunc ) cmp_account_by_number );

	//g_debug( "remove_rows_rec: removing %s", ofo_account_get_number( account ));
	gtk_tree_store_remove( GTK_TREE_STORE( self ), iter );

	return( list );
}

static gint
cmp_account_by_number( const ofoAccount *a, const ofoAccount *b )
{
	const gchar *id_a, *id_b;

	id_a = ofo_account_get_number( a );
	id_b = ofo_account_get_number( b );

	return( g_utf8_collate( id_a, id_b ));
}

static void
set_account_new_id( ofaAccountStore *self, ofoAccount *account, const gchar *prev_id )
{
	GtkTreeIter iter;
	const gchar *number;
	GList *list, *it;

	number = ofo_account_get_number( account );

	if( prev_id && g_utf8_collate( prev_id, number )){

		/* first element of the list is the account itself */
		list = remove_rows_by_number( self, prev_id );
		for( it=list ; it ; it=it->next ){
			insert_row( self, OFO_ACCOUNT( it->data ));
		}
		g_list_free_full( list, ( GDestroyNotify ) g_object_unref );

	} else if( find_row_by_number( self, number, &iter, NULL )){
		set_row_by_iter( self, account, &iter);
	}
}

/*
 * Update the store rows+objects for new currency code
 */
static void
set_currency_new_id( ofaAccountStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		set_currency_new_id_rec( self, prev_id, new_id, &iter );
	}
}

static void
set_currency_new_id_rec( ofaAccountStore *self, const gchar *prev_id, const gchar *new_id, GtkTreeIter *iter )
{
	GtkTreeIter child_iter;
	ofoAccount *account;
	gchar *stored_id;
	gint cmp;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			set_currency_new_id_rec( self, prev_id, new_id, &child_iter );
		}
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter,
				ACCOUNT_COL_CURRENCY, &stored_id, ACCOUNT_COL_OBJECT, &account, -1 );

		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
		g_object_unref( account );
		if( stored_id ){
			cmp = g_utf8_collate( stored_id, prev_id );
			g_free( stored_id );

			if( cmp == 0 ){
				ofo_account_set_currency( account, new_id );
				gtk_tree_store_set( GTK_TREE_STORE( self ), iter, ACCOUNT_COL_CURRENCY, new_id, -1 );
			}
		}
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaAccountStore *self )
{
	ofaAccountStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_account_store_get_instance_private( self );

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
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaAccountStore *self )
{
	static const gchar *thisfn = "ofa_account_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		insert_row( self, OFO_ACCOUNT( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaAccountStore *self )
{
	static const gchar *thisfn = "ofa_account_store_signaler_on_updated_base";
	const gchar *new_id;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		set_account_new_id( self, OFO_ACCOUNT( object ), prev_id );

	} else if( OFO_IS_CURRENCY( object )){
		new_id = ofo_currency_get_code( OFO_CURRENCY( object ));
		if( my_strlen( prev_id ) && my_collate( prev_id, new_id )){
			set_currency_new_id( self, prev_id, new_id );
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaAccountStore *self )
{
	static const gchar *thisfn = "ofa_account_store_signaler_on_deleted_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ACCOUNT( object )){
		signaler_on_deleted_account( self, OFO_ACCOUNT( object ));
	}
}

static void
signaler_on_deleted_account( ofaAccountStore *self, ofoAccount *account )
{
	ofaAccountStorePrivate *priv;
	GList *list, *children, *it;

	priv = ofa_account_store_get_instance_private( self );

	/* first element of the list is the account itself */
	list = remove_rows_by_number( self, ofo_account_get_number( account ));
	children = list ? list->next : NULL;

	if( !ofa_prefs_account_get_delete_with_children( priv->getter )){
		for( it=children ; it ; it=it->next ){
			insert_row( self, OFO_ACCOUNT( it->data ));
		}
	}

	g_list_free_full( list, ( GDestroyNotify ) g_object_unref );
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaAccountStore *self )
{
	static const gchar *thisfn = "ofa_account_store_signaler_on_reload_collection";
	ofaAccountStorePrivate *priv;

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	priv = ofa_account_store_get_instance_private( self );

	if( type == OFO_TYPE_ACCOUNT ){
		gtk_tree_store_clear( GTK_TREE_STORE( self ));
		priv->dataset_is_loaded = FALSE;
		tree_store_v_load_dataset( OFA_TREE_STORE( self ));
	}
}
