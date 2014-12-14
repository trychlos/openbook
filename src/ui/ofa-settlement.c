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

/* priv instance data
 */
struct _ofaSettlementPrivate {

	/* internals
	 */
	ofoDossier        *dossier;			/* dossier */
	gchar             *account_number;
	ofaEntrySettlement settlement;

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
	GtkWidget         *debit_entry;
	GtkWidget         *credit_entry;
};

/* columns in the combo box which let us select which type of entries
 * are displayed
 */
enum {
	SET_COL_CODE = 0,
	SET_COL_ORIG,
	SET_COL_LABEL,
	SET_N_COLUMNS
};

typedef struct {
	gint         code;
	const gchar *orig;
	const gchar *label;
}
	sSettlement;

static const sSettlement st_settlements[] = {
		{ ENT_SETTLEMENT_YES, "Settled",   N_( "Settled entries" ) },
		{ ENT_SETTLEMENT_NO,  "Unsettled", N_( "Unsettled entries" ) },
		{ ENT_SETTLEMENT_ALL, "All",       N_( "All entries" ) },
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

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-settlement.piece.ui";
static const gchar *st_ui_id            = "SettlementWindow";

static const gchar *st_pref_account     = "SettlementLastAccount";
static const gchar *st_pref_status      = "SettlementLastStatus";

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
static gboolean       is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlement *self );
static gboolean       settlement_status_is_valid( ofaSettlement *self );
static void           try_display_entries( ofaSettlement *self );
static void           display_entries( ofaSettlement *self, GList *entries );
static void           display_entry( ofaSettlement *self, GtkTreeModel *tmodel, ofoEntry *entry );
static void           on_entries_treeview_selection_changed( GtkTreeSelection *select, ofaSettlement *self );
static void           enum_selected( GtkTreeModel *tmodel, GtkTreePath *path, GtkTreeIter *iter, sEnumSelected *ses );
static void           on_settle_clicked( GtkButton *button, ofaSettlement *self );
static void           on_unsettle_clicked( GtkButton *button, ofaSettlement *self );
static void           update_selection( ofaSettlement *self, gboolean settle );
static void           update_row( GtkTreeModel *tmodel, GtkTreeIter *iter, sEnumSelected *ses );

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
	g_return_if_fail( OFA_IS_SETTLEMENT( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
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

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-settle" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_settle_clicked ), self );
	priv->settle_btn = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-unsettle" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_unsettle_clicked ), self );
	priv->unsettle_btn = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-debit" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->debit_entry = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "pt-credit" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->credit_entry = widget;
}

/*
 * the treeview is filtered on the settlement status
 */
static void
setup_entries_treeview( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkTreeView *tview;
	GtkTreeModel *tmodel, *tfilter;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	gint column_id;

	priv = self->priv;

	tview = ( GtkTreeView * ) my_utils_container_get_child_by_name( priv->top_box, "p1-entries" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			ENT_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,		/* dope, deff, number */
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

	gtk_tree_view_set_model( tview, tfilter );
	g_object_unref( tfilter );

	/* operation date
	 */
	column_id = ENT_COL_DOPE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );

	/* effect date
	 */
	column_id = ENT_COL_DEFF;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Effect" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );

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

	/* ledger
	 */
	column_id = ENT_COL_LEDGER;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ledger" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );

	/* account
	 */
	column_id = ENT_COL_ACCOUNT;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Account" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );

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

	/* settlement status
	 */
	column_id = ENT_COL_SETTLEMENT;
	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( text_cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Settlement" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_entries_treeview_selection_changed ), self );

	priv->tview = tview;
}

static void
setup_account_selection( ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkWidget *widget;
	gchar *text;

	priv = self->priv;

	/* label must be setup before entry may be changed */
	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->account_label = widget;

	widget = my_utils_container_get_child_by_name( priv->top_box, "f1-account-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_account_changed ), self );
	text = ofa_settings_get_string( st_pref_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( widget ), text );
	}
	g_free( text );
	priv->account_entry = widget;

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
	gchar *text;

	priv = self->priv;

	combo = my_utils_container_get_child_by_name( priv->top_box, "f3-settlement" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
					SET_N_COLUMNS,
					G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model(GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SET_COL_LABEL );

	text = ofa_settings_get_string( st_pref_status );
	idx = -1;

	for( i=0 ; st_settlements[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SET_COL_CODE,  st_settlements[i].code,
				SET_COL_ORIG,  st_settlements[i].orig,
				SET_COL_LABEL, gettext( st_settlements[i].label ),
				-1 );
		if( text && g_utf8_strlen( text, -1 ) && idx==-1 ){
			if( !g_utf8_collate( text, st_settlements[i].orig )){
				idx = i;
			}
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

	g_free( priv->account_number );
	priv->account_number = g_strdup( gtk_entry_get_text( entry ));

	account = ofo_account_get_by_number( priv->dossier, priv->account_number );

	if( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account )){
		gtk_label_set_text( GTK_LABEL( priv->account_label ), ofo_account_get_label( account ));
		try_display_entries( self );

	} else {
		gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	}
}

static void
on_account_select( GtkButton *button, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	gchar *account_number;

	priv = self->priv;

	account_number = ofa_account_select_run(
							ofa_page_get_main_window( OFA_PAGE( self )),
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( account_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account_number );
		ofa_settings_set_string( st_pref_account, account_number );
		g_free( account_number );
	}
}

static void
on_settlement_changed( GtkComboBox *box, ofaSettlement *self )
{
	ofaSettlementPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	const gchar *label;

	priv = self->priv;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				SET_COL_CODE, &priv->settlement,
				SET_COL_ORIG, &label,
				-1 );
		ofa_settings_set_string( st_pref_status, label );

		gtk_tree_model_filter_refilter(
				GTK_TREE_MODEL_FILTER( gtk_tree_view_get_model( priv->tview )));
	}
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

	priv = self->priv;
	visible = FALSE;

	gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_object_unref( entry );

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

	return( visible );
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

/*
 * the hash is used to store the balance per currency
 */
static void
display_entries( ofaSettlement *self, GList *entries )
{
	ofaSettlementPrivate *priv;
	GtkTreeModel *tfilter, *tmodel;
	GList *iset;

	priv = self->priv;

	tfilter = gtk_tree_view_get_model( priv->tview );
	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	for( iset=entries ; iset ; iset=iset->next ){
		display_entry( self, tmodel, OFO_ENTRY( iset->data ));
	}
}

static void
display_entry( ofaSettlement *self, GtkTreeModel *tmodel, ofoEntry *entry )
{
	GtkTreeIter iter;
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

	gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
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

	priv = self->priv;

	memset( &ses, '\0', sizeof( sEnumSelected ));
	ses.self = self;
	gtk_tree_selection_selected_foreach( select, ( GtkTreeSelectionForeachFunc ) enum_selected, &ses );

	gtk_widget_set_sensitive( priv->settle_btn, ses.unsettled > 0 );
	gtk_widget_set_sensitive( priv->unsettle_btn, ses.settled > 0 );

	samount = my_double_to_str( ses.debit );
	gtk_entry_set_text( GTK_ENTRY( priv->debit_entry ), samount );
	g_free( samount );

	samount = my_double_to_str( ses.credit );
	gtk_entry_set_text( GTK_ENTRY( priv->credit_entry ), samount );
	g_free( samount );
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
	GtkTreeModel *tfilter, *tmodel;
	gchar *path_str;
	GtkTreeIter filter_iter, *model_iter;
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
	selected_paths = gtk_tree_selection_get_selected_rows( select, &tfilter );
	tmodel = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tfilter ));
	list_iters = NULL;

	/* convert the list of selected path on the tfilter to a list of
	 * selected iters on the underlying tmodel (tstore) */
	for( ipath=selected_paths ; ipath ; ipath=ipath->next ){
		path_str = gtk_tree_path_to_string( ( GtkTreePath * ) ipath->data );
		if( !gtk_tree_model_get_iter_from_string( tfilter, &filter_iter, path_str )){
			g_return_if_reached();
		}
		g_free( path_str );
		model_iter = g_new0( GtkTreeIter, 1 );
		gtk_tree_model_filter_convert_iter_to_child_iter(
				GTK_TREE_MODEL_FILTER( tfilter ), model_iter, &filter_iter );
		list_iters = g_list_append( list_iters, model_iter );
	}
	g_list_free_full( selected_paths, ( GDestroyNotify ) gtk_tree_path_free );

	/* now update the rows based on this list */
	for( it=list_iters ; it ; it=it->next ){
		update_row( tmodel, ( GtkTreeIter * ) it->data, &ses );
	}
	g_list_free_full( list_iters, ( GDestroyNotify ) g_free );

	gtk_widget_set_sensitive( priv->settle_btn, ses.unsettled > 0 );
	gtk_widget_set_sensitive( priv->unsettle_btn, ses.settled > 0 );

	gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( gtk_tree_view_get_model( priv->tview )));
}

/*
 * enumeration called when we are clicking on 'settle' or 'unsettle'
 * button
 *
 * @tmodel: the underlying store tree model
 * @iter: an iter on this model
 */
static void
update_row( GtkTreeModel *tmodel, GtkTreeIter *iter, sEnumSelected *ses )
{
	ofaSettlementPrivate *priv;
	ofoEntry *entry;
	gint number;
	gchar *snum;

	priv = ses->self->priv;

	/* get the object and update it, according to the clicked button */

	gtk_tree_model_get( tmodel, iter, ENT_COL_OBJECT, &entry, -1 );
	g_object_unref( entry );

	ofo_entry_update_settlement( entry, priv->dossier, ses->set_number );

	number = ofo_entry_get_settlement_number( entry );
	if( number > 0 ){
		snum = g_strdup_printf( "%u", number );
	} else {
		snum = g_strdup( "" );
	}

	gtk_list_store_set( GTK_LIST_STORE( tmodel ), iter,
				ENT_COL_SETTLEMENT, snum,
				-1 );

	g_free( snum );

	/* update counters in the structure */
	enum_selected( tmodel, NULL, iter, ses );
}
