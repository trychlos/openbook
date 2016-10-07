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

#include "my/my-date-editable.h"
#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-periodicity.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-base.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-ope.h"

#include "ofa-recurrent-generate.h"
#include "ofa-recurrent-model-treeview.h"
#include "ofa-recurrent-run-store.h"
#include "ofa-recurrent-run-treeview.h"
#include "ofo-recurrent-gen.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* private instance data
 */
typedef struct {
	gboolean                 dispose_has_run;

	/* initialization
	 */
	ofaHub                  *hub;
	ofaIGetter              *getter;
	ofaRecurrentModelPage   *model_page;
	gchar                   *settings_prefix;

	/* internals
	 */
	GDate                    begin_date;
	GDate                    end_date;
	GList                   *dataset;
	ofaRecurrentRunStore    *store;

	/* UI
	 */
	GtkWidget               *top_paned;
	ofaRecurrentRunTreeview *tview;
	GtkWidget               *begin_entry;
	GtkWidget               *end_entry;
	GtkWidget               *ok_btn;
	GtkWidget               *msg_label;

	/* actions
	 */
	GSimpleAction           *reset_action;
	GSimpleAction           *generate_action;
}
	ofaRecurrentGeneratePrivate;

/* a structure passed to #ofa_periodicity_enum_dates_between()
 */
typedef struct {
	ofaRecurrentGenerate *self;
	ofoRecurrentModel    *model;
	ofoOpeTemplate       *template;
	GList                *opes;
	guint                 already;
	GList                *messages;
}
	sEnumDates;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-generate.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_treeview( ofaRecurrentGenerate *self );
static void     init_dates( ofaRecurrentGenerate *self );
static void     init_actions( ofaRecurrentGenerate *self );
static void     init_data( ofaRecurrentGenerate *self );
static void     on_begin_date_changed( GtkEditable *editable, ofaRecurrentGenerate *self );
static void     on_end_date_changed( GtkEditable *editable, ofaRecurrentGenerate *self );
static void     on_date_changed( ofaRecurrentGenerate *self, GtkEditable *editable, GDate *date );
static gboolean is_dialog_validable( ofaRecurrentGenerate *self );
static void     action_on_reset_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentGenerate *self );
static void     action_on_generate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentGenerate *self );
static void     generate_do( ofaRecurrentGenerate *self );
static gboolean confirm_redo( ofaRecurrentGenerate *self, const GDate *last_date );
static GList   *generate_do_opes( ofaRecurrentGenerate *self, ofoRecurrentModel *model, const GDate *begin_date, const GDate *end_date, GList **messages );
static void     generate_enum_dates_cb( const GDate *date, sEnumDates *data );
static void     display_error_messages( ofaRecurrentGenerate *self, GList *messages );
static gboolean do_record( ofaRecurrentGenerate *self, gchar **msgerr );
static void     get_settings( ofaRecurrentGenerate *self );
static void     set_settings( ofaRecurrentGenerate *self );
static void     set_msgerr( ofaRecurrentGenerate *self, const gchar *msg );
static void     iactionable_iface_init( ofaIActionableInterface *iface );
static guint    iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentGenerate, ofa_recurrent_generate, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRecurrentGenerate )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ))

static void
recurrent_generate_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_generate_finalize";
	ofaRecurrentGeneratePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_GENERATE( instance ));

	/* free data members here */
	priv = ofa_recurrent_generate_get_instance_private( OFA_RECURRENT_GENERATE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_generate_parent_class )->finalize( instance );
}

static void
recurrent_generate_dispose( GObject *instance )
{
	ofaRecurrentGeneratePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_GENERATE( instance ));

	priv = ofa_recurrent_generate_get_instance_private( OFA_RECURRENT_GENERATE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		set_settings( OFA_RECURRENT_GENERATE( instance ));

		/* unref object members here */
		g_list_free_full( priv->dataset, ( GDestroyNotify ) g_object_unref );

		g_object_unref( priv->reset_action );
		g_object_unref( priv->generate_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_generate_parent_class )->dispose( instance );
}

static void
ofa_recurrent_generate_init( ofaRecurrentGenerate *self )
{
	static const gchar *thisfn = "ofa_recurrent_generate_init";
	ofaRecurrentGeneratePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_GENERATE( self ));

	priv = ofa_recurrent_generate_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	my_date_clear( &priv->begin_date );
	my_date_clear( &priv->end_date );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_recurrent_generate_class_init( ofaRecurrentGenerateClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_generate_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_generate_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_generate_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_recurrent_generate_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 * @page: the current #ofaRecurrentModelPage page.
 *
 * Generate new operations from selected recurrent models.
 * Make sure there is one single dialog opened at time.
 */
void
ofa_recurrent_generate_run( ofaIGetter *getter, GtkWindow *parent, ofaRecurrentModelPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_generate_run";
	ofaRecurrentGenerate *self;
	ofaRecurrentGeneratePrivate *priv;
	ofaHub *hub;
	myICollector *collector;
	myIWindow *shown;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	g_debug( "%s: getter=%p, parent=%p, page=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) page );

	hub = ofa_igetter_get_hub( getter );
	collector = ofa_hub_get_collector( hub );
	self = ( ofaRecurrentGenerate * ) my_icollector_single_get_object( collector, OFA_TYPE_RECURRENT_GENERATE );

	if( self ){
		shown = my_iwindow_present( MY_IWINDOW( self ));
		g_return_if_fail( shown && OFA_IS_RECURRENT_GENERATE( shown ));

		if( is_dialog_validable( OFA_RECURRENT_GENERATE( shown ))){
			priv = ofa_recurrent_generate_get_instance_private( OFA_RECURRENT_GENERATE( shown ));
			g_action_activate( G_ACTION( priv->reset_action ), NULL );
			g_action_activate( G_ACTION( priv->generate_action ), NULL );
		}

	} else {
		self = g_object_new( OFA_TYPE_RECURRENT_GENERATE, NULL );
		my_icollector_single_set_object( collector, self );

		my_iwindow_set_parent( MY_IWINDOW( self ), parent );
		my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

		priv = ofa_recurrent_generate_get_instance_private( self );

		priv->getter = getter;
		priv->hub = hub;
		priv->model_page = page;

		/* after this call, @self may be invalid */
		my_iwindow_present( MY_IWINDOW( self ));
	}
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_generate_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_generate_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_recurrent_generate_idialog_init";
	ofaRecurrentGeneratePrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_generate_get_instance_private( OFA_RECURRENT_GENERATE( instance ));

	priv->top_paned = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "paned" );
	g_return_if_fail( priv->top_paned && GTK_IS_PANED( priv->top_paned ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_record );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );

	init_treeview( OFA_RECURRENT_GENERATE( instance ));
	init_dates( OFA_RECURRENT_GENERATE( instance ));
	init_actions( OFA_RECURRENT_GENERATE( instance ));
	init_data( OFA_RECURRENT_GENERATE( instance ));

get_settings( OFA_RECURRENT_GENERATE( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

/*
 * setup an emmpty treeview for to-be-generated ofoRecurrentRun opes
 */
static void
init_treeview( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	GtkWidget *parent;

	priv = ofa_recurrent_generate_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->tview = ofa_recurrent_run_treeview_new( priv->hub );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_recurrent_run_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_recurrent_run_treeview_setup_columns( priv->tview );
	ofa_recurrent_run_treeview_set_visible( priv->tview, REC_VISIBLE_WAITING );
	ofa_tvbin_set_selection_mode( OFA_TVBIN( priv->tview ), GTK_SELECTION_BROWSE );
}

/*
 * setup the dates frame
 * - last date from db
 * - begin date (which defaults to last date)
 * - end date
 */
static void
init_dates( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	GtkWidget *prompt, *entry, *label;
	const GDate *last_date;
	gchar *str;

	priv = ofa_recurrent_generate_get_instance_private( self );

	/* previous date */
	last_date = ofo_recurrent_gen_get_last_run_date( priv->hub );

	if( my_date_is_valid( last_date )){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-last-date" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));

		str = my_date_to_str( last_date, ofa_prefs_date_display());
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		my_date_set_from_date( &priv->begin_date, last_date );
		g_date_add_days( &priv->begin_date, 1 );
		my_date_set_from_date( &priv->end_date, &priv->begin_date );
	}

	/* (included) begin date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->begin_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-begin-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin_date );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite());

	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_date_changed ), self );

	/* (included) end date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->end_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-end-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end_date );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite());

	g_signal_connect( entry, "changed", G_CALLBACK( on_end_date_changed ), self );
}

static void
init_actions( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	GtkWidget *btn;
	GMenu *menu;

	priv = ofa_recurrent_generate_get_instance_private( self );

	priv->reset_action = g_simple_action_new( "reset", NULL );
	g_signal_connect( priv->reset_action, "activate", G_CALLBACK( action_on_reset_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->reset_action ),
			_( "Clear the operations" ));
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-reset-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->reset_action ));
	g_simple_action_set_enabled( priv->reset_action, FALSE );

	priv->generate_action = g_simple_action_new( "generate", NULL );
	g_signal_connect( priv->generate_action, "activate", G_CALLBACK( action_on_generate_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->generate_action ),
			_( "Generate operations" ));
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-generate-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	ofa_iactionable_set_button(
			OFA_IACTIONABLE( self ), btn, priv->settings_prefix, G_ACTION( priv->generate_action ));
	g_simple_action_set_enabled( priv->generate_action, FALSE );

	/* setup the context menu */
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static void
init_data( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;

	priv = ofa_recurrent_generate_get_instance_private( self );

	priv->store = ofa_recurrent_run_store_new( priv->hub, REC_MODE_FROM_LIST );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );
}

static void
on_begin_date_changed( GtkEditable *editable, ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;

	priv = ofa_recurrent_generate_get_instance_private( self );

	on_date_changed( self, editable, &priv->begin_date );
}

static void
on_end_date_changed( GtkEditable *editable, ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;

	priv = ofa_recurrent_generate_get_instance_private( self );

	on_date_changed( self, editable, &priv->end_date );
}

static void
on_date_changed( ofaRecurrentGenerate *self, GtkEditable *editable, GDate *date )
{
	my_date_set_from_date( date, my_date_editable_get_date( editable, NULL ));
	is_dialog_validable( self );
}

static gboolean
is_dialog_validable( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	gboolean valid;
	gchar *msgerr;

	priv = ofa_recurrent_generate_get_instance_private( self );

	msgerr = NULL;
	valid = TRUE;

	priv = ofa_recurrent_generate_get_instance_private( self );

	if( !my_date_is_valid( &priv->begin_date )){
		msgerr = g_strdup( _( "Empty beginning date" ));
		valid = FALSE;

	} else if( !my_date_is_valid( &priv->end_date )){
		msgerr = g_strdup( _( "Empty ending date" ));
		valid = FALSE;

	} else if( my_date_compare( &priv->begin_date, &priv->end_date ) > 0 ){
		msgerr = g_strdup( _( "Beginning date is greater than ending date" ));
		valid = FALSE;
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	g_simple_action_set_enabled( priv->generate_action, valid );

	return( valid );
}

static void
action_on_reset_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;

	priv = ofa_recurrent_generate_get_instance_private( self );

	gtk_list_store_clear( GTK_LIST_STORE( priv->store ));
	g_list_free_full( priv->dataset, ( GDestroyNotify ) g_object_unref );

	gtk_widget_set_sensitive( priv->begin_entry, TRUE );
	gtk_widget_set_sensitive( priv->end_entry, TRUE );
	g_simple_action_set_enabled( priv->generate_action, TRUE );

	g_simple_action_set_enabled( priv->reset_action, FALSE );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );
}

static void
action_on_generate_activated( GSimpleAction *action, GVariant *empty, ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;

	priv = ofa_recurrent_generate_get_instance_private( self );

	gtk_widget_set_sensitive( priv->begin_entry, FALSE );
	gtk_widget_set_sensitive( priv->end_entry, FALSE );
	g_simple_action_set_enabled( priv->generate_action, FALSE );

	generate_do( self );
}

static void
generate_do( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	GList *models_dataset, *it, *opes, *messages;
	const GDate *last_date;
	gchar *str;
	gint count;

	priv = ofa_recurrent_generate_get_instance_private( self );

	models_dataset = ofa_recurrent_model_page_get_selected( priv->model_page );
	//g_debug( "generate_do: models_dataset_count=%d", g_list_length( models_dataset ));

	count = 0;
	opes = NULL;
	messages = NULL;
	last_date = ofo_recurrent_gen_get_last_run_date( priv->hub );

	if( !my_date_is_valid( last_date ) ||
			my_date_compare( &priv->begin_date, last_date ) > 0 ||
			confirm_redo( self, last_date )){

		/* for each selected template,
		 *   generate recurrent operations between provided dates */
		for( it=models_dataset ; it ; it=it->next ){
			opes = g_list_concat( opes,
					generate_do_opes( self, OFO_RECURRENT_MODEL( it->data ), &priv->begin_date, &priv->end_date, &messages ));
		}

		if( g_list_length( messages )){
			display_error_messages( self, messages );
			g_list_free_full( messages, ( GDestroyNotify ) g_free );
		}

		count = g_list_length( opes );
		ofa_recurrent_run_store_set_from_list( priv->store, opes );

		if( count == 0 ){
			str = g_strdup( _( "No generated operation" ));
		} else if( count == 1 ){
			str = g_strdup( _( "One generated operation" ));
		} else {
			str = g_strdup_printf( _( "%d generated operations" ), count );
		}
		my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_INFO, str );
		g_free( str );
	}

	priv->dataset = opes;

	if( count == 0 ){
		gtk_widget_set_sensitive( priv->begin_entry, TRUE );
		gtk_widget_set_sensitive( priv->end_entry, TRUE );

	} else {
		g_simple_action_set_enabled( priv->reset_action, TRUE );
		gtk_widget_set_sensitive( priv->ok_btn, TRUE );

		gtk_widget_set_sensitive( priv->begin_entry, FALSE );
		gtk_widget_set_sensitive( priv->end_entry, FALSE );
		g_simple_action_set_enabled( priv->generate_action, FALSE );
	}

	ofa_recurrent_model_treeview_free_selected( models_dataset );
}

/*
 * Requests a user confirm when beginning date of the generation is less
 * or equal than last generation date
 */
static gboolean
confirm_redo( ofaRecurrentGenerate *self, const GDate *last_date )
{
	ofaRecurrentGeneratePrivate *priv;
	gchar *sbegin, *slast, *str;
	gboolean ok;

	priv = ofa_recurrent_generate_get_instance_private( self );

	sbegin = my_date_to_str( &priv->begin_date, ofa_prefs_date_display());
	slast = my_date_to_str( last_date, ofa_prefs_date_display());

	str = g_strdup_printf(
			_( "Beginning date %s is less or equal to previous generation date %s.\n"
				"Please note that already generated operations will not be re-generated.\n"
				"If they have been cancelled, you might want cancel the cancellation instead.\n"
				"Do you confirm this generation ?" ), sbegin, slast );

	ok = my_utils_dialog_question( str, _( "C_onfirm" ));

	g_free( sbegin );
	g_free( slast );
	g_free( str );

	return( ok );
}

/*
 * generating new operations (mnemo+date) between the two dates
 */
static GList *
generate_do_opes( ofaRecurrentGenerate *self, ofoRecurrentModel *model, const GDate *begin_date, const GDate *end_date, GList **messages )
{
	static const gchar *thisfn = "ofa_recurrent_generate_generate_do_opes";
	ofaRecurrentGeneratePrivate *priv;
	const gchar *per_main, *per_detail;
	sEnumDates sdata;

	priv = ofa_recurrent_generate_get_instance_private( self );

	sdata.self = self;
	sdata.opes = NULL;
	sdata.already = 0;
	sdata.messages = NULL;

	if( ofo_recurrent_model_get_is_enabled( model )){

		sdata.model = model;
		sdata.template = ofo_ope_template_get_by_mnemo( priv->hub, ofo_recurrent_model_get_ope_template( model ));
		per_main = ofo_recurrent_model_get_periodicity( model );
		per_detail = ofo_recurrent_model_get_periodicity_detail( model );

		g_debug( "%s: model=%s, periodicity=%s,%s",
				thisfn, ofo_recurrent_model_get_label( model ), per_main, per_detail );

		ofa_periodicity_enum_dates_between(
				per_main, per_detail,
				begin_date, end_date,
				( PeriodicityDatesCb ) generate_enum_dates_cb, &sdata );

		if( g_list_length( sdata.messages )){
			*messages = g_list_concat( *messages, sdata.messages );
		}
	}

	return( sdata.opes );
}

/*
 * Takes care of having at most one Waiting|Validated operation for a
 * given mnemo+date couple
 */
static void
generate_enum_dates_cb( const GDate *date, sEnumDates *data )
{
	ofaRecurrentGeneratePrivate *priv;
	ofoRecurrentRun *recrun;
	const gchar *mnemo, *csdef;
	ofsOpe *ope;
	gchar *msg, *str;
	gboolean valid;

	priv = ofa_recurrent_generate_get_instance_private( data->self );

	mnemo = ofo_recurrent_model_get_mnemo( data->model );

	recrun = ofo_recurrent_run_get_by_id( priv->hub, mnemo, date );
	if( recrun ){
		data->already += 1;

	} else {
		recrun = ofo_recurrent_run_new();
		ofo_recurrent_run_set_mnemo( recrun, mnemo );
		ofo_recurrent_run_set_date( recrun, date );
		ofo_base_set_hub( OFO_BASE( recrun ), priv->hub );

		valid = TRUE;
		ope = ofs_ope_new( data->template );
		my_date_set_from_date( &ope->dope, date );
		ope->dope_user_set = TRUE;
		ofs_ope_apply_template( ope );

		csdef = ofo_recurrent_model_get_def_amount1( data->model );
		if( my_strlen( csdef )){
			ofo_recurrent_run_set_amount1( recrun, ofs_ope_get_amount( ope, csdef, &msg ));
			if( my_strlen( msg )){
				str = g_strdup_printf(
						_( "Model='%s', specification='%s': %s" ), mnemo, csdef, msg );
				data->messages = g_list_append( data->messages, str );
				valid = FALSE;
			}
			g_free( msg );
		}

		csdef = ofo_recurrent_model_get_def_amount2( data->model );
		if( my_strlen( csdef )){
			ofo_recurrent_run_set_amount2( recrun, ofs_ope_get_amount( ope, csdef, &msg ));
			if( my_strlen( msg )){
				str = g_strdup_printf(
						_( "Model='%s', specification='%s': %s" ), mnemo, csdef, msg );
				data->messages = g_list_append( data->messages, str );
				valid = FALSE;
			}
			g_free( msg );
		}

		csdef = ofo_recurrent_model_get_def_amount3( data->model );
		if( my_strlen( csdef )){
			ofo_recurrent_run_set_amount3( recrun, ofs_ope_get_amount( ope, csdef, &msg ));
			if( my_strlen( msg )){
				str = g_strdup_printf(
						_( "Model='%s', specification='%s': %s" ), mnemo, csdef, msg );
				data->messages = g_list_append( data->messages, str );
				valid = FALSE;
			}
			g_free( msg );
		}

		if( valid ){
			data->opes = g_list_prepend( data->opes, recrun );

		} else {
			g_object_unref( recrun );
		}
	}
}

static void
display_error_messages( ofaRecurrentGenerate *self, GList *messages )
{
	GString *str;
	GList *it;
	gboolean first;

	str = g_string_new( "" );
	first = TRUE;

	for( it=messages ; it ; it=it->next ){
		if( !first ){
			str = g_string_append( str, "\n" );
		}
		str = g_string_append( str, ( const gchar * ) it->data );
		first = FALSE;
	}

	my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_ERROR, str->str );

	g_string_free( str, TRUE );
}

/*
 * at user validation, record newly generated recurrent operations
 *  in the DBMS with 'waiting' status
 */
static gboolean
do_record( ofaRecurrentGenerate *self, gchar **msgerr )
{
	ofaRecurrentGeneratePrivate *priv;
	ofoRecurrentRun *object;
	GList *it;
	gchar *str;
	gint count;

	priv = ofa_recurrent_generate_get_instance_private( self );

	for( it=priv->dataset ; it ; it=it->next ){
		object = OFO_RECURRENT_RUN( it->data );
		if( !ofo_recurrent_run_insert( object, priv->hub )){
			if( msgerr ){
				*msgerr = g_strdup( _( "Unable to insert a new operation" ));
			}
			return( FALSE );
		}
		/* this is the reference we just give to the collection dataset */
		g_object_ref( object );
	}

	count = g_list_length( priv->dataset );
	ofo_recurrent_gen_set_last_run_date( priv->hub, &priv->end_date );

	if( count == 1 ){
		str = g_strdup( _( "One successfully inserted operation" ));
	} else {
		str = g_strdup_printf( _( "%d successfully inserted operations" ), count );
	}
	my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_INFO, str );
	g_free( str );

	return( TRUE );
}

/*
 * settings: paned_position;
 */
static void
get_settings( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	gint pos;
	gchar *settings_key;

	priv = ofa_recurrent_generate_get_instance_private( self );

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
	gtk_paned_set_position( GTK_PANED( priv->top_paned ), pos );

	ofa_settings_free_string_list( slist );
	g_free( settings_key );
}

static void
set_settings( ofaRecurrentGenerate *self )
{
	ofaRecurrentGeneratePrivate *priv;
	gchar *str, *settings_key;
	gint pos;

	priv = ofa_recurrent_generate_get_instance_private( self );

	pos = gtk_paned_get_position( GTK_PANED( priv->top_paned ));

	settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	str = g_strdup_printf( "%d;", pos );

	ofa_settings_user_set_string( settings_key, str );

	g_free( str );
	g_free( settings_key );
}

static void
set_msgerr( ofaRecurrentGenerate *self, const gchar *msg )
{
	ofaRecurrentGeneratePrivate *priv;
	GtkWidget *label;

	priv = ofa_recurrent_generate_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_account_frame_bin_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
