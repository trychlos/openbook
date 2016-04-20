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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-exercice-combo.h"
#include "ui/ofa-exercice-store.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-user-credentials-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;

	/* data
	 */
	ofaIDBMeta         *meta;			/* the selected dossier */
	ofaIDBPeriod       *period;			/* the selected exercice */
	gchar              *account;		/* user credentials */
	gchar              *password;
	ofaIDBConnect      *connect;		/* the DB connection */
	gboolean            opened;
	gboolean            read_only;

	/* UI
	 */
	ofaDossierTreeview *dossier_tview;
	ofaExerciceCombo   *exercice_combo;
	GtkWidget          *readonly_btn;
	GtkWidget          *message_label;
	GtkWidget          *ok_btn;
}
	ofaDossierOpenPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-open.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_dossier_changed( ofaDossierTreeview *tview, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaDossierOpen *self );
static void      on_exercice_changed( ofaExerciceCombo *combo, ofaIDBPeriod *period, ofaDossierOpen *self );
static void      on_user_credentials_changed( ofaUserCredentialsBin *credentials, const gchar *account, const gchar *password, ofaDossierOpen *self );
static void      check_for_enable_dlg( ofaDossierOpen *self );
static gboolean  are_data_set( ofaDossierOpen *self, gchar **msg );
static gboolean  idialog_quit_on_ok( myIDialog *instance );
static gboolean  is_connection_valid( ofaDossierOpen *self, gchar **msg );
static gboolean  do_open_dossier( ofaDossierOpen *self );
static void      set_message( ofaDossierOpen *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaDossierOpen, ofa_dossier_open, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierOpen )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_open_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_open_finalize";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_OPEN( instance ));

	/* free data members here */
	priv = ofa_dossier_open_get_instance_private( OFA_DOSSIER_OPEN( instance ));

	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_open_parent_class )->finalize( instance );
}

static void
dossier_open_dispose( GObject *instance )
{
	ofaDossierOpenPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_OPEN( instance ));

	priv = ofa_dossier_open_get_instance_private( OFA_DOSSIER_OPEN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->meta );
		g_clear_object( &priv->period );
		g_clear_object( &priv->connect );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_open_parent_class )->dispose( instance );
}

static void
ofa_dossier_open_init( ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_init";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_OPEN( self ));

	priv = ofa_dossier_open_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->opened = FALSE;
	priv->read_only = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_open_class_init( ofaDossierOpenClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_open_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_open_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_open_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_open_run:
 * @getter: a #ofaIGetter instance.
 * @parent: the parent #GtkWindow.
 * @meta: [allow-none]: the dossier to be opened.
 * @period: [allow-none]: the exercice to be opened.
 * @account: [allow-none]: the user account.
 * @password: [allow-none]: the user password.
 *
 * Open the specified dossier, requiring the missing informations
 * if needed.
 *
 * Returns: %TRUE if the dossier has been opened, %FALSE else.
 */
gboolean
ofa_dossier_open_run( ofaIGetter *getter, GtkWindow *parent,
										ofaIDBMeta *meta, ofaIDBPeriod *period,
										const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_dossier_open_run";
	ofaDossierOpen *self;
	ofaDossierOpenPrivate *priv;
	gboolean opened;
	gchar *msg;

	g_debug( "%s: getter=%p, parent=%p, meta=%p, period=%p, account=%s, password=%s",
			thisfn, ( void * ) getter, ( void * ) parent,
			( void * ) meta, ( void * ) period, account, password ? "******" : "(null)" );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );

	self = g_object_new( OFA_TYPE_DOSSIER_OPEN, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_dossier_open_get_instance_private( self );

	priv->getter = getter;

	if( meta ){
		priv->meta = g_object_ref( meta );
		if( period ){
			priv->period = g_object_ref( period );
		}
	}

	priv->account = g_strdup( account );
	priv->password = g_strdup( password );

	opened = FALSE;
	msg = NULL;

	if( are_data_set( self, &msg ) && is_connection_valid( self, &msg )){
		do_open_dossier( self );

	} else {
		g_debug( "%s: %s", thisfn, msg );
		g_free( msg );
		if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
			opened = priv->opened;
			my_iwindow_close( MY_IWINDOW( self ));
		}
	}

	return( opened );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_open_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_open_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_open_idialog_init";
	ofaDossierOpenPrivate *priv;
	GtkWidget *container, *button, *focus, *label;
	ofaUserCredentialsBin *user_credentials;
	GtkSizeGroup *group;
	gchar *dossier_name;
	ofaIDBPeriod *init_period;
	static ofaDossierDispColumn st_columns[] = {
			DOSSIER_DISP_DOSNAME,
			0 };

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_open_get_instance_private( OFA_DOSSIER_OPEN( instance ));

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* do this first to be available as soon as the first signal
	 * triggers */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-open" );
	priv->ok_btn = button;

	/* setup exercice combobox (before dossier) */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "do-exercice-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	priv->exercice_combo = ofa_exercice_combo_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->exercice_combo ));
	g_signal_connect(
			G_OBJECT( priv->exercice_combo ), "ofa-changed", G_CALLBACK( on_exercice_changed ), instance );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "do-exercice-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->exercice_combo ));
	gtk_size_group_add_widget( group, label );

	/* setup dossier treeview */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "do-dossier-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->dossier_tview = ofa_dossier_treeview_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->dossier_tview ));
	ofa_dossier_treeview_set_columns( priv->dossier_tview, st_columns );
	ofa_dossier_treeview_set_show( priv->dossier_tview, DOSSIER_SHOW_UNIQUE );
	g_signal_connect(
			G_OBJECT( priv->dossier_tview ), "changed", G_CALLBACK( on_dossier_changed ), instance );
	focus = GTK_WIDGET( priv->dossier_tview );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "do-dossier-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget(
			GTK_LABEL( label ), ofa_dossier_treeview_get_treeview( priv->dossier_tview ));
	gtk_size_group_add_widget( group, label );

	if( priv->meta ){
		/* because initial priv->period will be reset when selecting the
		 * dossier */
		init_period = priv->period ? g_object_ref( priv->period ) : NULL;
		dossier_name = ofa_idbmeta_get_dossier_name( priv->meta );
		ofa_dossier_treeview_set_selected( priv->dossier_tview, dossier_name );
		g_free( dossier_name );
		focus = GTK_WIDGET( priv->exercice_combo );

		if( init_period ){
			//ofa_idbperiod_dump( priv->init_period );
			ofa_exercice_combo_set_selected( priv->exercice_combo, init_period );
			focus = NULL;
		}

		g_clear_object( &init_period );
	}

	/* setup account and password */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "do-user-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	user_credentials = ofa_user_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( user_credentials ));
	g_signal_connect( user_credentials, "ofa-changed", G_CALLBACK( on_user_credentials_changed ), instance );
	my_utils_size_group_add_size_group(
			group, ofa_user_credentials_bin_get_size_group( user_credentials, 0 ));
	if( priv->account ){
		ofa_user_credentials_bin_set_account( user_credentials, priv->account );
	}
	if( priv->password ){
		ofa_user_credentials_bin_set_password( user_credentials, priv->password );
	}

	/* get read-only mode */
	priv->readonly_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "read-only-btn" );
	g_return_if_fail( priv->readonly_btn && GTK_IS_CHECK_BUTTON( priv->readonly_btn ));

	/* focus defauls to be set on the dossier treeview
	 * if the dossier is already set, then set the focus on the exercice combo
	 * if the exercice is also already selected, then set focus to NULL
	 * this means that the focus is to be grabbed by user credentials */
	if( focus ){
		gtk_widget_grab_focus( focus );
	} else {
		ofa_user_credentials_bin_grab_focus( user_credentials );
	}

	g_object_unref( group );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_DOSSIER_OPEN( instance ));
}

static void
on_dossier_changed( ofaDossierTreeview *tview, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = ofa_dossier_open_get_instance_private( self );

	g_clear_object( &priv->meta );
	if( meta ){
		priv->meta = g_object_ref( meta );
	}
	ofa_exercice_combo_set_dossier( priv->exercice_combo, meta );

	check_for_enable_dlg( self );
}

static void
on_exercice_changed( ofaExerciceCombo *combo, ofaIDBPeriod *period, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = ofa_dossier_open_get_instance_private( self );

	g_clear_object( &priv->period );
	if( period ){
		priv->period = g_object_ref( period );
	}

	check_for_enable_dlg( self );
}

static void
on_user_credentials_changed( ofaUserCredentialsBin *credentials, const gchar *account, const gchar *password, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = ofa_dossier_open_get_instance_private( self );

	g_free( priv->account );
	priv->account = g_strdup( account );

	g_free( priv->password );
	priv->password = g_strdup( password );

	check_for_enable_dlg( self );
}

/*
 * For security reasons, we do not check automatically the user
 * credentials. Instead, we check automatically that they are set, but
 * only check for the DB connection when the user clicks OK
 */
static void
check_for_enable_dlg( ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;
	gboolean ok_enable, ro_enable;
	gchar *msg;

	priv = ofa_dossier_open_get_instance_private( self );

	msg = NULL;

	ok_enable = are_data_set( self, &msg );

	ro_enable = priv->meta && priv->period && ofa_idbperiod_get_current( priv->period );
	gtk_widget_set_sensitive( priv->readonly_btn, ro_enable );

	set_message( self, msg );
	g_free( msg );

	gtk_widget_set_sensitive( priv->ok_btn, ok_enable );
}

static gboolean
are_data_set( ofaDossierOpen *self, gchar **msg )
{
	ofaDossierOpenPrivate *priv;
	gboolean valid;

	priv = ofa_dossier_open_get_instance_private( self );

	valid = FALSE;

	if( !priv->meta ){
		if( msg ){
			*msg = g_strdup( _( "No selected dossier" ));
		}
	} else if( !priv->period ){
		if( msg ){
			*msg = g_strdup( _( "No selected exercice" ));
		}
	} else if( !my_strlen( priv->account )){
		if( msg ){
			*msg = g_strdup( _( "Empty connection account" ));
		}
	} else if( !my_strlen( priv->password )){
		if( msg ){
			*msg = g_strdup(_( "Empty connection password" ));
		}
	} else {
		valid = TRUE;
	}

	return( valid );
}

/*
 * all data are expected to be set
 * but we have still to check the DB connection
 */
static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	ofaDossierOpenPrivate *priv;
	gchar *msg;

	g_return_val_if_fail( are_data_set( OFA_DOSSIER_OPEN( instance ), NULL ), FALSE );

	priv = ofa_dossier_open_get_instance_private( OFA_DOSSIER_OPEN( instance ));

	msg = NULL;

	if( is_connection_valid( OFA_DOSSIER_OPEN( instance ), &msg )){
		priv->read_only = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->readonly_btn ));
		priv->opened = do_open_dossier( OFA_DOSSIER_OPEN( instance ));
		return( priv->opened );
	}

	my_iwindow_msg_dialog( MY_IWINDOW( instance ), GTK_MESSAGE_WARNING, msg );
	g_free( msg );

	return( FALSE );
}

static gboolean
is_connection_valid( ofaDossierOpen *self, gchar **msg )
{
	ofaDossierOpenPrivate *priv;
	ofaIDBProvider *provider;
	gboolean valid;

	priv = ofa_dossier_open_get_instance_private( self );

	g_clear_object( &priv->connect );
	valid = FALSE;

	provider = ofa_idbmeta_get_provider( priv->meta );
	priv->connect = ofa_idbprovider_new_connect( provider );
	valid = ofa_idbconnect_open_with_meta(
					priv->connect, priv->account, priv->password, priv->meta, priv->period );
	g_object_unref( provider );

	if( msg ){
		if( valid ){
			g_free( *msg );
			*msg = NULL;
		} else {
			*msg = g_strdup_printf( _( "Invalid credentials for '%s' account" ), priv->account );
		}
	}

	return( valid );
}

/*
 * is called when the user click on the 'Open' button
 */
static gboolean
do_open_dossier( ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;
	ofaHub *hub;
	gboolean ok;

	priv = ofa_dossier_open_get_instance_private( self );

	ok = FALSE;

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	if( ofa_hub_dossier_open( hub, priv->connect, GTK_WINDOW( self ), priv->read_only )){
		ofa_hub_dossier_remediate_settings( hub );
		ok = TRUE;
	}

	return( ok );
}

static void
set_message( ofaDossierOpen *self, const gchar *msg )
{
	ofaDossierOpenPrivate *priv;

	priv = ofa_dossier_open_get_instance_private( self );

	if( !priv->message_label ){
		priv->message_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "message" );
		g_return_if_fail( priv->message_label && GTK_IS_LABEL( priv->message_label ));

		my_utils_widget_set_style( priv->message_label, "labelerror" );
	}

	gtk_label_set_text( GTK_LABEL( priv->message_label ), msg );
}
