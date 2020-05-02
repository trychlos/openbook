/*
 * Open Firm Ledgering
 * A double-entry ledgering application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Ledgering is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Ledgering is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Ledgering; see the file COPYING. If not,
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
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-ledger-arc-store.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	ofoLedger  *ledger;
}
	ofaLedgerArcStorePrivate;

static GType st_col_types[LEDGER_ARC_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* date, iso, debit */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* symbol1, credit, symbol2 */
		G_TYPE_OBJECT, G_TYPE_OBJECT					/* #ofoLedger, #ofoCurrency */
};

static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerArcStore *self );
static void load_dataset( ofaLedgerArcStore *self, ofoLedger *ledger );
static void insert_row( ofaLedgerArcStore *self, ofoLedger *ledger, gint i );
static void set_row_by_iter( ofaLedgerArcStore *self, ofoLedger *ledger, gint i, GtkTreeIter *iter );

G_DEFINE_TYPE_EXTENDED( ofaLedgerArcStore, ofa_ledger_arc_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaLedgerArcStore ))

static void
ledger_arc_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_arc_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_ARC_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_arc_store_parent_class )->finalize( instance );
}

static void
ledger_arc_store_dispose( GObject *instance )
{
	ofaLedgerArcStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_ARC_STORE( instance ));

	priv = ofa_ledger_arc_store_get_instance_private( OFA_LEDGER_ARC_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_arc_store_parent_class )->dispose( instance );
}

static void
ofa_ledger_arc_store_init( ofaLedgerArcStore *self )
{
	static const gchar *thisfn = "ofa_ledger_arc_store_init";
	ofaLedgerArcStorePrivate *priv;

	g_return_if_fail( OFA_IS_LEDGER_ARC_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ledger_arc_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ledger_arc_store_class_init( ofaLedgerArcStoreClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_arc_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_arc_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_arc_store_finalize;
}

/**
 * ofa_ledger_arc_store_new:
 * @getter: a #ofaIGetter instance.
 * @ledger: the #ofoLedger.
 *
 * Load the archived soldes of the ledger.
 *
 * Returns: a new reference to the #ofaLedgerArcStore object.
 */
ofaLedgerArcStore *
ofa_ledger_arc_store_new( ofaIGetter *getter, ofoLedger *ledger )
{
	ofaLedgerArcStore *store;
	ofaLedgerArcStorePrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );

	store = g_object_new( OFA_TYPE_LEDGER_ARC_STORE, NULL );

	priv = ofa_ledger_arc_store_get_instance_private( store );

	priv->getter = getter;
	priv->ledger = ledger;

	gtk_list_store_set_column_types(
			GTK_LIST_STORE( store ), LEDGER_ARC_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	load_dataset( store, ledger );

	return( store );
}

/*
 * sorting the self per date
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaLedgerArcStore *self )
{
	ofaLedgerArcStorePrivate *priv;
	gchar *sdatea, *sdateb;
	gint cmp;

	priv = ofa_ledger_arc_store_get_instance_private( self );

	gtk_tree_model_get( tmodel, a, LEDGER_ARC_COL_DATE, &sdatea, -1 );
	gtk_tree_model_get( tmodel, b, LEDGER_ARC_COL_DATE, &sdateb, -1 );

	cmp = my_date_compare_by_str( sdatea, sdateb, ofa_prefs_date_get_display_format( priv->getter ));

	g_free( sdatea );
	g_free( sdateb );

	return( cmp );
}

static void
load_dataset( ofaLedgerArcStore *self, ofoLedger *ledger )
{
	gint i, count;

	count = ofo_ledger_archive_get_count( ledger );

	for( i=0 ; i<count ; ++i ){
		insert_row( self, ledger, i );
	}
}

static void
insert_row( ofaLedgerArcStore *self, ofoLedger *ledger, gint i )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, ledger, i, &iter );
}

static void
set_row_by_iter( ofaLedgerArcStore *self, ofoLedger *ledger, gint i, GtkTreeIter *iter )
{
	ofaLedgerArcStorePrivate *priv;
	gchar *sdate, *sdebit, *scredit;
	ofoCurrency *currency;
	const gchar *iso, *symbol;
	const GDate *date;

	priv = ofa_ledger_arc_store_get_instance_private( self );

	iso = ofo_ledger_archive_get_currency( ledger, i );
	currency = ofo_currency_get_by_code( priv->getter, iso );
	g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
	symbol = ofo_currency_get_symbol( currency );

	date = ofo_ledger_archive_get_date( ledger, i );
	sdate = my_date_to_str( date, ofa_prefs_date_get_display_format( priv->getter ));
	sdebit = ofa_amount_to_str( ofo_ledger_archive_get_debit( ledger, iso, date ), currency, priv->getter );
	scredit = ofa_amount_to_str( ofo_ledger_archive_get_credit( ledger, iso, date ), currency, priv->getter );

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			LEDGER_ARC_COL_DATE,     sdate,
			LEDGER_ARC_COL_ISO,      iso,
			LEDGER_ARC_COL_DEBIT,    sdebit,
			LEDGER_ARC_COL_SYMBOL1,  symbol,
			LEDGER_ARC_COL_CREDIT,   scredit,
			LEDGER_ARC_COL_SYMBOL2,  symbol,
			LEDGER_ARC_COL_LEDGER,   ledger,
			LEDGER_ARC_COL_CURRENCY, currency,
			-1 );

	g_free( scredit );
	g_free( sdebit );
	g_free( sdate );
}
