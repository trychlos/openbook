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
 *
 * $Id$
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
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-plugin.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-reconciliation.h"

/* private instance data
 */
struct _ofaReconciliationPrivate {

	/* UI
	 */
	GtkEntry          *account;
	GtkLabel          *account_label;
	GtkLabel          *account_debit;
	GtkLabel          *account_credit;
	GtkComboBox       *mode;
	GtkButton         *clear;
	GtkEntry          *date_concil;
	GtkTreeView       *tview;
	GtkTreeModel      *tfilter;				/* GtkTreeModelFilter of the tree view */
	GtkTreeModel      *tsort;				/* GtkTreeModelSort stacked onto the TreeModelFilter */
	GtkTreeViewColumn *sort_column;
	GtkLabel          *bal_debit;			/* balance of the account  */
	GtkLabel          *bal_credit;			/*  ... deducting unreconciliated entries */

	/* internals
	 */
	GDate              dconcil;
	GList             *batlines;				/* loaded bank account transaction lines */
	ofoDossier        *dossier;
	GList             *handlers;
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
	COL_VALID,				/* whether the reconciliation date has been validated */
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

static const sConcil st_concils[] = {
		{ ENT_CONCILED_YES, N_( "Reconciliated" ) },
		{ ENT_CONCILED_NO,  N_( "Not reconciliated" ) },
		{ ENT_CONCILED_ALL, N_( "All" ) },
		{ 0 }
};

#define COLOR_ACCOUNT                   "#0000ff"	/* blue */

static const gchar *st_reconciliation   = "Reconciliation";

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
static GtkWidget   *setup_manual_rappro( ofaPage *page );
static GtkWidget   *setup_auto_rappro( ofaPage *page );
static GtkWidget   *setup_account_display( ofaPage *page );
static GtkWidget   *setup_treeview( ofaPage *page );
static GtkWidget   *setup_balance( ofaPage *page );
static GtkWidget   *v_setup_buttons( ofaPage *page );
static void         v_init_view( ofaPage *page );
static GtkWidget   *v_get_top_focusable_widget( const ofaPage *page );
static gint         on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconciliation *self );
static void         on_header_clicked( GtkTreeViewColumn *column, ofaReconciliation *self );
static gboolean     is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconciliation *self );
static gboolean     is_visible_entry( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static gboolean     is_visible_batline( ofaReconciliation *self, ofoBatLine *batline );
static void         on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconciliation *self );
static void         on_account_changed( GtkEntry *entry, ofaReconciliation *self );
static void         on_account_button_clicked( GtkButton *button, ofaReconciliation *self );
static void         do_account_selection( ofaReconciliation *self );
static void         on_combo_mode_changed( GtkComboBox *box, ofaReconciliation *self );
static gint         get_selected_concil_mode( ofaReconciliation *self );
static void         select_mode( ofaReconciliation *self, gint mode );
static gboolean     check_for_enable_view( ofaReconciliation *self, ofoAccount **account, gint *mode );
static ofoAccount  *get_reconciliable_account( ofaReconciliation *self );
static void         on_fetch_button_clicked( GtkButton *button, ofaReconciliation *self );
static void         do_fetch_entries( ofaReconciliation *self );
static void         insert_entry( ofaReconciliation *self, GtkTreeModel *tstore, ofoEntry *entry );
static void         set_row_entry( ofaReconciliation *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry );
static void         on_date_concil_changed( GtkEditable *editable, ofaReconciliation *self );
static void         on_select_bat( GtkButton *button, ofaReconciliation *self );
static void         on_file_set( GtkFileChooserButton *button, ofaReconciliation *self );
static void         on_clear_button_clicked( GtkButton *button, ofaReconciliation *self );
static void         setup_bat_lines( ofaReconciliation *self, gint bat_id );
static void         clear_bat_lines( ofaReconciliation *self );
static void         display_bat_lines( ofaReconciliation *self );
static GtkTreeIter *search_for_entry_by_number( ofaReconciliation *self, gint number );
static GtkTreeIter *search_for_entry_by_amount( ofaReconciliation *self, const gchar *sbat_deb, const gchar *sbat_cre );
static void         update_candidate_entry( ofaReconciliation *self, ofoBatLine *batline, GtkTreeIter *entry_iter );
static void         insert_bat_line( ofaReconciliation *self, ofoBatLine *batline, GtkTreeIter *entry_iter, const gchar *sdeb, const gchar *scre );
static gboolean     on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaReconciliation *self );
static void         collapse_node( ofaReconciliation *self, GtkWidget *widget );
static void         expand_node( ofaReconciliation *self, GtkWidget *widget );
static void         on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static gboolean     toggle_rappro( ofaReconciliation *self, GtkTreePath *path );
static void         reconciliate_entry( ofaReconciliation *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *iter );
static void         set_reconciliated_balance( ofaReconciliation *self );
static void         get_settings( ofaReconciliation *self );
static void         set_settings( ofaReconciliation *self );
static void         dossier_signaling_connect( ofaReconciliation *self );
static void         on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaReconciliation *self );
static void         on_new_entry( ofaReconciliation *self, ofoEntry *entry );
static void         on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaReconciliation *self );
static void         on_updated_entry( ofaReconciliation *self, ofoEntry *entry );
static gboolean     find_entry_by_number( ofaReconciliation *self, GtkTreeModel *tmodel, ofxCounter number, GtkTreeIter *iter );

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

		ofo_bat_line_free_dataset( priv->batlines );

		if( priv->handlers && priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
			priv->handlers = NULL;
		}
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

static GtkWidget *
v_setup_view( ofaPage *page )
{
	GtkFrame *frame;
	GtkGrid *grid;
	GtkWidget *account;
	GtkWidget *rappro;
	GtkWidget *tview;
	GtkWidget *soldes;

	frame = GTK_FRAME( gtk_frame_new( NULL ));

	grid = GTK_GRID( gtk_grid_new());
	gtk_widget_set_margin_left( GTK_WIDGET( grid ), 4 );
	gtk_widget_set_margin_right( GTK_WIDGET( grid ), 4 );
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_grid_set_row_spacing( grid, 3 );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( grid ));

	account = setup_account_selection( page );
	gtk_grid_attach( grid, account, 0, 0, 1, 1 );

	/* manual reconciliation (enter a date) */
	rappro = setup_manual_rappro( page );
	gtk_grid_attach( grid, rappro, 1, 0, 1, 1 );

	/* auto reconciliation from imported BAT file */
	rappro = setup_auto_rappro( page );
	gtk_grid_attach( grid, rappro, 2, 0, 1, 1 );

	account = setup_account_display( page );
	gtk_grid_attach( grid, account, 0, 1, 3, 1 );

	tview = setup_treeview( page );
	gtk_grid_attach( grid, tview, 0, 2, 3, 1 );

	soldes = setup_balance( page );
	gtk_grid_attach( grid, soldes, 0, 3, 3, 1 );

	get_settings( OFA_RECONCILIATION( page ));

	dossier_signaling_connect( OFA_RECONCILIATION( page ));

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_account_selection( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
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

	priv = OFA_RECONCILIATION( page )->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
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

	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
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
	gtk_widget_set_hexpand( GTK_WIDGET( grid ), TRUE );
	gtk_grid_set_column_spacing( grid, 4 );
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( grid ));

	label = GTK_LABEL( gtk_label_new_with_mnemonic( _( "Da_te :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	priv->date_concil = GTK_ENTRY( gtk_entry_new());
	label = GTK_LABEL( gtk_label_new( "" ));

	my_editable_date_init( GTK_EDITABLE( priv->date_concil ));
	my_editable_date_set_format( GTK_EDITABLE( priv->date_concil ), MY_DATE_DMYY );
	my_editable_date_set_date( GTK_EDITABLE( priv->date_concil ), &priv->dconcil );
	my_editable_date_set_label( GTK_EDITABLE( priv->date_concil ), GTK_WIDGET( label ), MY_DATE_DMMM );

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
	GtkLabel *label;
	gchar *markup;
	GtkWidget *button, *image;

	priv = OFA_RECONCILIATION( page )->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b> %s </b>", _( "Assisted reconciliation" ));
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
			_( "Select a previously imported Bank Account Transactions list" ));

	button = gtk_file_chooser_button_new( NULL, GTK_FILE_CHOOSER_ACTION_OPEN );
	gtk_grid_attach( grid, GTK_WIDGET( button ), 2, 1, 1, 1 );
	g_signal_connect( G_OBJECT( button ), "file-set", G_CALLBACK( on_file_set ), page );
	gtk_widget_set_tooltip_text(
			button,
			_( "Import an new Bank Account Transactions list to be used in the reconciliation" ));

	image = gtk_image_new_from_icon_name( "gtk-clear", GTK_ICON_SIZE_BUTTON );
	priv->clear = GTK_BUTTON( gtk_button_new());
	gtk_button_set_image( priv->clear, image );
	gtk_grid_attach( grid, GTK_WIDGET( priv->clear ), 3, 1, 1, 1 );
	gtk_widget_set_tooltip_text(
			GTK_WIDGET( priv->clear ),
			_( "Clear the displayed Bank Account Transaction lines" ));
	g_signal_connect(
			G_OBJECT( priv->clear ),
			"clicked", G_CALLBACK( on_clear_button_clicked ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
setup_account_display( ofaPage *page )
{
	ofaReconciliationPrivate *priv;
	GtkBox *box;
	GtkLabel *label;
	GdkRGBA color;

	priv = OFA_RECONCILIATION( page )->priv;

	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 4 ));

	gdk_rgba_parse( &color, COLOR_ACCOUNT );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_label_set_width_chars( label, 13 );
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

	priv->account_label = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_override_color( GTK_WIDGET( priv->account_label ), GTK_STATE_FLAG_NORMAL, &color );
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
			G_TYPE_STRING, G_TYPE_BOOLEAN,					/* dcreconcil, bvalid */
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
	gtk_tree_view_column_set_expand( column, TRUE );
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
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );

	gtk_widget_set_sensitive( GTK_WIDGET( tview ), FALSE );
	priv->tview = tview;

	return( GTK_WIDGET( frame ));
}

/*
 * two widgets (debit/credit) display the 'theoric' balance of the
 * account, by deducting the unreconciliated entries from the balance
 * in our book - this is supposed simulate the actual bank balance
 */
static GtkWidget *
setup_balance( ofaPage *page )
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
	gtk_label_set_width_chars( label, 13 );
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
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaPage *page )
{
	check_for_enable_view( OFA_RECONCILIATION( page ), NULL, NULL );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_RECONCILIATION( page ), NULL );

	return( GTK_WIDGET( OFA_RECONCILIATION( page )->priv->tview ));
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
							ofo_bat_line_get_dope( OFO_BAT_LINE( object_a ));
			date_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_dope( OFO_ENTRY( object_b )) :
							ofo_bat_line_get_dope( OFO_BAT_LINE( object_b ));
			g_return_val_if_fail( my_date_is_valid( date_a ), 0 );
			g_return_val_if_fail( my_date_is_valid( date_b ), 0 );
			cmp = my_date_compare( date_a, date_b );
			break;
		case COL_PIECE:
			str_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_ref( OFO_ENTRY( object_a )) : empty;
			str_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_ref( OFO_ENTRY( object_b )) : empty;
			cmp = g_utf8_collate( str_a, str_b );
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
			date_a = OFO_IS_ENTRY( object_a ) ?
						ofo_entry_get_concil_dval( OFO_ENTRY( object_a )) : NULL;
			date_b = OFO_IS_ENTRY( object_b ) ?
						ofo_entry_get_concil_dval( OFO_ENTRY( object_b )) : NULL;
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
	gboolean visible;
	GObject *object;

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );

	/* as we insert the row before populating it, it may happen that
	 * the object be not set */
	if( !object ){
		return( FALSE );
	}
	g_object_unref( object );
	g_return_val_if_fail( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object ), TRUE );

	visible = OFO_IS_ENTRY( object ) ?
			is_visible_entry( self, tmodel, iter, OFO_ENTRY( object )) :
			is_visible_batline( self, OFO_BAT_LINE( object ));

	return( visible );
}

static gboolean
is_visible_entry( ofaReconciliation *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	gboolean visible;
	gboolean validated;
	gint mode;

	gtk_tree_model_get( tmodel, iter, COL_VALID, &validated, -1 );

	mode = get_selected_concil_mode( self );
	visible = TRUE;

	if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
		visible = FALSE;

	} else {
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
				/* when display mode is not set */
				visible = FALSE;
		}
	}

	return( visible );
}

static gboolean
is_visible_batline( ofaReconciliation *self, ofoBatLine *batline )
{
	gint ecr_number;

	ecr_number = ofo_bat_line_get_entry( batline );
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
						ofaReconciliation *self )
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
		if( id == COL_DRECONCIL ){
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
on_account_changed( GtkEntry *entry, ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_on_account_changed";
	ofaReconciliationPrivate *priv;
	ofoAccount *account;
	gdouble debit, credit;
	gchar *str, *msg;

	priv = self->priv;
	check_for_enable_view( self, &account, NULL );

	if( account ){
		g_debug( "%s: setting account %s properties", thisfn, ofo_account_get_number( account ));

		gtk_label_set_text( priv->account_label, ofo_account_get_label( account ));

		debit = ofo_account_get_val_debit( account )+ofo_account_get_rough_debit( account );
		credit = ofo_account_get_val_credit( account )+ofo_account_get_rough_credit( account );

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

		set_settings( self );

		/* automatically fetch entries */
		on_fetch_button_clicked( NULL, self );

	} else {
		g_debug( "%s: clearing account properties", thisfn );

		gtk_label_set_text( priv->account_label, "" );
		gtk_label_set_text( priv->account_debit, "" );
		gtk_label_set_text( priv->account_credit, "" );
	}
}

static void
on_account_button_clicked( GtkButton *button, ofaReconciliation *self )
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
	const gchar *account_number;
	gchar *number;

	account_number = gtk_entry_get_text( self->priv->account );
	if( !my_strlen( account_number )){
		account_number = st_default_reconciliated_class;
	}

	number = ofa_account_select_run(
					ofa_page_get_main_window( OFA_PAGE( self )),
					account_number,
					FALSE );

	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( self->priv->account, number );
	}

	g_free( number );
}

static void
on_combo_mode_changed( GtkComboBox *box, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;

	if( check_for_enable_view( self, NULL, NULL )){

		priv = self->priv;

		set_settings( self );

		/* do not re-fetch entries, but refilter the view */
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
	}
}

static gint
get_selected_concil_mode( ofaReconciliation *self )
{
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gint mode;

	mode = -1;

	if( gtk_combo_box_get_active_iter( self->priv->mode, &iter )){
		tmodel = gtk_combo_box_get_model( self->priv->mode );
		gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &mode, -1 );
	}

	return( mode );
}

static void
select_mode( ofaReconciliation *self, gint mode )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint box_mode;

	priv = self->priv;

	tmodel = gtk_combo_box_get_model( priv->mode );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter, ENT_COL_CODE, &box_mode, -1 );
			if( box_mode == mode ){
				gtk_combo_box_set_active_iter( priv->mode, &iter );
				break;
			}
			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

/*
 * the view is disabled (insensitive) each time the configuration parms
 * are not valid (invalid account or invalid reconciliation display
 * mode)
 * the status of the BAT (whether one is displayed or not) does not
 * interfer with the sensitivity of the view
 */
static gboolean
check_for_enable_view( ofaReconciliation *self, ofoAccount **account, gint *mode )
{
	gboolean enabled;
	ofoAccount *my_account;
	gint my_mode;

	enabled = TRUE;

	my_account = get_reconciliable_account( self );
	enabled &= ( my_account != NULL );
	if( account ){
		*account = my_account;
	}

	my_mode = get_selected_concil_mode( self );
	enabled &= ( my_mode > ENT_CONCILED_FIRST && my_mode < ENT_CONCILED_LAST );
	if( mode ){
		*mode = my_mode;
	}

	gtk_widget_set_sensitive( GTK_WIDGET( self->priv->tview ), enabled );

	return( enabled );
}

static ofoAccount *
get_reconciliable_account( ofaReconciliation *self )
{
	const gchar *number;
	ofoAccount *account;

	number = gtk_entry_get_text( self->priv->account );
	account = ofo_account_get_by_number(
						ofa_page_get_dossier( OFA_PAGE( self )), number );
	if( account ){
		g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );
		if( ofo_account_is_root( account )){
			account = NULL;
		}
	}

	return( account );
}

/*
 * there used to be a 'Fetch' button, but we have remove it to provide
 * a more dynamic display
 * All entries for the given account are fetched as soon as the account
 * entry is valid
 */
static void
on_fetch_button_clicked( GtkButton *button /* =NULL */, ofaReconciliation *self )
{
	do_fetch_entries( self );
	display_bat_lines( self );
	set_reconciliated_balance( self );
}

static void
do_fetch_entries( ofaReconciliation *self )
{
	static const gchar *thisfn = "ofa_reconciliation_do_fetch_entries";
	ofoAccount *account;
	GList *entries, *it;
	GtkTreeModel *tmodel;

	check_for_enable_view( self, &account, NULL );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	g_debug( "%s: clearing treestore=%p", thisfn, ( void * ) tmodel );
	gtk_tree_store_clear( GTK_TREE_STORE( tmodel ));

	entries = ofo_entry_get_dataset_by_account(
					ofa_page_get_dossier( OFA_PAGE( self )),
					ofo_account_get_number( account ));

	for( it=entries ; it ; it=it->next ){
		insert_entry( self, tmodel, OFO_ENTRY( it->data ));
	}

	ofo_entry_free_dataset( entries );
}

static void
insert_entry( ofaReconciliation *self, GtkTreeModel *tstore, ofoEntry *entry )
{
	GtkTreeIter iter;

	gtk_tree_store_insert( GTK_TREE_STORE( tstore ), &iter, NULL, -1 );
	set_row_entry( self, tstore, &iter, entry );
}

static void
set_row_entry( ofaReconciliation *self, GtkTreeModel *tstore, GtkTreeIter *iter, ofoEntry *entry )
{
	gchar *sdope, *sdeb, *scre, *sdrap;
	const GDate *dconcil;

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DMYY );
	sdeb = my_double_to_str( ofo_entry_get_debit( entry ));
	scre = my_double_to_str( ofo_entry_get_credit( entry ));
	dconcil = ofo_entry_get_concil_dval( entry );
	sdrap = my_date_to_str( dconcil, MY_DATE_DMYY );

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
			COL_VALID,     my_date_is_valid( dconcil ),
			COL_OBJECT,    entry,
			-1 );

	g_free( sdope );
	g_free( sdeb );
	g_free( scre );
	g_free( sdrap );
}

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
		set_settings( self );
	}
}

/*
 * select an already imported Bank Account Transaction list file
 *
 * @button is NULL when called from on_file_set().
 */
static void
on_select_bat( GtkButton *button, ofaReconciliation *self )
{
	gint bat_id;

	bat_id = ofa_bat_select_run( ofa_page_get_main_window( OFA_PAGE( self )));

	if( bat_id > 0 ){
		setup_bat_lines( self, bat_id );
	}
}

/*
 * try to import a bank account transaction list
 */
static void
on_file_set( GtkFileChooserButton *button, ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	ofaFileFormat *settings;
	ofaIImportable *importable;

	priv = self->priv;

	settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
	ofa_file_format_set( settings,
			NULL, OFA_FFTYPE_OTHER, OFA_FFMODE_IMPORT, "UTF-8", 0, 0, 0, 0 );

	importable = ofa_iimportable_find_willing_to(
			gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( button )), settings );

	if( importable ){
		if( !ofa_iimportable_import_uri( importable, priv->dossier, NULL )){
			on_select_bat( NULL, self );
		}
		g_object_unref( importable );
	}

	g_object_unref( settings );
}

static void
on_clear_button_clicked( GtkButton *button, ofaReconciliation *self )
{
	clear_bat_lines( self );
}

/*
 * makes use of a Bank Account Transaction (BAT) list
 * whether is has just been imported, or it is reloaded from sgbd
 *
 * only lines which have not yet been used for reconciliation are read
 */
static void
setup_bat_lines( ofaReconciliation *self, gint bat_id )
{
	clear_bat_lines( self );

	self->priv->batlines =
			ofo_bat_line_get_dataset(
					ofa_page_get_dossier( OFA_PAGE( self )),
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
clear_bat_lines( ofaReconciliation *self )
{
	GtkTreeModel *tmodel;
	gboolean bvalid;
	GObject *object;
	GtkTreeIter iter, child_iter;

	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		while( TRUE ){

			gtk_tree_model_get(
					tmodel,
					&iter,
					COL_VALID,  &bvalid,
					COL_OBJECT, &object,
					-1 );
			g_object_unref( object );

			if( OFO_IS_ENTRY( object )){

				if( !bvalid ){
					gtk_tree_store_set(
							GTK_TREE_STORE( tmodel ),
							&iter,
							COL_DRECONCIL, "",
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

	g_list_free_full( self->priv->batlines, ( GDestroyNotify ) g_object_unref );
	self->priv->batlines = NULL;
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
display_bat_lines( ofaReconciliation *self )
{
	GList *line;
	ofoBatLine *batline;
	gboolean done;
	gint bat_ecr;
	gdouble bat_amount;
	gchar *sbat_deb, *sbat_cre;
	GtkTreeIter *entry_iter;

	for( line=self->priv->batlines ; line ; line=line->next ){

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

		bat_ecr = ofo_bat_line_get_entry( batline );
		if( bat_ecr > 0 ){
			entry_iter = search_for_entry_by_number( self, bat_ecr );
			if( entry_iter ){
				insert_bat_line( self, batline, entry_iter, sbat_deb, sbat_cre );
				gtk_tree_iter_free( entry_iter );
				done = TRUE;
			}
		}

		if( !done ){
			entry_iter = search_for_entry_by_amount( self, sbat_deb, sbat_cre );
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
search_for_entry_by_number( ofaReconciliation *self, gint number )
{
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	gint ecr_number;
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
search_for_entry_by_amount( ofaReconciliation *self, const gchar *sbat_deb, const gchar *sbat_cre )
{
	GtkTreeModel *child_tmodel;
	GtkTreeIter iter;
	GtkTreeIter *entry_iter;
	gchar *sdeb, *scre;
	GObject *object;
	gboolean found;

	found = FALSE;
	entry_iter = NULL;
	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

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
				g_return_val_if_fail( g_utf8_strlen( sdeb, -1 ) || g_utf8_strlen( scre, -1 ), NULL );

				if( !g_utf8_collate( scre, sbat_deb ) || !g_utf8_collate( sdeb, sbat_cre )){
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
update_candidate_entry( ofaReconciliation *self, ofoBatLine *batline, GtkTreeIter *entry_iter )
{
	GtkTreeModel *child_tmodel;
	gchar *sdvaleur;

	g_return_if_fail( entry_iter );

	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	sdvaleur = my_date_to_str( ofo_bat_line_get_deffect( batline ), MY_DATE_DMYY );

	/* set the proposed reconciliation date in the entry */
	gtk_tree_store_set(
			GTK_TREE_STORE( child_tmodel ),
			entry_iter,
			COL_DRECONCIL, sdvaleur,
			-1 );

	g_free( sdvaleur );
}

/*
 * insert the bat line either as a child of entry_iter, or without parent
 */
static void
insert_bat_line( ofaReconciliation *self, ofoBatLine *batline,
							GtkTreeIter *entry_iter, const gchar *sdeb, const gchar *scre )
{
	GtkTreeModel *child_tmodel;
	const GDate *dope;
	gchar *sdope, *snum;
	GtkTreeIter new_iter;

	child_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( self->priv->tfilter ));

	dope = ofo_bat_line_get_dope( batline );
	if( !my_date_is_valid( dope )){
		dope = ofo_bat_line_get_deffect( batline );
	}
	sdope = my_date_to_str( dope, MY_DATE_DMYY );
	snum = g_strdup_printf( "%ld", ofo_bat_line_get_line_id( batline ));

	/* set the bat line as a hint */
	gtk_tree_store_insert_with_values(
				GTK_TREE_STORE( child_tmodel ),
				&new_iter,
				entry_iter,
				-1,
				COL_DOPE,    sdope,
				COL_PIECE,   ofo_bat_line_get_ref( batline ),
				COL_NUMBER,  snum,
				COL_LABEL,   ofo_bat_line_get_label( batline ),
				COL_DEBIT,   sdeb,
				COL_CREDIT,  scre,
				COL_OBJECT,  batline,
				-1 );

	g_free( snum );
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
	GtkTreeIter iter, parent;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, NULL, &iter )){

			if( gtk_tree_model_iter_has_child( self->priv->tsort, &iter )){
				path = gtk_tree_model_get_path( self->priv->tsort, &iter );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );

			} else if( gtk_tree_model_iter_parent( self->priv->tsort, &parent, &iter )){
				path = gtk_tree_model_get_path( self->priv->tsort, &parent );
				gtk_tree_view_collapse_row( GTK_TREE_VIEW( widget ), path );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
expand_node( ofaReconciliation *self, GtkWidget *widget )
{
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreePath *path;

	if( GTK_IS_TREE_VIEW( widget )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ));
		if( gtk_tree_selection_get_selected( select, NULL, &iter )){

			if( gtk_tree_model_iter_has_child( self->priv->tsort, &iter )){
				path = gtk_tree_model_get_path( self->priv->tsort, &iter );
				gtk_tree_view_expand_row( GTK_TREE_VIEW( widget ), path, FALSE );
				gtk_tree_path_free( path );
			}
		}
	}
}

static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	static const gchar *thisfn = "ofa_reconciliation_on_row_activated";
	ofaReconciliationPrivate *priv;

	g_debug( "%s: view=%p, path=%p, column=%p, page=%p",
			thisfn, ( void * ) view, ( void * ) path, ( void * ) column, ( void * ) page );

	priv = OFA_RECONCILIATION( page )->priv;

	if( toggle_rappro( OFA_RECONCILIATION( page ), path )){
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		set_reconciliated_balance( OFA_RECONCILIATION( page ));
	}
}

/*
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
	gboolean bvalid;
	GObject *object;
	GDate date;

	priv = self->priv;

	if( gtk_tree_model_get_iter( priv->tsort, &iter, path )){

		gtk_tree_model_get(
				priv->tsort,
				&iter,
				COL_DRECONCIL, &srappro,
				COL_VALID,     &bvalid,
				COL_OBJECT,    &object,
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
			my_date_set_from_date( &date, &priv->dconcil );
			if( my_date_is_valid( &date )){
				reconciliate_entry( self, OFO_ENTRY( object ), &date, &iter );
			}
		}
	}

	return( TRUE );
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
static void
reconciliate_entry( ofaReconciliation *self, ofoEntry *entry, const GDate *drappro, GtkTreeIter *sort_iter )
{
	ofaReconciliationPrivate *priv;
	GtkTreeModel *store_tmodel;
	GtkTreeIter filter_iter, store_iter, store_bat_iter;
	ofoBatLine *batline;
	gboolean is_valid_rappro;
	gchar *str;

	priv = self->priv;
	is_valid_rappro = my_date_is_valid( drappro );
	batline = NULL;

	/* set the reconciliation date in the entry */
	ofo_entry_set_concil_dval( entry, is_valid_rappro ? drappro : NULL );

	/* update the child bat line if it exists
	 * we work on child model because 'gtk_tree_model_iter_has_child'
	 * actually says if we have a *visible* child
	 * but a possible batline is not visible when we are clearing
	 * the reconciliation date */
	store_tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));

	gtk_tree_model_sort_convert_iter_to_child_iter(
			GTK_TREE_MODEL_SORT( priv->tsort ), &filter_iter, sort_iter );
	gtk_tree_model_filter_convert_iter_to_child_iter(
			GTK_TREE_MODEL_FILTER( priv->tfilter ), &store_iter, &filter_iter );

	if( gtk_tree_model_iter_has_child( store_tmodel, &store_iter ) &&
		gtk_tree_model_iter_children( store_tmodel, &store_bat_iter, &store_iter )){

		gtk_tree_model_get(
				store_tmodel,
				&store_bat_iter,
				COL_OBJECT,  &batline,
				-1 );
		g_object_unref( batline );

		ofo_bat_line_set_entry( batline,
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
		str = my_date_to_str( drappro, MY_DATE_DMYY );
	} else if( batline ){
		str = my_date_to_str( ofo_bat_line_get_deffect( batline ), MY_DATE_DMYY );
	} else {
		str = g_strdup( "" );
	}

	gtk_tree_store_set(
			GTK_TREE_STORE( store_tmodel ),
			&store_iter,
			COL_DRECONCIL, str,
			COL_VALID,     is_valid_rappro,
			-1 );

	g_free( str );

	/* last, update the sgbd */
	ofo_entry_update_concil(
			entry,
			ofa_page_get_dossier( OFA_PAGE( self )),
			ofo_entry_get_concil_dval( entry ));

	if( batline ){
		ofo_bat_line_update(
				batline,
				ofa_page_get_dossier( OFA_PAGE( self )));
	}
}

/*
 * display the new balance of the account, taking into account the
 * reconciliated entries and the unreconciliated BAT lines
 */
static void
set_reconciliated_balance( ofaReconciliation *self )
{
	gdouble debit, credit;
	const gchar *account_number;
	ofoAccount *account;
	gdouble account_debit, account_credit;
	GtkTreeIter iter;
	gboolean bvalid;
	GObject *object;
	gdouble amount;
	gchar *str, *sdeb, *scre;

	account = NULL;
	account_debit = 0.0;
	account_credit = 0.0;
	account_number = gtk_entry_get_text( self->priv->account );
	if( account_number && g_utf8_strlen( account_number, -1 )){
		account = ofo_account_get_by_number(
						ofa_page_get_dossier( OFA_PAGE( self )), account_number );
	}
	if( account ){
		account_debit = ofo_account_get_val_debit( account )+ofo_account_get_rough_debit( account );
		account_credit = ofo_account_get_val_credit( account )+ofo_account_get_rough_credit( account );
	}

	debit = account_debit;
	credit = account_credit;

	if( gtk_tree_model_get_iter_first( self->priv->tsort, &iter )){
		while( TRUE ){
			gtk_tree_model_get(
					self->priv->tsort, &iter, COL_VALID, &bvalid, COL_OBJECT, &object, -1 );
			g_object_unref( object );

			if( !bvalid ){
				if( OFO_IS_ENTRY( object )){
					debit -= ofo_entry_get_debit( OFO_ENTRY( object ));
					credit -= ofo_entry_get_credit( OFO_ENTRY( object ));
				} else {
					amount = ofo_bat_line_get_amount( OFO_BAT_LINE( object ));
					if( amount < 0 ){
						credit += -amount;
					} else {
						debit += amount;
					}
				}
			}
			if( !gtk_tree_model_iter_next( self->priv->tsort, &iter )){
				break;
			}
		}
	}

	if( debit > credit ){
		str = my_double_to_str( debit-credit );
		sdeb = g_strdup_printf( _( "%s DB" ), str );
		scre = g_strdup( "" );
	} else {
		sdeb = g_strdup( "" );
		str = my_double_to_str( credit-debit );
		scre = g_strdup_printf( _( "%s CR" ), str );
	}

	gtk_label_set_text( self->priv->bal_debit, sdeb );
	gtk_label_set_text( self->priv->bal_credit, scre );

	g_free( str );
	g_free( sdeb );
	g_free( scre );
}

/*
 * settings format: account;type;date;
 */
static void
get_settings( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_reconciliation );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( cstr && priv->account ){
			gtk_entry_set_text( priv->account, cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr && priv->mode ){
			select_mode( self, atoi( cstr ));
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( cstr && priv->date_concil ){
			gtk_entry_set_text( priv->date_concil, cstr );
		}

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaReconciliation *self )
{
	ofaReconciliationPrivate *priv;
	const gchar *account, *sdate;
	gint mode;
	gchar *smode, *str;

	priv = self->priv;

	account = priv->account ? gtk_entry_get_text( priv->account ) : NULL;
	mode = priv->mode ? get_selected_concil_mode( self ) : -1;
	smode = g_strdup_printf( "%d", mode );
	sdate = priv->date_concil ? gtk_entry_get_text( priv->date_concil ) : NULL;

	str = g_strdup_printf( "%s;%s;%s;", account ? account : "", smode, sdate ? sdate : "" );

	ofa_settings_set_string( st_reconciliation, str );

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
	GtkTreeModel *tstore;

	priv = self->priv;

	selected_account = gtk_entry_get_text( priv->account );
	entry_account = ofo_entry_get_account( entry );

	if( !g_utf8_collate( selected_account, entry_account )){
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		insert_entry( self, tstore, entry );
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
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

static void
on_updated_entry( ofaReconciliation *self, ofoEntry *entry )
{
	ofaReconciliationPrivate *priv;
	const gchar *selected_account, *entry_account;
	GtkTreeModel *tstore;
	GtkTreeIter iter;

	priv = self->priv;

	selected_account = gtk_entry_get_text( priv->account );
	entry_account = ofo_entry_get_account( entry );

	if( !g_utf8_collate( selected_account, entry_account )){
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		if( find_entry_by_number( self, tstore, ofo_entry_get_number( entry ), &iter )){
			set_row_entry( self, tstore, &iter, entry );
			gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( priv->tfilter ));
		}
	}
}

/*
 * return TRUE if we have found the requested entry row
 */
static gboolean
find_entry_by_number( ofaReconciliation *self, GtkTreeModel *tmodel, ofxCounter entry_number, GtkTreeIter *iter )
{
	ofxCounter row_number;

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, COL_NUMBER, &row_number, -1 );
			if( row_number == entry_number ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}
