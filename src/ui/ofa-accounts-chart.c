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

#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-chart.h"
#include "ui/ofo-account.h"

/* private class data
 */
struct _ofaAccountsChartClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaAccountsChartPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* are set when run for the first time (page is new)
	 */

	/* UI
	 */
	GtkNotebook   *chart_book;			/* one page per account class */
	GtkButton     *update_btn;
	GtkButton     *delete_btn;
	GtkButton     *consult_btn;
	GtkTreeView   *current;				/* tree view of the current page */
};

/* column ordering in the selection listview
 */
enum {
	COL_NUMBER = 0,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_OBJECT,
	N_COLUMNS
};

static const gchar  *st_class_labels[] = {
		N_( "Class I" ),
		N_( "Class II" ),
		N_( "Class III" ),
		N_( "Class IV" ),
		N_( "Financial accounts" ),
		N_( "Expense accounts" ),
		N_( "Revenue accounts" ),
		N_( "Class VIII" ),
		N_( "Class IX" ),
		NULL
};

/* data attached to each page of the classes notebook
 */
#define DATA_PAGE_CLASS                 "data-page-class"
#define DATA_PAGE_VIEW                  "data-page-view"

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaAccountsChartClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_constructed( GObject *instance );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_page_chart( ofaAccountsChart *self );
static GtkWidget *setup_chart_book( ofaAccountsChart *self );
static void       store_set_account( GtkTreeModel *model, GtkTreeIter *iter, const ofoAccount *account );
static GtkWidget *setup_buttons_box( ofaAccountsChart *self );
static void       setup_first_selection( ofaAccountsChart *self );
static void       on_class_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountsChart *self );
static GtkWidget *notebook_create_page( ofaAccountsChart *self, GtkNotebook *book, gint class, gint position );
static void       on_account_selected( GtkTreeSelection *selection, ofaAccountsChart *self );
static void       on_new_account( GtkButton *button, ofaAccountsChart *self );
static void       on_update_account( GtkButton *button, ofaAccountsChart *self );
static void       on_delete_account( GtkButton *button, ofaAccountsChart *self );
static void       error_undeletable( ofaAccountsChart *self, ofoAccount *account );
static gboolean   delete_confirmed( ofaAccountsChart *self, ofoAccount *account );
static void       on_view_entries( GtkButton *button, ofaAccountsChart *self );
static void       insert_new_row( ofaAccountsChart *self, ofoAccount *account );

GType
ofa_accounts_chart_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_accounts_chart_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaAccountsChartClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaAccountsChart ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaAccountsChart", &info, 0 );

	return( type );
}

static void
class_init( ofaAccountsChartClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaAccountsChartClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_init";
	ofaAccountsChart *self;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_ACCOUNTS_CHART( instance );

	self->private = g_new0( ofaAccountsChartPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_constructed";

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	/* first chain up to the parent class */
	if( G_OBJECT_CLASS( st_parent_class )->constructed ){
		G_OBJECT_CLASS( st_parent_class )->constructed( instance );
	}

	/* then setup the page
	 */
	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	setup_page_chart( OFA_ACCOUNTS_CHART( instance ));
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_dispose";
	ofaAccountsChartPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	priv = ( OFA_ACCOUNTS_CHART( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_finalize";
	ofaAccountsChart *self;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_ACCOUNTS_CHART( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_accounts_chart_run:
 *
 * When called by the main_window, the page has been created, showed
 * and activated - there is nothing left to do here....
 */
void
ofa_accounts_chart_run( ofaMainPage *this )
{
	static const gchar *thisfn = "ofa_accounts_chart_run";

	g_return_if_fail( this && OFA_IS_ACCOUNTS_CHART( this ));

	g_debug( "%s: this=%p (%s)",
			thisfn, ( void * ) this, G_OBJECT_TYPE_NAME( this ));
}

/*
 * +-----------------------------------------------------------------------+
 * | grid1 (this is the notebook page)                                     |
 * | +-----------------------------------------------+-------------------+ |
 * | | book1                                         |                   | |
 * | | each page of the book contains accounts for   |                   | |
 * | | the corresponding class (if any)              |                   | |
 * | +-----------------------------------------------+-------------------+ |
 * +-----------------------------------------------------------------------+
 */
static void
setup_page_chart( ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_setup_page_chart";
	GtkGrid *grid;
	GtkWidget *chart_book;
	GtkWidget *buttons_box;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	grid = ofa_main_page_get_grid( OFA_MAIN_PAGE( self ));

	chart_book = setup_chart_book( self );
	gtk_grid_attach( grid, chart_book, 0, 0, 1, 1 );
	self->private->chart_book = GTK_NOTEBOOK( chart_book );

	buttons_box = setup_buttons_box( self );
	gtk_grid_attach( grid, buttons_box, 1, 0, 1, 1 );

	setup_first_selection( self );
}

static GtkWidget *
setup_chart_book( ofaAccountsChart *self )
{
	GtkNotebook *chart_book;
	ofoDossier *dossier;
	GList *chart;
	gint class, prev_class;
	GList *icpt;
	GtkWidget *page;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ofoAccount *account;

	chart_book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( chart_book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( chart_book ), 4 );

	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	chart = ofo_dossier_get_accounts_chart( dossier );
	ofa_main_page_set_dataset( OFA_MAIN_PAGE( self ), chart );

	prev_class = 0;
	model = NULL;

	for( icpt=chart ; icpt ; icpt=icpt->next ){

		/* create a page per class
		 */
		class = ofo_account_get_class( OFO_ACCOUNT( icpt->data ));
		if( class != prev_class ){
			page = notebook_create_page( self, chart_book, class, -1 );
			view = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( page ), DATA_PAGE_VIEW ));
			model = gtk_tree_view_get_model( view );
			prev_class = class;
		}

		account = OFO_ACCOUNT( icpt->data );
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		store_set_account( model, &iter, account );
	}

	g_signal_connect( G_OBJECT( chart_book ), "switch-page", G_CALLBACK( on_class_page_switched ), self );

	return( GTK_WIDGET( chart_book ));
}

static GtkWidget *
notebook_create_page( ofaAccountsChart *self, GtkNotebook *book, gint class, gint position )
{
	static const gchar *thisfn = "ofa_accounts_chart_notebook_create_page";
	GtkScrolledWindow *scroll;
	GtkLabel *label;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	label = GTK_LABEL( gtk_label_new( st_class_labels[class-1] ));
	gtk_notebook_insert_page( book, GTK_WIDGET( scroll ), GTK_WIDGET( label ), position );
	gtk_notebook_set_tab_reorderable( book, GTK_WIDGET( scroll ), TRUE );
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_CLASS, GINT_TO_POINTER( class ));
	g_debug( "%s: adding page for class %d", thisfn, class );

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));
	g_object_set_data( G_OBJECT( scroll ), DATA_PAGE_VIEW, view );

	model = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Number" ),
			text_cell, "text", COL_NUMBER,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	/* doesn't work as expected
	label = GTK_LABEL( gtk_label_new( _( "Débit" )));
	gtk_widget_set_margin_right( GTK_WIDGET( label ), 12 );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_tree_view_column_set_widget( column, GTK_WIDGET( label ));*/
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 120 );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	gtk_cell_renderer_set_padding( text_cell, 12, 0 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	/*label = GTK_LABEL( gtk_label_new( _( "Débit" )));
	gtk_widget_set_margin_right( GTK_WIDGET( label ), 12 );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_tree_view_column_set_widget( column, GTK_WIDGET( label ));*/
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 120 );
	gtk_tree_view_append_column( view, column );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_account_selected ), self );

	return( GTK_WIDGET( scroll ));
}

static void
store_set_account( GtkTreeModel *model, GtkTreeIter *iter, const ofoAccount *account )
{
	gchar *sdeb, *scre;

	if( ofo_account_is_root( account )){
		sdeb = g_strdup( "" );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup_printf( "%.2f €", ofo_account_get_deb_mnt( account ));
		scre = g_strdup_printf( "%.2f €", ofo_account_get_cre_mnt( account ));
	}
	gtk_list_store_set(
			GTK_LIST_STORE( model ),
			iter,
			COL_NUMBER, ofo_account_get_number( account ),
			COL_LABEL,  ofo_account_get_label( account ),
			COL_DEBIT,  sdeb,
			COL_CREDIT, scre,
			COL_OBJECT, account,
			-1 );
	g_free( scre );
	g_free( sdeb );
}

static GtkWidget *
setup_buttons_box( ofaAccountsChart *self )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 30 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_New..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_account ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Update..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_account ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->update_btn = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Delete..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_account ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->delete_btn = button;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 25 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "View _entries..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_view_entries ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->consult_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
setup_first_selection( ofaAccountsChart *self )
{
	GtkWidget *first_tab;
	GtkTreeView *first_treeview;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	first_tab = gtk_notebook_get_nth_page( GTK_NOTEBOOK( self->private->chart_book ), 0 );
	if( first_tab ){
		first_treeview = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( first_tab ), DATA_PAGE_VIEW ));
		model = gtk_tree_view_get_model( first_treeview );
		if( gtk_tree_model_get_iter_first( model, &iter )){
			select = gtk_tree_view_get_selection( first_treeview );
			gtk_tree_selection_select_iter( select, &iter );
		}
	}
}

/*
 * as the page changes, we have to reset the current selection
 */
static void
on_class_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaAccountsChart *self )
{
	self->private->current =
			GTK_TREE_VIEW( g_object_get_data( G_OBJECT( wpage ), DATA_PAGE_VIEW ));
}

static void
on_account_selected( GtkTreeSelection *selection, ofaAccountsChart *self )
{
	gboolean select_ok;

	select_ok = gtk_tree_selection_get_selected( selection, NULL, NULL );

	gtk_widget_set_sensitive( GTK_WIDGET( self->private->update_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->delete_btn ), select_ok );
	gtk_widget_set_sensitive( GTK_WIDGET( self->private->consult_btn ), select_ok );
}

static void
on_new_account( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_new_account";
	ofoAccount *account;
	ofaMainWindow *main_window;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));

	account = ofo_account_new();
	if( ofa_account_properties_run( main_window, account )){
		insert_new_row( self, account );
	}
}

static void
on_update_account( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_update_account";
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ofoAccount *account;
	gchar *prev_number;
	const gchar *new_number;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &account, -1 );
		g_object_unref( account );

		prev_number = g_strdup( ofo_account_get_number( account ));

		if( ofa_account_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), account )){

			new_number = ofo_account_get_number( account );

			if( g_utf8_collate( prev_number, new_number )){
				gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
				insert_new_row( self, account );

			} else {
				store_set_account( model, &iter, account );
			}
		}

		g_free( prev_number );
	}
}

/*
 * un compte peut être supprimé si ses soldes sont nuls, et après
 * confirmation de l'utilisateur
 */
static void
on_delete_account( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_delete_account";
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ofoAccount *account;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	select = gtk_tree_view_get_selection( self->private->current );
	if( gtk_tree_selection_get_selected( select, &model, &iter )){

		gtk_tree_model_get( model, &iter, COL_OBJECT, &account, -1 );
		g_object_unref( account );

		if( ofo_account_get_deb_mnt( account ) ||
				ofo_account_get_cre_mnt( account ) ||
				ofo_account_get_bro_deb_mnt( account ) ||
				ofo_account_get_bro_cre_mnt( account )) {

			error_undeletable( self, account );
			return;
		}

		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

		if( delete_confirmed( self, account ) &&
				ofo_dossier_delete_account( dossier, account )){

			/* update our chart of accounts */
			ofa_main_page_set_dataset(
					OFA_MAIN_PAGE( self ), ofo_dossier_get_accounts_chart( dossier ));

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
		}
	}
}

static void
error_undeletable( ofaAccountsChart *self, ofoAccount *account )
{
	GtkMessageDialog *dlg;
	gchar *msg;
	ofaMainWindow *main_window;

	msg = g_strdup_printf(
				_( "We are unable to remove the '%s - %s' account "
					"as at least one of its amounts is not nul" ),
				ofo_account_get_number( account ),
				ofo_account_get_label( account ));

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( main_window ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
	g_free( msg );
}

static gboolean
delete_confirmed( ofaAccountsChart *self, ofoAccount *account )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' account ?" ),
			ofo_account_get_number( account ),
			ofo_account_get_label( account ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_view_entries";

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );
}

static void
insert_new_row( ofaAccountsChart *self, ofoAccount *account )
{
	gint page_num;
	gint count, i;
	gint account_class, page_class;
	GtkWidget *page, *page_found;
	GtkTreeModel *model;
	GtkTreeIter iter, new_iter;
	GValue value = G_VALUE_INIT;
	const gchar *num, *account_num;
	GtkTreeView *view;
	gboolean iter_found;
	GtkTreePath *path;
	ofoDossier *dossier;

	/* update our chart of accounts */
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	ofa_main_page_set_dataset(
			OFA_MAIN_PAGE( self ), ofo_dossier_get_accounts_chart( dossier ));

	/* activate the page of the correct class, or create a new one */
	account_class = ofo_account_get_class( account );
	count = gtk_notebook_get_n_pages( self->private->chart_book );
	page_found = NULL;
	for( i=0 ; i<count ; ++i ){
		page = gtk_notebook_get_nth_page( self->private->chart_book, i );
		page_class = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( page ), DATA_PAGE_CLASS ));
		if( page_class == account_class ){
			page_found = page;
			break;
		}
	}
	if( !page_found ){
		page_found = notebook_create_page(
				self, self->private->chart_book, account_class, account_class-1 );
	}
	gtk_widget_show_all( page_found );
	page_num = gtk_notebook_page_num( self->private->chart_book, page_found );
	gtk_notebook_set_current_page( self->private->chart_book, page_num );

	/* insert the new row at the right place */
	view = GTK_TREE_VIEW( g_object_get_data( G_OBJECT( page_found ), DATA_PAGE_VIEW ));
	model = gtk_tree_view_get_model( view );
	iter_found = FALSE;
	if( gtk_tree_model_get_iter_first( model, &iter )){
		account_num = ofo_account_get_number( account );
		while( !iter_found ){
			gtk_tree_model_get_value( model, &iter, COL_NUMBER, &value );
			num = g_value_get_string( &value );
			if( g_utf8_collate( num, account_num ) > 0 ){
				iter_found = TRUE;
				break;
			}
			if( !gtk_tree_model_iter_next( model, &iter )){
				break;
			}
			g_value_unset( &value );
		}
	}
	if( !iter_found ){
		gtk_list_store_append( GTK_LIST_STORE( model ), &new_iter );
	} else {
		gtk_list_store_insert_before( GTK_LIST_STORE( model ), &new_iter, &iter );
	}
	store_set_account( model, &new_iter, account );

	/* select the newly added account */
	path = gtk_tree_model_get_path( model, &new_iter );
	gtk_tree_view_set_cursor( view, path, NULL, FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( view ));
	gtk_tree_path_free( path );
}
