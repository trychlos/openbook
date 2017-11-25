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
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"

#include "ofa-tva-record-store.h"
#include "ofa-tva-record-treeview.h"
#include "ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;
	gchar             *settings_prefix;

	/* UI
	 */
	ofaTVARecordStore *store;
}
	ofaTVARecordTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void          setup_columns( ofaTVARecordTreeview *self );
static void          on_selection_changed( ofaTVARecordTreeview *self, GtkTreeSelection *selection, void *empty );
static void          on_selection_activated( ofaTVARecordTreeview *self, GtkTreeSelection *selection, void *empty );
static void          on_selection_delete( ofaTVARecordTreeview *self, GtkTreeSelection *selection, void *empty );
static void          get_and_send( ofaTVARecordTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoTVARecord *get_selected_with_selection( ofaTVARecordTreeview *self, GtkTreeSelection *selection );
static gint          tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaTVARecordTreeview, ofa_tva_record_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaTVARecordTreeview ))

static void
tva_record_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_treeview_finalize";
	ofaTVARecordTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_tva_record_treeview_get_instance_private( OFA_TVA_RECORD_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_treeview_parent_class )->finalize( instance );
}

static void
tva_record_treeview_dispose( GObject *instance )
{
	ofaTVARecordTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_TREEVIEW( instance ));

	priv = ofa_tva_record_treeview_get_instance_private( OFA_TVA_RECORD_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_treeview_parent_class )->dispose( instance );
}

static void
ofa_tva_record_treeview_init( ofaTVARecordTreeview *self )
{
	static const gchar *thisfn = "ofa_tva_record_treeview_init";
	ofaTVARecordTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_RECORD_TREEVIEW( self ));

	priv = ofa_tva_record_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->store = NULL;
}

static void
ofa_tva_record_treeview_class_init( ofaTVARecordTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaTVARecordTreeview::ofa-vatchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaTVARecordTreeview proxyes it with this 'ofa-vatchanged'
	 * signal, providing the selected object (which may be %NULL).
	 *
	 * Argument is the selected object; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVARecordTreeview *view,
	 * 						ofoTVARecord       *object,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-vatchanged",
				OFA_TYPE_TVA_RECORD_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVARecordTreeview::ofa-vatactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaTVARecordTreeview proxyes it with this 'ofa-vatactivated'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVARecordTreeview *view,
	 * 						ofoTVARecord       *object,
	 * 						gpointer            user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-vatactivated",
				OFA_TYPE_TVA_RECORD_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVARecordTreeview::ofa-vatdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaTVARecordTreeview proxyes it with this 'ofa-vatdelete'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVARecordTreeview *view,
	 * 						ofoTVARecord       *object,
	 * 						gpointer            user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-vatdelete",
				OFA_TYPE_TVA_RECORD_TREEVIEW,
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
 * ofa_tva_record_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the key prefix in user settings.
 *
 * Returns: a new #ofaTVARecordTreeview instance.
 */
ofaTVARecordTreeview *
ofa_tva_record_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaTVARecordTreeview *view;
	ofaTVARecordTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_TVA_RECORD_TREEVIEW,
				"ofa-tvbin-getter", getter,
				"ofa-tvbin-shadow", GTK_SHADOW_IN,
				NULL );

	priv = ofa_tva_record_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	setup_columns( view );

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
setup_columns( ofaTVARecordTreeview *self )
{
	static const gchar *thisfn = "ofa_tva_record_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_MNEMO,              _( "Mnemo" ),        _( "Mnemonic" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), TVA_RECORD_COL_END,                _( "End" ),          _( "Ending date" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), TVA_RECORD_COL_HAS_CORRESPONDENCE, _( "Has.corresp." ), _( "Has correspondence" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_CRE_USER,           _( "Cre.user" ),     _( "Creation user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_CRE_STAMP,          _( "Cre.stamp" ),    _( "Creation timestamp" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), TVA_RECORD_COL_LABEL,              _( "Label" ),            NULL );
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), TVA_RECORD_COL_CORRESPONDENCE,     _( "Corresp." ),     _( "Correspondence" ));
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), TVA_RECORD_COL_CORRESPONDENCE_PNG,    "",               _( "Correspondence indicator" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), TVA_RECORD_COL_BEGIN,              _( "Begin" ),        _( "Beginning date" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), TVA_RECORD_COL_NOTES,              _( "Notes" ),            NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), TVA_RECORD_COL_NOTES_PNG,             "",               _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_UPD_USER,           _( "Upd.user" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_UPD_STAMP,          _( "Upd.stamp" ),    _( "Last update timestamp" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), TVA_RECORD_COL_DOPE,               _( "Ope" ),          _( "Operation date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_OPE_USER,           _( "Ope.user" ),     _( "Operation user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_OPE_STAMP,          _( "Ope.stamp" ),    _( "Operation timestamp" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_STATUS,             _( "Valid." ),       _( "Validation status" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_STA_CLOSING,        _( "Val.date" ),     _( "Validation date" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_STA_USER,           _( "Sta.user" ),     _( "Status last change user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_STA_STAMP,          _( "Sta.stamp" ),    _( "Status last change timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), TVA_RECORD_COL_LABEL );
}

/**
 * ofa_tva_record_treeview_setup_store:
 * @view: this #ofaTVARecordTreeview instance.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_tva_record_treeview_setup_store( ofaTVARecordTreeview *view )
{
	ofaTVARecordTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_TVA_RECORD_TREEVIEW( view ));

	priv = ofa_tva_record_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_tva_record_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), TVA_RECORD_COL_MNEMO, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaTVARecordTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-vatchanged" );
}

static void
on_selection_activated( ofaTVARecordTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-vatactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaTVARecordTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-vatdelete" );
}

static void
get_and_send( ofaTVARecordTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoTVARecord *form;

	form = get_selected_with_selection( self, selection );
	g_signal_emit_by_name( self, signal, form );
}

/**
 * ofa_tva_record_treeview_get_selected:
 * @view: this #ofaTVARecordTreeview instance.
 *
 * Returns: the selected #ofoTVARecord object, which may be %NULL.
 *
 * The returned reference is owned by the underlying #ofaTVARecordStore,
 * and should not be released by the caller.
 */
ofoTVARecord *
ofa_tva_record_treeview_get_selected( ofaTVARecordTreeview *view )
{
	ofaTVARecordTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_return_val_if_fail( view && OFA_IS_TVA_RECORD_TREEVIEW( view ), NULL );

	priv = ofa_tva_record_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

static ofoTVARecord *
get_selected_with_selection( ofaTVARecordTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *form;

	form = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, TVA_RECORD_COL_OBJECT, &form, -1 );
		g_return_val_if_fail( form && OFO_IS_TVA_RECORD( form ), NULL );
		g_object_unref( form );
	}

	return( form );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_tva_record_treeview_v_sort";
	ofaTVARecordTreeviewPrivate *priv;
	gint cmp;
	gchar *mnemoa, *enda, *hascora, *screua, *scresa, *labela, *correspa, *begina, *notesa, *supdua, *supdsa,
			*dopea, *sopeua, *sopesa, *stata, *closa, *sstaua, *sstasa;
	gchar *mnemob, *endb, *hascorb, *screub, *scresb, *labelb, *correspb, *beginb, *notesb, *supdub, *supdsb,
			*dopeb, *sopeub, *sopesb, *statb, *closb, *sstaub, *sstasb;
	GdkPixbuf *corinda, *pnga, *corindb, *pngb;

	priv = ofa_tva_record_treeview_get_instance_private( OFA_TVA_RECORD_TREEVIEW( bin ));

	gtk_tree_model_get( tmodel, a,
			TVA_RECORD_COL_MNEMO,              &mnemoa,
			TVA_RECORD_COL_END,                &enda,
			TVA_RECORD_COL_HAS_CORRESPONDENCE, &hascora,
			TVA_RECORD_COL_CRE_USER,           &screua,
			TVA_RECORD_COL_CRE_STAMP,          &scresa,
			TVA_RECORD_COL_LABEL,              &labela,
			TVA_RECORD_COL_CORRESPONDENCE,     &correspa,
			TVA_RECORD_COL_CORRESPONDENCE_PNG, &corinda,
			TVA_RECORD_COL_BEGIN,              &begina,
			TVA_RECORD_COL_NOTES,              &notesa,
			TVA_RECORD_COL_NOTES_PNG,          &pnga,
			TVA_RECORD_COL_UPD_USER,           &supdua,
			TVA_RECORD_COL_UPD_STAMP,          &supdsa,
			TVA_RECORD_COL_DOPE,               &dopea,
			TVA_RECORD_COL_OPE_USER,           &sopeua,
			TVA_RECORD_COL_OPE_STAMP,          &sopesa,
			TVA_RECORD_COL_STATUS,             &stata,
			TVA_RECORD_COL_STA_CLOSING,        &closa,
			TVA_RECORD_COL_STA_USER,           &sstaua,
			TVA_RECORD_COL_STA_STAMP,          &sstasa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			TVA_RECORD_COL_MNEMO,              &mnemob,
			TVA_RECORD_COL_END,                &endb,
			TVA_RECORD_COL_HAS_CORRESPONDENCE, &hascorb,
			TVA_RECORD_COL_CRE_USER,           &screub,
			TVA_RECORD_COL_CRE_STAMP,          &scresb,
			TVA_RECORD_COL_LABEL,              &labelb,
			TVA_RECORD_COL_CORRESPONDENCE,     &correspb,
			TVA_RECORD_COL_CORRESPONDENCE_PNG, &corindb,
			TVA_RECORD_COL_BEGIN,              &beginb,
			TVA_RECORD_COL_NOTES,              &notesb,
			TVA_RECORD_COL_NOTES_PNG,          &pngb,
			TVA_RECORD_COL_UPD_USER,           &supdub,
			TVA_RECORD_COL_UPD_STAMP,          &supdsb,
			TVA_RECORD_COL_DOPE,               &dopeb,
			TVA_RECORD_COL_OPE_USER,           &sopeub,
			TVA_RECORD_COL_OPE_STAMP,          &sopesb,
			TVA_RECORD_COL_STATUS,             &statb,
			TVA_RECORD_COL_STA_CLOSING,        &closb,
			TVA_RECORD_COL_STA_USER,           &sstaub,
			TVA_RECORD_COL_STA_STAMP,          &sstasb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case TVA_RECORD_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case TVA_RECORD_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case TVA_RECORD_COL_HAS_CORRESPONDENCE:
			cmp = my_collate( hascora, hascorb );
			break;
		case TVA_RECORD_COL_CRE_USER:
			cmp = my_collate( screua, screub );
			break;
		case TVA_RECORD_COL_CRE_STAMP:
			cmp = my_collate( scresa, scresb );
			break;
		case TVA_RECORD_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case TVA_RECORD_COL_CORRESPONDENCE:
			cmp = my_collate( correspa, correspb );
			break;
		case TVA_RECORD_COL_CORRESPONDENCE_PNG:
			cmp = ofa_itvsortable_sort_png( corinda, corindb );
			break;
		case TVA_RECORD_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case TVA_RECORD_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case TVA_RECORD_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case TVA_RECORD_COL_UPD_USER:
			cmp = my_collate( supdua, supdub );
			break;
		case TVA_RECORD_COL_UPD_STAMP:
			cmp = my_collate( supdsa, supdsb );
			break;
		case TVA_RECORD_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case TVA_RECORD_COL_OPE_USER:
			cmp = my_collate( sopeua, sopeub );
			break;
		case TVA_RECORD_COL_OPE_STAMP:
			cmp = my_collate( sopesa, sopesb );
			break;
		case TVA_RECORD_COL_STATUS:
			cmp = my_collate( stata, statb );
			break;
		case TVA_RECORD_COL_STA_CLOSING:
			cmp = my_date_compare_by_str( closa, closb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case TVA_RECORD_COL_STA_USER:
			cmp = my_collate( sstaua, sstaub );
			break;
		case TVA_RECORD_COL_STA_STAMP:
			cmp = my_collate( sstasa, sstasb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( enda );
	g_free( hascora );
	g_free( screua );
	g_free( scresa );
	g_free( labela );
	g_free( correspa );
	g_free( begina );
	g_free( notesa );
	g_free( supdua );
	g_free( supdsa );
	g_free( dopea );
	g_free( sopeua );
	g_free( sopesa );
	g_free( stata );
	g_free( closa );
	g_free( sstaua );
	g_free( sstasa );
	g_clear_object( &corinda );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( endb );
	g_free( hascorb );
	g_free( screub );
	g_free( scresb );
	g_free( labelb );
	g_free( correspb );
	g_free( beginb );
	g_free( notesb );
	g_free( supdub );
	g_free( supdsb );
	g_free( dopeb );
	g_free( sopeub );
	g_free( sopesb );
	g_free( statb );
	g_free( closb );
	g_free( sstaub );
	g_free( sstasb );
	g_clear_object( &corindb );
	g_clear_object( &pngb );

	return( cmp );
}
