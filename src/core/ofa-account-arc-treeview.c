/*
 * Open Firm AccountArcing
 * A double-entry account_arcing application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm AccountArcing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm AccountArcing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm AccountArcing; see the file COPYING. If not,
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
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"

#include "core/ofa-account-arc-store.h"
#include "core/ofa-account-arc-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
}
	ofaAccountArcTreeviewPrivate;

static void setup_columns( ofaAccountArcTreeview *self );
static void setup_store( ofaAccountArcTreeview *self, ofoAccount *account );
static gint tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaAccountArcTreeview, ofa_account_arc_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaAccountArcTreeview ))

static void
account_arc_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_arc_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_ARC_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_arc_treeview_parent_class )->finalize( instance );
}

static void
account_arc_treeview_dispose( GObject *instance )
{
	ofaAccountArcTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_ARC_TREEVIEW( instance ));

	priv = ofa_account_arc_treeview_get_instance_private( OFA_ACCOUNT_ARC_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_arc_treeview_parent_class )->dispose( instance );
}

static void
ofa_account_arc_treeview_init( ofaAccountArcTreeview *self )
{
	static const gchar *thisfn = "ofa_account_arc_treeview_init";
	ofaAccountArcTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_ACCOUNT_ARC_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_account_arc_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_account_arc_treeview_class_init( ofaAccountArcTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_account_arc_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_arc_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_arc_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;
}

/**
 * ofa_account_arc_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @account: the #ofoAccount.
 *
 * Define the treeview along with the subjacent store.
 *
 * Returns: a new instance.
 */
ofaAccountArcTreeview *
ofa_account_arc_treeview_new( ofaIGetter *getter, ofoAccount *account )
{
	ofaAccountArcTreeview *view;
	ofaAccountArcTreeviewPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

	view = g_object_new( OFA_TYPE_ACCOUNT_ARC_TREEVIEW,
				"ofa-tvbin-getter", getter,
				NULL );

	priv = ofa_account_arc_treeview_get_instance_private( view );

	priv->getter = getter;

	setup_columns( view );
	setup_store( view, account );

	return( view );
}

/*
 * Defines the treeview columns.
 * All the columns are visible (no user settings).
 */
static void
setup_columns( ofaAccountArcTreeview *self )
{
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ACCOUNT_ARC_COL_DATE,      _( "Date" ),   NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_ARC_COL_DEBIT,     _( "Debit" ),  NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_ARC_COL_SYMBOL1,      " ",        NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_ARC_COL_CREDIT,    _( "Credit" ), NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_ARC_COL_SYMBOL2,      " ",        NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), ACCOUNT_ARC_COL_TYPE,      _( "Type" ),   NULL );

	ofa_itvcolumnable_show_columns_all( OFA_ITVCOLUMNABLE( self ));

	ofa_itvcolumnable_twins_group_new(
			OFA_ITVCOLUMNABLE( self ), "amount", ACCOUNT_ARC_COL_DEBIT, ACCOUNT_ARC_COL_CREDIT, -1 );
}

static void
setup_store( ofaAccountArcTreeview *self, ofoAccount *account )
{
	ofaAccountArcTreeviewPrivate *priv;
	ofaAccountArcStore *store;

	priv = ofa_account_arc_treeview_get_instance_private( self );

	store = ofa_account_arc_store_new( priv->getter, account );

	ofa_tvbin_set_store( OFA_TVBIN( self ), GTK_TREE_MODEL( store ));
}

/*
 * This is the main goal of these #ofaAccountArcStore, #ofaAccountArcTreeview:
 * being able to sort the archived soldes by date.
 */
static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_account_arc_treeview_v_sort";
	ofaAccountArcTreeviewPrivate *priv;
	gint cmp;
	gchar *sdatea, *debita, *credita, *symbola, *stypea;
	gchar *sdateb, *debitb, *creditb, *symbolb, *stypeb;

	priv = ofa_account_arc_treeview_get_instance_private( OFA_ACCOUNT_ARC_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			ACCOUNT_ARC_COL_DATE,    &sdatea,
			ACCOUNT_ARC_COL_DEBIT,   &debita,
			ACCOUNT_ARC_COL_SYMBOL1, &symbola,
			ACCOUNT_ARC_COL_CREDIT,  &credita,
			ACCOUNT_ARC_COL_TYPE,    &stypea,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ACCOUNT_ARC_COL_DATE,    &sdateb,
			ACCOUNT_ARC_COL_DEBIT,   &debitb,
			ACCOUNT_ARC_COL_SYMBOL1, &symbolb,
			ACCOUNT_ARC_COL_CREDIT,  &creditb,
			ACCOUNT_ARC_COL_TYPE,    &stypeb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case ACCOUNT_ARC_COL_DATE:
			cmp = my_date_compare_by_str( sdatea, sdateb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case ACCOUNT_ARC_COL_DEBIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), debita, debitb );
			break;
		case ACCOUNT_ARC_COL_CREDIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), credita, creditb );
			break;
		case ACCOUNT_ARC_COL_SYMBOL1:
		case ACCOUNT_ARC_COL_SYMBOL2:
			cmp = my_collate( symbola, symbolb );
			break;
		case ACCOUNT_ARC_COL_TYPE:
			cmp = my_collate( stypea, stypeb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( sdatea );
	g_free( debita );
	g_free( credita );
	g_free( symbola );
	g_free( stypea );

	g_free( sdateb );
	g_free( debitb );
	g_free( creditb );
	g_free( symbolb );
	g_free( stypeb );

	return( cmp );
}
