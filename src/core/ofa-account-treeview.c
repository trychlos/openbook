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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-currency.h"

#include "core/ofa-account-store.h"
#include "core/ofa-account-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	gint        class_num;

	/* runtime
	 */
	GList      *signaler_handlers;
}
	ofaAccountTreeviewPrivate;

/* class properties
 */
enum {
	PROP_CLASSNUM_ID = 1,
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        setup_key_pressed_event( ofaAccountTreeview *self );
static void        setup_columns( ofaAccountTreeview *self );
static void        on_selection_changed( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_activated( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_delete( ofaAccountTreeview *self, GtkTreeSelection *selection, void *empty );
static void        get_and_send( ofaAccountTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoAccount *get_selected_with_selection( ofaAccountTreeview *self, GtkTreeSelection *selection );
static gint        find_account_iter( ofaAccountTreeview *self, const gchar *account_id, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void        cell_data_render_text( GtkCellRendererText *renderer, gboolean is_root, gint level, gboolean is_error );
static gboolean    tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountTreeview *self );
static void        tview_collapse_node( ofaAccountTreeview *self, GtkWidget *widget );
static void        tview_expand_node( ofaAccountTreeview *self, GtkWidget *widget );
static void        signaler_connect_to_signaling_system( ofaAccountTreeview *self );
static void        signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaAccountTreeview *self );
static gboolean    tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter );

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
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_TREEVIEW( instance ));

	priv = ofa_account_treeview_get_instance_private( OFA_ACCOUNT_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_treeview_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
account_treeview_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaAccountTreeviewPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_TREEVIEW( instance ));

	priv = ofa_account_treeview_get_instance_private( OFA_ACCOUNT_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_CLASSNUM_ID:
				g_value_set_int( value, priv->class_num );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * the user asks to set a property and provides it into a GValue
 * read the content of the provided GValue and set our instance datas
 */
static void
account_treeview_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaAccountTreeviewPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_TREEVIEW( instance ));

	priv = ofa_account_treeview_get_instance_private( OFA_ACCOUNT_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_CLASSNUM_ID:
				priv->class_num = g_value_get_int( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
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
	priv->class_num = -1;
}

static void
ofa_account_treeview_class_init( ofaAccountTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_account_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = account_treeview_get_property;
	G_OBJECT_CLASS( klass )->set_property = account_treeview_set_property;
	G_OBJECT_CLASS( klass )->dispose = account_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_CLASSNUM_ID,
			g_param_spec_int(
					"ofa-account-treeview-class-number",
					"Class number",
					"Filtered class number",
					-1, 9, GTK_POLICY_AUTOMATIC,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

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
 * @getter: a #ofaIGetter instance.
 * @class_number: the filtered class number.
 *  It must be set at instanciation time as it is also used as a
 *  qualifier for the actions group name.
 *
 * Returns: a new instance.
 */
ofaAccountTreeview *
ofa_account_treeview_new( ofaIGetter *getter, gint class_number )
{
	ofaAccountTreeview *view;
	ofaAccountTreeviewPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_ACCOUNT_TREEVIEW,
				"ofa-tvbin-getter",  getter,
				NULL );

	priv = ofa_account_treeview_get_instance_private( view );

	priv->getter = getter;
	priv->class_num = class_number;

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoAccount object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	setup_key_pressed_event( view );

	/* because the ofaAccountTreeview is built to live inside of a
	 * GtkNotebook, not each view will save its settings, but only
	 * the last being saw by the user (see ofaAccountFrameBin::dispose)
	 */
	ofa_tvbin_set_write_settings( OFA_TVBIN( view ), FALSE );

	/* connect to ISignaler */
	signaler_connect_to_signaling_system( view );

	return( view );
}

/*
 * Have to intercept the key-pressed event from the treeview
 * in order to manage the hierarchy.
 */
static void
setup_key_pressed_event( ofaAccountTreeview *self )
{
	GtkWidget *treeview;

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( self ));
	g_signal_connect( treeview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );
}

/**
 * ofa_account_treeview_get_class:
 * @view: this #ofaAccountTreeview instance.
 *
 * Returns: the class number associated to this @view.
 */
gint
ofa_account_treeview_get_class( ofaAccountTreeview *view )
{
	ofaAccountTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ), -1 );

	priv = ofa_account_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, -1 );

	return( priv->class_num );
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
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_account_treeview_setup_columns:
 * @view: this #ofaAccountTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_account_treeview_setup_columns( ofaAccountTreeview *view )
{
	ofaAccountTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ));

	priv = ofa_account_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
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
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), ACCOUNT_COL_NOTES,         _( "Notes" ),        NULL );
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
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCOUNT_COL_EXE_SOLDE,     _( "Solde" ),    _( "Exercice solde" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), ACCOUNT_COL_LABEL );

	ofa_itvcolumnable_twins_group_new(
			OFA_ITVCOLUMNABLE( self ), "val", ACCOUNT_COL_VAL_DEBIT, ACCOUNT_COL_VAL_CREDIT, -1 );
	ofa_itvcolumnable_twins_group_new(
			OFA_ITVCOLUMNABLE( self ), "rough", ACCOUNT_COL_ROUGH_DEBIT, ACCOUNT_COL_ROUGH_CREDIT, -1 );
	ofa_itvcolumnable_twins_group_new(
			OFA_ITVCOLUMNABLE( self ), "fut", ACCOUNT_COL_FUT_DEBIT, ACCOUNT_COL_FUT_CREDIT, -1 );
	ofa_itvcolumnable_twins_group_new(
			OFA_ITVCOLUMNABLE( self ), "exe", ACCOUNT_COL_EXE_DEBIT, ACCOUNT_COL_EXE_CREDIT, ACCOUNT_COL_EXE_SOLDE, -1 );
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
 * Account may be %NULL when selection is empty (on 'ofa-accchanged' signal)
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
 * Return: the currently selected #ofoAccount, or %NULL.
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
 * @selection: the current #GtkTreeSelection.
 *
 * Return: the currently selected #ofoAccount, or %NULL.
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
 * Selects the account identified by @account_id, or the closest row if
 * the identifier is not visible in this @view.
 */
void
ofa_account_treeview_set_selected( ofaAccountTreeview *view, const gchar *account_id )
{
	static const gchar *thisfn = "ofa_account_treeview_set_selected";
	ofaAccountTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: view=%p, account_id=%s", thisfn, ( void * ) view, account_id );

	g_return_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ));

	priv = ofa_account_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			find_account_iter( view, account_id, tmodel, &iter );
			ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
		}
	}
}

/*
 * On enter, @iter is expected to point to the first row.
 *
 * Returns: %TRUE if found.
 */
static gint
find_account_iter( ofaAccountTreeview *self, const gchar *account_id, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreeIter child_iter;
	gchar *row_id;
	gint cmp;

	while( TRUE ){

		/* first examine children of the current iter */
		if( gtk_tree_model_iter_has_child( tmodel, iter )){
			gtk_tree_model_iter_children( tmodel, &child_iter, iter );
			cmp = find_account_iter( self, account_id, tmodel, &child_iter );
			if( cmp >= 0 ){
				*iter = child_iter;
				return( cmp );
			}
		}

		/* examine the current iter */
		gtk_tree_model_get( tmodel, iter, ACCOUNT_COL_NUMBER, &row_id, -1 );
		cmp = my_collate( row_id, account_id );
		//g_debug( "find_account_iter: account_id=%s, row_id=%s, cmp=%d", account_id, row_id, cmp );
		g_free( row_id );

		/* found
		 * or not found but have found a greater id, so stop there */
		if( cmp >= 0 ){
			return( cmp );
		}

		/* continue on next iter of same level */
		if( !gtk_tree_model_iter_next( tmodel, iter )){
			break;
		}
	}

	/* not found, and worth to continue */
	return( -1 );
}

/**
 * ofa_account_treeview_cell_data_render:
 * @view: this #ofaAccountTreeview instance.
 * @column: the #GtkTreeViewColumn treeview colum.
 * @renderer: a #GtkCellRenderer attached to the column.
 * @model: the #GtkTreeModel of the treeview.
 * @iter: the #GtkTreeIter which addresses the row.
 *
 * Paints the row.
 *
 * level 1: not displayed (should not appear)
 * level 2 and root: bold, colored background
 * level 3 and root: colored background
 * other root: italic
 *
 * Detail accounts who have no currency are red written.
 */
void
ofa_account_treeview_cell_data_render( ofaAccountTreeview *view,
				GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaAccountTreeviewPrivate *priv;
	gchar *account_num;
	ofoAccount *account_obj;
	GString *number;
	gint level;
	gint column_id;
	ofoCurrency *currency;
	gboolean is_root, is_error;

	g_return_if_fail( view && OFA_IS_ACCOUNT_TREEVIEW( view ));
	g_return_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ));
	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER( renderer ));
	g_return_if_fail( model && GTK_IS_TREE_MODEL( model ));

	priv = ofa_account_treeview_get_instance_private( view );

	gtk_tree_model_get( model, iter,
			ACCOUNT_COL_NUMBER, &account_num, ACCOUNT_COL_OBJECT, &account_obj, -1 );
	g_return_if_fail( account_obj && OFO_IS_ACCOUNT( account_obj ));
	g_object_unref( account_obj );

	level = ofo_account_get_level_from_number( ofo_account_get_number( account_obj ));
	g_return_if_fail( level >= 2 );

	is_root = ofo_account_is_root( account_obj );

	is_error = FALSE;
	if( !is_root ){
		currency = ofo_currency_get_by_code( priv->getter, ofo_account_get_currency( account_obj ));
		is_error |= !currency;
	}

	column_id = ofa_itvcolumnable_get_column_id( OFA_ITVCOLUMNABLE( view ), column );
	if( column_id == ACCOUNT_COL_NUMBER ){
		number = g_string_new( " " );
		g_string_append_printf( number, "%s", account_num );
		g_object_set( G_OBJECT( renderer ), "text", number->str, NULL );
		g_string_free( number, TRUE );
	}

	if( GTK_IS_CELL_RENDERER_TEXT( renderer )){
		cell_data_render_text( GTK_CELL_RENDERER_TEXT( renderer ), is_root, level, is_error );

	} else if( GTK_IS_CELL_RENDERER_PIXBUF( renderer )){

		/*if( ofo_account_is_root( account_obj ) && level == 2 ){
			path = gtk_tree_model_get_path( tmodel, iter );
			tview = gtk_tree_view_column_get_tree_view( tcolumn );
			gtk_tree_view_get_cell_area( tview, path, tcolumn, &rc );
			gtk_tree_path_free( path );
		}*/
	}
}

static void
cell_data_render_text( GtkCellRendererText *renderer, gboolean is_root, gint level, gboolean is_error )
{
	GdkRGBA color;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( renderer ));

	g_object_set( G_OBJECT( renderer ),
						"style-set",      FALSE,
						"weight-set",     FALSE,
						"background-set", FALSE,
						"foreground-set", FALSE,
						NULL );

	if( is_root ){
		if( level == 2 ){
			gdk_rgba_parse( &color, "#c0ffff" );
			g_object_set( G_OBJECT( renderer ), "background-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "weight", PANGO_WEIGHT_BOLD, NULL );

		} else if( level == 3 ){
			gdk_rgba_parse( &color, "#0000ff" );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "weight", PANGO_WEIGHT_BOLD, NULL );

		} else {
			gdk_rgba_parse( &color, "#0000ff" );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
		}

	} else if( is_error ){
		gdk_rgba_parse( &color, "#800000" );
		g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaAccountTreeview *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			tview_collapse_node( self, widget );

		} else if( event->keyval == GDK_KEY_Right ){
			tview_expand_node( self, widget );
		}
	}

	return( stop );
}

static void
tview_collapse_node( ofaAccountTreeview *self, GtkWidget *widget )
{
	GtkTreeSelection *selection;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, parent;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );

			} else if( gtk_tree_model_iter_parent( tmodel, &parent, &iter )){
				path = gtk_tree_model_get_path( tmodel, &parent );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
tview_expand_node( ofaAccountTreeview *self, GtkWidget *widget )
{
	GtkTreeSelection *selection;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
			if( gtk_tree_model_iter_has_child( tmodel, &iter )){
				path = gtk_tree_model_get_path( tmodel, &iter );
				gtk_tree_view_expand_row( GTK_TREE_VIEW( widget ), path, FALSE );
				gtk_tree_path_free( path );
			}
		}
	}
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaAccountTreeview *self )
{
	ofaAccountTreeviewPrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_account_treeview_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaAccountTreeview *self )
{
	static const gchar *thisfn = "ofa_account_treeview_signaler_on_new_base";
	ofaAccountTreeviewPrivate *priv;
	gint class_num;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	priv = ofa_account_treeview_get_instance_private( self );

	if( OFO_IS_ACCOUNT( object )){
		class_num = ofo_account_get_class( OFO_ACCOUNT( object ));
		if( class_num == priv->class_num ){
			ofa_account_treeview_set_selected( self, ofo_account_get_number( OFO_ACCOUNT( object )));
		}
	}
}

/*
 * We are here filtering the child model of the GtkTreeModelFilter,
 * which happens to be the sort model, itself being built on top of
 * the ofaTreeStore
 */
static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaAccountTreeviewPrivate *priv;
	gchar *number;
	gint class_num;

	priv = ofa_account_treeview_get_instance_private( OFA_ACCOUNT_TREEVIEW( tvbin ));

	gtk_tree_model_get( model, iter, ACCOUNT_COL_NUMBER, &number, -1 );
	class_num = ofo_account_get_class_from_number( number );
	g_free( number );

	return( priv->class_num == class_num );
}
