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

#include "my/my-date-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-periodicity.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-base.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-ope.h"

#include "ofa-recurrent-new.h"
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
	ofaIGetter              *getter;

	/* internals
	 */
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
}
	ofaRecurrentNewPrivate;

/* a structure passed to #ofa_periodicity_enum_dates_between()
 */
typedef struct {
	ofaRecurrentNew   *self;
	ofoRecurrentModel *model;
	ofoOpeTemplate    *template;
	GList             *opes;
	guint              already;
	GList             *messages;
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
static gboolean confirm_redo( ofaRecurrentNew *self, const GDate *last_date );
static GList   *generate_do_opes( ofaRecurrentNew *self, ofoRecurrentModel *model, const GDate *begin_date, const GDate *end_date, GList **messages );
static void     generate_enum_dates_cb( const GDate *date, sEnumDates *data );
static void     display_error_messages( ofaRecurrentNew *self, GList *messages );
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
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 */
void
ofa_recurrent_new_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_recurrent_new_run";
	ofaRecurrentNew *self;
	ofaRecurrentNewPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	g_debug( "%s: getter=%p, parent=%p",
			thisfn, ( void * ) getter, ( void * ) parent );

	self = g_object_new( OFA_TYPE_RECURRENT_NEW, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_recurrent_new_get_instance_private( self );

	priv->getter = getter;

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

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_new_get_instance_private( OFA_RECURRENT_NEW( instance ));

	priv->top_paned = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "paned" );
	g_return_if_fail( priv->top_paned && GTK_IS_PANED( priv->top_paned ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );
	gtk_widget_set_sensitive( priv->ok_btn, FALSE );

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
	ofaHub *hub;

	priv = ofa_recurrent_new_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	hub = ofa_igetter_get_hub( priv->getter );

	priv->tview = ofa_recurrent_run_treeview_new( hub, FALSE );
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
	ofaHub *hub;

	priv = ofa_recurrent_new_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

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
	last_date = ofo_recurrent_gen_get_last_run_date( hub );

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

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end_date );

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

	my_date_set_from_date( date, my_date_editable_get_date( editable, &valid ));
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
	GList *models_dataset, *it, *opes, *messages;
	const GDate *last_date;
	gchar *str;
	gint count;
	ofaHub *hub;

	priv = ofa_recurrent_new_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	models_dataset = ofo_recurrent_model_get_dataset( hub );
	//g_debug( "generate_do: models_dataset_count=%d", g_list_length( models_dataset ));

	count = 0;
	opes = NULL;
	messages = NULL;
	last_date = ofo_recurrent_gen_get_last_run_date( hub );

	if( !my_date_is_valid( last_date ) ||
			my_date_compare( &priv->begin_date, last_date ) > 0 ||
			confirm_redo( self, last_date )){

		for( it=models_dataset ; it ; it=it->next ){
			opes = g_list_concat( opes,
					generate_do_opes( self, OFO_RECURRENT_MODEL( it->data ), &priv->begin_date, &priv->end_date, &messages ));
		}

		if( g_list_length( messages )){
			display_error_messages( self, messages );
			g_list_free_full( messages, ( GDestroyNotify ) g_free );
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
	}

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
 * Requests a user confirm when beginning date of the generation is less
 * or equal than last generation date
 */
static gboolean
confirm_redo( ofaRecurrentNew *self, const GDate *last_date )
{
	ofaRecurrentNewPrivate *priv;
	gchar *sbegin, *slast, *str;
	gboolean ok;

	priv = ofa_recurrent_new_get_instance_private( self );

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
generate_do_opes( ofaRecurrentNew *self, ofoRecurrentModel *model, const GDate *begin_date, const GDate *end_date, GList **messages )
{
	static const gchar *thisfn = "ofa_recurrent_new_generate_do_opes";
	ofaRecurrentNewPrivate *priv;
	const gchar *per_main, *per_detail;
	sEnumDates sdata;
	ofaHub *hub;

	priv = ofa_recurrent_new_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	per_main = ofo_recurrent_model_get_periodicity( model );
	per_detail = ofo_recurrent_model_get_periodicity_detail( model );

	sdata.self = self;
	sdata.model = model;
	sdata.template = ofo_ope_template_get_by_mnemo( hub, ofo_recurrent_model_get_ope_template( model ));
	sdata.opes = NULL;
	sdata.already = 0;
	sdata.messages = NULL;

	g_debug( "%s: model=%s, periodicity=%s,%s",
			thisfn, ofo_recurrent_model_get_label( model ), per_main, per_detail );

	ofa_periodicity_enum_dates_between(
			per_main, per_detail,
			begin_date, end_date,
			( PeriodicityDatesCb ) generate_enum_dates_cb, &sdata );

	if( g_list_length( sdata.messages )){
		*messages = g_list_concat( *messages, sdata.messages );
	}

	return( sdata.opes );
}

static void
generate_enum_dates_cb( const GDate *date, sEnumDates *data )
{
	ofaRecurrentNewPrivate *priv;
	ofoRecurrentRun *recrun;
	const gchar *mnemo, *csdef;
	ofaHub *hub;
	ofsOpe *ope;
	gchar *msg, *str;
	gboolean valid;

	priv = ofa_recurrent_new_get_instance_private( data->self );

	hub = ofa_igetter_get_hub( priv->getter );
	mnemo = ofo_recurrent_model_get_mnemo( data->model );

	recrun = ofo_recurrent_run_get_by_id( hub, mnemo, date );
	if( recrun ){
		data->already += 1;

	} else {
		recrun = ofo_recurrent_run_new();
		ofo_recurrent_run_set_mnemo( recrun, mnemo );
		ofo_recurrent_run_set_date( recrun, date );
		ofo_base_set_hub( OFO_BASE( recrun ), hub );

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
display_error_messages( ofaRecurrentNew *self, GList *messages )
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

static gboolean
do_update( ofaRecurrentNew *self, gchar **msgerr )
{
	ofaRecurrentNewPrivate *priv;
	ofoRecurrentRun *object;
	GList *it;
	gchar *str;
	gint count;
	ofaHub *hub;

	priv = ofa_recurrent_new_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	for( it=priv->dataset ; it ; it=it->next ){
		object = OFO_RECURRENT_RUN( it->data );
		if( !ofo_recurrent_run_insert( object, hub )){
			if( msgerr ){
				*msgerr = g_strdup( _( "Unable to insert a new operation" ));
			}
			return( FALSE );
		}
		/* this is the reference we just give to the collection dataset */
		g_object_ref( object );
	}

	count = g_list_length( priv->dataset );
	ofo_recurrent_gen_set_last_run_date( hub, &priv->end_date );

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
