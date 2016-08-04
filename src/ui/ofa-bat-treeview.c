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
#include "api/ofa-isortable.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-store.h"
#include "ui/ofa-bat-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* UI
	 */
	ofaBatStore       *store;
}
	ofaBatTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     init_columns( ofaBatTreeview *self );
static void     on_selection_changed( ofaBatTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_activated( ofaBatTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_delete( ofaBatTreeview *self, GtkTreeSelection *selection, void *empty );
static void     get_and_send( ofaBatTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoBat  *get_selected_with_selection( ofaBatTreeview *self, GtkTreeSelection *selection );
static gint     v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaBatTreeview, ofa_bat_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaBatTreeview ))

static void
bat_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_treeview_parent_class )->finalize( instance );
}

static void
bat_treeview_dispose( GObject *instance )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_TREEVIEW( instance ));

	priv = ofa_bat_treeview_get_instance_private( OFA_BAT_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_treeview_parent_class )->dispose( instance );
}

static void
ofa_bat_treeview_init( ofaBatTreeview *self )
{
	static const gchar *thisfn = "ofa_bat_treeview_init";
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_BAT_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_bat_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoBat object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( self, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( self, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( self, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	ofa_tvbin_set_selection_mode( OFA_TVBIN( self ), GTK_SELECTION_BROWSE );

	init_columns( self );
}

/*
 * Defines the treeview columns
 */
static void
init_columns( ofaBatTreeview *self )
{
	static const gchar *thisfn = "ofa_bat_treeview_init_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), BAT_COL_ID,          _( "Id." ),      _( "BAT Id." ));
	ofa_tvbin_add_column_text_lx( OFA_TVBIN( self ), BAT_COL_URI,         _( "URI" ),      _( "URI" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAT_COL_FORMAT,      _( "Format" ),   _( "Format" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), BAT_COL_BEGIN,       _( "Begin" ),    _( "Begin date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), BAT_COL_END,         _( "End" ),      _( "End date" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), BAT_COL_COUNT,       _( "Count" ),    _( "Lines count" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), BAT_COL_UNUSED,      _( "Unused" ),   _( "Unused lines" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAT_COL_RIB,         _( "RIB" ),      _( "RIB" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), BAT_COL_BEGIN_SOLDE, _( "Begin" ),    _( "Begin solde" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), BAT_COL_END_SOLDE,   _( "End" ),      _( "End solde" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAT_COL_CURRENCY,    _( "Currency" ), _( "Currency" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAT_COL_ACCOUNT,     _( "Account" ),  _( "Account" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAT_COL_NOTES,       _( "Notes" ),    _( "Notes" ));
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), BAT_COL_NOTES_PNG,      "",           _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), BAT_COL_UPD_USER,    _( "User" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), BAT_COL_UPD_STAMP,       NULL,        _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), BAT_COL_URI );
}

static void
ofa_bat_treeview_class_init( ofaBatTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_bat_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = v_sort;

	/**
	 * ofaBatTreeview::ofa-batchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaBatTreeview proxyes it with this 'ofa-batchanged' signal,
	 * providing the #ofoBat selected object.
	 *
	 * Argument is the current #ofoBat object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatTreeview *view,
	 * 						ofoBat       *object,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-batchanged",
				OFA_TYPE_BAT_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaBatTreeview::ofa-batactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaBatTreeview proxyes it with this 'ofa-batactivated' signal,
	 * providing the #ofoBat selected object.
	 *
	 * Argument is the current #ofoBat object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatTreeview *view,
	 * 						ofoBat       *object,
	 * 						gpointer      user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-batactivated",
				OFA_TYPE_BAT_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaBatTreeview::ofa-batdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaBatTreeview proxyes it with this 'ofa-batdelete' signal,
	 * providing the #ofoBat selected object.
	 *
	 * Argument is the current #ofoBat object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaBatTreeview *view,
	 * 						ofoBat       *object,
	 * 						gpointer      user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-batdelete",
				OFA_TYPE_BAT_TREEVIEW,
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
 * ofa_bat_treeview_new:
 */
ofaBatTreeview *
ofa_bat_treeview_new( void )
{
	ofaBatTreeview *view;

	view = g_object_new( OFA_TYPE_BAT_TREEVIEW, NULL );

	return( view );
}

/**
 * ofa_bat_treeview_set_settings_key:
 * @view: this #ofaBatTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_bat_treeview_set_settings_key( ofaBatTreeview *view, const gchar *key )
{
	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_settings_key( OFA_TVBIN( view ), key );
}

/**
 * ofa_bat_treeview_set_hub:
 * @view: this #ofaBatTreeview instance.
 * @hub: the current #ofaHub object.
 *
 * Setup the current hub.
 * Initialize the underlying store.
 */
void
ofa_bat_treeview_set_hub( ofaBatTreeview *view, ofaHub *hub )
{
	ofaBatTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_bat_store_new( hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), OFA_ISTORE( priv->store ));
	g_object_unref( priv->store );

	ofa_isortable_set_default_sort( OFA_ISORTABLE( view ), BAT_COL_ID, GTK_SORT_DESCENDING );
}

static void
on_selection_changed( ofaBatTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-batchanged" );
}

static void
on_selection_activated( ofaBatTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-batactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaBatTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-batdelete" );
}

/*
 * BAT may be %NULL when selection is empty (on 'ofa-batchanged' signal)
 */
static void
get_and_send( ofaBatTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoBat *bat;

	bat = get_selected_with_selection( self, selection );
	g_return_if_fail( !bat || OFO_IS_BAT( bat ));

	g_signal_emit_by_name( self, signal, bat );
}

/**
 * ofa_bat_treeview_get_selected:
 * @view: this #ofaBatTreeview instance.
 *
 * Return: the currently selected BAT file, or %NULL.
 */
ofoBat *
ofa_bat_treeview_get_selected( ofaBatTreeview *view )
{
	static const gchar *thisfn = "ofa_bat_treeview_get_selected";
	ofaBatTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoBat *bat;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_BAT_TREEVIEW( view ), NULL );

	priv = ofa_bat_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	bat = get_selected_with_selection( view, selection );

	return( bat );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaBatTreeview instance.
 * @selection:
 *
 * Return: the identifier of the currently selected BAT file, or 0.
 */
static ofoBat *
get_selected_with_selection( ofaBatTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	bat = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, BAT_COL_OBJECT, &bat, -1 );
		g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
		g_object_unref( bat );
	}

	return( bat );
}

/**
 * ofa_bat_treeview_set_selected:
 * @view: this #ofaBatTreeview instance.
 * @id: the BAT identifier to be selected.
 *
 * Selects the BAT file identified by @id.
 */
void
ofa_bat_treeview_set_selected( ofaBatTreeview *view, ofxCounter id )
{
	static const gchar *thisfn = "ofa_bat_treeview_set_selected";
	ofaBatTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sid;
	ofxCounter row_id;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	g_debug( "%s: view=%p, id=%ld", thisfn, ( void * ) view, id );

	g_return_if_fail( view && OFA_IS_BAT_TREEVIEW( view ));

	priv = ofa_bat_treeview_get_instance_private( view );
	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_treeview( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, BAT_COL_ID, &sid, -1 );
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
}

static gint
v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_bat_treeview_v_sort";
	gint cmp;
	gchar *ida, *uria, *formata, *begina, *enda, *riba, *cura, *bsoldea, *esoldea, *notesa, *counta, *unuseda, *accounta, *updusera, *updstampa;
	gchar *idb, *urib, *formatb, *beginb, *endb, *ribb, *curb, *bsoldeb, *esoldeb, *notesb, *countb, *unusedb, *accountb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			BAT_COL_ID,             &ida,
			BAT_COL_URI,            &uria,
			BAT_COL_FORMAT,         &formata,
			BAT_COL_BEGIN,          &begina,
			BAT_COL_END,            &enda,
			BAT_COL_RIB,            &riba,
			BAT_COL_CURRENCY,       &cura,
			BAT_COL_BEGIN_SOLDE,    &bsoldea,
			BAT_COL_END_SOLDE,      &esoldea,
			BAT_COL_NOTES,          &notesa,
			BAT_COL_NOTES_PNG,      &pnga,
			BAT_COL_COUNT,          &counta,
			BAT_COL_UNUSED,         &unuseda,
			BAT_COL_ACCOUNT,        &accounta,
			BAT_COL_UPD_USER,       &updusera,
			BAT_COL_UPD_STAMP,      &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			BAT_COL_ID,             &idb,
			BAT_COL_URI,            &urib,
			BAT_COL_FORMAT,         &formatb,
			BAT_COL_BEGIN,          &beginb,
			BAT_COL_END,            &endb,
			BAT_COL_RIB,            &ribb,
			BAT_COL_CURRENCY,       &curb,
			BAT_COL_BEGIN_SOLDE,    &bsoldeb,
			BAT_COL_END_SOLDE,      &esoldeb,
			BAT_COL_NOTES,          &notesb,
			BAT_COL_NOTES_PNG,      &pngb,
			BAT_COL_COUNT,          &countb,
			BAT_COL_UNUSED,         &unusedb,
			BAT_COL_ACCOUNT,        &accountb,
			BAT_COL_UPD_USER,       &upduserb,
			BAT_COL_UPD_STAMP,      &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case BAT_COL_ID:
			cmp = ofa_isortable_sort_str_int( ida, idb );
			break;
		case BAT_COL_URI:
			cmp = my_collate( uria, urib );
			break;
		case BAT_COL_FORMAT:
			cmp = my_collate( formata, formatb );
			break;
		case BAT_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_display());
			break;
		case BAT_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_display());
			break;
		case BAT_COL_RIB:
			cmp = my_collate( riba, ribb );
			break;
		case BAT_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case BAT_COL_BEGIN_SOLDE:
			cmp = ofa_isortable_sort_str_amount( bsoldea, bsoldeb );
			break;
		case BAT_COL_END_SOLDE:
			cmp = ofa_isortable_sort_str_amount( esoldea, esoldeb );
			break;
		case BAT_COL_NOTES:
			cmp = my_collate( begina, beginb );
			break;
		case BAT_COL_NOTES_PNG:
			cmp = ofa_isortable_sort_png( pnga, pngb );
			break;
		case BAT_COL_COUNT:
			cmp = ofa_isortable_sort_str_int( counta, countb );
			break;
		case BAT_COL_UNUSED:
			cmp = ofa_isortable_sort_str_int( unuseda, unusedb );
			break;
		case BAT_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case BAT_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case BAT_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( ida );
	g_free( uria );
	g_free( formata );
	g_free( begina );
	g_free( enda );
	g_free( riba );
	g_free( cura );
	g_free( bsoldea );
	g_free( esoldea );
	g_free( notesa );
	g_free( counta );
	g_free( unuseda );
	g_free( accounta );
	g_free( updusera );
	g_free( updstampa );
	g_object_unref( pnga );

	g_free( idb );
	g_free( urib );
	g_free( formatb );
	g_free( beginb );
	g_free( endb );
	g_free( ribb );
	g_free( curb );
	g_free( bsoldeb );
	g_free( esoldeb );
	g_free( notesb );
	g_free( countb );
	g_free( unusedb );
	g_free( accountb );
	g_free( upduserb );
	g_free( updstampb );
	g_object_unref( pngb );

	return( cmp );
}
