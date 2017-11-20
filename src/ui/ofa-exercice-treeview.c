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

#include "my/my-utils.h"

#include "api/ofa-dossier-store.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-exercice-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;
	gchar             *settings_prefix;

	/* runtime
	 */
	ofaDossierStore   *store;
	ofaIDBDossierMeta *meta;
}
	ofaExerciceTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     setup_columns( ofaExerciceTreeview *view );
static void     on_selection_changed( ofaExerciceTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_activated( ofaExerciceTreeview *self, GtkTreeSelection *selection, void *empty );
static void     on_selection_delete( ofaExerciceTreeview *self, GtkTreeSelection *selection, void *empty );
static void     get_and_send( ofaExerciceTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static gboolean get_selected_with_selection( ofaExerciceTreeview *self, GtkTreeSelection *selection, ofaIDBExerciceMeta **period );
static void     set_selected_with_selection( ofaExerciceTreeview *self, GtkTreeSelection *selection, ofaIDBExerciceMeta *meta );
static gboolean tvbin_v_filter( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint     tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaExerciceTreeview, ofa_exercice_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaExerciceTreeview ))

static void
exercice_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_treeview_finalize";
	ofaExerciceTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_exercice_treeview_get_instance_private( OFA_EXERCICE_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_treeview_parent_class )->finalize( instance );
}

static void
exercice_treeview_dispose( GObject *instance )
{
	ofaExerciceTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_TREEVIEW( instance ));

	priv = ofa_exercice_treeview_get_instance_private( OFA_EXERCICE_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->store );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_treeview_parent_class )->dispose( instance );
}

static void
ofa_exercice_treeview_init( ofaExerciceTreeview *self )
{
	static const gchar *thisfn = "ofa_exercice_treeview_init";
	ofaExerciceTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_TREEVIEW( self ));

	priv = ofa_exercice_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->store = NULL;
	priv->meta = NULL;
}

static void
ofa_exercice_treeview_class_init( ofaExerciceTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaExerciceTreeview::ofa-exechanged:
	 *
	 * This signal is sent on the #ofaExerciceTreeview when the selection
	 * is changed.
	 *
	 * Arguments is the selected ofaIDBExerciceMeta object, which may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceTreeview  *view,
	 * 						ofaIDBExerciceMeta *period,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-exechanged",
				OFA_TYPE_EXERCICE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaExerciceTreeview::ofa-exeactivated:
	 *
	 * This signal is sent on the #ofaExerciceTreeview when the selection is
	 * activated.
	 *
	 * Argument is the selected ofaIDBExerciceMeta object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceTreeview  *view,
	 * 						ofaIDBExerciceMeta *period,
	 * 						gpointer            user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-exeactivated",
				OFA_TYPE_EXERCICE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaExerciceTreeview::ofa-exedelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaExerciceTreeview proxyes it with this 'ofa-exedelete' signal,
	 * providing the #ofoIDBDossierMeta/#ofaIDBExerciceMeta selected objects.
	 *
	 * Arguments is the selected ofaIDBExerciceMeta object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExerciceTreeview  *view,
	 * 						ofaIDBExerciceMeta *period,
	 * 						gpointer            user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-exedelete",
				OFA_TYPE_EXERCICE_TREEVIEW,
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
 * ofa_exercice_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the user preferences in settings.
 *
 * Returns: a new #ofaExerciceTreeview instance.
 */
ofaExerciceTreeview *
ofa_exercice_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaExerciceTreeview *view;
	ofaExerciceTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_EXERCICE_TREEVIEW,
					"ofa-tvbin-getter", getter,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	priv = ofa_exercice_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * #ofoIDBMEta/#ofaIDBExerciceMeta objects instead of just the raw
	 * GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	setup_columns( view );

	priv->store = ofa_dossier_store_new( getter );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaExerciceTreeview *self )
{
	static const gchar *thisfn = "ofa_exercice_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), DOSSIER_COL_EXELABEL, _( "Label" ),      NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), DOSSIER_COL_END,      _( "End" ),    _( "Exercice end" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), DOSSIER_COL_BEGIN,    _( "Begin" ),  _( "Exercice begin" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), DOSSIER_COL_STATUS,   _( "Status" ),     NULL );

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), DOSSIER_COL_EXELABEL );
}

/**
 * ofa_exercice_treeview_set_dossier:
 * @view: this #ofaExerciceTreeview instance.
 * @meta: [allow-none]: the #ofaIDBDossierMeta which describes the dossier.
 *
 * Update the treeview to show the exercices attached to @meta.
 * Select the first available exercice if any.
 */
void
ofa_exercice_treeview_set_dossier( ofaExerciceTreeview *view, ofaIDBDossierMeta *meta )
{
	static const gchar *thisfn = "ofa_exercice_treeview_set_dossier";
	ofaExerciceTreeviewPrivate *priv;
	const GList *periods;

	g_debug( "%s: view=%p, meta=%p", thisfn, ( void * ) view, ( void * ) meta );

	g_return_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ));
	g_return_if_fail( !meta || OFA_IS_IDBDOSSIER_META( meta ));

	priv = ofa_exercice_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->meta = meta;
	ofa_tvbin_refilter( OFA_TVBIN( view ));

	if( meta ){
		periods = ofa_idbdossier_meta_get_periods( meta );
		if( periods && g_list_length(( GList * ) periods ) >= 1 ){
			ofa_exercice_treeview_set_selected( view, OFA_IDBEXERCICE_META( periods->data ));
		}
	}
}

static void
on_selection_changed( ofaExerciceTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-exechanged" );
}

static void
on_selection_activated( ofaExerciceTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-exeactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaExerciceTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-exedelete" );
}

/*
 * ofaIDBExerciceMeta may be %NULL when selection is empty (on 'ofa-exechanged' signal)
 */
static void
get_and_send( ofaExerciceTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	gboolean ok;
	ofaIDBExerciceMeta *period;

	ok = get_selected_with_selection( self, selection, &period );
	g_return_if_fail( !ok || OFA_IS_IDBEXERCICE_META( period ));

	g_signal_emit_by_name( self, signal, period );
}

/**
 * ofa_exercice_treeview_get_selected:
 * @view: this #ofaExerciceTreeview instance.
 *
 * Returns: the currently selected #ofaIDBExerciceMeta row, or %NULL.
 *
 * When exists, the returned reference is owned by the @view and should
 *  not be releawed by the caller.
 */
ofaIDBExerciceMeta *
ofa_exercice_treeview_get_selected( ofaExerciceTreeview *view )
{
	ofaExerciceTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofaIDBExerciceMeta *meta;

	g_return_val_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ), FALSE );

	priv = ofa_exercice_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	get_selected_with_selection( view, selection, &meta );

	return( meta );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaExerciceTreeview instance.
 * @selection: the current #GtkTreeSelection.
 * @period: [out][allow-none]:
 *
 * Returns: %TRUE if an exercice was selected (and @period is thus non
 *  %NULL), %FALSE if there was no current selection, and @period is set
 *  to %NULL.
 */
static gboolean
get_selected_with_selection( ofaExerciceTreeview *self, GtkTreeSelection *selection, ofaIDBExerciceMeta **period )
{
	gboolean ok;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofaIDBExerciceMeta *row_period;

	ok = FALSE;
	row_period = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		ok = TRUE;
		gtk_tree_model_get( tmodel, &iter, DOSSIER_COL_EXE_META, &row_period, -1 );
		g_return_val_if_fail( row_period && OFA_IS_IDBEXERCICE_META( row_period ), FALSE );
		g_object_unref( row_period );
	}

	if( period ){
		*period = row_period;
	}

	return( ok );
}

/**
 * ofa_exercice_treeview_set_selected:
 * @view: this #ofaExerciceTreeview instance.
 * @meta: [allow-none]: the #ofaIDBExerciceMeta to select;
 *  if %NULL, then unselect all.
 *
 * Select the @meta row.
 */
void
ofa_exercice_treeview_set_selected( ofaExerciceTreeview *view, ofaIDBExerciceMeta *meta )
{
	static const gchar *thisfn = "ofa_exercice_treeview_set_selected";
	ofaExerciceTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_debug( "%s: view=%p, meta=%p", thisfn, ( void * ) view, ( void * ) meta );

	g_return_if_fail( view && OFA_IS_EXERCICE_TREEVIEW( view ));
	g_return_if_fail( !meta || OFA_IS_IDBEXERCICE_META( meta ));

	priv = ofa_exercice_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	set_selected_with_selection( view, selection, meta );
}

static void
set_selected_with_selection( ofaExerciceTreeview *self, GtkTreeSelection *selection, ofaIDBExerciceMeta *meta )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofaIDBExerciceMeta *row_period;

	gtk_tree_selection_unselect_all( selection );

	if( meta ){
		tmodel = ofa_tvbin_get_tree_model( OFA_TVBIN( self ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, DOSSIER_COL_EXE_META, &row_period, -1 );
				g_return_if_fail( row_period && OFA_IS_IDBEXERCICE_META( row_period ));
				g_object_unref( row_period );
				if( ofa_idbexercice_meta_compare( meta, row_period ) == 0 ){
					gtk_tree_selection_select_iter( selection, &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}
			}
		}
	}
}

static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaExerciceTreeviewPrivate *priv;
	ofaIDBDossierMeta *meta;

	priv = ofa_exercice_treeview_get_instance_private( OFA_EXERCICE_TREEVIEW( tvbin ));

	gtk_tree_model_get( model, iter, DOSSIER_COL_DOS_META, &meta, -1 );

	if( !meta ){
		return( FALSE );
	}

	g_object_unref( meta );

	return( meta == priv->meta );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_exercice_treeview_v_sort";
	ofaExerciceTreeviewPrivate *priv;
	gint cmp;
	gchar *dosa, *prova, *pera, *enda, *begina, *stata, *laba;
	gchar *dosb, *provb, *perb, *endb, *beginb, *statb, *labb;

	priv = ofa_exercice_treeview_get_instance_private( OFA_EXERCICE_TREEVIEW( bin ));

	gtk_tree_model_get( tmodel, a,
			DOSSIER_COL_DOSNAME,  &dosa,
			DOSSIER_COL_PROVNAME, &prova,
			DOSSIER_COL_PERNAME,  &pera,
			DOSSIER_COL_EXELABEL, &laba,
			DOSSIER_COL_END,      &enda,
			DOSSIER_COL_BEGIN,    &begina,
			DOSSIER_COL_STATUS,   &stata,
			-1 );

	gtk_tree_model_get( tmodel, b,
			DOSSIER_COL_DOSNAME,  &dosb,
			DOSSIER_COL_PROVNAME, &provb,
			DOSSIER_COL_PERNAME,  &perb,
			DOSSIER_COL_EXELABEL, &labb,
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
		case DOSSIER_COL_EXELABEL:
			cmp = my_collate( laba, labb );
			break;
		case DOSSIER_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case DOSSIER_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_get_display_format( priv->getter ));
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
	g_free( laba );
	g_free( enda );
	g_free( begina );
	g_free( stata );

	g_free( dosb );
	g_free( provb );
	g_free( perb );
	g_free( labb );
	g_free( endb );
	g_free( beginb );
	g_free( statb );

	return( cmp );
}
