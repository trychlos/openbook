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
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ofa-recurrent-model-properties.h"
#include "ofa-recurrent-model-store.h"
#include "ofa-recurrent-main.h"
#include "ofa-recurrent-manage-page.h"
#include "ofo-recurrent-model.h"

/* priv instance data
 */
struct _ofaRecurrentManagePagePrivate {

	/* internals
	 */
	ofaHub              *hub;
	gboolean             is_current;

	/* UI
	 */
	GtkWidget           *model_treeview;
	GtkWidget           *update_btn;
	GtkWidget           *delete_btn;
	GtkWidget           *run_btn;
};

static GtkWidget         *v_setup_view( ofaPage *page );
static GtkWidget         *setup_model_treeview( ofaRecurrentManagePage *self );
static GtkWidget         *v_setup_buttons( ofaPage *page );
static GtkWidget         *v_get_top_focusable_widget( const ofaPage *page );
static gboolean           on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRecurrentManagePage *self );
static void               on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecurrentManagePage *self );
static void               on_row_selected( GtkTreeSelection *selection, ofaRecurrentManagePage *self );
static ofoRecurrentModel *treeview_get_selected( ofaRecurrentManagePage *self, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void               on_new_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void               on_update_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void               on_delete_clicked( GtkButton *button, ofaRecurrentManagePage *self );
static void               try_to_delete_current_row( ofaRecurrentManagePage *self );
static void               do_delete( ofaRecurrentManagePage *self, ofoRecurrentModel *model, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean           delete_confirmed( ofaRecurrentManagePage *self, ofoRecurrentModel *model );
static void               on_run_clicked( GtkButton *button, ofaRecurrentManagePage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentManagePage, ofa_recurrent_manage_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentManagePage ))

static void
recurrent_manage_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MANAGE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_manage_page_parent_class )->finalize( instance );
}

static void
recurrent_manage_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RECURRENT_MANAGE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_manage_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_manage_page_init( ofaRecurrentManagePage *self )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_MANAGE_PAGE( self ));
}

static void
ofa_recurrent_manage_page_class_init( ofaRecurrentManagePageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_manage_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_manage_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_manage_page_v_setup_view";
	ofaRecurrentManagePagePrivate *priv;
	ofoDossier *dossier;
	GtkWidget *grid, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	priv->is_current = ofo_dossier_is_current( dossier );

	grid = gtk_grid_new();

	widget = setup_model_treeview( OFA_RECURRENT_MANAGE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( grid ), widget, 0, 0, 1, 1 );

	return( grid );
}

/*
 * returns the container which displays the records
 */
static GtkWidget *
setup_model_treeview( ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GtkWidget *frame, *scrolled, *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofaRecurrentModelStore *store;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	frame = gtk_frame_new( NULL );
	my_utils_widget_set_margins( frame, 4, 4, 4, 0 );
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

	store = ofa_recurrent_model_store_new( priv->hub );
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), GTK_TREE_MODEL( store ));

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", REC_MODEL_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", REC_MODEL_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ope template" ),
			text_cell, "text", REC_MODEL_COL_OPE_TEMPLATE,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Periodicity" ),
			text_cell, "text", REC_MODEL_COL_PERIODICITY,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Detail" ),
			text_cell, "text", REC_MODEL_COL_PERIODICITY_DETAIL,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	priv->model_treeview = tview;

	return( frame );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaRecurrentManagePagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 4, 4, 0, 0 );

	btn = ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, TRUE );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	priv->run_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "_Run at date..." ), G_CALLBACK( on_run_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentManagePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_MANAGE_PAGE( page ), NULL );

	priv = ofa_recurrent_manage_page_get_instance_private( OFA_RECURRENT_MANAGE_PAGE( page ));

	return( priv->model_treeview );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRecurrentManagePage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_new_clicked( NULL, self );
		} else if( event->keyval == GDK_KEY_Delete ){
			try_to_delete_current_row( self );
		}
	}

	return( stop );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecurrentManagePage *self )
{
	on_update_clicked( NULL, self );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaRecurrentManagePage *self )
{
	ofaRecurrentManagePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoRecurrentModel *model;
	gboolean is_model;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	model = treeview_get_selected( self, &tmodel, &iter );
	is_model = model && OFO_IS_RECURRENT_MODEL( model );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_model );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_current && is_model && ofo_recurrent_model_is_deletable( model ));
	}

	if( priv->run_btn ){
		gtk_widget_set_sensitive( priv->run_btn,
				is_model );
	}
}

static ofoRecurrentModel *
treeview_get_selected( ofaRecurrentManagePage *self, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaRecurrentManagePagePrivate *priv;
	GtkTreeSelection *select;
	ofoRecurrentModel *object;

	priv = ofa_recurrent_manage_page_get_instance_private( self );

	object = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->model_treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, REC_MODEL_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && OFO_IS_RECURRENT_MODEL( object ), NULL );
		g_object_unref( object );
	}

	return( object );
}

/*
 * create a new recurrent model
 * creating a new recurrent record is the rule of 'Declare' button
 */
static void
on_new_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	ofoRecurrentModel *model;

	model = ofo_recurrent_model_new();
	ofa_recurrent_model_properties_run( ofa_page_get_main_window( OFA_PAGE( self )), model );
}

static void
on_update_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoRecurrentModel *model;

	model = treeview_get_selected( self, &tmodel, &iter );
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	ofa_recurrent_model_properties_run( ofa_page_get_main_window( OFA_PAGE( self )), model );
}

static void
on_delete_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoRecurrentModel *model;

	model = treeview_get_selected( self, &tmodel, &iter );
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	do_delete( self, model, tmodel, &iter );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

/*
 * when pressing the 'Delete' key on the treeview
 * cannot be sure that the current row is deletable
 */
static void
try_to_delete_current_row( ofaRecurrentManagePage *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoRecurrentModel *model;

	model = treeview_get_selected( self, &tmodel, &iter );
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	if( ofo_recurrent_model_is_deletable( model )){
		do_delete( self, model, tmodel, &iter );
	}
}

static void
do_delete( ofaRecurrentManagePage *self, ofoRecurrentModel *model, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	g_return_if_fail( ofo_recurrent_model_is_deletable( model ));

	if( delete_confirmed( self, model )){
		ofo_recurrent_model_delete( model );
		/* taken into account by dossier signaling system */
	}
}

static gboolean
delete_confirmed( ofaRecurrentManagePage *self, ofoRecurrentModel *model )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' recurrent model ?" ),
				ofo_recurrent_model_get_mnemo( model ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

/*
 * opening the Run page
 */
static void
on_run_clicked( GtkButton *button, ofaRecurrentManagePage *self )
{
	gint theme;

	theme = ofa_recurrent_main_get_theme( "recurrentrun" );
	if( theme > 0 ){
		ofa_main_window_activate_theme( OFA_MAIN_WINDOW( ofa_page_get_main_window( OFA_PAGE( self ))), theme );
	}
}
