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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "core/ofa-bat-properties.h"
#include "core/ofa-bat-properties-bin.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;
	ofoBat              *bat;

	/* runtime
	 */
	gchar               *settings_prefix;
	GtkWindow           *actual_parent;
	gboolean             is_writable;
	gboolean             is_new;			/* always FALSE here */
	ofaBatPropertiesBin *bat_bin;

	/* UI
	 */
	GtkWidget           *ok_btn;
}
	ofaBatPropertiesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-bat-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     check_for_enable_dlg( ofaBatProperties *self );
static gboolean is_dialog_validable( ofaBatProperties *self );
static void     on_ok_clicked( ofaBatProperties *self );
static gboolean do_update( ofaBatProperties *self, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( ofaBatProperties, ofa_bat_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaBatProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
bat_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_properties_finalize";
	ofaBatPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_parent_class )->finalize( instance );
}

static void
bat_properties_dispose( GObject *instance )
{
	ofaBatPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_PROPERTIES( instance ));

	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_parent_class )->dispose( instance );
}

static void
ofa_bat_properties_init( ofaBatProperties *self )
{
	static const gchar *thisfn = "ofa_bat_properties_init";
	ofaBatPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_PROPERTIES( self ));

	priv = ofa_bat_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_bat_properties_class_init( ofaBatPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_bat_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_bat_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @bat: the #ofoBat to be displayed.
 *
 * Display the properties of a bat file.
 * Let update the notes if the dossier is not an archive.
 */
void
ofa_bat_properties_run( ofaIGetter *getter, GtkWindow *parent , ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_properties_run";
	ofaBatProperties *self;
	ofaBatPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, bat=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) bat );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	self = g_object_new( OFA_TYPE_BAT_PROPERTIES, NULL );

	priv = ofa_bat_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->bat = bat;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_bat_properties_iwindow_init";
	ofaBatPropertiesPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%ld",
				G_OBJECT_TYPE_NAME( instance ), ofo_bat_get_id( priv->bat ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * BAT record
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_bat_properties_idialog_init";
	ofaBatPropertiesPrivate *priv;
	ofaHub *hub;
	gchar *title;
	GtkWidget *parent, *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	title = g_strdup( _( "Displaying BAT properties" ));
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "properties-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->bat_bin = ofa_bat_properties_bin_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bat_bin ));

	ofa_bat_properties_bin_set_bat( priv->bat_bin, priv->bat );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_BAT_PROPERTIES( instance ));
}

static void
check_for_enable_dlg( ofaBatProperties *self )
{
	ofaBatPropertiesPrivate *priv;

	priv = ofa_bat_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

static gboolean
is_dialog_validable( ofaBatProperties *self )
{
	return( TRUE );
}

static void
on_ok_clicked( ofaBatProperties *self )
{
	gchar *msgerr = NULL;

	do_update( self, &msgerr );

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

/*
 * Only notes are updatable here
 */
static gboolean
do_update( ofaBatProperties *self, gchar **msgerr )
{
	ofaBatPropertiesPrivate *priv;
	gboolean ok;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *notes;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_bat_properties_get_instance_private( self );

	g_return_val_if_fail( !priv->is_new, FALSE );

	//my_utils_container_notes_get( GTK_WINDOW( self ), bat );

	text = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "pn-notes" );
	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( text ));
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );

	ok = ofo_bat_update_notes( priv->bat, notes );
	if( !ok ){
		*msgerr = g_strdup( _( "Unable to update this BAT record" ));
	}

	g_free( notes );

	return( ok );
}
