/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-ledger.h"

#include "ui/my-buttons-box.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ope-template-properties.h"
#include "ui/ofa-ope-templates-page.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaOpeTemplatesPagePrivate {

	/* properties
	 */

	/* internals
	 */
	GList         *handlers;

	/* UI
	 */
	GtkNotebook   *book;				/* one page per imputation ledger */
	GtkTreeView   *tview;				/* the treeview of the current page */
	GtkTreeModel  *tmodel;
	GtkWidget     *update_btn;
	GtkWidget     *duplicate_btn;
	GtkWidget     *delete_btn;
	GtkWidget     *guided_input_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_OBJECT,
	N_COLUMNS
};

/* data attached to each page of the model category notebook
 */
#define DATA_PAGE_LEDGER                 "data-page-ledger-id"
#define DATA_PAGE_VIEW                   "data-page-treeview"

G_DEFINE_TYPE( ofaOpeTemplatesPage, ofa_ope_templates_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static void       insert_ledger_dataset( ofaOpeTemplatesPage *self, GtkNotebook *book );
static void       setup_dossier_signaling( ofaOpeTemplatesPage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_ope_templates_dataset( ofaOpeTemplatesPage *self );
static GtkWidget *book_create_page( ofaOpeTemplatesPage *self, GtkNotebook *book, const gchar *ledger, const gchar *ledger_label );
static gboolean   book_activate_page_by_ledger( ofaOpeTemplatesPage *self, const gchar *ledger );
static gint       book_get_page_by_ledger( ofaOpeTemplatesPage *self, const gchar *ledger );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplatesPage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       insert_new_row( ofaOpeTemplatesPage *self, ofoOpeTemplate *model, gboolean with_selection );
static void       set_row_by_iter( ofaOpeTemplatesPage *self, ofoOpeTemplate *model, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void       setup_first_selection( ofaOpeTemplatesPage *self );
static void       on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplatesPage *self );
static void       on_model_selected( GtkTreeSelection *selection, ofaOpeTemplatesPage *self );
static void       enable_buttons( ofaOpeTemplatesPage *self, GtkTreeSelection *selection );
static void       on_new_clicked( GtkButton *button, ofaPage *page );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesPage *self );
static void       on_update_clicked( GtkButton *button, ofaPage *page );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaOpeTemplatesPage *self );
static void       do_update_ledger_label( ofaOpeTemplatesPage *self, ofoLedger *ledger );
static void       on_delete_clicked( GtkButton *button, ofaPage *page );
static gboolean   delete_confirmed( ofaOpeTemplatesPage *self, ofoOpeTemplate *model );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesPage *self );
static void       on_duplicate( GtkButton *button, ofaOpeTemplatesPage *self );
static void       on_dossier_dates_changed( ofoDossier *dossier, const GDate *begin, const GDate *end, ofaOpeTemplatesPage *self );
static void       on_guided_input( GtkButton *button, ofaOpeTemplatesPage *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaOpeTemplatesPage *self );

static void
ope_templates_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_templates_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATES_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_templates_page_parent_class )->finalize( instance );
}

static void
ope_templates_page_dispose( GObject *instance )
{
	ofaOpeTemplatesPagePrivate *priv;
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATES_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		priv = ( OFA_OPE_TEMPLATES_PAGE( instance ))->priv;
		dossier = ofa_page_get_dossier( OFA_PAGE( instance ));
		if( OFO_IS_DOSSIER( dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_templates_page_parent_class )->dispose( instance );
}

static void
ofa_ope_templates_page_init( ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATES_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATES_PAGE, ofaOpeTemplatesPagePrivate );
}

static void
ofa_ope_templates_page_class_init( ofaOpeTemplatesPageClass *klass )
{
	static const gchar *thisfn = "ofa_ope_templates_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_templates_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_templates_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplatesPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaOpeTemplatesPage *self;
	ofaOpeTemplatesPagePrivate *priv;
	GtkNotebook *book;

	self = OFA_OPE_TEMPLATES_PAGE( page );
	priv = self->priv;

	setup_dossier_signaling( self );

	book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( book ), 4 );
	gtk_widget_set_hexpand( GTK_WIDGET( book ), TRUE );
	gtk_notebook_set_scrollable( book, TRUE );
	gtk_notebook_popup_enable( book );

	priv->book = GTK_NOTEBOOK( book );

	insert_ledger_dataset( OFA_OPE_TEMPLATES_PAGE( page ), priv->book );

	/* connect after the pages have been created */
	g_signal_connect( G_OBJECT( book ), "switch-page", G_CALLBACK( on_page_switched ), self );

	return( GTK_WIDGET( book ));
}

static void
insert_ledger_dataset( ofaOpeTemplatesPage *self, GtkNotebook *book )
{
	GList *dataset, *iset;
	ofoLedger *ledger;

	dataset = ofo_ledger_get_dataset( ofa_page_get_dossier( OFA_PAGE( self )));

	for( iset=dataset ; iset ; iset=iset->next ){
		ledger = OFO_LEDGER( iset->data );
		book_create_page( self, book, ofo_ledger_get_mnemo( ledger ), ofo_ledger_get_label( ledger ));
	}
}

static void
setup_dossier_signaling( ofaOpeTemplatesPage *self )
{
	ofaOpeTemplatesPagePrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = self->priv;
	dossier = ofa_page_get_dossier( OFA_PAGE( self ));

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

/*
 * have to rebuild the whole buttons box, in order to insert a
 * 'Duplicate' button
 */
static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaOpeTemplatesPagePrivate *priv;
	myButtonsBox *box;
	GtkWidget *button;
	ofoDossier *dossier;
	gulong handler;

	priv = OFA_OPE_TEMPLATES_PAGE( page )->priv;
	box = my_buttons_box_new();
	my_buttons_box_set_header_rows( box, 2 );

	my_buttons_box_pack_button_by_id( box,
								BUTTONS_BOX_NEW,
								TRUE, G_CALLBACK( on_new_clicked ), page );
	priv->update_btn = my_buttons_box_pack_button_by_id( box,
								BUTTONS_BOX_PROPERTIES,
								FALSE, G_CALLBACK( on_update_clicked ), page );
	priv->duplicate_btn = my_buttons_box_pack_button_by_id( box,
								BUTTONS_BOX_DUPLICATE,
								FALSE, G_CALLBACK( on_duplicate ), page );
	priv->delete_btn = my_buttons_box_pack_button_by_id( box,
								BUTTONS_BOX_DELETE,
								FALSE, G_CALLBACK( on_delete_clicked ), page );

	my_buttons_box_add_spacer( box );

	button = gtk_button_new_with_mnemonic( _( "_Guided input..." ));
	my_buttons_box_pack_button( box,
			button, FALSE, G_CALLBACK( on_guided_input ), page );
	priv->guided_input_btn = button;

	dossier = ofa_page_get_dossier( page );
	on_dossier_dates_changed( dossier, NULL, NULL, OFA_OPE_TEMPLATES_PAGE( page ));

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_DATES_CHANGED, G_CALLBACK( on_dossier_dates_changed ), page );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	return( GTK_WIDGET( box ));
}

static void
v_init_view( ofaPage *page )
{
	insert_ope_templates_dataset( OFA_OPE_TEMPLATES_PAGE( page ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_OPE_TEMPLATES_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_OPE_TEMPLATES_PAGE( page )->priv->tview ));
}

static void
insert_ope_templates_dataset( ofaOpeTemplatesPage *self )
{
	GList *dataset, *iset;

	dataset = ofo_ope_template_get_dataset( ofa_page_get_dossier( OFA_PAGE( self )));

	for( iset=dataset ; iset ; iset=iset->next ){
		insert_new_row( self, OFO_OPE_TEMPLATE( iset->data ), FALSE );
	}

	setup_first_selection( self );
}

static GtkWidget *
book_create_page( ofaOpeTemplatesPage *self, GtkNotebook *book, const gchar *ledger, const gchar *ledger_label )
{
	GtkScrolledWindow *scroll;
	GtkLabel *label;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	label = GTK_LABEL( gtk_label_new_with_mnemonic( ledger_label ));
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), GTK_WIDGET( label ), -1 );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_LEDGER, g_strdup( ledger ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_VIEW, tview );
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), self );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_model_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	gtk_widget_show_all( GTK_WIDGET( scroll ));

	return( GTK_WIDGET( scroll ));
}

static gboolean
book_activate_page_by_ledger( ofaOpeTemplatesPage *self, const gchar *mnemo )
{
	gint page_num;

	page_num = book_get_page_by_ledger( self, mnemo );
	if( page_num < 0 ){
		page_num = book_get_page_by_ledger( self, UNKNOWN_LEDGER_MNEMO );
		if( page_num < 0 ){
				book_create_page( self, self->priv->book, UNKNOWN_LEDGER_MNEMO, UNKNOWN_LEDGER_LABEL );
				page_num = book_get_page_by_ledger( self, UNKNOWN_LEDGER_MNEMO );
		}
	}
	g_return_val_if_fail( page_num >= 0, FALSE );

	gtk_notebook_set_current_page( self->priv->book, page_num );
	return( TRUE );
}

static gint
book_get_page_by_ledger( ofaOpeTemplatesPage *self, const gchar *mnemo )
{
	gint count, i;
	GtkWidget *page_widget;
	const gchar *ledger;

	count = gtk_notebook_get_n_pages( self->priv->book );

	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( self->priv->book, i );
		ledger = ( const gchar * ) g_object_get_data( G_OBJECT( page_widget ), DATA_PAGE_LEDGER );
		if( !g_utf8_collate( ledger, mnemo )){
			return( i );
		}
	}

	return( -1 );
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplatesPage *self )
{
	gchar *amnemo, *bmnemo, *afold, *bfold;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_MNEMO, &amnemo, -1 );
	afold = g_utf8_casefold( amnemo, -1 );

	gtk_tree_model_get( tmodel, b, COL_MNEMO, &bmnemo, -1 );
	bfold = g_utf8_casefold( bmnemo, -1 );

	cmp = g_utf8_collate( afold, bfold );

	g_free( amnemo );
	g_free( afold );
	g_free( bmnemo );
	g_free( bfold );

	return( cmp );
}

/*
 * double click on a row opens the rate properties
 */
static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	on_update_clicked( NULL, page );
}

static void
insert_new_row( ofaOpeTemplatesPage *self, ofoOpeTemplate *model, gboolean with_selection )
{
	ofaOpeTemplatesPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreePath *path;

	/* find the page for this ledger and activates it
	 * creating a new page if the ledger has not been found */
	if( book_activate_page_by_ledger( self, ofo_ope_template_get_ledger( model ))){

		priv = self->priv;

		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
		g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( priv->tmodel ),
				&iter,
				-1,
				COL_MNEMO,  ofo_ope_template_get_mnemo( model ),
				COL_OBJECT, model,
				-1 );

		set_row_by_iter( self, model, priv->tmodel, &iter );

		/* select the newly added row */
		if( with_selection ){
			path = gtk_tree_model_get_path( priv->tmodel, &iter );
			gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
			gtk_tree_path_free( path );
			gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
		}
	}
}

static void
set_row_by_iter( ofaOpeTemplatesPage *self, ofoOpeTemplate *model, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			iter,
			COL_MNEMO,  ofo_ope_template_get_mnemo( model ),
			COL_LABEL,  ofo_ope_template_get_label( model ),
			-1 );
}

static void
setup_first_selection( ofaOpeTemplatesPage *self )
{
	ofaOpeTemplatesPagePrivate *priv;
	GtkWidget *first_tab;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;

	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self->priv->book ), 0 );

	if( first_tab ){
		gtk_notebook_set_current_page( GTK_NOTEBOOK( self->priv->book ), 0 );
		priv->tview =
				( GtkTreeView * ) my_utils_container_get_child_by_type(
												GTK_CONTAINER( first_tab ),
												GTK_TYPE_TREE_VIEW );
		g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));

		priv->tmodel = gtk_tree_view_get_model( priv->tview );
		if( gtk_tree_model_get_iter_first( priv->tmodel, &iter )){
			select = gtk_tree_view_get_selection( priv->tview );
			gtk_tree_selection_select_iter( select, &iter );
		}

		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

/*
 * note that switching the notebook page, though *visually* selects the
 * first row, actually does NOT send any selection message (so does not
 * trigger #on_model_selected()) when the selection does not change on
 * the treeview:
 *
 * - when activating a notebook for the first time, and if there is at
 *   least one row, then select this row (and emit the signal)
 *
 * - when coming back another time to this same page, then the selection
 *   does not change, and though no message is sent.
 *
 * After reloading the dataset, the notebook switches to the first page
 * before having created the treeview
 */
static void
on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_page_switched";
	ofaOpeTemplatesPagePrivate *priv;

	g_debug( "%s: book=%p, wpage=%p, npage=%d, page=%p",
			thisfn, ( void * ) book, ( void * ) wpage, npage, ( void * ) self );

	priv = self->priv;

	priv->tview = ( GtkTreeView * )
							my_utils_container_get_child_by_type(
									GTK_CONTAINER( wpage ), GTK_TYPE_TREE_VIEW );
	if( priv->tview ){
		g_return_if_fail( GTK_IS_TREE_VIEW( priv->tview ));
		priv->tmodel = gtk_tree_view_get_model( priv->tview );
		enable_buttons( self, gtk_tree_view_get_selection(priv-> tview ));

	} else {
		enable_buttons( self, NULL );
	}
}

static void
on_model_selected( GtkTreeSelection *selection, ofaOpeTemplatesPage *self )
{
	/*static const gchar *thisfn = "ofa_ope_templates_page_on_model_selected";*/

	/*g_debug( "%s: selection=%p, self=%p", thisfn, ( void * ) selection, ( void * ) self );*/

	enable_buttons( self, selection );
}

static void
enable_buttons( ofaOpeTemplatesPage *self, GtkTreeSelection *selection )
{
	/*static const gchar *thisfn = "ofa_ope_templates_page_enable_buttons";*/
	ofaOpeTemplatesPagePrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoOpeTemplate *model;
	gboolean select_ok;

	/*g_debug( "%s: self=%p, selection=%p", thisfn, ( void * ) self, ( void * ) selection );*/

	select_ok = selection
					? gtk_tree_selection_get_selected( selection, &tmodel, &iter )
					: FALSE;

	priv = self->priv;

	if( select_ok ){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );
		/*g_debug( "%s: current selection is %s", thisfn, ofo_ope_template_get_mnemo( model ));*/

		gtk_widget_set_sensitive(
				priv->update_btn,
				model && OFO_IS_OPE_TEMPLATE( model ));
		gtk_widget_set_sensitive(
				priv->delete_btn,
				model && OFO_IS_OPE_TEMPLATE( model ) &&
					ofo_ope_template_is_deletable( model, ofa_page_get_dossier( OFA_PAGE( self ))));

	} else {
		gtk_widget_set_sensitive( priv->update_btn, FALSE );
		gtk_widget_set_sensitive( priv->delete_btn, FALSE );
	}

	gtk_widget_set_sensitive( priv->duplicate_btn, select_ok );
	gtk_widget_set_sensitive( priv->guided_input_btn, select_ok );
}

static void
on_new_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_new_clicked";
	ofoOpeTemplate *model;
	gint page_n;
	GtkWidget *page_w;
	const gchar *mnemo;

	g_return_if_fail( OFA_IS_OPE_TEMPLATES_PAGE( page ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) page );

	model = ofo_ope_template_new();
	page_n = gtk_notebook_get_current_page( OFA_OPE_TEMPLATES_PAGE( page )->priv->book );
	page_w = gtk_notebook_get_nth_page( OFA_OPE_TEMPLATES_PAGE( page )->priv->book, page_n );
	mnemo = ( const gchar * ) g_object_get_data( G_OBJECT( page_w ), DATA_PAGE_LEDGER );

	if( ofa_ope_template_properties_run(
			ofa_page_get_main_window( page ), model, mnemo )){

		/* managed by dossier signaling system */

	} else {
		g_object_unref( model );
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		insert_new_row( self, OFO_OPE_TEMPLATE( object ), TRUE );
	}
}

/*
 * We cannot rely here on the standard dossier signaling system.
 * This is due because we display the entry models in a notebook per
 * ledger - So managing a ledger change implies having both the
 * previous identifier and the previous ledger.
 * As this is the only use case, we consider upgrading the dossier
 * signaling system of no worth..
 */
static void
on_update_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_update_clicked";
	ofaOpeTemplatesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	gchar *prev_ledger;
	const gchar *new_ledger;
	ofoOpeTemplate *model;

	g_return_if_fail( OFA_IS_OPE_TEMPLATES_PAGE( page ));

	priv = OFA_OPE_TEMPLATES_PAGE( page )->priv;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		prev_ledger = g_strdup( ofo_ope_template_get_ledger( model ));

		if( ofa_ope_template_properties_run(
				ofa_page_get_main_window( page ),
				model,
				NULL )){

			new_ledger = ofo_ope_template_get_ledger( model );

			if( g_utf8_collate( prev_ledger, new_ledger )){
				gtk_list_store_remove( GTK_LIST_STORE( priv->tmodel ), &iter );
				insert_new_row( OFA_OPE_TEMPLATES_PAGE( page ), model, TRUE );

			} else {
				set_row_by_iter( OFA_OPE_TEMPLATES_PAGE( page ), model, priv->tmodel, &iter );
			}
		}

		g_free( prev_ledger );
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		/* managed by the button clic handle */

	} else if( OFO_IS_LEDGER( object )){
		do_update_ledger_label( self, OFO_LEDGER( object ));
	}
}

/*
 * only takes care here of the change of the ledger label
 * if the mnemo has been changed, then the whole dataset is to be
 * reloaded
 */
static void
do_update_ledger_label( ofaOpeTemplatesPage *self, ofoLedger *ledger )
{
	gint page_num;
	GtkWidget *page_w;

	page_num = book_get_page_by_ledger( self, ofo_ledger_get_mnemo( ledger ));
	if( page_num >= 0 ){
		page_w = gtk_notebook_get_nth_page( self->priv->book, page_num );
		g_return_if_fail( page_w && GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text(
				self->priv->book, page_w, ofo_ledger_get_label( ledger ));
	}
}

/*
 * un model peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
on_delete_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_delete_clicked";
	ofaOpeTemplatesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoOpeTemplate *model;

	g_return_if_fail( OFA_IS_OPE_TEMPLATES_PAGE( page ));

	priv = OFA_OPE_TEMPLATES_PAGE( page )->priv;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		if( delete_confirmed( OFA_OPE_TEMPLATES_PAGE( page ), model ) &&
				ofo_ope_template_delete( model, ofa_page_get_dossier( page ))){

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( priv->tmodel ), &iter );
		}
	}
}

static gboolean
delete_confirmed( ofaOpeTemplatesPage *self, ofoOpeTemplate *model )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' entry model ?" ),
			ofo_ope_template_get_mnemo( model ),
			ofo_ope_template_get_label( model ));

	delete_ok = ofa_main_window_confirm_deletion(
						ofa_page_get_main_window( OFA_PAGE( self )), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		/* manage by the button clic handle */

	} else if( OFO_IS_LEDGER( object )){
		/* a ledger has been deleted */
	}
}

static void
on_duplicate( GtkButton *button, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_duplicate";
	ofaOpeTemplatesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoOpeTemplate *model;
	ofoOpeTemplate *duplicate;
	gchar *str;

	g_return_if_fail( OFA_IS_OPE_TEMPLATES_PAGE( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	priv = self->priv;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		duplicate = ofo_ope_template_new_from_template( model );
		str = ofo_ope_template_get_mnemo_new_from( model, ofa_page_get_dossier( OFA_PAGE( self )));
		ofo_ope_template_set_mnemo( duplicate, str );
		g_free( str );
		str = g_strdup_printf( "%s (%s)", ofo_ope_template_get_label( model ), _( "Duplicate" ));
		ofo_ope_template_set_label( duplicate, str );
		g_free( str );

		if( !ofo_ope_template_insert( duplicate, ofa_page_get_dossier( OFA_PAGE( self )))){
			g_object_unref( duplicate );
		}
	}
}

static void
on_dossier_dates_changed( ofoDossier *dossier, const GDate *begin, const GDate *end, ofaOpeTemplatesPage *self )
{
	/* what to do here (if anything to do..) ? */
}

static void
on_guided_input( GtkButton *button, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_guided_input";
	ofaOpeTemplatesPagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoOpeTemplate *model;

	g_return_if_fail( OFA_IS_OPE_TEMPLATES_PAGE( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	priv = self->priv;

	g_return_if_fail( priv->tview && GTK_IS_TREE_VIEW( priv->tview ));
	g_return_if_fail( priv->tmodel && GTK_IS_TREE_MODEL( priv->tmodel ));

	select = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &model, -1 );
		g_object_unref( model );

		ofa_guided_input_run(
				ofa_page_get_main_window( OFA_PAGE( self )), model );
	}
}

/*
 * all pages are rebuilt not only if the entry models is reloaded, but
 * also when the ledgers are reloaded
 */
static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaOpeTemplatesPage *self )
{
	static const gchar *thisfn = "ofa_ope_templates_page_on_reloaded_dataset";
	ofaOpeTemplatesPagePrivate *priv;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	priv = self->priv;

	if( type == OFO_TYPE_OPE_TEMPLATE || type == OFO_TYPE_LEDGER ){
		while( gtk_notebook_get_n_pages( priv->book )){
			gtk_notebook_remove_page( priv->book, 0 );
		}
		insert_ledger_dataset( self, priv->book );
		insert_ope_templates_dataset( self );
	}
}
