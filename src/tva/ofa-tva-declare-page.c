/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-record-properties.h"
#include "tva/ofa-tva-record-store.h"
#include "tva/ofo-tva-record.h"

/* priv instance data
 */
struct _ofaTVADeclarePagePrivate {

	/* internals
	 */
	ofaHub       *hub;
	gboolean      is_current;

	/* UI
	 */
	GtkWidget    *record_treeview;
	GtkWidget    *update_btn;
	GtkWidget    *delete_btn;
};

static GtkWidget    *v_setup_view( ofaPage *page );
static GtkWidget    *setup_record_treeview( ofaTVADeclarePage *self );
static GtkWidget    *v_setup_buttons( ofaPage *page );
static GtkWidget    *v_get_top_focusable_widget( const ofaPage *page );
static gboolean      on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVADeclarePage *self );
static void          on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVADeclarePage *page );
static void          on_row_selected( GtkTreeSelection *selection, ofaTVADeclarePage *self );
static ofoTVARecord *treeview_get_selected( ofaTVADeclarePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void          on_update_clicked( GtkButton *button, ofaTVADeclarePage *page );
static void          on_delete_clicked( GtkButton *button, ofaTVADeclarePage *page );
static void          try_to_delete_current_row( ofaTVADeclarePage *page );
static void          do_delete( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean      delete_confirmed( ofaTVADeclarePage *self, ofoTVARecord *record );
static gboolean      find_row_by_ptr( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel **tmodel, GtkTreeIter *iter );

G_DEFINE_TYPE( ofaTVADeclarePage, ofa_tva_declare_page, OFA_TYPE_PAGE )

static void
tva_declare_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_declare_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_DECLARE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_declare_page_parent_class )->finalize( instance );
}

static void
tva_declare_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_TVA_DECLARE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_declare_page_parent_class )->dispose( instance );
}

static void
ofa_tva_declare_page_init( ofaTVADeclarePage *self )
{
	static const gchar *thisfn = "ofa_tva_declare_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_DECLARE_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_DECLARE_PAGE, ofaTVADeclarePagePrivate );
}

static void
ofa_tva_declare_page_class_init( ofaTVADeclarePageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_declare_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_declare_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_declare_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaTVADeclarePagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_declare_page_v_setup_view";
	ofaTVADeclarePagePrivate *priv;
	ofoDossier *dossier;
	GtkWidget *grid, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_TVA_DECLARE_PAGE( page )->priv;

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	priv->is_current = ofo_dossier_is_current( dossier );

	grid = gtk_grid_new();

	widget = setup_record_treeview( OFA_TVA_DECLARE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( grid ), widget, 0, 0, 1, 1 );

	return( grid );
}

/*
 * returns the container which displays the TVA form
 */
static GtkWidget *
setup_record_treeview( ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *frame, *scrolled, *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofaTVARecordStore *store;

	priv = self->priv;

	frame = gtk_frame_new( NULL );
	my_utils_widget_set_margin( frame, 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( frame ), scrolled );

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );
	g_signal_connect( tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( on_treeview_key_pressed ), self );
	gtk_container_add( GTK_CONTAINER( scrolled ), tview );

	store = ofa_tva_record_store_new( priv->hub );
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), GTK_TREE_MODEL( store ));

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", TVA_RECORD_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", TVA_RECORD_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Validated" ),
			text_cell, "text", TVA_RECORD_COL_IS_VALIDATED,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Begin" ),
			text_cell, "text", TVA_RECORD_COL_BEGIN,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "End" ),
			text_cell, "text", TVA_RECORD_COL_END,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	priv->record_treeview = tview;

	return( frame );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaTVADeclarePagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = OFA_TVA_DECLARE_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );

	/* always disabled */
	ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_NEW, NULL, NULL );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaTVADeclarePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_TVA_DECLARE_PAGE( page ), NULL );

	priv = OFA_TVA_DECLARE_PAGE( page )->priv;

	return( priv->record_treeview );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVADeclarePage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVADeclarePage *page )
{
	on_update_clicked( NULL, page );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;
	gboolean is_record;

	priv = self->priv;
	record = treeview_get_selected( self, &tmodel, &iter );
	is_record = record && OFO_IS_TVA_RECORD( record );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_record );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_current && is_record && ofo_tva_record_is_deletable( record ));
	}
}

static ofoTVARecord *
treeview_get_selected( ofaTVADeclarePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaTVADeclarePagePrivate *priv;
	GtkTreeSelection *select;
	ofoTVARecord *object;

	priv = page->priv;
	object = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->record_treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, TVA_RECORD_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && OFO_IS_TVA_RECORD( object ), NULL );
		g_object_unref( object );
	}

	return( object );
}

static void
on_update_clicked( GtkButton *button, ofaTVADeclarePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;

	record = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	ofa_tva_record_properties_run( ofa_page_get_main_window( OFA_PAGE( page )), record );
	/* update is taken into account by dossier signaling system */

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
on_delete_clicked( GtkButton *button, ofaTVADeclarePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;

	record = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	do_delete( page, record, tmodel, &iter );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

/*
 * when pressing the 'Delete' key on the treeview
 * cannot be sure that the current row is deletable
 */
static void
try_to_delete_current_row( ofaTVADeclarePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVARecord *record;

	record = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	if( ofo_tva_record_is_deletable( record )){
		do_delete( page, record, tmodel, &iter );
	}
}

static void
do_delete( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	g_return_if_fail( ofo_tva_record_is_deletable( record ));

	if( delete_confirmed( page, record )){
		ofo_tva_record_delete( record );
		/* taken into account by dossier signaling system */
	}
}

static gboolean
delete_confirmed( ofaTVADeclarePage *self, ofoTVARecord *record )
{
	gchar *msg, *send;
	gboolean delete_ok;

	send = my_date_to_str( ofo_tva_record_get_end( record ), ofa_prefs_date_display());
	msg = g_strdup_printf( _( "Are you sure you want delete the %s at %s TVA declaration ?" ),
				ofo_tva_record_get_mnemo( record ), send );

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );
	g_free( send );

	return( delete_ok );
}

/**
 * ofa_tva_declare_page_set_selected:
 * @page: this #ofaTVADeclarePage instance.
 * @record: the #ofoTVARecord to be selected.
 *
 * Select the specified @record.
 */
void
ofa_tva_declare_page_set_selected( ofaTVADeclarePage *page, ofoTVARecord *record )
{
	ofaTVADeclarePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_if_fail( page && OFA_IS_TVA_DECLARE_PAGE( page ));
	g_return_if_fail( record && OFO_IS_TVA_RECORD( record ));

	if( OFA_PAGE( page )->prot->dispose_has_run ){
		g_return_if_reached();
	}

	if( find_row_by_ptr( page, record, &tmodel, &iter )){
		priv = page->priv;
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->record_treeview ));
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_selection_select_path( select, path );
		gtk_tree_path_free( path );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static gboolean
find_row_by_ptr( ofaTVADeclarePage *page, ofoTVARecord *record, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaTVADeclarePagePrivate *priv;
	ofoTVARecord *row_object;
	const gchar *mnemo;
	const GDate *dend;
	gint cmp;

	priv = page->priv;
	*tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->record_treeview ));
	mnemo = ofo_tva_record_get_mnemo( record );
	dend = ofo_tva_record_get_end( record );
	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, TVA_RECORD_COL_OBJECT, &row_object, -1 );
			cmp = ofo_tva_record_compare_by_key( row_object, mnemo, dend );
			g_object_unref( row_object );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}
