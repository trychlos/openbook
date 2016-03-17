/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "api/my-editable-date.h"
#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-periodicity.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"

#include "core/ofa-main-window.h"

#include "ofa-recurrent-new.h"
#include "ofa-recurrent-run-treeview.h"
#include "ofo-recurrent-gen.h"
#include "ofo-recurrent-model.h"
#include "ofo-recurrent-run.h"

/* private instance data
 */
struct _ofaRecurrentNewPrivate {
	gboolean                 dispose_has_run;

	/* internals
	 */
	ofaHub                  *hub;
	GDate                    begin_date;
	GDate                    end_date;
	GList                   *dataset;

	/* UI
	 */
	GtkWidget               *top_paned;
	ofaRecurrentRunTreeview *tview;
	GtkWidget               *begin_entry;
	GtkWidget               *end_entry;
	GtkWidget               *ok_btn;
	GtkWidget               *generate_btn;
	GtkWidget               *reset_btn;
	GtkWidget               *msg_label;
};

/* a structure passed to #ofa_periodicity_enum_dates_between()
 */
typedef struct {
	ofaRecurrentNew *self;
	ofoRecurrentModel      *model;
	GList                  *opes;
}
	sEnumDates;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-new.ui";
static const gchar *st_settings         = "ofaRecurrentNew-settings";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_treeview( ofaRecurrentNew *self );
static void     init_dates( ofaRecurrentNew *self );
static void     generate_on_begin_date_changed( GtkEditable *editable, ofaRecurrentNew *self );
static void     generate_on_end_date_changed( GtkEditable *editable, ofaRecurrentNew *self );
static void     generate_on_date_changed( ofaRecurrentNew *self, GtkEditable *editable, GDate *date );
static void     generate_on_btn_clicked( GtkButton *button, ofaRecurrentNew *self );
static void     generate_on_reset_clicked( GtkButton *button, ofaRecurrentNew *self );
static void     generate_do( ofaRecurrentNew *self );
static GList   *generate_do_opes( ofaRecurrentNew *self, ofoRecurrentModel *model, const GDate *begin_date, const GDate *end_date );
static void     generate_enum_dates_cb( const GDate *date, sEnumDates *data );
static gboolean do_update( ofaRecurrentNew *self, gchar **msgerr );
static void     get_settings( ofaRecurrentNew *self );
static void     set_settings( ofaRecurrentNew *self );
static void     set_msgerr( ofaRecurrentNew *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentNew, ofa_recurrent_new, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRecurrentNew )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
recurrent_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_new_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_NEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_new_parent_class )->finalize( instance );
}

static void
recurrent_new_dispose( GObject *instance )
{
	ofaRecurrentNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_NEW( instance ));

	priv = ofa_recurrent_new_get_instance_private( OFA_RECURRENT_NEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		set_settings( OFA_RECURRENT_NEW( instance ));

		/* unref object members here */

		g_list_free_full( priv->dataset, ( GDestroyNotify ) g_object_unref );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_new_parent_class )->dispose( instance );
}

static void
ofa_recurrent_new_init( ofaRecurrentNew *self )
{
	static const gchar *thisfn = "ofa_recurrent_new_init";
	ofaRecurrentNewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_NEW( self ));

	priv = ofa_recurrent_new_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	my_date_clear( &priv->begin_date );
	my_date_clear( &priv->end_date );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_recurrent_new_class_init( ofaRecurrentNewClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_new_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_recurrent_new_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @parent: [allow-none]: the #GtkWindow parent of this dialog.
 */
void
ofa_recurrent_new_run( ofaMainWindow *main_window, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_recurrent_new_run";
	ofaRecurrentNew *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	g_debug( "%s: main_window=%p, parent=%p",
			thisfn, ( void * ) main_window, ( void * ) parent );

	self = g_object_new( OFA_TYPE_RECURRENT_NEW, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_new_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_recurrent_new_idialog_init";
	ofaRecurrentNewPrivate *priv;
	GtkApplicationWindow *main_window;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_new_get_instance_private( OFA_RECURRENT_NEW( instance ));

	priv->top_paned = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "paned" );
	g_return_if_fail( priv->top_paned && GTK_IS_PANED( priv->top_paned ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	init_treeview( OFA_RECURRENT_NEW( instance ));
	init_dates( OFA_RECURRENT_NEW( instance ));

	get_settings( OFA_RECURRENT_NEW( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

/*
 * setup an emmpty treeview for to-be-generated ofoRecurrentRun opes
 */
static void
init_treeview( ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;
	GtkWidget *parent;

	priv = ofa_recurrent_new_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->tview = ofa_recurrent_run_treeview_new( priv->hub );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));

	ofa_recurrent_run_treeview_set_visible( priv->tview, REC_STATUS_WAITING, TRUE );
}

/*
 * setup the dates frame
 * - last date from db
 * - begin date (which defaults to last date)
 * - end date
 */
static void
init_dates( ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;
	GtkWidget *prompt, *entry, *label, *btn;
	const GDate *last_date;
	gchar *str;

	priv = ofa_recurrent_new_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-generate-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->generate_btn = btn;

	g_signal_connect( btn, "clicked", G_CALLBACK( generate_on_btn_clicked ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-reset-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->reset_btn = btn;
	gtk_widget_set_sensitive( btn, FALSE );
	g_signal_connect( btn, "clicked", G_CALLBACK( generate_on_reset_clicked ), self );

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

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->begin_date );

	g_signal_connect( entry, "changed", G_CALLBACK( generate_on_begin_date_changed ), self );

	/* (included) end date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->end_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-end-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p22-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->end_date );

	g_signal_connect( entry, "changed", G_CALLBACK( generate_on_end_date_changed ), self );
}

static void
generate_on_begin_date_changed( GtkEditable *editable, ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;

	priv = ofa_recurrent_new_get_instance_private( self );

	generate_on_date_changed( self, editable, &priv->begin_date );
}

static void
generate_on_end_date_changed( GtkEditable *editable, ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;

	priv = ofa_recurrent_new_get_instance_private( self );

	generate_on_date_changed( self, editable, &priv->end_date );
}

static void
generate_on_date_changed( ofaRecurrentNew *self, GtkEditable *editable, GDate *date )
{
	ofaRecurrentNewPrivate *priv;
	gboolean valid;
	gchar *msgerr;

	priv = ofa_recurrent_new_get_instance_private( self );

	my_date_set_from_date( date, my_editable_date_get_date( editable, &valid ));
	msgerr = NULL;
	valid = TRUE;

	priv = ofa_recurrent_new_get_instance_private( self );

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

	gtk_widget_set_sensitive( priv->generate_btn, valid );
}

static void
generate_on_btn_clicked( GtkButton *button, ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;

	priv = ofa_recurrent_new_get_instance_private( self );

	gtk_widget_set_sensitive( priv->begin_entry, FALSE );
	gtk_widget_set_sensitive( priv->end_entry, FALSE );
	gtk_widget_set_sensitive( priv->generate_btn, FALSE );

	generate_do( self );
}

static void
generate_on_reset_clicked( GtkButton *button, ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;

	priv = ofa_recurrent_new_get_instance_private( self );

	ofa_recurrent_run_treeview_clear( priv->tview );
	g_list_free_full( priv->dataset, ( GDestroyNotify ) g_object_unref );

	gtk_widget_set_sensitive( priv->begin_entry, TRUE );
	gtk_widget_set_sensitive( priv->end_entry, TRUE );
	gtk_widget_set_sensitive( priv->generate_btn, TRUE );

	gtk_widget_set_sensitive( priv->reset_btn, FALSE );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );
}

static void
generate_do( ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;
	GList *models_dataset, *it, *opes;
	gchar *str;
	gint count;

	priv = ofa_recurrent_new_get_instance_private( self );

	models_dataset = ofo_recurrent_model_get_dataset( priv->hub );

	opes = NULL;

	for( it=models_dataset ; it ; it=it->next ){
		opes = g_list_concat( opes,
				generate_do_opes( self, OFO_RECURRENT_MODEL( it->data ), &priv->begin_date, &priv->end_date ));
	}

	count = g_list_length( opes );
	ofa_recurrent_run_treeview_set_from_list( priv->tview, opes );

	if( count == 0 ){
		str = g_strdup( _( "No generated operation" ));
	} else if( count == 1 ){
		str = g_strdup( _( "One generated operation" ));
	} else {
		str = g_strdup_printf( _( "%d generated operations" ), count );
	}
	my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_INFO, str );
	g_free( str );

	priv->dataset = opes;

	if( count == 0 ){
		gtk_widget_set_sensitive( priv->begin_entry, TRUE );
		gtk_widget_set_sensitive( priv->end_entry, TRUE );

	} else {
		gtk_widget_set_sensitive( priv->reset_btn, TRUE );
		gtk_widget_set_sensitive( priv->ok_btn, TRUE );

		gtk_widget_set_sensitive( priv->begin_entry, FALSE );
		gtk_widget_set_sensitive( priv->end_entry, FALSE );
		gtk_widget_set_sensitive( priv->generate_btn, FALSE );
	}
}

/*
 * generating new operations (mnemo+date) between the two dates
 */
static GList *
generate_do_opes( ofaRecurrentNew *self, ofoRecurrentModel *model, const GDate *begin_date, const GDate *end_date )
{
	const gchar *per_main, *per_detail;
	sEnumDates sdata;

	per_main = ofo_recurrent_model_get_periodicity( model );
	per_detail = ofo_recurrent_model_get_periodicity_detail( model );

	sdata.self = self;
	sdata.model = model;
	sdata.opes = NULL;

	ofa_periodicity_enum_dates_between(
			per_main, per_detail,
			begin_date, end_date,
			( PeriodicityDatesCb ) generate_enum_dates_cb, &sdata );

	return( sdata.opes );
}

static void
generate_enum_dates_cb( const GDate *date, sEnumDates *data )
{
	ofaRecurrentNewPrivate *priv;
	ofoRecurrentRun *ope;
	const gchar *mnemo;

	priv = ofa_recurrent_new_get_instance_private( data->self );

	mnemo = ofo_recurrent_model_get_mnemo( data->model );
	ope = ofo_recurrent_run_get_by_id( priv->hub, mnemo, date );

	if( !ope ){
		ope = ofo_recurrent_run_new();
		ofo_recurrent_run_set_mnemo( ope, mnemo );
		ofo_recurrent_run_set_date( ope, date );
		data->opes = g_list_prepend( data->opes, ope );
	}
}

static gboolean
do_update( ofaRecurrentNew *self, gchar **msgerr )
{
	ofaRecurrentNewPrivate *priv;
	ofoRecurrentRun *object;
	GList *it;
	gchar *str;
	gint count;

	priv = ofa_recurrent_new_get_instance_private( self );

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
 * settings: sort_column_id;sort_sens;paned_position;
 */
static void
get_settings( ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	gint sort_column_id, sort_sens, pos;

	priv = ofa_recurrent_new_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_settings );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	sort_column_id = cstr ? atoi( cstr ) : 0;

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	sort_sens = cstr ? atoi( cstr ) : 0;

	ofa_recurrent_run_treeview_set_sort_settings( priv->tview, sort_column_id, sort_sens );

	it = it ? it->next : NULL;
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
}

static void
set_settings( ofaRecurrentNew *self )
{
	ofaRecurrentNewPrivate *priv;
	gchar *str;
	gint sort_column_id, sort_sens, pos;

	priv = ofa_recurrent_new_get_instance_private( self );

	ofa_recurrent_run_treeview_get_sort_settings( priv->tview, &sort_column_id, &sort_sens );
	pos = gtk_paned_get_position( GTK_PANED( priv->top_paned ));

	str = g_strdup_printf( "%d;%d;%d;", sort_column_id, sort_sens, pos );

	ofa_settings_user_set_string( st_settings, str );

	g_free( str );
}

static void
set_msgerr( ofaRecurrentNew *self, const gchar *msg )
{
	ofaRecurrentNewPrivate *priv;
	GtkWidget *label;

	priv = ofa_recurrent_new_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
