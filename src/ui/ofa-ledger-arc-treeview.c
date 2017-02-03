/*
 * Open Firm LedgerArcing
 * A double-entry ledger_arcing application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm LedgerArcing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm LedgerArcing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm LedgerArcing; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-ledger-arc-store.h"
#include "ui/ofa-ledger-arc-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
}
	ofaLedgerArcTreeviewPrivate;

static void setup_columns( ofaLedgerArcTreeview *self );
static void setup_store( ofaLedgerArcTreeview *self, ofoLedger *ledger );
static gint tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaLedgerArcTreeview, ofa_ledger_arc_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaLedgerArcTreeview ))

static void
ledger_arc_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_arc_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_ARC_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_arc_treeview_parent_class )->finalize( instance );
}

static void
ledger_arc_treeview_dispose( GObject *instance )
{
	ofaLedgerArcTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_ARC_TREEVIEW( instance ));

	priv = ofa_ledger_arc_treeview_get_instance_private( OFA_LEDGER_ARC_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_arc_treeview_parent_class )->dispose( instance );
}

static void
ofa_ledger_arc_treeview_init( ofaLedgerArcTreeview *self )
{
	static const gchar *thisfn = "ofa_ledger_arc_treeview_init";
	ofaLedgerArcTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_LEDGER_ARC_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ledger_arc_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ledger_arc_treeview_class_init( ofaLedgerArcTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_arc_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_arc_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_arc_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;
}

/**
 * ofa_ledger_arc_treeview_new:
 * @getterter: a #ofaIGetter instance.
 * @ledger: the #ofoLedger.
 *
 * Define the treeview along with the subjacent store.
 *
 * Returns: a new instance.
 */
ofaLedgerArcTreeview *
ofa_ledger_arc_treeview_new( ofaIGetter *getter, ofoLedger *ledger )
{
	ofaLedgerArcTreeview *view;
	ofaLedgerArcTreeviewPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), NULL );

	view = g_object_new( OFA_TYPE_LEDGER_ARC_TREEVIEW,
					"ofa-tvbin-getter", getter,
					NULL );

	priv = ofa_ledger_arc_treeview_get_instance_private( view );

	priv->getter = getter;

	setup_columns( view );
	setup_store( view, ledger );

	return( view );
}

/*
 * Defines the treeview columns.
 * All the columns are visible (no user settings).
 */
static void
setup_columns( ofaLedgerArcTreeview *self )
{
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), LEDGER_ARC_COL_DATE,      _( "Date" ),     NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), LEDGER_ARC_COL_ISO,       _( "Currency" ), NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), LEDGER_ARC_COL_DEBIT,     _( "Debit" ),    NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), LEDGER_ARC_COL_SYMBOL1,      " ",          NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), LEDGER_ARC_COL_CREDIT,    _( "Credit" ),   NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), LEDGER_ARC_COL_SYMBOL2,      " ",          NULL );

	ofa_itvcolumnable_show_columns_all( OFA_ITVCOLUMNABLE( self ));

	ofa_itvcolumnable_twins_group_new(
			OFA_ITVCOLUMNABLE( self ), "amount", LEDGER_ARC_COL_DEBIT, LEDGER_ARC_COL_CREDIT, -1 );
}

static void
setup_store( ofaLedgerArcTreeview *self, ofoLedger *ledger )
{
	ofaLedgerArcTreeviewPrivate *priv;
	ofaLedgerArcStore *store;

	priv = ofa_ledger_arc_treeview_get_instance_private( self );

	store = ofa_ledger_arc_store_new( priv->getter, ledger );

	ofa_tvbin_set_store( OFA_TVBIN( self ), GTK_TREE_MODEL( store ));
}

/*
 * This is the main goal of these #ofaLedgerArcStore, #ofaLedgerArcTreeview:
 * being able to sort the archived soldes by date.
 */
static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_ledger_arc_treeview_v_sort";
	ofaLedgerArcTreeviewPrivate *priv;
	gint cmp;
	gchar *sdatea, *isoa, *debita, *credita, *symbola;
	gchar *sdateb, *isob, *debitb, *creditb, *symbolb;

	priv = ofa_ledger_arc_treeview_get_instance_private( OFA_LEDGER_ARC_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			LEDGER_ARC_COL_DATE,    &sdatea,
			LEDGER_ARC_COL_ISO,     &isoa,
			LEDGER_ARC_COL_DEBIT,   &debita,
			LEDGER_ARC_COL_SYMBOL1, &symbola,
			LEDGER_ARC_COL_CREDIT,  &credita,
			-1 );

	gtk_tree_model_get( tmodel, b,
			LEDGER_ARC_COL_DATE,    &sdateb,
			LEDGER_ARC_COL_ISO,     &isob,
			LEDGER_ARC_COL_DEBIT,   &debitb,
			LEDGER_ARC_COL_SYMBOL1, &symbolb,
			LEDGER_ARC_COL_CREDIT,  &creditb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case LEDGER_ARC_COL_DATE:
			cmp = my_date_compare_by_str( sdatea, sdateb, ofa_prefs_date_display( priv->getter ));
			break;
		case LEDGER_ARC_COL_ISO:
			cmp = my_collate( isoa, isob );
			break;
		case LEDGER_ARC_COL_DEBIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), debita, debitb );
			break;
		case LEDGER_ARC_COL_CREDIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), credita, creditb );
			break;
		case LEDGER_ARC_COL_SYMBOL1:
		case LEDGER_ARC_COL_SYMBOL2:
			cmp = my_collate( symbola, symbolb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( sdatea );
	g_free( isoa );
	g_free( debita );
	g_free( credita );
	g_free( symbola );

	g_free( sdateb );
	g_free( isob );
	g_free( debitb );
	g_free( creditb );
	g_free( symbolb );

	return( cmp );
}
