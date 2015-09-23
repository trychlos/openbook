/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bat-properties-bin.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaBatPropertiesPrivate {

	/* internals
	 */
	ofoBat              *bat;
	gboolean             is_new;		/* always FALSE here */
	gboolean             updated;
	ofaBatPropertiesBin *bat_bin;

	/* data
	 */
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-bat-properties.ui";
static const gchar *st_ui_id            = "BatPropertiesDlg";

G_DEFINE_TYPE( ofaBatProperties, ofa_bat_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      check_for_enable_dlg( ofaBatProperties *self );
static gboolean  is_dialog_validable( ofaBatProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaBatProperties *self );

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
	g_return_if_fail( instance && OFA_IS_BAT_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_parent_class )->dispose( instance );
}

static void
ofa_bat_properties_init( ofaBatProperties *self )
{
	static const gchar *thisfn = "ofa_bat_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_PROPERTIES, ofaBatPropertiesPrivate );
	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
}

static void
ofa_bat_properties_class_init( ofaBatPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_bat_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaBatPropertiesPrivate ));
}

/**
 * ofa_bat_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an bat
 */
gboolean
ofa_bat_properties_run( ofaMainWindow *main_window, ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_properties_run";
	ofaBatProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, bat=%p",
			thisfn, ( void * ) main_window, ( void * ) bat );

	self = g_object_new(
				OFA_TYPE_BAT_PROPERTIES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->bat = bat;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;
	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaBatProperties *self;
	ofaBatPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gchar *title;
	GtkWindow *toplevel;
	GtkWidget *container;
	gboolean is_current;

	main_window = my_window_get_main_window( MY_WINDOW( dialog ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));

	title = g_strdup( _( "Updating the BAT properties" ));
	gtk_window_set_title( toplevel, title );

	self = OFA_BAT_PROPERTIES( dialog );
	priv = self->priv;
	is_current = ofo_dossier_is_current( dossier );

	container = my_utils_container_get_child_by_name(
			GTK_CONTAINER( toplevel ), "containing-frame" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->bat_bin = ofa_bat_properties_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->bat_bin ));
	ofa_bat_properties_bin_set_bat( priv->bat_bin, priv->bat, dossier, is_current );

	check_for_enable_dlg( self );

	/* if not the current exercice, then only have a 'Close' button */
	if( !is_current ){
		my_dialog_set_readonly_buttons( dialog );
	}
}

static void
check_for_enable_dlg( ofaBatProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaBatProperties *self )
{
	return( TRUE );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_BAT_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaBatProperties *self )
{
	ofaBatPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );
	g_return_val_if_fail( !self->priv->is_new, FALSE );

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	priv = self->priv;

	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), bat );

	priv->updated = ofo_bat_update( priv->bat, dossier );

	return( priv->updated );
}
