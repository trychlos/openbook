/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "core/ofa-preferences.h"

#include "ui/ofa-account-istore.h"

/* data associated to each implementor object
 */
typedef struct {

	/* static data
	 * to be set at initialization time
	 */
	ofaAccountColumns columns;
	ofoDossier        *dossier;

	/* runtime data
	 */
	GList             *handlers;
	GList             *stores;			/* a GtkTreeStore per class number */
}
	sIStore;

/* columns ordering in the store
 */
enum {
	COL_NUMBER = 0,
	COL_LABEL,
	COL_CURRENCY,
	COL_TYPE,
	COL_NOTES,
	COL_UPD_USER,
	COL_UPD_STAMP,
	COL_VAL_DEBIT,
	COL_VAL_CREDIT,
	COL_ROUGH_DEBIT,
	COL_ROUGH_CREDIT,
	COL_OPEN_DEBIT,
	COL_OPEN_CREDIT,
	COL_FUT_DEBIT,
	COL_FUT_CREDIT,
	COL_SETTLEABLE,
	COL_RECONCILIABLE,
	COL_FORWARD,
	COL_EXE_DEBIT,
	COL_EXE_CREDIT,
	COL_OBJECT,
	N_COLUMNS
};

#define ACCOUNT_ISTORE_LAST_VERSION     1

/* data attached to the implementor object (e.g. a treeview) */
#define ACCOUNT_ISTORE_DATA             "ofa-account-istore-data"

/* data attached to each managed GtkTreeStore */
#define ACCOUNT_ISTORE_CLASS            "ofa-account-istore-class"

/* a structure used when moving a subtree to another place
 */
typedef struct {
	sIStore             *sdata;
	ofoAccount          *account;
	GtkTreeModel        *tmodel;
	GtkTreeRowReference *ref;
}
	sChild;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations         = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaAccountIStoreInterface *klass );
static void          interface_base_finalize( ofaAccountIStoreInterface *klass );
static void          load_dataset( ofaAccountIStore *instance, sIStore *sdata );
static void          insert_row( ofaAccountIStore *instance, GtkTreeStore *store, sIStore *sdata, const ofoAccount *account );
static void          set_row( ofaAccountIStore *instance, GtkTreeStore *store, sIStore *sdata, GtkTreeIter *iter, const ofoAccount *account );
static gboolean      find_parent_iter( ofaAccountIStore *instance, const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *parent_iter );
static gboolean      find_row_by_number( ofaAccountIStore *instance, const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gboolean *bvalid );
static gboolean      find_row_by_number_rec( ofaAccountIStore *instance, const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gint *last );
static void          realign_children( ofaAccountIStore *instance, GtkTreeStore *store, sIStore *sdata, GtkTreeIter *parent_iter, const ofoAccount *account );
static GList        *realign_children_rec( const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter, GList *children );
static void          realign_children_move( sChild *child_str, ofaAccountIStore *instance );
static void          child_free( sChild *child_str );
static void          remove_row_by_number( ofaAccountIStore *instance, sIStore *sdata, const gchar *number );
static void          setup_signaling_connect( ofaAccountIStore *instance, sIStore *sdata );
static void          on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountIStore *instance );
static void          on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountIStore *instance );
static void          on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountIStore *instance );
static void          on_reload_dataset( ofoDossier *dossier, GType type, ofaAccountIStore *instance );
static void          on_parent_finalized( ofaAccountIStore *instance, gpointer finalized_parent );
static void          on_object_finalized( sIStore *sdata, gpointer finalized_object );
static sIStore      *get_istore_data( ofaAccountIStore *instance );
static GtkTreeStore *get_tree_store( ofaAccountIStore *instance, sIStore *sdata, gint class );

/**
 * ofa_account_istore_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_account_istore_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_account_istore_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_account_istore_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaAccountIStoreInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaAccountIStore", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaAccountIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_account_istore_interface_base_init";
	GType interface_type = G_TYPE_FROM_INTERFACE( klass );

	g_debug( "%s: klass=%p (%s), st_initializations=%d",
			thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ), st_initializations );

	if( !st_initializations ){
		/* declare interface default methods here */

		/**
		 * ofaAccountIStore::changed:
		 *
		 * This signal is sent by the views when the selection is
		 * changed.
		 *
		 * Argument is the selected account number.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaAccountIStore *store,
		 * 						const gchar    *number,
		 * 						gpointer        user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"changed",
					interface_type,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );

		/**
		 * ofaAccountIStore::activated:
		 *
		 * This signal is sent by the views when the selection is
		 * activated.
		 *
		 * Argument is the selected account number.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaAccountIStore *store,
		 * 						const gchar    *number,
		 * 						gpointer        user_data );
		 */
		st_signals[ ACTIVATED ] = g_signal_new_class_handler(
					"activated",
					interface_type,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaAccountIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_account_istore_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_account_istore_get_interface_last_version:
 * @instance: this #ofaAccountIStore instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_account_istore_get_interface_last_version( const ofaAccountIStore *instance )
{
	return( ACCOUNT_ISTORE_LAST_VERSION );
}

/**
 * ofa_account_istore_attach_to:
 * @instance: this #ofaAccountIStore instance.
 * @parent: the #GtkContainer to which the widget should be attached.
 *
 * Attach the widget to its parent.
 *
 * A weak notify reference is put on the parent, so that the GObject
 * will be unreffed when the parent will be destroyed.
 */
void
ofa_account_istore_attach_to( ofaAccountIStore *instance, GtkContainer *parent )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_ACCOUNT_ISTORE( instance ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, instance );

	if( OFA_ACCOUNT_ISTORE_GET_INTERFACE( instance )->attach_to ){
		OFA_ACCOUNT_ISTORE_GET_INTERFACE( instance )->attach_to( instance, parent );
	}

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

/**
 * ofa_account_istore_set_columns:
 * @instance: this #ofaAccountIStore instance.
 * @columns: the columns to be displayed from the #GtkTreeStore.
 */
void
ofa_account_istore_set_columns( ofaAccountIStore *instance, ofaAccountColumns columns )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_ACCOUNT_ISTORE( instance ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->columns = columns;

	load_dataset( instance, sdata );
}

/**
 * ofa_account_istore_set_dossier:
 * @instance: this #ofaAccountIStore instance.
 * @dossier: the opened #ofoDossier.
 *
 * Set the dossier and load the corresponding dataset.
 * Connect to the dossier signaling system in order to maintain the
 * dataset up to date.
 */
void
ofa_account_istore_set_dossier( ofaAccountIStore *instance, ofoDossier *dossier )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_ACCOUNT_ISTORE( instance ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->dossier = dossier;

	setup_signaling_connect( instance, sdata );

	load_dataset( instance, sdata );
}

/*
 * load the dataset when columns and dossier have been both set
 */
static void
load_dataset( ofaAccountIStore *instance, sIStore *sdata )
{
	const GList *dataset, *it;
	ofoAccount *account;
	GtkTreeStore *store;

	if( sdata->columns && sdata->dossier ){

		dataset = ofo_account_get_dataset( sdata->dossier );

		for( it=dataset ; it ; it=it->next ){
			account = OFO_ACCOUNT( it->data );
			store = get_tree_store( instance, sdata, ofo_account_get_class( account ));
			insert_row( instance, store, sdata, account );
		}
	}
}

static void
insert_row( ofaAccountIStore *instance, GtkTreeStore *store, sIStore *sdata, const ofoAccount *account )
{
	GtkTreeIter parent_iter, iter;
	gboolean parent_found;

	parent_found = find_parent_iter( instance, account, GTK_TREE_MODEL( store ), &parent_iter );
	gtk_tree_store_insert_with_values(
			store,
			&iter,
			parent_found ? &parent_iter : NULL,
			-1,
			COL_NUMBER, ofo_account_get_number( account ),
			COL_OBJECT, account,
			-1 );

	set_row( instance, store, sdata, &iter, account );
	realign_children( instance, store, sdata, &iter, account );
}

static void
set_row( ofaAccountIStore *instance, GtkTreeStore *store, sIStore *sdata, GtkTreeIter *iter, const ofoAccount *account )
{
	const gchar *currency_code;
	gint digits;
	ofoCurrency *currency_obj;
	gchar *stamp;
	gchar *svdeb, *svcre, *srdeb, *srcre, *sodeb, *socre, *sfdeb, *sfcre, *sedeb, *secre;
	ofxAmount val_debit, val_credit, rough_debit, rough_credit;

	currency_code = ofo_account_get_currency( account );
	if( !ofo_account_is_root( account )){
		currency_obj = ofo_currency_get_by_code( sdata->dossier, currency_code );
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
	stamp  = my_utils_stamp_to_str( ofo_account_get_upd_stamp( account ), MY_STAMP_DMYYHM );

	gtk_tree_store_set(
			store,
			iter,
			COL_LABEL,         ofo_account_get_label( account ),
			COL_CURRENCY,      currency_code,
			COL_TYPE,          ofo_account_get_type_account( account ),
			COL_NOTES,         ofo_account_get_notes( account ),
			COL_UPD_USER,      ofo_account_get_upd_user( account ),
			COL_UPD_STAMP,     stamp,
			COL_VAL_DEBIT,     svdeb,
			COL_VAL_CREDIT,    svcre,
			COL_ROUGH_DEBIT,   srdeb,
			COL_ROUGH_CREDIT,  srcre,
			COL_OPEN_DEBIT,    sodeb,
			COL_OPEN_CREDIT,   socre,
			COL_FUT_DEBIT,     sfdeb,
			COL_FUT_CREDIT,    sfcre,
			COL_SETTLEABLE,    ofo_account_is_settleable( account ) ? ACCOUNT_SETTLEABLE : "",
			COL_RECONCILIABLE, ofo_account_is_reconciliable( account ) ? ACCOUNT_RECONCILIABLE : "",
			COL_FORWARD,       ofo_account_is_forward( account ) ? ACCOUNT_FORWARDABLE : "",
			COL_EXE_DEBIT,     sedeb,
			COL_EXE_CREDIT,    secre,
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
 * @self: this #ofaAccountIStore
 * @account: [in]: the account for which we are searching the closest
 *  parent
 * @tmodel: [in]: the GtkTreeModel.
 * @parent_iter: [out]: the GtkTreeIter of the closes parent if found.
 *
 *
 * Search for the GtkTreeIter corresponding to the closest parent of
 * this account
 *
 * Returns %TRUE if a parent has been found, and @parent_iter adresses
 * this parent, else @parent_iter is undefined.
 */
static gboolean
find_parent_iter( ofaAccountIStore *instance,
						const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *parent_iter )
{
	const gchar *number;
	gchar *candidate_number;
	gboolean found;

	number = ofo_account_get_number( account );
	candidate_number = g_strdup( number );
	found = FALSE;

	while( !found && g_utf8_strlen( candidate_number, -1 ) > 1 ){
		candidate_number[g_utf8_strlen( candidate_number, -1 )-1] = '\0';
		found = find_row_by_number( instance, candidate_number, tmodel, parent_iter, NULL );
	}

	g_free( candidate_number );

	return( found );
}

/*
 * find_row_by_number:
 * @self: this #ofaAccountIStore
 * @number: [in]: the account number we are searching for.
 * @tmodel: [in]: the GtkTreeModel.
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
find_row_by_number( ofaAccountIStore *instance,
							const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gboolean *bvalid )
{
	gint last;
	gboolean found;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		if( bvalid ){
			*bvalid = TRUE;
		}
		last = -1;
		found = find_row_by_number_rec( instance, number, tmodel, iter, &last );
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
find_row_by_number_rec( ofaAccountIStore *instance,
								const gchar *number, GtkTreeModel *tmodel, GtkTreeIter *iter, gint *last )
{
	gchar *cmp_number;
	GtkTreeIter cmp_iter, child_iter, last_iter;

	cmp_iter = *iter;

	while( TRUE ){
		gtk_tree_model_get( tmodel, &cmp_iter, COL_NUMBER, &cmp_number, -1 );
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
		if( gtk_tree_model_iter_children( tmodel, &child_iter, &cmp_iter )){
			find_row_by_number_rec( instance, number, tmodel, &child_iter, last );
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
		if( !gtk_tree_model_iter_next( tmodel, &cmp_iter )){
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
realign_children( ofaAccountIStore *instance, GtkTreeStore *store, sIStore *sdata, GtkTreeIter *parent_iter, const ofoAccount *account )
{
	static const gchar *thisfn = "ofa_account_istore_realign_children";
	GList *children;
	GtkTreeIter iter;

	if( gtk_tree_model_iter_has_child( GTK_TREE_MODEL( store ), parent_iter )){
		g_warning( "%s: newly inserted row already has at least one child", thisfn );

	} else {
		children = NULL;
		iter = *parent_iter;
		children = realign_children_rec( account, GTK_TREE_MODEL( store ), &iter, children );
		if( children ){
			g_list_foreach( children, ( GFunc ) realign_children_move, instance );
			g_list_free_full( children, ( GDestroyNotify ) child_free );
		}
	}
}

/*
 * tstore_realign_stored_children_rec:
 * copy into 'children' GList all children accounts, along with their
 * row reference - it will so be easy to remove them from the model,
 * then reinsert these same accounts
 */
static GList *
realign_children_rec( const ofoAccount *account, GtkTreeModel *tmodel, GtkTreeIter *iter, GList *children )
{
	ofoAccount *candidate;
	GtkTreeIter child_iter;
	GtkTreePath *path;
	sChild *child_str;

	while( gtk_tree_model_iter_next( tmodel, iter )){
		gtk_tree_model_get( tmodel, iter, COL_OBJECT, &candidate, -1 );
		g_object_unref( candidate );

		if( ofo_account_is_child_of( account, candidate )){
			child_str = g_new0( sChild, 1 );
			path = gtk_tree_model_get_path( tmodel, iter );
			child_str->account = g_object_ref( candidate );
			child_str->tmodel = tmodel;
			child_str->ref = gtk_tree_row_reference_new( tmodel, path );
			children = g_list_prepend( children, child_str );
			gtk_tree_path_free( path );

			if( gtk_tree_model_iter_children( tmodel, &child_iter, iter )){
				children = realign_children_rec( account, tmodel, &child_iter, children );
			}

		} else {
			break;
		}
	}

	return( children );
}

static void
realign_children_move( sChild *child_str, ofaAccountIStore *instance )
{
	GtkTreePath *path;
	GtkTreeIter iter;

	path = gtk_tree_row_reference_get_path( child_str->ref );
	if( path && gtk_tree_model_get_iter( child_str->tmodel, &iter, path )){
		gtk_tree_store_remove( GTK_TREE_STORE( child_str->tmodel ), &iter );
		gtk_tree_path_free( path );
		insert_row( instance, GTK_TREE_STORE( child_str->tmodel ), child_str->sdata, child_str->account );
	}
}

static void
child_free( sChild *child_str )
{
	g_object_unref( child_str->account );
	gtk_tree_row_reference_free( child_str->ref );
	g_free( child_str );
}

static void
remove_row_by_number( ofaAccountIStore *instance, sIStore *sdata, const gchar *number )
{
	GtkTreeStore *store;
	GtkTreeIter iter;

	store = get_tree_store( instance, sdata, ofo_account_get_class_from_number( number ));
	if( find_row_by_number( instance, number, GTK_TREE_MODEL( store ), &iter, NULL )){
		gtk_tree_store_remove( store, &iter );
	}
}

static void
setup_signaling_connect( ofaAccountIStore *instance, sIStore *sdata )
{
	gulong handler;

	handler = g_signal_connect( G_OBJECT(
					sdata->dossier ),
					SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( sdata->dossier ),
					SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( sdata->dossier ),
					SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( sdata->dossier ),
					SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), instance );
	sdata->handlers = g_list_prepend( sdata->handlers, ( gpointer ) handler );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaAccountIStore *instance )
{
	static const gchar *thisfn = "ofa_account_istore_on_new_object";
	sIStore *sdata;
	GtkTreeStore *store;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_ACCOUNT( object )){
		store = get_tree_store( instance, sdata, ofo_account_get_class( OFO_ACCOUNT( object )));
		insert_row( instance, store, sdata, OFO_ACCOUNT( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaAccountIStore *instance )
{
	static const gchar *thisfn = "ofa_account_istore_on_updated_object";
	sIStore *sdata;
	GtkTreeStore *store;
	GtkTreeIter iter;
	const gchar *number;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_ACCOUNT( object )){
		number = ofo_account_get_number( OFO_ACCOUNT( object ));

		if( prev_id && g_utf8_collate( prev_id, number )){
			remove_row_by_number( instance, sdata, prev_id );
			store = get_tree_store( instance, sdata, ofo_account_get_class( OFO_ACCOUNT( object )));
			insert_row( instance, store, sdata, OFO_ACCOUNT( object ));

		} else {
			store = get_tree_store( instance, sdata, ofo_account_get_class( OFO_ACCOUNT( object )));
			find_row_by_number( instance, number, GTK_TREE_MODEL( store ), &iter, NULL );
			set_row( instance, store, sdata, &iter, OFO_ACCOUNT( object ));
		}
	}
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaAccountIStore *instance )
{
	static const gchar *thisfn = "ofa_account_istore_on_deleted_object";
	sIStore *sdata;
	GList *children, *it;
	GtkTreeStore *store;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( OFO_IS_ACCOUNT( object )){
		children = ofo_account_get_children( OFO_ACCOUNT( object ), sdata->dossier );
		g_object_ref( OFO_ACCOUNT( object ));
		remove_row_by_number( instance, sdata, ofo_account_get_number( OFO_ACCOUNT( object )));
		for( it=children ; it ; it=it->next ){
			remove_row_by_number( instance, sdata, ofo_account_get_number( OFO_ACCOUNT( it->data )));
		}
		if( !ofa_prefs_account_delete_root_with_children()){
			store = get_tree_store( instance, sdata, ofo_account_get_class( OFO_ACCOUNT( object )));
			for( it=children ; it ; it=it->next ){
				insert_row( instance, store, sdata, OFO_ACCOUNT( it->data ));
			}
		}
		g_object_unref( OFO_ACCOUNT( object ));
		g_list_free( children );
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaAccountIStore *instance )
{
	static const gchar *thisfn = "ofa_account_istore_on_reload_dataset";
	sIStore *sdata;

	g_debug( "%s: dossier=%p, type=%lu, instance=%p",
			thisfn, ( void * ) dossier, type, ( void * ) instance );

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	if( type == OFO_TYPE_ACCOUNT ){
		g_list_free_full( sdata->stores, ( GDestroyNotify ) g_object_unref );
		load_dataset( instance, sdata );
	}
}

/**
 * ofa_account_istore_get_column_number:
 * @instance: this #ofaAccountIStore instance.
 * @column: the #ofaAccountColumns identifier.
 *
 * Returns: the number of the column in the store, counted from zero,
 * or -1 if the column is unknown.
 */
gint
ofa_account_istore_get_column_number( const ofaAccountIStore *instance, ofaAccountColumns column )
{
	static const gchar *thisfn = "ofa_account_istore_get_column_number";

	switch( column ){
		case ACCOUNT_COL_NUMBER:
			return( COL_NUMBER );
			break;

		case ACCOUNT_COL_LABEL:
			return( COL_LABEL );
			break;

		case ACCOUNT_COL_CURRENCY:
			return( COL_CURRENCY );
			break;

		case ACCOUNT_COL_TYPE:
			return( COL_TYPE );
			break;

		case ACCOUNT_COL_NOTES:
			return( COL_NOTES );
			break;

		case ACCOUNT_COL_UPD_USER:
			return( COL_UPD_USER );
			break;

		case ACCOUNT_COL_UPD_STAMP:
			return( COL_UPD_STAMP );
			break;

		case ACCOUNT_COL_VAL_DEBIT:
			return( COL_VAL_DEBIT );
			break;

		case ACCOUNT_COL_VAL_CREDIT:
			return( COL_VAL_CREDIT );
			break;

		case ACCOUNT_COL_ROUGH_DEBIT:
			return( COL_ROUGH_DEBIT );
			break;

		case ACCOUNT_COL_ROUGH_CREDIT:
			return( COL_ROUGH_CREDIT );
			break;

		case ACCOUNT_COL_OPEN_DEBIT:
			return( COL_OPEN_DEBIT );
			break;

		case ACCOUNT_COL_OPEN_CREDIT:
			return( COL_OPEN_CREDIT );
			break;

		case ACCOUNT_COL_FUT_DEBIT:
			return( COL_FUT_DEBIT );
			break;

		case ACCOUNT_COL_FUT_CREDIT:
			return( COL_FUT_CREDIT );
			break;

		case ACCOUNT_COL_SETTLEABLE:
			return( COL_SETTLEABLE );
			break;

		case ACCOUNT_COL_RECONCILIABLE:
			return( COL_RECONCILIABLE );
			break;

		case ACCOUNT_COL_FORWARD:
			return( COL_FORWARD );
			break;

		case ACCOUNT_COL_EXE_DEBIT:
			return( COL_EXE_DEBIT );
			break;

		case ACCOUNT_COL_EXE_CREDIT:
			return( COL_EXE_CREDIT );
			break;
	}

	g_warning( "%s: unknown column:%d", thisfn, column );
	return( -1 );
}

static void
on_parent_finalized( ofaAccountIStore *instance, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_account_istore_on_parent_finalized";

	g_debug( "%s: instance=%p, finalized_parent=%p",
			thisfn, ( void * ) instance, ( void * ) finalized_parent );

	g_return_if_fail( instance );

	g_object_unref( G_OBJECT( instance ));
}

static void
on_object_finalized( sIStore *sdata, gpointer finalized_object )
{
	static const gchar *thisfn = "ofa_account_istore_on_object_finalized";
	GList *it;

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_return_if_fail( sdata );

	if( sdata->dossier && OFO_IS_DOSSIER( sdata->dossier )){
		for( it=sdata->handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( sdata->dossier, ( gulong ) it->data );
		}
	}

	g_free( sdata );
}

static sIStore *
get_istore_data( ofaAccountIStore *instance )
{
	sIStore *sdata;

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( instance ), ACCOUNT_ISTORE_DATA );

	if( !sdata ){
		sdata = g_new0( sIStore, 1 );
		g_object_set_data( G_OBJECT( instance ), ACCOUNT_ISTORE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_object_finalized, sdata );
	}

	return( sdata );
}

static GtkTreeStore *
get_tree_store( ofaAccountIStore *instance, sIStore *sdata, gint class )
{
	GtkTreeStore *store;
	gint number;
	GList *it;

	store = NULL;
	for( it=sdata->stores ; it ; it=it->next ){
		number = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( it->data ), ACCOUNT_ISTORE_CLASS ));
		if( number == class ){
			return( GTK_TREE_STORE( it->data ));
		}
	}

	store = gtk_tree_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* number, label, currency */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* type, notes, upd_user */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* upd_stamp, val_debit, val_credit */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* upd_stamp, val_debit, val_credit */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* rough_debit, rough_credit, open_debit */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* open_credit, fut_debit, fut_credit */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* settle, reconciliable, forward */
			G_TYPE_STRING, G_TYPE_STRING, 					/* exe_debit, exe_credit */
			G_TYPE_OBJECT );								/* the #ofoAccount itself */

	if( OFA_ACCOUNT_ISTORE_GET_INTERFACE( instance )->set_columns ){
		OFA_ACCOUNT_ISTORE_GET_INTERFACE( instance )->set_columns( instance, store, sdata->columns );
	}

	g_object_unref( store );

	g_object_set_data( G_OBJECT( store ), ACCOUNT_ISTORE_CLASS, GINT_TO_POINTER( class ));
	sdata->stores = g_list_prepend( sdata->stores, store );

	return( store );
}
