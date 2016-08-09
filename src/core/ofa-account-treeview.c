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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-icontext.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-account-store.h"
#include "core/ofa-account-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* UI
	 */
	ofaAccountStore *store;
}
	ofaAccountTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        setup_columns( ofaAccountTreeview *self );
static void        on_selection_changed( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_activated( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_delete( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty );
static void        get_and_send( ofaAccountTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoAccount *get_selected_with_selection( ofaAccountTreeview *self, GtkTreeSelection *selection );

G_DEFINE_TYPE_EXTENDED( ofaAccountTreeview, ofa_account_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaAccountTreeview ))

static void
account_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_treeview_parent_class )->finalize( instance );
}

static void
account_treeview_dispose( GObject *instance )
{
	ofaAccountTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_TREEVIEW( instance ));

	priv = ofa_account_treeview_get_instance_private( OFA_ACCOUNT_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_treeview_parent_class )->dispose( instance );
}

static void
ofa_account_treeview_init( ofaAccountTreeview *self )
{
	static const gchar *thisfn = "ofa_account_treeview_init";
	ofaAccountTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_ACCOUNT_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_account_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_account_treeview_class_init( ofaAccountTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_account_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_treeview_finalize;

	//OFA_TVBIN_CLASS( klass )->sort = v_sort;

	/**
	 * ofaAccountTreeview::ofa-accchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaAccountTreeview proxyes it with this 'ofa-accchanged' signal,
	 * providing the #ofoAccount selected object.
	 *
	 * Argument is the current #ofoAccount object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountTreeview *view,
	 * 						ofoAccount       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-accchanged",
				OFA_TYPE_ACCOUNT_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaAccountTreeview::ofa-accactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaAccountTreeview proxyes it with this 'ofa-accactivated' signal,
	 * providing the #ofoAccount selected object.
	 *
	 * Argument is the current #ofoAccount object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountTreeview *view,
	 * 						ofoAccount       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-accactivated",
				OFA_TYPE_ACCOUNT_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaAccountTreeview::ofa-accdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaAccountTreeview proxyes it with this 'ofa-accdelete' signal,
	 * providing the #ofoAccount selected object.
	 *
	 * Argument is the current #ofoAccount object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountTreeview *view,
	 * 						ofoAccount       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-accdelete",
				OFA_TYPE_ACCOUNT_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofa_account_treeview_new:
 */
ofaAccountTreeview *
ofa_account_treeview_new( void )
{
	ofaAccountTreeview *view;

	view = g_object_new( OFA_TYPE_ACCOUNT_TREEVIEW,
					"ofa-tvbin-hpolicy", GTK_POLICY_NEVER,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoBat object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	setup_columns( view );

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaAccountTreeview *self )
{
	static const gchar *thisfn = "ofa_account_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_NUMBER,        _( "Number" ),   _( "Account number" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), ACCOUNT_COL_LABEL,         _( "Label" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_CURRENCY,      _( "Currency" ),     NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_NOTES,         _( "Notes" ),        NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), ACCOUNT_COL_NOTES_PNG,        "",           _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_UPD_USER,      _( "User" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), ACCOUNT_COL_UPD_STAMP,         NULL,        _( "Last update timestamp" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_VAL_DEBIT,     _( "Debit" ),    _( "Validated debit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_VAL_CREDIT,    _( "Credit" ),   _( "Validated credit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_ROUGH_DEBIT,   _( "Debit" ),    _( "Rough debit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_ROUGH_CREDIT,  _( "Credit" ),   _( "Rough credit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_FUT_DEBIT,     _( "Debit" ),    _( "Future debit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_FUT_CREDIT,    _( "Credit" ),   _( "Future credit" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_SETTLEABLE,    _( "S" ),        _( "Settleable" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_RECONCILIABLE, _( "R" ),        _( "Reconciliable" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_FORWARDABLE,   _( "F" ),        _( "Forwardable" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCOUNT_COL_CLOSED,        _( "C" ),        _( "Closed" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_EXE_DEBIT,     _( "Debit" ),    _( "Exercice debit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_EXE_CREDIT,    _( "Credit" ),   _( "Exercice credit" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_EXE_CREDIT,    _( "Solde" ),    _( "Exercice solde" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), ACCOUNT_COL_LABEL );
}

/**
 * ofa_account_treeview_set_settings_key:
 * @view: this #ofaAccountTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_account_treeview_set_settings_key( ofaAccountTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_account_treeview_set_settings_key";
	ofaAccountTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ));

	priv = ofa_account_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_settings_key( OFA_TVBIN( view ), key );
}

/**
 * ofa_account_treeview_set_hub:
 * @view: this #ofaAccountTreeview instance.
 * @hub: the current #ofaHub object.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_account_treeview_set_hub( ofaAccountTreeview *view, ofaHub *hub )
{
	ofaAccountTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_account_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_account_store_new( hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );
}

static void
on_selection_changed( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-accchanged" );
}

static void
on_selection_activated( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-accactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-accdelete" );
}

/*
 * BAT may be %NULL when selection is empty (on 'ofa-batchanged' signal)
 */
static void
get_and_send( ofaAccountTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoAccount *account;

	account = get_selected_with_selection( self, selection );
	g_return_if_fail( !account || OFO_IS_ACCOUNT( account ));

	g_signal_emit_by_name( self, signal, account );
}

/**
 * ofa_account_treeview_get_selected:
 * @view: this #ofaAccountTreeview instance.
 *
 * Return: the currently selected BAT file, or %NULL.
 */
ofoAccount *
ofa_account_treeview_get_selected( ofaAccountTreeview *view )
{
	static const gchar *thisfn = "ofa_account_treeview_get_selected";
	ofaAccountTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoAccount *account;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ), NULL );

	priv = ofa_account_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	account = get_selected_with_selection( view, selection );

	return( account );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaAccountTreeview instance.
 * @selection:
 *
 * Return: the currently selected BAT file, or %NULL.
 */
static ofoAccount *
get_selected_with_selection( ofaAccountTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoAccount *account;

	account = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_OBJECT, &account, -1 );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );
		g_object_unref( account );
	}

	return( account );
}

/**
 * ofa_account_treeview_set_selected:
 * @view: this #ofaAccountTreeview instance.
 * @account_id: the account identifier to be selected.
 *
 * Selects the account identified by @account_id.
 * Does not do anything if the @account_id is not visible in this @view.
 */
void
ofa_account_treeview_set_selected( ofaAccountTreeview *view, const gchar *account_id )
{
#if 0
	static const gchar *thisfn = "ofa_account_treeview_set_selected";
	ofaAccountTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sid;
	ofxCounter row_id;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	g_debug( "%s: view=%p, id=%ld", thisfn, ( void * ) view, id );

	g_return_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ));

	priv = ofa_account_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_treeview( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, ACCOUNT_COL_NUMBER, &sid, -1 );
				row_id = atol( sid );
				g_free( sid );
				if( row_id == id ){
					//ofa_tvbin_set_selected( OFA_TVBIN( view ), tmodel, &iter );
					selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( treeview ));
					gtk_tree_selection_select_iter( selection, &iter );
					/* move the cursor so that it is visible */
					path = gtk_tree_model_get_path( tmodel, &iter );
					gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( treeview ), path, NULL, FALSE, 0, 0 );
					gtk_tree_view_set_cursor( GTK_TREE_VIEW( treeview ), path, NULL, FALSE );
					gtk_tree_path_free( path );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
#endif
}
