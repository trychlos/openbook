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
#include "api/ofo-base.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-reconcil-store.h"
#include "ui/ofa-reconcil-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime
	 */
	GtkTreeModelFilterVisibleFunc filter_fn;
	void                         *filter_data;
}
	ofaReconcilTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void      setup_columns( ofaReconcilTreeview *self );
static void      on_selection_changed( ofaReconcilTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_activated( ofaReconcilTreeview *self, GtkTreeSelection *selection, void *empty );
static void      get_and_send( ofaReconcilTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static GList    *get_selected_with_selection( ofaReconcilTreeview *self, GtkTreeSelection *selection );
static gboolean  tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint      tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaReconcilTreeview, ofa_reconcil_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaReconcilTreeview ))

static void
reconcil_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECONCIL_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_treeview_parent_class )->finalize( instance );
}

static void
reconcil_treeview_dispose( GObject *instance )
{
	ofaReconcilTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECONCIL_TREEVIEW( instance ));

	priv = ofa_reconcil_treeview_get_instance_private( OFA_RECONCIL_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_treeview_parent_class )->dispose( instance );
}

static void
ofa_reconcil_treeview_init( ofaReconcilTreeview *self )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_init";
	ofaReconcilTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_RECONCIL_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_reconcil_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->filter_fn = NULL;
	priv->filter_data = NULL;
}

static void
ofa_reconcil_treeview_class_init( ofaReconcilTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaReconcilTreeview::ofa-entchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaReconcilTreeview proxyes it with this 'ofa-entchanged' signal,
	 * signal, providing the selected objects.
	 *
	 * Argument is the list of selected objects; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaReconcilTreeview *view,
	 * 						GList             *list,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-entchanged",
				OFA_TYPE_RECONCIL_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaReconcilTreeview::ofa-entactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaReconcilTreeview proxyes it with this 'ofa-entactivated' signal,
	 * providing the selected objects.
	 *
	 * Argument is the list of selected objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaReconcilTreeview *view,
	 * 						GList             *list,
	 * 						gpointer           user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-entactivated",
				OFA_TYPE_RECONCIL_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

/**
 * ofa_reconcil_treeview_new:
 *
 * Returns: a new instance.
 */
ofaReconcilTreeview *
ofa_reconcil_treeview_new( void )
{
	ofaReconcilTreeview *view;

	view = g_object_new( OFA_TYPE_RECONCIL_TREEVIEW,
				"ofa-tvbin-selmode", GTK_SELECTION_MULTIPLE,
				"ofa-tvbin-shadow", GTK_SHADOW_IN,
				NULL );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * the selected objects instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	return( view );
}

/**
 * ofa_reconcil_treeview_set_settings_key:
 * @view: this #ofaReconcilTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_reconcil_treeview_set_settings_key( ofaReconcilTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_set_settings_key";
	ofaReconcilTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_RECONCIL_TREEVIEW( view ));

	priv = ofa_reconcil_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_reconcil_treeview_setup_columns:
 * @view: this #ofaReconcilTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_reconcil_treeview_setup_columns( ofaReconcilTreeview *view )
{
	ofaReconcilTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECONCIL_TREEVIEW( view ));

	priv = ofa_reconcil_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaReconcilTreeview *self )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), RECONCIL_COL_DOPE,          _( "Ope." ),        _( "Operation date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), RECONCIL_COL_DEFFECT,       _( "Effect" ),      _( "Effect date" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), RECONCIL_COL_LABEL,         _( "Label" ),           NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_REF,           _( "Ref." ),        _( "Piece reference" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_CURRENCY,      _( "Currency" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_LEDGER,        _( "Ledger" ),          NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_OPE_TEMPLATE,  _( "Template" ),    _( "Operation template" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_ACCOUNT,       _( "Account" ),         NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), RECONCIL_COL_DEBIT,         _( "Debit" ),           NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), RECONCIL_COL_CREDIT,        _( "Credit" ),          NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), RECONCIL_COL_OPE_NUMBER,    _( "Ope." ),        _( "Operation number" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), RECONCIL_COL_STLMT_NUMBER,  _( "Set.num" ),     _( "Settlement number" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_STLMT_USER,    _( "Set.user" ),    _( "Settlement user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), RECONCIL_COL_STLMT_STAMP,   _( "Set.stamp" ),   _( "Settlement timestamp" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), RECONCIL_COL_ENT_NUMBER,    _( "Ent.num" ),     _( "Reconcil number" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_UPD_USER,      _( "Ent.user" ),    _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), RECONCIL_COL_UPD_STAMP,     _( "Ent.stamp" ),   _( "Last update timestamp" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), RECONCIL_COL_STATUS,        _( "Status" ),          NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), RECONCIL_COL_CONCIL_NUMBER, _( "Concil.num" ),  _( "Conciliation number" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), RECONCIL_COL_CONCIL_DATE,   _( "Concil.date" ), _( "Conciliation date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), RECONCIL_COL_CONCIL_TYPE,   _( "Concil.type" ), _( "Conciliation type" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), RECONCIL_COL_LABEL );
}

/**
 * ofa_reconcil_treeview_set_filter:
 * @view: this #ofaReconcilTreeview instance.
 * @filter_fn: an external filter function.
 * @filter_data: the data to be passed to the @filter_fn function.
 *
 * Setup the filtering function.
 */
void
ofa_reconcil_treeview_set_filter_func( ofaReconcilTreeview *view, GtkTreeModelFilterVisibleFunc filter_fn, void *filter_data )
{
	ofaReconcilTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_RECONCIL_TREEVIEW( view ));

	priv = ofa_reconcil_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->filter_fn = filter_fn;
	priv->filter_data = filter_data;
}

static void
on_selection_changed( ofaReconcilTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entchanged" );
}

static void
on_selection_activated( ofaReconcilTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entactivated" );
}

/*
 * Reconcil may be %NULL when selection is empty (on 'ofa-entchanged' signal)
 */
static void
get_and_send( ofaReconcilTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	GList *list;

	list = get_selected_with_selection( self, selection );
	g_signal_emit_by_name( self, signal, list );
	ofa_reconcil_treeview_free_selected( list );
}

/**
 * ofa_reconcil_treeview_get_selected:
 * @view: this #ofaReconcilTreeview instance.
 *
 * Returns: the list of selected objects, which may be %NULL.
 *
 * The returned list should be ofa_recurrent_model_treeview_free_selected()
 * by the caller.
 */
GList *
ofa_reconcil_treeview_get_selected( ofaReconcilTreeview *view )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_get_selected";
	ofaReconcilTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_RECONCIL_TREEVIEW( view ), NULL );

	priv = ofa_reconcil_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

/*
 * get_selected_with_selection:
 * @view: this #ofaReconcilTreeview instance.
 * @selection: the current #GtkTreeSelection.
 *
 * Return: the list of selected objects, or %NULL.
 */
static GList *
get_selected_with_selection( ofaReconcilTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *selected_rows, *irow, *selected_objects;
	ofoBase *object;

	selected_objects = NULL;
	selected_rows = gtk_tree_selection_get_selected_rows( selection, &tmodel );
	for( irow=selected_rows ; irow ; irow=irow->next ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) irow->data )){
			gtk_tree_model_get( tmodel, &iter, RECONCIL_COL_OBJECT, &object, -1 );
			g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
			selected_objects = g_list_prepend( selected_objects, object );
		}
	}

	g_list_free_full( selected_rows, ( GDestroyNotify ) gtk_tree_path_free );

	return( selected_objects );
}

/**
 * ofa_reconcil_treeview_set_selected:
 * @view: this #ofaReconcilTreeview instance.
 * @entry: the number of the entry to be selected.
 *
 * Selects the entry identified by @entry.
 */
void
ofa_reconcil_treeview_set_selected( ofaReconcilTreeview *view, ofxCounter entry )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_set_selected";
	ofaReconcilTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofxCounter row_number;

	g_debug( "%s: view=%p, entry=%lu", thisfn, ( void * ) view, entry );

	g_return_if_fail( view && OFA_IS_RECONCIL_TREEVIEW( view ));

	priv = ofa_reconcil_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_treeview( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, RECONCIL_COL_ENT_NUMBER_I, &row_number, -1 );
				if( row_number == entry ){
					ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}

#if 0
/**
 * ofa_reconcil_treeview_cell_data_render:
 * @view: this #ofaReconcilTreeview instance.
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
ofa_reconcil_treeview_cell_data_render( ofaReconcilTreeview *view,
				GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaReconcilTreeviewPrivate *priv;
	ofaReconcilStatus status;
	GdkRGBA color;
	gint err_level;
	const gchar *color_str;

	g_return_if_fail( view && OFA_IS_RECONCIL_TREEVIEW( view ));
	g_return_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ));
	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER_TEXT( renderer ));
	g_return_if_fail( model && GTK_IS_TREE_MODEL( model ));

	priv = ofa_reconcil_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	err_level = get_row_errlevel( view, model, iter );
	gtk_tree_model_get( model, iter, RECONCIL_COL_STATUS_I, &status, -1 );

	g_object_set( G_OBJECT( renderer ),
						"style-set",      FALSE,
						"background-set", FALSE,
						"foreground-set", FALSE,
						NULL );

	switch( status ){

		case ENT_STATUS_PAST:
			gdk_rgba_parse( &color, RGBA_PAST );
			g_object_set( G_OBJECT( renderer ), "background-rgba", &color, NULL );
			break;

		case ENT_STATUS_VALIDATED:
			gdk_rgba_parse( &color, RGBA_VALIDATED );
			g_object_set( G_OBJECT( renderer ), "background-rgba", &color, NULL );
			break;

		case ENT_STATUS_DELETED:
			gdk_rgba_parse( &color, RGBA_DELETED );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
			break;

		case ENT_STATUS_ROUGH:
			switch( err_level ){
				case RECONCIL_ERR_ERROR:
					color_str = RGBA_ERROR;
					break;
				case RECONCIL_ERR_WARNING:
					color_str = RGBA_WARNING;
					break;
				default:
					color_str = RGBA_NORMAL;
					break;
			}
			gdk_rgba_parse( &color, color_str );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			break;

		case ENT_STATUS_FUTURE:
			gdk_rgba_parse( &color, RGBA_FUTURE );
			g_object_set( G_OBJECT( renderer ), "background-rgba", &color, NULL );
			break;

		default:
			break;
	}
}
#endif

static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaReconcilTreeviewPrivate *priv;

	priv = ofa_reconcil_treeview_get_instance_private( OFA_RECONCIL_TREEVIEW( tvbin ));

	return( priv->filter_fn ? ( *priv->filter_fn )( tmodel, iter, priv->filter_data ) : TRUE );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_reconcil_treeview_v_sort";
	gint cmp;
	gchar *dopea, *deffa, *labela, *refa, *cura, *ledgera, *templatea, *accounta, *debita, *credita, *openuma, *stlmtnuma, *stlmtusera, *stlmtstampa, *entnuma, *updusera, *updstampa, *concilnuma, *concildatea, *statusa, *typea;
	gchar *dopeb, *deffb, *labelb, *refb, *curb, *ledgerb, *templateb, *accountb, *debitb, *creditb, *openumb, *stlmtnumb, *stlmtuserb, *stlmtstampb, *entnumb, *upduserb, *updstampb, *concilnumb, *concildateb, *statusb, *typeb;

	gtk_tree_model_get( tmodel, a,
			RECONCIL_COL_DOPE,          &dopea,
			RECONCIL_COL_DEFFECT,       &deffa,
			RECONCIL_COL_LABEL,         &labela,
			RECONCIL_COL_REF,           &refa,
			RECONCIL_COL_CURRENCY,      &cura,
			RECONCIL_COL_LEDGER,        &ledgera,
			RECONCIL_COL_OPE_TEMPLATE,  &templatea,
			RECONCIL_COL_ACCOUNT,       &accounta,
			RECONCIL_COL_DEBIT,         &debita,
			RECONCIL_COL_CREDIT,        &credita,
			RECONCIL_COL_OPE_NUMBER,    &openuma,
			RECONCIL_COL_STLMT_NUMBER,  &stlmtnuma,
			RECONCIL_COL_STLMT_USER,    &stlmtusera,
			RECONCIL_COL_STLMT_STAMP,   &stlmtstampa,
			RECONCIL_COL_ENT_NUMBER,    &entnuma,
			RECONCIL_COL_UPD_USER,      &updusera,
			RECONCIL_COL_UPD_STAMP,     &updstampa,
			RECONCIL_COL_STATUS,        &statusa,
			RECONCIL_COL_CONCIL_NUMBER, &concilnuma,
			RECONCIL_COL_CONCIL_DATE,   &concildatea,
			RECONCIL_COL_CONCIL_TYPE,   &typea,
			-1 );

	gtk_tree_model_get( tmodel, b,
			RECONCIL_COL_DOPE,          &dopeb,
			RECONCIL_COL_DEFFECT,       &deffb,
			RECONCIL_COL_LABEL,         &labelb,
			RECONCIL_COL_REF,           &refb,
			RECONCIL_COL_CURRENCY,      &curb,
			RECONCIL_COL_LEDGER,        &ledgerb,
			RECONCIL_COL_OPE_TEMPLATE,  &templateb,
			RECONCIL_COL_ACCOUNT,       &accountb,
			RECONCIL_COL_DEBIT,         &debitb,
			RECONCIL_COL_CREDIT,        &creditb,
			RECONCIL_COL_OPE_NUMBER,    &openumb,
			RECONCIL_COL_STLMT_NUMBER,  &stlmtnumb,
			RECONCIL_COL_STLMT_USER,    &stlmtuserb,
			RECONCIL_COL_STLMT_STAMP,   &stlmtstampb,
			RECONCIL_COL_ENT_NUMBER,    &entnumb,
			RECONCIL_COL_UPD_USER,      &upduserb,
			RECONCIL_COL_UPD_STAMP,     &updstampb,
			RECONCIL_COL_STATUS,        &statusb,
			RECONCIL_COL_CONCIL_NUMBER, &concilnumb,
			RECONCIL_COL_CONCIL_DATE,   &concildateb,
			RECONCIL_COL_CONCIL_TYPE,   &typeb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case RECONCIL_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_display());
			break;
		case RECONCIL_COL_DEFFECT:
			cmp = my_date_compare_by_str( deffa, deffb, ofa_prefs_date_display());
			break;
		case RECONCIL_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case RECONCIL_COL_REF:
			cmp = my_collate( refa, refb );
			break;
		case RECONCIL_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case RECONCIL_COL_LEDGER:
			cmp = my_collate( ledgera, ledgerb );
			break;
		case RECONCIL_COL_OPE_TEMPLATE:
			cmp = my_collate( templatea, templateb );
			break;
		case RECONCIL_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case RECONCIL_COL_DEBIT:
			cmp = ofa_itvsortable_sort_str_amount( debita, debitb );
			break;
		case RECONCIL_COL_CREDIT:
			cmp = ofa_itvsortable_sort_str_amount( credita, creditb );
			break;
		case RECONCIL_COL_OPE_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( openuma, openumb );
			break;
		case RECONCIL_COL_STLMT_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( stlmtnuma, stlmtnumb );
			break;
		case RECONCIL_COL_STLMT_USER:
			cmp = my_collate( stlmtusera, stlmtuserb );
			break;
		case RECONCIL_COL_STLMT_STAMP:
			cmp = my_collate( stlmtstampa, stlmtstampb );
			break;
		case RECONCIL_COL_ENT_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( entnuma, entnumb );
			break;
		case RECONCIL_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case RECONCIL_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		case RECONCIL_COL_STATUS:
			cmp = ofa_itvsortable_sort_str_int( statusa, statusb );
			break;
		case RECONCIL_COL_CONCIL_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( concilnuma, concilnumb );
			break;
		case RECONCIL_COL_CONCIL_DATE:
			cmp = my_date_compare_by_str( concildatea, concildateb, ofa_prefs_date_display());
			break;
		case RECONCIL_COL_CONCIL_TYPE:
			cmp = my_collate( typea, typeb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( dopea );
	g_free( deffa );
	g_free( labela );
	g_free( refa );
	g_free( cura );
	g_free( ledgera );
	g_free( templatea );
	g_free( accounta );
	g_free( debita );
	g_free( credita );
	g_free( openuma );
	g_free( stlmtnuma );
	g_free( stlmtusera );
	g_free( stlmtstampa );
	g_free( entnuma );
	g_free( updusera );
	g_free( updstampa );
	g_free( statusa );
	g_free( concilnuma );
	g_free( concildatea );
	g_free( typea );

	g_free( dopeb );
	g_free( deffb );
	g_free( labelb );
	g_free( refb );
	g_free( curb );
	g_free( ledgerb );
	g_free( templateb );
	g_free( accountb );
	g_free( debitb );
	g_free( creditb );
	g_free( openumb );
	g_free( stlmtnumb );
	g_free( stlmtuserb );
	g_free( stlmtstampb );
	g_free( entnumb );
	g_free( upduserb );
	g_free( updstampb );
	g_free( statusb );
	g_free( concilnumb );
	g_free( concildateb );
	g_free( typeb );

	return( cmp );
}
