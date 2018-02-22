/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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

#include "my/my-date.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-operation-group.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-counters.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-currency.h"

#include "core/ofa-entry-properties.h"
#include "core/ofa-entry-store.h"
#include "core/ofa-entry-treeview.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-reconcil-group.h"
#include "core/ofa-settlement-group.h"
#include "core/ofa-settlement-page.h"

/* when enumerating the selected rows
 * this structure is maintained each time the selection changes
 * and is later used when settling or unsettling the selection
 */
typedef struct {
	guint       rows;					/* count of. */
	guint       settled;				/* count of. */
	guint       unsettled;				/* count of. */
	ofsCurrency scur;
}
	sEnumSelected;

/* priv instance data
 */
typedef struct {

	/* runtime
	 */
	ofaIGetter        *getter;
	gchar             *settings_prefix;
	GList             *store_handlers;

	/* UI
	 */
	GtkWidget         *paned;
	ofaEntryTreeview  *tview;
	ofaEntryStore     *store;

	/* frame 1: account selection
	 */
	GtkWidget         *account_entry;
	GtkWidget         *account_label;
	gchar             *account_number;
	ofoCurrency       *account_currency;

	/* frame 2: filtering mode
	 */
	GtkWidget         *filter_combo;
	gchar             *filter_id;

	/* footer
	 */
	GtkWidget         *footer_msg;
	GtkWidget         *footer_paned;
	GtkWidget         *debit_balance;
	GtkWidget         *credit_balance;
	GtkWidget         *light_balance;

	/* actions
	 */
	GSimpleAction     *edit_action;
	GSimpleAction     *settle_action;
	GSimpleAction     *unsettle_action;
	GSimpleAction     *vope_action;
	GSimpleAction     *vconcil_action;
	GSimpleAction     *vsettle_action;

	/* settlement button:
	 * when clicked with <Ctrl>, then does not check for selection balance
	 */
	gboolean           ctrl_on_pressed;
	gboolean           ctrl_pressed;

	/* selection management
	 */
	sEnumSelected      ses;
	ofxCounter         snumber;
	gboolean           updating;
	GList             *sel_opes;
	ofxCounter         sel_concil_id;
	ofxCounter         sel_settle_id;
}
	ofaSettlementPagePrivate;

/* filtering the EntryTreeview:
 * - displaying only settled entries
 * - displaying only unsettled ones
 * - displaying all
 * - displaying unsettled + the entries which have been settled this day
 */
enum {
	STLMT_FILTER_YES = 1,
	STLMT_FILTER_NO,
	STLMT_FILTER_ALL,
	STLMT_FILTER_SESSION
};

/* columns in the filtering combo box which let us select which type of
 * entries are displayed
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
	sSettlementPage;

static const sSettlementPage st_settlements[] = {
		{ STLMT_FILTER_YES,     N_( "Settled entries" ) },
		{ STLMT_FILTER_NO,      N_( "Unsettled entries" ) },
		{ STLMT_FILTER_SESSION, N_( "Settlement session" ) },
		{ STLMT_FILTER_ALL,     N_( "All entries" ) },
		{ 0 }
};

#define COLOR_SETTLED                   "#e0e0e0"		/* light gray background */
#define COLOR_ERROR                     "#ff0000"		/* red */
#define COLOR_WARNING                   "#ff8000"		/* orange */
#define COLOR_INFO                      "#0000ff"		/* blue */

static const gchar *st_resource_ui           = "/org/trychlos/openbook/core/ofa-settlement-page.ui";
static const gchar *st_resource_light_green  = "/org/trychlos/openbook/core/ofa-settlement-page-light-green-14.png";
static const gchar *st_resource_light_yellow = "/org/trychlos/openbook/core/ofa-settlement-page-light-yellow-14.png";
static const gchar *st_resource_light_empty  = "/org/trychlos/openbook/core/ofa-settlement-page-light-empty-14.png";
static const gchar *st_ui_name1              = "SettlementPageView1";
static const gchar *st_ui_name2              = "SettlementPageView2";

static void       paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static GtkWidget *setup_view1( ofaSettlementPage *self );
static void       setup_footer( ofaSettlementPage *self, GtkContainer *parent );
static void       setup_treeview( ofaSettlementPage *self, GtkContainer *parent );
static void       tview_on_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self );
static gboolean   tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self );
static gboolean   tview_is_session_settled( ofaSettlementPage *self, ofoEntry *entry );
static void       tview_on_row_selected( ofaEntryTreeview *view, GList *selected, ofaSettlementPage *self );
static void       tview_enum_selected( ofoEntry *entry, ofaSettlementPage *self );
static GtkWidget *setup_view2( ofaSettlementPage *self );
static void       setup_account_selection( ofaSettlementPage *self, GtkContainer *parent );
static void       setup_settlement_selection( ofaSettlementPage *self, GtkContainer *parent );
static void       setup_actions( ofaSettlementPage *self, GtkContainer *parent );
static void       paned_page_v_init_view( ofaPanedPage *page );
static void       on_account_changed( GtkEntry *entry, ofaSettlementPage *self );
static void       on_settlement_changed( GtkComboBox *box, ofaSettlementPage *self );
static void       action_on_edit_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       do_edit( ofaSettlementPage *self, ofoEntry *entry );
static gboolean   settle_on_pressed( GtkWidget *button, GdkEvent *event, ofaSettlementPage *self );
static gboolean   settle_on_released( GtkWidget *button, GdkEvent *event, ofaSettlementPage *self );
static void       action_on_settle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static gboolean   do_settle_user_confirm( ofaSettlementPage *self );
static void       action_on_unsettle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       update_selection( ofaSettlementPage *self, gboolean settle );
static void       update_row_enum( ofoEntry *entry, ofaSettlementPage *self );
static void       refresh_selection_compute( ofaSettlementPage *self );
static void       refresh_selection_compute_with_selected( ofaSettlementPage *self, GList *selected );
static void       refresh_display( ofaSettlementPage *self );
static void       action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       action_on_vsettle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self );
static void       set_message( ofaSettlementPage *self, const gchar *msg, const gchar *color );
static void       read_settings( ofaSettlementPage *self );
static void       write_settings( ofaSettlementPage *self );
static void       store_on_changed( ofaEntryStore *store, ofaSettlementPage *self );

G_DEFINE_TYPE_EXTENDED( ofaSettlementPage, ofa_settlement_page, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaSettlementPage ))

static void
settlement_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_settlement_page_finalize";
	ofaSettlementPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_SETTLEMENT_PAGE( instance ));

	/* free data members here */
	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->account_number );
	g_free( priv->filter_id );
	g_list_free( priv->sel_opes );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_page_parent_class )->finalize( instance );
}

static void
settlement_page_dispose( GObject *instance )
{
	ofaSettlementPagePrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_SETTLEMENT_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_SETTLEMENT_PAGE( instance ));

		priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( instance ));

		/* disconnect ofaEntryStore signal handlers */
		for( it=priv->store_handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
		}
		g_list_free( priv->store_handlers );

		/* unref object members here */
		g_object_unref( priv->edit_action );
		g_object_unref( priv->settle_action );
		g_object_unref( priv->unsettle_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_page_parent_class )->dispose( instance );
}

static void
ofa_settlement_page_init( ofaSettlementPage *self )
{
	static const gchar *thisfn = "ofa_settlement_page_init";
	ofaSettlementPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_SETTLEMENT_PAGE( self ));

	priv = ofa_settlement_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->account_number = NULL;
	priv->filter_id = g_strdup_printf( "%d", STLMT_FILTER_ALL );
}

static void
ofa_settlement_page_class_init( ofaSettlementPageClass *klass )
{
	static const gchar *thisfn = "ofa_settlement_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settlement_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = settlement_page_finalize;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;
}

static void
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_settlement_page_v_setup_view";
	ofaSettlementPagePrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p, paned=%p", thisfn, ( void * ) page, ( void * ) paned );

	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	priv->paned = GTK_WIDGET( paned );

	view = setup_view1( OFA_SETTLEMENT_PAGE( page ));
	gtk_paned_pack1( paned, view, TRUE, FALSE );

	view = setup_view2( OFA_SETTLEMENT_PAGE( page ));
	gtk_paned_pack2( paned, view, FALSE, FALSE );
}

static GtkWidget *
setup_view1( ofaSettlementPage *self )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name1, "top1" );

	/* build first the targets of the data, and only after the triggers */
	setup_footer( self, GTK_CONTAINER( box ));
	setup_treeview( self, GTK_CONTAINER( box ));

	return( box );
}

static void
setup_footer( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_settlement_page_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( parent, "footer-paned" );
	g_return_if_fail( widget && GTK_IS_PANED( widget ));
	priv->footer_paned = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-msg" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->footer_msg = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-debit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->debit_balance = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-credit" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->credit_balance = widget;

	widget = my_utils_container_get_child_by_name( parent, "footer-light" );
	g_return_if_fail( widget && GTK_IS_IMAGE( widget ));
	priv->light_balance = widget;
	gtk_image_set_from_resource( GTK_IMAGE( priv->light_balance ), st_resource_light_empty );
}

/*
 * the treeview is filtered on the settlement status
 */
static void
setup_treeview( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *tview_parent;

	priv = ofa_settlement_page_get_instance_private( self );

	tview_parent = my_utils_container_get_child_by_name( parent, "entry-treeview" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_entry_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));
	ofa_entry_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, self );
	ofa_tvbin_set_cell_data_func( OFA_TVBIN( priv->tview ), ( GtkTreeCellDataFunc ) tview_on_cell_data_func, self );

	/* insertion/delete and activation are not handled here */
	g_signal_connect( priv->tview, "ofa-entchanged", G_CALLBACK( tview_on_row_selected ), self );
}

/*
 * light gray background on settled entries
 */
static void
tview_on_cell_data_func( GtkTreeViewColumn *tcolumn,
						GtkCellRendererText *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
						ofaSettlementPage *self )
{
	ofoEntry *entry;
	ofxCounter number;
	GdkRGBA color;

	g_return_if_fail( tcolumn && GTK_IS_TREE_VIEW_COLUMN( tcolumn ));
	g_return_if_fail( GTK_IS_CELL_RENDERER( cell ));
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));
	g_return_if_fail( self && OFA_IS_SETTLEMENT_PAGE( self ));

	g_object_set( G_OBJECT( cell ), "cell-background-set", FALSE, NULL );

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_OBJECT, &entry, -1 );
	if( entry ){
		g_return_if_fail( OFO_IS_ENTRY( entry ));
		g_object_unref( entry );

		number = ofo_entry_get_settlement_number( entry );

		if( number > 0 ){
			gdk_rgba_parse( &color, COLOR_SETTLED );
			g_object_set( G_OBJECT( cell ), "cell-background-rgba", &color, NULL );
		}
	}
}

/*
 * A row is visible if it is consistant with the selected settlement
 * account and status.
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gboolean visible;
	ofoEntry *entry;
	gint entry_set_number;
	const gchar *ent_account;
	guint filter_int;

	priv = ofa_settlement_page_get_instance_private( self );

	visible = FALSE;

	/* make sure an account is selected */
	if( !my_strlen( priv->account_number )){
		return( FALSE );
	}

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_OBJECT, &entry, -1 );
	if( entry ){
		g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
		g_object_unref( entry );

		if( ofo_entry_get_status( entry ) == ENT_STATUS_DELETED ){
			return( FALSE );
		}

		ent_account = ofo_entry_get_account( entry );
		if( my_collate( ent_account, priv->account_number )){
			return( FALSE );
		}

		filter_int = atoi( priv->filter_id );
		entry_set_number = ofo_entry_get_settlement_number( entry );

		switch( filter_int ){
			case STLMT_FILTER_YES:
				visible = ( entry_set_number > 0 );
				break;
			case STLMT_FILTER_NO:
				visible = ( entry_set_number <= 0 );
				break;
			case STLMT_FILTER_SESSION:
				if( entry_set_number <= 0 ){
					visible = TRUE;
				} else {
					visible = tview_is_session_settled( self, entry );
				}
				break;
			case STLMT_FILTER_ALL:
				visible = TRUE;
				break;
			default:
				break;
		}
	}

	return( visible );
}

static gboolean
tview_is_session_settled( ofaSettlementPage *self, ofoEntry *entry )
{
	gboolean is_session;
	const GTimeVal *stamp;
	GDate date, dnow;

	stamp = ofo_entry_get_settlement_stamp( entry );
	my_date_set_from_stamp( &date, stamp );
	my_date_set_now( &dnow );
	is_session = my_date_compare( &date, &dnow ) == 0;

	return( is_session );
}

/*
 * recompute the balance per currency each time the selection changes
 */
static void
tview_on_row_selected( ofaEntryTreeview *view, GList *selected, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gboolean ventry_enabled, vope_enabled, vconcil_enabled, vsettle_enabled;
	ofoConcil *concil;
	ofxCounter openum;

	priv = ofa_settlement_page_get_instance_private( self );

	ventry_enabled = FALSE;
	vope_enabled = FALSE;
	vconcil_enabled = FALSE;
	vsettle_enabled = FALSE;

	ventry_enabled = ( g_list_length( selected ) == 1 );
	if( ventry_enabled ){
		openum = ofo_entry_get_ope_number( OFO_ENTRY( selected->data ));
		vope_enabled = ( openum > 0 );
		g_list_free( priv->sel_opes );
		priv->sel_opes = openum > 0 ? g_list_append( NULL, ( gpointer ) openum ) : NULL;
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( OFO_ENTRY( selected->data )));
		priv->sel_concil_id = ( concil ? ofo_concil_get_id( concil ) : 0 );
		vconcil_enabled = ( priv->sel_concil_id > 0 );
		priv->sel_settle_id = ofo_entry_get_settlement_number( OFO_ENTRY( selected->data ));
		vsettle_enabled = ( priv->sel_settle_id > 0 );
	}

	g_simple_action_set_enabled( priv->edit_action, ventry_enabled );
	g_simple_action_set_enabled( priv->vope_action, vope_enabled );
	g_simple_action_set_enabled( priv->vconcil_action, vconcil_enabled );
	g_simple_action_set_enabled( priv->vsettle_action, vsettle_enabled );

	priv->updating = FALSE;
	refresh_selection_compute_with_selected( self, selected );
}

/*
 * a function called each time the selection is changed, for each selected row
 */
static void
tview_enum_selected( ofoEntry *entry, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	ofxCounter stlmt_number;
	ofxAmount debit, credit;

	priv = ofa_settlement_page_get_instance_private( self );

	priv->ses.rows += 1;

	stlmt_number = ofo_entry_get_settlement_number( entry );
	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	if( stlmt_number > 0 ){
		priv->ses.settled += 1;
	} else {
		priv->ses.unsettled += 1;
	}

	priv->ses.scur.debit += debit;
	priv->ses.scur.credit += credit;
}

static GtkWidget *
setup_view2( ofaSettlementPage *self )
{
	GtkWidget *box;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_container_attach_from_resource( GTK_CONTAINER( box ), st_resource_ui, st_ui_name2, "top2" );

	setup_settlement_selection( self, GTK_CONTAINER( box ));
	setup_account_selection( self, GTK_CONTAINER( box ));
	setup_actions( self, GTK_CONTAINER( box ));

	return( box );
}

static void
setup_account_selection( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *widget;

	priv = ofa_settlement_page_get_instance_private( self );

	/* label must be setup before entry may be changed */
	widget = my_utils_container_get_child_by_name( parent, "account-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->account_label = widget;

	widget = my_utils_container_get_child_by_name( parent, "account-number" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->account_entry = widget;
	g_signal_connect( widget, "changed", G_CALLBACK( on_account_changed ), self );
	ofa_account_editable_init( GTK_EDITABLE( widget ), priv->getter, ACCOUNT_ALLOW_SETTLEABLE );
}

static void
setup_settlement_selection( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	gint i;
	GtkTreeIter iter;
	gchar *str;

	priv = ofa_settlement_page_get_instance_private( self );

	combo = my_utils_container_get_child_by_name( parent, "entries-filter" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	priv->filter_combo = combo;

	label = my_utils_container_get_child_by_name( parent, "entries-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
					SET_N_COLUMNS,
					G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	gtk_combo_box_set_id_column( GTK_COMBO_BOX( combo ), SET_COL_CODE );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SET_COL_LABEL );

	for( i=0 ; st_settlements[i].code ; ++i ){
		str = g_strdup_printf( "%d", st_settlements[i].code );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SET_COL_CODE,  str,
				SET_COL_LABEL, gettext( st_settlements[i].label ),
				-1 );
		g_free( str );
	}

	g_signal_connect( combo, "changed", G_CALLBACK( on_settlement_changed ), self );
}

static void
setup_actions( ofaSettlementPage *self, GtkContainer *parent )
{
	ofaSettlementPagePrivate *priv;
	GtkWidget *button;

	priv = ofa_settlement_page_get_instance_private( self );

	/* edit action */
	priv->edit_action = g_simple_action_new( "edit", NULL );
	g_signal_connect( priv->edit_action, "activate", G_CALLBACK( action_on_edit_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->edit_action ),
			_( "View/edit properties..." ));
	g_simple_action_set_enabled( priv->edit_action, FALSE );

	/* settle action */
	priv->settle_action = g_simple_action_new( "settle", NULL );
	g_signal_connect( priv->settle_action, "activate", G_CALLBACK( action_on_settle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->settle_action ),
			_( "Settle the selection" ));
	button = my_utils_container_get_child_by_name( parent, "settle-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->settle_action ));

	g_signal_connect( button, "button-press-event", G_CALLBACK( settle_on_pressed ), self );
	g_signal_connect( button, "button-release-event", G_CALLBACK( settle_on_released ), self );

	/* unsettle action */
	priv->unsettle_action = g_simple_action_new( "unsettle", NULL );
	g_signal_connect( priv->unsettle_action, "activate", G_CALLBACK( action_on_unsettle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->unsettle_action ),
			_( "Unsettle the selection" ));
	button = my_utils_container_get_child_by_name( parent, "unsettle-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), button, priv->settings_prefix, G_ACTION( priv->unsettle_action ));

	/* view operation action */
	priv->vope_action = g_simple_action_new( "vope", NULL );
	g_signal_connect( priv->vope_action, "activate", G_CALLBACK( action_on_vope_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vope_action ),
			_( "View the operation..." ));

	/* view conciliation action */
	priv->vconcil_action = g_simple_action_new( "vconcil", NULL );
	g_signal_connect( priv->vconcil_action, "activate", G_CALLBACK( action_on_vconcil_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vconcil_action ),
			_( "View the conciliation group..." ));

	/* view settlement action */
	priv->vsettle_action = g_simple_action_new( "vsettle", NULL );
	g_signal_connect( priv->vsettle_action, "activate", G_CALLBACK( action_on_vsettle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vsettle_action ),
			_( "View the settlement group..." ));
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_settlement_page_v_init_view";
	ofaSettlementPagePrivate *priv;
	GMenu *menu;
	gulong handler;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_settlement_page_get_instance_private( OFA_SETTLEMENT_PAGE( page ));

	/* setup contextual menu */
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* install the entry store before setting up the initial values */
	priv->store = ofa_entry_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	handler = g_signal_connect( priv->store, "ofa-changed", G_CALLBACK( store_on_changed ), page );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );

	/* as GTK_SELECTION_MULTIPLE is set, we have to explicitely
	 * setup the initial selection if a first row exists */
	ofa_tvbin_select_first_row( OFA_TVBIN( priv->tview ));

	/* setup initial values */
	read_settings( OFA_SETTLEMENT_PAGE( page ));
}

static void
on_account_changed( GtkEntry *entry, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	ofoAccount *account;
	const gchar *cnumber, *clabel, *cur_code, *ccolor;
	gchar *msgerr, *str;

	priv = ofa_settlement_page_get_instance_private( self );

	msgerr = NULL;
	clabel = "";
	ccolor = COLOR_ERROR;

	g_free( priv->account_number );
	priv->account_number = NULL;
	priv->account_currency = NULL;

	cnumber = g_strdup( gtk_entry_get_text( entry ));

	if( my_strlen( cnumber )){
		account = ofo_account_get_by_number( priv->getter, cnumber );

		if( account && OFO_IS_ACCOUNT( account )){
			clabel = ofo_account_get_label( account );
			ccolor = COLOR_WARNING;

			if( ofo_account_is_root( account )){
				msgerr = g_strdup_printf( _( "Account number '%s' is not a detail account" ), cnumber );

			} else if( ofo_account_is_closed( account )){
				msgerr = g_strdup_printf( _( "Account number '%s' is closed" ), cnumber );

			} else if( ofo_account_is_settleable( account )){
				priv->account_number = g_strdup( cnumber );
				cur_code = ofo_account_get_currency( account );
				g_return_if_fail( cur_code && my_strlen( cur_code ));
				priv->account_currency = ofo_currency_get_by_code( priv->getter, cur_code );
				g_return_if_fail( priv->account_currency && OFO_IS_CURRENCY( priv->account_currency ));

			} else {
				msgerr = g_strdup_printf( _( "Account number '%s' is not settleable" ), cnumber );
			}
		} else {
			msgerr = g_strdup_printf( _( "Account number '%s' is unknown or invalid" ), cnumber );
		}
	} else {
		msgerr = g_strdup( _( "Account number is not set" ));
	}

	if( my_strlen( msgerr )){
		str = g_strdup_printf( "<span style=\"italic\" color=\"%s\">%s</span>", ccolor, clabel );
	} else {
		str = g_strdup_printf( "<span color=\"%s\">%s</span>", COLOR_INFO, clabel );
	}
	gtk_label_set_markup( GTK_LABEL( priv->account_label ), str );
	g_free( str );

	set_message( self, msgerr, ccolor );
	g_free( msgerr );

	refresh_display( self );
}

static void
on_settlement_changed( GtkComboBox *box, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	priv = ofa_settlement_page_get_instance_private( self );

	if( gtk_combo_box_get_active_iter( box, &iter )){

		g_free( priv->filter_id );
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				SET_COL_CODE, &priv->filter_id,
				-1 );

		refresh_display( self );
	}
}

/*
 * View entry
 */
static void
action_on_edit_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GList *selected;
	ofoEntry *entry;

	priv = ofa_settlement_page_get_instance_private( self );

	selected = ofa_entry_treeview_get_selected( priv->tview );
	entry = ( ofoEntry * ) selected->data;
	do_edit( self, entry );
	ofa_entry_treeview_free_selected( selected );
}

static void
do_edit( ofaSettlementPage *self, ofoEntry *entry )
{
	ofaSettlementPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_settlement_page_get_instance_private( self );

	if( entry ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_entry_properties_run( priv->getter, toplevel, entry, FALSE );
	}
}

static gboolean
settle_on_pressed( GtkWidget *button, GdkEvent *event, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GdkModifierType modifiers;

	priv = ofa_settlement_page_get_instance_private( self );

	modifiers = gtk_accelerator_get_default_mod_mask();

	priv->ctrl_on_pressed = ((( GdkEventButton * ) event )->state & modifiers ) == GDK_CONTROL_MASK;

	return( FALSE );
}

static gboolean
settle_on_released( GtkWidget *button, GdkEvent *event, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GdkModifierType modifiers;
	gboolean ctrl_on_released;

	priv = ofa_settlement_page_get_instance_private( self );

	modifiers = gtk_accelerator_get_default_mod_mask();

	ctrl_on_released = ((( GdkEventButton * ) event )->state & modifiers ) == GDK_CONTROL_MASK;
	priv->ctrl_pressed = priv->ctrl_on_pressed && ctrl_on_released;

	return( FALSE );
}

static void
action_on_settle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;

	priv = ofa_settlement_page_get_instance_private( self );

	/* ask for a user confirmation when selection is not balanced
	 *  (and Ctrl key is not pressed) */
	if( !ofs_currency_is_balanced( &priv->ses.scur ) &&
			( ofa_prefs_account_settle_warns_if_unbalanced( priv->getter ) &&
			( !ofa_prefs_account_settle_warns_unless_ctrl( priv->getter ) || !priv->ctrl_pressed ))){
		if( !do_settle_user_confirm( self )){
			return;
		}
	}

	update_selection( self, TRUE );

	priv->ctrl_on_pressed = FALSE;
	priv->ctrl_pressed = FALSE;
}

static gboolean
do_settle_user_confirm( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	gboolean ok;
	gchar *sdeb, *scre, *str;
	GtkWindow *toplevel;

	priv = ofa_settlement_page_get_instance_private( self );

	sdeb = ofa_amount_to_str( priv->ses.scur.debit, priv->account_currency, priv->getter );
	scre = ofa_amount_to_str( priv->ses.scur.credit, priv->account_currency, priv->getter );
	str = g_strdup_printf( _( "Caution: settleable amounts are not balanced:\n"
			"debit=%s, credit=%s.\n"
			"Are you sure you want to settle this group ?" ), sdeb, scre );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "_Settle" ));

	g_free( sdeb );
	g_free( scre );
	g_free( str );

	return( ok );
}

static void
action_on_unsettle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	update_selection( self, FALSE );
}

/*
 * we update here the rows to settled/unsettled
 * due to the GtkTreeModelFilter, this may lead the updated row to
 * disappear from the view -> so update is based on store iters
 */
static void
update_selection( ofaSettlementPage *self, gboolean settle )
{
	ofaSettlementPagePrivate *priv;
	GList *selected;

	priv = ofa_settlement_page_get_instance_private( self );

	priv->updating = TRUE;

	if( settle ){
		priv->snumber = ofo_counters_get_next_settlement_id( priv->getter );

	} else {
		priv->snumber = -1;
	}

	selected = ofa_entry_treeview_get_selected( priv->tview );
	g_list_foreach( selected, ( GFunc ) update_row_enum, self );
	ofa_entry_treeview_free_selected( selected );

	g_simple_action_set_enabled( priv->settle_action, priv->ses.unsettled > 0 );
	g_simple_action_set_enabled( priv->unsettle_action, priv->ses.settled > 0 );

	refresh_display( self );
}

/*
 * #ofo_entry_update_settlement() triggers a hub signal: the store will
 * so auto-update itself.
 */
static void
update_row_enum( ofoEntry *entry, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;

	priv = ofa_settlement_page_get_instance_private( self );

	ofo_entry_update_settlement( entry, priv->snumber );
}

/**
 * ofa_settlement_page_set_account:
 * @page: this #ofaSettlementPage instance.
 * @number: the account identifier.
 *
 * Setup an account identifier on the page.
 */
void
ofa_settlement_page_set_account( ofaSettlementPage *page, const gchar *number )
{
	ofaSettlementPagePrivate *priv;

	g_return_if_fail( page && OFA_IS_SETTLEMENT_PAGE( page ));
	g_return_if_fail( !OFA_PAGE( page )->prot->dispose_has_run );

	priv = ofa_settlement_page_get_instance_private( page );

	gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), number );
}

/*
 * Recompute the whole selection struct each time the selection change
 */
static void
refresh_selection_compute( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GList *selected;

	priv = ofa_settlement_page_get_instance_private( self );

	selected = ofa_entry_treeview_get_selected( priv->tview );
	refresh_selection_compute_with_selected( self, selected );
	ofa_entry_treeview_free_selected( selected );
}

static void
refresh_selection_compute_with_selected( ofaSettlementPage *self, GList *selected )
{
	ofaSettlementPagePrivate *priv;
	gchar *samount;

	priv = ofa_settlement_page_get_instance_private( self );

	if( !priv->updating ){
		memset( &priv->ses, '\0', sizeof( sEnumSelected ));
		priv->snumber = -1;

		if( priv->account_currency ){
			priv->ses.scur.currency = priv->account_currency;
			g_list_foreach( selected, ( GFunc ) tview_enum_selected, self );
		}

		g_simple_action_set_enabled( priv->settle_action, priv->ses.unsettled > 0 );
		g_simple_action_set_enabled( priv->unsettle_action, priv->ses.settled > 0 );

		samount = priv->account_currency ?
						ofa_amount_to_str( priv->ses.scur.debit, priv->account_currency, priv->getter ) :
						g_strdup( "" );
		gtk_label_set_text( GTK_LABEL( priv->debit_balance ), samount );
		g_free( samount );

		samount = priv->account_currency ?
						ofa_amount_to_str( priv->ses.scur.credit, priv->account_currency, priv->getter ) :
						g_strdup( "" );
		gtk_label_set_text( GTK_LABEL( priv->credit_balance ), samount );
		g_free( samount );

		if( priv->ses.rows > 0 ){
			if( ofs_currency_is_balanced( &priv->ses.scur )){
				gtk_image_set_from_resource( GTK_IMAGE( priv->light_balance ), st_resource_light_green );
			} else {
				gtk_image_set_from_resource( GTK_IMAGE( priv->light_balance ), st_resource_light_yellow );
			}
		} else {
			gtk_image_set_from_resource( GTK_IMAGE( priv->light_balance ), st_resource_light_empty );
		}
	}
}

static void
refresh_display( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;

	priv = ofa_settlement_page_get_instance_private( self );

	ofa_tvbin_refilter( OFA_TVBIN( priv->tview ));
	refresh_selection_compute( self );
}

static void
action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;

	priv = ofa_settlement_page_get_instance_private( self );

	ofa_operation_group_run( priv->getter, NULL, priv->sel_opes );
}

static void
action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_settlement_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_reconcil_group_run( priv->getter, toplevel, priv->sel_concil_id );
}

static void
action_on_vsettle_activated( GSimpleAction *action, GVariant *empty, ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_settlement_page_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_settlement_group_run( priv->getter, toplevel, priv->sel_settle_id );
}

static void
set_message( ofaSettlementPage *self, const gchar *msg, const gchar *color )
{
	ofaSettlementPagePrivate *priv;
	gchar *str;

	priv = ofa_settlement_page_get_instance_private( self );

	if( my_strlen( msg )){
		if( my_strlen( color )){
			str = g_strdup_printf( "<span style=\"italic\" color=\"%s\">%s</span>", color, msg );
		} else {
			str = g_strdup( msg );
		}
	} else {
		str = g_strdup( "" );
	}

	gtk_label_set_markup( GTK_LABEL( priv->footer_msg ), str );
	g_free( str );
}

/*
 * settings: mode;account;paned_position;footer_paned_position;
 *
 * Order is not unimportant: account should be set after the filtering
 * mode; it is so easier to read it in second position.
 *
 * Prevent writing settings when just initializing the data.
 */
static void
read_settings( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;
	guint pos;

	priv = ofa_settlement_page_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	/* filter mode */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->filter_combo ), cstr );
	}

	/* account number */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}

	/* paned position */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	pos = my_strlen( cstr ) ? atoi( cstr ) : 0;
	if( pos < 150 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->paned ), pos );

	/* footer paned position */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	pos = my_strlen( cstr ) ? atoi( cstr ) : 0;
	if( pos < 150 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->footer_paned ), pos );

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaSettlementPage *self )
{
	ofaSettlementPagePrivate *priv;
	myISettings *settings;
	gchar *str, *key;

	priv = ofa_settlement_page_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;%d;%d;",
			priv->filter_id,
			priv->account_number,
			gtk_paned_get_position( GTK_PANED( priv->paned )),
			gtk_paned_get_position( GTK_PANED( priv->footer_paned )));

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

/*
 * ofaEntryStore::ofa-changed signal handler
 *
 * This signal is sent by ofaEntryStore after it has treated an
 * ofaISignaler event.
 */
static void
store_on_changed( ofaEntryStore *store, ofaSettlementPage *self )
{
	refresh_display( self );
}
