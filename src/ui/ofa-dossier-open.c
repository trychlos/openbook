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
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
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
	gboolean               dispose_has_run;

	/* initialization
	 */
	ofaIGetter            *getter;
	GtkWindow             *parent;

	/* runtime
	 */
	gchar                 *settings_prefix;
	ofaHub                *hub;
	ofaIDBDossierMeta     *dossier_meta;		/* the selected dossier */
	ofaIDBExerciceMeta    *exercice_meta;		/* the selected exercice */
	gchar                 *account;				/* user credentials */
	gchar                 *password;
	ofaIDBConnect         *connect;				/* the DB connection */
	gboolean               opened;
	gboolean               read_only;

	/* UI
	 */
	ofaDossierTreeview    *dossier_tview;
	ofaExerciceCombo      *exercice_combo;
	ofaUserCredentialsBin *user_credentials;
	GtkWidget             *readonly_btn;
	GtkWidget             *message_label;
	GtkWidget             *ok_btn;
}
	ofaDossierOpenPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-open.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     idialog_init_exercice( ofaDossierOpen *self, GtkSizeGroup *group );
static void     idialog_init_dossier( ofaDossierOpen *self, GtkSizeGroup *group );
static void     idialog_init_credentials( ofaDossierOpen *self, GtkSizeGroup *group );
static void     idialog_init_menu( ofaDossierOpen *self );
static void     on_dossier_changed( ofaDossierTreeview *tview, ofaIDBDossierMeta *dossier_meta, ofaIDBExerciceMeta *period, ofaDossierOpen *self );
static void     on_exercice_changed( ofaExerciceCombo *combo, ofaIDBExerciceMeta *period, ofaDossierOpen *self );
static void     on_user_credentials_changed( ofaUserCredentialsBin *credentials, const gchar *account, const gchar *password, ofaDossierOpen *self );
static void     check_for_enable_dlg( ofaDossierOpen *self );
static gboolean are_data_set( ofaDossierOpen *self, gchar **msg );
static gboolean idialog_quit_on_ok( myIDialog *instance );
static gboolean is_connection_valid( ofaDossierOpen *self, gchar **msg );
static gboolean do_open_dossier( ofaDossierOpen *self );
static void     set_message( ofaDossierOpen *self, const gchar *msg );

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

	g_free( priv->settings_prefix );
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
		g_clear_object( &priv->dossier_meta );
		g_clear_object( &priv->exercice_meta );
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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
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
										ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period,
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

	priv = ofa_dossier_open_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;

	if( meta ){
		priv->dossier_meta = g_object_ref( meta );
		if( period ){
			priv->exercice_meta = g_object_ref( period );
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

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_open_iwindow_init";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_open_get_instance_private( OFA_DOSSIER_OPEN( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_settings( instance, ofa_hub_get_user_settings( priv->hub ));
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
	GtkWidget *button, *focus;
	GtkSizeGroup *group;
	const gchar *dossier_name;
	ofaIDBExerciceMeta *init_period;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_open_get_instance_private( OFA_DOSSIER_OPEN( instance ));

	/* do this first to be available as soon as the first signal
	 * triggers */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-open" );
	priv->ok_btn = button;

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	idialog_init_exercice( OFA_DOSSIER_OPEN( instance ), group );
	idialog_init_dossier( OFA_DOSSIER_OPEN( instance ), group );
	idialog_init_credentials( OFA_DOSSIER_OPEN( instance ), group );
	g_object_unref( group );

	/* setup the focus depending of the provided data */
	focus = GTK_WIDGET( priv->dossier_tview );

	if( priv->dossier_meta ){
		/* because initial priv->exercice_meta will be reset when selecting the
		 * dossier */
		init_period = priv->exercice_meta ? g_object_ref( priv->exercice_meta ) : NULL;
		dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->dossier_meta );
		ofa_dossier_treeview_set_selected( priv->dossier_tview, dossier_name );
		focus = GTK_WIDGET( priv->exercice_combo );

		if( init_period ){
			//ofa_idbexercice_meta_dump( priv->init_period );
			ofa_exercice_combo_set_selected( priv->exercice_combo, init_period );
			focus = NULL;
		}

		g_clear_object( &init_period );
	}

	if( priv->account ){
		ofa_user_credentials_bin_set_account( priv->user_credentials, priv->account );
	}
	if( priv->password ){
		ofa_user_credentials_bin_set_password( priv->user_credentials, priv->password );
	}

	/* focus defauls to be set on the dossier treeview
	 * if the dossier is already set, then set the focus on the exercice combo
	 * if the exercice is also already selected, then set focus to NULL
	 * this means that the focus is to be grabbed by user credentials */
	if( focus ){
		gtk_widget_grab_focus( focus );
	} else {
		ofa_user_credentials_bin_grab_focus( priv->user_credentials );
	}

	/* get read-only mode */
	priv->readonly_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "read-only-btn" );
	g_return_if_fail( priv->readonly_btn && GTK_IS_CHECK_BUTTON( priv->readonly_btn ));

	idialog_init_menu( OFA_DOSSIER_OPEN( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_DOSSIER_OPEN( instance ));
}

static void
idialog_init_exercice( ofaDossierOpen *self, GtkSizeGroup *group )
{
	ofaDossierOpenPrivate *priv;
	GtkWidget *container, *label;

	priv = ofa_dossier_open_get_instance_private( self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "do-exercice-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	priv->exercice_combo = ofa_exercice_combo_new( priv->hub );
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->exercice_combo ));
	g_signal_connect( priv->exercice_combo, "ofa-changed", G_CALLBACK( on_exercice_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "do-exercice-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->exercice_combo ));
	gtk_size_group_add_widget( group, label );
}

static void
idialog_init_dossier( ofaDossierOpen *self, GtkSizeGroup *group )
{
	ofaDossierOpenPrivate *priv;
	GtkWidget *container, *label;

	priv = ofa_dossier_open_get_instance_private( self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "do-dossier-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->dossier_tview = ofa_dossier_treeview_new( priv->hub );
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->dossier_tview ));
	ofa_tvbin_set_headers( OFA_TVBIN( priv->dossier_tview ), FALSE );
	ofa_dossier_treeview_set_settings_key( priv->dossier_tview, priv->settings_prefix );
	ofa_dossier_treeview_setup_columns( priv->dossier_tview );
	ofa_dossier_treeview_set_show_all( priv->dossier_tview, FALSE );

	g_signal_connect( priv->dossier_tview, "ofa-doschanged", G_CALLBACK( on_dossier_changed ), self );

	ofa_dossier_treeview_setup_store( priv->dossier_tview );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "do-dossier-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget(
			GTK_LABEL( label ), ofa_tvbin_get_tree_view( OFA_TVBIN( priv->dossier_tview )));
	gtk_size_group_add_widget( group, label );
}

static void
idialog_init_credentials( ofaDossierOpen *self, GtkSizeGroup *group )
{
	ofaDossierOpenPrivate *priv;
	GtkWidget *container;

	priv = ofa_dossier_open_get_instance_private( self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "do-user-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->user_credentials = ofa_user_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->user_credentials ));

	g_signal_connect( priv->user_credentials, "ofa-changed", G_CALLBACK( on_user_credentials_changed ), self );

	my_utils_size_group_add_size_group(
			group, ofa_user_credentials_bin_get_size_group( priv->user_credentials, 0 ));
}

static void
idialog_init_menu( ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;
	GMenu *menu;

	priv = ofa_dossier_open_get_instance_private( self );

	menu = g_menu_new();
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->dossier_tview ), OFA_IACTIONABLE( priv->dossier_tview ), menu );
	g_object_unref( menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->dossier_tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->dossier_tview ), OFA_IACTIONABLE( priv->dossier_tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static void
on_dossier_changed( ofaDossierTreeview *tview, ofaIDBDossierMeta *dossier_meta, ofaIDBExerciceMeta *period, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = ofa_dossier_open_get_instance_private( self );

	g_clear_object( &priv->dossier_meta );

	if( dossier_meta ){
		priv->dossier_meta = g_object_ref( dossier_meta );
		ofa_exercice_combo_set_dossier( priv->exercice_combo, dossier_meta );
	}

	check_for_enable_dlg( self );
}

static void
on_exercice_changed( ofaExerciceCombo *combo, ofaIDBExerciceMeta *period, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = ofa_dossier_open_get_instance_private( self );

	g_clear_object( &priv->exercice_meta );
	if( period ){
		priv->exercice_meta = g_object_ref( period );
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
	ro_enable = priv->dossier_meta && priv->exercice_meta && ofa_idbexercice_meta_get_current( priv->exercice_meta );

	if( priv->readonly_btn ){
		gtk_widget_set_sensitive( priv->readonly_btn, ro_enable );
	}

	set_message( self, msg );
	g_free( msg );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, ok_enable );
	}
}

static gboolean
are_data_set( ofaDossierOpen *self, gchar **msg )
{
	ofaDossierOpenPrivate *priv;
	gboolean valid;

	priv = ofa_dossier_open_get_instance_private( self );

	valid = FALSE;

	if( !priv->dossier_meta ){
		if( msg ){
			*msg = g_strdup( _( "No selected dossier" ));
		}
	} else if( !priv->exercice_meta ){
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

	provider = ofa_idbdossier_meta_get_provider( priv->dossier_meta );
	priv->connect = ofa_idbprovider_new_connect( provider );
	valid = ofa_idbconnect_open_with_meta(
					priv->connect, priv->account, priv->password, priv->dossier_meta, priv->exercice_meta );
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
	gboolean ok;

	priv = ofa_dossier_open_get_instance_private( self );

	ok = FALSE;

	if( ofa_hub_dossier_open( priv->hub, GTK_WINDOW( self ), priv->connect, TRUE, priv->read_only )){
		if( ofa_hub_dossier_remediate_settings( priv->hub )){
			g_signal_emit_by_name( priv->hub, SIGNAL_HUB_DOSSIER_CHANGED );
		}
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

		my_style_add( priv->message_label, "labelerror" );
	}

	gtk_label_set_text( GTK_LABEL( priv->message_label ), msg );
}
