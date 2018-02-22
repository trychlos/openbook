/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-counter.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-batline-properties.h"
#include "core/ofa-batline-treeview.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-reconcil-group.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIGetter    *getter;
	gchar         *settings_prefix;

	/* runtime
	 */
	ofoCurrency   *currency;

	/* UI
	 */
	GtkListStore  *store;

	/* actions
	 */
	GSimpleAction *vconcil_action;			/* display reconciliation group */
	GSimpleAction *vbatline_action;			/* display bat line */

	/* current selection
	 */
	ofxCounter     concil_id;
	ofoBatLine    *batline;
}
	ofaBatlineTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        setup_columns( ofaBatlineTreeview *self );
static void        setup_actions( ofaBatlineTreeview *self );
static void        setup_store( ofaBatlineTreeview *view );
static void        store_batline( ofaBatlineTreeview *self, ofoBatLine *line );
static void        on_selection_changed( ofaBatlineTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_activated( ofaBatlineTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_delete( ofaBatlineTreeview *self, GtkTreeSelection *selection, void *empty );
static void        get_and_send( ofaBatlineTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoBatLine *get_selected_with_selection( ofaBatlineTreeview *self, GtkTreeSelection *selection );
static void        action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaBatlineTreeview *self );
static void        action_on_vbatline_activated( GSimpleAction *action, GVariant *empty, ofaBatlineTreeview *self );
static gint        tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaBatlineTreeview, ofa_batline_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaBatlineTreeview ))

static void
batline_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_batline_treeview_finalize";
	ofaBatlineTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BATLINE_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_batline_treeview_get_instance_private( OFA_BATLINE_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_batline_treeview_parent_class )->finalize( instance );
}

static void
batline_treeview_dispose( GObject *instance )
{
	ofaBatlineTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BATLINE_TREEVIEW( instance ));

	priv = ofa_batline_treeview_get_instance_private( OFA_BATLINE_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_object_unref( priv->vconcil_action );
		g_object_unref( priv->vbatline_action );
		g_object_unref( priv->store );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_batline_treeview_parent_class )->dispose( instance );
}

static void
ofa_batline_treeview_init( ofaBatlineTreeview *self )
{
	static const gchar *thisfn = "ofa_batline_treeview_init";
	ofaBatlineTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_BATLINE_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_batline_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_batline_treeview_class_init( ofaBatlineTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_batline_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = batline_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = batline_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaBatlineTreeview::ofa-balchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaBatlineTreeview proxyes it with this 'ofa-balchanged' signal,
	 * providing the #ofoBatLine selected object.
	 *
	 * Argument is the current #ofoBatLine object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatlineTreeview *view,
	 * 						ofoBatLine   *object,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-balchanged",
				OFA_TYPE_BATLINE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaBatlineTreeview::ofa-balactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaBatlineTreeview proxyes it with this 'ofa-balactivated' signal,
	 * providing the #ofoBatLine selected object.
	 *
	 * Argument is the current #ofoBatLine object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatlineTreeview *view,
	 * 						ofoBat       *object,
	 * 						gpointer      user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-balactivated",
				OFA_TYPE_BATLINE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaBatlineTreeview::ofa-baldelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaBatlineTreeview proxyes it with this 'ofa-baldelete' signal,
	 * providing the #ofoBatLine selected object.
	 *
	 * Argument is the current #ofoBatLine object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatlineTreeview *view,
	 * 						ofoBat       *object,
	 * 						gpointer      user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-baldelete",
				OFA_TYPE_BATLINE_TREEVIEW,
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
 * ofa_batline_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: [allow-none]: the prefix of the settings key.
 *
 * Returns: a new #ofoBatlineTreeview object.
 */
ofaBatlineTreeview *
ofa_batline_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaBatlineTreeview *view;
	ofaBatlineTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_BATLINE_TREEVIEW,
					"ofa-tvbin-getter", getter,
					NULL );

	priv = ofa_batline_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	setup_columns( view );
	setup_actions( view );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoBatLine object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaBatlineTreeview *self )
{
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), BAL_COL_BAT_ID,    _( "Bat Id." ),             NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), BAL_COL_LINE_ID,   _( "Line Id." ),            NULL );
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), BAL_COL_DEFFECT,   _( "Effect" ),          _( "Effect date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), BAL_COL_DOPE,      _( "Operation" ),       _( "Operation date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAL_COL_REF,       _( "Ref." ),            _( "Reference" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), BAL_COL_LABEL,     _( "Label" ),               NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAL_COL_CURRENCY,  _( "Currency" ),            NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), BAL_COL_AMOUNT,    _( "Amount" ),              NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAL_COL_CONCIL_ID, _( "Concil. Id." ),     _( "Conciliation Id." ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAL_COL_ENTRY,     _( "Concil. entries" ), _( "Conciliation entries" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAL_COL_USER,      _( "Concil. user" ),    _( "Conciliation user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), BAL_COL_STAMP,     _( "Concil. stamp" ),   _( "Conciliation timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), BAL_COL_LABEL );
	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( self ), BAL_COL_DEFFECT, GTK_SORT_DESCENDING );
}

static void
setup_actions( ofaBatlineTreeview *self )
{
	ofaBatlineTreeviewPrivate *priv;
	GMenu *menu;

	priv = ofa_batline_treeview_get_instance_private( self );

	/* display bat line action */
	priv->vbatline_action = g_simple_action_new( "vbatline", NULL );
	g_signal_connect( priv->vbatline_action, "activate", G_CALLBACK( action_on_vbatline_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vbatline_action ),
			_( "View the BAT line..." ));
	g_simple_action_set_enabled( priv->vbatline_action, FALSE );

	/* display conciliation group action */
	priv->vconcil_action = g_simple_action_new( "vconcil", NULL );
	g_signal_connect( priv->vconcil_action, "activate", G_CALLBACK( action_on_vconcil_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vconcil_action ),
			_( "Display conciliation group..." ));
	g_simple_action_set_enabled( priv->vconcil_action, FALSE );

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( self ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( self ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( self ), OFA_IACTIONABLE( self ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

/**
 * ofa_batline_treeview_set_bat:
 * @view: this #ofaBatlineTreeview instance.
 * @bat: the #ofoBat object to be displayed.
 *
 * Setup the store with the lines of the @bat.
 */
void
ofa_batline_treeview_set_bat( ofaBatlineTreeview *view, ofoBat *bat )
{
	ofaBatlineTreeviewPrivate *priv;
	GList *dataset, *it;
	const gchar *cur_code;

	g_return_if_fail( view && OFA_IS_BATLINE_TREEVIEW( view ));
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	priv = ofa_batline_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !priv->store ){
		setup_store( view );

	} else {
		gtk_list_store_clear( priv->store );
	}

	priv->currency = NULL;
	cur_code = ofo_bat_get_currency( bat );
	if( my_strlen( cur_code )){
		priv->currency = ofo_currency_get_by_code( priv->getter, cur_code );
	}

	dataset = ofo_bat_line_get_dataset( priv->getter, ofo_bat_get_id( bat ));
	for( it=dataset ; it ; it=it->next ){
		store_batline( view, OFO_BAT_LINE( it->data ));
	}

	ofo_bat_line_free_dataset( dataset );
}

/*
 * setup_store:
 * @view: this #ofaBatlineTreeview instance.
 *
 * Associates the treeview to the underlying (maybe empty) store, read
 * the settings and show the columns.
 *
 * This should be called only after the columns have already been
 * defined by the caller.
 *
 * If the store is not explicitely defined, then it will be when setting
 * the BAT data for the first time.
 */
static void
setup_store( ofaBatlineTreeview *view )
{
	ofaBatlineTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_BATLINE_TREEVIEW( view ));

	priv = ofa_batline_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->store = gtk_list_store_new(
			BAL_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* bat_id, line_id, dope */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* deffect, ref, label */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* currency, amount, concil_id */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* entry, upd_user, upd_stamp */
			G_TYPE_OBJECT );								/* the ofoBatLine object itself */

	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
}

static void
store_batline( ofaBatlineTreeview *self, ofoBatLine *line )
{
	ofaBatlineTreeviewPrivate *priv;
	gchar *sbatid, *slineid, *sdope, *sdeffect, *samount, *stamp,*sconcilid;
	const gchar *cuser, *cur_code;
	GtkTreeIter iter;
	ofoConcil *concil;
	GString *snumbers;
	GList *ids, *it;
	ofsConcilId *scid;

	priv = ofa_batline_treeview_get_instance_private( self );

	sbatid = g_strdup_printf( "%lu", ofo_bat_line_get_bat_id( line ));
	slineid = g_strdup_printf( "%lu", ofo_bat_line_get_line_id( line ));
	sdeffect = my_date_to_str( ofo_bat_line_get_deffect( line ), ofa_prefs_date_get_display_format( priv->getter ));
	sdope = my_date_to_str( ofo_bat_line_get_dope( line ), ofa_prefs_date_get_display_format( priv->getter ));

	if( !priv->currency ){
		cur_code = ofo_bat_line_get_currency( line );
		priv->currency = cur_code ? ofo_currency_get_by_code( priv->getter, cur_code ) : NULL;
		g_return_if_fail( !priv->currency || OFO_IS_CURRENCY( priv->currency ));
	}

	samount = ofa_amount_to_str( ofo_bat_line_get_amount( line ), priv->currency, priv->getter );

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( line ));
	snumbers = g_string_new( "" );
	if( concil ){
		sconcilid = g_strdup_printf( "%lu", ofo_concil_get_id( concil ));
		cuser = ofo_concil_get_upd_user( concil );
		stamp = my_stamp_to_str( ofo_concil_get_upd_stamp( concil ), MY_STAMP_YYMDHMS );
		ids = ofo_concil_get_ids( concil );
		for( it=ids ; it ; it=it->next ){
			scid = ( ofsConcilId * ) it->data;
			if( !my_collate( scid->type, CONCIL_TYPE_ENTRY )){
				if( snumbers->len ){
					snumbers = g_string_append( snumbers, "," );
				}
				g_string_append_printf( snumbers, "%ld", scid->other_id );
			}
		}
	} else {
		sconcilid = g_strdup( "" );
		cuser = "";
		stamp = g_strdup( "" );
	}

	gtk_list_store_insert_with_values( priv->store, &iter, -1,
			BAL_COL_BAT_ID,    sbatid,
			BAL_COL_LINE_ID,   slineid,
			BAL_COL_DEFFECT,   sdeffect,
			BAL_COL_DOPE,      sdope,
			BAL_COL_REF,       ofo_bat_line_get_ref( line ),
			BAL_COL_LABEL,     ofo_bat_line_get_label( line ),
			BAL_COL_CURRENCY,  priv->currency ? ofo_currency_get_code( priv->currency ) : "",
			BAL_COL_AMOUNT,    samount,
			BAL_COL_CONCIL_ID, sconcilid,
			BAL_COL_ENTRY,     snumbers->str,
			BAL_COL_USER,      cuser,
			BAL_COL_STAMP,     stamp,
			BAL_COL_OBJECT,    line,
			-1 );

	g_string_free( snumbers, TRUE );
	g_free( stamp );
	g_free( samount );
	g_free( sdeffect );
	g_free( sdope );
	g_free( slineid );
	g_free( sbatid );
}

static void
on_selection_changed( ofaBatlineTreeview *self, GtkTreeSelection *selection, void *empty )
{
	ofaBatlineTreeviewPrivate *priv;
	ofoConcil *concil;

	priv = ofa_batline_treeview_get_instance_private( self );

	priv->concil_id = 0;
	priv->batline = get_selected_with_selection( self, selection );
	g_return_if_fail( !priv->batline || OFO_IS_BAT_LINE( priv->batline ));

	if( priv->batline ){
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( priv->batline ));
		if( concil ){
			priv->concil_id = ofo_concil_get_id( concil );
		}
	}

	g_simple_action_set_enabled( priv->vbatline_action, priv->batline != NULL );
	g_simple_action_set_enabled( priv->vconcil_action, priv->concil_id > 0 );

	get_and_send( self, selection, "ofa-balchanged" );
}

static void
on_selection_activated( ofaBatlineTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-balactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaBatlineTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-baldelete" );
}

/*
 * BatLine may be %NULL when selection is empty (on 'ofa-balchanged' signal)
 */
static void
get_and_send( ofaBatlineTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoBatLine *batline;

	batline = get_selected_with_selection( self, selection );
	g_return_if_fail( !batline || OFO_IS_BAT_LINE( batline ));

	g_signal_emit_by_name( self, signal, batline );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaBatlineTreeview instance.
 * @selection:
 *
 * Return: the identifier of the currently selected BAT file, or 0.
 */
static ofoBatLine *
get_selected_with_selection( ofaBatlineTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBatLine *line;

	line = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, BAL_COL_OBJECT, &line, -1 );
		g_return_val_if_fail( line && OFO_IS_BAT_LINE( line ), NULL );
		g_object_unref( line );
	}

	return( line );
}

/*
 * display the bat line properties
 */
static void
action_on_vbatline_activated( GSimpleAction *action, GVariant *empty, ofaBatlineTreeview *self )
{
	ofaBatlineTreeviewPrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_batline_treeview_get_instance_private( self );

	toplevel = GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	ofa_batline_properties_run( priv->getter, toplevel, priv->batline );
}

/*
 * display the reconciliation group
 */
static void
action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaBatlineTreeview *self )
{
	ofaBatlineTreeviewPrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_batline_treeview_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_reconcil_group_run( priv->getter, toplevel, priv->concil_id );
}

static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_batline_treeview_v_sort";
	ofaBatlineTreeviewPrivate *priv;
	gint cmp;
	gchar *batida, *lineida, *deffecta, *dopea, *refa, *labela, *cura, *amounta, *entrya, *usera, *stampa, *sconcilida;
	gchar *batidb, *lineidb, *deffectb, *dopeb, *refb, *labelb, *curb, *amountb, *entryb, *userb, *stampb, *sconcilidb;

	priv = ofa_batline_treeview_get_instance_private( OFA_BATLINE_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			BAL_COL_BAT_ID,    &batida,
			BAL_COL_LINE_ID,   &lineida,
			BAL_COL_DEFFECT,   &deffecta,
			BAL_COL_DOPE,      &dopea,
			BAL_COL_REF,       &refa,
			BAL_COL_LABEL,     &labela,
			BAL_COL_CURRENCY,  &cura,
			BAL_COL_AMOUNT,    &amounta,
			BAL_COL_CONCIL_ID, &sconcilida,
			BAL_COL_ENTRY,     &entrya,
			BAL_COL_USER,      &usera,
			BAL_COL_STAMP,     &stampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			BAL_COL_BAT_ID,    &batidb,
			BAL_COL_LINE_ID,   &lineidb,
			BAL_COL_DEFFECT,   &deffectb,
			BAL_COL_DOPE,      &dopeb,
			BAL_COL_REF,       &refb,
			BAL_COL_LABEL,     &labelb,
			BAL_COL_CURRENCY,  &curb,
			BAL_COL_AMOUNT,    &amountb,
			BAL_COL_CONCIL_ID, &sconcilidb,
			BAL_COL_ENTRY,     &entryb,
			BAL_COL_USER,      &userb,
			BAL_COL_STAMP,     &stampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case BAL_COL_BAT_ID:
			cmp = ofa_itvsortable_sort_str_int( batida, batidb );
			break;
		case BAL_COL_LINE_ID:
			cmp = ofa_itvsortable_sort_str_int( lineida, lineidb );
			break;
		case BAL_COL_DEFFECT:
			cmp = my_date_compare_by_str( deffecta, deffectb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case BAL_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case BAL_COL_REF:
			cmp = my_collate( refa, refb );
			break;
		case BAL_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case BAL_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case BAL_COL_AMOUNT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), amounta, amountb );
			break;
		case BAL_COL_CONCIL_ID:
			cmp = ofa_itvsortable_sort_str_int( sconcilida, sconcilidb );
			break;
		case BAL_COL_ENTRY:
			cmp = my_collate( entrya, entryb );
			break;
		case BAL_COL_USER:
			cmp = my_collate( usera, userb );
			break;
		case BAL_COL_STAMP:
			cmp = my_collate( stampa, stampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( batida );
	g_free( lineida );
	g_free( deffecta );
	g_free( dopea );
	g_free( refa );
	g_free( labela );
	g_free( cura );
	g_free( amounta );
	g_free( sconcilida );
	g_free( entrya );
	g_free( usera );
	g_free( stampa );

	g_free( batidb );
	g_free( lineidb );
	g_free( deffectb );
	g_free( dopeb );
	g_free( refb );
	g_free( labelb );
	g_free( curb );
	g_free( amountb );
	g_free( sconcilidb );
	g_free( entryb );
	g_free( userb );
	g_free( stampb );

	return( cmp );
}
