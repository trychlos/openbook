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
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"

#include "ofa-tva-record-store.h"
#include "ofa-tva-record-treeview.h"
#include "ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

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

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_TREEVIEW( instance ));

	/* free data members here */

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
 *
 * Returns: a new #ofaTVARecordTreeview instance.
 */
ofaTVARecordTreeview *
ofa_tva_record_treeview_new( void )
{
	ofaTVARecordTreeview *view;

	view = g_object_new( OFA_TYPE_TVA_RECORD_TREEVIEW,
				"ofa-tvbin-shadow", GTK_SHADOW_IN,
				NULL );

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

/**
 * ofa_tva_record_treeview_set_settings_key:
 * @view: this #ofaTVARecordTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_tva_record_treeview_set_settings_key( ofaTVARecordTreeview *view, const gchar *key )
{
	ofaTVARecordTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_TVA_RECORD_TREEVIEW( view ));

	priv = ofa_tva_record_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_tva_record_treeview_setup_columns:
 * @view: this #ofaTVARecordTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_tva_record_treeview_setup_columns( ofaTVARecordTreeview *view )
{
	ofaTVARecordTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_TVA_RECORD_TREEVIEW( view ));

	priv = ofa_tva_record_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaTVARecordTreeview *self )
{
	static const gchar *thisfn = "ofa_tva_record_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_MNEMO,          _( "Mnemo" ),          _( "Mnemonic" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), TVA_RECORD_COL_LABEL,          _( "Label" ),              NULL );
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), TVA_RECORD_COL_CORRESPONDENCE, _( "Correspondence" ),     NULL );
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), TVA_RECORD_COL_BEGIN,          _( "Begin" ),          _( "Beginning date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), TVA_RECORD_COL_END,            _( "End" ),            _( "Ending date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_IS_VALIDATED,   _( "Valid" ),          _( "Validated" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), TVA_RECORD_COL_DOPE,           _( "Ope" ),            _( "Operation date" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), TVA_RECORD_COL_NOTES,          _( "Notes" ),              NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), TVA_RECORD_COL_NOTES_PNG,         "",                 _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), TVA_RECORD_COL_UPD_USER,       _( "User" ),           _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), TVA_RECORD_COL_UPD_STAMP,          NULL,              _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), TVA_RECORD_COL_LABEL );
}

/**
 * ofa_tva_record_treeview_set_hub:
 * @view: this #ofaTVARecordTreeview instance.
 * @hub: the #ofaHub object of the application.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_tva_record_treeview_set_hub( ofaTVARecordTreeview *view, ofaHub *hub )
{
	ofaTVARecordTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_TVA_RECORD_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_tva_record_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	if( !ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( view ))){
		setup_columns( view );
	}

	priv->store = ofa_tva_record_store_new( hub );
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
	gint cmp;
	gchar *mnemoa, *labela, *correspa, *begina, *enda, *valida, *dopea, *notesa, *updusera, *updstampa;
	gchar *mnemob, *labelb, *correspb, *beginb, *endb, *validb, *dopeb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			TVA_RECORD_COL_MNEMO,          &mnemoa,
			TVA_RECORD_COL_LABEL,          &labela,
			TVA_RECORD_COL_CORRESPONDENCE, &correspa,
			TVA_RECORD_COL_BEGIN,          &begina,
			TVA_RECORD_COL_END,            &enda,
			TVA_RECORD_COL_IS_VALIDATED,   &valida,
			TVA_RECORD_COL_DOPE,           &dopea,
			TVA_RECORD_COL_NOTES,          &notesa,
			TVA_RECORD_COL_NOTES_PNG,      &pnga,
			TVA_RECORD_COL_UPD_USER,       &updusera,
			TVA_RECORD_COL_UPD_STAMP,      &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			TVA_RECORD_COL_MNEMO,          &mnemob,
			TVA_RECORD_COL_LABEL,          &labelb,
			TVA_RECORD_COL_CORRESPONDENCE, &correspb,
			TVA_RECORD_COL_BEGIN,          &beginb,
			TVA_RECORD_COL_END,            &endb,
			TVA_RECORD_COL_IS_VALIDATED,   &validb,
			TVA_RECORD_COL_DOPE,           &dopeb,
			TVA_RECORD_COL_NOTES,          &notesb,
			TVA_RECORD_COL_NOTES_PNG,      &pngb,
			TVA_RECORD_COL_UPD_USER,       &upduserb,
			TVA_RECORD_COL_UPD_STAMP,      &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case TVA_RECORD_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case TVA_RECORD_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case TVA_RECORD_COL_CORRESPONDENCE:
			cmp = my_collate( correspa, correspb );
			break;
		case TVA_RECORD_COL_BEGIN:
			cmp = my_date_compare_by_str( begina, beginb, ofa_prefs_date_display());
			break;
		case TVA_RECORD_COL_END:
			cmp = my_date_compare_by_str( enda, endb, ofa_prefs_date_display());
			break;
		case TVA_RECORD_COL_IS_VALIDATED:
			cmp = my_collate( valida, validb );
			break;
		case TVA_RECORD_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_display());
			break;
		case TVA_RECORD_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case TVA_RECORD_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case TVA_RECORD_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case TVA_RECORD_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( mnemoa );
	g_free( labela );
	g_free( correspa );
	g_free( begina );
	g_free( enda );
	g_free( valida );
	g_free( dopea );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( labelb );
	g_free( correspb );
	g_free( beginb );
	g_free( endb );
	g_free( validb );
	g_free( dopeb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
