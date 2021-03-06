/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "my/my-iprogress.h"
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idate-filter.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-counters.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-ope.h"

#include "ofa-recurrent-run-page.h"
#include "ofa-recurrent-run-store.h"
#include "ofa-recurrent-run-treeview.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* priv instance data
 */
typedef struct {

	/* runtime
	 */
	ofaIGetter              *getter;
	gchar                   *settings_prefix;

	/* UI
	 */
	GtkWidget               *paned;
	ofaRecurrentRunTreeview *tview;

	GtkWidget               *cancelled_toggle;
	GtkWidget               *waiting_toggle;
	GtkWidget               *validated_toggle;

	ofaDateFilterHVBin      *date_filter;

	GSimpleAction           *cancel_action;
	GSimpleAction           *waiting_action;
	GSimpleAction           *accounting_action;

	/* update status input
	 */
	guint                    update_ope_count;
	guint                    update_entry_count;
	GSourceFunc              update_cb;
	ofeRecurrentStatus       update_old_status;
	ofeRecurrentStatus       update_new_status;
	gboolean                 update_with_progress;
	void                    *update_worker;
	const gchar             *update_title;

	/* update status run
	 */
	GtkWidget               *update_dialog;
	myProgressBar           *update_bar;
	GtkWidget               *update_close_btn;
	ofoRecurrentRun         *update_recrun;
}
	ofaRecurrentRunPagePrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-run-page.ui";

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static void       paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static GtkWidget *setup_view1( ofaRecurrentRunPage *self );
static GtkWidget *setup_view2( ofaRecurrentRunPage *self );
static void       setup_status_filter( ofaRecurrentRunPage *self, GtkContainer *parent );
static void       setup_date_filter( ofaRecurrentRunPage *self, GtkContainer *parent );
static void       setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent );
static void       paned_page_v_init_view( ofaPanedPage *page );
static void       filter_on_cancelled_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void       filter_on_waiting_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void       filter_on_validated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void       on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaRecurrentRunPage *self );
static void       on_row_selected( ofaRecurrentRunTreeview *view, GList *list, ofaRecurrentRunPage *self );
static void       tview_examine_selected( ofaRecurrentRunPage *self, GList *selected, guint *cancelled, guint *waiting, guint *validated );
static void       action_on_cancel_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self );
static void       action_on_wait_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self );
static void       action_on_accounting_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self );
static gboolean   action_user_confirm( ofaRecurrentRunPage *self );
static gboolean   action_update_status( ofaRecurrentRunPage *self );
static gboolean   actualize_selection( ofaRecurrentRunPage *self );
static gboolean   action_on_object_validated( ofaRecurrentRunPage *self );
static void       read_settings( ofaRecurrentRunPage *self );
static void       write_settings( ofaRecurrentRunPage *self );
static void       iprogress_iface_init( myIProgressInterface *iface );
static void       iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget );
static void       iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total );
static void       iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunPage, ofa_recurrent_run_page, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentRunPage )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IPROGRESS, iprogress_iface_init ))

static void
recurrent_run_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_finalize";
	ofaRecurrentRunPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_PAGE( instance ));

	/* free data members here */
	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_page_parent_class )->finalize( instance );
}

static void
recurrent_run_page_dispose( GObject *instance )
{
	ofaRecurrentRunPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		write_settings( OFA_RECURRENT_RUN_PAGE( instance ));

		/* unref object members here */
		priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( instance ));

		g_object_unref( priv->cancel_action );
		g_object_unref( priv->waiting_action );
		g_object_unref( priv->accounting_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_run_page_init( ofaRecurrentRunPage *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_init";
	ofaRecurrentRunPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_RUN_PAGE( self ));

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_recurrent_run_page_class_init( ofaRecurrentRunPageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = paned_page_v_setup_view;
	OFA_PANED_PAGE_CLASS( klass )->init_view = paned_page_v_init_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentRunPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_RUN_PAGE( page ), NULL );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	return( ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));
}

static void
paned_page_v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_v_setup_view";
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *view;
	ofaHub *hub;

	g_debug( "%s: page=%p, paned=%p", thisfn, ( void * ) page, ( void * ) paned );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	priv->getter = ofa_page_get_getter( OFA_PAGE( page ));

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv->paned = GTK_WIDGET( paned );

	view = setup_view1( OFA_RECURRENT_RUN_PAGE( page ));
	gtk_paned_pack1( paned, view, TRUE, FALSE );

	view = setup_view2( OFA_RECURRENT_RUN_PAGE( page ));
	gtk_paned_pack2( paned, view, FALSE, FALSE );
}

static GtkWidget *
setup_view1( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( self ));

	priv->tview = ofa_recurrent_run_treeview_new( priv->getter, priv->settings_prefix );

	/* ofaRecurrentRunTreeview signals */
	g_signal_connect( priv->tview, "ofa-recchanged", G_CALLBACK( on_row_selected ), self );

	return( GTK_WIDGET( priv->tview ));
}

static GtkWidget *
setup_view2( ofaRecurrentRunPage *self )
{
	GtkWidget *parent;

	parent = gtk_grid_new();
	my_utils_container_attach_from_resource( GTK_CONTAINER( parent ), st_resource_ui, "RecurrentRunPageWindow", "top" );

	setup_status_filter( OFA_RECURRENT_RUN_PAGE( self ), GTK_CONTAINER( parent ));
	setup_date_filter( OFA_RECURRENT_RUN_PAGE( self ), GTK_CONTAINER( parent ));
	setup_actions( OFA_RECURRENT_RUN_PAGE( self ), GTK_CONTAINER( parent ));

	return( parent );
}

/*
 * initialize the filter area
 */
static void
setup_status_filter( ofaRecurrentRunPage *self, GtkContainer *parent )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *btn;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( parent, "p3-cancelled-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->cancelled_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_cancelled_btn_toggled ), self );

	btn = my_utils_container_get_child_by_name( parent, "p3-waiting-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->waiting_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_waiting_btn_toggled ), self );

	btn = my_utils_container_get_child_by_name( parent, "p3-validated-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->validated_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_validated_btn_toggled ), self );
}

static void
setup_date_filter( ofaRecurrentRunPage *self, GtkContainer *container )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( container, "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Operation date selection " ));

	g_signal_connect( G_OBJECT( filter ), "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );

	priv->date_filter = filter;
}

/*
 * initialize the actions area
 */
static void
setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *btn;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->cancel_action = g_simple_action_new( "cancel", NULL );
	g_signal_connect( priv->cancel_action, "activate", G_CALLBACK( action_on_cancel_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->cancel_action ),
			_( "Cancel" ));
	btn = my_utils_container_get_child_by_name( parent, "p2-cancel-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->cancel_action ));
	g_simple_action_set_enabled( priv->cancel_action, FALSE );

	priv->waiting_action = g_simple_action_new( "waiting", NULL );
	g_signal_connect( priv->waiting_action, "activate", G_CALLBACK( action_on_wait_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->waiting_action ),
			_( "Reset to wait" ));
	btn = my_utils_container_get_child_by_name( parent, "p2-wait-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->waiting_action ));
	g_simple_action_set_enabled( priv->waiting_action, FALSE );

	priv->accounting_action = g_simple_action_new( "accounting", NULL );
	g_signal_connect( priv->accounting_action, "activate", G_CALLBACK( action_on_accounting_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->accounting_action ),
			_( "Send to accounting..." ));
	btn = my_utils_container_get_child_by_name( parent, "p2-account-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->accounting_action ));
	g_simple_action_set_enabled( priv->accounting_action, FALSE );
}

static void
paned_page_v_init_view( ofaPanedPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_v_init_view";
	ofaRecurrentRunPagePrivate *priv;
	GMenu *menu;
	ofaRecurrentRunStore *store;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( page ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( page ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* install the store at the very end of the initialization
	 * (i.e. after treeview creation, signals connection, actions and
	 *  menus definition) */
	store = ofa_recurrent_run_store_new( priv->getter, REC_MODE_FROM_DBMS );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	/* setup initial values */
	read_settings( OFA_RECURRENT_RUN_PAGE( page ));

	/* as GTK_SELECTION_MULTIPLE is set, we have to explicitely
	 * setup the initial selection if a first row exists */
	ofa_tvbin_select_all( OFA_TVBIN( priv->tview ));
}

static void
filter_on_cancelled_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gint visible;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	visible = ofa_recurrent_run_treeview_get_visible( priv->tview );
	if( gtk_toggle_button_get_active( button )){
		visible |= REC_VISIBLE_CANCELLED;
	} else {
		visible &= ~REC_VISIBLE_CANCELLED;
	}
	ofa_recurrent_run_treeview_set_visible( priv->tview, visible );
}

static void
filter_on_waiting_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gint visible;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	visible = ofa_recurrent_run_treeview_get_visible( priv->tview );
	if( gtk_toggle_button_get_active( button )){
		visible |= REC_VISIBLE_WAITING;
	} else {
		visible &= ~REC_VISIBLE_WAITING;
	}
	ofa_recurrent_run_treeview_set_visible( priv->tview, visible );
}

static void
filter_on_validated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gint visible;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	visible = ofa_recurrent_run_treeview_get_visible( priv->tview );
	if( gtk_toggle_button_get_active( button )){
		visible |= REC_VISIBLE_VALIDATED;
	} else {
		visible &= ~REC_VISIBLE_VALIDATED;
	}
	ofa_recurrent_run_treeview_set_visible( priv->tview, visible );
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	const GDate *from, *to;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	from = ofa_idate_filter_get_date( OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM );
	to = ofa_idate_filter_get_date( OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO );

	ofa_recurrent_run_treeview_set_ope_date( priv->tview, from, to );
}

/*
 * RecurrentRunTreeview callback
 */
static void
on_row_selected( ofaRecurrentRunTreeview *view, GList *list, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	guint cancelled, waiting, validated;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	tview_examine_selected( self, list, &cancelled, &waiting, &validated );

	g_simple_action_set_enabled( priv->cancel_action, waiting > 0 );
	g_simple_action_set_enabled( priv->waiting_action, cancelled > 0 );
	g_simple_action_set_enabled( priv->accounting_action, waiting > 0 );
}

static void
tview_examine_selected( ofaRecurrentRunPage *self, GList *selected, guint *cancelled, guint *waiting, guint *validated )
{
	GList *it;
	ofoRecurrentRun *obj;
	ofeRecurrentStatus status;

	*cancelled = 0;
	*waiting = 0;
	*validated = 0;

	for( it=selected ; it ; it=it->next ){
		obj = OFO_RECURRENT_RUN( it->data );
		status = ofo_recurrent_run_get_status( obj );

		switch( status ){
			case REC_STATUS_CANCELLED:
				*cancelled += 1;
				break;
			case REC_STATUS_WAITING:
				*waiting += 1;
				break;
			case REC_STATUS_VALIDATED:
				*validated += 1;
				break;
			default:
				break;
		}
	}
}

/*
 * cancel waiting operations
 */
static void
action_on_cancel_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->update_ope_count = 0;
	priv->update_entry_count = 0;
	priv->update_old_status = REC_STATUS_WAITING;
	priv->update_new_status = REC_STATUS_CANCELLED;
	priv->update_cb = NULL;
	priv->update_with_progress = FALSE;
	priv->update_title = NULL;

	g_idle_add(( GSourceFunc ) action_update_status, self );
}

/*
 * uncancel operations, making them waiting back
 */
static void
action_on_wait_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->update_ope_count = 0;
	priv->update_entry_count = 0;
	priv->update_old_status = REC_STATUS_CANCELLED;
	priv->update_new_status = REC_STATUS_WAITING;
	priv->update_cb = NULL;
	priv->update_with_progress = FALSE;
	priv->update_title = NULL;

	g_idle_add(( GSourceFunc ) action_update_status, self );
}

static void
action_on_accounting_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	if( action_user_confirm( self )){
		priv->update_ope_count = 0;
		priv->update_entry_count = 0;
		priv->update_old_status = REC_STATUS_WAITING;
		priv->update_new_status = REC_STATUS_VALIDATED;
		priv->update_cb = ( GSourceFunc ) action_on_object_validated;
		priv->update_with_progress = TRUE;
		priv->update_title = _( " Recording operations " );
		priv->update_worker = GUINT_TO_POINTER( priv->update_new_status );
		my_iprogress_start_work( MY_IPROGRESS( self ), priv->update_worker, NULL );

		g_idle_add(( GSourceFunc ) action_update_status, self );
	}
}

/*
 * a user confirmation before validating operations
 */
static gboolean
action_user_confirm( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *selected;
	gint count;
	gchar *msg;
	gboolean ok;
	GtkWindow *toplevel;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	selected = ofa_recurrent_run_treeview_get_selected( priv->tview );
	count = g_list_length( selected );
	ofa_recurrent_run_treeview_free_selected( selected );

	msg = g_strdup_printf( _( "About to send to accounting %d waiting operation(s).\n"
								"Are you sure ?" ),
			count );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, msg, _( "Send to acc_ounting" ));

	g_free( msg );

	return( ok );
}

/*
 * if this a validation and concerns more than one waiting operation,
 * then display a progress bar
 */
static gboolean
action_update_status( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *selected, *it;
	ofeRecurrentStatus cur_status;
	ofoRecurrentRun *run_obj;
	gint count, i;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	selected = ofa_recurrent_run_treeview_get_selected( priv->tview );
	count = g_list_length( selected );
	i = 0;

	for( it=selected ; it ; it=it->next ){
		run_obj = OFO_RECURRENT_RUN( it->data );
		g_return_val_if_fail( run_obj && OFO_IS_RECURRENT_RUN( run_obj ), G_SOURCE_REMOVE );

		cur_status = ofo_recurrent_run_get_status( run_obj );
		if( cur_status == priv->update_old_status ){
			ofo_recurrent_run_set_status( run_obj, priv->update_new_status );
			if( ofo_recurrent_run_update_status( run_obj ) && priv->update_cb ){
				priv->update_recrun = run_obj;
				//g_idle_add(( GSourceFunc ) priv->update_cb, self );
				( *priv->update_cb )( self );
			}
			if( priv->update_with_progress ){
				my_iprogress_pulse( MY_IPROGRESS( self ), priv->update_worker, ++i, count );
			}
		}
	}

	ofa_recurrent_run_treeview_free_selected( selected );
	if( priv->update_with_progress ){
		my_iprogress_set_ok( MY_IPROGRESS( self ), priv->update_worker, NULL, 0 );
	}

	/* actualize the actions state */
	g_idle_add(( GSourceFunc ) actualize_selection, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * After having updated all status, try to renew the state of the actions
 */
static gboolean
actualize_selection( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *selected;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	selected = ofa_recurrent_run_treeview_get_selected( priv->tview );
	on_row_selected( priv->tview, selected, self );
	ofa_recurrent_run_treeview_free_selected( selected );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * Not a GSourceFunc, but just a function sync-called after each validation
 */
static gboolean
action_on_object_validated( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	const gchar *rec_id, *tmpl_id, *ledger_id;
	ofaIDBConnect *connect;
	gboolean ok;
	ofoRecurrentModel *model;
	ofoOpeTemplate *template_obj;
	ofoLedger *ledger_obj;
	ofsOpe *ope;
	GList *entries, *it;
	GDate dmin;
	ofoEntry *entry;
	ofxCounter ope_number;
	const gchar *csdef;
	ofxAmount amount;
	ofaHub *hub;
	ofoDossier *dossier;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), G_SOURCE_REMOVE );

	connect = ofa_hub_get_connect( hub );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), G_SOURCE_REMOVE );

	rec_id = ofo_recurrent_run_get_mnemo( priv->update_recrun );
	model = ofo_recurrent_model_get_by_mnemo( priv->getter, rec_id );
	g_return_val_if_fail( model && OFO_IS_RECURRENT_MODEL( model ), G_SOURCE_REMOVE );

	tmpl_id = ofo_recurrent_model_get_ope_template( model );
	template_obj = ofo_ope_template_get_by_mnemo( priv->getter, tmpl_id );
	g_return_val_if_fail( template_obj && OFO_IS_OPE_TEMPLATE( template_obj ), G_SOURCE_REMOVE );

	ledger_id = ofo_ope_template_get_ledger( template_obj );
	ledger_obj = ofo_ledger_get_by_mnemo( priv->getter, ledger_id );
	g_return_val_if_fail( ledger_obj && OFO_IS_LEDGER( ledger_obj ), G_SOURCE_REMOVE );

	ope = ofs_ope_new( template_obj );
	my_date_set_from_date( &ope->dope, ofo_recurrent_run_get_date( priv->update_recrun ));
	ope->dope_user_set = TRUE;
	ofo_dossier_get_min_deffect( dossier, ledger_obj, &dmin );
	my_date_set_from_date( &ope->deffect, my_date_compare( &ope->dope, &dmin ) >= 0 ? &ope->dope : &dmin );

	csdef = ofo_recurrent_model_get_def_amount1( model );
	if( my_strlen( csdef )){
		amount = ofo_recurrent_run_get_amount1( priv->update_recrun );
		ofs_ope_set_amount( ope, csdef, amount );
	}

	csdef = ofo_recurrent_model_get_def_amount2( model );
	if( my_strlen( csdef )){
		amount = ofo_recurrent_run_get_amount2( priv->update_recrun );
		ofs_ope_set_amount( ope, csdef, amount );
	}

	csdef = ofo_recurrent_model_get_def_amount3( model );
	if( my_strlen( csdef )){
		amount = ofo_recurrent_run_get_amount3( priv->update_recrun );
		ofs_ope_set_amount( ope, csdef, amount );
	}

	ofs_ope_apply_template( ope );
	entries = ofs_ope_generate_entries( ope );

	ok = ofa_idbconnect_transaction_start( connect, FALSE, NULL );

	if( ok ){
		ope_number = ofo_counters_get_next_ope_id( priv->getter );
		priv->update_ope_count += 1;

		for( it=entries ; it && ok ; it=it->next ){
			entry = OFO_ENTRY( it->data );
			ofo_entry_set_ope_number( entry, ope_number );
			ok = ofo_entry_insert( entry );
			priv->update_entry_count += 1;
		}
	}

	if( ok ){
		ofa_idbconnect_transaction_commit( connect, FALSE, NULL );
		g_list_free( entries );

	} else {
		ok = ofa_idbconnect_transaction_cancel( connect, FALSE, NULL );
		g_list_free_full( entries, g_object_unref );
	}

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * settings: paned_position; cancelled_visible; waiting_visible; validated_visible; from_date; to_date;
 */
static void
read_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gint pos;
	gchar *key;
	GDate date;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	/* paned position */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	pos = my_strlen( cstr ) ? atoi( cstr ) : 0;
	if( pos < 150 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->paned ), pos );

	/* cancelled visible */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->cancelled_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_cancelled_btn_toggled( GTK_TOGGLE_BUTTON( priv->cancelled_toggle ), self );
	}

	/* waiting visible */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->waiting_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_waiting_btn_toggled( GTK_TOGGLE_BUTTON( priv->waiting_toggle ), self );
	}

	/* validated visible */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->validated_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_validated_btn_toggled( GTK_TOGGLE_BUTTON( priv->validated_toggle ), self );
	}

	/* from date */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date( OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, &date );
	}

	/* to date */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date( OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, &date );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	myISettings *settings;
	gchar *str, *key, *sdfrom, *sdto;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%d;%s;%s;%s;%s;%s;",
			gtk_paned_get_position( GTK_PANED( priv->paned )),
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->cancelled_toggle )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->waiting_toggle )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->validated_toggle )) ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "" );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

/*
 * myIProgress interface management
 */
static void
iprogress_iface_init( myIProgressInterface *iface )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_iprogress_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->start_work = iprogress_start_work;
	iface->pulse = iprogress_pulse;
	iface->set_ok = iprogress_set_ok;
}

/*
 * @widget: ignored
 */
static void
iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *content;

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( instance ));

	priv->update_dialog = gtk_dialog_new_with_buttons(
			priv->update_title,
			my_utils_widget_get_toplevel( GTK_WIDGET( instance )),
			0,
			_( "_Close" ), GTK_RESPONSE_CLOSE,
			NULL );

	gtk_container_set_border_width( GTK_CONTAINER( priv->update_dialog ), 4 );

	content = gtk_dialog_get_content_area( GTK_DIALOG( priv->update_dialog ));
	priv->update_bar = my_progress_bar_new();
	my_utils_widget_set_margins( GTK_WIDGET( priv->update_bar ), 8, 8, 12, 12 );
	gtk_container_add( GTK_CONTAINER( content ), GTK_WIDGET( priv->update_bar ));

	g_signal_connect_swapped( priv->update_dialog, "response", G_CALLBACK( gtk_widget_destroy ), priv->update_dialog );

	priv->update_close_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( priv->update_dialog ), GTK_RESPONSE_CLOSE );
	g_return_if_fail( priv->update_close_btn && GTK_IS_BUTTON( priv->update_close_btn ));
	gtk_widget_set_sensitive( priv->update_close_btn, FALSE );

	gtk_widget_show_all( priv->update_dialog );
}

static void
iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total )
{
	ofaRecurrentRunPagePrivate *priv;
	gdouble progress;
	gchar *str;

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( instance ));

	progress = total ? ( gdouble ) count / ( gdouble ) total : 0;
	g_signal_emit_by_name( priv->update_bar, "my-double", progress );

	str = g_strdup_printf( "%lu/%lu", count, total );
	g_signal_emit_by_name( priv->update_bar, "my-text", str );
	g_free( str );
}

static void
iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( instance ));

	gtk_widget_set_sensitive( priv->update_close_btn, TRUE );
}
