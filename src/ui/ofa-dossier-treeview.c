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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization
	 */
	ofaHub          *hub;

	/* runtime
	 */
	gboolean         show_all;

	/* UI
	 */
	ofaDossierStore *store;
}
	ofaDossierTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     setup_columns( ofaDossierTreeview *view );
static void     on_selection_changed( ofaDossierTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_activated( ofaDossierTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_delete( ofaDossierTreeview *self, GtkTreeSelection *selection, void *empty );
static void     get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static gboolean get_selected_with_selection( ofaDossierTreeview *self, GtkTreeSelection *selection, ofaIDBDossierMeta **meta, ofaIDBExerciceMeta **period );
static gboolean tvbin_v_filter( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint     tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaDossierTreeview, ofa_dossier_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaDossierTreeview ))

static void
dossier_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_treeview_parent_class )->finalize( instance );
}

static void
dossier_treeview_dispose( GObject *instance )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_TREEVIEW( instance ));

	priv = ofa_dossier_treeview_get_instance_private( OFA_DOSSIER_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->store );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_treeview_parent_class )->dispose( instance );
}

static void
ofa_dossier_treeview_init( ofaDossierTreeview *self )
{
	static const gchar *thisfn = "ofa_dossier_treeview_init";
	ofaDossierTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_TREEVIEW( self ));

	priv = ofa_dossier_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_dossier_treeview_class_init( ofaDossierTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaDossierTreeview::ofa-doschanged:
	 *
	 * This signal is sent on the #ofaDossierTreeview when the selection
	 * is changed.
	 *
	 * Arguments are the selected ofaIDBDossierMeta and ofaIDBExerciceMeta objects;
	 * they may be %NULL
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview  *view,
	 * 						ofaIDBDossierMeta *meta,
	 * 						ofaIDBExerciceMeta      *period,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-doschanged",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_OBJECT );

	/**
	 * ofaDossierTreeview::ofa-dosactivated:
	 *
	 * This signal is sent on the #ofaDossierTreeview when the selection is
	 * activated.
	 *
	 * Arguments are the selected ofaIDBDossierMeta and ofaIDBExerciceMeta objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview  *view,
	 * 						ofaIDBDossierMeta *meta,
	 * 						ofaIDBExerciceMeta      *period,
	 * 						gpointer           user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-dosactivated",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_OBJECT );

	/**
	 * ofaDossierTreeview::ofa-dosdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaDossierTreeview proxyes it with this 'ofa-dosdelete' signal,
	 * providing the #ofoIDBDossierMeta/#ofaIDBExerciceMeta selected objects.
	 *
	 * Arguments are the selected ofaIDBDossierMeta and ofaIDBExerciceMeta objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierTreeview  *view,
	 * 						ofaIDBDossierMeta *meta,
	 * 						ofaIDBExerciceMeta      *period,
	 * 						gpointer           user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-dosdelete",
				OFA_TYPE_DOSSIER_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_OBJECT );
}

/**
 * ofa_dossier_treeview_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new ofaDossierTreeview instance.
 */
ofaDossierTreeview *
ofa_dossier_treeview_new( ofaHub *hub )
{
	ofaDossierTreeview *view;
	ofaDossierTreeviewPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	view = g_object_new( OFA_TYPE_DOSSIER_TREEVIEW,
					"ofa-tvbin-hub",    hub,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	priv = ofa_dossier_treeview_get_instance_private( view );

	priv->hub = hub;

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * #ofoIDBDossierMEta/#ofaIDBExerciceMeta objects instead of just
	 * the raw GtkTreeSelection
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

/**
 * ofa_dossier_treeview_set_settings_key:
 * @view: this #ofaDossierTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_dossier_treeview_set_settings_key( ofaDossierTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_dossier_treeview_set_settings_key";
	ofaDossierTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_dossier_treeview_setup_columns:
 * @view: this #ofaDossierTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_dossier_treeview_setup_columns( ofaDossierTreeview *view )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaDossierTreeview *self )
{
	static const gchar *thisfn = "ofa_dossier_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), DOSSIER_COL_DOSNAME,  _( "Dossier" ),  _( "Dossier name" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), DOSSIER_COL_PROVNAME, _( "Provider" ), _( "Provider name" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), DOSSIER_COL_PERNAME,  _( "Period" ),       NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), DOSSIER_COL_END,      _( "End" ),      _( "Exercice end" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), DOSSIER_COL_BEGIN,    _( "Begin" ),    _( "Exercice begin" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), DOSSIER_COL_STATUS,   _( "Status" ),       NULL );

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), DOSSIER_COL_DOSNAME );
}

static void
on_selection_changed( ofaDossierTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-doschanged" );
}

static void
on_selection_activated( ofaDossierTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-dosactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaDossierTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-dosdelete" );
}

/*
 * ofaIDBDossierMeta/ofaIDBExerciceMeta may be %NULL when selection is empty
 * (on 'ofa-doschanged' signal)
 */
static void
get_and_send( ofaDossierTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	gboolean ok;
	ofaIDBDossierMeta *meta;
	ofaIDBExerciceMeta *period;

	ok = get_selected_with_selection( self, selection, &meta, &period );
	g_return_if_fail( !ok || ( OFA_IS_IDBDOSSIER_META( meta ) && OFA_IS_IDBEXERCICE_META( period )));

	g_signal_emit_by_name( self, signal, meta, period );
}

/**
 * ofa_dossier_treeview_get_selected:
 * @view: this #ofaDossierTreeview instance.
 * @meta: [allow-none][out]: the currently selected #ofaIDBDossierMeta row;
 *  this reference is owned by the underlying #GtkTreeModel and should
 *  not be unreffed by the caller.
 * @period: [allow-none][out]: the currently selected #ofaIDBExerciceMeta row;
 *  this reference is owned by the underlying #GtkTreeModel and should
 *  not be unreffed by the caller.
 *
 * Returns: %TRUE if a selection exists, %FALSE else.
 */
gboolean
ofa_dossier_treeview_get_selected( ofaDossierTreeview *view, ofaIDBDossierMeta **meta, ofaIDBExerciceMeta **period )
{
	ofaDossierTreeviewPrivate *priv;
	gboolean ok;
	GtkTreeSelection *selection;

	g_return_val_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ), FALSE );

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	ok = get_selected_with_selection( view, selection, meta, period );

	return( ok );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaDossierTreeview instance.
 * @selection: the current #GtkTreeSelection.
 * @meta: [out][allow-none]:
 * @period: [out][allow-none]:
 *
 * Return: the currently selected class, or %NULL.
 */
static gboolean
get_selected_with_selection( ofaDossierTreeview *self, GtkTreeSelection *selection, ofaIDBDossierMeta **meta, ofaIDBExerciceMeta **period )
{
	gboolean ok;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofaIDBDossierMeta *row_meta = NULL;
	ofaIDBExerciceMeta *row_period = NULL;

	ok = FALSE;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		ok = TRUE;
		gtk_tree_model_get( tmodel, &iter,
				DOSSIER_COL_DOS_META,   &row_meta,
				DOSSIER_COL_EXE_META, &row_period,
				-1 );
		g_object_unref( row_meta );
		g_object_unref( row_period );
	}

	if( meta ){
		*meta = row_meta;
	}
	if( period ){
		*period = row_period;
	}

	return( ok );
}

/**
 * ofa_dossier_treeview_set_selected:
 * @view: this #ofaDossierTreeview instance.
 * @dname: [allow-none]: the name of the dossier to be selected.
 *
 * Select the specified @dname.
 */
void
ofa_dossier_treeview_set_selected( ofaDossierTreeview *view, const gchar *dname )
{
	static const gchar *thisfn = "ofa_dossier_treeview_set_selected";
	ofaDossierTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *str;
	gint cmp;

	g_debug( "%s: view=%p, dname=%s", thisfn, ( void * ) view, dname );

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_strlen( dname )){
		treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( view ));
		g_return_if_fail( treeview && GTK_IS_TREE_VIEW( treeview ));
		model = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( model, &iter )){
			while( TRUE ){
				gtk_tree_model_get( model, &iter, DOSSIER_COL_DOSNAME, &str, -1 );
				cmp = g_utf8_collate( str, dname );
				g_free( str );
				if( cmp == 0 ){
					ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( model, &iter )){
					break;
				}
			}
		}
	}
}

/**
 * ofa_dossier_treeview_set_show_all:
 * @view: this #ofaDossierTreeview instance.
 * @show_all: whether show all periods of each dossier.
 *
 * Whether to show all periods of each dossier (if %TRUE), or only one
 * row per dossier (if %FALSE).
 *
 * Defaults to %FALSE (one row per dossier).
 *
 * Note that not all columns are relevant when only one row is
 * displayed per dossier, as there is no sense of exercice in this
 * case.
 */
void
ofa_dossier_treeview_set_show_all( ofaDossierTreeview *view, gboolean show_all )
{
	ofaDossierTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->show_all = show_all;

	if( show_all ){
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_PERNAME, TRUE );
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_END,     TRUE );
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_BEGIN,   TRUE );
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_STATUS,  TRUE );
	} else {
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_PERNAME, FALSE );
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_END,     FALSE );
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_BEGIN,   FALSE );
		ofa_itvcolumnable_enable_column( OFA_ITVCOLUMNABLE( view ), DOSSIER_COL_STATUS,  FALSE );
	}

	ofa_tvbin_refilter( OFA_TVBIN( view ));
}

/**
 * ofa_dossier_treeview_setup_store:
 * @view: this #ofaDossierTreeview instance.
 *
 * Create the store which automatically loads the dataset.
 */
void
ofa_dossier_treeview_setup_store( ofaDossierTreeview *view )
{
	static const gchar *thisfn = "ofa_dossier_treeview_setup_store";
	ofaDossierTreeviewPrivate *priv;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_if_fail( view && OFA_IS_DOSSIER_TREEVIEW( view ));

	priv = ofa_dossier_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !priv->store ){
		priv->store = ofa_dossier_store_new( NULL, NULL );
		ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	}

	gtk_widget_show_all( GTK_WIDGET( view ));
}

static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaDossierTreeviewPrivate *priv;
	gboolean visible;
	ofaIDBDossierMeta *meta;
	ofaIDBExerciceMeta *period;
	GList *periods;

	priv = ofa_dossier_treeview_get_instance_private( OFA_DOSSIER_TREEVIEW( tvbin ));

	visible = TRUE;
	gtk_tree_model_get( model, iter, DOSSIER_COL_DOS_META, &meta, DOSSIER_COL_EXE_META, &period, -1 );

	/* note: a new row is first inserted, before the columns be set
	 * (cf. ofaDossierStore::insert_row())
	 * so just ignore the row if columns are still empty */
	if( !meta || !period ){
		return( FALSE );
	}

	if( priv->show_all ){
		visible = TRUE;

	} else {
		periods = ofa_idbdossier_meta_get_periods( meta );
		if( g_list_length( periods ) == 1 ){
			visible = TRUE;
		} else {
			visible = ofa_idbexercice_meta_get_current( period );
		}
		ofa_idbdossier_meta_free_periods( periods );
	}

	g_object_unref( period );
	g_object_unref( meta );

	return( visible );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_dossier_treeview_v_sort";
	ofaDossierTreeviewPrivate *priv;
	gint cmp;
	gchar *dosa, *prova, *pera, *enda, *begina, *stata;
	gchar *dosb, *provb, *perb, *endb, *beginb, *statb;

	priv = ofa_dossier_treeview_get_instance_private( OFA_DOSSIER_TREEVIEW( bin ));

	gtk_tree_model_get( tmodel, a,
			DOSSIER_COL_DOSNAME,  &dosa,
			DOSSIER_COL_PROVNAME, &prova,
			DOSSIER_COL_PERNAME,  &pera,
			DOSSIER_COL_END,      &enda,
			DOSSIER_COL_BEGIN,    &begina,
			DOSSIER_COL_STATUS,   &stata,
			-1 );

	gtk_tree_model_get( tmodel, b,
			DOSSIER_COL_DOSNAME,  &dosb,
			DOSSIER_COL_PROVNAME, &provb,
			DOSSIER_COL_PERNAME,  &perb,
			DOSSIER_COL_END,      &endb,
			DOSSIER_COL_BEGIN,    &beginb,
			DOSSIER_COL_STATUS,   &statb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case DOSSIER_COL_DOSNAME:
			cmp = my_collate( dosa, dosb );
			break;
		case DOSSIER_COL_PROVNAME:
			cmp = my_collate( prova, provb );
			break;
		case DOSSIER_COL_PERNAME:
			cmp = my_collate( pera, perb );
			break;
		case DOSSIER_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_display( priv->hub ));
			break;
		case DOSSIER_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_display( priv->hub ));
			break;
		case DOSSIER_COL_STATUS:
			cmp = my_collate( stata, statb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( dosa );
	g_free( prova );
	g_free( pera );
	g_free( enda );
	g_free( begina );
	g_free( stata );

	g_free( dosb );
	g_free( provb );
	g_free( perb );
	g_free( endb );
	g_free( beginb );
	g_free( statb );

	return( cmp );
}
