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
#include <glib/gprintf.h>
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-dossier-edit-bin.h"
#include "ui/ofa-dossier-new.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;
	GtkWindow              *parent;

	/* runtime
	 */
	gchar                  *settings_prefix;
	ofaHub                 *hub;
	gboolean                b_open;

	/* UI
	 */
	ofaDossierEditBin      *edit_bin;
	GtkWidget              *open_btn;
	GtkWidget              *ok_btn;
	GtkWidget              *msg_label;
}
	ofaDossierNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-new.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_edit_bin_changed( ofaDossierEditBin *bin, ofaDossierNew *self );
static void     on_open_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void     check_for_enable_dlg( ofaDossierNew *self );
static gboolean do_create( ofaDossierNew *self, gchar **msgerr );
static gboolean create_confirmed( const ofaDossierNew *self );
static void     set_message( ofaDossierNew *self, const gchar *message );
static void     read_settings( ofaDossierNew *self );
static void     write_settings( ofaDossierNew *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierNew, ofa_dossier_new, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierNew )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_finalize";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	/* free data members here */
	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->finalize( instance );
}

static void
dossier_new_dispose( GObject *instance )
{
	ofaDossierNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_DOSSIER_NEW( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_init( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_init";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW( self ));

	priv = ofa_dossier_new_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_new_class_init( ofaDossierNewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_new_run:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent window.
 *
 * Run the DossierNew dialog.
 */
void
ofa_dossier_new_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_dossier_new_run_with_parent";
	ofaDossierNew *self;
	ofaDossierNewPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p",
			thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_DOSSIER_NEW, NULL );

	priv = ofa_dossier_new_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_iwindow_init";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_geometry_settings( instance, ofa_hub_get_user_settings( priv->hub ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_new_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * the dialog is composed with:
 *
 * - DossierEditBin composite widget
 *   which includes dossier name, provider selection, connection
 *   informations and dbms root credentials
 *
 * - toggle buttons for actions on opening
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_idialog_init";
	ofaDossierNewPrivate *priv;
	GtkWidget *parent, *toggle, *label;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	/* create the composite widget and attach it to the dialog */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "edit-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->edit_bin = ofa_dossier_edit_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_NEW );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->edit_bin ));
	g_signal_connect( priv->edit_bin, "ofa-changed", G_CALLBACK( on_edit_bin_changed ), instance );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-open" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_open_toggled ), instance );
	priv->open_btn = toggle;

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_create );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelerror" );
	priv->msg_label = label;

	read_settings( OFA_DOSSIER_NEW( instance ));

	check_for_enable_dlg( OFA_DOSSIER_NEW( instance ));
}

static void
on_edit_bin_changed( ofaDossierEditBin *bin, ofaDossierNew *self )
{
	check_for_enable_dlg( self );
}

static void
on_open_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	priv->b_open = gtk_toggle_button_get_active( button );
}

/*
 * enable the various dynamic buttons
 *
 * as each check may send an error message which will supersede the
 * previously set, we start from the end so that the user will first
 * see the message at the top of the stack (from the first field in
 * the focus chain)
 */
static void
check_for_enable_dlg( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean enabled;
	gchar *message;

	priv = ofa_dossier_new_get_instance_private( self );

	message = NULL;
	enabled = ofa_dossier_edit_bin_is_valid( priv->edit_bin, &message );
	set_message( self, message );
	g_free( message );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, enabled );
	}
}

static gboolean
do_create( ofaDossierNew *self, gchar **msgerr )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;

	priv = ofa_dossier_new_get_instance_private( self );

	ok = FALSE;

	/* ask for user confirmation */
	if( !create_confirmed( self )){
		return( FALSE );
	}

	if( !ofa_dossier_edit_bin_apply( priv->edit_bin )){
		*msgerr = g_strdup( _( "Unable to create the dossier" ));
		return( FALSE );
	}

	if( priv->b_open ){
#if 0
		dossier_meta = ofa_dossier_edit_bin_get_dossier_meta( priv->edit_bin );
		exercice_meta = ofa_idbdossier_meta_get_current_period( dossier_meta );
		connect = ofa_idbprovider_new_connect( provider );
		if( !ofa_idbconnect_open_with_meta(
				connect, priv->adm_account, priv->adm_password, priv->meta, period )){
			*msgerr = g_strdup( _( "Unable to connect to newly created dossier" ));
			g_clear_object( &connect );
			ok = FALSE;
		}
		g_clear_object( &period );
		g_clear_object( &provider );
	}

	if( ok && priv->b_open ){
		if( ofa_hub_dossier_open( priv->hub, GTK_WINDOW( self ), connect, TRUE, FALSE )){
			ofa_hub_dossier_remediate_settings( priv->hub );

			if( priv->b_properties ){
				main_window = ofa_igetter_get_main_window( priv->getter );
				ofa_main_window_dossier_properties( OFA_MAIN_WINDOW( main_window ));
			}
		} else {
			ok = FALSE;
		}
#endif
	}

	return( ok );
}

static gboolean
create_confirmed( const ofaDossierNew *self )
{
	gboolean ok;
	gchar *str;

	str = g_strdup_printf(
				_( "The create operation will drop and fully reset the target database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to create into this database ?" ));

	ok = my_utils_dialog_question( str, _( "C_reate" ));

	g_free( str );

	return( ok );
}

static void
set_message( ofaDossierNew *self, const gchar *message )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), my_strlen( message ) ? message : "" );
	}
}

/*
 * settings are: "open(b);"
 */
static void
read_settings( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_dossier_new_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->open_btn ), my_utils_boolean_from_str( cstr ));
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_dossier_new_get_instance_private( self );

	str = g_strdup_printf( "%s;",
				priv->b_open ? "True":"False" );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
