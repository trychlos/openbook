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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-class-properties.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* internals
	 */
	ofoClass   *class;
	gboolean    is_writable;
	gboolean    is_new;

	/* data
	 */
	gint        number;
	gchar      *label;

	/* UI
	 */
	GtkWidget  *ok_btn;
	GtkWidget  *msg_label;
}
	ofaClassPropertiesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-class-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static gchar    *iwindow_get_identifier( const myIWindow *instance );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_number_changed( GtkEntry *entry, ofaClassProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaClassProperties *self );
static void      check_for_enable_dlg( ofaClassProperties *self );
static gboolean  is_dialog_validable( ofaClassProperties *self );
static gboolean  do_update( ofaClassProperties *self, gchar **msgerr );
static void      set_msgerr( ofaClassProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaClassProperties, ofa_class_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaClassProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
class_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_class_properties_finalize";
	ofaClassPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLASS_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_class_properties_get_instance_private( OFA_CLASS_PROPERTIES( instance ));

	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_properties_parent_class )->finalize( instance );
}

static void
class_properties_dispose( GObject *instance )
{
	ofaClassPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLASS_PROPERTIES( instance ));

	priv = ofa_class_properties_get_instance_private( OFA_CLASS_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_properties_parent_class )->dispose( instance );
}

static void
ofa_class_properties_init( ofaClassProperties *self )
{
	static const gchar *thisfn = "ofa_class_properties_init";
	ofaClassPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CLASS_PROPERTIES( self ));

	priv = ofa_class_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_class_properties_class_init( ofaClassPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_class_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_class_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @class: the #ofoClass to be displayed/updated.
 *
 * Update the properties of an class
 */
void
ofa_class_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoClass *class )
{
	static const gchar *thisfn = "ofa_class_properties_run";
	ofaClassProperties *self;
	ofaClassPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, class=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) class );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_CLASS_PROPERTIES, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_class_properties_get_instance_private( self );

	priv->getter = getter;
	priv->class = class;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_class_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_identifier = iwindow_get_identifier;
}

/*
 * identifier is built with class name and template mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaClassPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_class_properties_get_instance_private( OFA_CLASS_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%d",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_class_get_number( priv->class ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_class_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * account class
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_class_properties_idialog_init";
	ofaClassPropertiesPrivate *priv;
	ofaHub *hub;
	gchar *title;
	gint number;
	GtkEntry *entry;
	gchar *str;
	GtkWidget *label;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_class_properties_get_instance_private( OFA_CLASS_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	number = ofo_class_get_number( priv->class );
	if( number < 1 ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new class" ));
	} else {
		title = g_strdup_printf( _( "Updating class « %d »" ), number );
	}
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	/* class number */
	priv->number = number;
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-number" ));
	if( priv->is_new ){
		str = g_strdup( "" );
	} else {
		str = g_strdup_printf( "%d", priv->number );
	}
	gtk_entry_set_text( entry, str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_number_changed ), instance );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-class-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	/* class label */
	priv->label = g_strdup( ofo_class_get_label( priv->class ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( instance ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), instance );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( instance, class );
	my_utils_container_updstamp_init( instance, class );
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_CLASS_PROPERTIES( instance ));
}

static void
on_number_changed( GtkEntry *entry, ofaClassProperties *self )
{
	ofaClassPropertiesPrivate *priv;

	priv = ofa_class_properties_get_instance_private( self );

	priv->number = atoi( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaClassProperties *self )
{
	ofaClassPropertiesPrivate *priv;

	priv = ofa_class_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaClassProperties *self )
{
	ofaClassPropertiesPrivate *priv;

	priv = ofa_class_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

static gboolean
is_dialog_validable( ofaClassProperties *self )
{
	ofaClassPropertiesPrivate *priv;
	gboolean ok;
	ofoClass *exists;
	ofaHub *hub;
	gchar *msgerr;

	priv = ofa_class_properties_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	ok = ofo_class_is_valid_data( priv->number, priv->label, &msgerr );
	if( ok ){
		exists = ofo_class_get_by_number( hub, priv->number );
		ok &= !exists ||
				( ofo_class_get_number( exists ) == ofo_class_get_number( priv->class ));
		if( !ok ){
			msgerr = g_strdup( _( "Account class already exists" ));
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static gboolean
do_update( ofaClassProperties *self, gchar **msgerr )
{
	ofaClassPropertiesPrivate *priv;
	gint prev_id;
	gboolean ok;
	ofaHub *hub;

	priv = ofa_class_properties_get_instance_private( self );

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	hub = ofa_igetter_get_hub( priv->getter );

	prev_id = ofo_class_get_number( priv->class );

	ofo_class_set_number( priv->class, priv->number );
	ofo_class_set_label( priv->class, priv->label );
	my_utils_container_notes_get( self, class );

	if( priv->is_new ){
		ok = ofo_class_insert( priv->class, hub );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new account class" ));
		}
	} else {
		ok = ofo_class_update( priv->class, prev_id );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the account class" ));
		}
	}

	return( ok );
}

static void
set_msgerr( ofaClassProperties *self, const gchar *msg )
{
	ofaClassPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_class_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
