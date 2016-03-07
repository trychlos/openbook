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

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bat-properties-bin.h"

/* private instance data
 */
struct _ofaBatPropertiesPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */

	/* internals
	 */
	ofoBat              *bat;
	ofaHub              *hub;
	gboolean             is_current;
	gboolean             is_new;		/* always FALSE here */
	ofaBatPropertiesBin *bat_bin;

	/* UI
	 */
	GtkWidget           *ok_btn;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-bat-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static gchar    *iwindow_get_identifier( const myIWindow *instance );
static void      iwindow_init( myIWindow *instance );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      check_for_enable_dlg( ofaBatProperties *self );
static gboolean  is_dialog_validable( ofaBatProperties *self );
static void      on_ok_clicked( GtkButton *button, ofaBatProperties *self );
static gboolean  do_update( ofaBatProperties *self, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( ofaBatProperties, ofa_bat_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaBatProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
bat_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_PROPERTIES( instance ));

	/* free data members here */

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
 * @main_window: the main window of the application.
 * @bat: the #ofoBat to be displayed.
 *
 * Display the properties of a bat file.
 * Let update the notes if the dossier is not an archive.
 */
void
ofa_bat_properties_run( const ofaMainWindow *main_window, ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_properties_run";
	ofaBatProperties *self;
	ofaBatPropertiesPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, bat=%p",
			thisfn, ( void * ) main_window, ( void * ) bat );

	self = g_object_new( OFA_TYPE_BAT_PROPERTIES, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_bat_properties_get_instance_private( self );
	priv->bat = bat;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_identifier = iwindow_get_identifier;
	iface->init = iwindow_init;
}

/*
 * identifier is built with class name and template mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaBatPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%ld",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_bat_get_id( priv->bat ));

	return( id );
}

/*
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * account
 */
static void
iwindow_init( myIWindow *instance )
{
	ofaBatPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gchar *title;
	GtkWidget *parent;

	my_idialog_init_dialog( MY_IDIALOG( instance ));

	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	g_signal_connect( priv->ok_btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );

	main_window = my_iwindow_get_main_window( instance );
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->is_current = ofo_dossier_is_current( dossier );

	title = g_strdup( _( "Updating the BAT properties" ));
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "properties-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->bat_bin = ofa_bat_properties_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bat_bin ));
	ofa_bat_properties_bin_set_bat( priv->bat_bin, priv->bat );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		my_idialog_set_close_button( MY_IDIALOG( instance ));
		priv->ok_btn = NULL;
	}

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_BAT_PROPERTIES( instance ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

static void
check_for_enable_dlg( ofaBatProperties *self )
{
	ofaBatPropertiesPrivate *priv;

	priv = ofa_bat_properties_get_instance_private( self );

	if( priv->is_current ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

static gboolean
is_dialog_validable( ofaBatProperties *self )
{
	return( TRUE );
}

static void
on_ok_clicked( GtkButton *button, ofaBatProperties *self )
{
	gboolean ok;
	gchar *msgerr;

	msgerr = NULL;
	ok = do_update( self, &msgerr );

	if( ok ){
		my_iwindow_close( MY_IWINDOW( self ));

	} else {
		my_utils_dialog_warning( msgerr );
		g_free( msgerr );
	}
}

static gboolean
do_update( ofaBatProperties *self, gchar **msgerr )
{
	ofaBatPropertiesPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_bat_properties_get_instance_private( self );

	g_return_val_if_fail( !priv->is_new, FALSE );

	my_utils_container_notes_get( GTK_WINDOW( self ), bat );

	ok = ofo_bat_update( priv->bat );
	if( !ok ){
		*msgerr = g_strdup( _( "Unable to update this BAT record" ));
	}

	return( ok );
}
