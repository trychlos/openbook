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
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-admin-credentials-bin.h"

#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewPrivate {

	/* UI
	 */
	ofaDossierNewBin       *new_bin;
	ofaAdminCredentialsBin *admin_credentials;
	GtkWidget              *properties_toggle;
	GtkWidget              *ok_btn;
	GtkWidget              *msg_label;

	/* runtime data
	 */
	gchar                  *dname;
	gchar                  *database;
	gchar                  *root_account;
	gchar                  *root_password;
	gchar                  *adm_account;
	gchar                  *adm_password;
	gboolean                b_open;
	gboolean                b_properties;

	gchar                  *prov_name;

	/* result
	 */
	gboolean       dossier_created;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-new.ui";
static const gchar *st_ui_id            = "DossierNewDlg";

G_DEFINE_TYPE( ofaDossierNew, ofa_dossier_new, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, void *infos, const gchar *account, const gchar *password, ofaDossierNew *self );
static void      on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaDossierNew *self );
static void      on_open_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      on_properties_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      get_settings( ofaDossierNew *self );
static void      update_settings( ofaDossierNew *self );
static void      check_for_enable_dlg( ofaDossierNew *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  create_confirmed( const ofaDossierNew *self );

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

	g_free( priv->dname );
	g_free( priv->database );
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
	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
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
	ofsDossierOpen *sdo;
	gboolean dossier_opened;

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
			sdo = g_new0( ofsDossierOpen, 1 );
			sdo->dname = g_strdup( priv->dname );
			sdo->dbname = g_strdup( priv->database );
			sdo->account = g_strdup( priv->adm_account );
			sdo->password = g_strdup( priv->adm_password );
		}
	}

	g_object_unref( self );

	if( dossier_created ){
		if( open_dossier ){
			g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_DOSSIER_OPEN, sdo );
			if( open_properties ){
				g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_DOSSIER_PROPERTIES );
			}
			dossier_opened = TRUE;
		}
	}

	return( dossier_opened );
}

/*
 * the provided 'page' is the toplevel widget of the asistant's page
 */
static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierNewPrivate *priv;
	GtkWindow *toplevel;
	GtkSizeGroup *group;
	GtkWidget *parent, *toggle;

	priv = OFA_DOSSIER_NEW( dialog )->priv;
	get_settings( OFA_DOSSIER_NEW( dialog ));

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "new-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->new_bin = ofa_dossier_new_bin_new();
	ofa_dossier_new_bin_attach_to( priv->new_bin, GTK_CONTAINER( parent ), group );
	g_object_unref( group );
	ofa_dossier_new_bin_set_frame( priv->new_bin, TRUE );
	if( my_strlen( priv->prov_name )){
		ofa_dossier_new_bin_set_provider( priv->new_bin, priv->prov_name );
	}

	g_signal_connect( priv->new_bin, "changed", G_CALLBACK( on_new_bin_changed ), dialog );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "admin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->admin_credentials = ofa_admin_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->admin_credentials ));

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

	check_for_enable_dlg( OFA_DOSSIER_NEW( dialog ));
}

static void
on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, void *infos, const gchar *account, const gchar *password, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->priv;

	g_free( priv->dname );
	priv->dname = g_strdup( dname );

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
	static const gchar *thisfn = "ofa_dossier_new_check_for_enable_dlg";
	ofaDossierNewPrivate *priv;
	gboolean enabled;
	gboolean oka, okb;

	priv = self->priv;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	oka = ofa_dossier_new_bin_is_valid( priv->new_bin );
	okb = ofa_admin_credentials_bin_is_valid( priv->admin_credentials, NULL );

	enabled = oka && okb;

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, enabled );
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;
	ofaIDbms *prov_instance;

	priv = OFA_DOSSIER_NEW( dialog )->priv;

	/* get the database name */
	g_free( priv->database );
	ofa_dossier_new_bin_get_database( priv->new_bin, &priv->database );
	g_return_val_if_fail( my_strlen( priv->database ), FALSE );

	/* ask for user confirmation */
	if( !create_confirmed( OFA_DOSSIER_NEW( dialog ))){
		return( FALSE );
	}

	/* define the new dossier in user settings */
	ok = ofa_dossier_new_bin_apply( priv->new_bin );

	if( ok ){
		/* get the provider name (from user settings) */
		g_free( priv->prov_name );
		priv->prov_name = ofa_settings_get_dossier_provider( priv->dname );
		g_return_val_if_fail( my_strlen( priv->prov_name ), FALSE );

		/* and a new ref on the DBMS provider module */
		prov_instance = ofa_idbms_get_provider_by_name( priv->prov_name );
		g_return_val_if_fail( prov_instance && OFA_IS_IDBMS( prov_instance ), FALSE );

		/* create the database itself */
		ok = ofa_idbms_new_dossier(
				prov_instance, priv->dname,  priv->root_account, priv->root_password );
	}

	if( ok ){
		/* last, grant administrative credentials */
		ok = ofa_idbms_set_admin_credentials(
				prov_instance, priv->dname, priv->root_account, priv->root_password,
				priv->adm_account, priv->adm_password );
	}

	g_clear_object( &prov_instance );
	priv->dossier_created = ok;
	update_settings( OFA_DOSSIER_NEW( dialog ));

	return( ok );
}

static gboolean
create_confirmed( const ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	GtkWidget *dialog;
	gchar *str;
	gint response;

	priv = self->priv;

	str = g_strdup_printf(
				_( "The create operation will drop and fully reset the '%s' target database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to create into this database ?" ), priv->database );

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", str );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "C_reate" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	g_free( str );
	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
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
