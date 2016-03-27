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
#include <glib/gprintf.h>
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-file-dir.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-admin-credentials-bin.h"
#include "core/ofa-dbms-root-bin.h"

#include "ui/ofa-application.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;

	/* UI
	 */
	ofaDossierNewBin       *new_bin;
	ofaDBMSRootBin         *root_credentials;
	ofaAdminCredentialsBin *admin_credentials;
	GtkWidget              *properties_toggle;
	GtkWidget              *ok_btn;
	GtkWidget              *msg_label;

	/* runtime data
	 */
	gchar                  *dossier_name;
	gchar                  *root_account;
	gchar                  *root_password;
	gchar                  *adm_account;
	gchar                  *adm_password;
	gboolean                b_open;
	gboolean                b_properties;

	/* settings
	 */
	gchar                  *prov_name;

	/* result
	 */
	ofaIDBMeta             *meta;
}
	ofaDossierNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-new.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, ofaIDBEditor *editor, ofaDossierNew *self );
static void      on_root_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self );
static void      on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self );
static void      on_open_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      on_properties_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      get_settings( ofaDossierNew *self );
static void      update_settings( ofaDossierNew *self );
static void      check_for_enable_dlg( ofaDossierNew *self );
static gboolean  root_credentials_get_valid( ofaDossierNew *self, gchar **message );
static gboolean  do_create( ofaDossierNew *self, gchar **msgerr );
static gboolean  create_confirmed( const ofaDossierNew *self );
static void      set_message( ofaDossierNew *self, const gchar *message );

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

	g_free( priv->dossier_name );
	g_free( priv->root_account );
	g_free( priv->root_password );
	g_free( priv->adm_account );
	g_free( priv->adm_password );
	g_free( priv->prov_name );

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

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->meta );
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
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_dossier_new_get_instance_private( self );

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
	static const gchar *thisfn = "ofa_dossier_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
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
 * - DossierNewBin composite widget
 *   which includes dossier name, provider selection, connection
 *   informations and dbms root credentials
 *
 * - AdminCredentialsBin composite widget
 *   which includes administrative credentials
 *
 * - toggle buttons for actions on opening
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_idialog_init";
	ofaDossierNewPrivate *priv;
	GtkSizeGroup *group0, *group1;
	GtkWidget *parent, *toggle, *label;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_new_get_instance_private( OFA_DOSSIER_NEW( instance ));

	get_settings( OFA_DOSSIER_NEW( instance ));

	/* define the size groups */
	group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	group1 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* create the composite widget and attach it to the dialog */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "new-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->new_bin = ofa_dossier_new_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->new_bin ));
	my_utils_size_group_add_size_group(
			group0, ofa_dossier_new_bin_get_size_group( priv->new_bin, 0 ));
	g_object_unref( group0 );

	g_signal_connect( priv->new_bin, "ofa-changed", G_CALLBACK( on_new_bin_changed ), instance );

	/* dbms root credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "root-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->root_credentials = ofa_dbms_root_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->root_credentials ));
	my_utils_size_group_add_size_group(
			group0, ofa_dbms_root_bin_get_size_group( priv->root_credentials, 0 ));

	g_signal_connect( priv->root_credentials, "ofa-changed", G_CALLBACK( on_root_credentials_changed ), instance );

	/* admin credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "admin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->admin_credentials = ofa_admin_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->admin_credentials ));
	my_utils_size_group_add_size_group(
			group1, ofa_admin_credentials_bin_get_size_group( priv->admin_credentials, 0 ));
	g_object_unref( group1 );

	g_signal_connect( priv->admin_credentials,
			"ofa-changed", G_CALLBACK( on_admin_credentials_changed ), instance );

	/* set properties_toggle before setting open value
	 * pwi 2015-12-18: no more try to open properties right after dossier
	 * creation (this will nonetheless be proposed if exercice is not set) */
	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-properties" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_properties_toggled ), instance );
	priv->b_properties = FALSE;
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), priv->b_properties );
	gtk_widget_set_sensitive( toggle, FALSE );
	priv->properties_toggle = toggle;

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-open" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_open_toggled ), instance );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), priv->b_open );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_create );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelerror" );
	priv->msg_label = label;

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_DOSSIER_NEW( instance ));
}

static void
on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, ofaIDBEditor *editor, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	g_free( priv->dossier_name );
	priv->dossier_name = g_strdup( dname );

	check_for_enable_dlg( self );
}

static void
on_root_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	g_debug( "on_root_credentials_changed: account=%s, password=%s", account, password );

	g_free( priv->root_account );
	priv->root_account = g_strdup( account );

	g_free( priv->root_password );
	priv->root_password = g_strdup( password );

	check_for_enable_dlg( self );
}

static void
on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	g_free( priv->adm_account );
	priv->adm_account = g_strdup( account );

	g_free( priv->adm_password );
	priv->adm_password = g_strdup( password );

	check_for_enable_dlg( self );
}

static void
on_open_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	priv->b_open = gtk_toggle_button_get_active( button );

#if 0
	if( priv->properties_toggle ){
		gtk_widget_set_sensitive( priv->properties_toggle, priv->b_open );
	}
#endif
}

static void
on_properties_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = ofa_dossier_new_get_instance_private( self );

	priv->b_properties = gtk_toggle_button_get_active( button );
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

	enabled = ofa_dossier_new_bin_get_valid( priv->new_bin, &message ) &&
					root_credentials_get_valid( self, &message ) &&
					ofa_admin_credentials_bin_is_valid( priv->admin_credentials, &message );

	set_message( self, message );
	g_free( message );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, enabled );
	}
}

/*
 * test if root credentials are valid by opening a connection
 * with editor informations (because ofa_dbms_root_bin_get_valid()
 * can only be called on dossiers which are already defined in the
 * settings)
 */
static gboolean
root_credentials_get_valid( ofaDossierNew *self, gchar **message )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;
	ofaIDBEditor *editor;
	ofaIDBProvider *provider;
	ofaIDBConnect *connect;

	priv = ofa_dossier_new_get_instance_private( self );

	/* check for DBMS root credentials in all cases so that the DBMS
	 * status message is erased when credentials are no longer valid
	 * (and even if another error message is displayed) */
	ok = FALSE;
	if( my_strlen( priv->root_account )){
		editor = ofa_dossier_new_bin_get_editor( priv->new_bin );
		provider = ofa_idbeditor_get_provider( editor );
		connect = ofa_idbprovider_new_connect( provider );
		ok = ofa_idbconnect_open_with_editor(
							connect, priv->root_account, priv->root_password, editor, TRUE );
		g_clear_object( &connect );
		g_clear_object( &provider );
	}
	ofa_dbms_root_bin_set_valid( priv->root_credentials, ok );
	if( !ok ){
		g_free( *message );
		*message = g_strdup( _( "DBMS root credentials are not valid" ));
	}

	return( ok );
}

/*
 * this function will return FALSE in all cases
 * only the ofaFileDir::changed signal handler will terminate the dialog
 */
static gboolean
do_create( ofaDossierNew *self, gchar **msgerr )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;
	ofaIDBPeriod *period;
	ofaIDBProvider *provider;
	ofaIDBConnect *connect;
	gchar *account, *password;
	ofaIDBEditor *editor;
	ofaHub *hub;
	GtkApplicationWindow *main_window;

	priv = ofa_dossier_new_get_instance_private( self );

	ok = FALSE;

	/* ask for user confirmation */
	if( !create_confirmed( self )){
		return( FALSE );
	}

	/* define the new dossier in user settings */
	priv->meta = ofa_dossier_new_bin_apply( priv->new_bin );

	if( priv->meta ){
		provider = ofa_idbmeta_get_provider( priv->meta );
		period = ofa_idbmeta_get_current_period( priv->meta );
		ofa_dbms_root_bin_get_credentials( priv->root_credentials, &account, &password );
		connect = ofa_idbprovider_new_connect( provider );
		editor = ofa_dossier_new_bin_get_editor( priv->new_bin );

		if( !ofa_idbconnect_open_with_editor(
							connect, account, password, editor, TRUE )){
			*msgerr = g_strdup( _( "Unable to open the connection with editor informations" ));

		} else if( !ofa_idbconnect_create_dossier(
							connect, priv->meta, priv->adm_account, priv->adm_password )){
			*msgerr = g_strdup( _( "Unable to create the dossier" ));

		} else {
			ok = TRUE;
		}

		g_free( account );
		g_free( password );
		g_clear_object( &period );
		g_clear_object( &connect );
		g_clear_object( &provider );
	}

	if( !ok ){
		ofa_idbmeta_remove_meta( priv->meta );
		g_clear_object( &priv->meta );
		gtk_widget_set_sensitive( priv->ok_btn, FALSE );
	}

	update_settings( self );
	connect = NULL;

	if( ok && priv->b_open ){
		provider = ofa_idbmeta_get_provider( priv->meta );
		period = ofa_idbmeta_get_current_period( priv->meta );
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
		hub = ofa_igetter_get_hub( priv->getter );
		g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

		if( ofa_hub_dossier_open( hub, connect, GTK_WINDOW( self ))){
			ofa_hub_remediate_settings( hub );

			if( priv->b_properties ){
				main_window = ofa_igetter_get_main_window( priv->getter );
				g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_DOSSIER_PROPERTIES );
			}
		} else {
			ok = FALSE;
			*msgerr = g_strdup( _( "Unable to create the dossier" ));
		}
	}

	g_clear_object( &connect );

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

/*
 * settings are: "provider_name;open,properties;"
 */
static void
get_settings( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_dossier_new_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( "DossierNew" );
	it = slist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->prov_name );
		priv->prov_name = g_strdup( cstr );
	}
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->b_open = my_utils_boolean_from_str( cstr );
	}
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->b_properties = my_utils_boolean_from_str( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
update_settings( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	GList *slist;

	priv = ofa_dossier_new_get_instance_private( self );

	slist = g_list_append( NULL, g_strdup( priv->prov_name ));
	slist = g_list_append( slist, g_strdup_printf( "%s", priv->b_open ? "True":"False" ));
	slist = g_list_append( slist, g_strdup_printf( "%s", priv->b_properties ? "True":"False" ));

	ofa_settings_user_set_string_list( "DossierNew", slist );

	g_list_free_full( slist, ( GDestroyNotify ) g_free );
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
