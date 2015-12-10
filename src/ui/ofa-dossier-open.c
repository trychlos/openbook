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
#include "api/ofa-dbms.h"
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
struct _ofaDossierOpenPrivate {

	/* data
	 */
	ofaIFileMeta       *meta;			/* the selected dossier */
	ofaIFilePeriod     *period;			/* the selected exercice */
	gchar              *account;		/* user credentials */
	gchar              *password;
	ofaIDBConnect      *connect;		/* the DB connection */

	/* UI
	 */
	ofaDossierTreeview *dossier_tview;
	ofaExerciceCombo   *exercice_combo;
	GtkWidget          *message_label;
	GtkWidget          *ok_btn;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-open.ui";
static const gchar *st_ui_id            = "DossierOpenDlg";

G_DEFINE_TYPE( ofaDossierOpen, ofa_dossier_open, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_dossier_changed( ofaDossierTreeview *tview, ofaIFileMeta *meta, ofaIFilePeriod *period, ofaDossierOpen *self );
static void      on_exercice_changed( ofaExerciceCombo *combo, ofaIFilePeriod *period, ofaDossierOpen *self );
static void      on_user_credentials_changed( ofaUserCredentialsBin *credentials, const gchar *account, const gchar *password, ofaDossierOpen *self );
static void      check_for_enable_dlg( ofaDossierOpen *self );
static gboolean  are_data_set( ofaDossierOpen *self, gchar **msg );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  is_connection_valid( ofaDossierOpen *self, gchar **msg );
static void      do_open_dossier( ofaDossierOpen *self );
static void      set_message( ofaDossierOpen *self, const gchar *msg );

static void
dossier_open_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_open_finalize";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_OPEN( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_OPEN( instance )->priv;

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

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_DOSSIER_OPEN( instance )->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_OPEN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_OPEN, ofaDossierOpenPrivate );
}

static void
ofa_dossier_open_class_init( ofaDossierOpenClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_open_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_open_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_open_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierOpenPrivate ));
}

/**
 * ofa_dossier_open_run:
 * @main_window: the main window of the application.
 * @meta: [allow-none]: the dossier to be opened.
 * @period: [allow-none]: the exercice to be opened.
 * @account: [allow-none]: the user account.
 * @password: [allow-none]: the user password.
 *
 * Open the specified dossier, requiring the missing informations
 * if needed.
 */
void
ofa_dossier_open_run( ofaMainWindow *main_window,
		ofaIFileMeta *meta, ofaIFilePeriod *period, const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_dossier_open_run";
	ofaDossierOpen *self;
	ofaDossierOpenPrivate *priv;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, meta=%p, period=%p, account=%s, password=%s",
			thisfn, ( void * ) main_window, ( void * ) meta, ( void * ) period, account, password ? "******" : "(null)" );

	self = g_object_new(
				OFA_TYPE_DOSSIER_OPEN,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	priv = self->priv;
	if( meta ){
		priv->meta = g_object_ref( meta );
		if( period ){
			priv->period = g_object_ref( period );
		}
	}
	priv->account = g_strdup( account );
	priv->password = g_strdup( password );

	if( are_data_set( self, NULL ) && is_connection_valid( self, NULL )){
		do_open_dossier( self );
	} else {
		my_dialog_run_dialog( MY_DIALOG( self ));
	}

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierOpenPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *container, *button, *focus, *label;
	ofaUserCredentialsBin *user_credentials;
	GtkSizeGroup *group;
	static ofaDossierDispColumn st_columns[] = {
			DOSSIER_DISP_DOSNAME,
			0 };

	priv = OFA_DOSSIER_OPEN( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));
	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* do this first to be available as soon as the first signal
	 * triggers */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-open" );
	priv->ok_btn = button;

	/* setup exercice combobox (before dossier) */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "do-exercice-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	priv->exercice_combo = ofa_exercice_combo_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->exercice_combo ));
	g_signal_connect(
			G_OBJECT( priv->exercice_combo ), "ofa-changed", G_CALLBACK( on_exercice_changed ), dialog );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "do-exercice-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->exercice_combo ));
	gtk_size_group_add_widget( group, label );

	/* setup dossier treeview */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "do-dossier-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->dossier_tview = ofa_dossier_treeview_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->dossier_tview ));
	ofa_dossier_treeview_set_columns( priv->dossier_tview, st_columns );
	ofa_dossier_treeview_set_show( priv->dossier_tview, DOSSIER_SHOW_CURRENT );
	g_signal_connect(
			G_OBJECT( priv->dossier_tview ), "changed", G_CALLBACK( on_dossier_changed ), dialog );
	focus = GTK_WIDGET( priv->dossier_tview );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "do-dossier-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget(
			GTK_LABEL( label ), ofa_dossier_treeview_get_treeview( priv->dossier_tview ));
	gtk_size_group_add_widget( group, label );

#if 0
	if( priv->dname ){
		ofa_dossier_treeview_set_selected( priv->dossier_tview, priv->dname );
		focus = GTK_WIDGET( priv->exercice_combo );

		if( priv->open_db ){
			//ofa_exercice_combo_set_selected( priv->exercice_combo, EXERCICE_COL_DBNAME, priv->open_db );
			focus = NULL;
		}
	}
#endif

	/* setup account and password */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "do-user-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	user_credentials = ofa_user_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( user_credentials ));
	g_signal_connect( user_credentials, "ofa-changed", G_CALLBACK( on_user_credentials_changed ), dialog );
	my_utils_size_group_add_size_group(
			group, ofa_user_credentials_bin_get_size_group( user_credentials, 0 ));

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

	check_for_enable_dlg( OFA_DOSSIER_OPEN( dialog ));
}

static void
on_dossier_changed( ofaDossierTreeview *tview, ofaIFileMeta *meta, ofaIFilePeriod *period, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = self->priv;
	g_clear_object( &priv->meta );
	if( meta ){
		priv->meta = g_object_ref( meta );
	}
	ofa_exercice_combo_set_dossier( priv->exercice_combo, meta );

	check_for_enable_dlg( self );
}

static void
on_exercice_changed( ofaExerciceCombo *combo, ofaIFilePeriod *period, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = self->priv;
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

	priv = self->priv;

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
	gboolean ok_enable;
	gchar *msg;

	priv = self->priv;
	msg = NULL;
	set_message( self, "" );

	ok_enable = are_data_set( self, &msg );
	set_message( self, msg );
	g_free( msg );

	gtk_widget_set_sensitive( priv->ok_btn, ok_enable );
}

static gboolean
are_data_set( ofaDossierOpen *self, gchar **msg )
{
	ofaDossierOpenPrivate *priv;
	gboolean valid;

	priv = self->priv;
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
 * but we have yet to check the DB connection
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierOpen *self;
	gchar *msg;

	self = OFA_DOSSIER_OPEN( dialog );

	g_return_val_if_fail( are_data_set( self, NULL ), FALSE );

	if( is_connection_valid( self, &msg )){
		do_open_dossier( OFA_DOSSIER_OPEN( dialog ));
		return( TRUE );
	}

	my_utils_dialog_warning( msg );
	g_free( msg );

	return( FALSE );
}

static gboolean
is_connection_valid( ofaDossierOpen *self, gchar **msg )
{
	ofaDossierOpenPrivate *priv;
	ofaIDBProvider *provider;
	gboolean valid;

	priv = self->priv;
	g_clear_object( &priv->connect );
	valid = FALSE;

	provider = ofa_ifile_meta_get_provider( priv->meta );
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
static void
do_open_dossier( ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = self->priv;

	ofa_main_window_open_dossier(
			OFA_MAIN_WINDOW( my_window_get_main_window( MY_WINDOW( self ))),
			priv->connect,
			TRUE );
}

static void
set_message( ofaDossierOpen *self, const gchar *msg )
{
	ofaDossierOpenPrivate *priv;
	GtkWindow *toplevel;

	priv = self->priv;

	if( !priv->message_label ){
		toplevel = my_window_get_toplevel( MY_WINDOW( self ));
		g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

		priv->message_label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "message" );
		g_return_if_fail( priv->message_label && GTK_IS_LABEL( priv->message_label ));

		my_utils_widget_set_style( priv->message_label, "labelerror" );
	}

	gtk_label_set_text( GTK_LABEL( priv->message_label ), msg );
}
