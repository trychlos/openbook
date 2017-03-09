/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include <stdarg.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ope-template-editable.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "recurrent/ofa-rec-period-bin.h"
#include "recurrent/ofa-recurrent-model-properties.h"
#include "recurrent/ofo-rec-period.h"
#include "recurrent/ofo-recurrent-model.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;
	GtkWindow         *parent;

	/* internals
	 */
	gboolean           is_writable;
	ofoRecurrentModel *recurrent_model;
	gboolean           is_new;
	gchar             *orig_template;

	/* UI
	 */
	GtkWidget         *ok_btn;
	GtkWidget         *msg_label;
	GtkWidget         *mnemo_entry;
	GtkWidget         *label_entry;
	GtkWidget         *ope_template_entry;
	GtkWidget         *ope_template_label;
	ofaRecPeriodBin   *periodicity_bin;
	GtkWidget         *def1_entry;
	GtkWidget         *def2_entry;
	GtkWidget         *def3_entry;
	GtkWidget         *enabled_btn;

	/* data
	 */
	gchar             *mnemo;
	gchar             *label;
	gchar             *ope_template;
	ofoOpeTemplate    *template_obj;
	ofoRecPeriod      *periodicity;
	ofxCounter         periodicity_detail;
	gboolean           enabled;
}
	ofaRecurrentModelPropertiesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-model-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_title( ofaRecurrentModelProperties *self );
static void     init_page_properties( ofaRecurrentModelProperties *self );
static void     setup_data( ofaRecurrentModelProperties *self );
static void     on_mnemo_changed( GtkEntry *entry, ofaRecurrentModelProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaRecurrentModelProperties *self );
static void     on_ope_template_changed( GtkEntry *entry, ofaRecurrentModelProperties *self );
static void     on_periodicity_changed( ofaRecPeriodBin *bin, ofoRecPeriod *period, ofaRecurrentModelProperties *self );
static void     on_periodicity_detail_changed( ofaRecPeriodBin *bin, ofoRecPeriod *period, ofxCounter detail_id, ofaRecurrentModelProperties *self );
static void     on_enabled_toggled( GtkToggleButton *button, ofaRecurrentModelProperties *self );
static void     check_for_enable_dlg( ofaRecurrentModelProperties *self );
static gboolean is_dialog_validable( ofaRecurrentModelProperties *self );
static gboolean check_for_mnemo( ofaRecurrentModelProperties *self, gchar **msgerr );
static void     on_ok_clicked( ofaRecurrentModelProperties *self );
static gboolean do_update( ofaRecurrentModelProperties *self, gchar **msgerr );
static void     set_msgerr( ofaRecurrentModelProperties *dialog, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelProperties, ofa_recurrent_model_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRecurrentModelProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
recurrent_model_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_finalize";
	ofaRecurrentModelPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_recurrent_model_properties_get_instance_private( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	g_free( priv->orig_template );
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->ope_template );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_properties_parent_class )->finalize( instance );
}

static void
recurrent_model_properties_dispose( GObject *instance )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_MODEL_PROPERTIES( instance ));

	priv = ofa_recurrent_model_properties_get_instance_private( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_model_properties_parent_class )->dispose( instance );
}

static void
ofa_recurrent_model_properties_init( ofaRecurrentModelProperties *self )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_init";
	ofaRecurrentModelPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_MODEL_PROPERTIES( self ));

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_recurrent_model_properties_class_init( ofaRecurrentModelPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_model_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_model_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_recurrent_model_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 * @model: the #ofoRecurrentModel to be displayed/updated.
 *
 * Update the properties of a recurrent model.
 */
void
ofa_recurrent_model_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoRecurrentModel *model )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_run";
	ofaRecurrentModelProperties *self;
	ofaRecurrentModelPropertiesPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	g_debug( "%s: getter=%p, parent=%p, model=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) model );

	self = g_object_new( OFA_TYPE_RECURRENT_MODEL_PROPERTIES, NULL );

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->recurrent_model = model;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_iwindow_init";
	ofaRecurrentModelPropertiesPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_model_properties_get_instance_private( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ), ofo_recurrent_model_get_mnemo( priv->recurrent_model ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * recurrent_model
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_idialog_init";
	ofaRecurrentModelPropertiesPrivate *priv;
	ofaHub *hub;
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_model_properties_get_instance_private( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	init_title( OFA_RECURRENT_MODEL_PROPERTIES( instance ));
	init_page_properties( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	my_utils_container_notes_init( GTK_CONTAINER( instance ), recurrent_model );
	my_utils_container_updstamp_init( GTK_CONTAINER( instance ), recurrent_model );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	setup_data( OFA_RECURRENT_MODEL_PROPERTIES( instance ));
	check_for_enable_dlg( OFA_RECURRENT_MODEL_PROPERTIES( instance ));
}

static void
init_title( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	const gchar *mnemo;
	gchar *title;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	mnemo = ofo_recurrent_model_get_mnemo( priv->recurrent_model );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new recurrent model" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » recurrent model" ), mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( self ), title );
	g_free( title );
}

static void
init_page_properties( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	GtkWidget *entry, *prompt, *label, *parent, *combo, *btn;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	/* mnemonic */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_mnemo_changed ), self );
	priv->mnemo_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	/* label */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );
	priv->label_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	/* operation template */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ope-template-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_ope_template_changed ), self );
	priv->ope_template_entry = entry;
	ofa_ope_template_editable_init( GTK_EDITABLE( entry ), priv->getter );

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ope-template-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ope-template-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->ope_template_label = label;

	/* periodicity */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-periodicity-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->periodicity_bin = ofa_rec_period_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->periodicity_bin ));
	g_signal_connect( priv->periodicity_bin, "ofa-perchanged", G_CALLBACK( on_periodicity_changed ), self );
	g_signal_connect( priv->periodicity_bin, "ofa-detchanged", G_CALLBACK( on_periodicity_detail_changed ), self );
	combo = ofa_rec_period_bin_get_periodicity_combo( priv->periodicity_bin );

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-period-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), combo );

	/* amount definitions */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-def1" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->def1_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-editables-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-def2" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->def2_entry = entry;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-def3" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->def3_entry = entry;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-enabled" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->enabled_btn = btn;
	g_signal_connect( priv->enabled_btn, "toggled", G_CALLBACK( on_enabled_toggled ), self );
}

static void
setup_data( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	const gchar *cstr;
	gboolean is_enabled;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	cstr = ofo_recurrent_model_get_mnemo( priv->recurrent_model );
	gtk_entry_set_text( GTK_ENTRY( priv->mnemo_entry ), cstr ? cstr : "" );

	cstr = ofo_recurrent_model_get_label( priv->recurrent_model );
	gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), cstr ? cstr : "" );

	cstr = ofo_recurrent_model_get_ope_template( priv->recurrent_model );
	gtk_entry_set_text( GTK_ENTRY( priv->ope_template_entry ), cstr ? cstr : "" );
	priv->orig_template = g_strdup( cstr );

	ofa_rec_period_bin_set_selected( priv->periodicity_bin,
			ofo_recurrent_model_get_periodicity( priv->recurrent_model ),
			ofo_recurrent_model_get_periodicity_detail( priv->recurrent_model ));

	cstr = ofo_recurrent_model_get_def_amount1( priv->recurrent_model );
	gtk_entry_set_text( GTK_ENTRY( priv->def1_entry ), cstr ? cstr : "" );

	cstr = ofo_recurrent_model_get_def_amount2( priv->recurrent_model );
	gtk_entry_set_text( GTK_ENTRY( priv->def2_entry ), cstr ? cstr : "" );

	cstr = ofo_recurrent_model_get_def_amount3( priv->recurrent_model );
	gtk_entry_set_text( GTK_ENTRY( priv->def3_entry ), cstr ? cstr : "" );

	is_enabled = ofo_recurrent_model_get_is_enabled( priv->recurrent_model );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->enabled_btn ), is_enabled );
	priv->enabled = is_enabled;
}

static void
on_mnemo_changed( GtkEntry *entry, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_ope_template_changed( GtkEntry *entry, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	g_free( priv->ope_template );
	priv->ope_template = g_strdup( gtk_entry_get_text( entry ));

	priv->template_obj = ofo_ope_template_get_by_mnemo( priv->getter, priv->ope_template );
	gtk_label_set_text(
			GTK_LABEL( priv->ope_template_label ),
			priv->template_obj ? ofo_ope_template_get_label( priv->template_obj ) : "" );

	check_for_enable_dlg( self );
}

static void
on_periodicity_changed( ofaRecPeriodBin *bin, ofoRecPeriod *period, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	priv->periodicity = period;
	priv->periodicity_detail = -1;

	check_for_enable_dlg( self );
}

static void
on_periodicity_detail_changed( ofaRecPeriodBin *bin, ofoRecPeriod *period, ofxCounter detail_id, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	priv->periodicity = period;
	priv->periodicity_detail = detail_id;

	check_for_enable_dlg( self );
}

static void
on_enabled_toggled( GtkToggleButton *button, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	priv->enabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->enabled_btn ));

	check_for_enable_dlg( self );
}

/*
 * are we able to validate this recurrent model
 */
static void
check_for_enable_dlg( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	gboolean ok;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	if( priv->is_writable ){
		ok = is_dialog_validable( self );
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

/*
 * are we able to validate this recurrent model
 *
 * Note that we always accept to save the dialog if the record is disabled
 */
static gboolean
is_dialog_validable( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	gboolean ok;
	gchar *msgerr;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	ok = check_for_mnemo( self, &msgerr );

	/* other tests must only be satisfied if enabled */
	if( ok && priv->enabled ){
		ok = ofo_recurrent_model_is_valid_data(
				priv->mnemo, priv->label, priv->ope_template,
						priv->periodicity, priv->periodicity_detail, &msgerr );

		if( ok && !priv->template_obj ){
			ok = FALSE;
			msgerr = g_strdup_printf( _( "Operation template '%s' is unknown" ), priv->ope_template );
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

/*
 * Check that mnemo is set and unique
 * This test must be satisfied in all cases:
 * - even if dossier is not writable
 * - even if model is disabled
 */
static gboolean
check_for_mnemo( ofaRecurrentModelProperties *self, gchar **msgerr )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	gboolean ok, exists, subok;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	ok = TRUE;
	*msgerr = NULL;

	if( !my_strlen( priv->mnemo )){
		ok = FALSE;
		*msgerr = g_strdup( _( "Mnemonic is empty" ));

	} else {
		exists = ( ofo_recurrent_model_get_by_mnemo( priv->getter, priv->mnemo ) != NULL );
		subok = !priv->is_new &&
						!my_collate( priv->mnemo, ofo_recurrent_model_get_mnemo( priv->recurrent_model ));
		ok = !exists || subok;
		if( !ok ){
			*msgerr = g_strdup( _( "Mnemonic is already defined" ));
		}
	}

	return( ok );
}

/*
 * either creating a new recurrent_model (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by its mnemo
 */
static void
on_ok_clicked( ofaRecurrentModelProperties *self )
{
	gchar *msgerr = NULL;

	do_update( self, &msgerr );

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

static gboolean
do_update( ofaRecurrentModelProperties *self, gchar **msgerr )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	ofaISignaler *signaler;
	gchar *prev_mnemo;
	gboolean ok, is_enabled;
	const gchar *cstr;
	ofoOpeTemplate *template_obj;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	msgerr = NULL;
	prev_mnemo = g_strdup( ofo_recurrent_model_get_mnemo( priv->recurrent_model ));

	ofo_recurrent_model_set_mnemo( priv->recurrent_model, priv->mnemo );
	ofo_recurrent_model_set_label( priv->recurrent_model, priv->label );
	ofo_recurrent_model_set_ope_template( priv->recurrent_model, priv->ope_template );
	if( priv->periodicity ){
		ofo_recurrent_model_set_periodicity( priv->recurrent_model, ofo_rec_period_get_id( priv->periodicity ));
		ofo_recurrent_model_set_periodicity_detail( priv->recurrent_model, priv->periodicity_detail );
	}

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->def1_entry ));
	ofo_recurrent_model_set_def_amount1( priv->recurrent_model, cstr );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->def2_entry ));
	ofo_recurrent_model_set_def_amount2( priv->recurrent_model, cstr );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->def3_entry ));
	ofo_recurrent_model_set_def_amount3( priv->recurrent_model, cstr );

	is_enabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->enabled_btn ));
	ofo_recurrent_model_set_is_enabled( priv->recurrent_model, is_enabled );

	my_utils_container_notes_get( GTK_WINDOW( self ), recurrent_model );

	if( priv->is_new ){
		ok = ofo_recurrent_model_insert( priv->recurrent_model );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new recurrent model" ));
		}
	} else {
		ok = ofo_recurrent_model_update( priv->recurrent_model, prev_mnemo );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the recurrent model" ));
		}
	}

	g_free( prev_mnemo );

	/* if the template has changed, then send an update message to the
	 * initial template to update the treeview
	 */
	if( ok ){
		if( my_strlen( priv->orig_template )){
			template_obj = ofo_ope_template_get_by_mnemo( priv->getter, priv->orig_template );
			if( template_obj ){
				g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, template_obj, NULL );
			}
		}
		if( my_strlen( priv->ope_template ) && my_collate( priv->ope_template, priv->orig_template )){
			template_obj = ofo_ope_template_get_by_mnemo( priv->getter, priv->ope_template );
			if( template_obj ){
				g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, template_obj, NULL );
			}
		}
	}

	return( ok );
}

static void
set_msgerr( ofaRecurrentModelProperties *dialog, const gchar *msg )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( dialog );

	if( !priv->msg_label ){
		priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
		my_style_add( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
