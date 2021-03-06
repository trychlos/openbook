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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
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
	GtkWindow  *parent;
	ofoClass   *class;

	/* runtime
	 */
	GtkWindow  *actual_parent;
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

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_number_changed( GtkEntry *entry, ofaClassProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaClassProperties *self );
static void     check_for_enable_dlg( ofaClassProperties *self );
static gboolean is_dialog_validable( ofaClassProperties *self );
static void     on_ok_clicked( ofaClassProperties *self );
static gboolean do_update( ofaClassProperties *self, gchar **msgerr );
static void     set_msgerr( ofaClassProperties *self, const gchar *msg );

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

	priv = ofa_class_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->class = class;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_class_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_class_properties_iwindow_init";
	ofaClassPropertiesPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_class_properties_get_instance_private( OFA_CLASS_PROPERTIES( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%d",
				G_OBJECT_TYPE_NAME( instance ), ofo_class_get_number( priv->class ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
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
	GtkWidget *label, *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_class_properties_get_instance_private( OFA_CLASS_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

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
	g_signal_connect( entry, "changed", G_CALLBACK( on_number_changed ), instance );
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
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), instance );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( instance, class );
	my_utils_container_crestamp_init( instance, class );
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
	gchar *msgerr;

	priv = ofa_class_properties_get_instance_private( self );

	ok = ofo_class_is_valid_data( priv->number, priv->label, &msgerr );
	if( ok ){
		exists = ofo_class_get_by_number( priv->getter, priv->number );
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

static void
on_ok_clicked( ofaClassProperties *self )
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
do_update( ofaClassProperties *self, gchar **msgerr )
{
	ofaClassPropertiesPrivate *priv;
	gint prev_id;
	gboolean ok;

	priv = ofa_class_properties_get_instance_private( self );

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	prev_id = ofo_class_get_number( priv->class );

	ofo_class_set_number( priv->class, priv->number );
	ofo_class_set_label( priv->class, priv->label );
	my_utils_container_notes_get( self, class );

	if( priv->is_new ){
		ok = ofo_class_insert( priv->class );
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
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
