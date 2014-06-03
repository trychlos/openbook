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

#include "ui/my-utils.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-importer.h"
#include "ui/ofa-plugin.h"
#include "ui/ofa-rappro.h"
#include "ui/ofo-account.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"

/* private instance data
 */
struct _ofaRapproPrivate {
	gboolean      dispose_has_run;

	/* internals
	 */
	GtkEntry     *account;
	GtkComboBox  *mode;
	GtkButton    *fetch;
	GtkEntry     *concil;
	GtkTreeModel *tmodel;
	GtkEntry     *debit;
	GtkEntry     *credit;
};

/* origin of a row in the treeview:
 *  from the entries table, or from an imported bank account transaction list
 */
enum {
	FROM_ENTRIES = 1,
	FROM_BAT
};

/* column ordering in the main entries listview
 */
enum {
	COL_FROM,				/* from the entries or from a bank account transaction */
	COL_DOPE,
	COL_PIECE,
	COL_NUMBER,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_RAPPRO,
	COL_OBJECT,
	N_COLUMNS
};

/* columns in the combo box which let us select which type of entries
 * are displayed
 */
enum {
	ENT_COL_CODE = 0,
	ENT_COL_LABEL,
	ENT_N_COLUMNS
};

typedef struct {
	gint         code;
	const gchar *label;
}
	sConcil;

static const sConcil st_concils[] = {
		{ ENT_CONCILED_YES, N_( "Reconciliated" ) },
		{ ENT_CONCILED_NO,  N_( "Not reconciliated" ) },
		{ ENT_CONCILED_ALL, N_( "All" ) },
		{ 0 }
};

G_DEFINE_TYPE( ofaRappro, ofa_rappro, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *setup_select( ofaMainPage *page );
static GtkWidget *setup_rappro( ofaMainPage *page );
static GtkWidget *setup_treeview( ofaMainPage *page );
static GtkWidget *setup_soldes( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRappro *self );
static gboolean   is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRappro *self );
static gboolean   on_account_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRappro *self );
static void       on_account_changed( GtkEntry *entry, ofaRappro *self );
static void       on_account_button_clicked( GtkButton *button, ofaRappro *self );
static void       do_account_selection( ofaRappro *self );
static void       on_combo_mode_changed( GtkComboBox *box, ofaRappro *self );
static gint       get_selected_concil_mode( ofaRappro *self );
static void       check_for_enable_fetch( ofaRappro *self );
static gboolean   is_fetch_enableable( ofaRappro *self, ofoAccount **account, gint *mode );
static void       on_fetch_button_clicked( GtkButton *button, ofaRappro *self );
static void       do_fetch( ofaRappro *self );
static void       on_file_set( GtkFileChooserButton *button, ofaRappro *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       toggle_rappro( ofaRappro *self, GtkTreeView *tview, GtkTreePath *path );
static void       on_row_selected( GtkTreeSelection *selection, ofaRappro *self );
static void       reset_soldes( ofaRappro *self );

static void
rappro_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rappro_finalize";
	ofaRapproPrivate *priv;

	g_return_if_fail( OFA_IS_RAPPRO( instance ));

	priv = OFA_RAPPRO( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rappro_parent_class )->finalize( instance );
}

static void
rappro_dispose( GObject *instance )
{
	ofaRapproPrivate *priv;

	g_return_if_fail( OFA_IS_RAPPRO( instance ));

	priv = ( OFA_RAPPRO( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rappro_parent_class )->dispose( instance );
}

static void
ofa_rappro_init( ofaRappro *self )
{
	static const gchar *thisfn = "ofa_rappro_init";

	g_return_if_fail( OFA_IS_RAPPRO( self ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaRapproPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_rappro_class_init( ofaRapproClass *klass )
{
	static const gchar *thisfn = "ofa_rappro_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rappro_dispose;
	G_OBJECT_CLASS( klass )->finalize = rappro_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	GtkFrame *frame;
	GtkGrid *grid;
	GtkWidget *select;
	GtkWidget *rappro;
	GtkWidget *tview;
	GtkWidget *soldes;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	/*gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_right( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );*/

	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( grid ));

	select = setup_select( page );
	gtk_grid_attach( grid, select, 0, 0, 1, 1 );

	rappro = setup_rappro( page );
	gtk_grid_attach( grid, rappro, 1, 0, 1, 1 );

	tview = setup_treeview( page );
	gtk_grid_attach( grid, tview, 0, 1, 2, 1 );

	soldes = setup_soldes( page );
	gtk_grid_attach( grid, soldes, 0, 2, 2, 1 );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_select( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkFrame *frame;
	GtkAlignment *alignment;
	GtkGrid *grid, *grid2;
	gchar *markup;
	GtkLabel *label;
	GtkWidget *image;
	GtkButton *button;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i;

	priv = OFA_RAPPRO( page )->private;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Selection" ));
	gtk_label_set_markup( label, markup );
	gtk_frame_set_label_widget( frame, GTK_WIDGET( label ));
	g_free( markup );

	alignment = GTK_ALIGNMENT( gtk_alignment_new( 0.5, 0.5, 1.0, 1.0 ));
	gtk_alignment_set_padding( alignment, 4, 4, 12, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( alignment ));

	grid = GTK_GRID( gtk_grid_new());
	gtk_widget_set_hexpand( GTK_WIDGET( grid ), TRUE );
	gtk_grid_set_column_spacing( grid, 8 );
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( grid ));

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "_Account :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	grid2 = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid2, 2 );
	gtk_grid_attach( grid, GTK_WIDGET( grid2 ), 1, 0, 1, 1 );

	priv->account = GTK_ENTRY( gtk_entry_new());
	gtk_entry_set_max_length( priv->account, 20 );
	gtk_entry_set_width_chars( priv->account, 10 );
	gtk_label_set_mnemonic_widget( label, GTK_WIDGET( priv->account ));
	gtk_grid_attach( grid2, GTK_WIDGET( priv->account ), 0, 0, 1, 1 );
	g_signal_connect(
			G_OBJECT( priv->account ),
			"key-press-event", G_CALLBACK( on_account_key_pressed ), page );
	g_signal_connect(
			G_OBJECT( priv->account ),
			"changed", G_CALLBACK( on_account_changed ), page );

	image = gtk_image_new_from_stock( GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	gtk_button_set_image( button, image );
	gtk_grid_attach( grid2, GTK_WIDGET( button ), 1, 0, 1, 1 );
	g_signal_connect(
			G_OBJECT( button ),
			"clicked", G_CALLBACK( on_account_button_clicked ), page );

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "_Entries :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 2, 0, 1, 1 );

	priv->mode = GTK_COMBO_BOX( gtk_combo_box_new());
	gtk_label_set_mnemonic_widget( label, GTK_WIDGET( priv->mode ));
	gtk_grid_attach( grid, GTK_WIDGET( priv->mode ), 3, 0, 1, 1 );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new( ENT_N_COLUMNS, G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->mode, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->mode ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->mode ), cell, "text", ENT_COL_LABEL );

	for( i=0 ; st_concils[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				ENT_COL_CODE,  st_concils[i].code,
				ENT_COL_LABEL, gettext( st_concils[i].label ),
				-1 );
	}

	g_signal_connect(
			G_OBJECT( priv->mode ),
			"changed", G_CALLBACK( on_combo_mode_changed ), page );

	image = gtk_image_new_from_stock( GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON );
	priv->fetch = GTK_BUTTON( gtk_button_new());
	gtk_button_set_image( priv->fetch, image );
	gtk_grid_attach( grid, GTK_WIDGET( priv->fetch ), 4, 0, 1, 1 );
	g_signal_connect(
			G_OBJECT( priv->fetch ),
			"clicked", G_CALLBACK( on_fetch_button_clicked ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_rappro( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkFrame *frame;
	GtkAlignment *alignment;
	GtkGrid *grid;
	GtkLabel *label;
	gchar *markup;
	GtkWidget *button;

	priv = OFA_RAPPRO( page )->private;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_right( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Reconciliation" ));
	gtk_label_set_markup( label, markup );
	gtk_frame_set_label_widget( frame, GTK_WIDGET( label ));
	g_free( markup );

	alignment = GTK_ALIGNMENT( gtk_alignment_new( 0.5, 0.5, 1.0, 1.0 ));
	gtk_alignment_set_padding( alignment, 4, 4, 12, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( alignment ));

	grid = GTK_GRID( gtk_grid_new());
	gtk_widget_set_hexpand( GTK_WIDGET( grid ), TRUE );
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( grid ));

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "Re_conciliation date :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	priv->concil = GTK_ENTRY( gtk_entry_new());
	gtk_entry_set_max_length( priv->concil, 10 );
	gtk_entry_set_width_chars( priv->concil, 10 );
	gtk_label_set_mnemonic_widget( label, GTK_WIDGET( priv->concil ));
	gtk_grid_attach( grid, GTK_WIDGET( priv->concil ), 1, 0, 1, 1 );

	button = gtk_file_chooser_button_new( _( "Browse "), GTK_FILE_CHOOSER_ACTION_OPEN );
	gtk_grid_attach( grid, GTK_WIDGET( button ), 2, 0, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "file-set", G_CALLBACK( on_file_set ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_treeview( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = OFA_RAPPRO( page )->private;

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_INT,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
			G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_OBJECT ));
	priv->tmodel = gtk_tree_model_filter_new( tmodel, NULL );
	g_object_unref( tmodel );
	gtk_tree_view_set_model( tview, priv->tmodel );
	g_object_unref( priv->tmodel );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( priv->tmodel ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			page,
			NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ope." ),
			text_cell, "text", COL_DOPE,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", COL_PIECE,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_column_set_title( column, _( "Concil." ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_RAPPRO );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), page );

	/* be sure that this is the underlying (child) tree model which is
	 * sorted, and not the tree model filter (which only takes care of
	 * making some row visible or not)
	 */
	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( scroll ));
}

static GtkWidget *
setup_soldes( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkBox *box;
	GtkLabel *label;
	GtkFrame *frame;

	priv = OFA_RAPPRO( page )->private;

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));
	gtk_widget_set_margin_bottom( GTK_WIDGET( box ), 2 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), 80, -1 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_end( box, GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	priv->credit = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( priv->credit ), FALSE );
	gtk_entry_set_width_chars( priv->credit, 10 );
	gtk_entry_set_alignment( priv->credit, 1.0 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->credit ), FALSE, FALSE, 0 );

	priv->debit = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( priv->debit ), FALSE );
	gtk_entry_set_width_chars( priv->debit, 10 );
	gtk_entry_set_alignment( priv->debit, 1.0 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->debit ), FALSE, FALSE, 0 );

	label = GTK_LABEL( gtk_label_new( _( "Totals :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), TRUE, TRUE, 0 );

	return( GTK_WIDGET( box ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaMainPage *page )
{
	check_for_enable_fetch( OFA_RAPPRO( page ));
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRappro *self )
{
	gchar *dopea, *dopeb, *sqla, *sqlb;
	GDate da, db;
	gint numa, numb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_DOPE, &dopea, COL_NUMBER, &numa, -1 );
	memcpy( &da, my_utils_date_from_str( dopea ), sizeof( GDate ));
	sqla = my_utils_sql_from_date( &da );

	gtk_tree_model_get( tmodel, b, COL_DOPE, &dopeb, COL_NUMBER, &numb, -1 );
	memcpy( &db, my_utils_date_from_str( dopeb ), sizeof( GDate ));
	sqlb = my_utils_sql_from_date( &db );

	cmp = g_utf8_collate( sqla, sqlb );
	if( cmp == 0 ){
		cmp = ( numa < numb ? -1 : ( numa > numb ? 1 : 0 ));
	}

	g_free( dopea );
	g_free( sqla );
	g_free( dopeb );
	g_free( sqlb );

	return( cmp );
}

static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRappro *self )
{
	gboolean visible;
	gchar *srappro;
	GDate date;
	gint mode;

	gtk_tree_model_get( tmodel, iter, COL_RAPPRO, &srappro, -1 );
	memcpy( &date, my_utils_date_from_str( srappro ), sizeof( GDate ));

	mode = get_selected_concil_mode( self );

	visible = ( g_date_valid( &date ) && ( mode == ENT_CONCILED_YES || mode == ENT_CONCILED_ALL )) ||
				( !g_date_valid( &date ) && ( mode == ENT_CONCILED_NO || mode == ENT_CONCILED_ALL ));

	return( visible );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_account_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRappro *self )
{
	return( FALSE );
}

static void
on_account_changed( GtkEntry *entry, ofaRappro *self )
{
	check_for_enable_fetch( self );
}

static void
on_account_button_clicked( GtkButton *button, ofaRappro *self )
{
	do_account_selection( self );
}

static void
do_account_selection( ofaRappro *self )
{
	gchar *number;

	number = ofa_account_select_run(
			ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
			gtk_entry_get_text( self->private->account ));

	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( self->private->account, number );
	}

	g_free( number );
}

static void
on_combo_mode_changed( GtkComboBox *box, ofaRappro *self )
{
	check_for_enable_fetch( self );
}

static gint
get_selected_concil_mode( ofaRappro *self )
{
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gint mode;

	mode = -1;

	if( gtk_combo_box_get_active_iter( self->private->mode, &iter )){
		tmodel = gtk_combo_box_get_model( self->private->mode );
		gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &mode, -1 );
	}

	return( mode );
}

static void
check_for_enable_fetch( ofaRappro *self )
{
	gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->fetch ),
			is_fetch_enableable( self, NULL, NULL ));
}

static gboolean
is_fetch_enableable( ofaRappro *self, ofoAccount **account, gint *mode )
{
	gboolean enableable;
	const gchar *acc_number;
	ofoAccount *my_account;
	gint my_mode;

	my_mode = -1;
	acc_number = gtk_entry_get_text( self->private->account );
	my_account = ofo_account_get_by_number(
						ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )), acc_number );
	enableable = my_account && OFO_IS_ACCOUNT( my_account );

	if( enableable ){
		my_mode = get_selected_concil_mode( self );
		enableable &= ( my_mode >= 1 );
	}

	if( account ){
		*account = my_account;
	}
	if( mode ){
		*mode = my_mode;
	}

	return( enableable );
}

static void
on_fetch_button_clicked( GtkButton *button, ofaRappro *self )
{
	do_fetch( self );
	reset_soldes( self );
}

static void
do_fetch( ofaRappro *self )
{
	static  const gchar *thisfn = "ofa_rappro_do_fetch";
	ofoAccount *account;
	gint mode;
	GList *entries, *it;
	ofoEntry *entry;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *dope, *sdeb, *scre, *drap;
	ofaEntrySens sens;

	is_fetch_enableable( self, &account, &mode );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
	g_return_if_fail( mode >= 1 );

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	entries = ofo_entry_get_dataset(
					ofa_main_page_get_dossier(
							OFA_MAIN_PAGE( self )), ofo_account_get_number( account ), mode );

	for( it=entries ; it ; it=it->next ){

		entry = OFO_ENTRY( it->data );

		dope = my_utils_display_from_date( ofo_entry_get_dope( entry ), MY_UTILS_DATE_DDMM );
		sens = ofo_entry_get_sens( entry );
		switch( sens ){
			case ENT_SENS_DEBIT:
				sdeb = g_strdup_printf( "%.2lf", ofo_entry_get_amount( entry ));
				scre = g_strdup( "" );
				break;
			case ENT_SENS_CREDIT:
				sdeb = g_strdup( "" );
				scre = g_strdup_printf( "%.2lf", ofo_entry_get_amount( entry ));
				break;
			default:
				g_warning( "%s: invalid entry sens: %d", thisfn, ofo_entry_get_sens( entry ));
				sdeb = g_strdup( "" );
				scre = g_strdup( "" );
				break;
		}
		drap = my_utils_display_from_date( ofo_entry_get_rappro( entry ), MY_UTILS_DATE_DDMM );

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_FROM,   FROM_ENTRIES,
				COL_DOPE,   dope,
				COL_PIECE,  ofo_entry_get_ref( entry ),
				COL_LABEL,  ofo_entry_get_label( entry ),
				COL_DEBIT,  sdeb,
				COL_CREDIT, scre,
				COL_RAPPRO, drap,
				COL_OBJECT, entry,
				-1 );
	}

	ofo_entry_free_dataset( entries );
}

/*
 * try to import a bank account transaction list
 */
static void
on_file_set( GtkFileChooserButton *button, ofaRappro *self )
{
	gint bat_id;

	bat_id = ofa_importer_import_from_uri(
					ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )),
					gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( button )));

	g_debug( "bat_id=%d", bat_id );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page )
{
	toggle_rappro( OFA_RAPPRO( page ), view, path );
	reset_soldes( OFA_RAPPRO( page ));
}

static void
toggle_rappro( ofaRappro *self, GtkTreeView *tview, GtkTreePath *path )
{
	gchar *srappro;
	ofoEntry *entry;
	const gchar *concil;
	GDate date;
	GtkTreeModel *tmodel;
	GtkTreeIter iter, child;
	gboolean updated;

	updated = FALSE;

	if( gtk_tree_model_get_iter( self->private->tmodel, &iter, path )){
		gtk_tree_model_get(
				self->private->tmodel,
				&iter,
				COL_RAPPRO, &srappro,
				COL_OBJECT, &entry,
				-1 );
		g_object_unref( entry );
		memcpy( &date, my_utils_date_from_str( srappro ), sizeof( GDate ));
		g_free( srappro );

		/* reconciliation is already set up, so clears it */
		if( g_date_valid( &date )){
			ofo_entry_set_rappro( entry, NULL );
			srappro = g_strdup( "" );
			updated = TRUE;

		/* reconciliation is not set yet, so set it if proposed date is
		 * valid */
		} else {
			concil = gtk_entry_get_text( self->private->concil );
			memcpy( &date, my_utils_date_from_str( concil ), sizeof( GDate ));
			if( g_date_valid( &date )){
				ofo_entry_set_rappro( entry, &date );
				srappro = my_utils_display_from_date( &date, MY_UTILS_DATE_DDMM );
				updated = TRUE;
			}
		}
	}

	if( updated ){
		tmodel = gtk_tree_model_filter_get_model(
						GTK_TREE_MODEL_FILTER( self->private->tmodel ));
		gtk_tree_model_filter_convert_iter_to_child_iter(
						GTK_TREE_MODEL_FILTER( self->private->tmodel ), &child, &iter );
		gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				&child,
				COL_RAPPRO, srappro,
				-1 );
		g_free( srappro );
		ofo_entry_update_rappro( entry, ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )));
	}
}

static void
on_row_selected( GtkTreeSelection *selection, ofaRappro *self )
{
}

static void
reset_soldes( ofaRappro *self )
{
	gdouble debit, credit;
	GtkTreeIter iter;
	ofoEntry *entry;
	gchar *sdeb, *scre;

	debit = credit = 0.0;

	if( gtk_tree_model_get_iter_first( self->private->tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( self->private->tmodel, &iter, COL_OBJECT, &entry, -1 );
			g_object_unref( entry );
			if( ofo_entry_get_sens( entry ) == ENT_SENS_DEBIT ){
				debit += ofo_entry_get_amount( entry );
			} else {
				credit += ofo_entry_get_amount( entry );
			}
			if( !gtk_tree_model_iter_next( self->private->tmodel, &iter )){
				break;
			}
		}
	}

	sdeb = g_strdup_printf( "%.2lf", debit );
	scre = g_strdup_printf( "%.2lf", credit );

	gtk_entry_set_text( self->private->debit, sdeb );
	gtk_entry_set_text( self->private->credit, scre );

	g_free( sdeb );
	g_free( scre );
}
