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
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"

#include "ofa-tva-form-store.h"
#include "ofa-tva-form-treeview.h"
#include "ofo-tva-form.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization
	 */
	ofaIGetter      *getter;
	gchar           *settings_prefix;

	/* UI
	 */
	ofaTVAFormStore *store;
}
	ofaTVAFormTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        setup_columns( ofaTVAFormTreeview *self );
static void        on_cell_data_fn( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaTVAFormTreeview *self );
static void        on_selection_changed( ofaTVAFormTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_activated( ofaTVAFormTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_delete( ofaTVAFormTreeview *self, GtkTreeSelection *selection, void *empty );
static void        get_and_send( ofaTVAFormTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoTVAForm *get_selected_with_selection( ofaTVAFormTreeview *self, GtkTreeSelection *selection );
static gint        tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaTVAFormTreeview, ofa_tva_form_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaTVAFormTreeview ))

static void
tva_form_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_form_treeview_finalize";
	ofaTVAFormTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_FORM_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_tva_form_treeview_get_instance_private( OFA_TVA_FORM_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_treeview_parent_class )->finalize( instance );
}

static void
tva_form_treeview_dispose( GObject *instance )
{
	ofaTVAFormTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_FORM_TREEVIEW( instance ));

	priv = ofa_tva_form_treeview_get_instance_private( OFA_TVA_FORM_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_treeview_parent_class )->dispose( instance );
}

static void
ofa_tva_form_treeview_init( ofaTVAFormTreeview *self )
{
	static const gchar *thisfn = "ofa_tva_form_treeview_init";
	ofaTVAFormTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_FORM_TREEVIEW( self ));

	priv = ofa_tva_form_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->store = NULL;
}

static void
ofa_tva_form_treeview_class_init( ofaTVAFormTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_tva_form_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaTVAFormTreeview::ofa-vatchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaTVAFormTreeview proxyes it with this 'ofa-vatchanged'
	 * signal, providing the selected object (which may be %NULL).
	 *
	 * Argument is the selected object; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVAFormTreeview *view,
	 * 						ofoTVAForm       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-vatchanged",
				OFA_TYPE_TVA_FORM_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVAFormTreeview::ofa-vatactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaTVAFormTreeview proxyes it with this 'ofa-vatactivated'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVAFormTreeview *view,
	 * 						ofoTVAForm       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-vatactivated",
				OFA_TYPE_TVA_FORM_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVAFormTreeview::ofa-vatdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaTVAFormTreeview proxyes it with this 'ofa-vatdelete'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVAFormTreeview *view,
	 * 						ofoTVAForm       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-vatdelete",
				OFA_TYPE_TVA_FORM_TREEVIEW,
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
 * ofa_tva_form_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the key prefix in user settings.
 *
 * Returns: a new #ofaTVAFormTreeview instance.
 */
ofaTVAFormTreeview *
ofa_tva_form_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaTVAFormTreeview *view;
	ofaTVAFormTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_TVA_FORM_TREEVIEW,
				"ofa-tvbin-getter", getter,
				"ofa-tvbin-shadow", GTK_SHADOW_IN,
				NULL );

	priv = ofa_tva_form_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	setup_columns( view );

	ofa_tvbin_set_cell_data_func( OFA_TVBIN( view ), ( GtkTreeCellDataFunc ) on_cell_data_fn, view );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoCurrency object instead of just the raw GtkTreeSelection
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
setup_columns( ofaTVAFormTreeview *self )
{
	static const gchar *thisfn = "ofa_tva_form_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_FORM_COL_MNEMO,              _( "Mnemo" ),     _( "Mnemonic" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), TVA_FORM_COL_LABEL,              _( "Label" ),         NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_FORM_COL_CRE_USER,           _( "Cre.user" ),  _( "Creation user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_FORM_COL_CRE_STAMP,          _( "Cre.stamp" ), _( "Creation timestamp" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_FORM_COL_ENABLED,            _( "Enabled" ),       NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_FORM_COL_HAS_CORRESPONDENCE, _( "Corresp." ),  _( "Has correspondence" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), TVA_FORM_COL_NOTES,              _( "Notes" ),         NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), TVA_FORM_COL_NOTES_PNG,             "",            _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_FORM_COL_UPD_USER,           _( "Upd.user" ),  _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_FORM_COL_UPD_STAMP,          _( "Upd.stamp" ), _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), TVA_FORM_COL_LABEL );
}

/*
 * gray+italic disabled items
 */
static void
on_cell_data_fn( GtkTreeViewColumn *column, GtkCellRenderer *renderer, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaTVAFormTreeview *self )
{
	ofoTVAForm *form;
	GdkRGBA color;

	if( GTK_IS_CELL_RENDERER_TEXT( renderer )){

		gtk_tree_model_get( tmodel, iter, TVA_FORM_COL_OBJECT, &form, -1 );
		g_return_if_fail( form && OFO_IS_TVA_FORM( form ));
		g_object_unref( form );

		g_object_set( G_OBJECT( renderer ),
				"style-set",      FALSE,
				"foreground-set", FALSE,
				NULL );

		if( !ofo_tva_form_get_is_enabled( form )){
			gdk_rgba_parse( &color, "#808080" );
			g_object_set( G_OBJECT( renderer ), "foreground-rgba", &color, NULL );
			g_object_set( G_OBJECT( renderer ), "style", PANGO_STYLE_ITALIC, NULL );
		}
	}
}

/**
 * ofa_tva_form_treeview_setup_store:
 * @view: this #ofaTVAFormTreeview instance.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_tva_form_treeview_setup_store( ofaTVAFormTreeview *view )
{
	ofaTVAFormTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_TVA_FORM_TREEVIEW( view ));

	priv = ofa_tva_form_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_tva_form_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), TVA_FORM_COL_MNEMO, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaTVAFormTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-vatchanged" );
}

static void
on_selection_activated( ofaTVAFormTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-vatactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaTVAFormTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-vatdelete" );
}

static void
get_and_send( ofaTVAFormTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoTVAForm *form;

	form = get_selected_with_selection( self, selection );
	g_signal_emit_by_name( self, signal, form );
}

/**
 * ofa_tva_form_treeview_get_selected:
 * @view: this #ofaTVAFormTreeview instance.
 *
 * Returns: the selected #ofoTVAForm object, which may be %NULL.
 *
 * The returned reference is owned by the underlying #ofaTVAFormStore,
 * and should not be released by the caller.
 */
ofoTVAForm *
ofa_tva_form_treeview_get_selected( ofaTVAFormTreeview *view )
{
	ofaTVAFormTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_return_val_if_fail( view && OFA_IS_TVA_FORM_TREEVIEW( view ), NULL );

	priv = ofa_tva_form_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

static ofoTVAForm *
get_selected_with_selection( ofaTVAFormTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	form = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, TVA_FORM_COL_OBJECT, &form, -1 );
		g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
		g_object_unref( form );
	}

	return( form );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_tva_form_treeview_v_sort";
	gint cmp;
	gchar *mnemoa, *labela, *creusera, *crestampa, *enaa, *hascorrespa, *notesa, *updusera, *updstampa;
	gchar *mnemob, *labelb, *creuserb, *crestampb, *enab, *hascorrespb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			TVA_FORM_COL_MNEMO,              &mnemoa,
			TVA_FORM_COL_LABEL,              &labela,
			TVA_FORM_COL_CRE_USER,           &creusera,
			TVA_FORM_COL_CRE_STAMP,          &crestampa,
			TVA_FORM_COL_ENABLED,            &enaa,
			TVA_FORM_COL_HAS_CORRESPONDENCE, &hascorrespa,
			TVA_FORM_COL_NOTES,              &notesa,
			TVA_FORM_COL_NOTES_PNG,          &pnga,
			TVA_FORM_COL_UPD_USER,           &updusera,
			TVA_FORM_COL_UPD_STAMP,          &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			TVA_FORM_COL_MNEMO,              &mnemob,
			TVA_FORM_COL_LABEL,              &labelb,
			TVA_FORM_COL_CRE_USER,           &creuserb,
			TVA_FORM_COL_CRE_STAMP,          &crestampb,
			TVA_FORM_COL_ENABLED,            &enab,
			TVA_FORM_COL_HAS_CORRESPONDENCE, &hascorrespb,
			TVA_FORM_COL_NOTES,              &notesb,
			TVA_FORM_COL_NOTES_PNG,          &pngb,
			TVA_FORM_COL_UPD_USER,           &upduserb,
			TVA_FORM_COL_UPD_STAMP,          &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case TVA_FORM_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case TVA_FORM_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case TVA_FORM_COL_CRE_USER:
			cmp = my_collate( creusera, creuserb );
			break;
		case TVA_FORM_COL_CRE_STAMP:
			cmp = my_collate( crestampa, crestampb );
			break;
		case TVA_FORM_COL_ENABLED:
			cmp = my_collate( enaa, enab );
			break;
		case TVA_FORM_COL_HAS_CORRESPONDENCE:
			cmp = my_collate( hascorrespa, hascorrespb );
			break;
		case TVA_FORM_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case TVA_FORM_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case TVA_FORM_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case TVA_FORM_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( labela );
	g_free( creusera );
	g_free( crestampa );
	g_free( enaa );
	g_free( hascorrespa );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( labelb );
	g_free( creuserb );
	g_free( crestampb );
	g_free( enab );
	g_free( hascorrespb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
