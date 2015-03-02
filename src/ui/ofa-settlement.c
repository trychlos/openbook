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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-settlement.h"

/*
 * ofaEntrySettlement:
 */
typedef enum {
	ENT_SETTLEMENT_FIRST = 0,
	ENT_SETTLEMENT_YES,
	ENT_SETTLEMENT_NO,
	ENT_SETTLEMENT_ALL,
	ENT_SETTLEMENT_LAST
}
	ofaEntrySettlement;

/* priv instance data
 */
struct _ofaSettlementPrivate {

	/* internals
	 */
	ofoDossier        *dossier;			/* dossier */
	gchar             *account_number;
	const gchar       *account_currency;
	ofaEntrySettlement settlement;
	GList             *handlers;

	/* sorting the view
	 */
	gint               sort_column_id;
	gint               sort_sens;
	GtkTreeViewColumn *sort_column;

	/* UI
	 */
	GtkContainer      *top_box;
	GtkTreeView       *tview;

	/* frame 1: account selection
	 */
	GtkWidget         *account_entry;
	GtkWidget         *account_label;

	/* footer
	 */
	GtkWidget         *settle_btn;
	GtkWidget         *unsettle_btn;
	GtkWidget         *debit_balance;
	GtkWidget         *credit_balance;
	GtkWidget         *currency_balance;
};

/* columns in the combo box which let us select which type of entries
 * are displayed
 */
enum {
	SET_COL_CODE = 0,
	SET_COL_LABEL,
	SET_N_COLUMNS
};

typedef struct {
	gint         code;
	const gchar *label;
}
	sSettlement;

static const sSettlement st_settlements[] = {
		{ ENT_SETTLEMENT_YES, N_( "Settled entries" ) },
		{ ENT_SETTLEMENT_NO,  N_( "Unsettled entries" ) },
		{ ENT_SETTLEMENT_ALL, N_( "All entries" ) },
		{ 0 }
};

/* columns in the entries view
 */
enum {
	ENT_COL_DOPE = 0,
	ENT_COL_DEFF,
	ENT_COL_NUMBER,
	ENT_COL_REF,
	ENT_COL_LABEL,
	ENT_COL_LEDGER,
	ENT_COL_ACCOUNT,
	ENT_COL_DEBIT,
	ENT_COL_CREDIT,
	ENT_COL_SETTLEMENT,
	ENT_COL_STATUS,
	ENT_COL_OBJECT,
	ENT_N_COLUMNS
};

/* when enumerating the selected rows
 * this structure is used twice:
 * - each time the selection is updated, in order to update the footer fields
 * - when settling or unsettling the selection
 */
typedef struct {
	ofaSettlement *self;
	gint           rows;				/* count of. */
	gint           settled;				/* count of. */
	gint           unsettled;			/* count of. */
	gdouble        debit;				/* sum of debit amounts */
	gdouble        credit;				/* sum of credit amounts */
	gint           set_number;
}
	sEnumSelected;

/* the id of the column is set against sortable columns */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

#define RGBA_NORMAL                     "#0000ff"		/* blue */
#define RGBA_WARNING                    "#ff8000"		/* orange */

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-settlement.ui";
static const gchar *st_ui_id            = "SettlementWindow";

static const gchar *st_pref_settlement  = "SettlementPrefs";

G_DEFINE_TYPE( ofaSettlement, ofa_settlement, OFA_TYPE_PAGE )

static GtkWidget     *v_setup_view( ofaPage *page );
static void           reparent_from_dialog( ofaSettlement *self, GtkContainer *parent );
static void           setup_footer( ofaSettlement *self );
static void           setup_entries_treeview( ofaSettlement *self );
static void           setup_account_selection( ofaSettlement *self );
static void           setup_settlement_selection( ofaSettlement *self );
static void           setup_signaling_connect( ofaSettlement *self );
static GtkWidget     *v_setup_buttons( ofaPage *page );
static void           v_init_view( ofaPage *page );
static GtkWidget     *v_get_top_focusable_widget( const ofaPage *page );
static void           on_account_changed( GtkEntry *entry, ofaSettlement *self );
static void           on_account_select( GtkButton *button, ofaSettlement *self );
static void           on_settlement_changed( GtkComboBox *box, ofaSettlement *self );
static gint           on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaSettlement *self );
static gint           cmp_strings( ofaSettlement *self, const gchar *stra, const gchar *strb );
static gint           cmp_amounts( ofaSettlement *self, const gchar *stra, const gchar *strb );
static gint           cmp_counters( ofaSettlement *self, const gchar *stra, const gchar *strb );
static gboolean       is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlement *self );
static void           on_header_clicked( GtkTreeViewColumn *column, ofaSettlement *self );
static gboolean       settlement_status_is_valid( ofaSettlement *self );
static void           try_display_entries( ofaSettlement *self );
static void           display_entries( ofaSettlement *self, GList *entries );
static void           display_entry( ofaSettlement *self, GtkTreeModel *tmodel, ofoEntry *entry );
static void           set_row_entry( ofaSettlement *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry );
static void           on_entries_treeview_selection_changed( GtkTreeSelection *select, ofaSettlement *self );
static void           enum_selected( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, sEnumSelected *ses );
static void           on_settle_clicked( GtkButton *button, ofaSettlement *self );
static void           on_unsettle_clicked( GtkButton *button, ofaSettlement *self );
static void           update_selection( ofaSettlement *self, gboolean settle );
static void           update_row( GtkTreeModel *tmodel, GtkTreeIter *iter, sEnumSelected *ses );
static void           get_settings( ofaSettlement *self );
static void           set_settings( ofaSettlement *self );
static void           on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaSettlement *self );
static void           on_new_entry( ofaSettlement *self, ofoEntry *entry );
static void           on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaSettlement *self );
static void           on_updated_entry( ofaSettlement *self, ofoEntry *entry );
static gboolean       find_entry_by_number( ofaSettlement *self, GtkTreeModel *tmodel, ofxCounter number, GtkTreeIter *iter );

static void
settlement_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_settlement_finalize";
	ofaSettlementPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_SETTLEMENT( instance ));

	/* free data members here */
	priv = OFA_SETTLEMENT( instance )->priv;

	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_parent_class )->finalize( instance );
}

static void
settlement_dispose( GObject *instance )
{
	ofaSettlementPrivate *priv;
	GList *it;

	g_return_if_fail( OFA_IS_SETTLEMENT( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_SETTLEMENT( instance )->priv;

		if( priv->handlers && priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
			priv->handlers = NULL;
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_parent_class )->dispose( instance );
}

static void
ofa_settlement_init( ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_SETTLEMENT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_SETTLEMENT, ofaSettlementPrivate );

	self->priv->settlement = -1;
	self->priv->sort_column_id = -1;
	self->priv->sort_sens = -1;
}

static void
ofa_settlement_class_init( ofaSettlementClass *klass )
{
	static const gchar *thisfn = "ofa_settlement_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settlement_dispose;
	G_OBJECT_CLASS( klass )->finalize = settlement_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaSettlementPrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	ofaSettlementPrivate *priv;
	GtkWidget *frame;

	priv = OFA_SETTLEMENT( page )->priv;

	priv->dossier = ofa_page_get_dossier( page );
	get_settings( OFA_SETTLEMENT( page ));

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_NONE );
	reparent_from_dialog( OFA_SETTLEMENT( page ), GTK_CONTAINER( frame ));

	/* build first the targets of the data, and only after the triggers */
	setup_footer( OFA_SETTLEMENT( page ));
	setup_entries_treeview( OFA_SETTLEMENT( page ));
	setup_settlement_selection( OFA_SETTLEMENT( page ));
	setup_account_selection( OFA_SETTLEMENT( page ));

	/* connect to dossier signaling system */
	setup_signaling_connect( OFA_SETTLEMENT( page ));

	return( frame );
}

static void
reparent_from_dialog( ofaSettlement *self, GtkContainer *parent )
{
	GtkWidget *dialog;
	GtkWidget *box;

	/* load our dialog */
	dialog = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( dialog && GTK_IS_WINDOW( dialog ));

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "px-box" );
	g_return_if_fail( box && GTK_IS_BOX( box ));

	self->priv->top_box = GTK_CONTAINER( box );

	/* attach our box to the parent's frame */
	gtk_widget_reparent( box, GTK_WIDGET( parent ));
}

static void
setup_footer( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkWidget *widget;
	GdkRGBA color;

	priv = self->priv;

	gdk_rgba_parse( &color, RGBA_NORMAL );

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-settle" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_settle_clicked ), self );
	priv->settle_btn = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-unsettle" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_unsettle_clicked ), self );
	priv->unsettle_btn = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-settlement-selection" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_widget_override_color( widget, GTK_STATE_FLAG_NORMAL, &color );

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-debit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->debit_balance = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-credit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->credit_balance = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-currency" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_widget_override_color( widget, GTK_STATE_FLAG_NORMAL, &color );
	priv->currency_balance = widget;
}

/*
 * the treeview is filtered on the settlement status
 */
static void
setup_entries_treeview( ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_setup_entries_treeview";
	ofaSettlementPrivate *priv;
	GtkTreeView *tview;
	GtkTreeModel *tmodel, *tfilter, *tsort;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;
	GtkTreeViewColumn *sort_column;

	priv = self->priv;

	tview = ( GtkTreeView * ) my_utils_container_get_child_by_name( priv->top_box, "p1-entries" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			ENT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ULONG,		/* dope, deff, number */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* ref, label, ledger */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* account, debit, credit */
			G_TYPE_STRING, G_TYPE_STRING,					/* settlement, status */
			G_TYPE_OBJECT ));								/* ofoEntry */

	tfilter = gtk_tree_model_filter_new( tmodel, NULL );
	g_object_unref( tmodel );

	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) is_visible_row,
			self,
			NULL );

	tsort = gtk_tree_model_sort_new_with_model( tfilter );
	g_object_unref( tfilter );

	gtk_tree_view_set_model( tview, tsort );
	g_object_unref( tsort );

	g_debug( "%s: tstore=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) tmodel, ( void * ) tfilter, ( void * ) tsort );

	/* default is to sort by ascending operation date
	 */
	sort_column = NULL;
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = ENT_COL_DOPE;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_ASCENDING;
	}

	/* operation date
	 */
	column_id = ENT_COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* effect date
	 */
	column_id = ENT_COL_DEFF;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Effect" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* piece's reference
	 */
	column_id = ENT_COL_REF;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Piece" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* ledger
	 */
	column_id = ENT_COL_LEDGER;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ledger" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* account
	 */
	column_id = ENT_COL_ACCOUNT;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Account" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* label
	 */
	column_id = ENT_COL_LABEL;
	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_END, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* debit
	 */
	column_id = ENT_COL_DEBIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Debit" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* credit
	 */
	column_id = ENT_COL_CREDIT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Credit" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_column_set_min_width( column, 110 );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	/* settlement number
	 */
	column_id = ENT_COL_SETTLEMENT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Settlement" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_entries_treeview_selection_changed ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ));
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tsort ), priv->sort_column_id, priv->sort_sens );

	priv->tview = tview;
}

static void
setup_account_selection( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkWidget *widget;

	priv = self->priv;

	/* label must be setup before entry may be changed */
	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->account_label = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->account_entry = widget;
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_account_changed ), self );
	if( my_strlen( priv->account_number )){
		gtk_entry_set_text( GTK_ENTRY( widget ), priv->account_number );
	}

	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-account-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_account_select ), self );
}

static void
setup_settlement_selection( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	gint i, idx;
	GtkTreeIter iter;

	priv = self->priv;

	combo = my_utils_container_get_child_by_name( priv->top_box, "f3-settlement" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
					SET_N_COLUMNS,
					G_TYPE_INT, G_TYPE_STRING ));
	gtk_combo_box_set_model(GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SET_COL_LABEL );

	idx = -1;

	for( i=0 ; st_settlements[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SET_COL_CODE,  st_settlements[i].code,
				SET_COL_LABEL, gettext( st_settlements[i].label ),
				-1 );
		if( st_settlements[i].code == priv->settlement ){
			idx = i;
		}
	}

	gtk_widget_set_tooltip_text(
			combo, _( "Select the type of entries to be displayed" ));

	g_signal_connect(
			G_OBJECT( combo ), "changed", G_CALLBACK( on_settlement_changed ), self );

	if( idx != -1 ){
		gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), idx );
	}
}

static void
setup_signaling_connect( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	gulong handler;

	priv = self->priv;

	handler = g_signal_connect(
					G_OBJECT( priv->dossier ),
					SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_dossier_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( priv->dossier ),
					SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_dossier_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaPage *page )
{
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_SETTLEMENT( page ), NULL );

	return( NULL );
}

static void
on_account_changed( GtkEntry *entry, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	ofoAccount *account;

	priv = self->priv;
	priv->account_currency = NULL;

	g_free( priv->account_number );
	priv->account_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->dossier, priv->account_number );

	if( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account )){
		priv->account_currency = ofo_account_get_currency( account );

		gtk_label_set_text( GTK_LABEL( priv->account_label ), ofo_account_get_label( account ));
		try_display_entries( self );

	} else {
		gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	}

	set_settings( self );
}

static void
on_account_select( GtkButton *button, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	gchar *account_number;

	priv = self->priv;

	account_number = ofa_account_select_run(
							ofa_page_get_main_window( OFA_PAGE( self )),
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )),
							FALSE );
	if( account_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account_number );
		g_free( account_number );
	}
}

static void
on_settlement_changed( GtkComboBox *box, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel, *tsort, *tfilter;

	priv = self->priv;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				SET_COL_CODE, &priv->settlement,
				-1 );
		tsort = gtk_tree_view_get_model( priv->tview );
		tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));

		set_settings( self );
	}
}

/*
 * sorting the treeview
 *
 * actually sort the content of the store, as the entry itself is only
 * updated when it is about to be saved -
 * the difference is small, but actually exists
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_on_sort_model";
	ofaSettlementPrivate *priv;
	gint cmp, sort_column_id;
	gchar *sdopea, *sdeffa, *srefa, *slabela, *sleda, *sacca, *sdeba, *screa, *sstlmta;
	gchar *sdopeb, *sdeffb, *srefb, *slabelb, *sledb, *saccb, *sdebb, *screb, *sstlmtb;

	gtk_tree_model_get( tmodel, a,
			ENT_COL_DOPE,       &sdopea,
			ENT_COL_DEFF,       &sdeffa,
			ENT_COL_REF,        &srefa,
			ENT_COL_LABEL,      &slabela,
			ENT_COL_LEDGER,     &sleda,
			ENT_COL_ACCOUNT,    &sacca,
			ENT_COL_DEBIT,      &sdeba,
			ENT_COL_CREDIT,     &screa,
			ENT_COL_SETTLEMENT, &sstlmta,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ENT_COL_DOPE,       &sdopeb,
			ENT_COL_DEFF,       &sdeffb,
			ENT_COL_REF,        &srefb,
			ENT_COL_LABEL,      &slabelb,
			ENT_COL_LEDGER,     &sledb,
			ENT_COL_ACCOUNT,    &saccb,
			ENT_COL_DEBIT,      &sdebb,
			ENT_COL_CREDIT,     &screb,
			ENT_COL_SETTLEMENT, &sstlmtb,
			-1 );

	cmp = 0;
	priv = self->priv;

	switch( priv->sort_column_id ){
		case ENT_COL_DOPE:
			cmp = my_date_compare_by_str( sdopea, sdopeb, MY_DATE_DMYY );
			break;
		case ENT_COL_DEFF:
			cmp = my_date_compare_by_str( sdeffa, sdeffb, MY_DATE_DMYY );
			break;
		case ENT_COL_REF:
			cmp = cmp_strings( self, srefa, srefb );
			break;
		case ENT_COL_LABEL:
			cmp = cmp_strings( self, slabela, slabelb );
			break;
		case ENT_COL_LEDGER:
			cmp = cmp_strings( self, sleda, sledb );
			break;
		case ENT_COL_ACCOUNT:
			cmp = cmp_strings( self, sacca, saccb );
			break;
		case ENT_COL_DEBIT:
			cmp = cmp_amounts( self, sdeba, sdebb );
			break;
		case ENT_COL_CREDIT:
			cmp = cmp_amounts( self, screa, screb );
			break;
		case ENT_COL_SETTLEMENT:
			cmp = cmp_counters( self, sstlmta, sstlmtb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, sort_column_id );
			break;
	}

	g_free( sdopea );
	g_free( sdopeb );
	g_free( sdeffa );
	g_free( sdeffb );
	g_free( srefa );
	g_free( srefb );
	g_free( slabela );
	g_free( slabelb );
	g_free( sleda );
	g_free( sledb );
	g_free( sacca );
	g_free( saccb );
	g_free( sdeba );
	g_free( screa );
	g_free( sdebb );
	g_free( screb );
	g_free( sstlmta );
	g_free( sstlmtb );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static gint
cmp_strings( ofaSettlement *self, const gchar *stra, const gchar *strb )
{
	if( !stra || !g_utf8_strlen( stra, -1 )){
		if( !strb || !g_utf8_strlen( strb, -1 )){
			/* the two strings are both empty */
			return( 0 );
		}
		/* a is empty while b is set */
		return( -1 );
	} else if( !strb || !g_utf8_strlen( strb, -1 )){
		/* a is set while b is empty */
		return( 1 );
	}

	/* both a and b are set */
	return( g_utf8_collate( stra, strb ));
}

static gint
cmp_amounts( ofaSettlement *self, const gchar *stra, const gchar *strb )
{
	ofxAmount a, b;

	if( !stra || !g_utf8_strlen( stra, -1 )){
		if( !strb || !g_utf8_strlen( strb, -1 )){
			/* the two strings are both empty */
			return( 0 );
		}
		/* a is empty while b is set */
		return( -1 );
	} else if( !strb || !g_utf8_strlen( strb, -1 )){
		/* a is set while b is empty */
		return( 1 );
	}

	/* both a and b are set */
	a = my_double_set_from_str( stra );
	b = my_double_set_from_str( strb );

	return( a < b ? -1 : ( a > b ? 1 : 0 ));
}

static gint
cmp_counters( ofaSettlement *self, const gchar *stra, const gchar *strb )
{
	ofxCounter a, b;

	if( !stra || !g_utf8_strlen( stra, -1 )){
		if( !strb || !g_utf8_strlen( strb, -1 )){
			/* the two strings are both empty */
			return( 0 );
		}
		/* a is empty while b is set */
		return( -1 );
	} else if( !strb || !g_utf8_strlen( strb, -1 )){
		/* a is set while b is empty */
		return( 1 );
	}

	/* both a and b are set */
	a = atol( stra );
	b = atol( strb );

	return( a < b ? -1 : ( a > b ? 1 : 0 ));
}

/*
 * a row is visible if it is consistant with the selected settlement status
 */
static gboolean
is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	gboolean visible;
	ofoEntry *entry;
	gint entry_set_number;
	const gchar *ent_account;

	priv = self->priv;
	visible = FALSE;

	/* make sure an account is selected */
	if( !my_strlen( priv->account_number )){
		return( FALSE );
	}

	gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );
	if( entry ){
		g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
		g_object_unref( entry );

		if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
			return( FALSE );
		}

		ent_account = ofo_entry_get_account( entry );
		if( g_utf8_collate( ent_account, priv->account_number )){
			return( FALSE );
		}

		entry_set_number = ofo_entry_get_settlement_number( entry );

		switch( priv->settlement ){
			case ENT_SETTLEMENT_YES:
				visible = ( entry_set_number > 0 );
				break;
			case ENT_SETTLEMENT_NO:
				visible = ( entry_set_number <= 0 );
				break;
			case ENT_SETTLEMENT_ALL:
				visible = TRUE;
				break;
			default:
				break;
		}
	}

	return( visible );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
on_header_clicked( GtkTreeViewColumn *column, ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_on_header_clicked";
	ofaSettlementPrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;
	GtkTreeModel *tsort;

	priv = self->priv;

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	tsort = gtk_tree_view_get_model( priv->tview );
	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( tsort ), new_column_id, sort_order );

	g_debug( "%s: setting new_column_id=%u, new_sort_order=%s",
			thisfn, new_column_id,
			sort_order == OFA_SORT_ASCENDING ? "OFA_SORT_ASCENDING":"OFA_SORT_DESCENDING" );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;

	set_settings( self );
}

/*
 * at least a settlement status must be set
 */
static gboolean
settlement_status_is_valid( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;

	priv = self->priv;

	return( priv->settlement > ENT_SETTLEMENT_FIRST &&
				priv->settlement < ENT_SETTLEMENT_LAST );
}

static void
try_display_entries( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GList *entries;

	priv = self->priv;

	if( priv->account_number &&
			settlement_status_is_valid( self ) &&
			priv->tview && GTK_IS_TREE_VIEW( priv->tview )){

		entries = ofo_entry_get_dataset_by_account( priv->dossier, priv->account_number );
		display_entries( self, entries );
		ofo_entry_free_dataset( entries );
	}
}

static void
display_entries( ofaSettlement *self, GList *entries )
{
	ofaSettlementPrivate *priv;
	GtkTreeModel *tsort, *tfilter, *tstore;
	GList *iset;

	priv = self->priv;

	tsort = gtk_tree_view_get_model( priv->tview );
	tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
	tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
	gtk_list_store_clear( GTK_LIST_STORE( tstore ));

	for( iset=entries ; iset ; iset=iset->next ){
		display_entry( self, tstore, OFO_ENTRY( iset->data ));
	}
}

static void
display_entry( ofaSettlement *self, GtkTreeModel *tmodel, ofoEntry *entry )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( tmodel ), &iter, -1 );
	set_row_entry( self, tmodel, &iter, entry );
}

static void
set_row_entry( ofaSettlement *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoEntry *entry )
{
	gchar *sdope, *sdeff, *sdeb, *scre, *str_number;
	gdouble amount;
	gint set_number;

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), MY_DATE_DMYY );
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
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
	set_number = ofo_entry_get_settlement_number( entry );
	if( set_number <= 0 ){
		str_number = g_strdup( "" );
	} else {
		str_number = g_strdup_printf( "%u", set_number );
	}

	gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				iter,
				ENT_COL_DOPE,       sdope,
				ENT_COL_DEFF,       sdeff,
				ENT_COL_NUMBER,     ofo_entry_get_number( entry ),
				ENT_COL_REF,        ofo_entry_get_ref( entry ),
				ENT_COL_LABEL,      ofo_entry_get_label( entry ),
				ENT_COL_LEDGER,     ofo_entry_get_ledger( entry ),
				ENT_COL_ACCOUNT,    ofo_entry_get_account( entry ),
				ENT_COL_DEBIT,      sdeb,
				ENT_COL_CREDIT,     scre,
				ENT_COL_SETTLEMENT, str_number,
				ENT_COL_OBJECT,     entry,
				-1 );

	g_free( str_number );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );
}

/*
 * recompute the balance per currency each time the selection changes
 */
static void
on_entries_treeview_selection_changed( GtkTreeSelection *select, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	sEnumSelected ses;
	gchar *samount;
	GdkRGBA color_blue, color_warn;

	priv = self->priv;

	gdk_rgba_parse( &color_blue, RGBA_NORMAL );
	gdk_rgba_parse( &color_warn, RGBA_WARNING );

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	gtk_tree_selection_selected_foreach( select, ( GtkTreeSelectionForeachFunc ) enum_selected, &ses );

	gtk_widget_set_sensitive( priv->settle_btn, ses.unsettled > 0 );
	gtk_widget_set_sensitive( priv->unsettle_btn, ses.settled > 0 );

	samount = my_double_to_str( ses.debit );
	gtk_widget_override_color(
			priv->debit_balance,
			GTK_STATE_FLAG_NORMAL, ses.debit == ses.credit ? &color_blue : &color_warn );
	gtk_label_set_text( GTK_LABEL( priv->debit_balance ), samount );
	g_free( samount );

	samount = my_double_to_str( ses.credit );
	gtk_widget_override_color(
			priv->credit_balance,
			GTK_STATE_FLAG_NORMAL, ses.debit == ses.credit ? &color_blue : &color_warn );
	gtk_label_set_text( GTK_LABEL( priv->credit_balance ), samount );
	g_free( samount );

	gtk_label_set_text( GTK_LABEL( priv->currency_balance ), priv->account_currency );
}

/*
 * a function called each time the selection is changed, for each selected row
 */
static void
enum_selected( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, sEnumSelected *ses )
{
	gchar *sdeb, *scre, *snum;
	gdouble amount;
	gint number;

	ses->rows += 1;

	gtk_tree_model_get( tmodel, iter,
			ENT_COL_DEBIT,      &sdeb,
			ENT_COL_CREDIT,     &scre,
			ENT_COL_SETTLEMENT, &snum,
			-1 );

	number = atoi( snum );
	if( number > 0 ){
		ses->settled += 1;
	} else {
		ses->unsettled += 1;
	}

	amount = my_double_set_from_str( sdeb );
	ses->debit += amount;

	amount = my_double_set_from_str( scre );
	ses->credit += amount;

	g_free( sdeb );
	g_free( scre );
	g_free( snum );
}

static void
on_settle_clicked( GtkButton *button, ofaSettlement *self )
{
	update_selection( self, TRUE );
}

static void
on_unsettle_clicked( GtkButton *button, ofaSettlement *self )
{
	update_selection( self, FALSE );
}

/*
 * we update here the rows to settled/unsettled
 * due to the GtkTreeModelFilter, this may lead the updated row to
 * disappear from the view -> so update based on GtkListStore iters
 */
static void
update_selection( ofaSettlement *self, gboolean settle )
{
	ofaSettlementPrivate *priv;
	GtkTreeSelection *select;
	sEnumSelected ses;
	GList *selected_paths, *ipath;
	GtkTreeModel *tsort, *tfilter, *tstore;
	gchar *path_str;
	GtkTreeIter sort_iter, filter_iter, *store_iter;
	GList *list_iters, *it;

	priv = self->priv;

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	if( settle ){
		ses.set_number = ofo_dossier_get_next_settlement( priv->dossier );
	} else {
		ses.set_number = -1;
	}

	select = gtk_tree_view_get_selection( priv->tview );
	selected_paths = gtk_tree_selection_get_selected_rows( select, &tsort );
	tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
	tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
	list_iters = NULL;

	/* convert the list of selected path on the tsort to a list of
	 * selected iters on the underlying tstore */
	for( ipath=selected_paths ; ipath ; ipath=ipath->next ){
		path_str = gtk_tree_path_to_string( ( GtkTreePath * ) ipath->data );
		if( !gtk_tree_model_get_iter_from_string( tsort, &sort_iter, path_str )){
			g_return_if_reached();
		}
		g_free( path_str );
		gtk_tree_model_sort_convert_iter_to_child_iter(
				GTK_TREE_MODEL_SORT( tsort ), &filter_iter, &sort_iter );
		store_iter = g_new0( GtkTreeIter, 1 );
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( tfilter ), store_iter, &filter_iter );
		list_iters = g_list_append( list_iters, store_iter );
	}
	g_list_free_full( selected_paths, ( GDestroyNotify ) gtk_tree_path_free );

	/* now update the rows based on this list */
	for( it=list_iters ; it ; it=it->next ){
		update_row( tstore, ( GtkTreeIter * ) it->data, &ses );
	}
	g_list_free_full( list_iters, ( GDestroyNotify ) g_free );

	gtk_widget_set_sensitive( priv->settle_btn, ses.unsettled > 0 );
	gtk_widget_set_sensitive( priv->unsettle_btn, ses.settled > 0 );

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));
}

/*
 * enumeration called when we are clicking on 'settle' or 'unsettle'
 * button
 *
 * @tstore: the underlying store tree model
 * @iter: an iter on this model
 */
static void
update_row( GtkTreeModel *tstore, GtkTreeIter *iter, sEnumSelected *ses )
{
	ofaSettlementPrivate *priv;
	ofoEntry *entry;
	gint number;
	gchar *snum;

	priv = ses->self->priv;

	/* get the object and update it, according to the clicked button */

	gtk_tree_model_get( tstore, iter, ENT_COL_OBJECT, &entry, -1 );
	g_object_unref( entry );

	ofo_entry_update_settlement( entry, priv->dossier, ses->set_number );

	number = ofo_entry_get_settlement_number( entry );
	if( number > 0 ){
		snum = g_strdup_printf( "%u", number );
	} else {
		snum = g_strdup( "" );
	}

	gtk_list_store_set( GTK_LIST_STORE( tstore ), iter,
				ENT_COL_SETTLEMENT, snum,
				-1 );

	g_free( snum );

	/* update counters in the structure */
	enum_selected( tstore, NULL, iter, ses );
}

/*
 * settings: account;mode;sort_column_id;sort_sens;
 */
static void
get_settings( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = self->priv;

	slist = ofa_settings_get_string_list( st_pref_settlement );
	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->account_number );
		priv->account_number = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->settlement = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_sens = atoi( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%s;%d;%d;%d;",
			priv->account_number, priv->settlement, priv->sort_column_id, priv->sort_sens );

	ofa_settings_set_string( st_pref_settlement, str );

	g_free( str );
}

static void
on_dossier_new_object( ofoDossier *dossier, ofoBase *object, ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_on_dossier_new_object";

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
 * insert the new entry in the list store if it is registered on the
 * currently selected account
 */
static void
on_new_entry( ofaSettlement *self, ofoEntry *entry )
{
	ofaSettlementPrivate *priv;
	const gchar *entry_account;
	GtkTreeModel *tsort, *tfilter, *tstore;

	priv = self->priv;
	entry_account = ofo_entry_get_account( entry );

	if( !g_utf8_collate( priv->account_number, entry_account )){
		tsort = gtk_tree_view_get_model( priv->tview );
		tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
		display_entry( self, tstore, entry );
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));
	}
}

/*
 * a ledger mnemo, an account number, a currency code may has changed
 */
static void
on_dossier_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_on_dossier_updated_object";

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
on_updated_entry( ofaSettlement *self, ofoEntry *entry )
{
	ofaSettlementPrivate *priv;
	const gchar *entry_account;
	GtkTreeModel *tsort, *tfilter, *tstore;
	GtkTreeIter iter;

	priv = self->priv;
	entry_account = ofo_entry_get_account( entry );

	if( !g_utf8_collate( priv->account_number, entry_account )){
		tsort = gtk_tree_view_get_model( priv->tview );
		tfilter = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( tsort ));
		tstore = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
		if( find_entry_by_number( self, tstore, ofo_entry_get_number( entry ), &iter )){
			set_row_entry( self, tstore, &iter, entry );
			gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( tfilter ));

		/* the entry was not present, but may appear depending of the
		 * actual modification */
		} else {
			try_display_entries( self );
		}
	}
}

/*
 * return TRUE if we have found the requested entry row
 */
static gboolean
find_entry_by_number( ofaSettlement *self, GtkTreeModel *tmodel, ofxCounter entry_number, GtkTreeIter *iter )
{
	ofxCounter row_number;

	if( gtk_tree_model_get_iter_first( tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( tmodel, iter, ENT_COL_NUMBER, &row_number, -1 );
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
