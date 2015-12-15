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
#include <glib/gprintf.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta.h"
#include "api/ofo-dossier.h"

#include "core/ofa-admin-credentials-bin.h"
#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-file-dir.h"
#include "core/ofa-settings.h"

#include "ui/ofa-application.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewPrivate {

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
	gboolean                dossier_created;
	ofaIDBMeta             *meta;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-new.ui";
static const gchar *st_ui_id            = "DossierNewDlg";

G_DEFINE_TYPE( ofaDossierNew, ofa_dossier_new, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, ofaIDBEditor *editor, ofaDossierNew *self );
static void      on_root_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self );
static void      on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self );
static void      on_open_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      on_properties_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      get_settings( ofaDossierNew *self );
static void      update_settings( ofaDossierNew *self );
static void      check_for_enable_dlg( ofaDossierNew *self );
static gboolean  root_credentials_get_valid( ofaDossierNew *self, gchar **message );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  create_confirmed( const ofaDossierNew *self );
static void      set_message( ofaDossierNew *self, const gchar *message );

static void
dossier_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_finalize";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_NEW( instance )->priv;

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

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_DOSSIER_NEW( instance )->priv;

		g_clear_object( &priv->meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_init( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_NEW, ofaDossierNewPrivate );
	self->priv->dossier_created = FALSE;
}

static void
ofa_dossier_new_class_init( ofaDossierNewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierNewPrivate ));
}

/**
 * ofa_dossier_new_run:
 * @main: the main window of the application.
 *
 * Returns: %TRUE if a dossier has been created, and opened in the UI.
 */
gboolean
ofa_dossier_new_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_new_run";
	ofaDossierNew *self;
	ofaDossierNewPrivate *priv;
	gboolean dossier_created, open_dossier, open_properties;
	gboolean dossier_opened;
	ofaIDBProvider *provider;
	ofaIDBPeriod *period;
	ofaIDBConnect *connect;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	priv = self->priv;
	dossier_opened = FALSE;
	dossier_created = priv->dossier_created;
	open_dossier = priv->b_open;
	open_properties = priv->b_properties;

	if( dossier_created ){
		if( open_dossier ){
			provider = ofa_idbmeta_get_provider( priv->meta );
			period = ofa_idbmeta_get_current_period( priv->meta );
			connect = ofa_idbprovider_new_connect( provider );
			if( !ofa_idbconnect_open_with_meta(
					connect, priv->adm_account, priv->adm_password, priv->meta, period )){
				g_warning( "%s: unable to connect to newly created dossier", thisfn );
				g_clear_object( &connect );
				open_dossier = FALSE;
			}
			g_clear_object( &period );
			g_clear_object( &provider );
		}
	}

	g_object_unref( self );

	if( dossier_created ){
		if( open_dossier ){
			g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_DOSSIER_OPEN, connect, TRUE );
			if( open_properties ){
				g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_DOSSIER_PROPERTIES );
			}
			dossier_opened = TRUE;
		}
	}

	return( dossier_opened );
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
v_init_dialog( myDialog *dialog )
{
	ofaDossierNewPrivate *priv;
	GtkWindow *toplevel;
	GtkSizeGroup *group0, *group1;
	GtkWidget *parent, *toggle, *label;

	priv = OFA_DOSSIER_NEW( dialog )->priv;
	get_settings( OFA_DOSSIER_NEW( dialog ));

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	/* define the size groups */
	group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	group1 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* create the composite widget and attach it to the dialog */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "new-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->new_bin = ofa_dossier_new_bin_new(
							OFA_MAIN_WINDOW( my_window_get_main_window( MY_WINDOW( dialog ))));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->new_bin ));
	my_utils_size_group_add_size_group(
			group0, ofa_dossier_new_bin_get_size_group( priv->new_bin, 0 ));
	g_object_unref( group0 );

	g_signal_connect( priv->new_bin, "ofa-changed", G_CALLBACK( on_new_bin_changed ), dialog );

	/* dbms root credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "root-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->root_credentials = ofa_dbms_root_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->root_credentials ));
	my_utils_size_group_add_size_group(
			group0, ofa_dbms_root_bin_get_size_group( priv->root_credentials, 0 ));

	g_signal_connect( priv->root_credentials, "ofa-changed", G_CALLBACK( on_root_credentials_changed ), dialog );

	/* admin credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "admin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->admin_credentials = ofa_admin_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->admin_credentials ));
	my_utils_size_group_add_size_group(
			group1, ofa_admin_credentials_bin_get_size_group( priv->admin_credentials, 0 ));
	g_object_unref( group1 );

	g_signal_connect( priv->admin_credentials,
			"ofa-changed", G_CALLBACK( on_admin_credentials_changed ), dialog );

	/* set properties_toggle before setting open value */
	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dn-properties" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_properties_toggled ), dialog );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), priv->b_properties );
	priv->properties_toggle = toggle;

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dn-open" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_open_toggled ), dialog );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), priv->b_open );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "dn-msg" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelerror" );
	priv->msg_label = label;

	check_for_enable_dlg( OFA_DOSSIER_NEW( dialog ));
}

static void
on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, ofaIDBEditor *editor, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->priv;

	g_free( priv->dossier_name );
	priv->dossier_name = g_strdup( dname );

	check_for_enable_dlg( self );
}

static void
on_root_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->priv;
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

	priv = self->priv;

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

	priv = self->priv;

	priv->b_open = gtk_toggle_button_get_active( button );

	if( priv->properties_toggle ){
		gtk_widget_set_sensitive( priv->properties_toggle, priv->b_open );
	}
}

static void
on_properties_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;
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

	priv = self->priv;
	g_debug( "root_credentials_get_valid" );

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
v_quit_on_ok( myDialog *dialog )
{
	static const gchar *thisfn = "ofa_dossier_new_v_quit_on_ok";
	ofaDossierNewPrivate *priv;
	gboolean ok;
	ofaIDBPeriod *period;
	ofaIDBProvider *provider;
	ofaIDBConnect *connect;
	gchar *account, *password;
	ofaIDBEditor *editor;

	priv = OFA_DOSSIER_NEW( dialog )->priv;
	ok = FALSE;

	/* ask for user confirmation */
	if( !create_confirmed( OFA_DOSSIER_NEW( dialog ))){
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
			g_warning( "%s: unable to open the connection with editor informations", thisfn );
			g_clear_object( &priv->meta );

		} else if( !ofa_idbconnect_create_dossier(
							connect, priv->meta, priv->adm_account, priv->adm_password )){
			g_warning( "%s: unable to create the dossier", thisfn );
			g_clear_object( &priv->meta );

		} else {
			ok = TRUE;
		}

		g_free( account );
		g_free( password );
		g_clear_object( &period );
		g_clear_object( &connect );
		g_clear_object( &provider );
	}

	priv->dossier_created = ok;
	update_settings( OFA_DOSSIER_NEW( dialog ));

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

	priv = self->priv;

	slist = ofa_settings_get_string_list( "DossierNew" );
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

	priv = self->priv;

	slist = g_list_append( NULL, g_strdup( priv->prov_name ));
	slist = g_list_append( slist, g_strdup_printf( "%s", priv->b_open ? "True":"False" ));
	slist = g_list_append( slist, g_strdup_printf( "%s", priv->b_properties ? "True":"False" ));

	ofa_settings_set_string_list( "DossierNew", slist );

	g_list_free_full( slist, ( GDestroyNotify ) g_free );
}

static void
set_message( ofaDossierNew *self, const gchar *message )
{
	ofaDossierNewPrivate *priv;

	priv = self->priv;

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), my_strlen( message ) ? message : "" );
	}
}
