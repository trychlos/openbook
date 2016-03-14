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
#include <stdarg.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-idialog.h"
#include "api/my-igridlist.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ientry-ope-template.h"
#include "api/ofa-periodicity-combo.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-main-window.h"

#include "recurrent/ofa-recurrent-model-properties.h"
#include "recurrent/ofo-recurrent-model.h"

/* private instance data
 */
struct _ofaRecurrentModelPropertiesPrivate {
	gboolean             dispose_has_run;

	/* internals
	 */
	ofaHub              *hub;
	gboolean             is_current;
	ofoRecurrentModel   *recurrent_model;
	gboolean             is_new;

	/* UI
	 */
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;
	GtkWidget           *mnemo_entry;
	GtkWidget           *label_entry;
	GtkWidget           *ope_template_entry;
	GtkWidget           *ope_template_label;
	ofaPeriodicityCombo *periodicity_combo;

	/* data
	 */
	gchar               *mnemo;
	gchar               *label;
	gchar               *ope_template;
	ofoOpeTemplate      *template_obj;
	gchar               *periodicity;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-model-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_title( ofaRecurrentModelProperties *self );
static void     init_page_properties( ofaRecurrentModelProperties *self );
static void     setup_data( ofaRecurrentModelProperties *self );
static void     ientry_ope_template_iface_init( ofaIEntryOpeTemplateInterface *iface );
static void     on_mnemo_changed( GtkEntry *entry, ofaRecurrentModelProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaRecurrentModelProperties *self );
static void     on_ope_template_changed( GtkEntry *entry, ofaRecurrentModelProperties *self );
static void     on_periodicity_changed( ofaPeriodicityCombo *combo, const gchar *code, ofaRecurrentModelProperties *self );
static void     check_for_enable_dlg( ofaRecurrentModelProperties *self );
static gboolean is_dialog_validable( ofaRecurrentModelProperties *self );
static gboolean do_update( ofaRecurrentModelProperties *self, gchar **msgerr );
static void     set_msgerr( ofaRecurrentModelProperties *dialog, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentModelProperties, ofa_recurrent_model_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRecurrentModelProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IENTRY_OPE_TEMPLATE, ientry_ope_template_iface_init ))

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

	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->ope_template );
	g_free( priv->periodicity );

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
 * @main_window: the #ofaMainWindow main window of the application.
 * @model: the #ofoRecurrentModel to be displayed/updated.
 *
 * Update the properties of a recurrent model.
 */
void
ofa_recurrent_model_properties_run( const ofaMainWindow *main_window, ofoRecurrentModel *model )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_run";
	ofaRecurrentModelProperties *self;
	ofaRecurrentModelPropertiesPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, model=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new( OFA_TYPE_RECURRENT_MODEL_PROPERTIES, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_recurrent_model_properties_get_instance_private( self );
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

	iface->get_identifier = iwindow_get_identifier;
}

/*
 * identifier is built with class name and VAT model mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_recurrent_model_properties_get_instance_private( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_recurrent_model_get_mnemo( priv->recurrent_model ));

	return( id );
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
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * recurrent_model
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_idialog_init";
	ofaRecurrentModelPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_model_properties_get_instance_private( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->is_current = ofo_dossier_is_current( dossier );

	init_title( OFA_RECURRENT_MODEL_PROPERTIES( instance ));
	init_page_properties( OFA_RECURRENT_MODEL_PROPERTIES( instance ));

	my_utils_container_notes_init( GTK_CONTAINER( instance ), recurrent_model );
	my_utils_container_updstamp_init( GTK_CONTAINER( instance ), recurrent_model );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_current );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
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
	GtkWidget *entry, *prompt, *label, *parent;

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
	ofa_ientry_ope_template_init(
			OFA_IENTRY_OPE_TEMPLATE( self ),
			OFA_MAIN_WINDOW( my_iwindow_get_main_window( MY_IWINDOW( self ))),
			GTK_ENTRY( entry ));

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ope-template-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ope-template-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->ope_template_label = label;

	/* periodicity */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-period-parent" );
	g_return_if_fail( entry && GTK_IS_CONTAINER( parent ));
	priv->periodicity_combo = ofa_periodicity_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->periodicity_combo ));
	g_signal_connect( priv->periodicity_combo, "ofa-changed", G_CALLBACK( on_periodicity_changed ), self );

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-period-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), GTK_WIDGET( priv->periodicity_combo ));
}

static void
setup_data( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	priv->mnemo = g_strdup( ofo_recurrent_model_get_label( priv->recurrent_model ));
	if( priv->mnemo ){
		gtk_entry_set_text( GTK_ENTRY( priv->mnemo_entry ), priv->mnemo );
	}

	priv->label = g_strdup( ofo_recurrent_model_get_label( priv->recurrent_model ));
	if( priv->label ){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), priv->label );
	}

	priv->ope_template = g_strdup( ofo_recurrent_model_get_ope_template( priv->recurrent_model ));
	if( priv->ope_template ){
		gtk_entry_set_text( GTK_ENTRY( priv->ope_template_entry ), priv->ope_template );
	}
}

/*
 * ofaIEntryOpeTemplate interface management
 */
static void
ientry_ope_template_iface_init( ofaIEntryOpeTemplateInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_model_properties_ientry_ope_template_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
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

	priv->template_obj = ofo_ope_template_get_by_mnemo( priv->hub, priv->ope_template );
	gtk_label_set_text(
			GTK_LABEL( priv->ope_template_label ),
			priv->template_obj ? ofo_ope_template_get_label( priv->template_obj ) : "" );

	check_for_enable_dlg( self );
}

static void
on_periodicity_changed( ofaPeriodicityCombo *combo, const gchar *code, ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	g_free( priv->periodicity );
	priv->periodicity = g_strdup( code );

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

	if( priv->is_current ){
		ok = is_dialog_validable( self );
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

/*
 * are we able to validate this recurrent model
 */
static gboolean
is_dialog_validable( ofaRecurrentModelProperties *self )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	gboolean ok, exists, subok;
	gchar *msgerr;

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	msgerr = NULL;
	ok = ofo_recurrent_model_is_valid_data( priv->mnemo, priv->label, priv->ope_template, priv->periodicity, &msgerr );

	if( ok && !priv->template_obj ){
		ok = FALSE;
		msgerr = g_strdup_printf( _( "Unknown operation template: %s" ), priv->ope_template );
	}

	if( ok ){
		exists = ( ofo_recurrent_model_get_by_mnemo( priv->hub, priv->mnemo ) != NULL );
		subok = !priv->is_new &&
						!g_utf8_collate( priv->mnemo, ofo_recurrent_model_get_mnemo( priv->recurrent_model ));
		ok = !exists || subok;
		if( !ok ){
			msgerr = g_strdup( _( "Mnemonic is already defined" ));
		}
	}
	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

/*
 * either creating a new recurrent_model (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by its mnemo
 */
static gboolean
do_update( ofaRecurrentModelProperties *self, gchar **msgerr )
{
	ofaRecurrentModelPropertiesPrivate *priv;
	gchar *prev_mnemo;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_recurrent_model_properties_get_instance_private( self );

	prev_mnemo = g_strdup( ofo_recurrent_model_get_mnemo( priv->recurrent_model ));

	ofo_recurrent_model_set_mnemo( priv->recurrent_model, priv->mnemo );
	ofo_recurrent_model_set_label( priv->recurrent_model, priv->label );
	ofo_recurrent_model_set_ope_template( priv->recurrent_model, priv->ope_template );
	ofo_recurrent_model_set_periodicity( priv->recurrent_model, priv->periodicity );
	my_utils_container_notes_get( GTK_WINDOW( self ), recurrent_model );

	if( priv->is_new ){
		ok = ofo_recurrent_model_insert( priv->recurrent_model, priv->hub );
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
		my_utils_widget_set_style( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
