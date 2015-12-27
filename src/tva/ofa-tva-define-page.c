/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-define-page.h"
#include "tva/ofa-tva-form-properties.h"
#include "tva/ofa-tva-form-store.h"
#include "tva/ofo-tva-form.h"

/* priv instance data
 */
struct _ofaTVADefinePagePrivate {

	/* internals
	 */
	ofoDossier      *dossier;
	gboolean         editable;

	/* UI
	 */
	GtkWidget       *treeview;
	GtkWidget       *update_btn;
	GtkWidget       *delete_btn;

	ofaTVAFormStore *store;
};

static GtkWidget  *v_setup_view( ofaPage *page );
static GtkWidget  *setup_treeview( ofaTVADefinePage *self );
static GtkWidget  *v_setup_buttons( ofaPage *page );
static GtkWidget  *v_get_top_focusable_widget( const ofaPage *page );
static gboolean    on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVADefinePage *self );
static ofoTVAForm *treeview_get_selected( ofaTVADefinePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void        setup_first_selection( ofaTVADefinePage *self );
static void        on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVADefinePage *page );
static void        on_row_selected( GtkTreeSelection *selection, ofaTVADefinePage *self );
static void        on_new_clicked( GtkButton *button, ofaTVADefinePage *page );
static void        on_update_clicked( GtkButton *button, ofaTVADefinePage *page );
static void        on_delete_clicked( GtkButton *button, ofaTVADefinePage *page );
static void        select_row_by_mnemo( ofaTVADefinePage *page, const gchar *mnemo );
static void        try_to_delete_current_row( ofaTVADefinePage *page );
static gboolean    delete_confirmed( ofaTVADefinePage *self, ofoTVAForm *form );
static void        do_delete( ofaTVADefinePage *page, ofoTVAForm *form, GtkTreeModel *tmodel, GtkTreeIter *iter );

G_DEFINE_TYPE( ofaTVADefinePage, ofa_tva_define_page, OFA_TYPE_PAGE )

static void
tva_define_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_define_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_DEFINE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_define_page_parent_class )->finalize( instance );
}

static void
tva_define_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_TVA_DEFINE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_define_page_parent_class )->dispose( instance );
}

static void
ofa_tva_define_page_init( ofaTVADefinePage *self )
{
	static const gchar *thisfn = "ofa_tva_define_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_DEFINE_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_DEFINE_PAGE, ofaTVADefinePagePrivate );
}

static void
ofa_tva_define_page_class_init( ofaTVADefinePageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_define_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_define_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_define_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaTVADefinePagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_define_page_v_setup_view";
	ofaTVADefinePagePrivate *priv;
	GtkWidget *treeview;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_TVA_DEFINE_PAGE( page )->priv;
	priv->dossier = ofa_page_get_dossier( page );
	priv->editable = ofo_dossier_is_current( priv->dossier );

	treeview = setup_treeview( OFA_TVA_DEFINE_PAGE( page ));
	setup_first_selection( OFA_TVA_DEFINE_PAGE( page ));

	return( treeview );
}

static GtkWidget *
setup_treeview( ofaTVADefinePage *self )
{
	ofaTVADefinePagePrivate *priv;
	GtkWidget *frame, *scroll, *tview;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

	frame = gtk_frame_new( NULL );
	my_utils_widget_set_margin( frame, 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	scroll = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scroll ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), scroll );

	tview = gtk_tree_view_new();
	gtk_widget_set_hexpand( tview, TRUE );
	gtk_widget_set_vexpand( tview, TRUE );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tview ), TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), tview );
	g_signal_connect( tview, "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( tview, "key-press-event", G_CALLBACK( on_treeview_key_pressed ), self );
	priv->treeview = tview;

	priv->store = ofa_tva_form_store_new( priv->dossier );
	gtk_tree_view_set_model( GTK_TREE_VIEW( priv->treeview ), GTK_TREE_MODEL( priv->store ));

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", TVA_COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->treeview ), column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", TVA_COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->treeview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaTVADefinePagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	priv = OFA_TVA_DEFINE_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );
	btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_NEW, TRUE, G_CALLBACK( on_new_clicked ), page );
	my_utils_widget_set_editable( btn, priv->editable );
	priv->update_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_PROPERTIES, FALSE, G_CALLBACK( on_update_clicked ), page );
	priv->delete_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_DELETE, FALSE, G_CALLBACK( on_delete_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_TVA_DEFINE_PAGE( page ), NULL );

	return( NULL );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_treeview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTVADefinePage *self )
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

static ofoTVAForm *
treeview_get_selected( ofaTVADefinePage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeSelection *select;
	ofoTVAForm *form;

	priv = page->priv;
	form = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){

		gtk_tree_model_get( *tmodel, iter, TVA_COL_OBJECT, &form, -1 );
		g_object_unref( form );
	}

	return( form );
}

static void
setup_first_selection( ofaTVADefinePage *self )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;
	model = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->treeview ));
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( priv->treeview );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVADefinePage *page )
{
	on_update_clicked( NULL, page );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaTVADefinePage *self )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	form = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, TVA_COL_OBJECT, &form, -1 );
		g_object_unref( form );
	}

	priv = self->priv;

	if( priv->update_btn ){
		gtk_widget_set_sensitive(
				priv->update_btn,
				form && OFO_IS_TVA_FORM( form ));
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive(
				priv->delete_btn,
				form && OFO_IS_TVA_FORM( form ) &&
					ofo_tva_form_is_deletable( form, priv->dossier ));
	}
}

static void
on_new_clicked( GtkButton *button, ofaTVADefinePage *page )
{
	ofaTVADefinePagePrivate *priv;
	ofoTVAForm *form;

	priv = page->priv;
	form = ofo_tva_form_new();

	if( ofa_tva_form_properties_run( ofa_page_get_main_window( OFA_PAGE( page )), form )){
		g_debug( "on_new_clicked: form=%p", ( void * ) form );
		select_row_by_mnemo( page, ofo_tva_form_get_mnemo( form ));

	} else {
		g_object_unref( form );
	}

	gtk_widget_grab_focus( priv->treeview );
}

static void
on_update_clicked( GtkButton *button, ofaTVADefinePage *page )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	priv = page->priv;
	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, TVA_COL_OBJECT, &form, -1 );
		g_object_unref( form );
		ofa_tva_form_properties_run( ofa_page_get_main_window( OFA_PAGE( page )), form );
		/* taken into account by dossier signaling system */
	}

	gtk_widget_grab_focus( priv->treeview );
}

static void
on_delete_clicked( GtkButton *button, ofaTVADefinePage *page )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	priv = page->priv;
	form = treeview_get_selected( page, &tmodel, &iter );
	if( form ){
		do_delete( page, form, tmodel, &iter );
	}

	gtk_widget_grab_focus( priv->treeview );
}

static void
select_row_by_mnemo( ofaTVADefinePage *page, const gchar *mnemo )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;
	GtkTreeSelection *selection;

	priv = page->priv;
	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->treeview ));
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, TVA_COL_OBJECT, &form, -1 );
			g_object_unref( form );
			if( !g_utf8_collate( mnemo, ofo_tva_form_get_mnemo( form ))){
				selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
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
try_to_delete_current_row( ofaTVADefinePage *page )
{
	ofaTVADefinePagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTVAForm *form;

	priv = page->priv;
	form = treeview_get_selected( page, &tmodel, &iter );
	if( form &&
			ofo_tva_form_is_deletable( form, priv->dossier )){
		do_delete( page, form, tmodel, &iter );
	}
}

static gboolean
delete_confirmed( ofaTVADefinePage *self, ofoTVAForm *form )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s' TVA form ?" ),
			ofo_tva_form_get_mnemo( form ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
do_delete( ofaTVADefinePage *page, ofoTVAForm *form, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaTVADefinePagePrivate *priv;
	gboolean deletable;

	priv = page->priv;
	deletable = ofo_tva_form_is_deletable( form, priv->dossier );
	g_return_if_fail( deletable );

	if( delete_confirmed( page, form )){
		ofo_tva_form_delete( form, priv->dossier );
		/* taken into account by dossier signaling system */
	}
}
