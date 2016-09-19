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
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-entry-listview.h"
#include "core/ofa-entry-store.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* initialization
	 */
}
	ofaEntryListviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

/* when editing an entry, we may have two levels of errors:
 * fatal error: the entry is not valid and cannot be saved
 *              (e.g. a mandatory data is empty)
 * warning: the entry may be valid, but will not be applied in standard
 *          conditions (e.g. effect date is before the exercice)
 */
#define RGBA_NORMAL                     "#000000"		/* black */
#define RGBA_ERROR                      "#ff0000"		/* full red */
#define RGBA_WARNING                    "#ff8000"		/* orange */

/* status colors */
#define RGBA_PAST                       "#d8ffa0"		/* green background */
#define RGBA_VALIDATED                  "#ffe880"		/* pale gold background */
#define RGBA_DELETED                    "#808080"		/* gray foreground */
#define RGBA_FUTURE                     "#ffe8a8"		/* pale orange background */

static void      setup_columns( ofaEntryListview *self );
static void      on_selection_changed( ofaEntryListview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_activated( ofaEntryListview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_delete( ofaEntryListview *self, GtkTreeSelection *selection, void *empty );
static void      get_and_send( ofaEntryListview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoEntry *get_selected_with_selection( ofaEntryListview *self, GtkTreeSelection *selection );
static gint      get_row_errlevel( ofaEntryListview *self, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean  v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter );
static gint      v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaEntryListview, ofa_entry_listview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaEntryListview ))

static void
entry_listview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_listview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ENTRY_LISTVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_listview_parent_class )->finalize( instance );
}

static void
entry_listview_dispose( GObject *instance )
{
	ofaEntryListviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ENTRY_LISTVIEW( instance ));

	priv = ofa_entry_listview_get_instance_private( OFA_ENTRY_LISTVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_listview_parent_class )->dispose( instance );
}

static void
ofa_entry_listview_init( ofaEntryListview *self )
{
	static const gchar *thisfn = "ofa_entry_listview_init";
	ofaEntryListviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_ENTRY_LISTVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_entry_listview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_entry_listview_class_init( ofaEntryListviewClass *klass )
{
	static const gchar *thisfn = "ofa_entry_listview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_listview_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_listview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = v_filter;
	OFA_TVBIN_CLASS( klass )->sort = v_sort;

	/**
	 * ofaEntryListview::ofa-entchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaEntryListview proxyes it with this 'ofa-entchanged' signal,
	 * providing the #ofoEntry selected object.
	 *
	 * Argument is the current #ofoEntry object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryListview *view,
	 * 						ofoEntry       *object,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-entchanged",
				OFA_TYPE_ENTRY_LISTVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaEntryListview::ofa-entactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaEntryListview proxyes it with this 'ofa-entactivated' signal,
	 * providing the #ofoEntry selected object.
	 *
	 * Argument is the current #ofoEntry object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryListview *view,
	 * 						ofoEntry       *object,
	 * 						gpointer        user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-entactivated",
				OFA_TYPE_ENTRY_LISTVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaEntryListview::ofa-entdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaEntryListview proxyes it with this 'ofa-entdelete' signal,
	 * providing the #ofoEntry selected object.
	 *
	 * Argument is the current #ofoEntry object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaEntryListview *view,
	 * 						ofoEntry       *object,
	 * 						gpointer        user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-entdelete",
				OFA_TYPE_ENTRY_LISTVIEW,
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
 * ofa_entry_listview_new:
 *
 * Returns: a new instance.
 */
ofaEntryListview *
ofa_entry_listview_new( void )
{
	ofaEntryListview *view;

	view = g_object_new( OFA_TYPE_ENTRY_LISTVIEW,
				NULL );

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

	setup_columns( view );

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaEntryListview *self )
{
	static const gchar *thisfn = "ofa_entry_listview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ENTRY_COL_DOPE,          _( "Ope." ),        _( "Operation date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ENTRY_COL_DEFFECT,       _( "Effect" ),      _( "Effect date" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), ENTRY_COL_LABEL,         _( "Label" ),           NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_REF,           _( "Ref." ),        _( "Piece reference" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_CURRENCY,      _( "Currency" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_LEDGER,        _( "Ledger" ),          NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_OPE_TEMPLATE,  _( "Template" ),    _( "Operation template" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_ACCOUNT,       _( "Account" ),         NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ENTRY_COL_DEBIT,         _( "Debit" ),           NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ENTRY_COL_CREDIT,        _( "Credit" ),          NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_OPE_NUMBER,    _( "Ope." ),        _( "Operation number" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_STLMT_NUMBER,  _( "Set.num" ),     _( "Settlement number" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_STLMT_USER,    _( "Set.user" ),    _( "Settlement user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), ENTRY_COL_STLMT_STAMP,   _( "Set.stamp" ),   _( "Settlement timestamp" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_ENT_NUMBER,    _( "Ent.num" ),     _( "Entry number" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_UPD_USER,      _( "Ent.user" ),    _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), ENTRY_COL_UPD_STAMP,     _( "Ent.stamp" ),   _( "Last update timestamp" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ENTRY_COL_CONCIL_NUMBER, _( "Concil.num" ),  _( "Rough debit" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ENTRY_COL_CONCIL_DATE,   _( "Concil.date" ), _( "Rough credit" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ENTRY_COL_STATUS,        _( "Status" ),      _( "Future debit" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), ENTRY_COL_LABEL );
}

/**
 * ofa_entry_listview_set_settings_key:
 * @view: this #ofaEntryListview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_entry_listview_set_settings_key( ofaEntryListview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_entry_listview_set_settings_key";
	ofaEntryListviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_ENTRY_LISTVIEW( view ));

	priv = ofa_entry_listview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

static void
on_selection_changed( ofaEntryListview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entchanged" );
}

static void
on_selection_activated( ofaEntryListview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaEntryListview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-entdelete" );
}

/*
 * Entry may be %NULL when selection is empty (on 'ofa-entchanged' signal)
 */
static void
get_and_send( ofaEntryListview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoEntry *entry;

	entry = get_selected_with_selection( self, selection );
	g_return_if_fail( !entry || OFO_IS_ENTRY( entry ));

	g_signal_emit_by_name( self, signal, entry );
}

/**
 * ofa_entry_listview_get_selected:
 * @view: this #ofaEntryListview instance.
 *
 * Return: the currently selected #ofoEntry, or %NULL.
 */
ofoEntry *
ofa_entry_listview_get_selected( ofaEntryListview *view )
{
	static const gchar *thisfn = "ofa_entry_listview_get_selected";
	ofaEntryListviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoEntry *entry;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_ENTRY_LISTVIEW( view ), NULL );

	priv = ofa_entry_listview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	entry = get_selected_with_selection( view, selection );

	return( entry );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaEntryListview instance.
 * @selection: the current #GtkTreeSelection.
 *
 * Return: the currently selected #ofoAccount, or %NULL.
 */
static ofoEntry *
get_selected_with_selection( ofaEntryListview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoEntry *entry;

	entry = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, ENTRY_COL_OBJECT, &entry, -1 );
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
		g_object_unref( entry );
	}

	return( entry );
}

/**
 * ofa_entry_listview_set_selected:
 * @view: this #ofaEntryListview instance.
 * @entry: the number of the entry to be selected.
 *
 * Selects the entry identified by @entry.
 */
void
ofa_entry_listview_set_selected( ofaEntryListview *view, ofxCounter entry )
{
	static const gchar *thisfn = "ofa_entry_listview_set_selected";
	ofaEntryListviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofxCounter row_number;

	g_debug( "%s: view=%p, entry=%lu", thisfn, ( void * ) view, entry );

	g_return_if_fail( view && OFA_IS_ENTRY_LISTVIEW( view ));

	priv = ofa_entry_listview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_treeview( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, ENTRY_COL_ENT_NUMBER_I, &row_number, -1 );
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

/**
 * ofa_entry_listview_cell_data_render:
 * @view: this #ofaEntryListview instance.
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
ofa_entry_listview_cell_data_render( ofaEntryListview *view,
				GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaEntryListviewPrivate *priv;
	ofaEntryStatus status;
	GdkRGBA color;
	gint err_level;
	const gchar *color_str;

	g_return_if_fail( view && OFA_IS_ENTRY_LISTVIEW( view ));
	g_return_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ));
	g_return_if_fail( renderer && GTK_IS_CELL_RENDERER_TEXT( renderer ));
	g_return_if_fail( model && GTK_IS_TREE_MODEL( model ));

	priv = ofa_entry_listview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	err_level = get_row_errlevel( view, model, iter );
	gtk_tree_model_get( model, iter, ENTRY_COL_STATUS_I, &status, -1 );

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
				case ENTRY_ERR_ERROR:
					color_str = RGBA_ERROR;
					break;
				case ENTRY_ERR_WARNING:
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

static gint
get_row_errlevel( ofaEntryListview *self, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gchar *msgerr, *msgwarn;
	gint err_level;

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_MSGERR, &msgerr, ENTRY_COL_MSGWARN, &msgwarn, -1 );

	if( my_strlen( msgerr )){
		err_level = ENTRY_ERR_ERROR;

	} else if( my_strlen( msgwarn )){
		err_level = ENTRY_ERR_WARNING;

	} else {
		err_level = ENTRY_ERR_NONE;
	}

	g_free( msgerr );
	g_free( msgwarn );

	return( err_level );
}

/*
 * We are here filtering the child model of the GtkTreeModelFilter,
 * which happens to be the sort model, itself being built on top of
 * the ofaEntryStore
 */
static gboolean
v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter )
{
#if 0
	ofaEntryListviewPrivate *priv;
	gchar *number;
	gint class_num;

	priv = ofa_entry_listview_get_instance_private( OFA_ENTRY_LISTVIEW( tvbin ));

	gtk_tree_model_get( model, iter, ACCOUNT_COL_NUMBER, &number, -1 );
	class_num = ofo_account_get_class_from_number( number );
	g_free( number );

	return( priv->class_num == class_num );
#endif
	return( TRUE );
}

static gint
v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_entry_listview_v_sort";
	gint cmp;
	gchar *dopea, *deffa, *labela, *refa, *cura, *ledgera, *templatea, *accounta, *debita, *credita, *openuma, *stlmtnuma, *stlmtusera, *stlmtstampa, *entnuma, *updusera, *updstampa, *concilnuma, *concildatea, *statusa;
	gchar *dopeb, *deffb, *labelb, *refb, *curb, *ledgerb, *templateb, *accountb, *debitb, *creditb, *openumb, *stlmtnumb, *stlmtuserb, *stlmtstampb, *entnumb, *upduserb, *updstampb, *concilnumb, *concildateb, *statusb;

	gtk_tree_model_get( tmodel, a,
			ENTRY_COL_DOPE,          &dopea,
			ENTRY_COL_DEFFECT,       &deffa,
			ENTRY_COL_LABEL,         &labela,
			ENTRY_COL_REF,           &refa,
			ENTRY_COL_CURRENCY,      &cura,
			ENTRY_COL_LEDGER,        &ledgera,
			ENTRY_COL_OPE_TEMPLATE,  &templatea,
			ENTRY_COL_ACCOUNT,       &accounta,
			ENTRY_COL_DEBIT,         &debita,
			ENTRY_COL_CREDIT,        &credita,
			ENTRY_COL_OPE_NUMBER,    &openuma,
			ENTRY_COL_STLMT_NUMBER,  &stlmtnuma,
			ENTRY_COL_STLMT_USER,    &stlmtusera,
			ENTRY_COL_STLMT_STAMP,   &stlmtstampa,
			ENTRY_COL_ENT_NUMBER,    &entnuma,
			ENTRY_COL_UPD_USER,      &updusera,
			ENTRY_COL_UPD_STAMP,     &updstampa,
			ENTRY_COL_CONCIL_NUMBER, &concilnuma,
			ENTRY_COL_CONCIL_DATE,   &concildatea,
			ENTRY_COL_STATUS,        &statusa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ENTRY_COL_DOPE,          &dopeb,
			ENTRY_COL_DEFFECT,       &deffb,
			ENTRY_COL_LABEL,         &labelb,
			ENTRY_COL_REF,           &refb,
			ENTRY_COL_CURRENCY,      &curb,
			ENTRY_COL_LEDGER,        &ledgerb,
			ENTRY_COL_OPE_TEMPLATE,  &templateb,
			ENTRY_COL_ACCOUNT,       &accountb,
			ENTRY_COL_DEBIT,         &debitb,
			ENTRY_COL_CREDIT,        &creditb,
			ENTRY_COL_OPE_NUMBER,    &openumb,
			ENTRY_COL_STLMT_NUMBER,  &stlmtnumb,
			ENTRY_COL_STLMT_USER,    &stlmtuserb,
			ENTRY_COL_STLMT_STAMP,   &stlmtstampb,
			ENTRY_COL_ENT_NUMBER,    &entnumb,
			ENTRY_COL_UPD_USER,      &upduserb,
			ENTRY_COL_UPD_STAMP,     &updstampb,
			ENTRY_COL_CONCIL_NUMBER, &concilnumb,
			ENTRY_COL_CONCIL_DATE,   &concildateb,
			ENTRY_COL_STATUS,        &statusb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case ENTRY_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_display());
			break;
		case ENTRY_COL_DEFFECT:
			cmp = my_date_compare_by_str( deffa, deffb, ofa_prefs_date_display());
			break;
		case ENTRY_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case ENTRY_COL_REF:
			cmp = my_collate( refa, refb );
			break;
		case ENTRY_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case ENTRY_COL_LEDGER:
			cmp = my_collate( ledgera, ledgerb );
			break;
		case ENTRY_COL_OPE_TEMPLATE:
			cmp = my_collate( templatea, templateb );
			break;
		case ENTRY_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case ENTRY_COL_DEBIT:
			cmp = ofa_itvsortable_sort_str_amount( debita, debitb );
			break;
		case ENTRY_COL_CREDIT:
			cmp = ofa_itvsortable_sort_str_amount( credita, creditb );
			break;
		case ENTRY_COL_OPE_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( openuma, openumb );
			break;
		case ENTRY_COL_STLMT_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( stlmtnuma, stlmtnumb );
			break;
		case ENTRY_COL_STLMT_USER:
			cmp = my_collate( stlmtusera, stlmtuserb );
			break;
		case ENTRY_COL_STLMT_STAMP:
			cmp = my_collate( stlmtstampa, stlmtstampb );
			break;
		case ENTRY_COL_ENT_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( entnuma, entnumb );
			break;
		case ENTRY_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case ENTRY_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		case ENTRY_COL_CONCIL_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( concilnuma, concilnumb );
			break;
		case ENTRY_COL_CONCIL_DATE:
			cmp = my_date_compare_by_str( concildatea, concildateb, ofa_prefs_date_display());
			break;
		case ENTRY_COL_STATUS:
			cmp = ofa_itvsortable_sort_str_int( statusa, statusb );
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
	g_free( concilnuma );
	g_free( concildatea );
	g_free( statusa );

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
	g_free( concilnumb );
	g_free( concildateb );
	g_free( statusb );

	return( cmp );
}
