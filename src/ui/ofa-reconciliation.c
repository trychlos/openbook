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
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-concil-collection.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-plugin.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-date-filter-bin.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-pdf-reconcil.h"
#include "ui/ofa-reconciliation.h"

/* private instance data
 */
struct _ofaReconciliationPrivate {

	/* UI - account
	 */
	GtkEntry          *account_entry;
	GtkLabel          *account_label;
	GtkLabel          *account_debit;
	GtkLabel          *account_credit;
	ofoAccount        *account;

	/* UI - filtering mode
	 */
	GtkComboBox       *mode_combo;
	gint               mode;

	/* UI - effect dates filter
	 */
	ofaDateFilterBin  *effect_filter;

	/* UI - manual conciliation
	 */
	GtkEntry          *date_concil;
	GDate              dconcil;

	/* UI - assisted conciliation
	 */
	GtkWidget         *file_chooser;
	GtkWidget         *count_label;
	GtkWidget         *unused_label;
	GtkButton         *clear;
	GtkWidget         *accept_btn;
	GtkWidget         *decline_btn;

	/* UI - actions
	 */
	GtkWidget         *print_btn;

	/* UI - entries view
	 */
	GtkTreeModel      *tfilter;				/* GtkTreeModelFilter of the tree view */
	GtkTreeModel      *tsort;				/* GtkTreeModelSort stacked onto the TreeModelFilter */
	GtkTreeView       *tview;				/* the treeview built on the sorted model */
	GtkTreeViewColumn *sort_column;

	/* UI - reconciliated balance
	 */
	GtkLabel          *bal_debit;			/* balance of the account  */
	GtkLabel          *bal_credit;			/*  ... deducting unreconciliated entries */

	/* UI - Buttons box
	 */
	ofaButtonsBox     *box;

	/* internals
	 */
	ofoDossier        *dossier;
	GList             *handlers;
	ofoBat            *bat;
	GList             *batlines;
};

/* column ordering in the main entries listview
 */
enum {
	COL_ACCOUNT,
	COL_DOPE,
	COL_PIECE,
	COL_NUMBER,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	COL_DRECONCIL,
	COL_OBJECT,				/* may be an ofoEntry or an ofoBatLine */
	N_COLUMNS
};

/* set against the COL_DRECONCIL column to be used in on_cell_data_func() */
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

/*
 * ofaEntryConcil:
 */
typedef enum {
	ENT_CONCILED_YES = 1,
	ENT_CONCILED_NO,
	ENT_CONCILED_ALL,
	ENT_CONCILED_SESSION
}
	ofaEntryConcil;

static const sConcil st_concils[] = {
		{ ENT_CONCILED_YES,     N_( "Reconciliated" ) },
		{ ENT_CONCILED_NO,      N_( "Not reconciliated" ) },
		{ ENT_CONCILED_SESSION, N_( "Reconciliation session" ) },
		{ ENT_CONCILED_ALL,     N_( "All" ) },
		{ 0 }
};

#define COLOR_LABEL_NORMAL              "#000000"	/* black */
#define COLOR_LABEL_INVALID             "#808080"	/* gray */

#define COLOR_ACCOUNT                   "#0000ff"	/* blue */
#define COLOR_BAT_CONCIL_FONT           "#008000"	/* middle green */
#define COLOR_BAT_UNCONCIL_FONT         "#00ff00"	/* pure green */
#define COLOR_BAT_UNCONCIL_BACKGROUND   "#00ff00"	/* pure green */

static const gchar *st_reconciliation   = "Reconciliation";
static const gchar *st_effect_dates     = "ReconciliationEffects";

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

static const gchar *st_default_reconciliated_class = "5"; /* default account class to be reconciliated */

G_DEFINE_TYPE( ofaReconciliation, ofa_reconciliation, OFA_TYPE_PAGE )

static GtkWidget   *v_setup_view( ofaPage *page );
static GtkWidget   *setup_account_selection( ofaPage *page );
static GtkWidget   *setup_mode_filter( ofaPage *page );
static GtkWidget   *setup_effect_dates( ofaPage *page );
static GtkWidget   *setup_manual_rappro( ofaPage *page );
static GtkWidget   *setup_auto_rappro( ofaPage *page );
static GtkWidget   *setup_treeview_header( ofaPage *page );
static GtkWidget   *setup_treeview( ofaPage *page );
static GtkWidget   *setup_buttons( ofaPage *page );
static GtkWidget   *setup_treeview_footer( ofaPage *page );
static GtkWidget   *v_setup_buttons( ofaPage *page );
static void         v_init_view( ofaPage *page );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static void         on_account_entry_changed( GtkEntry *entry, ofaReconciliation *self );
static void         on_account_select_clicked( GtkButton *button, ofaReconciliation *self );
static void         do_account_selection( ofaReconciliation *self );
static void         clear_account_properties( ofaReconciliation *self );
static ofoAccount  *get_reconciliable_account( ofaReconciliation *self );
static void         do_fetch_entries( ofaReconciliation *self );
static void         insert_entry( ofaReconciliation *self, GtkTreeModel *tstore, ofoEntry *entry, GtkTreeIter *iter );
static void         set_row_entry( ofaReconciliation *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry );
static void         on_mode_combo_changed( GtkComboBox *box, ofaReconciliation *self );
static void         select_mode( ofaReconciliation *self, gint mode );
static void         on_effect_dates_changed( ofaDateFilterBin *filter, gint who, const GDate *date, ofaReconciliation *self );
static void         on_date_concil_changed( GtkEditable *editable, ofaReconciliation *self );
static void         on_select_bat( GtkButton *button, ofaReconciliation *self );
static void         do_select_bat( ofaReconciliation *self );
static void         on_file_set( GtkFileChooserButton *button, ofaReconciliation *self );
static void         on_clear_button_clicked( GtkButton *button, ofaReconciliation *self );
static void         clear_bat_file( ofaReconciliation *self );
static void         setup_bat_file( ofaReconciliation *self, ofxCounter bat_id );
static void         display_bat_lines( ofaReconciliation *self );
static void         display_bat_count( ofaReconciliation *self );
static gint         on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconciliation *self );
static void         on_header_clicked( GtkTreeViewColumn *column, ofaReconciliation *self );
static gboolean     is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconciliation *self );
static gboolean     is_visible_entry( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static gboolean     is_visible_batline( ofaReconciliation *self, ofoBatLine *batline );
static gboolean     is_entry_session_conciliated( ofaReconciliation *self, ofoEntry *entry, ofoConcil *concil );
static void         on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconciliation *self );
static void         on_tview_selection_changed( GtkTreeSelection *select, ofaReconciliation *self );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaReconciliation *self );
static gboolean     on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaReconciliation *self );
static void         collapse_node( ofaReconciliation *self, GtkWidget *widget );
static void         collapse_node_by_iter( ofaReconciliation *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static void         expand_node( ofaReconciliation *self, GtkWidget *widget );
static void         expand_node_by_iter( ofaReconciliation *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gboolean     check_for_enable_view( ofaReconciliation *self );
static void         default_expand_view( ofaReconciliation *self );
static void         on_accept_clicked( GtkButton *button, ofaReconciliation *self );
static void         on_decline_clicked( GtkButton *button, ofaReconciliation *self );
static GtkTreeIter *search_for_entry_by_number( ofaReconciliation *self, ofxCounter number );
static GtkTreeIter *search_for_entry_by_amount( ofaReconciliation *self, const gchar *sbat_deb, const gchar *sbat_cre );
static void         insert_bat_line( ofaReconciliation *self, ofoBatLine *batline, GtkTreeIter *entry_iter, const gchar *sdeb, const gchar *scre );
static gboolean     run_selection_engine( ofaReconciliation *self );
static void         examine_selection( ofaReconciliation *self, GtkTreeModel *tmodel, GList *paths, ofxAmount *debit, ofxAmount *credit, ofxCounter *recid, gboolean *unique, gint *rowsnb, GList **concils, GList **unconcils );
static gboolean     is_unreconciliate_accepted( ofaReconciliation *self, ofoConcil *concil );
/*
static gboolean     unreconciliate_entries( ofaReconciliation *self, ofoEntry *entry, GList *entries );
static gboolean     reconciliate_entry( ofaReconciliation *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *iter );
static gboolean     reconciliate_entry_by_store_iter( ofaReconciliation *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *store_iter );
*/
static void         set_reconciliated_balance( ofaReconciliation *self );
static void         get_batline_dval_from_objects( ofaReconciliation *self, GDate *dval, GList *objects );
static void         get_entry_dval_by_path( ofaReconciliation *self, GDate *dval, GtkTreeModel *tmodel, GtkTreePath *path );
static void         set_entry_dval_by_path( ofaReconciliation *self, GDate *dval, GtkTreeModel *tmodel, GtkTreePath *path );
static void         set_entry_dval_by_store_iter( ofaReconciliation *self, const GDate *dval, GtkTreeModel *store_model, GtkTreeIter *store_iter );
static void         set_reconciliated_dval_from_concil( ofaReconciliation *self, ofoConcil *concil );
static void         reset_proposed_dval_from_concil( ofaReconciliation *self, ofoConcil *concil );
static void         add_child_to_concil_by_path( ofaReconciliation *self, ofoConcil *concil, GtkTreeModel *tmodel, GtkTreePath *path );
static ofoBatLine  *get_child_by_path( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreePath *path );
static ofoBatLine  *get_child_by_store_iter( ofaReconciliation *self, GtkTreeModel *store_model, GtkTreeIter *ent_iter );
static const GDate *get_bat_line_dope( ofaReconciliation *self, ofoBatLine *batline );
static ofoBase     *get_object_by_path( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreePath *path );
static void         get_store_iter_from_path( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeModel **store_model, GtkTreeIter *store_iter );
static void         get_settings( ofaReconciliation *self );
static void         set_settings( ofaReconciliation *self );
static void         dossier_signaling_connect( ofaReconciliation *self );
static void         on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaReconciliation *self );
static void         on_new_entry( ofaReconciliation *self, ofoEntry *entry );
static void         insert_new_entry( ofaReconciliation *self, ofoEntry *entry );
static void         remediate_bat_lines( ofaReconciliation *self, GtkTreeModel *tstore, ofoEntry *entry, GtkTreeIter *entry_iter );
static void         on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaReconciliation *self );
static void         on_updated_entry( ofaReconciliation *self, ofoEntry *entry );
static void         on_print_clicked( GtkButton *button, ofaReconciliation *self );

static void
reconciliation_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconciliation_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_RECONCILIATION( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconciliation_parent_class )->finalize( instance );
}

static void
reconciliation_dispose( GObject *instance )
{
	ofaReconciliationPrivate *priv;
	GList *it;

	g_return_if_fail( OFA_IS_RECONCILIATION( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ( OFA_RECONCILIATION( instance ))->priv;

		if( priv->batlines ){
			ofo_bat_line_free_dataset( priv->batlines );
		}

		if( priv->handlers && priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
			priv->handlers = NULL;
		}

		/*
		if( priv->concils ){
			g_list_free_full( priv->concils, ( GDestroyNotify ) g_object_unref );
		}
		*/
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconciliation_parent_class )->dispose( instance );
}

static void
ofa_reconciliation_init( ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_RECONCILIATION( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RECONCILIATION, ofaReconciliationPrivate );

	my_date_clear( &self->priv->dconcil );
}

static void
ofa_reconciliation_class_init( ofaReconciliationClass *klass )
{
	static const gchar *thisfn = "ofa_reconciliation_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconciliation_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconciliation_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaReconciliationPrivate ));
}

/*
 * grid: the first row contains 'n' columns for selection and filters
 * the second row contains another grid which manages the treeview,
 * along with header and footer
 */
static GtkWidget *
v_setup_view( ofaPage *page )
{
	GtkGrid *grid;
	gint column;
	GtkWidget *account, *mode, *effect, *rappro, *tview, *buttons, *soldes;
	GtkWidget *grid2;

	grid = GTK_GRID( gtk_grid_new());
	gtk_widget_set_margin_left( GTK_WIDGET( grid ), 4 );
	gtk_widget_set_margin_right( GTK_WIDGET( grid ), 4 );
	gtk_grid_set_column_spacing( grid, 4 );
	column = 0;

	account = setup_account_selection( page );
	gtk_grid_attach( grid, account, column++, 0, 1, 1 );

	mode = setup_mode_filter( page );
	gtk_grid_attach( grid, mode, column++, 0, 1, 1 );

	/* effect dates filter */
	effect = setup_effect_dates( page );
	gtk_grid_attach( grid, effect, column++, 0, 1, 1 );

	/* manual reconciliation (enter a date) */
	rappro = setup_manual_rappro( page );
	gtk_grid_attach( grid, rappro, column++, 0, 1, 1 );

	/* auto reconciliation from imported BAT file */
	rappro = setup_auto_rappro( page );
	gtk_grid_attach( grid, rappro, column++, 0, 1, 1 );

	grid2 = gtk_grid_new();
	gtk_grid_attach( grid, grid2, 0, 1, column, 1 );

	/* account label and balance header display */
	account = setup_treeview_header( page );
	gtk_grid_attach( GTK_GRID( grid2 ), account, 0, 0, 1, 1 );

	tview = setup_treeview( page );
	gtk_grid_attach( GTK_GRID( grid2 ), tview, 0, 1, 1, 1 );

	/* buttons box */
	buttons = setup_buttons( page );
	gtk_grid_attach( GTK_GRID( grid2 ), buttons, 1, 1, 1, 1 );

	/* computed bank balance */
	soldes = setup_treeview_footer( page );
	gtk_grid_attach( GTK_GRID( grid2 ), soldes, 0, 2, 1, 1 );

	get_settings( OFA_RECONCILIATION( page ));
	dossier_signaling_connect( OFA_RECONCILIATION( page ));

	return( GTK_WIDGET( grid ));
}

/*
 * account selection is an entry + a select button
 */
static GtkWidget *
setup_account_selection( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkWidget *frame, *alignment, *label, *grid_account, *grid_select;
	gchar *markup;
	GtkWidget *image, *button;

	priv = OFA_RECONCILIATION( page )->priv;

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	label = gtk_label_new( NULL );
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Account selection" ));
	gtk_label_set_markup( GTK_LABEL( label ), markup );
	gtk_frame_set_label_widget( GTK_FRAME( frame ), label );
	g_free( markup );

	alignment = gtk_alignment_new( 0.5, 0.5, 1.0, 1.0 );
	gtk_alignment_set_padding( GTK_ALIGNMENT( alignment ), 4, 4, 8, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), alignment );

	/* the grid for account */
	grid_account = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( grid_account ), 4 );
	gtk_grid_set_row_spacing( GTK_GRID( grid_account ), 3 );
	gtk_container_add( GTK_CONTAINER( alignment ), grid_account );

	label = gtk_label_new_with_mnemonic( _( "_Account :" ));
	gtk_widget_set_halign( label, GTK_ALIGN_START );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( GTK_GRID( grid_account ), label, 0, 0, 1, 1 );

	/* a small grid just for entry + select button
	 * have a label at the end to make it expand*/
	grid_select = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( grid_select ), 2 );
	gtk_grid_attach( GTK_GRID( grid_account ), grid_select, 1, 0, 1, 1 );

	priv->account_entry = GTK_ENTRY( gtk_entry_new());
	gtk_entry_set_max_length( priv->account_entry, 20 );
	gtk_entry_set_width_chars( priv->account_entry, 10 );
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->account_entry ));
	gtk_grid_attach( GTK_GRID( grid_select ), GTK_WIDGET( priv->account_entry ), 0, 0, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->account_entry ),
			_( "Enter here the number of the account to be reconciliated" ));
	g_signal_connect(
			G_OBJECT( priv->account_entry ),
			"changed", G_CALLBACK( on_account_entry_changed ), page );

	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
	button = gtk_button_new();
	gtk_button_set_image( GTK_BUTTON( button ), image );
	gtk_grid_attach( GTK_GRID( grid_select ), button, 1, 0, 1, 1 );
	gtk_widget_set_tooltip_text(
			button,
			_( "Select the account to be reconciliated" ));
	g_signal_connect(
			G_OBJECT( button ),
			"clicked", G_CALLBACK( on_account_select_clicked ), page );

	label = gtk_label_new( "" );
	gtk_widget_set_halign( label, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( label, TRUE );
	gtk_grid_attach( GTK_GRID( grid_select ), label, 2, 0, 1, 1 );

	/* have the account label in row 2 */
	priv->account_label = GTK_LABEL( gtk_label_new( "" ));
	gtk_misc_set_alignment( GTK_MISC( priv->account_label ), 0, 0.5 );
	gtk_label_set_ellipsize( priv->account_label, PANGO_ELLIPSIZE_END );
	gtk_grid_attach( GTK_GRID( grid_account ), GTK_WIDGET( priv->account_label ), 0, 1, 2, 1 );

	return( GTK_WIDGET( frame ));
}

/*
 * the combo box for filtering the displayed entries
 */
static GtkWidget *
setup_mode_filter( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkWidget *frame, *alignment, *label, *grid_filter;
	gchar *markup;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i;

	priv = OFA_RECONCILIATION( page )->priv;

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	label = gtk_label_new( NULL );
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Mode filter" ));
	gtk_label_set_markup( GTK_LABEL( label ), markup );
	gtk_frame_set_label_widget( GTK_FRAME( frame ), label );
	g_free( markup );

	alignment = gtk_alignment_new( 0.5, 0.5, 1.0, 1.0 );
	gtk_alignment_set_padding( GTK_ALIGNMENT( alignment ), 4, 4, 8, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), alignment );

	/* the grid for filtering entries */
	grid_filter = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( grid_filter ), 4 );
	gtk_container_add( GTK_CONTAINER( alignment ), grid_filter );

	label = gtk_label_new_with_mnemonic( _( "_Entries :" ));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( GTK_GRID( grid_filter ), label, 0, 0, 1, 1 );

	priv->mode_combo = GTK_COMBO_BOX( gtk_combo_box_new());
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->mode_combo ));
	gtk_grid_attach( GTK_GRID( grid_filter ), GTK_WIDGET( priv->mode_combo ), 1, 0, 1, 1 );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new( ENT_N_COLUMNS, G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model( priv->mode_combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->mode_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->mode_combo ), cell, "text", ENT_COL_LABEL );

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
			GTK_WIDGET( priv->mode_combo ),
			_( "Filter the type of entries to be displayed" ));
	g_signal_connect(
			G_OBJECT( priv->mode_combo ),
			"changed", G_CALLBACK( on_mode_combo_changed ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_effect_dates( ofaPage *page )
{
	ofaReconciliationPrivate *priv;

	priv = OFA_RECONCILIATION( page )->priv;

	priv->effect_filter = ofa_date_filter_bin_new( st_effect_dates );
	g_signal_connect( priv->effect_filter, "changed", G_CALLBACK( on_effect_dates_changed ), page );

	return( GTK_WIDGET( priv->effect_filter ));
}

static GtkWidget *
setup_manual_rappro( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkFrame *frame;
	GtkAlignment *alignment;
	GtkGrid *grid;
	GtkLabel *label;
	gchar *markup;

	priv = OFA_RECONCILIATION( page )->priv;

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
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( grid ));

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "Da_te :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	priv->date_concil = GTK_ENTRY( gtk_entry_new());
	label = GTK_LABEL( gtk_label_new( "" ));

	my_editable_date_init( GTK_EDITABLE( priv->date_concil ));
	my_editable_date_set_format( GTK_EDITABLE( priv->date_concil ), ofa_prefs_date_display());
	my_editable_date_set_date( GTK_EDITABLE( priv->date_concil ), &priv->dconcil );
	my_editable_date_set_label( GTK_EDITABLE( priv->date_concil ), GTK_WIDGET( label ), ofa_prefs_date_check());

	gtk_entry_set_width_chars( priv->date_concil, 10 );
	gtk_label_set_mnemonic_widget( label, GTK_WIDGET( priv->date_concil ));
	gtk_grid_attach( grid, GTK_WIDGET( priv->date_concil ), 1, 0, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->date_concil ),
			_( "The date to which the entry will be set as reconciliated if no account transaction is proposed" ));

	g_signal_connect(
			G_OBJECT( priv->date_concil ), "changed", G_CALLBACK( on_date_concil_changed ), page );

	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_label_set_width_chars( label, 10 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 2, 0, 1, 1 );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_auto_rappro( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkFrame *frame;
	GtkAlignment *alignment;
	GtkGrid *grid;
	GtkWidget *label;
	gchar *markup;
	GtkWidget *button, *image;
	GdkRGBA color;

	priv = OFA_RECONCILIATION( page )->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	label = gtk_label_new( NULL );
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Assisted reconciliation" ));
	gtk_label_set_markup( GTK_LABEL( label ), markup );
	gtk_frame_set_label_widget( frame, label );
	g_free( markup );

	alignment = GTK_ALIGNMENT( gtk_alignment_new( 0.5, 0.5, 1.0, 1.0 ));
	gtk_alignment_set_padding( alignment, 4, 4, 12, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( alignment ));

	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( grid ));

	button = gtk_button_new_with_mnemonic( _( "_Select..." ));
	gtk_grid_attach( grid, button, 0, 0, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_select_bat ), page );
	gtk_widget_set_tooltip_text(
			button,
			_( "Select a previously imported Bank Account Transactions list" ));

	button = gtk_file_chooser_button_new( NULL, GTK_FILE_CHOOSER_ACTION_OPEN );
	gtk_grid_attach( grid, button, 1, 0, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "file-set", G_CALLBACK( on_file_set ), page );
	gtk_widget_set_tooltip_text(
			button,
			_( "Import an new Bank Account Transactions list to be used in the reconciliation" ));
	priv->file_chooser = button;

	label = gtk_label_new( "(" );
	gtk_grid_attach( grid, label, 2, 0, 1, 1 );

	label = gtk_label_new( "" );
	gtk_grid_attach( grid, label, 3, 0, 1, 1 );
	priv->count_label = label;

	label = gtk_label_new( "-" );
	gtk_grid_attach( grid, label, 4, 0, 1, 1 );

	label = gtk_label_new( "" );
	gtk_grid_attach( grid, label, 5, 0, 1, 1 );
	gdk_rgba_parse( &color, COLOR_BAT_UNCONCIL_FONT );
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
	priv->unused_label = label;

	label = gtk_label_new( ")" );
	gtk_grid_attach( grid, label, 6, 0, 1, 1 );

	image = gtk_image_new_from_icon_name( "gtk-clear", GTK_ICON_SIZE_BUTTON );
	priv->clear = GTK_BUTTON( gtk_button_new());
	gtk_button_set_image( priv->clear, image );
	gtk_grid_attach( grid, GTK_WIDGET( priv->clear ), 7, 0, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->clear ),
			_( "Clear the displayed Bank Account Transaction lines" ));
	g_signal_connect(
			G_OBJECT( priv->clear ),
			"clicked", G_CALLBACK( on_clear_button_clicked ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_treeview_header( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkBox *box;
	GtkLabel *label;
	GdkRGBA color;

	priv = OFA_RECONCILIATION( page )->priv;

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));

	gdk_rgba_parse( &color, COLOR_ACCOUNT );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_label_set_width_chars( label, 14 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), FALSE, FALSE, 0 );

	priv->account_credit = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_override_color( GTK_WIDGET( priv->account_credit ), GTK_STATE_FLAG_NORMAL, &color );
	gtk_misc_set_alignment( GTK_MISC( priv->account_credit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->account_credit, 12 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->account_credit ), FALSE, FALSE, 0 );

	priv->account_debit = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_override_color( GTK_WIDGET( priv->account_debit ), GTK_STATE_FLAG_NORMAL, &color );
	gtk_misc_set_alignment( GTK_MISC( priv->account_debit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->account_debit, 12 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->account_debit ), FALSE, FALSE, 0 );

	label = GTK_LABEL( gtk_label_new( _( "Openbook account balance :" )));
	gtk_widget_override_color( GTK_WIDGET( label ), GTK_STATE_FLAG_NORMAL, &color );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
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
setup_treeview( ofaPage *page )
{
	static const gchar *thisfn = "ofa_reconciliation_setup_treeview";
	ofaReconciliationPrivate *priv;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;

	priv = OFA_RECONCILIATION( page )->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_key_pressed ), page );

	tmodel = GTK_TREE_MODEL( gtk_tree_store_new(
			N_COLUMNS,
			G_TYPE_STRING,									/* account */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,		/* dope, piece, number */
			G_TYPE_STRING,									/* label */
			G_TYPE_STRING, G_TYPE_STRING,					/* debit, credit */
			G_TYPE_STRING,									/* dcreconcil */
			G_TYPE_OBJECT ));

	priv->tfilter = gtk_tree_model_filter_new( tmodel, NULL );
	g_object_unref( tmodel );
	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( priv->tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			page,
			NULL );

	priv->tsort = gtk_tree_model_sort_new_with_model( priv->tfilter );
	g_object_unref( priv->tfilter );

	gtk_tree_view_set_model( tview, priv->tsort );
	g_object_unref( priv->tsort );

	g_debug( "%s: treestore=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) tmodel, ( void * ) priv->tfilter, ( void * ) priv->tsort );

	/* account is not displayed */

	/* operation date
	 */
	column_id = COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ope." ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	/* default is to sort by ascending operation date
	 */
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, OFA_SORT_ASCENDING );

	/* piece's reference
	 */
	column_id = COL_PIECE;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_min_width( column, 80 );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	/* entry number is not displayed */

	/* entry label
	 */
	column_id = COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	/* debit
	 */
	column_id = COL_DEBIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	/* credit
	 */
	column_id = COL_CREDIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	/* reconciliation date
	 */
	column_id = COL_DRECONCIL;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 0.0, 0.5 );
	column = gtk_tree_view_column_new();
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_pack_end( column, text_cell, FALSE );
	gtk_tree_view_column_set_alignment( column, 0.5 );
	gtk_tree_view_column_set_title( column, _( "Reconcil." ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", column_id );
	gtk_tree_view_column_set_min_width( column, 100 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_cell_data_func(
			column, text_cell, ( GtkTreeCellDataFunc ) on_cell_data_func, page, NULL );
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), page );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( priv->tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	g_signal_connect( select, "changed", G_CALLBACK( on_tview_selection_changed ), page );

	gtk_widget_set_sensitive( GTK_WIDGET( tview ), FALSE );
	priv->tview = tview;

	return( GTK_WIDGET( frame ));
}

/*
 * two widgets (debit/credit) display the bank balance of the
 * account, by deducting the unreconciliated entries from the balance
 * in our book - this is supposed simulate the actual bank balance
 */
static GtkWidget *
setup_treeview_footer( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkBox *box;
	GtkLabel *label;
	GdkRGBA color;

	priv = OFA_RECONCILIATION( page )->priv;

	gdk_rgba_parse( &color, COLOR_ACCOUNT );

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));
	gtk_widget_set_margin_bottom( GTK_WIDGET( box ), 2 );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_label_set_width_chars( label, 14 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), FALSE, FALSE, 0 );

	priv->bal_credit = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_override_color( GTK_WIDGET( priv->bal_credit ), GTK_STATE_FLAG_NORMAL, &color );
	gtk_misc_set_alignment( GTK_MISC( priv->bal_credit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->bal_credit, 11 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->bal_credit ), FALSE, FALSE, 0 );

	priv->bal_debit = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_override_color( GTK_WIDGET( priv->bal_debit ), GTK_STATE_FLAG_NORMAL, &color );
	gtk_misc_set_alignment( GTK_MISC( priv->bal_debit ), 1.0, 0.5 );
	gtk_label_set_width_chars( priv->bal_debit, 11 );
	gtk_box_pack_end( box, GTK_WIDGET( priv->bal_debit ), FALSE, FALSE, 0 );

	label = GTK_LABEL( gtk_label_new( _( "Bank reconciliated balance :" )));
	gtk_widget_override_color( GTK_WIDGET( label ), GTK_STATE_FLAG_NORMAL, &color );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_box_pack_end( box, GTK_WIDGET( label ), TRUE, TRUE, 0 );

	return( GTK_WIDGET( box ));
}

static GtkWidget *
setup_buttons( ofaPage *page )
{
	ofaReconciliationPrivate *priv;

	priv = OFA_RECONCILIATION( page )->priv;
	priv->box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( priv->box );		/* treeview header */
	priv->accept_btn = ofa_buttons_box_add_button( priv->box,
			BUTTON_ACCEPT, TRUE, G_CALLBACK( on_accept_clicked ), page );
	priv->decline_btn = ofa_buttons_box_add_button( priv->box,
			BUTTON_DECLINE, FALSE, G_CALLBACK( on_decline_clicked ), page );

	ofa_buttons_box_add_spacer( priv->box );
	priv->print_btn = ofa_buttons_box_add_button( priv->box,
			BUTTON_PRINT, TRUE, G_CALLBACK( on_print_clicked ), page );

	return( GTK_WIDGET( priv->box ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaPage *page )
{
	check_for_enable_view( OFA_RECONCILIATION( page ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_RECONCILIATION( page ), NULL );

	return( GTK_WIDGET( OFA_RECONCILIATION( page )->priv->tview ));
}

/*
 * the treeview is invalidated (insensitive) while the account is not ok
 */
static void
on_account_entry_changed( GtkEntry *entry, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	gdouble debit, credit;
	gchar *str, *msg;

	priv = self->priv;

	clear_account_properties( self );
	priv->account = get_reconciliable_account( self );

	if( priv->account ){
		debit = ofo_account_get_val_debit( priv->account )
				+ ofo_account_get_rough_debit( priv->account );
		credit = ofo_account_get_val_credit( priv->account )
				+ ofo_account_get_rough_credit( priv->account );

		if( credit >= debit ){
			str = my_double_to_str( credit - debit );
			msg = g_strdup_printf( _( "%s CR" ), str );
			gtk_label_set_text( priv->account_credit, msg );
			g_free( str );
			g_free( msg );

		} else {
			str = my_double_to_str( debit - credit );
			msg = g_strdup_printf( _( "%s DB" ), str );
			gtk_label_set_text( priv->account_debit, msg );
			g_free( str );
			g_free( msg );
		}

		/* automatically fetch entries */
		/* clear the store and insert the entry rows */
		do_fetch_entries( self );

		if( priv->bat ){
			/* redisplay the current bat lines */
			display_bat_lines( self );
			default_expand_view( self );
		}

		/* setup reconciliated balance */
		set_reconciliated_balance( self );
	}

	check_for_enable_view( self );
}

static void
on_account_select_clicked( GtkButton *button, ofaReconciliation *self )
{
	do_account_selection( self );
}

/*
 * setting the entry (gtk_entry_set_text) will trigger a 'changed'
 * message, which itself will update the account properties in the
 * dialog
 */
static void
do_account_selection( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	const gchar *account_number;
	gchar *number;

	priv = self->priv;

	account_number = gtk_entry_get_text( priv->account_entry );
	if( !my_strlen( account_number )){
		account_number = st_default_reconciliated_class;
	}

	number = ofa_account_select_run(
					ofa_page_get_main_window( OFA_PAGE( self )),
					account_number,
					OFA_ALLOW_RECONCILIABLE );

	if( my_strlen( number )){
		gtk_entry_set_text( priv->account_entry, number );
	}

	g_free( number );
}

static void
clear_account_properties( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;

	priv = self->priv;

	gtk_label_set_text( priv->account_label, "" );
	gtk_label_set_text( priv->account_debit, "" );
	gtk_label_set_text( priv->account_credit, "" );

	if( priv->tview ){
		gtk_widget_set_sensitive( GTK_WIDGET( priv->tview ), FALSE );
	}
	if( priv->print_btn ){
		gtk_widget_set_sensitive( priv->print_btn, FALSE );
	}

	priv->account = NULL;
	set_reconciliated_balance( self );
}

/*
 * get the account addressed by the account selection entry
 * display its label
 * returning it if valid for reconciliation
 */
static ofoAccount *
get_reconciliable_account( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	const gchar *number;
	gboolean ok;
	ofoAccount *account;
	const gchar *label;
	gchar *str;
	GdkRGBA color;

	priv = self->priv;
	ok = FALSE;

	number = gtk_entry_get_text( priv->account_entry );
	account = ofo_account_get_by_number(
						ofa_page_get_dossier( OFA_PAGE( self )), number );

	if( account ){
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );
		label = ofo_account_get_label( account );

		ok = !ofo_account_is_root( account ) &&
				!ofo_account_is_closed( account ) &&
				ofo_account_is_reconciliable( account );

		if( ok ){
			str = g_strdup( label );
			gdk_rgba_parse( &color, COLOR_LABEL_NORMAL );

		} else {
			str = g_strdup_printf( "<i>%s</i>", label );
			gdk_rgba_parse( &color, COLOR_LABEL_INVALID );
		}

		gtk_label_set_markup( priv->account_label, str );
		g_free( str );
		gtk_widget_override_color( GTK_WIDGET( priv->account_label ), GTK_STATE_FLAG_NORMAL, &color );
	}

	set_settings( self );

	return( ok ? account : NULL );
}

static void
do_fetch_entries( ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_do_fetch_entries";
	ofaReconciliationPrivate *priv;
	GList *entries, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = self->priv;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	g_debug( "%s: clearing treestore=%p", thisfn, ( void * ) tmodel );
	gtk_tree_store_clear( GTK_TREE_STORE( tmodel ));

	entries = ofo_entry_get_dataset_by_account(
					ofa_page_get_dossier( OFA_PAGE( self )),
					ofo_account_get_number( priv->account ));

	for( it=entries ; it ; it=it->next ){
		insert_entry( self, tmodel, OFO_ENTRY( it->data ), &iter );
	}

	ofo_entry_free_dataset( entries );
}

/*
 * @iter: [out]: set to the newly inserted row
 *
 * inserting an entry automatically adds to the global list the
 * reconciliation group for this entry (if any)
 */
static void
insert_entry( ofaReconciliation *self, GtkTreeModel *tstore, ofoEntry *entry, GtkTreeIter *iter )
{
	gtk_tree_store_insert( GTK_TREE_STORE( tstore ), iter, NULL, -1 );
	set_row_entry( self, tstore, iter, entry );
}

static void
set_row_entry( ofaReconciliation *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry )
{
	ofxAmount amount;
	gchar *sdope, *sdeb, *scre, *sdrap;
	const GDate *dconcil;
	ofoConcil *concil;

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_display());
	amount = ofo_entry_get_debit( entry );
	if( amount ){
		sdeb = my_double_to_str( amount );
	} else {
		sdeb = g_strdup( "" );
	}
	amount = ofo_entry_get_credit( entry );
	if( amount ){
		scre = my_double_to_str( amount );
	} else {
		scre = g_strdup( "" );
	}
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), ofa_page_get_dossier( OFA_PAGE( self )));
	dconcil = concil ? ofo_concil_get_dval( concil ) : NULL;
	sdrap = my_date_to_str( dconcil, ofa_prefs_date_display());

	gtk_tree_store_set(
			GTK_TREE_STORE( tstore ),
			iter,
			COL_ACCOUNT,   ofo_entry_get_account( entry ),
			COL_DOPE,      sdope,
			COL_PIECE,     ofo_entry_get_ref( entry ),
			COL_NUMBER,    ofo_entry_get_number( entry ),
			COL_LABEL,     ofo_entry_get_label( entry ),
			COL_DEBIT,     sdeb,
			COL_CREDIT,    scre,
			COL_DRECONCIL, sdrap,
			COL_OBJECT,    entry,
			-1 );

	g_free( sdope );
	g_free( sdeb );
	g_free( scre );
	g_free( sdrap );
}

static void
on_mode_combo_changed( GtkComboBox *box, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	priv = self->priv;
	priv->mode = -1;

	if( gtk_combo_box_get_active_iter( priv->mode_combo, &iter )){
		tmodel = gtk_combo_box_get_model( priv->mode_combo );
		gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &priv->mode, -1 );
	}

	if( check_for_enable_view( self )){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	}

	set_settings( self );
}

/*
 * called when reading the settings
 */
static void
select_mode( ofaReconciliation *self, gint mode )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint box_mode;

	priv = self->priv;

	tmodel = gtk_combo_box_get_model( priv->mode_combo );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &box_mode, -1 );
			if( box_mode == mode ){
				gtk_combo_box_set_active_iter( priv->mode_combo, &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/*
 * effect dates filter are not stored in settings
 */
static void
on_effect_dates_changed( ofaDateFilterBin *filter, gint who, const GDate *date, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;

	priv = self->priv;

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
}

/*
 * modifying the manual reconciliation date
 */
static void
on_date_concil_changed( GtkEditable *editable, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GDate date;
	gboolean valid;

	priv = self->priv;

	my_date_set_from_date( &date, my_editable_date_get_date( editable, &valid ));
	if( valid ){
		my_date_set_from_date( &priv->dconcil, &date );
	}

	set_settings( self );
}

/*
 * select an already imported Bank Account Transaction list file
 */
static void
on_select_bat( GtkButton *button, ofaReconciliation *self )
{
	do_select_bat( self );
}

/*
 * select an already imported Bank Account Transaction list file
 *
 * Only reset the BAT lines id we try to load another BAT file
 * Hitting Cancel on BAT selection doesn't change anything
 */
static void
do_select_bat( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	ofxCounter prev_id, bat_id;

	priv = self->priv;
	prev_id = priv->bat ? ofo_bat_get_id( priv->bat ) : -1;
	bat_id = ofa_bat_select_run( ofa_page_get_main_window( OFA_PAGE( self )), prev_id );

	if( bat_id > 0 ){
		clear_bat_file( self );
		setup_bat_file( self, bat_id );
	}
}

/*
 * try to import a bank account transaction list
 *
 * As coming here means that the user has selected a file, then we
 * begin with clearing the current bat lines (even if the import may
 * be unsuccessful)
 */
static void
on_file_set( GtkFileChooserButton *button, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_file_set";
	ofaReconciliationPrivate *priv;
	ofaFileFormat *settings;
	ofaIImportable *importable;
	ofxCounter *imported_id;
	gchar *uri;

	priv = self->priv;

	settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
	ofa_file_format_set( settings,
			NULL, OFA_FFTYPE_OTHER, OFA_FFMODE_IMPORT, "UTF-8", 0, ',', ' ', 0 );

	/* take the uri before clearing bat lines */
	uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( button ));

	clear_bat_file( self );
	importable = ofa_iimportable_find_willing_to( uri, settings );

	if( importable ){
		if( !ofa_iimportable_import_uri( importable, priv->dossier, NULL, ( void ** ) &imported_id )){
			setup_bat_file( self, *imported_id );
			g_free( imported_id );
		}
		g_debug( "%s: importable=%p (%s) ref_count=%d",
				thisfn, ( void * ) importable,
				G_OBJECT_TYPE_NAME( importable ), G_OBJECT( importable )->ref_count );
		g_object_unref( importable );
	}

	g_free( uri );
	g_object_unref( settings );
}

static void
on_clear_button_clicked( GtkButton *button, ofaReconciliation *self )
{
	clear_bat_file( self );
}

/*
 *  clear the proposed reconciliations from the model before displaying
 *  the just imported new ones
 *
 *  this means not only removing old BAT lines, but also resetting the
 *  proposed reconciliation date in the entries
 */
static void
clear_bat_file( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *store;
	ofoConcil *concil;
	GObject *object;
	GtkTreeIter iter, child_iter;

	priv = self->priv;
	store = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	if( gtk_tree_model_get_iter_first( store, &iter )){
		while( TRUE ){
			gtk_tree_model_get( store, &iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
			g_object_unref( object );

			if( OFO_IS_ENTRY( object )){
				concil = ofa_iconcil_get_concil(
						OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
				if( !concil ){
					gtk_tree_store_set( GTK_TREE_STORE( store ), &iter, COL_DRECONCIL, "", -1 );
				}

				if( gtk_tree_model_iter_has_child( store, &iter ) &&
						 gtk_tree_model_iter_children( store, &child_iter, &iter )){
					gtk_tree_store_remove( GTK_TREE_STORE( store ), &child_iter );
				}

				if( !gtk_tree_model_iter_next( store, &iter )){
					break;
				}

			} else {
				g_return_if_fail( OFO_IS_BAT_LINE( object ));
				if( !gtk_tree_store_remove( GTK_TREE_STORE( store ), &iter )){
					break;
				}
			}
		}
	}

	ofo_bat_line_free_dataset( priv->batlines );
	priv->batlines = NULL;
	priv->bat = NULL;

	/* also reinit the GtkFileChooserButton and the corresponding label */
	gtk_file_chooser_unselect_all( GTK_FILE_CHOOSER( priv->file_chooser ));
	gtk_label_set_text( GTK_LABEL( priv->count_label ), "" );

	/* and update the bank reconciliated balance */
	set_reconciliated_balance( self );
}

/*
 * makes use of a Bank Account Transaction (BAT) list
 * whether is has just been imported, or it is reloaded from sgbd
 */
static void
setup_bat_file( ofaReconciliation *self, ofxCounter bat_id )
{
	ofaReconciliationPrivate *priv;

	priv = self->priv;

	priv->bat = ofo_bat_get_by_id( ofa_page_get_dossier( OFA_PAGE( self )), bat_id );
	priv->batlines = ofo_bat_line_get_dataset( ofa_page_get_dossier( OFA_PAGE( self )), bat_id );

	display_bat_lines( self );
	default_expand_view( self );

	gtk_file_chooser_set_uri( GTK_FILE_CHOOSER( priv->file_chooser ), ofo_bat_get_uri( priv->bat ));
	display_bat_count( self );

	set_reconciliated_balance( self );
}

/*
 * have just loaded a new set of imported BAT lines
 * try to automatize a proposition of reconciliation
 *
 * for each BAT line,
 * - it has already been used to reconciliate an entry, then set the bat
 *   line alongside with the (first) corresponding entry
 * - else search for an entry with compatible (same, though inversed)
 *   amount, and not yet reconciliated
 * - else just add the bat line without parent
 */
static void
display_bat_lines( ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_display_bat_lines";
	ofaReconciliationPrivate *priv;
	GList *line;
	ofoBatLine *batline;
	gboolean done;
	gdouble bat_amount;
	gchar *sbat_deb, *sbat_cre, *bat_sdval;
	GtkTreeModel *store_model;
	GtkTreeIter *entry_iter;
	ofoConcil *concil;
	GList *ids;
	ofxCounter ent_number;

	priv = self->priv;
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	for( line=priv->batlines ; line ; line=line->next ){

		batline = OFO_BAT_LINE( line->data );
		done = FALSE;

		bat_amount = ofo_bat_line_get_amount( batline );
		if( bat_amount < 0 ){
			sbat_deb = my_double_to_str( -bat_amount );
			sbat_cre = g_strdup( "" );
		} else {
			sbat_deb = g_strdup( "" );
			sbat_cre = my_double_to_str( bat_amount );
		}

		if( 0 ){
			bat_sdval = my_date_to_str( ofo_bat_line_get_deffect( batline ), ofa_prefs_date_display());
			g_debug( "%s: batline: label=%s, dval=%s, sdeb=%s, scre=%s",
					thisfn, ofo_bat_line_get_label( batline ),
					bat_sdval, sbat_deb, sbat_cre );
			g_free( bat_sdval );
		}

		/* try to insert the batline under an entry it has been
		 * reconciliated aginst; this entry may not be found here if:
		 * - the batline is not yet reconciliated
		 * - or the batline has been reconciliated against another batline
		 * - or the batline is associated to another account */
		concil = ofa_iconcil_get_concil(
				OFA_ICONCIL( batline ), ofa_page_get_dossier( OFA_PAGE( self )));
		g_debug( "%s: label=%s, concil=%p", thisfn, ofo_bat_line_get_label( batline ), ( void * ) concil );
		if( concil ){
			ids = ofo_concil_get_ids( concil );
			ent_number = ofs_concil_id_get_first( ids, CONCIL_TYPE_ENTRY );
			g_debug( "%s: ent_number=%ld", thisfn, ent_number );
			if( ent_number > 0 ){
				entry_iter = search_for_entry_by_number( self, ent_number );
				if( entry_iter ){
					insert_bat_line( self, batline, entry_iter, sbat_deb, sbat_cre );
					gtk_tree_iter_free( entry_iter );
					done = TRUE;
				} else {
					g_debug( "%s: entry %ld not found by number", thisfn, ent_number );
				}
			}

		/* try for an automatic proposition if the batline is not yet
		 * reconciliated (is not member of any reconciliation group) */
		} else {
			entry_iter = search_for_entry_by_amount( self, sbat_deb, sbat_cre );
			if( entry_iter ){
				set_entry_dval_by_store_iter( self, ofo_bat_line_get_deffect( batline ), store_model, entry_iter);
				insert_bat_line( self, batline, entry_iter, sbat_deb, sbat_cre );
				gtk_tree_iter_free( entry_iter );
				done = TRUE;
			}
		}

		/* last just insert the bat line at level zero */
		if( !done ){
			insert_bat_line( self, batline, NULL, sbat_deb, sbat_cre );
		}

		g_free( sbat_deb );
		g_free( sbat_cre );
	}
}

static void
display_bat_count( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	ofoDossier *dossier;
	gchar *str;
	gint total, used;

	priv = self->priv;
	dossier = ofa_page_get_dossier( OFA_PAGE( self ));

	total = ofo_bat_get_lines_count( priv->bat, dossier );
	str = g_strdup_printf( "%u", total );
	gtk_label_set_text( GTK_LABEL( priv->count_label ), str );
	g_free( str );

	used = ofo_bat_get_used_count( priv->bat, dossier );
	str = g_markup_printf_escaped( "<i>%u</i>", total-used );
	gtk_label_set_markup( GTK_LABEL( priv->unused_label ), str );
	g_free( str );
}

/*
 * sorting the treeview
 *
 * sort the visible rows (entries as parent, and bat lines as children)
 * by operation date + entry number (entries only)
 *
 * for bat lines, operation date may be set to effect date (valeur) if
 * not provided in the bat file; entry number is set to zero
 *
 * We are only sorting the root lines of the treeview, but these root
 * lines may be entries or unreconciliated bat lines
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_sort_model";
	static const gchar *empty = "";
	ofaReconciliationPrivate *priv;
	gint cmp, sort_column_id;
	GtkSortType sort_order;
	ofoBase *object_a, *object_b;
	const GDate *date_a, *date_b;
	const gchar *str_a, *str_b;
	gdouble amount, amount_a, amount_b;
	gint int_a, int_b;
	ofoConcil *concil_a, *concil_b;

	cmp = 0;
	priv = self->priv;

	gtk_tree_model_get( tmodel, a, COL_OBJECT, &object_a, -1 );
	g_return_val_if_fail( object_a && ( OFO_IS_ENTRY( object_a ) || OFO_IS_BAT_LINE( object_a )), 0 );
	g_object_unref( object_a );

	gtk_tree_model_get( tmodel, b, COL_OBJECT, &object_b, -1 );
	g_return_val_if_fail( object_b && ( OFO_IS_ENTRY( object_b ) || OFO_IS_BAT_LINE( object_b )), 0 );
	g_object_unref( object_b );

	gtk_tree_sortable_get_sort_column_id(
			GTK_TREE_SORTABLE( priv->tsort ), &sort_column_id, &sort_order );

	switch( sort_column_id ){
		case COL_DOPE:
			date_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_dope( OFO_ENTRY( object_a )) :
							get_bat_line_dope( self, OFO_BAT_LINE( object_a ));
			date_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_dope( OFO_ENTRY( object_b )) :
						get_bat_line_dope( self, OFO_BAT_LINE( object_b ));
			cmp = my_date_compare( date_a, date_b );
			break;
		case COL_PIECE:
			str_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_ref( OFO_ENTRY( object_a )) : empty;
			str_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_ref( OFO_ENTRY( object_b )) : empty;
			cmp = str_a && str_b ? g_utf8_collate( str_a, str_b ) : ( str_a ? 1 : ( str_b ? -1 : 0 ));
			break;
		case COL_NUMBER:
			int_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_number( OFO_ENTRY( object_a )) :
							ofo_bat_line_get_line_id( OFO_BAT_LINE( object_a ));
			int_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_number( OFO_ENTRY( object_b )) :
							ofo_bat_line_get_line_id( OFO_BAT_LINE( object_b ));
			cmp = int_a > int_b ? 1 : ( int_a < int_b ? -1 : 0 );
			break;
		case COL_LABEL:
			str_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_label( OFO_ENTRY( object_a )) :
							ofo_bat_line_get_label( OFO_BAT_LINE( object_a ));
			str_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_label( OFO_ENTRY( object_b )) :
							ofo_bat_line_get_label( OFO_BAT_LINE( object_b ));
			cmp = g_utf8_collate( str_a, str_b );
			break;
		case COL_DEBIT:
			if( OFO_IS_BAT_LINE( object_a )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_a ));
				amount_a = amount < 0 ? -amount : 0;
			} else {
				amount_a = ofo_entry_get_debit( OFO_ENTRY( object_a ));
			}
			if( OFO_IS_BAT_LINE( object_b )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_b ));
				amount_b = amount < 0 ? -amount : 0;
			} else {
				amount_b = ofo_entry_get_debit( OFO_ENTRY( object_b ));
			}
			cmp = amount_a > amount_b ? 1 : ( amount_a < amount_b ? -1 : 0 );
			break;
		case COL_CREDIT:
			if( OFO_IS_BAT_LINE( object_a )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_a ));
				amount_a = amount < 0 ? 0 : amount;
			} else {
				amount_a = ofo_entry_get_credit( OFO_ENTRY( object_a ));
			}
			if( OFO_IS_BAT_LINE( object_b )){
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object_b ));
				amount_b = amount < 0 ? 0 : amount;
			} else {
				amount_b = ofo_entry_get_credit( OFO_ENTRY( object_b ));
			}
			cmp = amount_a > amount_b ? 1 : ( amount_a < amount_b ? -1 : 0 );
			break;
		case COL_DRECONCIL:
			concil_a = OFO_IS_ENTRY( object_a ) ?
					ofa_iconcil_get_concil(
							OFA_ICONCIL( object_a ), ofa_page_get_dossier( OFA_PAGE( self ))) :
						NULL;
			date_a = concil_a ? ofo_concil_get_dval( concil_a ) : NULL;
			concil_b = OFO_IS_ENTRY( object_b ) ?
					ofa_iconcil_get_concil(
							OFA_ICONCIL( object_b ), ofa_page_get_dossier( OFA_PAGE( self ))) :
					NULL;
			date_b = concil_b ? ofo_concil_get_dval( concil_b ) : NULL;
			g_return_val_if_fail( my_date_is_valid( date_a ), 0 );
			g_return_val_if_fail( my_date_is_valid( date_b ), 0 );
			cmp = my_date_compare( date_a, date_b );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, sort_column_id );
			break;
	}

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
on_header_clicked( GtkTreeViewColumn *column, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_header_clicked";
	ofaReconciliationPrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;

	priv = self->priv;

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( priv->tsort ), &sort_column_id, &sort_order );

	g_debug( "%s: current sort_column_id=%u, sort_order=%s",
			thisfn, sort_column_id,
			sort_order == OFA_SORT_ASCENDING ? "OFA_SORT_ASCENDING":"OFA_SORT_DESCENDING" );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );

	/*if( new_column_id == sort_column_id ){
		if( sort_order == GTK_SORT_ASCENDING ){
			sort_order = GTK_SORT_DESCENDING;
		} else {
			sort_order = GTK_SORT_ASCENDING;
		}
	} else {
		sort_order = GTK_SORT_ASCENDING;
	}*/

	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( priv->tsort ), new_column_id, sort_order );

	g_debug( "%s: setting new_column_id=%u, new_sort_order=%s",
			thisfn, new_column_id,
			sort_order == OFA_SORT_ASCENDING ? "OFA_SORT_ASCENDING":"OFA_SORT_DESCENDING" );
}

/*
 * a row is visible if it is consistant with the selected mode:
 * - entry: vs. the selected mode
 * - bat line: vs. the reconciliation status:
 *   > reconciliated (and validated): invisible
 *   > not reconciliated (or not validated): visible
 *
 * tmodel here is the main GtkTreeModelSort on which the view is built
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	gboolean visible, ok;
	GObject *object;
	const GDate *deffect, *filter;

	priv = self->priv;
	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );

	/* as we insert the row before populating it, it may happen that
	 * the object be not set */
	if( !object ){
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object ), TRUE );
	g_object_unref( object );

	visible = OFO_IS_ENTRY( object ) ?
			is_visible_entry( self, tmodel, iter, OFO_ENTRY( object )) :
			is_visible_batline( self, OFO_BAT_LINE( object ));

	if( visible ){
		/* check against effect dates filter */
		deffect = OFO_IS_ENTRY( object ) ?
				ofo_entry_get_deffect( OFO_ENTRY( object )) :
				ofo_bat_line_get_deffect( OFO_BAT_LINE( object ));
		g_return_val_if_fail( my_date_is_valid( deffect ), FALSE );
		/* ... against lower limit */
		filter = ofa_date_filter_bin_get_from( priv->effect_filter );
		ok = !my_date_is_valid( filter ) ||
				my_date_compare( filter, deffect ) <= 0;
		visible &= ok;
		/* ... against upper limit */
		filter = ofa_date_filter_bin_get_to( priv->effect_filter );
		ok = !my_date_is_valid( filter ) ||
				my_date_compare( filter, deffect ) >= 0;
		visible &= ok;
	}

	return( visible );
}

static gboolean
is_visible_entry( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	ofaReconciliationPrivate *priv;
	gboolean visible;
	ofoConcil *concil;
	gint mode;
	const gchar *selected_account, *entry_account;

	priv = self->priv;

	/* do not display deleted entries */
	if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
		return( FALSE );
	}

	/* check account is right
	 * do not rely on the initial dataset query as we may have inserted
	 *  a new entry */
	selected_account = gtk_entry_get_text( priv->account_entry );
	entry_account = ofo_entry_get_account( entry );
	if( g_utf8_collate( selected_account, entry_account )){
		return( FALSE );
	}

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), ofa_page_get_dossier( OFA_PAGE( self )));
	mode = priv->mode;
	visible = TRUE;

	switch( mode ){
		case ENT_CONCILED_ALL:
			/*g_debug( "%s: mode=%d, visible=True", thisfn, mode );*/
			break;
		case ENT_CONCILED_YES:
			visible = ( concil != NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_NO:
			visible = ( concil == NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_SESSION:
			if( concil ){
				visible = is_entry_session_conciliated( self, entry, concil );
			} else {
				visible = TRUE;
			}
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		default:
			/* when display mode is not set */
			visible = FALSE;
	}

	return( visible );
}

/*
 * bat lines are visible with the same criteria that the entries
 * even when reconciliated, it keeps to be displayed besides of its
 * entry
 */
static gboolean
is_visible_batline( ofaReconciliation *self, ofoBatLine *batline )
{
	ofaReconciliationPrivate *priv;
	gint mode;
	gboolean visible;
	ofoConcil *concil;
	GList *ids;
	ofxCounter number;
	GtkTreeIter *iter;
	GtkTreeModel *tstore;
	ofoEntry *entry;

	priv = self->priv;
	visible = FALSE;
	mode = priv->mode;
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ), ofa_page_get_dossier( OFA_PAGE( self )));

	switch( mode ){
		case ENT_CONCILED_ALL:
			/*g_debug( "%s: mode=%d, visible=True", thisfn, mode );*/
			visible = TRUE;
			break;
		case ENT_CONCILED_YES:
			visible = ( concil != NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_NO:
			visible = ( concil == NULL );
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		case ENT_CONCILED_SESSION:
			if( !concil ){
				visible = TRUE;
			} else {
				ids = ofo_concil_get_ids( concil );
				number = ofs_concil_id_get_first( ids, CONCIL_TYPE_ENTRY );
				iter = search_for_entry_by_number( self, number );
				if( iter ){
					tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
					gtk_tree_model_get( tstore, iter, COL_OBJECT, &entry, -1 );
					g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
					g_object_unref( entry );
					gtk_tree_iter_free( iter );
					visible = is_entry_session_conciliated( self, entry, NULL );
				}
			}
			/*g_debug( "%s: mode=%d, visible=%s", thisfn, mode, visible ? "True":"False" );*/
			break;
		default:
			/* when display mode is not set */
			break;
	}

	return( visible );
}

static gboolean
is_entry_session_conciliated( ofaReconciliation *self, ofoEntry *entry, ofoConcil *concil )
{
	gboolean is_session;
	const GTimeVal *stamp;
	GDate date, dnow;

	if( !concil ){
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ), ofa_page_get_dossier( OFA_PAGE( self )));
	}
	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );

	stamp = ofo_concil_get_stamp( concil );
	my_date_set_from_stamp( &date, stamp );
	my_date_set_now( &dnow );

	is_session = my_date_compare( &date, &dnow ) == 0;

	return( is_session );
}

/*
 * row       foreground  style   background
 * --------  ----------  ------  ----------
 * entry     normal      normal  normal
 * bat line  BAT_COLOR   italic  normal
 * proposal  normal      italic  BAT_BACKGROUND
 *
 * BAT lines are always displayed besides of their corresponding entry
 */
static void
on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaReconciliation *self )
{
	GObject *object;
	GdkRGBA color;
	gint id;
	ofoConcil *concil;

	g_return_if_fail( GTK_IS_CELL_RENDERER_TEXT( cell ));

	gtk_tree_model_get( tmodel, iter,
			COL_OBJECT, &object,
			-1 );
	g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
	g_object_unref( object );

	g_object_set( G_OBJECT( cell ),
						"style-set",      FALSE,
						"foreground-set", FALSE,
						"background-set", FALSE,
						NULL );

	if( OFO_IS_ENTRY( object )){
		id = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( tcolumn ), DATA_COLUMN_ID ));
		concil =  ofa_iconcil_get_concil(
				OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
		if( !concil &&
				id == COL_DRECONCIL &&
				gtk_tree_model_iter_has_child( tmodel, iter )){

			gdk_rgba_parse( &color, COLOR_BAT_UNCONCIL_BACKGROUND );
			g_object_set( G_OBJECT( cell ), "background-rgba", &color, NULL );
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
		}

	/* bat lines (normal if reconciliated, italic else */
	} else {
		concil = ofa_iconcil_get_concil(
				OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
		if( concil ){
			gdk_rgba_parse( &color, COLOR_BAT_CONCIL_FONT );
		} else {
			gdk_rgba_parse( &color, COLOR_BAT_UNCONCIL_FONT );
			g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
		}
		g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
	}
}

/*
 * - accept is enabled if selected lines are compatible:
 *   i.e. as soon as sum(debit)=sum(credit)
 *
 * - decline is enabled if we have only one batline
 */
static void
on_tview_selection_changed( GtkTreeSelection *select, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GList *selected, *it;
	GtkTreeModel *tmodel;
	gint count;
	GtkTreeIter iter, parent_iter;
	ofoBase *object;
	gboolean has_parent;
	gboolean accept_enabled, decline_enabled;
	ofxAmount tot_debit, tot_credit, amount;
	ofoConcil *concil;

	priv = self->priv;
	accept_enabled = FALSE;
	decline_enabled = FALSE;
	selected = gtk_tree_selection_get_selected_rows( select, &tmodel );
	count = g_list_length( selected );

	if( count == 1 ){
		if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) selected->data )){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
			g_object_unref( object );

			if( OFO_IS_BAT_LINE( object )){
				/* is the bat line member of a reconciliation group ? */
				concil = ofa_iconcil_get_concil(
						OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
				has_parent = gtk_tree_model_iter_parent( tmodel, &parent_iter, &iter );
				/* decline to use a bat line against a non-yet reconciliated
				 *  entry */
				decline_enabled = ( !concil && has_parent );
			}
		}

	} else if( count > 1 ){
		tot_debit = 0;
		tot_credit = 0;
		for( it=selected ; it ; it=it->next ){
			if( gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) it->data )){
				gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
				g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
				g_object_unref( object );

				if( OFO_IS_ENTRY( object )){
					tot_debit += ofo_entry_get_debit( OFO_ENTRY( object ));
					tot_credit += ofo_entry_get_credit( OFO_ENTRY( object ));
				} else {
					amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
					if( amount < 0 ){
						tot_debit += -1*amount;
					} else {
						tot_credit += amount;
					}
				}
			}
		}
		if( tot_debit == tot_credit ){
			accept_enabled = TRUE;
		}
	}

	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );

	gtk_widget_set_sensitive( priv->accept_btn, accept_enabled );
	gtk_widget_set_sensitive( priv->decline_btn, decline_enabled );
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_row_activated";

	g_debug( "%s: view=%p, path=%p, column=%p, self=%p",
			thisfn, ( void * ) view, ( void * ) path, ( void * ) column, ( void * ) self );

	run_selection_engine( self );
#if 0
	if( toggle_rappro( self )){
		if( priv->bat ){
			display_bat_count( self );
		}
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		default_expand_view( self );
		set_reconciliated_balance( self );
	}
#endif
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * Handles left and right arrows to expand/collapse nodes
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaReconciliation *self )
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
collapse_node( ofaReconciliation *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GList *selected;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		selected = gtk_tree_selection_get_selected_rows( select, &tmodel );
		if( g_list_length( selected ) == 1 &&
				gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) selected->data )){
			collapse_node_by_iter( self, GTK_TREE_VIEW( widget ), tmodel, &iter );
		}
		g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
	}
}

static void
collapse_node_by_iter( ofaReconciliation *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreePath *path;
	GtkTreeIter parent_iter;

	if( gtk_tree_model_iter_has_child( tmodel, iter )){
		path = gtk_tree_model_get_path( tmodel, iter );
		gtk_tree_view_collapse_row( tview, path );
		gtk_tree_path_free( path );

	} else if( gtk_tree_model_iter_parent( tmodel, &parent_iter, iter )){
		path = gtk_tree_model_get_path( tmodel, &parent_iter );
		gtk_tree_view_collapse_row( tview, path );
		gtk_tree_path_free( path );
	}
}

static void
expand_node( ofaReconciliation *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GList *selected;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		selected = gtk_tree_selection_get_selected_rows( select, &tmodel );
		if( g_list_length( selected ) == 1 &&
				gtk_tree_model_get_iter( tmodel, &iter, ( GtkTreePath * ) selected->data )){
			expand_node_by_iter( self, GTK_TREE_VIEW( widget ), tmodel, &iter );
		}
		g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
	}
}

static void
expand_node_by_iter( ofaReconciliation *self, GtkTreeView *tview, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	GtkTreePath *path;

	if( gtk_tree_model_iter_has_child( tmodel, iter )){
		path = gtk_tree_model_get_path( tmodel, iter );
		gtk_tree_view_expand_row( tview, path, FALSE );
		gtk_tree_path_free( path );
	}
}

/*
 * the view is disabled (insensitive) each time the configuration parms
 * are not valid (invalid account or invalid reconciliation display
 * mode)
 */
static gboolean
check_for_enable_view( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	gboolean enabled;

	priv = self->priv;

	enabled = priv->account && OFO_IS_ACCOUNT( priv->account );

	enabled &= ( priv->mode >= 1 );

	gtk_widget_set_sensitive( GTK_WIDGET( priv->tview ), enabled );
	gtk_widget_set_sensitive( priv->print_btn, enabled );

	return( enabled );
}

/*
 * default is to expand unreconciliated entries which have a proposed
 * bat line, collapsing the entries for which the proposal has been
 * accepted (and are so reconciliated)
 */
static void
default_expand_view( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoConcil *concil;
	GObject *object;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
			g_object_unref( object );

			/* if an entry which has a child:
			 * - collapse if entry is reconciliated and batline points
			 *   to this same entry
			 * - else expand */
			if( OFO_IS_ENTRY( object )){
				concil = ofa_iconcil_get_concil(
						OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
				if( concil ){
					collapse_node_by_iter( self, priv->tview, tmodel, &iter );
				} else {
					expand_node_by_iter( self, priv->tview, tmodel, &iter );
				}
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/*
 * use cases:
 * - importing a bat file while the corresponding entries have already
 *   been manually reconciliated: accept bat line
 * - the code is not able to automatically propose the bat line against
 *   the right entry
 * - two entries are presented together to the bank, thus having only
 *   one batline for these
 *
 * In all these cases, entry(ies) and bat line must be manually selected
 * together, so that accept may be enabled
 */
static void
on_accept_clicked( GtkButton *button, ofaReconciliation *self )
{
#if 0
	ofaReconciliationPrivate *priv;
	GtkTreeSelection *select;
	GList *selected, *it;
	GtkTreeModel *sort_model, *filter_model, *store_model;
	GtkTreeIter sort_iter, filter_iter, ent_store_iter, bat_store_iter;
	ofoBase *batline, *entry;
	const GDate *dval;
	gchar *bat_sdeb, *bat_scre;
	ofxAmount bat_amount;
	GtkTreePath *path;
	GtkTreeRowReference *row;
	GList *row_refs;

	priv = self->priv;
	g_return_if_fail( priv->bat && OFO_IS_BAT( priv->bat ));

	select = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( select, &sort_model );
	g_return_if_fail( g_list_length( selected ) >= 2 );

	dval = NULL;
	batline = NULL;
	entry = NULL;
	row_refs = NULL;
	filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( sort_model ));
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( filter_model ));

	/* search for the batline among the selection */
	for( it=selected ; it ; it=it->next ){
		if( gtk_tree_model_get_iter( sort_model, &sort_iter, ( GtkTreePath * ) it->data )){
			gtk_tree_model_get( sort_model, &sort_iter, COL_OBJECT, &batline, -1 );
			g_return_if_fail( batline && ( OFO_IS_ENTRY( batline ) || OFO_IS_BAT_LINE( batline )));
			g_object_unref( batline );
			if( OFO_IS_BAT_LINE( batline )){
				gtk_tree_model_sort_convert_iter_to_child_iter(
						GTK_TREE_MODEL_SORT( sort_model ), &filter_iter, &sort_iter );
				gtk_tree_model_filter_convert_iter_to_child_iter(
						GTK_TREE_MODEL_FILTER( filter_model ), &bat_store_iter, &filter_iter );
				dval = ofo_bat_line_get_deffect( OFO_BAT_LINE( batline ));
				break;
			} else {
				row = gtk_tree_row_reference_new( sort_model, ( GtkTreePath * ) it->data );
				row_refs = g_list_prepend( row_refs, row );
			}
		}
	}
	g_return_if_fail( batline && OFO_IS_BAT_LINE( batline ));

	/* other selected rows are entries to be reconciliated against this
	 * batline */
	for( it=row_refs ; it ; it=it->next ){
		path = gtk_tree_row_reference_get_path(( GtkTreeRowReference * ) it->data );
		g_return_if_fail( path );
		if( gtk_tree_model_get_iter( sort_model, &sort_iter, path )){
			gtk_tree_model_get( sort_model, &sort_iter, COL_OBJECT, &entry, -1 );
			g_return_if_fail( entry && OFO_IS_ENTRY( entry ));
			g_object_unref( entry );
			if( OFO_IS_ENTRY( entry )){
				g_debug( "entry=%s", ofo_entry_get_label( OFO_ENTRY( entry )));
				gtk_tree_model_sort_convert_iter_to_child_iter(
						GTK_TREE_MODEL_SORT( sort_model ), &filter_iter, &sort_iter );
				gtk_tree_model_filter_convert_iter_to_child_iter(
						GTK_TREE_MODEL_FILTER( filter_model ), &ent_store_iter, &filter_iter );
				ofo_entry_update_concil(
						OFO_ENTRY( entry ), ofa_page_get_dossier( OFA_PAGE( self )), dval );
				set_entry_dval_by_store_iter( self, &ent_store_iter, dval );
				/*
				ofo_bat_line_add_entry(
						OFO_BAT_LINE( batline ),
						ofa_page_get_dossier( OFA_PAGE( self )),
						ofo_entry_get_number( OFO_ENTRY( entry )));
						*/
			}
		}
		gtk_tree_path_free( path );
	}
	g_return_if_fail( entry && OFO_IS_ENTRY( entry ));

	display_bat_count( self );

	gtk_tree_store_remove( GTK_TREE_STORE( store_model ), &bat_store_iter );
	bat_amount = ofo_bat_line_get_amount( OFO_BAT_LINE( batline ));
	if( bat_amount < 0 ){
		bat_sdeb = my_double_to_str( -bat_amount );
		bat_scre = g_strdup( "" );
	} else {
		bat_sdeb = g_strdup( "" );
		bat_scre = my_double_to_str( bat_amount );
	}
	insert_bat_line( self, OFO_BAT_LINE( batline ), &ent_store_iter, bat_sdeb, bat_scre );
	g_free( bat_sdeb );
	g_free( bat_scre );

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( filter_model ));
	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
	g_list_free_full( row_refs, ( GDestroyNotify ) gtk_tree_row_reference_free );

	gtk_tree_selection_unselect_all( select );
	path = gtk_tree_model_get_path( store_model, &ent_store_iter );
	gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
	gtk_tree_path_free( path );
	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
#endif
	if( run_selection_engine( self )){

	}
}

/*
 * decline a proposition:
 * the bat line is moved from entry child to level 0
 */
static void
on_decline_clicked( GtkButton *button, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GtkTreeSelection *select;
	GList *selected;
	GtkTreeModel *sort_model, *filter_model, *store_model;
	GtkTreeIter sort_iter, filter_iter, store_iter, parent_iter;
	gboolean has_parent, ok;
	GObject *object;
	gchar *sdeb, *scre;
	ofxAmount amount;

	priv = self->priv;
	g_return_if_fail( priv->bat && OFO_IS_BAT( priv->bat ));

	select = gtk_tree_view_get_selection( priv->tview );
	selected = gtk_tree_selection_get_selected_rows( select, &sort_model );
	g_return_if_fail( g_list_length( selected ) == 1 );

	filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( sort_model ));
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( filter_model ));

	ok = gtk_tree_model_get_iter( sort_model, &sort_iter, ( GtkTreePath * ) selected->data );
	g_return_if_fail( ok );
	gtk_tree_model_sort_convert_iter_to_child_iter( GTK_TREE_MODEL_SORT( sort_model ), &filter_iter, &sort_iter );
	gtk_tree_model_filter_convert_iter_to_child_iter( GTK_TREE_MODEL_FILTER( filter_model ), &store_iter, &filter_iter );

	gtk_tree_model_get( store_model, &store_iter, COL_OBJECT, &object, -1 );
	g_return_if_fail( object && OFO_IS_BAT_LINE( object ));
	has_parent = gtk_tree_model_iter_parent( store_model, &parent_iter, &store_iter );
	g_return_if_fail( has_parent );
	/*
	g_return_if_fail( !ofo_bat_line_is_used( OFO_BAT_LINE( object )));
	*/

	gtk_tree_store_remove( GTK_TREE_STORE( store_model ), &store_iter );
	gtk_tree_store_set( GTK_TREE_STORE( store_model ), &parent_iter, COL_DRECONCIL, "", -1 );

	amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
	if( amount < 0 ){
		sdeb = my_double_to_str( -amount );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		scre = my_double_to_str( amount );
	}

	insert_bat_line( self, OFO_BAT_LINE( object ), NULL, sdeb, scre );
	g_object_unref( object );
	g_free( sdeb );
	g_free( scre );

	g_list_free_full( selected, ( GDestroyNotify ) gtk_tree_path_free );
}

/*
 * returns an iter on the store model, or NULL
 *
 * if not %NULL, the returned iter should be gtk_tree_iter_free() by
 * the caller
 */
static GtkTreeIter *
search_for_entry_by_number( ofaReconciliation *self, ofxCounter number )
{
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	ofxCounter ecr_number;
	GObject *object;

	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	if( gtk_tree_model_get_iter_first( child_tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( child_tmodel,
					&iter,
					COL_NUMBER, &ecr_number,
					COL_OBJECT, &object,
					-1 );
			g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
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
 *
 * returns an iter on the store model, or NULL
 *
 * if not %NULL, the returned iter should be gtk_tree_iter_free() by
 * the caller
 */
static GtkTreeIter *
search_for_entry_by_amount( ofaReconciliation *self, const gchar *sbat_deb, const gchar *sbat_cre )
{
	static const gchar *thisfn = "ofa_reconcilitation_search_for_entry_by_amount";
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	gchar *sdeb, *scre, *sdval;
	GObject *object;
	gboolean found;

	found = FALSE;
	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));
	/*g_debug( "search_for_entry_by_amount: sbat_deb=%s, sbat_cre=%s", sbat_deb, sbat_cre );*/

	if( gtk_tree_model_get_iter_first( child_tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( child_tmodel,
					&iter,
					COL_DEBIT,     &sdeb,
					COL_CREDIT,    &scre,
					COL_DRECONCIL, &sdval,
					COL_OBJECT,    &object,
					-1 );
			g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
			g_object_unref( object );

			if( OFO_IS_ENTRY( object ) &&
					!my_strlen( sdval ) &&
					!gtk_tree_model_iter_has_child( child_tmodel, &iter )){

				/* are the amounts compatible ?
				 * a positive bat_amount implies that the entry should be a debit */
				g_return_val_if_fail( g_utf8_strlen( sdeb, -1 ) || g_utf8_strlen( scre, -1 ), NULL );
				/*g_debug( "examining entry: sdeb=%s, scre=%s", sdeb, scre );*/

				if(( my_strlen( scre ) && my_strlen( sbat_deb ) && !g_utf8_collate( scre, sbat_deb )) ||
						( my_strlen( sdeb ) && my_strlen( sbat_cre ) && !g_utf8_collate( sdeb, sbat_cre ))){
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
	if( 0 ){
		g_debug( "%s: returning entry_iter=%p", thisfn, ( void * ) entry_iter );
	}
	return( entry_iter );
}

/*
 * insert the bat line either as a child of entry_iter, or without parent
 * the provided @entry_iter must be on the GtkTreeStore
 */
static void
insert_bat_line( ofaReconciliation *self, ofoBatLine *batline,
							GtkTreeIter *entry_iter, const gchar *sdeb, const gchar *scre )
{
	static const gchar *thisfn = "ofa_reconciliation_insert_bat_line";
	GtkTreeModel *child_tmodel;
	const GDate *dope;
	gchar *sdope, *slabel;
	ofxCounter line_id;
	GtkTreeIter new_iter;

	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	dope = get_bat_line_dope( self, batline );
	sdope = my_date_to_str( dope, ofa_prefs_date_display());

	/* set the bat line as a hint */
	gtk_tree_store_insert_with_values(
				GTK_TREE_STORE( child_tmodel ),
				&new_iter,
				entry_iter,
				-1,
				COL_DOPE,    sdope,
				COL_PIECE,   ofo_bat_line_get_ref( batline ),
				COL_NUMBER,  ofo_bat_line_get_line_id( batline ),
				COL_LABEL,   ofo_bat_line_get_label( batline ),
				COL_DEBIT,   sdeb,
				COL_CREDIT,  scre,
				COL_OBJECT,  batline,
				-1 );

	g_free( sdope );

	/* check for insertion */
	if( 0 ){
		gtk_tree_model_get( child_tmodel, &new_iter, COL_NUMBER, &line_id, COL_LABEL, &slabel, -1 );
		g_debug( "%s: line=%ld, label=%s", thisfn, line_id, slabel );
		g_free( slabel );
	}
}

#if 0
/*
 * Toggle the reconciliation: set a reconciliation date if not yet set,
 * or erase it if already set.
 * The used reconciliation date is preferably taken from the proposal.
 *
 * return TRUE if we have actually reconciliated an entry
 * this prevent us to recompute the account balances if we don't have
 * do anything
 */
static gboolean
toggle_rappro( ofaReconciliation *self, GtkTreePath *path )
{
	ofaReconciliationPrivate *priv;
	GtkTreeIter iter;
	gchar *srappro;
	const GDate *dval;
	GObject *object;
	GDate date;
	gboolean updated;
	GList *entries;

	priv = self->priv;
	updated = FALSE;
	entries = NULL;

	if( gtk_tree_model_get_iter( priv->tsort, &iter, path )){
		gtk_tree_model_get(
				priv->tsort,
				&iter,
				COL_DRECONCIL, &srappro,
				COL_OBJECT,    &object,
				-1 );
		g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
		g_object_unref( object );

		if( OFO_IS_ENTRY( object )){

			/* reconciliation is already set up, so clears it
			 * entry: set reconciliation date to null
			 * bat_line (if exists): set reconciliated entry to null */
			dval = ofo_entry_get_concil_dval( OFO_ENTRY( object ));
			if( my_date_is_valid( dval )){
				if( is_unreconciliate_accepted( self, OFO_ENTRY( object ), &entries )){
					updated = unreconciliate_entries( self, OFO_ENTRY( object ), entries );
				}
				g_list_free_full( entries, ( GDestroyNotify ) g_free );

			/* reconciliation is not set yet, so set it if proposed date is
			 * valid or if we have a proposed reconciliation from imported
			 * BAT */
			} else {
				/* if a proposed date has been set from a bat line */
				my_date_set_from_str( &date, srappro, ofa_prefs_date_display());
				if( my_date_is_valid( &date )){
					updated = reconciliate_entry( self, OFO_ENTRY( object ), &date, &iter );

				/* else try with the manually provided date */
				} else {
					my_date_set_from_date( &date, &priv->dconcil );
					if( my_date_is_valid( &date )){
						updated = reconciliate_entry( self, OFO_ENTRY( object ), &date, &iter );
					}
				}
			}
		}
	}

	return( updated );
}
#endif

/*
 * Run the selection engine on the reconciliation group of the current
 * selection, whether a row has been selected, or the Accept button has
 * been clicked.
 *
 * If the selection is balanced,
 * - create a new reconciliation group, or update the existing one (if any)
 *
 * Else, if we have only one selected entry
 * - if entry is reconciliated,
 *   clear the reconciliation group
 *
 * - else, create a new reconciliation group with this entry
 *   if it has a child, then add the batline to the group
 *   The used reconciliation date is preferably taken from the proposal.
 *
 * return TRUE if we have actually changed the reconciliation state
 * this prevent us to recompute the account balances if we don't have
 * do anything
 */
static gboolean
run_selection_engine( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GList *paths, *obj_concils, *obj_unconcils, *start, *it;
	gboolean updated;
	ofxAmount debit, credit;
	ofxCounter rec_id;
	gint rows_count;
	gboolean unique_concil;
	ofoBase *object;
	ofoConcil *concil;
	ofoDossier *dossier;
	GDate dval;

	priv = self->priv;
	updated = FALSE;
	dossier = ofa_page_get_dossier( OFA_PAGE( self ));
	select = gtk_tree_view_get_selection( priv->tview );
	paths = gtk_tree_selection_get_selected_rows( select, &tmodel );
	examine_selection( self,  tmodel, paths,
			&debit, &credit, &rec_id, &unique_concil, &rows_count, &obj_concils, &obj_unconcils );

	/* if the selection is empty or contains several reconciliation
	 * groups, then does nothing */
	if( rows_count >= 1 && unique_concil ){
		if( rows_count == 1 ){
			object = get_object_by_path( self, tmodel, ( GtkTreePath * ) paths->data );
			if( OFO_IS_ENTRY( object )){
				if( rec_id > 0 ){
					/* unreconciliate the selected entry and all its group */
					concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), dossier );
					if( is_unreconciliate_accepted( self, concil )){
						g_object_ref( concil );
						ofa_iconcil_remove_concil( concil, dossier );
						reset_proposed_dval_from_concil( self, concil );
						g_object_unref( concil );
						updated = TRUE;
					}
				} else {
					/* reconciliate the entry, with its child if any */
					get_entry_dval_by_path( self, &dval, tmodel, ( GtkTreePath * ) paths->data );
					if( my_date_is_valid( &dval )){
						concil = ofa_iconcil_new_concil( OFA_ICONCIL( object ), &dval, dossier );
						set_entry_dval_by_path( self, &dval, tmodel, ( GtkTreePath * ) paths->data );
						add_child_to_concil_by_path( self, concil, tmodel, ( GtkTreePath * ) paths->data );
						updated = TRUE;
					}
				}
			}
		} else if( debit == credit ){
			if( g_list_length( obj_unconcils ) == 0 ){
				/* unreconciliate the fully selected group */
				concil = ofa_iconcil_get_concil( OFA_ICONCIL( obj_concils->data ), dossier );
				if( is_unreconciliate_accepted( self, concil )){
					g_object_ref( concil );
					ofa_iconcil_remove_concil( concil, dossier );
					reset_proposed_dval_from_concil( self, concil );
					g_object_unref( concil );
					updated = TRUE;
				}

			} else {
				/* reconciliate the selected rows (entries/batlines) together */
				concil = NULL;
				start = obj_unconcils;
				if( rec_id == 0 ){
					get_batline_dval_from_objects( self, &dval, obj_unconcils );
					if( my_date_is_valid( &dval )){
						concil = ofa_iconcil_new_concil( OFA_ICONCIL( obj_unconcils->data ), &dval, dossier );
						rec_id = ofo_concil_get_id( concil );
						start = obj_unconcils->next;
						updated = TRUE;
					}
				} else {
					concil = ofa_concil_collection_get_by_id( rec_id, dossier );
				}
				if( rec_id > 0 ){
					g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );
					for( it=start ; it ; it=it->next ){
						ofa_iconcil_add_to_concil( OFA_ICONCIL( it->data ), concil, dossier );
					}
					updated = TRUE;
				}
				if( concil ){
					set_reconciliated_dval_from_concil( self, concil );
				}
			}
		}
	}

	g_list_free( obj_concils );
	g_list_free( obj_unconcils );
	g_list_free_full( paths, ( GDestroyNotify ) gtk_tree_path_free );

	if( updated ){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		set_reconciliated_balance( self );
		if( priv->bat ){
			display_bat_count( self );
		}
	}

	return( updated );
}

/*
 * examine the current selection, gathering the indicators the selection
 * engine needs in order to known what to do
 */
static void
examine_selection( ofaReconciliation *self, GtkTreeModel *tmodel, GList *paths,
		ofxAmount *debit, ofxAmount *credit,
		ofxCounter *recid, gboolean *unique, gint *rowsnb,
		GList **concils, GList **unconcils )
{
	GList *it;
	ofoConcil *concil;
	ofxCounter obj_rec_id;
	ofoBase *object;
	ofxAmount amount;

	*debit = 0;
	*credit = 0;
	*recid = 0;
	*unique = TRUE;
	*rowsnb = 0;
	*concils = NULL;
	*unconcils = NULL;

	for( it=paths ; it ; it=it->next ){
		object = get_object_by_path( self, tmodel, ( GtkTreePath * ) it->data );
		if( object ){
			*rowsnb += 1;

			if( OFO_IS_ENTRY( object )){
				*debit += ofo_entry_get_debit( OFO_ENTRY( object ));
				*credit += ofo_entry_get_credit( OFO_ENTRY( object ));

			} else {
				amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
				if( amount < 0 ){
					*debit += -1*amount;
				} else {
					*credit += amount;
				}
			}
			concil = ofa_iconcil_get_concil(
					OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
			if( concil ){
				obj_rec_id = ofo_concil_get_id( concil );
				if( *recid == 0 ){
					*recid = obj_rec_id;
				} else {
					*unique = FALSE;
				}
				*concils = g_list_prepend( *concils, object );
			} else {
				*unconcils = g_list_prepend( *unconcils, object );
			}
		}
	}
}

/*
 * An entry is about to be unreconciliated
 *
 * Ask for a confirmation if this entry belongs to a reconciliation group
 * (has been reconciliated together against another entry or a batline) as
 * all entries of the group are unreconciliated together
 */
static gboolean
is_unreconciliate_accepted( ofaReconciliation *self, ofoConcil *concil )
{
	gboolean ok;
	GList *ids;
	guint ent_count, bat_count;

	ok = TRUE;
	ids = ofo_concil_get_ids( concil );
	ofs_concil_id_get_count_by_type( ids, &ent_count, &bat_count );
	g_return_val_if_fail( ent_count+bat_count > 0, FALSE );

	if( ent_count > 1 || bat_count > 1 ){

		ok = my_utils_dialog_yesno(
					_( "You are about to unreconciliate a row which belongs "
						"to a reconciliation group.\n"
						"This implies that all entries and BAT lines of this "
						"reconciliation group will be unreconciliated together.\n"
						"Are you sure ?" ),
					_( "_OK" ));
	}

	return( ok );
}

#if 0
/*
 * unreconciliate all rows belonging to the same reconciliation group
 * that the given object
 *
 * Returns: %TRUE if ok
 */
static gboolean
unreconciliate_group( ofaReconciliation *self, ofoConcil *concil )
{
	ofaReconciliationPrivate *priv;
	gboolean ok;
	GList *it;
	ofxCounter *number;
	GtkTreeModel *store_model;
	GtkTreeIter *store_iter;
	ofoEntry *entry_object;

	priv = self->priv;
	ok = TRUE;
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	for( it=entries ; ok && it ; it=it->next ){
		number = ( ofxCounter * ) it->data;
		store_iter = search_for_entry_by_number( self, *number );
		if( store_iter ){
			gtk_tree_model_get( store_model, store_iter, COL_OBJECT, &entry_object, -1 );
			g_return_val_if_fail( entry_object && OFO_IS_ENTRY( entry_object ), FALSE );
			g_object_unref( entry_object );
			ok &= reconciliate_entry_by_store_iter( self, entry_object, NULL, store_iter );
			gtk_tree_iter_free( store_iter );
		}
	}

	return( ok );
	return( FALSE );
}

/*
 * @drappro: may be NULL for clearing a previously set reconciliation
 * @sort_iter: the iter on the entry row in the parent sort model
 *
 * Note on the notations:
 *
 * self->priv->tsort: the sort tree model, on which the view is built
 * self->priv->tfilter: the filter tree model
 * store_tmodel: the deepest underlying tree model which interfaces to GtkTreeStore
 *
 * sort_iter: the entry iter in the displayed sort tree model
 * filter_iter: the entry iter in the child filter tree model
 * store_iter: the entry iter in the child deepest underlying tree model
 * store_bat_iter: the entry's child batline iter in the child tree model
 *
 */
static gboolean
reconciliate_entry( ofaReconciliation *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *sort_iter )
{
	ofaReconciliationPrivate *priv;
	GtkTreeIter filter_iter, store_iter;
	gboolean done;

	priv = self->priv;

	gtk_tree_model_sort_convert_iter_to_child_iter(
			GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, sort_iter );
	gtk_tree_model_filter_convert_iter_to_child_iter(
			GTK_TREE_MODEL_FILTER( priv->tfilter ), &store_iter, &filter_iter );

	done = reconciliate_entry_by_store_iter( self, entry, drappro, &store_iter );

	return( done );
}

/*
 * @drappro: may be NULL for clearing a previously set reconciliation
 * @store_iter: the iter on the entry row in the store model
 */
static gboolean
reconciliate_entry_by_store_iter( ofaReconciliation *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *store_iter )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *store_tmodel;
	GtkTreeIter store_bat_iter;
	ofoBatLine *batline;
	gboolean is_valid_rappro;
	gchar *str;
	const GDate *dope;
	/*
	ofxCounter ent_number;
	*/

	priv = self->priv;
	is_valid_rappro = my_date_is_valid( drappro );
	batline = NULL;
	/*
	ent_number = ofo_entry_get_number( entry );
	*/

	/* update the child bat line if it exists
	 * we work on child model because 'gtk_tree_model_iter_has_child'
	 * actually says if we have a *visible* child
	 * but a batline may be not visible when we are clearing
	 * the reconciliation date */
	store_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	if( gtk_tree_model_iter_has_child( store_tmodel, store_iter ) &&
		gtk_tree_model_iter_children( store_tmodel, &store_bat_iter, store_iter )){

		gtk_tree_model_get(
				store_tmodel,
				&store_bat_iter,
				COL_OBJECT,  &batline,
				-1 );
		g_return_val_if_fail( batline && OFO_IS_BAT_LINE( batline ), FALSE );
		g_object_unref( batline );

#if 0
		if( is_valid_rappro ){
			ofo_bat_line_add_entry( batline,
					ofa_page_get_dossier( OFA_PAGE( self )), ent_number );
		} else {
			ofo_bat_line_remove_entries( batline,
					ofa_page_get_dossier( OFA_PAGE( self )), ent_number );
		}
#endif
	}

	/* update the sgbd, before resetting the new conciliation date
	 * in order to come after the dossier signaling system */
	ofo_entry_update_concil(
			entry,
			ofa_page_get_dossier( OFA_PAGE( self )),
			is_valid_rappro ? drappro : NULL );

	/* update the entry in the tree model with the new reconciliation
	 * date either the actual or back to the proposal if any */
	if( is_valid_rappro ){
		str = my_date_to_str( drappro, ofa_prefs_date_display());

	} else if( batline ){
		dope = get_bat_line_dope( self, batline );
		str = my_date_to_str( dope, ofa_prefs_date_display());

	} else {
		str = g_strdup( "" );
	}

	gtk_tree_store_set(
			GTK_TREE_STORE( store_tmodel ),
			store_iter,
			COL_DRECONCIL, str,
			-1 );

	g_free( str );

	return( TRUE );
}
#endif

/*
 * Compute the corresponding bank account balance, from our own account
 * balance, taking into account unreconciliated entries and (maybe) bat
 * lines
 *
 * Note that we have to iterate on the store model in order to count
 * all rows
 */
static void
set_reconciliated_balance( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	gdouble debit, credit;
	gdouble account_debit, account_credit;
	GtkTreeModel *tstore;
	GtkTreeIter iter;
	GObject *object;
	ofoConcil *concil;
	gdouble amount;
	gchar *str, *sdeb, *scre;

	priv = self->priv;
	debit = 0;
	credit = 0;

	if( priv->account ){
		account_debit = ofo_account_get_val_debit( priv->account )
				+ ofo_account_get_rough_debit( priv->account );
		account_credit = ofo_account_get_val_credit( priv->account )
				+ ofo_account_get_rough_credit( priv->account );
		debit = account_credit;
		credit = account_debit;
		/*g_debug( "initial: debit=%lf, credit=%lf, solde=%lf", debit, credit, debit-credit );*/

		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

		if( gtk_tree_model_get_iter_first( tstore, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tstore, &iter, COL_OBJECT, &object, -1 );
				g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
				g_object_unref( object );

				if( OFO_IS_ENTRY( object )){
					if( ofo_entry_get_status( OFO_ENTRY( object )) != ENT_STATUS_DELETED ){
						concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
						if( !concil ){
							debit += ofo_entry_get_debit( OFO_ENTRY( object ));
							credit += ofo_entry_get_credit( OFO_ENTRY( object ));
							/*g_debug( "label=%s, debit=%lf, credit=%lf, solde=%lf",
									ofo_entry_get_label( OFO_ENTRY( object )), debit, credit, debit-credit );*/
						}
					}
				} else {
					concil = ofa_iconcil_get_concil( OFA_ICONCIL( object ), ofa_page_get_dossier( OFA_PAGE( self )));
					if( !concil ){
						amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
						if( amount < 0 ){
							debit += -amount;
						} else {
							credit += amount;
						}
					}
				}
				if( !gtk_tree_model_iter_next( tstore, &iter )){
					break;
				}
			}
		}
	}

	/*g_debug( "end: debit=%lf, credit=%lf, solde=%lf", debit, credit, debit-credit );*/
	if( debit > credit ){
		str = my_double_to_str( debit-credit );
		sdeb = g_strdup_printf( _( "%s DB" ), str );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		str = my_double_to_str( credit-debit );
		scre = g_strdup_printf( _( "%s CR" ), str );
	}

	gtk_label_set_text( priv->bal_debit, sdeb );
	gtk_label_set_text( priv->bal_credit, scre );

	g_free( str );
	g_free( sdeb );
	g_free( scre );
}

/*
 * search for a valid dval in the first batline from the list of objects
 * defaults to manual reconciliation date
 */
static void
get_batline_dval_from_objects( ofaReconciliation *self, GDate *dval, GList *objects )
{
	ofaReconciliationPrivate *priv;
	GList *it;
	GDate date;
	ofoBase *object;
	gboolean found;

	priv = self->priv;
	my_date_clear( dval );
	found = FALSE;

	for( it=objects ; it ; it=it->next ){
		object = ( ofoBase * ) it->data;
		if( OFO_IS_BAT_LINE( object )){
			my_date_set_from_date( dval, ofo_bat_line_get_deffect( OFO_BAT_LINE( object )));
			found = TRUE;
			break;
		}
	}

	/* else try with the manually provided date */
	if( !found ){
		my_date_set_from_date( &date, &priv->dconcil );
		if( my_date_is_valid( &date )){
			my_date_set_from_date( dval, &date );
		}
	}
}

/*
 * the given path points to an entry
 * search the reconciliation date to be applied, either from a proposed
 * child bat line, or from the manual reconciliation date
 */
static void
get_entry_dval_by_path( ofaReconciliation *self, GDate *dval, GtkTreeModel *tmodel, GtkTreePath *path )
{
	ofaReconciliationPrivate *priv;
	gboolean ok;
	GtkTreeIter iter;
	gchar *sdval;
	GDate date;

	priv = self->priv;
	my_date_clear( dval );

	ok = gtk_tree_model_get_iter( tmodel, &iter, path );
	g_return_if_fail( ok );

	/* if a proposed date has been set from a child bat line */
	gtk_tree_model_get( tmodel, &iter, COL_DRECONCIL, &sdval, -1 );
	my_date_set_from_str( &date, sdval, ofa_prefs_date_display());
	if( my_date_is_valid( &date )){
		my_date_set_from_date( dval, &date );

	/* else try with the manually provided date */
	} else {
		my_date_set_from_date( &date, &priv->dconcil );
		if( my_date_is_valid( &date )){
			my_date_set_from_date( dval, &date );
		}
	}
}

/*
 * the given path points to an entry
 * set the displayed reconciliation date
 */
static void
set_entry_dval_by_path( ofaReconciliation *self, GDate *dval, GtkTreeModel *tmodel, GtkTreePath *path )
{
	GtkTreeModel *store_model;
	GtkTreeIter store_iter;

	get_store_iter_from_path( self, tmodel, path, &store_model, &store_iter );
	set_entry_dval_by_store_iter( self, dval, store_model, &store_iter );
}

/*
 * update the found candidate entry which is not yet reconciliated
 * the provided iter is on the child (store) model
 */
static void
set_entry_dval_by_store_iter( ofaReconciliation *self, const GDate *dval, GtkTreeModel *store_model, GtkTreeIter *store_iter )
{
	gchar *sdvaleur;

	g_return_if_fail( store_model && GTK_IS_TREE_STORE( store_model ));
	g_return_if_fail( store_iter );

	sdvaleur = my_date_to_str( dval, ofa_prefs_date_display());

	/* set the proposed reconciliation date in the entry */
	gtk_tree_store_set(
			GTK_TREE_STORE( store_model ),
			store_iter,
			COL_DRECONCIL, sdvaleur,
			-1 );

	g_free( sdvaleur );
}

/*
 * a group has been reconciliated
 * set the entries dval to those of the conciliation group
 */
static void
set_reconciliated_dval_from_concil( ofaReconciliation *self, ofoConcil *concil )
{
	ofaReconciliationPrivate *priv;
	GList *ids, *it;
	GtkTreeModel *store_model;
	GtkTreeIter *iter;
	ofsConcilId *sid;
	const GDate *dval;

	priv = self->priv;
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	dval = ofo_concil_get_dval( concil );
	ids = ofo_concil_get_ids( concil );

	for( it=ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( !g_utf8_collate( sid->type, CONCIL_TYPE_ENTRY )){
			iter = search_for_entry_by_number( self, sid->other_id );
			if( iter ){
				set_entry_dval_by_store_iter( self, dval, store_model, iter );
				gtk_tree_iter_free( iter );
			}
		}
	}
}

/*
 * a group has been unreconciliated
 * reset the proposed entries dval to a suitable value if any
 *
 * The ofoConcil gives us the list of involved entries
 */
static void
reset_proposed_dval_from_concil( ofaReconciliation *self, ofoConcil *concil )
{
	ofaReconciliationPrivate *priv;
	GList *ids, *it;
	ofsConcilId *sid;
	GtkTreeIter *iter;
	ofoBatLine *batline;
	GtkTreeModel *store_model;

	priv = self->priv;
	ids = ofo_concil_get_ids( concil );
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	for( it=ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( !g_utf8_collate( sid->type, CONCIL_TYPE_ENTRY )){
			iter = search_for_entry_by_number( self, sid->other_id );
			if( iter ){
				batline = get_child_by_store_iter( self, store_model, iter );
				set_entry_dval_by_store_iter( self,
						batline ? ofo_bat_line_get_deffect( batline ) : NULL, store_model, iter );
				gtk_tree_iter_free( iter );
			}
		}
	}
}

/*
 * tmodel, path are sort model and path of the just reconciliated entry
 * add child bat line to the conciliation group if it exists
 *
 * we work on child model because 'gtk_tree_model_iter_has_child'
 * actually says if we have a *visible* child
 * but a batline may be not visible when we are clearing
 * the reconciliation date
 */
static void
add_child_to_concil_by_path( ofaReconciliation *self, ofoConcil *concil, GtkTreeModel *tmodel, GtkTreePath *path )
{
	ofoBatLine *batline;

	batline = get_child_by_path( self, tmodel, path );
	if( batline ){
		ofa_iconcil_add_to_concil(
				OFA_ICONCIL( batline ), concil, ofa_page_get_dossier( OFA_PAGE( self )));
	}
}

/*
 * returns the child bat line of the pointed entry, if any
 */
static ofoBatLine *
get_child_by_path( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreePath *path )
{
	GtkTreeModel *store_model;
	GtkTreeIter ent_store_iter;
	ofoBatLine *batline;

	get_store_iter_from_path( self, tmodel, path, &store_model, &ent_store_iter );
	batline = get_child_by_store_iter( self, store_model, &ent_store_iter );

	return( batline );
}

/*
 * returns the child bat line of the pointed entry, if any
 *
 * tmodel, iter are store model and entry iter
 */
static ofoBatLine *
get_child_by_store_iter( ofaReconciliation *self, GtkTreeModel *store_model, GtkTreeIter *ent_iter )
{
	GtkTreeIter bat_store_iter;
	ofoBase *batline;

	batline = NULL;

	if( gtk_tree_model_iter_has_child( store_model, ent_iter ) &&
		gtk_tree_model_iter_children( store_model, &bat_store_iter, ent_iter )){

		gtk_tree_model_get(
				store_model,
				&bat_store_iter,
				COL_OBJECT,  &batline,
				-1 );
		g_return_val_if_fail( batline && OFO_IS_BAT_LINE( batline ), NULL );
		g_object_unref( batline );
	}

	return( batline ? OFO_BAT_LINE( batline ) : NULL );
}

static const GDate *
get_bat_line_dope( ofaReconciliation *self, ofoBatLine *batline )
{
	const GDate *dope;

	dope = ofo_bat_line_get_dope( batline );
	if( !my_date_is_valid( dope )){
		dope = ofo_bat_line_get_deffect( batline );
	}

	return( dope );
}

static ofoBase *
get_object_by_path( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreePath *path )
{
	GtkTreeIter iter;
	gboolean ok;
	ofoBase *object;

	ok = gtk_tree_model_get_iter( tmodel, &iter, path );
	g_return_val_if_fail( ok, NULL );

	gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &object, -1 );
	g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), NULL );
	g_object_unref( object );

	return( object );
}

/*
 * tmodel is the sort model (those of the treeview)
 * path is a path on this sort model (a path from the selection in the treeview)
 */
static void
get_store_iter_from_path( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeModel **store_model, GtkTreeIter *store_iter )
{
	ofaReconciliationPrivate *priv;
	GtkTreeIter iter, filter_iter;
	gboolean ok;

	priv = self->priv;

	*store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	ok = gtk_tree_model_get_iter( tmodel, &iter, path );
	g_return_if_fail( ok );

	gtk_tree_model_sort_convert_iter_to_child_iter(
			GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, &iter );
	gtk_tree_model_filter_convert_iter_to_child_iter(
			GTK_TREE_MODEL_FILTER( priv->tfilter ), store_iter, &filter_iter );
}

/*
 * This is called at the end of the view setup: all widgets are defined,
 * and triggers are connected.
 *
 * settings format: account;mode;manual_concil[sql];
 */
static void
get_settings( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	GDate date;
	gchar *sdate;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_reconciliation );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			gtk_entry_set_text( priv->account_entry, cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			select_mode( self, atoi( cstr ));
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr ){
			my_date_set_from_str( &date, cstr, MY_DATE_SQL );
			if( my_date_is_valid( &date )){
				sdate = my_date_to_str( &date, ofa_prefs_date_display());
				gtk_entry_set_text( priv->date_concil, sdate );
				g_free( sdate );
			}
		}

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	const gchar *account, *sdate;
	gchar *date_sql;
	GDate date;
	gchar *smode, *str;

	priv = self->priv;

	account = gtk_entry_get_text( priv->account_entry );

	smode = g_strdup_printf( "%d", priv->mode );

	sdate = gtk_entry_get_text( priv->date_concil );
	my_date_set_from_str( &date, sdate, ofa_prefs_date_display());
	if( my_date_is_valid( &date )){
		date_sql = my_date_to_str( &date, MY_DATE_SQL );
	} else {
		date_sql = g_strdup( "" );
	}

	str = g_strdup_printf( "%s;%s;%s;", account ? account : "", smode, date_sql );

	ofa_settings_set_string( st_reconciliation, str );

	g_free( date_sql );
	g_free( str );
	g_free( smode );
}

static void
dossier_signaling_connect( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	gulong handler;

	priv = self->priv;
	priv->dossier = ofa_page_get_dossier( OFA_PAGE( self ));

	handler = g_signal_connect(
					G_OBJECT( priv->dossier ),
					SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_dossier_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( priv->dossier ),
					SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_dossier_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static void
on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_dossier_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ENTRY( object )){
		on_new_entry( self, OFO_ENTRY( object ));
	}
}

/*
 * insert the new entry in the tree store if it is registered on the
 * currently selected account
 */
static void
on_new_entry( ofaReconciliation *self, ofoEntry *entry )
{
	ofaReconciliationPrivate *priv;
	const gchar *selected_account, *entry_account;

	priv = self->priv;

	selected_account = gtk_entry_get_text( priv->account_entry );
	entry_account = ofo_entry_get_account( entry );
	if( !g_utf8_collate( selected_account, entry_account )){
		insert_new_entry( self, entry );
	}
}

static void
insert_new_entry( ofaReconciliation *self, ofoEntry *entry )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *store;
	GtkTreeIter iter;

	priv = self->priv;
	store = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	gtk_tree_store_insert( GTK_TREE_STORE( store ), &iter, NULL, -1 );
	set_row_entry( self, store, &iter, entry );
	remediate_bat_lines( self, store, entry, &iter );
	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
}

/*
 * search for a BAT line at level 0 (not yet member of a proposal nor
 * reconciliated) which would be suitable for the given entry
 */
static void
remediate_bat_lines( ofaReconciliation *self, GtkTreeModel *tstore, ofoEntry *entry, GtkTreeIter *entry_iter )
{
	static const gchar *thisfn = "ofa_reconciliation_remediate_bat_lines";
	ofaReconciliationPrivate *priv;
	ofxAmount ent_debit, ent_credit, bat_amount;
	GtkTreeModel *store_model;
	GtkTreeIter iter;
	ofoBase *object;
	gchar *bat_sdeb, *bat_scre;

	priv = self->priv;
	ent_debit = ofo_entry_get_debit( entry );
	ent_credit = -1*ofo_entry_get_credit( entry );
	store_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	if( gtk_tree_model_get_iter_first( tstore, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tstore, &iter, COL_OBJECT, &object, -1 );
			g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
			g_object_unref( object );

			/* search for a batline at level 0, not yet reconciliated,
			 * with the right amounts */
			if( OFO_IS_BAT_LINE( object ) /*&& !ofo_bat_line_is_used( OFO_BAT_LINE( object ))*/){
				bat_amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
				if(( bat_amount > 0 && bat_amount == ent_debit ) ||
						( bat_amount < 0 && bat_amount == ent_credit )){

					g_object_ref( object );
					gtk_tree_store_remove( GTK_TREE_STORE( tstore ), &iter );
					if( bat_amount < 0 ){
						bat_sdeb = my_double_to_str( -bat_amount );
						bat_scre = g_strdup( "" );
					} else {
						bat_sdeb = g_strdup( "" );
						bat_scre = my_double_to_str( bat_amount );
					}
					g_debug( "%s: entry found for bat_sdeb=%s, bat_scre=%s", thisfn, bat_sdeb, bat_scre );
					insert_bat_line( self, OFO_BAT_LINE( object ), entry_iter, bat_sdeb, bat_scre );
					set_entry_dval_by_store_iter(
							self, ofo_bat_line_get_deffect( OFO_BAT_LINE( object )), store_model, entry_iter);
					g_object_unref( object );
					g_free( bat_sdeb );
					g_free( bat_scre );
					break;
				}
			}

			if( !gtk_tree_model_iter_next( tstore, &iter )){
				break;
			}
		}
	}
}

/*
 * a ledger mnemo, an account number, a currency code may has changed
 */
static void
on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_dossier_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p (%s)",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	if( OFO_IS_ENTRY( object )){
		on_updated_entry( self, OFO_ENTRY( object ));
	}
}

/*
 * we are doing our maximum to just requiring a store update
 * by comparing the stored data vs. the new entry data
 * After its modification, the entry may appear in - or disappear from -
 * the treeview...
 */
static void
on_updated_entry( ofaReconciliation *self, ofoEntry *entry )
{
	ofaReconciliationPrivate *priv;
	const gchar *selected_account, *entry_account;
	GtkTreeModel *tstore;
	GtkTreeIter *iter;

	priv = self->priv;
	iter = search_for_entry_by_number( self, ofo_entry_get_number( entry ));

	/* if the entry was present in the store, it is easy to remediate it */
	if( iter ){
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		set_row_entry( self, tstore, iter, entry );
		gtk_tree_iter_free( iter );
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	/* else, should it be present now ? */
	} else {
		selected_account = gtk_entry_get_text( priv->account_entry );
		entry_account = ofo_entry_get_account( entry );
		if( !g_utf8_collate( selected_account, entry_account )){
			insert_new_entry( self, entry );
		}
	}
}

static void
on_print_clicked( GtkButton *button, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	const gchar *acc_number;

	priv = self->priv;
	acc_number = gtk_entry_get_text( priv->account_entry );
	ofa_pdf_reconcil_run( ofa_page_get_main_window( OFA_PAGE( self )), acc_number );
}
