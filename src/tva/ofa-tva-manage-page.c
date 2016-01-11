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

#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-form-properties.h"
#include "tva/ofa-tva-form-store.h"
#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-manage-page.h"
#include "tva/ofa-tva-record-new.h"
#include "tva/ofa-tva-record-properties.h"
#include "tva/ofo-tva-form.h"
#include "tva/ofo-tva-record.h"

/* priv instance data
 */
struct _ofaTVAManagePagePrivate {

	/* internals
	 */
	const ofaMainWindow *main_window;
	ofaHub              *hub;
	gboolean             is_current;

	/* UI
	 */
	GtkWidget           *form_treeview;
	GtkWidget           *update_btn;
	GtkWidget           *delete_btn;
	GtkWidget           *declare_btn;
};

static GtkWidget  *v_setup_view( ofaPage *page );
static GtkWidget  *setup_form_treeview( ofaTVAManagePage *self );
static GtkWidget  *v_setup_buttons( ofaPage *page );
static GtkWidget  *v_get_top_focusable_widget( const ofaPage *page );
static gboolean    on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVAManagePage *self );
static void        on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVAManagePage *page );
static void        on_row_selected( GtkTreeSelection *selection, ofaTVAManagePage *self );
static ofoTVAForm *treeview_get_selected( ofaTVAManagePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void        on_new_clicked( GtkButton *button, ofaTVAManagePage *page );
static void        select_row_by_object( ofaTVAManagePage *page, ofoTVAForm *form );
static void        on_update_clicked( GtkButton *button, ofaTVAManagePage *page );
static void        on_delete_clicked( GtkButton *button, ofaTVAManagePage *page );
static void        try_to_delete_current_row( ofaTVAManagePage *page );
static void        do_delete( ofaTVAManagePage *page, ofoTVAForm *form, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean    delete_confirmed( ofaTVAManagePage *self, ofoTVAForm *form );
static void        on_declare_clicked( GtkButton *button, ofaTVAManagePage *page );
static gboolean    do_declare( ofaTVAManagePage *page, ofoTVAForm *form );

G_DEFINE_TYPE( ofaTVAManagePage, ofa_tva_manage_page, OFA_TYPE_PAGE )

static void
tva_manage_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_manage_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_MANAGE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_manage_page_parent_class )->finalize( instance );
}

static void
tva_manage_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_TVA_MANAGE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_manage_page_parent_class )->dispose( instance );
}

static void
ofa_tva_manage_page_init( ofaTVAManagePage *self )
{
	static const gchar *thisfn = "ofa_tva_manage_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_MANAGE_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_MANAGE_PAGE, ofaTVAManagePagePrivate );
}

static void
ofa_tva_manage_page_class_init( ofaTVAManagePageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_manage_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_manage_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_manage_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaTVAManagePagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_manage_page_v_setup_view";
	ofaTVAManagePagePrivate *priv;
	ofoDossier *dossier;
	GtkWidget *grid, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_TVA_MANAGE_PAGE( page )->priv;
	priv->main_window = ofa_page_get_main_window( page );

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	priv->is_current = ofo_dossier_is_current( dossier );

	grid = gtk_grid_new();

	widget = setup_form_treeview( OFA_TVA_MANAGE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( grid ), widget, 0, 0, 1, 1 );

	return( grid );
}

/*
 * returns the container which displays the TVA form
 */
static GtkWidget *
setup_form_treeview( ofaTVAManagePage *self )
{
	ofaTVAManagePagePrivate *priv;
	GtkWidget *frame, *scrolled, *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	ofaTVAFormStore *store;

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

	store = ofa_tva_form_store_new( priv->hub );
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), GTK_TREE_MODEL( store ));

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", TVA_FORM_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", TVA_FORM_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	priv->form_treeview = tview;

	return( frame );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaTVAManagePagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = OFA_TVA_MANAGE_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );

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

	priv->declare_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "Declare from _form..." ), G_CALLBACK( on_declare_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaTVAManagePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_TVA_MANAGE_PAGE( page ), NULL );

	priv = OFA_TVA_MANAGE_PAGE( page )->priv;

	return( priv->form_treeview );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVAManagePage *self )
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
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVAManagePage *page )
{
	on_update_clicked( NULL, page );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaTVAManagePage *self )
{
	ofaTVAManagePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;
	gboolean is_form;

	priv = self->priv;
	form = treeview_get_selected( self, &tmodel, &iter );
	is_form = form && OFO_IS_TVA_FORM( form );

	if( priv->update_btn ){
		gtk_widget_set_sensitive( priv->update_btn,
				is_form );
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive( priv->delete_btn,
				priv->is_current && is_form && ofo_tva_form_is_deletable( form ));
	}

	if( priv->declare_btn ){
		gtk_widget_set_sensitive( priv->declare_btn,
				priv->is_current && is_form );
	}
}

static ofoTVAForm *
treeview_get_selected( ofaTVAManagePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaTVAManagePagePrivate *priv;
	GtkTreeSelection *select;
	ofoTVAForm *object;

	priv = page->priv;
	object = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->form_treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, TVA_FORM_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && OFO_IS_TVA_FORM( object ), NULL );
		g_object_unref( object );
	}

	return( object );
}

/*
 * create a new tva form
 * creating a new tva record is the rule of 'Declare' button
 */
static void
on_new_clicked( GtkButton *button, ofaTVAManagePage *page )
{
	ofaTVAManagePagePrivate *priv;
	ofoTVAForm *form;

	priv = page->priv;
	form = ofo_tva_form_new();

	if( ofa_tva_form_properties_run( priv->main_window, form )){
		select_row_by_object( page, form );

	} else {
		g_object_unref( form );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
select_row_by_object( ofaTVAManagePage *page, ofoTVAForm *object )
{
	ofaTVAManagePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *row_object;
	GtkTreeSelection *selection;
	gint cmp;

	priv = page->priv;
	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->form_treeview ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, TVA_FORM_COL_OBJECT, &row_object, -1 );
			g_return_if_fail( row_object && OFO_IS_TVA_FORM( row_object ));
			g_object_unref( row_object );
			cmp = ofo_tva_form_compare_id( object, row_object );
			if( cmp == 0 ){
				selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->form_treeview ));
				gtk_tree_selection_select_iter( selection, &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

static void
on_update_clicked( GtkButton *button, ofaTVAManagePage *page )
{
	ofaTVAManagePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	priv = page->priv;
	form = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	ofa_tva_form_properties_run( priv->main_window, form );
	/* update is taken into account by dossier signaling system */

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
on_delete_clicked( GtkButton *button, ofaTVAManagePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	form = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	do_delete( page, form, tmodel, &iter );

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

/*
 * when pressing the 'Delete' key on the treeview
 * cannot be sure that the current row is deletable
 */
static void
try_to_delete_current_row( ofaTVAManagePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	form = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	if( ofo_tva_form_is_deletable( form )){
		do_delete( page, form, tmodel, &iter );
	}
}

static void
do_delete( ofaTVAManagePage *page, ofoTVAForm *form, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	g_return_if_fail( ofo_tva_form_is_deletable( form ));

	if( delete_confirmed( page, form )){
		ofo_tva_form_delete( form );
		/* taken into account by dossier signaling system */
	}
}

static gboolean
delete_confirmed( ofaTVAManagePage *self, ofoTVAForm *form )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' TVA form ?" ),
				ofo_tva_form_get_mnemo( form ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

/*
 * new declaration from the currently selected form
 */
static void
on_declare_clicked( GtkButton *button, ofaTVAManagePage *page )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	form = treeview_get_selected( page, &tmodel, &iter );
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	if( !do_declare( page, form )){
		gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
	}
}

/*
 * Returns: %TRUE if we have actually created a new declaration
 */
static gboolean
do_declare( ofaTVAManagePage *page, ofoTVAForm *form )
{
	ofaTVAManagePagePrivate *priv;
	ofoTVARecord *record;
	guint theme;
	const ofaMainWindow *main_window;
	ofaPage *declare_page;
	gboolean ok;

	priv = page->priv;
	ok = FALSE;

	record = ofo_tva_record_new_from_form( form );
	if( ofa_tva_record_new_run( priv->main_window, record ) &&
		ofa_tva_record_properties_run( priv->main_window, record )){

		theme = ofa_tva_main_get_theme( "tvadeclare" );
		main_window = ofa_page_get_main_window( OFA_PAGE( page ));
		declare_page = ofa_main_window_activate_theme( main_window, theme );
		ofa_tva_declare_page_set_selected( OFA_TVA_DECLARE_PAGE( declare_page ), record );
		ok = TRUE;

	} else {
		g_object_unref( record );
	}

	return( ok );
}
