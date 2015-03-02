/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-preferences.h"

#include "ui/ofa-account-store.h"

/* private instance data
 */
struct _ofaAccountStorePrivate {
	gboolean dispose_has_run;

	/* runtime data
	 */
};

/* a structure used when moving a subtree to another place
 */
typedef struct {
	ofoDossier          *dossier;
	ofoAccount          *account;
	GtkTreeRowReference *ref;
}
	sChild;

static GType st_col_types[ACCOUNT_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* number, label, currency */
		G_TYPE_STRING, G_TYPE_STRING, 0,				/* type, notes, notes_png */
		G_TYPE_STRING,									/* upd_user */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* upd_stamp, val_debit, val_credit */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* rough_debit, rough_credit, open_debit */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* open_credit, fut_debit, fut_credit */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* settleable, reconciliable, forward */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* closed, exe_debit, exe_credit */
		G_TYPE_OBJECT									/* the #ofoAccount itself */
};

/* the key which is attached to the dossier in order to identify this
 * store
 */
#define STORE_DATA_DOSSIER                   "ofa-account-store"

static const gchar *st_filler_png            = PKGUIDIR "/filler.png";
static const gchar *st_notes_png             = PKGUIDIR "/notes1.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountStore *store );
static void     tree_store_load_dataset( ofaTreeStore *store );
static void     insert_row( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account );
static void     set_row( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account, GtkTreeIter *iter );
static gboolean find_parent_iter( ofaAccountStore *store, const ofoAccount *account, GtkTreeIter *parent_iter );
static gboolean find_row_by_number( ofaAccountStore *store, const gchar *number, GtkTreeIter *iter, gboolean *bvalid );
static gboolean find_row_by_number_rec( ofaAccountStore *store, const gchar *number, GtkTreeIter *iter, gint *last );
static void     realign_children( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account, GtkTreeIter *parent_iter );
static GList   *realign_children_rec( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account, GtkTreeIter *iter, GList *children );
static void     realign_children_move( sChild *child_str, ofaAccountStore *store );
static void     child_free( sChild *child_str );
static void     remove_row_by_number( ofaAccountStore *store, const gchar *number );
static void     setup_signaling_connect( ofaAccountStore *store, ofoDossier *dossier );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountStore *store );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountStore *store );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountStore *store );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaAccountStore *store );

G_DEFINE_TYPE( ofaAccountStore, ofa_account_store, OFA_TYPE_TREE_STORE )

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

	g_return_if_fail( instance && OFA_IS_ACCOUNT_STORE( instance ));

	priv = OFA_ACCOUNT_STORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_store_parent_class )->dispose( instance );
}

static void
ofa_account_store_init( ofaAccountStore *self )
{
	static const gchar *thisfn = "ofa_account_store_init";

	g_return_if_fail( OFA_IS_ACCOUNT_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNT_STORE, ofaAccountStorePrivate );
}

static void
ofa_account_store_class_init( ofaAccountStoreClass *klass )
{
	static const gchar *thisfn = "ofa_account_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_store_finalize;

	OFA_TREE_STORE_CLASS( klass )->load_dataset = tree_store_load_dataset;

	g_type_class_add_private( klass, sizeof( ofaAccountStorePrivate ));
}

/**
 * ofa_account_store_new:
 * @dossier: the currently opened #ofoDossier.
 *
 * Instanciates a new #ofaAccountStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaAccountStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaAccountStore *
ofa_account_store_new( ofoDossier *dossier )
{
	ofaAccountStore *store;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	store = ( ofaAccountStore * ) g_object_get_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER );

	if( store ){
		g_return_val_if_fail( OFA_IS_ACCOUNT_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_ACCOUNT_STORE,
						OFA_PROP_DOSSIER, dossier,
						NULL );

		st_col_types[ACCOUNT_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_tree_store_set_column_types(
				GTK_TREE_STORE( store ), ACCOUNT_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_object_set_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER, store );

		setup_signaling_connect( store, dossier );
	}

	return( store );
}

/*
 * sorting the store per account number
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountStore *store )
{
	gchar *anumber, *bnumber;
	gint cmp;

	gtk_tree_model_get( tmodel, a, ACCOUNT_COL_NUMBER, &anumber, -1 );
	gtk_tree_model_get( tmodel, b, ACCOUNT_COL_NUMBER, &bnumber, -1 );

	cmp = g_utf8_collate( anumber, bnumber );

	g_free( anumber );
	g_free( bnumber );

	return( cmp );
}

/*
 * load the dataset when columns and dossier have been both set
 */
static void
tree_store_load_dataset( ofaTreeStore *store )
{
	const GList *dataset, *it;
	ofoAccount *account;
	ofoDossier *dossier;

	g_object_get( G_OBJECT( store ), OFA_PROP_DOSSIER, &dossier, NULL );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	dataset = ofo_account_get_dataset( dossier );
	g_object_unref( dossier );

	for( it=dataset ; it ; it=it->next ){
		account = OFO_ACCOUNT( it->data );
		insert_row( OFA_ACCOUNT_STORE( store ), dossier, account );
	}
}

static void
insert_row( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account )
{
	GtkTreeIter parent_iter, iter;
	gboolean parent_found;

	parent_found = find_parent_iter( store, account, &parent_iter );

	gtk_tree_store_insert_with_values(
			GTK_TREE_STORE( store ),
			&iter,
			parent_found ? &parent_iter : NULL,
			-1,
			ACCOUNT_COL_NUMBER, ofo_account_get_number( account ),
			ACCOUNT_COL_OBJECT, account,
			-1 );

	set_row( store, dossier, account, &iter );
	realign_children( store, dossier, account, &iter );
}

static void
set_row( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_account_store_set_row";
	const gchar *currency_code, *notes;
	gint digits;
	ofoCurrency *currency_obj;
	gchar *stamp;
	gchar *svdeb, *svcre, *srdeb, *srcre, *sodeb, *socre, *sfdeb, *sfcre, *sedeb, *secre;
	ofxAmount val_debit, val_credit, rough_debit, rough_credit;
	GdkPixbuf *notes_png;
	GError *error;

	currency_code = ofo_account_get_currency( account );
	if( !ofo_account_is_root( account )){
		currency_obj = ofo_currency_get_by_code( dossier, currency_code );
		if( currency_obj && OFO_IS_CURRENCY( currency_obj )){
			digits = ofo_currency_get_digits( currency_obj );
		} else {
			digits = 2;
		}
		val_debit = ofo_account_get_val_debit( account );
		svdeb = my_double_to_str_ex( val_debit, digits );
		val_credit = ofo_account_get_val_credit( account );
		svcre = my_double_to_str_ex( val_credit, digits );
		rough_debit = ofo_account_get_rough_debit( account );
		srdeb = my_double_to_str_ex( rough_debit, digits );
		rough_credit = ofo_account_get_rough_credit( account );
		srcre = my_double_to_str_ex( rough_credit, digits );
		sodeb = my_double_to_str_ex( ofo_account_get_open_debit( account ), digits );
		socre = my_double_to_str_ex( ofo_account_get_open_credit( account ), digits );
		sfdeb = my_double_to_str_ex( ofo_account_get_futur_debit( account ), digits );
		sfcre = my_double_to_str_ex( ofo_account_get_futur_credit( account ), digits );
		sedeb = my_double_to_str_ex( val_debit+rough_debit, digits );
		secre = my_double_to_str_ex( val_credit+rough_credit, digits );

	} else {
		svdeb = g_strdup( "" );
		svcre = g_strdup( "" );
		srdeb = g_strdup( "" );
		srcre = g_strdup( "" );
		sodeb = g_strdup( "" );
		socre = g_strdup( "" );
		sfdeb = g_strdup( "" );
		sfcre = g_strdup( "" );
		sedeb = g_strdup( "" );
		secre = g_strdup( "" );
	}

	stamp = my_utils_stamp_to_str( ofo_account_get_upd_stamp( account ), MY_STAMP_DMYYHM );
	notes = ofo_account_get_notes( account );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_file( notes ? st_notes_png : st_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_file: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_tree_store_set(
			GTK_TREE_STORE( store ),
			iter,
			ACCOUNT_COL_LABEL,         ofo_account_get_label( account ),
			ACCOUNT_COL_CURRENCY,      currency_code,
			ACCOUNT_COL_TYPE,          ofo_account_get_type_account( account ),
			ACCOUNT_COL_NOTES,         notes,
			ACCOUNT_COL_NOTES_PNG,     notes_png,
			ACCOUNT_COL_UPD_USER,      ofo_account_get_upd_user( account ),
			ACCOUNT_COL_UPD_STAMP,     stamp,
			ACCOUNT_COL_VAL_DEBIT,     svdeb,
			ACCOUNT_COL_VAL_CREDIT,    svcre,
			ACCOUNT_COL_ROUGH_DEBIT,   srdeb,
			ACCOUNT_COL_ROUGH_CREDIT,  srcre,
			ACCOUNT_COL_OPEN_DEBIT,    sodeb,
			ACCOUNT_COL_OPEN_CREDIT,   socre,
			ACCOUNT_COL_FUT_DEBIT,     sfdeb,
			ACCOUNT_COL_FUT_CREDIT,    sfcre,
			ACCOUNT_COL_SETTLEABLE,    ofo_account_is_settleable( account ) ? ACCOUNT_SETTLEABLE : "",
			ACCOUNT_COL_RECONCILIABLE, ofo_account_is_reconciliable( account ) ? ACCOUNT_RECONCILIABLE : "",
			ACCOUNT_COL_FORWARD,       ofo_account_is_forward( account ) ? ACCOUNT_FORWARDABLE : "",
			ACCOUNT_COL_CLOSED,        ofo_account_is_closed( account ) ? ACCOUNT_CLOSED : "",
			ACCOUNT_COL_EXE_DEBIT,     sedeb,
			ACCOUNT_COL_EXE_CREDIT,    secre,
			-1 );

	g_free( svdeb );
	g_free( svcre );
	g_free( srdeb );
	g_free( srcre );
	g_free( sodeb );
	g_free( socre );
	g_free( sfdeb );
	g_free( sfcre );
	g_free( sedeb );
	g_free( secre );
	g_free( stamp );
}

/*
 * @store: this #ofaAccountStore
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
find_parent_iter( ofaAccountStore *store, const ofoAccount *account, GtkTreeIter *parent_iter )
{
	const gchar *number;
	gchar *candidate_number;
	gboolean found;

	number = ofo_account_get_number( account );
	candidate_number = g_strdup( number );
	found = FALSE;

	while( !found && g_utf8_strlen( candidate_number, -1 ) > 1 ){
		candidate_number[g_utf8_strlen( candidate_number, -1 )-1] = '\0';
		found = find_row_by_number( store, candidate_number, parent_iter, NULL );
	}

	g_free( candidate_number );

	return( found );
}

/*
 * find_row_by_number:
 * @store: this #ofaAccountStore
 * @number: [in]: the account number we are searching for.
 * @iter: [out]: the last iter equal or immediately greater than the
 *  searched value.
 * @bvalid: [allow-none][out]: set to TRUE if the returned iter is
 *  valid.
 *
 * rows are sorted by account number
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
find_row_by_number( ofaAccountStore *store, const gchar *number, GtkTreeIter *iter, gboolean *bvalid )
{
	gint last;
	gboolean found;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		if( bvalid ){
			*bvalid = TRUE;
		}
		last = -1;
		found = find_row_by_number_rec( store, number, iter, &last );
		return( found );
	}

	return( FALSE );
}

/*
 * enter here with a valid @iter
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
find_row_by_number_rec( ofaAccountStore *store, const gchar *number, GtkTreeIter *iter, gint *last )
{
	gchar *cmp_number;
	GtkTreeIter cmp_iter, child_iter, last_iter;

	cmp_iter = *iter;

	while( TRUE ){
		gtk_tree_model_get( GTK_TREE_MODEL( store ), &cmp_iter, ACCOUNT_COL_NUMBER, &cmp_number, -1 );
		*last = g_utf8_collate( cmp_number, number );
		g_free( cmp_number );
		if( *last == 0 ){
			*iter = cmp_iter;
			return( TRUE );
		}
		if( *last > 0 ){
			*iter = cmp_iter;
			return( FALSE );
		}
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( store ), &child_iter, &cmp_iter )){
			find_row_by_number_rec( store, number, &child_iter, last );
			if( *last == 0 ){
				*iter = child_iter;
				return( TRUE );
			}
			if( *last > 0 ){
				*iter = child_iter;
				return( FALSE );
			}
		}
		last_iter = cmp_iter;
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), &cmp_iter )){
			break;
		}
	}

	*iter = last_iter;
	return( FALSE );
}

/*
 * the @account has just come inserted at @parent_iter
 * its possible children have to be reinserted under it
 * when entering here, @parent_iter should not have yet any child iter
 */
static void
realign_children( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account, GtkTreeIter *parent_iter )
{
	static const gchar *thisfn = "ofa_account_store_realign_children";
	GList *children;
	GtkTreeIter iter;

	if( gtk_tree_model_iter_has_child( GTK_TREE_MODEL( store ), parent_iter )){
		g_warning( "%s: newly inserted row already has at least one child", thisfn );

	} else {
		children = NULL;
		iter = *parent_iter;
		children = realign_children_rec( store, dossier, account, &iter, children );
		if( children ){
			g_list_foreach( children, ( GFunc ) realign_children_move, store );
			g_list_free_full( children, ( GDestroyNotify ) child_free );
		}
	}
}

/*
 * realign_children_rec:
 * copy into 'children' GList all children accounts, along with their
 * row reference - it will so be easy to remove them from the model,
 * then reinsert these same accounts
 */
static GList *
realign_children_rec( ofaAccountStore *store, ofoDossier *dossier, const ofoAccount *account, GtkTreeIter *iter, GList *children )
{
	ofoAccount *candidate;
	GtkTreeIter child_iter;
	GtkTreePath *path;
	sChild *child_str;

	while( gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), iter )){
		gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, ACCOUNT_COL_OBJECT, &candidate, -1 );
		g_object_unref( candidate );

		if( ofo_account_is_child_of( account, candidate )){
			child_str = g_new0( sChild, 1 );
			path = gtk_tree_model_get_path( GTK_TREE_MODEL( store ), iter );
			child_str->dossier = dossier;
			child_str->account = g_object_ref( candidate );
			child_str->ref = gtk_tree_row_reference_new( GTK_TREE_MODEL( store ), path );
			children = g_list_prepend( children, child_str );
			gtk_tree_path_free( path );

			if( gtk_tree_model_iter_children( GTK_TREE_MODEL( store ), &child_iter, iter )){
				children = realign_children_rec( store, dossier, account, &child_iter, children );
			}

		} else {
			break;
		}
	}

	return( children );
}

static void
realign_children_move( sChild *child_str, ofaAccountStore *store )
{
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_row_reference_get_path( child_str->ref );
	if( path && gtk_tree_model_get_iter( GTK_TREE_MODEL( store ), &iter, path )){
		gtk_tree_store_remove( GTK_TREE_STORE( store ), &iter );
		gtk_tree_path_free( path );
		insert_row( store, child_str->dossier, child_str->account );
	}
}

/**
 * ofa_account_store_set_dossier:
 * @instance: this #ofaAccountStore instance.
 * @dossier: the opened #ofoDossier.
 *
 * Set the dossier and load the corresponding dataset.
 * Connect to the dossier signaling system in order to maintain the
 * dataset up to date.
 */
static void
child_free( sChild *child_str )
{
	g_object_unref( child_str->account );
	gtk_tree_row_reference_free( child_str->ref );
	g_free( child_str );
}

static void
remove_row_by_number( ofaAccountStore *store, const gchar *number )
{
	GtkTreeIter iter;

	if( find_row_by_number( store, number, &iter, NULL )){
		gtk_tree_store_remove( GTK_TREE_STORE( store ), &iter );
	}
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this store is equal to those of the dossier
 */
static void
setup_signaling_connect( ofaAccountStore *store, ofoDossier *dossier )
{
	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), store );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountStore *store )
{
	static const gchar *thisfn = "ofa_account_store_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_ACCOUNT( object )){
		insert_row( store, dossier, OFO_ACCOUNT( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountStore *store )
{
	static const gchar *thisfn = "ofa_account_store_on_updated_object";
	GtkTreeIter iter;
	const gchar *number;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( OFO_IS_ACCOUNT( object )){
		number = ofo_account_get_number( OFO_ACCOUNT( object ));

		if( prev_id && g_utf8_collate( prev_id, number )){
			remove_row_by_number( store, prev_id );
			insert_row( store, dossier, OFO_ACCOUNT( object ));

		} else if( find_row_by_number( store, number, &iter, NULL )){
			set_row( store, dossier, OFO_ACCOUNT( object ), &iter);
		}
	}
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountStore *store )
{
	static const gchar *thisfn = "ofa_account_store_on_deleted_object";
	GList *children, *it;

	g_debug( "%s: dossier=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_ACCOUNT( object )){
		children = ofo_account_get_children( OFO_ACCOUNT( object ), dossier );
		g_object_ref( OFO_ACCOUNT( object ));
		remove_row_by_number( store, ofo_account_get_number( OFO_ACCOUNT( object )));
		for( it=children ; it ; it=it->next ){
			remove_row_by_number( store, ofo_account_get_number( OFO_ACCOUNT( it->data )));
		}
		if( !ofa_prefs_account_delete_root_with_children()){
			for( it=children ; it ; it=it->next ){
				insert_row( store, dossier, OFO_ACCOUNT( it->data ));
			}
		}
		g_object_unref( OFO_ACCOUNT( object ));
		g_list_free( children );
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaAccountStore *store )
{
	static const gchar *thisfn = "ofa_account_store_on_reload_dataset";

	g_debug( "%s: dossier=%p, type=%lu, store=%p",
			thisfn, ( void * ) dossier, type, ( void * ) store );

	if( type == OFO_TYPE_ACCOUNT ){
		gtk_tree_store_clear( GTK_TREE_STORE( store ));
		tree_store_load_dataset( OFA_TREE_STORE( store ));
	}
}

/**
 * ofa_account_store_get_by_number:
 * @instance:
 * @number:
 * @iter:
 *
 * Set the iter to the specified row.
 *
 * Returns: %TRUE if found.
 */
gboolean
ofa_account_store_get_by_number( ofaAccountStore *store, const gchar *number, GtkTreeIter *iter )
{
	gboolean found;

	g_return_val_if_fail( store && OFA_IS_ACCOUNT_STORE( store ), FALSE );

	found = find_row_by_number( store, number, iter, NULL );

	return( found );
}
