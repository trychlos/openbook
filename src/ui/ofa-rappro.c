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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-iimporter.h"
#include "api/ofo-account.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-plugin.h"

#include "ui/ofa-main-page.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-importer.h"
#include "ui/ofa-rappro.h"

/* private instance data
 */
struct _ofaRapproPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkEntry     *account;
	GtkLabel     *account_label;
	GtkLabel     *account_debit;
	GtkLabel     *account_credit;
	GtkComboBox  *mode;
	GtkButton    *clear;
	GtkEntry     *date_concil;
	GtkTreeModel *tmodel;				/* GtkTreeModelFilter of the tree view */
	GtkLabel     *bal_debit;			/* balance of the account  */
	GtkLabel     *bal_credit;			/*  ... deducting unreconciliated entries */

	/* internals
	 */
	GDate         dconcil;
	GList        *batlines;				/* loaded bank account transaction lines */
};

/* column ordering in the main entries listview
 */
enum {
	COL_DOPE,
	COL_PIECE,
	COL_NUMBER,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_RAPPRO,
	COL_VALID,				/* whether the reconciliation date has been validated */
	COL_OBJECT,				/* may be an ofoEntry or an ofoBatLine */
	N_COLUMNS
};

/* set against the COL_RAPPRO column to be used in on_cell_data_func() */
#define DATA_COLUMN_ID      "ofa-data-column-id"

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

static GtkWidget   *v_setup_view( ofaMainPage *page );
static GtkWidget   *setup_select_account( ofaMainPage *page );
static GtkWidget   *setup_manual_rappro( ofaMainPage *page );
static GtkWidget   *setup_auto_rappro( ofaMainPage *page );
static GtkWidget   *setup_display_account( ofaMainPage *page );
static GtkWidget   *setup_treeview( ofaMainPage *page );
static GtkWidget   *setup_balance( ofaMainPage *page );
static GtkWidget   *v_setup_buttons( ofaMainPage *page );
static void         v_init_view( ofaMainPage *page );
static gint         on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRappro *self );
static gboolean     is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRappro *self );
static gboolean     is_visible_entry( ofaRappro *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static gboolean     is_visible_batline( ofaRappro *self, ofoBatLine *batline );
static void         on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRappro *self );
static void         on_account_changed( GtkEntry *entry, ofaRappro *self );
static void         on_account_button_clicked( GtkButton *button, ofaRappro *self );
static void         do_account_selection( ofaRappro *self );
static void         on_combo_mode_changed( GtkComboBox *box, ofaRappro *self );
static gint         get_selected_concil_mode( ofaRappro *self );
static void         check_for_enable_fetch( ofaRappro *self );
static gboolean     is_fetch_enableable( ofaRappro *self, ofoAccount **account, gint *mode );
static void         on_fetch_button_clicked( GtkButton *button, ofaRappro *self );
static void         do_fetch( ofaRappro *self );
static void         on_select_bat( GtkButton *button, ofaRappro *self );
static void         on_file_set( GtkFileChooserButton *button, ofaRappro *self );
static void         on_clear_button_clicked( GtkButton *button, ofaRappro *self );
static void         setup_bat_lines( ofaRappro *self, gint bat_id );
static void         clear_bat_lines( ofaRappro *self );
static void         display_bat_lines( ofaRappro *self );
static GtkTreeIter *search_for_entry_by_number( ofaRappro *self, gint number );
static GtkTreeIter *search_for_entry_by_montant( ofaRappro *self, const gchar *sbat_deb, const gchar *sbat_cre );
static void         update_candidate_entry( ofaRappro *self, ofoBatLine *batline, GtkTreeIter *entry_iter );
static void         insert_bat_line( ofaRappro *self, ofoBatLine *batline, GtkTreeIter *entry_iter, const gchar *sdeb, const gchar *scre );
static gboolean     on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRappro *self );
static void         collapse_node( ofaRappro *self, GtkWidget *widget );
static void         expand_node( ofaRappro *self, GtkWidget *widget );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static gboolean     toggle_rappro( ofaRappro *self, GtkTreeView *tview, GtkTreePath *path );
static void         reconciliate_entry( ofaRappro *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *iter );
static void         set_reconciliated_balance( ofaRappro *self );

static void
rappro_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rappro_finalize";
	ofaRapproPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_RAPPRO( instance ));

	priv = OFA_RAPPRO( instance )->private;

	/* free data members here */
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
		g_list_free_full( priv->batlines, ( GDestroyNotify ) g_object_unref );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rappro_parent_class )->dispose( instance );
}

static void
ofa_rappro_init( ofaRappro *self )
{
	static const gchar *thisfn = "ofa_rappro_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_RAPPRO( self ));

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
	GtkWidget *account;
	GtkWidget *rappro;
	GtkWidget *tview;
	GtkWidget *soldes;

	frame = GTK_FRAME( gtk_frame_new( NULL ));

	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( grid ));

	account = setup_select_account( page );
	gtk_grid_attach( grid, account, 0, 0, 1, 1 );

	/* manual reconciliation (enter a date) */
	rappro = setup_manual_rappro( page );
	gtk_grid_attach( grid, rappro, 1, 0, 1, 1 );

	/* auto reconciliation from imported BAT file */
	rappro = setup_auto_rappro( page );
	gtk_grid_attach( grid, rappro, 2, 0, 1, 1 );

	account = setup_display_account( page );
	gtk_grid_attach( grid, account, 0, 1, 3, 1 );

	tview = setup_treeview( page );
	gtk_grid_attach( grid, tview, 0, 2, 3, 1 );

	soldes = setup_balance( page );
	gtk_grid_attach( grid, soldes, 0, 3, 3, 1 );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_select_account( ofaMainPage *page )
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
	gtk_alignment_set_padding( alignment, 4, 4, 8, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( alignment ));

	grid = GTK_GRID( gtk_grid_new());
	/*gtk_widget_set_hexpand( GTK_WIDGET( grid ), TRUE );*/
	gtk_grid_set_column_spacing( grid, 6 );
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
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->account ),
			_( "Enter here the number of the account to be reconciliated" ));
	g_signal_connect(
			G_OBJECT( priv->account ),
			"changed", G_CALLBACK( on_account_changed ), page );

	image = gtk_image_new_from_stock( GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	gtk_button_set_image( button, image );
	gtk_grid_attach( grid2, GTK_WIDGET( button ), 1, 0, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( button ),
			_( "Select the account to be reconciliated" ));
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

	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->mode ),
			_( "Select the type of entries to be displayed" ));
	g_signal_connect(
			G_OBJECT( priv->mode ),
			"changed", G_CALLBACK( on_combo_mode_changed ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_manual_rappro( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkFrame *frame;
	GtkAlignment *alignment;
	GtkGrid *grid;
	GtkLabel *label;
	gchar *markup;
	myDateParse parms;

	priv = OFA_RAPPRO( page )->private;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Manual reconciliation" ));
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

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "Da_te :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	memset( &parms, '\0', sizeof( parms ));
	parms.entry = gtk_entry_new();
	parms.entry_format = MY_DATE_DDMM;
	parms.label = gtk_label_new( "" 	);
	parms.label_format = MY_DATE_DMMM;
	parms.date = &priv->dconcil;
	my_date_parse_from_entry( &parms );

	priv->date_concil = GTK_ENTRY( parms.entry );
	gtk_entry_set_max_length( priv->date_concil, 10 );
	gtk_entry_set_width_chars( priv->date_concil, 10 );
	gtk_label_set_mnemonic_widget( label, GTK_WIDGET( priv->date_concil ));
	gtk_grid_attach( grid, GTK_WIDGET( priv->date_concil ), 1, 0, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->date_concil ),
			_( "The date to which the entry will be set as reconciliated if no account transaction is proposed" ));

	gtk_misc_set_alignment( GTK_MISC( parms.label ), 0, 0.5 );
	gtk_label_set_width_chars( GTK_LABEL( parms.label ), 10 );
	gtk_grid_attach( grid, parms.label, 2, 0, 1, 1 );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_auto_rappro( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkFrame *frame;
	GtkAlignment *alignment;
	GtkGrid *grid;
	GtkLabel *label;
	gchar *markup;
	GtkWidget *button, *image;

	priv = OFA_RAPPRO( page )->private;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_right( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Automatic reconciliation" ));
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

	button = gtk_button_new_with_mnemonic( _( "_Select..." ));
	gtk_grid_attach( grid, GTK_WIDGET( button ), 1, 1, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_select_bat ), page );
	gtk_widget_set_tooltip_text(
			button,
			_( "Select a previously imported account transactions list" ));

	button = gtk_file_chooser_button_new( NULL, GTK_FILE_CHOOSER_ACTION_OPEN );
	gtk_grid_attach( grid, GTK_WIDGET( button ), 2, 1, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "file-set", G_CALLBACK( on_file_set ), page );
	gtk_widget_set_tooltip_text(
			button,
			_( "Import an account transactions list to be used in the reconciliation" ));

	image = gtk_image_new_from_stock( GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON );
	priv->clear = GTK_BUTTON( gtk_button_new());
	gtk_button_set_image( priv->clear, image );
	gtk_grid_attach( grid, GTK_WIDGET( priv->clear ), 3, 1, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->account ),
			_( "Clear the displayed account transaction lines" ));
	g_signal_connect(
			G_OBJECT( priv->clear ),
			"clicked", G_CALLBACK( on_clear_button_clicked ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_display_account( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkBox *box;
	GtkLabel *label;

	priv = OFA_RAPPRO( page )->private;

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_label_set_width_chars( label, 13 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), FALSE, FALSE, 0 );

	priv->account_credit = GTK_LABEL( gtk_label_new( "" ));
	gtk_misc_set_alignment( GTK_MISC( priv->account_credit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->account_credit, 11 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->account_credit ), FALSE, FALSE, 0 );

	priv->account_debit = GTK_LABEL( gtk_label_new( "" ));
	gtk_misc_set_alignment( GTK_MISC( priv->account_debit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->account_debit, 11 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->account_debit ), FALSE, FALSE, 0 );

	label = GTK_LABEL( gtk_label_new( _( "Account balance :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), FALSE, FALSE, 0 );

	priv->account_label = GTK_LABEL( gtk_label_new( "" ));
	gtk_misc_set_alignment( GTK_MISC( priv->account_label ), 0, 0.5 );
	gtk_label_set_ellipsize( priv->account_label, PANGO_ELLIPSIZE_END );
	gtk_box_pack_end( box, GTK_WIDGET( priv->account_label ), TRUE, TRUE, 0 );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_label_set_width_chars( label, 1 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), FALSE, FALSE, 0 );

	return( GTK_WIDGET( box ));
}

/*
 * The treeview displays both entries and bank account transaction (bat)
 * lines. It is based on a filtered sorted tree store.
 *
 * Entries are 'parent' row. If a bat line is a good candidate to a
 * reconciliation, then it will be displayed as a child of the entry.
 * An entry has zero or one child, never more.
 */
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
	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_key_pressed ), page );

	tmodel = GTK_TREE_MODEL( gtk_tree_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
			G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_BOOLEAN,
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
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", COL_PIECE,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( COL_RAPPRO ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_column_set_title( column, _( "Reconcil." ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", COL_RAPPRO );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );

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

/*
 * two widgets (debit/credit) display the 'theoric' balance of the
 * account, by deducting the unreconciliated entries from the balance
 * in our book - this is supposed simulate the actual bank balance
 */
static GtkWidget *
setup_balance( ofaMainPage *page )
{
	ofaRapproPrivate *priv;
	GtkBox *box;
	GtkLabel *label;

	priv = OFA_RAPPRO( page )->private;

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));
	/*gtk_widget_set_margin_bottom( GTK_WIDGET( box ), 2 );*/

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_label_set_width_chars( label, 13 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), FALSE, FALSE, 0 );

	priv->bal_credit = GTK_LABEL( gtk_label_new( "" ));
	gtk_misc_set_alignment( GTK_MISC( priv->bal_credit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->bal_credit, 11 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->bal_credit ), FALSE, FALSE, 0 );

	priv->bal_debit = GTK_LABEL( gtk_label_new( "" ));
	gtk_misc_set_alignment( GTK_MISC( priv->bal_debit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->bal_debit, 11 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->bal_debit ), FALSE, FALSE, 0 );

	label = GTK_LABEL( gtk_label_new( _( "Reconciliated balance :" )));
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

/*
 * sort the visible rows (entries as parent, and bat lines as children)
 * by operation date + entry number (entries only)
 *
 * for bat lines, operation date may be set to effect date (valeur) if
 * not provided in the bat file; entry number is set to zero
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRappro *self )
{
	gchar *dopea, *dopeb;
	GDate da, db;
	gint numa, numb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_DOPE, &dopea, COL_NUMBER, &numa, -1 );
	my_date_parse_from_str( &da, dopea, MY_DATE_DDMM );

	gtk_tree_model_get( tmodel, b, COL_DOPE, &dopeb, COL_NUMBER, &numb, -1 );
	my_date_parse_from_str( &db, dopeb, MY_DATE_DDMM );

	cmp = my_date_cmp( &da, &db, FALSE );
	if( cmp == 0 ){
		cmp = ( numa < numb ? -1 : ( numa > numb ? 1 : 0 ));
	}

	g_free( dopea );
	g_free( dopeb );

	return( cmp );
}

/*
 * a row is visible if it is consistant with the selected mode:
 * - entry: vs. the selected mode
 * - bat line: vs. the reconciliation status:
 *   > reconciliated (and validated): invisible
 *   > not reconciliated (or not validated): visible
 *
 * tmodel here is the main GtkTreeModelFilter
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRappro *self )
{
	gboolean visible;
	GObject *object;

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );
	g_object_unref( object );
	g_return_val_if_fail( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object ), TRUE );

	visible = OFO_IS_ENTRY( object ) ?
			is_visible_entry( self, tmodel, iter, OFO_ENTRY( object )) :
			is_visible_batline( self, OFO_BAT_LINE( object ));

	return( visible );
}

static gboolean
is_visible_entry( ofaRappro *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	static const gchar *thisfn = "ofa_rappro_is_visible_entry";
	gboolean visible;
	gboolean validated;
	gint mode;

	gtk_tree_model_get( tmodel, iter, COL_VALID, &validated, -1 );

	mode = get_selected_concil_mode( self );
	visible = TRUE;

	switch( mode ){
		case ENT_CONCILED_ALL:
			/*g_debug( "%s: mode=%d, visible=True", thisfn, mode );*/
			break;
		case ENT_CONCILED_YES:
			visible = validated;
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_NO:
			visible = !validated;
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		default:
			g_warning( "%s: invalid display mode: %d", thisfn, mode );
	}

	return( visible );
}

static gboolean
is_visible_batline( ofaRappro *self, ofoBatLine *batline )
{
	gint ecr_number;

	ecr_number = ofo_bat_line_get_ecr( batline );
	return( ecr_number == 0 );
}

/*
 * rows are:                                      background
 * ---------------------------------------------  -----------------------
 * - if the reconciliation is validated           normal
 * - an entry without any proposed bat line       normal
 * - an entry with a proposed bat line            pale yellow concil date
 * - a bat line                                   pale yellow row
 *
 * So we only change the background to pale yellow:
 * - for the reconciliation date of an non-validated entry which has a
 *   child bat line (a reconciliation date should be set)
 * - for the whole row of a bat line
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaRappro *self )
{
	gboolean validated;
	GObject *object;
	GdkRGBA color;
	gint id;
	gboolean paintable;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter,
			COL_VALID,  &validated,
			COL_OBJECT, &object,
			-1 );
	g_object_unref( object );

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"background-set", FALSE,
						NULL );

	g_return_if_fail( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object ));

	paintable = FALSE;

	if( OFO_IS_ENTRY( object )){
		id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
		if( id == COL_RAPPRO ){
			if( gtk_tree_model_iter_has_child( tmodel, iter )){
				paintable = TRUE;
			}
		}
	} else {
		paintable = TRUE;
	}

	if( paintable ){
		gdk_rgba_parse( &color, "#ffffb0" );
		g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
		g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
	}
}

static void
on_account_changed( GtkEntry *entry, ofaRappro *self )
{
	ofaRapproPrivate *priv;
	const gchar *number;
	ofoAccount *account;
	gdouble amount;
	gchar *str;

	account = NULL;
	priv = self->private;
	number = gtk_entry_get_text( priv->account );

	if( number && g_utf8_strlen( number, -1 )){
		account = ofo_account_get_by_number(
						ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )), number );
		if( account ){
			gtk_label_set_text( priv->account_label, ofo_account_get_label( account ));

			amount = ofo_account_get_deb_mnt( account )+ofo_account_get_bro_deb_mnt( account );
			str = g_strdup_printf( "%.2f", amount );
			gtk_label_set_text( priv->account_debit, str );
			g_free( str );

			amount = ofo_account_get_cre_mnt( account )+ofo_account_get_bro_cre_mnt( account );
			str = g_strdup_printf( "%.2f", amount );
			gtk_label_set_text( priv->account_credit, str );
			g_free( str );
		}
	}
	if( !account ){
		gtk_label_set_text( priv->account_label, "" );
		gtk_label_set_text( priv->account_debit, "" );
		gtk_label_set_text( priv->account_credit, "" );
	}

	check_for_enable_fetch( self );
}

static void
on_account_button_clicked( GtkButton *button, ofaRappro *self )
{
	do_account_selection( self );
}

/*
 * setting the entry (gtk_entry_set_text) will trigger a 'changed'
 * message, which itself will update the account properties in the
 * dialog
 */
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
	/*gtk_widget_set_sensitive(
			GTK_WIDGET( self->private->fetch ),
			is_fetch_enableable( self, NULL, NULL ));*/

	if( is_fetch_enableable( self, NULL, NULL )){
		on_fetch_button_clicked( NULL, self );
	}
	set_reconciliated_balance( self );
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
	display_bat_lines( self );
}

static void
do_fetch( ofaRappro *self )
{
	gboolean enableable;
	ofoAccount *account;
	gint mode;
	GList *entries, *it;
	ofoEntry *entry;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *sdope, *sdeb, *scre, *sdrap;
	const GDate *drappro;

	enableable = is_fetch_enableable( self, &account, &mode );
	g_return_if_fail( enableable );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
	g_return_if_fail( mode >= 1 );

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));
	gtk_tree_store_clear( GTK_TREE_STORE( tmodel ));

	entries = ofo_entry_get_dataset_by_concil(
					ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )),
					ofo_account_get_number( account ),
					mode );

	for( it=entries ; it ; it=it->next ){

		entry = OFO_ENTRY( it->data );

		sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DDMM );
		sdeb = g_strdup_printf( "%'.2lf", ofo_entry_get_debit( entry ));
		scre = g_strdup_printf( "%'.2lf", ofo_entry_get_credit( entry ));
		drappro = ofo_entry_get_rappro_dval( entry );
		sdrap = my_date_to_str( drappro, MY_DATE_DDMM );

		gtk_tree_store_insert_with_values(
				GTK_TREE_STORE( tmodel ),
				&iter,
				NULL,
				-1,
				COL_DOPE,   sdope,
				COL_PIECE,  ofo_entry_get_ref( entry ),
				COL_LABEL,  ofo_entry_get_label( entry ),
				COL_DEBIT,  sdeb,
				COL_CREDIT, scre,
				COL_RAPPRO, sdrap,
				COL_VALID,  g_date_valid( drappro ),
				COL_OBJECT, entry,
				-1 );

		g_free( sdope );
		g_free( sdeb );
		g_free( scre );
		g_free( sdrap );
	}

	ofo_entry_free_dataset( entries );
}

/*
 * select an already imported Bank Account Transaction list file
 */
static void
on_select_bat( GtkButton *button, ofaRappro *self )
{
	gint bat_id;

	bat_id = ofa_bat_select_run( ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )));

	if( bat_id > 0 ){
		setup_bat_lines( self, bat_id );
	}
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
					IMPORTER_TYPE_BAT,
					gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( button )));

	if( bat_id > 0 ){
		setup_bat_lines( self, bat_id );
	}
}

static void
on_clear_button_clicked( GtkButton *button, ofaRappro *self )
{
	clear_bat_lines( self );
}

/*
 * makes use of a Bank Account Transaction (BAT) list
 * whether is has just been importer, or it is reloaded from sgbd
 *
 * only lines which have not yet been used for reconciliation are read
 */
static void
setup_bat_lines( ofaRappro *self, gint bat_id )
{
	clear_bat_lines( self );

	self->private->batlines =
			ofo_bat_line_get_dataset(
					ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )),
					bat_id );

	display_bat_lines( self );
	set_reconciliated_balance( self );
}

/*
 *  clear the proposed reconciliations from the model before displaying
 *  the just imported new ones
 *
 *  this means not only removing old BAT lines, but also resetting the
 *  proposed reconciliation date in the entries
 */
static void
clear_bat_lines( ofaRappro *self )
{
	GtkTreeModel *tmodel;
	gboolean bvalid;
	GObject *object;
	GtkTreeIter iter, child_iter;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){

			gtk_tree_model_get(
					tmodel,
					&iter,
					COL_VALID, &bvalid,
					COL_OBJECT, &object,
					-1 );
			g_object_unref( object );

			if( OFO_IS_ENTRY( object )){

				if( !bvalid ){
					gtk_tree_store_set(
							GTK_TREE_STORE( tmodel ),
							&iter,
							COL_RAPPRO, "",
							-1 );
				}

				if( gtk_tree_model_iter_has_child( tmodel, &iter )){
					gtk_tree_model_iter_children( tmodel, &child_iter, &iter );
					gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &child_iter );
				}

				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					break;
				}

			} else if( !gtk_tree_store_remove( GTK_TREE_STORE( tmodel ), &iter )){
				break;
			}
		}
	}

	g_list_free_full( self->private->batlines, ( GDestroyNotify ) g_object_unref );
	self->private->batlines = NULL;
}

/*
 * have just loaded a new set of imported BAT lines
 * try to automatize a proposition of reconciliation
 *
 * for each BAT line,
 * - it has already been used to reconciliate an entry, then set the bat
 *   line alongside with the corresponding entry
 * - else search for an entry with compatible (same, though inversed)
 *   amount, and not yet reconciliated
 * - else just add the bat line without parent
 */
static void
display_bat_lines( ofaRappro *self )
{
	GList *line;
	ofoBatLine *batline;
	gboolean done;
	gint bat_ecr;
	gdouble bat_amount;
	gchar *sbat_deb, *sbat_cre;
	GtkTreeIter *entry_iter;

	for( line=self->private->batlines ; line ; line=line->next ){

		batline = OFO_BAT_LINE( line->data );
		done = FALSE;

		bat_amount = ofo_bat_line_get_montant( batline );
		if( bat_amount < 0 ){
			sbat_deb = g_strdup_printf( "%'.2lf", -bat_amount );
			sbat_cre = g_strdup( "" );
		} else {
			sbat_deb = g_strdup( "" );
			sbat_cre = g_strdup_printf( "%'.2lf", bat_amount );
		}

		bat_ecr = ofo_bat_line_get_ecr( batline );
		if( bat_ecr > 0 ){
			entry_iter = search_for_entry_by_number( self, bat_ecr );
			if( entry_iter ){
				insert_bat_line( self, batline, entry_iter, sbat_deb, sbat_cre );
				gtk_tree_iter_free( entry_iter );
				done = TRUE;
			}
		}

		if( !done ){
			entry_iter = search_for_entry_by_montant( self, sbat_deb, sbat_cre );
			if( entry_iter ){
				update_candidate_entry( self, batline, entry_iter );
				insert_bat_line( self, batline, entry_iter, sbat_deb, sbat_cre );
				gtk_tree_iter_free( entry_iter );
				done = TRUE;
			}
		}

		if( !done ){
			insert_bat_line( self, batline, NULL, sbat_deb, sbat_cre );
		}

		g_free( sbat_deb );
		g_free( sbat_cre );
	}
}

/*
 * returns an iter of the child model, or NULL
 */
static GtkTreeIter *
search_for_entry_by_number( ofaRappro *self, gint number )
{
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	gint ecr_number;
	GObject *object;

	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));

	if( gtk_tree_model_get_iter_first( child_tmodel, &iter )){
		while( TRUE ){

			gtk_tree_model_get( child_tmodel,
					&iter,
					COL_NUMBER, &ecr_number,
					COL_OBJECT, &object,
					-1 );
			g_object_unref( object );

			/* search for the entry which has the specified number */
			if( OFO_IS_ENTRY( object ) && ecr_number == number ){
				entry_iter = gtk_tree_iter_copy( &iter );
				break;
			}

			if( !gtk_tree_model_iter_next( child_tmodel, &iter )){
				break;
			}
		}
	}

	return( entry_iter );
}

/*
 * search for a candidate entry which satisfies the specified criteria
 * returns TRUE and set the iter if found
 * returns FALSE if not found
 */
static GtkTreeIter *
search_for_entry_by_montant( ofaRappro *self, const gchar *sbat_deb, const gchar *sbat_cre )
{
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	gchar *sdeb, *scre;
	GObject *object;
	gboolean found;

	found = FALSE;
	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));

	if( gtk_tree_model_get_iter_first( child_tmodel, &iter )){
		while( TRUE ){

			gtk_tree_model_get( child_tmodel,
					&iter,
					COL_DEBIT,   &sdeb,
					COL_CREDIT,  &scre,
					COL_OBJECT,  &object,
					-1 );
			g_object_unref( object );

			if( OFO_IS_ENTRY( object ) &&
					!gtk_tree_model_iter_has_child( child_tmodel, &iter )){

				/* are the amounts compatible ?
				 * a positive bat_amount implies that the entry should be a debit */
				g_return_val_if_fail( !g_utf8_strlen( sdeb, -1 ) || !g_utf8_strlen( scre, -1 ), NULL );

				if( g_utf8_strlen( sbat_deb, -1 )){
					if( g_utf8_collate( scre, sbat_deb ) == 0 ){
						found = TRUE;
					}
				} else if( g_utf8_collate( sdeb, sbat_cre ) == 0 ){
					found = TRUE;
				}
			}
			g_free( sdeb );
			g_free( scre );

			if( found ){
				entry_iter = gtk_tree_iter_copy( &iter );
				break;
			}

			if( !gtk_tree_model_iter_next( child_tmodel, &iter )){
				break;
			}
		}
	}

	return( entry_iter );
}

/*
 * update the found candidate entry which is not yet reconciliated
 * the provided iter is on the child model
 */
static void
update_candidate_entry( ofaRappro *self, ofoBatLine *batline, GtkTreeIter *entry_iter )
{
	GtkTreeModel *child_tmodel;
	const GDate *dvaleur;
	gchar *sdvaleur;

	g_return_if_fail( entry_iter );

	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));

	dvaleur = ofo_bat_line_get_valeur( batline );
	sdvaleur = my_date_to_str( dvaleur, MY_DATE_DDMM );

	/* set the proposed reconciliation date in the entry */
	gtk_tree_store_set(
			GTK_TREE_STORE( child_tmodel ),
			entry_iter,
			COL_RAPPRO, sdvaleur,
			-1 );

	g_free( sdvaleur );
}

/*
 * insert the bat line either as a child of entry_iter, or without parent
 */
static void
insert_bat_line( ofaRappro *self, ofoBatLine *batline,
							GtkTreeIter *entry_iter, const gchar *sdeb, const gchar *scre )
{
	GtkTreeModel *child_tmodel;
	const GDate *dope;
	gchar *sdope;
	GtkTreeIter new_iter;

	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));

	dope = ofo_bat_line_get_ope( batline );
	if( !g_date_valid( dope )){
		dope = ofo_bat_line_get_valeur( batline );
	}
	sdope = my_date_to_str( dope, MY_DATE_DDMM );

	/* set the bat line as a hint */
	gtk_tree_store_insert_with_values(
				GTK_TREE_STORE( child_tmodel ),
				&new_iter,
				entry_iter,
				-1,
				COL_DOPE,    sdope,
				COL_PIECE,   ofo_bat_line_get_ref( batline ),
				COL_NUMBER,  0,
				COL_LABEL,   ofo_bat_line_get_label( batline ),
				COL_DEBIT,   sdeb,
				COL_CREDIT,  scre,
				COL_OBJECT,  batline,
				-1 );

	g_free( sdope );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * Handles left and right arrows to expand/collapse nodes
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRappro *self )
{
	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Left ){
			collapse_node( self, widget );
		} else if( event->keyval == GDK_KEY_Right ){
			expand_node( self, widget );
		}
	}

	return( FALSE );
}

static void
collapse_node( ofaRappro *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeIter iter, parent;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, NULL, &iter )){

			if( gtk_tree_model_iter_has_child( self->private->tmodel, &iter )){
				path = gtk_tree_model_get_path( self->private->tmodel, &iter );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );

			} else if( gtk_tree_model_iter_parent( self->private->tmodel, &parent, &iter )){
				path = gtk_tree_model_get_path( self->private->tmodel, &parent );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
expand_node( ofaRappro *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, NULL, &iter )){

			if( gtk_tree_model_iter_has_child( self->private->tmodel, &iter )){
				path = gtk_tree_model_get_path( self->private->tmodel, &iter );
				gtk_tree_view_expand_row( GTK_TREE_VIEW( widget ), path, FALSE );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page )
{
	if( toggle_rappro( OFA_RAPPRO( page ), view, path )){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( gtk_tree_view_get_model( view )));
		set_reconciliated_balance( OFA_RAPPRO( page ));
	}
}

/*
 * return TRUE if we have actually reconciliated an entry
 * this prevent us to recompute the account balances if we don't have
 * do anything
 */
static gboolean
toggle_rappro( ofaRappro *self, GtkTreeView *tview, GtkTreePath *path )
{
	GtkTreeIter iter;
	gchar *srappro;
	gboolean bvalid;
	GObject *object;
	GDate date;

	if( gtk_tree_model_get_iter( self->private->tmodel, &iter, path )){

		gtk_tree_model_get(
				self->private->tmodel,
				&iter,
				COL_RAPPRO,  &srappro,
				COL_VALID,   &bvalid,
				COL_OBJECT,  &object,
				-1 );
		g_object_unref( object );

		if( !OFO_IS_ENTRY( object )){
			return( FALSE );
		}

		/* reconciliation is already set up, so clears it
		 * entry: set reconciliation date to null
		 * bat_line (if exists): set reconciliated entry to null */
		if( bvalid ){
			reconciliate_entry( self, OFO_ENTRY( object ), NULL, &iter );

		/* reconciliation is not set yet, so set it if proposed date is
		 * valid or if we have a proposed reconciliation from imported
		 * BAT */
		} else {
			if( srappro && g_utf8_strlen( srappro, -1 )){
				my_date_parse_from_str( &date, srappro, MY_DATE_DDMM );
			} else {
				my_date_set_from_date( &date, &self->private->dconcil );
			}
			if( g_date_valid( &date )){
				reconciliate_entry( self, OFO_ENTRY( object ), &date, &iter );
			}
		}
	}

	return( TRUE );
}

/*
 * @drappro: may be NULL for clearing a previously set reconciliation
 * @iter: the iter on the entry row in the parent filter model
 *
 * Note on the notations:
 *
 * self->private->tmodel: the filter tree model
 * child_tmodel: the child tree model
 *
 * iter: the entry iter in the filter tree model
 * child_iter: the entry iter in the child tree model
 * child_iter_child: the entry's child batline iter in the child tree model
 *
 */
static void
reconciliate_entry( ofaRappro *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *iter )
{
	GtkTreeModel *child_tmodel;
	GtkTreeIter child_iter;
	GtkTreeIter child_iter_child;
	ofoBatLine *batline;
	gboolean is_valid_rappro;
	gchar *str;

	is_valid_rappro = drappro && g_date_valid( drappro );
	batline = NULL;

	/* set the reconciliation date in the entry */
	ofo_entry_set_rappro_dval( entry, is_valid_rappro ? drappro : NULL );

	/* update the child bat line if it exists
	 * we work on child model because 'gtk_tree_model_iter_has_child'
	 * actually says if we have a *visible* child
	 * but a possible batline is not visible when we are clearing
	 * the reconciliation date */
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->private->tmodel ));

	gtk_tree_model_filter_convert_iter_to_child_iter(
			GTK_TREE_MODEL_FILTER( self->private->tmodel ), &child_iter, iter );

	if( gtk_tree_model_iter_has_child( child_tmodel, &child_iter ) &&
		gtk_tree_model_iter_children( child_tmodel, &child_iter_child, &child_iter )){

		gtk_tree_model_get(
				child_tmodel,
				&child_iter_child,
				COL_OBJECT,  &batline,
				-1 );
		g_object_unref( batline );

		ofo_bat_line_set_ecr( batline,
				is_valid_rappro ? ofo_entry_get_number( entry ) : 0 );
	}

	/* update the entry in the tree model with the new reconciliation date
	 * this is needed
 	 * - if we are clearing the reconciliation date and there is no child to propose
 	 * - if we are setting a reconciliation date without child
 	 *
 	 * @child_tmodel: the child tree model
 	 * @iter_child: an iter on the entry row in the child tree model */

	if( is_valid_rappro ){
		str = my_date_to_str( drappro, MY_DATE_DDMM );
	} else if( batline ){
		str = my_date_to_str( ofo_bat_line_get_valeur( batline ), MY_DATE_DDMM );
	} else {
		str = g_strdup( "" );
	}

	gtk_tree_store_set(
			GTK_TREE_STORE( child_tmodel ),
			&child_iter,
			COL_RAPPRO, str,
			COL_VALID,  is_valid_rappro,
			-1 );

	g_free( str );

	/* last, update the sgbd */
	ofo_entry_update_rappro(
			entry,
			ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )));

	if( batline ){
		ofo_bat_line_update(
				batline,
				ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )));
	}
}

/*
 * display the new balance of the account, taking into account the
 * reconciliated entries and the unreconciliated BAT lines
 */
static void
set_reconciliated_balance( ofaRappro *self )
{
	gdouble debit, credit;
	const gchar *account_number;
	ofoAccount *account;
	gdouble account_debit, account_credit;
	GtkTreeIter iter;
	gboolean bvalid;
	GObject *object;
	gdouble amount;
	gchar *sdeb, *scre;

	account = NULL;
	account_debit = 0.0;
	account_credit = 0.0;
	account_number = gtk_entry_get_text( self->private->account );
	if( account_number && g_utf8_strlen( account_number, -1 )){
		account = ofo_account_get_by_number(
						ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )), account_number );
	}
	if( account ){
		account_debit = ofo_account_get_deb_mnt( account )+ofo_account_get_bro_deb_mnt( account );
		account_credit = ofo_account_get_cre_mnt( account )+ofo_account_get_bro_cre_mnt( account );
	}

	debit = account_debit;
	credit = account_credit;

	if( gtk_tree_model_get_iter_first( self->private->tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get(
					self->private->tmodel, &iter, COL_VALID, &bvalid, COL_OBJECT, &object, -1 );
			g_object_unref( object );

			if( !bvalid ){
				if( OFO_IS_ENTRY( object )){
					debit -= ofo_entry_get_debit( OFO_ENTRY( object ));
					credit -= ofo_entry_get_credit( OFO_ENTRY( object ));
				} else {
					amount = ofo_bat_line_get_montant( OFO_BAT_LINE( object ));
					if( amount < 0 ){
						credit += -amount;
					} else {
						debit += amount;
					}
				}
			}
			if( !gtk_tree_model_iter_next( self->private->tmodel, &iter )){
				break;
			}
		}
	}

	if( debit > credit ){
		sdeb = g_strdup_printf( "%.2lf", debit-credit );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		scre = g_strdup_printf( "%.2lf", credit-debit );
	}

	gtk_label_set_text( self->private->bal_debit, sdeb );
	gtk_label_set_text( self->private->bal_credit, scre );

	g_free( sdeb );
	g_free( scre );
}
