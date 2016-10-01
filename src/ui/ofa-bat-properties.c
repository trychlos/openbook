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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bat-properties-bin.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;

	/* internals
	 */
	ofoBat              *bat;
	gboolean             is_writable;
	gboolean             is_new;		/* always FALSE here */
	ofaBatPropertiesBin *bat_bin;

	/* UI
	 */
	GtkWidget           *ok_btn;
}
	ofaBatPropertiesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-bat-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static gchar    *iwindow_get_identifier( const myIWindow *instance );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      check_for_enable_dlg( ofaBatProperties *self );
static gboolean  is_dialog_validable( ofaBatProperties *self );
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
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_bat_properties_get_instance_private( self );

	priv->getter = getter;
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
	gchar *title, *key;
	GtkWidget *parent;
	ofaBatlineTreeview *line_tview;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_bat_properties_get_instance_private( OFA_BAT_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	title = g_strdup( _( "Updating the BAT properties" ));
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "properties-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->bat_bin = ofa_bat_properties_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->bat_bin ));

	key = g_strdup_printf( "%s.BatLine", G_OBJECT_TYPE_NAME( instance ));
	ofa_bat_properties_bin_set_settings_key( priv->bat_bin, key );
	g_free( key );

	line_tview = ofa_bat_properties_bin_get_batline_treeview( priv->bat_bin );
	ofa_batline_treeview_setup_columns( line_tview );

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
