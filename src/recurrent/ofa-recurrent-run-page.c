/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-periodicity.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
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
	ofaHub                  *hub;
	gchar                   *settings_prefix;

	/* UI
	 */
	GtkWidget               *paned;
	ofaRecurrentRunTreeview *tview;

	GtkWidget               *cancelled_toggle;
	GtkWidget               *waiting_toggle;
	GtkWidget               *validated_toggle;

	GSimpleAction           *cancel_action;
	GSimpleAction           *waiting_action;
	GSimpleAction           *validate_action;
}
	ofaRecurrentRunPagePrivate;

/* counters used when validating a waiting operation
 */
typedef struct {
	guint ope_count;
	guint entry_count;
}
	sCount;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-run-page.ui";

typedef void ( *RecurrentValidCb )( ofaRecurrentRunPage *self, ofoRecurrentRun *obj, guint *count );

static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       v_setup_view( ofaPanedPage *page, GtkPaned *paned );
static GtkWidget *setup_view1( ofaRecurrentRunPage *self );
static GtkWidget *setup_view2( ofaRecurrentRunPage *self );
static void       setup_filters( ofaRecurrentRunPage *self, GtkContainer *parent );
static void       setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent );
static void       v_init_view( ofaPanedPage *page );
static void       filter_on_cancelled_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void       filter_on_waiting_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void       filter_on_validated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void       on_row_selected( ofaRecurrentRunTreeview *view, GList *list, ofaRecurrentRunPage *self );
static void       tview_examine_selected( ofaRecurrentRunPage *self, GList *selected, guint *cancelled, guint *waiting, guint *validated );
static void       action_on_cancel_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self );
static void       action_on_wait_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self );
static void       action_on_validate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self );
static void       action_update_status( ofaRecurrentRunPage *self, const gchar *allowed_status, const gchar *new_status, RecurrentValidCb cb, void *user_data );
static void       action_on_object_validated( ofaRecurrentRunPage *self, ofoRecurrentRun *obj, sCount *counts );
static void       get_settings( ofaRecurrentRunPage *self );
static void       set_settings( ofaRecurrentRunPage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunPage, ofa_recurrent_run_page, OFA_TYPE_PANED_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentRunPage ))

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

		set_settings( OFA_RECURRENT_RUN_PAGE( instance ));

		/* unref object members here */
		priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( instance ));

		g_object_unref( priv->cancel_action );
		g_object_unref( priv->waiting_action );
		g_object_unref( priv->validate_action );
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

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	OFA_PANED_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PANED_PAGE_CLASS( klass )->init_view = v_init_view;
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentRunPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_RUN_PAGE( page ), NULL );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	return( ofa_tvbin_get_treeview( OFA_TVBIN( priv->tview )));
}

static void
v_setup_view( ofaPanedPage *page, GtkPaned *paned )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_v_setup_view";
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *view;

	g_debug( "%s: page=%p, paned=%p", thisfn, ( void * ) page, ( void * ) paned );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	priv->hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->paned = GTK_WIDGET( paned );

	view = setup_view1( OFA_RECURRENT_RUN_PAGE( page ));
	gtk_paned_pack1( paned, view, TRUE, FALSE );

	view = setup_view2( OFA_RECURRENT_RUN_PAGE( page ));
	gtk_paned_pack2( paned, view, FALSE, FALSE );

	get_settings( OFA_RECURRENT_RUN_PAGE( page ));
}

static GtkWidget *
setup_view1( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( self ));

	priv->tview = ofa_recurrent_run_treeview_new( priv->hub );
	ofa_recurrent_run_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_recurrent_run_treeview_setup_columns( priv->tview );

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

	setup_filters( OFA_RECURRENT_RUN_PAGE( self ), GTK_CONTAINER( parent ));
	setup_actions( OFA_RECURRENT_RUN_PAGE( self ), GTK_CONTAINER( parent ));

	return( parent );
}

/*
 * initialize the filter area
 */
static void
setup_filters( ofaRecurrentRunPage *self, GtkContainer *parent )
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
	g_simple_action_set_enabled( priv->cancel_action, FALSE );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->cancel_action ),
			_( "Cancel..." ));
	btn = my_utils_container_get_child_by_name( parent, "p2-cancel-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->cancel_action ));
	g_signal_connect( priv->cancel_action, "activate", G_CALLBACK( action_on_cancel_activated ), self );

	priv->waiting_action = g_simple_action_new( "waiting", NULL );
	g_simple_action_set_enabled( priv->waiting_action, FALSE );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->waiting_action ),
			_( "Wait..." ));
	btn = my_utils_container_get_child_by_name( parent, "p2-wait-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->waiting_action ));
	g_signal_connect( priv->waiting_action, "activate", G_CALLBACK( action_on_wait_activated ), self );

	priv->validate_action = g_simple_action_new( "validate", NULL );
	g_simple_action_set_enabled( priv->validate_action, FALSE );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->validate_action ),
			_( "Validate..." ));
	btn = my_utils_container_get_child_by_name( parent, "p2-validate-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->validate_action ));
	g_signal_connect( priv->validate_action, "activate", G_CALLBACK( action_on_validate_activated ), self );
}

static void
v_init_view( ofaPanedPage *page )
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
	store = ofa_recurrent_run_store_new( priv->hub, REC_MODE_FROM_DBMS );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	/* as GTK_SELECTION_MULTIPLE is set, we have to explicitely
	 * setup the initial selection if a first row exists */
	ofa_tvbin_select_first_row( OFA_TVBIN( priv->tview ));
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
	g_simple_action_set_enabled( priv->validate_action, waiting > 0 );
}

static void
tview_examine_selected( ofaRecurrentRunPage *self, GList *selected, guint *cancelled, guint *waiting, guint *validated )
{
	GList *it;
	ofoRecurrentRun *obj;
	const gchar *status;

	*cancelled = 0;
	*waiting = 0;
	*validated = 0;

	for( it=selected ; it ; it=it->next ){
		obj = OFO_RECURRENT_RUN( it->data );
		status = ofo_recurrent_run_get_status( obj );

		if( !my_collate( status, REC_STATUS_CANCELLED )){
			*cancelled += 1;
		} else if( !my_collate( status, REC_STATUS_WAITING )){
			*waiting += 1;
		} else if( !my_collate( status, REC_STATUS_VALIDATED )){
			*validated += 1;
		}
	}
}

static void
action_on_cancel_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self )
{
	action_update_status( self, REC_STATUS_WAITING, REC_STATUS_CANCELLED, NULL, NULL );
}

static void
action_on_wait_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self )
{
	action_update_status( self, REC_STATUS_CANCELLED, REC_STATUS_WAITING, NULL, NULL );
}

static void
action_on_validate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentRunPage *self )
{
	gchar *str;
	GtkWindow *toplevel;
	sCount counts;

	memset( &counts, '\0', sizeof( counts ));
	counts.ope_count = 0;
	counts.entry_count = 0;

	action_update_status( self, REC_STATUS_WAITING, REC_STATUS_VALIDATED, ( RecurrentValidCb ) action_on_object_validated, &counts );

	if( counts.ope_count == 0 ){
		str = g_strdup( _( "No created operation" ));
	} else {
		str = g_strdup_printf( _( "%u generated operations (%u inserted entries)" ), counts.ope_count, counts.entry_count );
	}

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	my_utils_msg_dialog( toplevel, GTK_MESSAGE_INFO, str );

	g_free( str );
}

static void
action_update_status( ofaRecurrentRunPage *self, const gchar *allowed_status, const gchar *new_status, RecurrentValidCb cb, void *user_data )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *selected, *it;
	const gchar *cur_status;
	ofoRecurrentRun *run_obj;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	selected = ofa_recurrent_run_treeview_get_selected( priv->tview );

	for( it=selected ; it ; it=it->next ){
		run_obj = OFO_RECURRENT_RUN( it->data );
		g_return_if_fail( run_obj && OFO_IS_RECURRENT_RUN( run_obj ));

		cur_status = ofo_recurrent_run_get_status( run_obj );
		if( !my_collate( cur_status, allowed_status )){
			ofo_recurrent_run_set_status( run_obj, new_status );
			if( ofo_recurrent_run_update( run_obj ) && cb ){
				( *cb )( self, run_obj, user_data );
			}
		}
	}

	ofa_recurrent_run_treeview_free_selected( selected );
}

static void
action_on_object_validated( ofaRecurrentRunPage *self, ofoRecurrentRun *recrun, sCount *counts )
{
	ofaRecurrentRunPagePrivate *priv;
	const gchar *rec_id, *tmpl_id, *ledger_id;
	ofoDossier *dossier;
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

	priv = ofa_recurrent_run_page_get_instance_private( self );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	rec_id = ofo_recurrent_run_get_mnemo( recrun );
	model = ofo_recurrent_model_get_by_mnemo( priv->hub, rec_id );
	g_return_if_fail( model && OFO_IS_RECURRENT_MODEL( model ));

	tmpl_id = ofo_recurrent_model_get_ope_template( model );
	template_obj = ofo_ope_template_get_by_mnemo( priv->hub, tmpl_id );
	g_return_if_fail( template_obj && OFO_IS_OPE_TEMPLATE( template_obj ));

	ledger_id = ofo_ope_template_get_ledger( template_obj );
	ledger_obj = ofo_ledger_get_by_mnemo( priv->hub, ledger_id );
	g_return_if_fail( ledger_obj && OFO_IS_LEDGER( ledger_obj ));

	ope = ofs_ope_new( template_obj );
	my_date_set_from_date( &ope->dope, ofo_recurrent_run_get_date( recrun ));
	ope->dope_user_set = TRUE;
	ofo_dossier_get_min_deffect( dossier, ledger_obj, &dmin );
	my_date_set_from_date( &ope->deffect, my_date_compare( &ope->dope, &dmin ) >= 0 ? &ope->dope : &dmin );

	csdef = ofo_recurrent_model_get_def_amount1( model );
	if( my_strlen( csdef )){
		amount = ofo_recurrent_run_get_amount1( recrun );
		ofs_ope_set_amount( ope, csdef, amount );
	}

	csdef = ofo_recurrent_model_get_def_amount2( model );
	if( my_strlen( csdef )){
		amount = ofo_recurrent_run_get_amount2( recrun );
		ofs_ope_set_amount( ope, csdef, amount );
	}

	csdef = ofo_recurrent_model_get_def_amount3( model );
	if( my_strlen( csdef )){
		amount = ofo_recurrent_run_get_amount3( recrun );
		ofs_ope_set_amount( ope, csdef, amount );
	}

	ofs_ope_apply_template( ope );
	entries = ofs_ope_generate_entries( ope );

	ope_number = ofo_dossier_get_next_ope( dossier );
	counts->ope_count += 1;

	for( it=entries ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		ofo_entry_set_ope_number( entry, ope_number );
		ofo_entry_insert( entry, priv->hub );
		counts->entry_count += 1;
	}
}

/*
 * settings: paned_position;cancelled_visible;waiting_visible;validated_visible;
 */
static void
get_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	gint pos;
	gchar *settings_key;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	slist = ofa_settings_user_get_string_list( settings_key );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	pos = 0;
	if( my_strlen( cstr )){
		pos = atoi( cstr );
	}
	if( pos <= 10 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->paned ), pos );

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->cancelled_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_cancelled_btn_toggled( GTK_TOGGLE_BUTTON( priv->cancelled_toggle ), self );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->waiting_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_waiting_btn_toggled( GTK_TOGGLE_BUTTON( priv->waiting_toggle ), self );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->validated_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_validated_btn_toggled( GTK_TOGGLE_BUTTON( priv->validated_toggle ), self );
	}

	ofa_settings_free_string_list( slist );
	g_free( settings_key );
}

static void
set_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gchar *str, *settings_key;
	gint pos;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );

	pos = gtk_paned_get_position( GTK_PANED( priv->paned ));

	str = g_strdup_printf( "%d;%s;%s;%s;",
			pos,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->cancelled_toggle )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->waiting_toggle )) ? "True":"False",
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->validated_toggle )) ? "True":"False" );

	ofa_settings_user_set_string( settings_key, str );

	g_free( str );
	g_free( settings_key );
}
