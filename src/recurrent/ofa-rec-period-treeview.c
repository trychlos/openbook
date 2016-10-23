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
#include "api/ofa-icontext.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"

#include "recurrent/ofa-rec-period-store.h"
#include "recurrent/ofa-rec-period-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* runtime
	 */
	ofaHub   *hub;
}
	ofaRecPeriodTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void          setup_columns( ofaRecPeriodTreeview *self );
static void          on_selection_changed( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, void *empty );
static void          on_selection_activated( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, void *empty );
static void          on_selection_delete( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, void *empty );
static void          get_and_send( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoRecPeriod *get_selected_with_selection( ofaRecPeriodTreeview *self, GtkTreeSelection *selection );
static gint          tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaRecPeriodTreeview, ofa_rec_period_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaRecPeriodTreeview ))

static void
rec_period_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rec_period_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_treeview_parent_class )->finalize( instance );
}

static void
rec_period_treeview_dispose( GObject *instance )
{
	ofaRecPeriodTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_TREEVIEW( instance ));

	priv = ofa_rec_period_treeview_get_instance_private( OFA_REC_PERIOD_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_treeview_parent_class )->dispose( instance );
}

static void
ofa_rec_period_treeview_init( ofaRecPeriodTreeview *self )
{
	static const gchar *thisfn = "ofa_rec_period_treeview_init";
	ofaRecPeriodTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_REC_PERIOD_TREEVIEW( self ));

	priv = ofa_rec_period_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_rec_period_treeview_class_init( ofaRecPeriodTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_rec_period_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rec_period_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = rec_period_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaRecPeriodTreeview::ofa-perchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecPeriodTreeview proxyes it with this 'ofa-perchanged'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object; may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecPeriodTreeview *view,
	 * 						ofoRecPeriod       *period,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-perchanged",
				OFA_TYPE_REC_PERIOD_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaRecPeriodTreeview::ofa-peractivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecPeriodTreeview proxyes it with this 'ofa-peractivated'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecPeriodTreeview *view,
	 * 						ofoRecPeriod       *period,
	 * 						gpointer            user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-peractivated",
				OFA_TYPE_REC_PERIOD_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaRecPeriodTreeview::ofa-perdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaRecPeriodTreeview proxyes it with this 'ofa-perdelete'
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRecPeriodTreeview *view,
	 * 						ofoRecPeriod       *period,
	 * 						gpointer            user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-perdelete",
				OFA_TYPE_REC_PERIOD_TREEVIEW,
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
 * ofa_rec_period_treeview_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new empty #ofaRecPeriodTreeview composite object.
 *
 * Rationale: this same #ofaRecPeriodTreeview class is used both by
 * the #ofaRecPeriodPage and by the #ofaRecurrentNew dialog. This
 * later should not be updated when new operations are inserted.
 */
ofaRecPeriodTreeview *
ofa_rec_period_treeview_new( ofaHub *hub )
{
	ofaRecPeriodTreeview *view;
	ofaRecPeriodTreeviewPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	view = g_object_new( OFA_TYPE_REC_PERIOD_TREEVIEW,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	priv = ofa_rec_period_treeview_get_instance_private( view );

	priv->hub = hub;

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoRecPeriod object instead of just the raw GtkTreeSelection
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
 * ofa_rec_period_treeview_set_settings_key:
 * @view: this #ofaRecPeriodTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_rec_period_treeview_set_settings_key( ofaRecPeriodTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_rec_period_treeview_set_settings_key";
	ofaRecPeriodTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_REC_PERIOD_TREEVIEW( view ));

	priv = ofa_rec_period_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_rec_period_treeview_setup_columns:
 * @view: this #ofaRecPeriodTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_rec_period_treeview_setup_columns( ofaRecPeriodTreeview *view )
{
	ofaRecPeriodTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_REC_PERIOD_TREEVIEW( view ));

	priv = ofa_rec_period_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaRecPeriodTreeview *self )
{
	static const gchar *thisfn = "ofa_rec_period_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), PER_COL_ID,            _( "Id" ),       _( "Periodicity identifier" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), PER_COL_ORDER,         _( "Order" ),    _( "Periodicity display order" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), PER_COL_LABEL,         _( "Label" ),    _( "Periodicity label" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), PER_COL_DETAILS_COUNT, _( "Details" ),  _( "Detail types count" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), PER_COL_NOTES,         _( "Notes" ),        NULL);
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), PER_COL_NOTES_PNG,        "",           _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), PER_COL_UPD_USER,      _( "User" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), PER_COL_UPD_STAMP,        NULL,         _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), PER_COL_LABEL );
}

static void
on_selection_changed( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-perchanged" );
}

static void
on_selection_activated( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-peractivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-perdelete" );
}

/*
 * gtk_tree_selection_get_selected_rows() works even if selection mode
 * is GTK_SELECTION_MULTIPLE (which may happen here)
 */
static void
get_and_send( ofaRecPeriodTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoRecPeriod *period;

	period = get_selected_with_selection( self, selection );
	g_return_if_fail( !period || OFO_IS_REC_PERIOD( period ));

	g_signal_emit_by_name( self, signal, period );
}

/**
 * ofa_rec_period_treeview_get_selected:
 *
 * Returns: the#ofoRecPeriod selected object, may be %NULL.
 */
ofoRecPeriod *
ofa_rec_period_treeview_get_selected( ofaRecPeriodTreeview *view )
{
	ofaRecPeriodTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_return_val_if_fail( view && OFA_IS_REC_PERIOD_TREEVIEW( view ), NULL );

	priv = ofa_rec_period_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

/*
 */
static ofoRecPeriod *
get_selected_with_selection( ofaRecPeriodTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoRecPeriod *period;

	period = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, PER_COL_OBJECT, &period, -1 );
		g_return_val_if_fail( period && OFO_IS_REC_PERIOD( period ), NULL );
		g_object_unref( period );
	}

	return( period );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_rec_period_treeview_v_sort";
	gchar *ida, *labela, *notesa, *usera, *stampa;
	gchar *idb, *labelb, *notesb, *userb, *stampb;
	guint ordera, orderb, counta, countb;
	GdkPixbuf *pnga, *pngb;
	gint cmp;

	gtk_tree_model_get( tmodel, a,
			PER_COL_ID,              &ida,
			PER_COL_ORDER_I,         &ordera,
			PER_COL_LABEL,           &labela,
			PER_COL_DETAILS_COUNT_I, &counta,
			PER_COL_NOTES,           &notesa,
			PER_COL_NOTES_PNG,       &pnga,
			PER_COL_UPD_USER,        &usera,
			PER_COL_UPD_STAMP,       &stampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			PER_COL_ID,              &idb,
			PER_COL_ORDER_I,         &orderb,
			PER_COL_LABEL,           &labelb,
			PER_COL_DETAILS_COUNT_I, &countb,
			PER_COL_NOTES,           &notesb,
			PER_COL_NOTES_PNG,       &pngb,
			PER_COL_UPD_USER,        &userb,
			PER_COL_UPD_STAMP,       &stampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case PER_COL_ID:
			cmp = my_collate( ida, idb );
			break;
		case PER_COL_ORDER:
			cmp = ordera < orderb ? -1 : ( ordera > orderb ? 1 : 0 );
			break;
		case PER_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case PER_COL_DETAILS_COUNT:
			cmp = ( counta < countb ? -1 : ( counta > countb ? 1 : 0 ));
			break;
		case PER_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case PER_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case PER_COL_UPD_USER:
			cmp = my_collate( usera, userb );
			break;
		case PER_COL_UPD_STAMP:
			cmp = my_collate( stampa, stampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( ida );
	g_free( labela );
	g_free( notesa );
	g_free( usera );
	g_free( stampa );

	g_free( idb );
	g_free( labelb );
	g_free( notesb );
	g_free( userb );
	g_free( stampb );

	return( cmp );
}
