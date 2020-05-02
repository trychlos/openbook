/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofo-paimean.h"

#include "core/ofa-paimean-store.h"
#include "core/ofa-paimean-treeview.h"

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
	ofaPaimeanStore *store;
}
	ofaPaimeanTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void        setup_columns( ofaPaimeanTreeview *self );
static void        on_selection_changed( ofaPaimeanTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_activated( ofaPaimeanTreeview *self, GtkTreeSelection *selection, void *empty );
static void        on_selection_delete( ofaPaimeanTreeview *self, GtkTreeSelection *selection, void *empty );
static void        get_and_send( ofaPaimeanTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoPaimean *get_selected_with_selection( ofaPaimeanTreeview *self, GtkTreeSelection *selection );
static gint        tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaPaimeanTreeview, ofa_paimean_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaPaimeanTreeview ))

static void
paimean_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_paimean_treeview_finalize";
	ofaPaimeanTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAIMEAN_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_paimean_treeview_get_instance_private( OFA_PAIMEAN_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_treeview_parent_class )->finalize( instance );
}

static void
paimean_treeview_dispose( GObject *instance )
{
	ofaPaimeanTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAIMEAN_TREEVIEW( instance ));

	priv = ofa_paimean_treeview_get_instance_private( OFA_PAIMEAN_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_treeview_parent_class )->dispose( instance );
}

static void
ofa_paimean_treeview_init( ofaPaimeanTreeview *self )
{
	static const gchar *thisfn = "ofa_paimean_treeview_init";
	ofaPaimeanTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_PAIMEAN_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_paimean_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->store = NULL;
}

static void
ofa_paimean_treeview_class_init( ofaPaimeanTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_paimean_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaPaimeanTreeview::ofa-pamchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaPaimeanTreeview proxyes it with this 'ofa-pamchanged' signal,
	 * providing the #ofoPaimean selected object.
	 *
	 * Argument is the current #ofoPaimean object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPaimeanTreeview *view,
	 * 						ofoPaimean       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-pamchanged",
				OFA_TYPE_PAIMEAN_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaPaimeanTreeview::ofa-pamactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaPaimeanTreeview proxyes it with this 'ofa-pamactivated' signal,
	 * providing the #ofoPaimean selected object.
	 *
	 * Argument is the current #ofoPaimean object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPaimeanTreeview *view,
	 * 						ofoPaimean       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-pamactivated",
				OFA_TYPE_PAIMEAN_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaPaimeanTreeview::ofa-pamdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaPaimeanTreeview proxyes it with this 'ofa-pamdelete' signal,
	 * providing the #ofoPaimean selected object.
	 *
	 * Argument is the current #ofoPaimean object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPaimeanTreeview *view,
	 * 						ofoPaimean       *object,
	 * 						gpointer          user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-pamdelete",
				OFA_TYPE_PAIMEAN_TREEVIEW,
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
 * ofa_paimean_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: [allow-none]: the prefix of the key in user settings;
 *  if %NULL, then rely on this class name;
 *  when set, then this class automatically adds its name as a suffix.
 *
 * Returns: a new instance.
 */
ofaPaimeanTreeview *
ofa_paimean_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaPaimeanTreeview *view;
	ofaPaimeanTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_PAIMEAN_TREEVIEW,
					"ofa-tvbin-getter", getter,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	priv = ofa_paimean_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoPaimean object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	/* initialize TVBin */
	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );
	setup_columns( view );

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaPaimeanTreeview *self )
{
	static const gchar *thisfn = "ofa_paimean_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), PAM_COL_CODE,       _( "Code" ),          NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), PAM_COL_CRE_USER,   _( "Cre.user" ),  _( "Creation user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), PAM_COL_CRE_STAMP,  _( "Cre.stamp" ), _( "Creation timestamp" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), PAM_COL_LABEL,      _( "Label" ),         NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), PAM_COL_ACCOUNT,    _( "Account" ),       NULL );
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), PAM_COL_NOTES,      _( "Notes" ),         NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), PAM_COL_NOTES_PNG,     "",            _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), PAM_COL_UPD_USER,   _( "Upd.user" ),  _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), PAM_COL_UPD_STAMP,  _( "Upd.stamp" ), _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), PAM_COL_LABEL );
}

/**
 * ofa_paimean_treeview_setup_store:
 * @view: this #ofaPaimeanTreeview instance.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_paimean_treeview_setup_store( ofaPaimeanTreeview *view )
{
	ofaPaimeanTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_PAIMEAN_TREEVIEW( view ));

	priv = ofa_paimean_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_paimean_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), PAM_COL_CODE, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaPaimeanTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-pamchanged" );
}

static void
on_selection_activated( ofaPaimeanTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-pamactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaPaimeanTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-pamdelete" );
}

/*
 * Paimean may be %NULL when selection is empty (on 'ofa-pamchanged' signal)
 */
static void
get_and_send( ofaPaimeanTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoPaimean *paimean;

	paimean = get_selected_with_selection( self, selection );
	g_return_if_fail( !paimean || OFO_IS_PAIMEAN( paimean ));

	g_signal_emit_by_name( self, signal, paimean );
}

/**
 * ofa_paimean_treeview_get_selected:
 * @view: this #ofaPaimeanTreeview instance.
 *
 * Return: the currently selected paimean, or %NULL.
 */
ofoPaimean *
ofa_paimean_treeview_get_selected( ofaPaimeanTreeview *view )
{
	static const gchar *thisfn = "ofa_paimean_treeview_get_selected";
	ofaPaimeanTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoPaimean *paimean;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_PAIMEAN_TREEVIEW( view ), NULL );

	priv = ofa_paimean_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	paimean = get_selected_with_selection( view, selection );

	return( paimean );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaPaimeanTreeview instance.
 * @selection:
 *
 * Return: the currently selected paimean, or %NULL.
 */
static ofoPaimean *
get_selected_with_selection( ofaPaimeanTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
ofoPaimean *paimean;

	paimean = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, PAM_COL_OBJECT, &paimean, -1 );
		g_return_val_if_fail( paimean && OFO_IS_PAIMEAN( paimean ), NULL );
		g_object_unref( paimean );
	}

	return( paimean );
}

/**
 * ofa_paimean_treeview_set_selected:
 * @view: this #ofaPaimeanTreeview instance.
 * @code: [allow-none]: the identifier to be selected.
 *
 * Select @code identifier's row.
 */
void
ofa_paimean_treeview_set_selected( ofaPaimeanTreeview *view, const gchar *code )
{
	static const gchar *thisfn = "ofa_paimean_treeview_set_selected";
	ofaPaimeanTreeviewPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *row_code;
	gint cmp;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_if_fail( view && OFA_IS_PAIMEAN_TREEVIEW( view ));

	priv = ofa_paimean_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( view ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, PAM_COL_CODE, &row_code, -1 );
			cmp = my_collate( row_code, code );
			g_free( row_code );
			if( cmp == 0 ){
				ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_paimean_treeview_v_sort";
	gint cmp;
	gchar *codea, *labela, *accounta, *notesa, *creusera, *crestampa, *updusera, *updstampa;
	gchar *codeb, *labelb, *accountb, *notesb, *creuserb, *crestampb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			PAM_COL_CODE,       &codea,
			PAM_COL_CRE_USER,   &creusera,
			PAM_COL_CRE_STAMP,  &crestampa,
			PAM_COL_LABEL,      &labela,
			PAM_COL_ACCOUNT,    &accounta,
			PAM_COL_NOTES,      &notesa,
			PAM_COL_NOTES_PNG,  &pnga,
			PAM_COL_UPD_USER,   &updusera,
			PAM_COL_UPD_STAMP,  &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			PAM_COL_CODE,       &codeb,
			PAM_COL_CRE_USER,   &creuserb,
			PAM_COL_CRE_STAMP,  &crestampb,
			PAM_COL_LABEL,      &labelb,
			PAM_COL_ACCOUNT,    &accountb,
			PAM_COL_NOTES,      &notesb,
			PAM_COL_NOTES_PNG,  &pngb,
			PAM_COL_UPD_USER,   &upduserb,
			PAM_COL_UPD_STAMP,  &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case PAM_COL_CODE:
			cmp = my_collate( codea, codeb );
			break;
		case PAM_COL_CRE_USER:
			cmp = my_collate( creusera, creuserb );
			break;
		case PAM_COL_CRE_STAMP:
			cmp = my_collate( crestampa, crestampb );
			break;
		case PAM_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case PAM_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case PAM_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case PAM_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case PAM_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case PAM_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( codea );
	g_free( labela );
	g_free( accounta );
	g_free( notesa );
	g_free( creusera );
	g_free( crestampa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( codeb );
	g_free( labelb );
	g_free( accountb );
	g_free( notesb );
	g_free( creuserb );
	g_free( crestampb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
