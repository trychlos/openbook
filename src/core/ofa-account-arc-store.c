/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"

#include "core/ofa-account-arc-store.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime
	 */
}
	ofaAccountArcStorePrivate;

static GType st_col_types[ACCOUNT_ARC_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* date, debit, symbol1 */
		G_TYPE_STRING, G_TYPE_STRING,					/* credit, symbol2 */
		G_TYPE_OBJECT, G_TYPE_OBJECT					/* #ofoAccount, #ofoCurrency */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountArcStore *self );
static void     load_dataset( ofaAccountArcStore *self, ofoAccount *account );
static void     insert_row( ofaAccountArcStore *self, ofoAccount *account, gint i );
static void     set_row_by_iter( ofaAccountArcStore *self, ofoAccount *account, gint i, GtkTreeIter *iter );

G_DEFINE_TYPE_EXTENDED( ofaAccountArcStore, ofa_account_arc_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaAccountArcStore ))

static void
account_arc_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_arc_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_ARC_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_arc_store_parent_class )->finalize( instance );
}

static void
account_arc_store_dispose( GObject *instance )
{
	ofaAccountArcStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_ARC_STORE( instance ));

	priv = ofa_account_arc_store_get_instance_private( OFA_ACCOUNT_ARC_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_arc_store_parent_class )->dispose( instance );
}

static void
ofa_account_arc_store_init( ofaAccountArcStore *self )
{
	static const gchar *thisfn = "ofa_account_arc_store_init";
	ofaAccountArcStorePrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_ARC_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_account_arc_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_account_arc_store_class_init( ofaAccountArcStoreClass *klass )
{
	static const gchar *thisfn = "ofa_account_arc_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_arc_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_arc_store_finalize;
}

/**
 * ofa_account_arc_store_new:
 * @account: the #ofoAccount.
 *
 * Load the archived soldes of the account.
 *
 * Returns: a new reference to the #ofaAccountArcStore object.
 */
ofaAccountArcStore *
ofa_account_arc_store_new( ofoAccount *account )
{
	ofaAccountArcStore *store;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

	store = g_object_new( OFA_TYPE_ACCOUNT_ARC_STORE, NULL );

	gtk_list_store_set_column_types(
			GTK_LIST_STORE( store ), ACCOUNT_ARC_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	load_dataset( store, account );

	return( store );
}

/*
 * sorting the self per account_arc code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaAccountArcStore *self )
{
	gchar *sdatea, *sdateb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, ACCOUNT_ARC_COL_DATE, &sdatea, -1 );
	gtk_tree_model_get( tmodel, b, ACCOUNT_ARC_COL_DATE, &sdateb, -1 );

	cmp = my_date_compare_by_str( sdatea, sdateb, ofa_prefs_date_display());

	g_free( sdatea );
	g_free( sdateb );

	return( cmp );
}

static void
load_dataset( ofaAccountArcStore *self, ofoAccount *account )
{
	gint i, count;

	count = ofo_account_archive_get_count( account );

	for( i=0 ; i<count ; ++i ){
		insert_row( self, account, i );
	}
}

static void
insert_row( ofaAccountArcStore *self, ofoAccount *account, gint i )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, account, i, &iter );
}

static void
set_row_by_iter( ofaAccountArcStore *self, ofoAccount *account, gint i, GtkTreeIter *iter )
{
	gchar *sdate, *sdebit, *scredit;
	ofaHub *hub;
	ofoCurrency *currency;
	const gchar *symbol;

	hub = ofo_base_get_hub( OFO_BASE( account ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	currency = ofo_currency_get_by_code( hub, ofo_account_get_currency( account ));
	g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
	symbol = ofo_currency_get_symbol( currency );

	sdate = my_date_to_str( ofo_account_archive_get_date( account, i ), ofa_prefs_date_display());
	sdebit = ofa_amount_to_str( ofo_account_archive_get_debit( account, i ), currency );
	scredit = ofa_amount_to_str( ofo_account_archive_get_credit( account, i ), currency );

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			ACCOUNT_ARC_COL_DATE,     sdate,
			ACCOUNT_ARC_COL_DEBIT,    sdebit,
			ACCOUNT_ARC_COL_SYMBOL1,  symbol,
			ACCOUNT_ARC_COL_CREDIT,   scredit,
			ACCOUNT_ARC_COL_SYMBOL2,  symbol,
			ACCOUNT_ARC_COL_ACCOUNT,  account,
			ACCOUNT_ARC_COL_CURRENCY, currency,
			-1 );

	g_free( scredit );
	g_free( sdebit );
	g_free( sdate );
}
