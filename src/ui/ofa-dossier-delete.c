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
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-dossier-delete-prefs-bin.h"
#include "core/ofa-main-window.h"

#include "ui/ofa-dossier-delete.h"

/* private instance data
 */
struct _ofaDossierDeletePrivate {

	/* initialization
	 */
	ofaMainWindow            *main_window;
	ofaIDBMeta               *meta;
	ofaIDBPeriod             *period;

	/* UI
	 */
	ofaIDBEditor             *infos;
	ofaDBMSRootBin           *credentials;
	ofaDossierDeletePrefsBin *prefs;
	GtkWidget                *err_msg;
	GtkWidget                *btn_delete;

	/* runtime data
	 */
	ofaIDBProvider           *provider;
	gboolean                  deleted;
	gchar                    *root_account;
	gchar                    *root_password;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-delete.ui";
static const gchar *st_ui_id            = "DossierDeleteDlg";

G_DEFINE_TYPE( ofaDossierDelete, ofa_dossier_delete, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_credentials_changed( ofaDBMSRootBin *bin, gchar *account, gchar *password, ofaDossierDelete *dialog );
static void      check_for_enable_dlg( ofaDossierDelete *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_delete_dossier( ofaDossierDelete *self );

static void
dossier_delete_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_finalize";
	ofaDossierDeletePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_DELETE( instance )->priv;

	g_free( priv->root_account );
	g_free( priv->root_password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_parent_class )->finalize( instance );
}

static void
dossier_delete_dispose( GObject *instance )
{
	ofaDossierDeletePrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_DOSSIER_DELETE( instance )->priv;

		/* unref object members here */
		g_clear_object( &priv->meta );
		g_clear_object( &priv->period );
		g_clear_object( &priv->provider );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_parent_class )->dispose( instance );
}

static void
ofa_dossier_delete_init( ofaDossierDelete *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DELETE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_DELETE, ofaDossierDeletePrivate );
}

static void
ofa_dossier_delete_class_init( ofaDossierDeleteClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_delete_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_delete_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_delete_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierDeletePrivate ));
}

/**
 * ofa_dossier_delete_run:
 * @main_window: the main window of the application.
 * @meta:
 * @period:
 *
 * Run the selection dialog to delete a dossier.
 *
 * Returns: %TRUE if the dossier has been deleted, %FALSE if the action
 * has been cancelled by the user or an error has occured.
 */
gboolean
ofa_dossier_delete_run( ofaMainWindow *main_window, const ofaIDBMeta *meta, const ofaIDBPeriod *period )
{
	static const gchar *thisfn = "ofa_dossier_delete_run";
	ofaDossierDelete *self;
	ofaDossierDeletePrivate *priv;
	gboolean deleted;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	g_return_val_if_fail( meta && OFA_IS_IDBMETA( meta ), FALSE );
	g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), FALSE );

	g_debug( "%s: main_window=%p, meta=%p, period=%p",
				thisfn, main_window, ( void * ) meta, ( void * ) period );

	self = g_object_new(
				OFA_TYPE_DOSSIER_DELETE,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	priv = self->priv;

	priv->main_window = main_window;
	priv->deleted = FALSE;
	priv->meta = g_object_ref(( gpointer ) meta );
	priv->period = g_object_ref(( gpointer ) period );

	my_dialog_run_dialog( MY_DIALOG( self ));

	deleted = priv->deleted;

	g_object_unref( self );

	return( deleted );
}

/*
 * this dialog is entirely built with composite widgets
 */
static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierDeletePrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *label, *parent;
	gchar *msg;
	GtkSizeGroup *group;
	gchar *dossier_name;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	priv = OFA_DOSSIER_DELETE( dialog )->priv;

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* informational message */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	dossier_name = ofa_idbmeta_get_dossier_name( priv->meta );
	msg = g_strdup_printf( _(
			"You are about to delete the '%s' dossier.\n"
			"Please provide below the connection informations "
			"for the DBserver administrative account." ),
				dossier_name );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );
	g_free( dossier_name );

	/* connection infos */
	priv->provider = ofa_idbmeta_get_provider( priv->meta );
	g_return_if_fail( priv->provider && OFA_IS_IDBPROVIDER( priv->provider ));
	priv->infos = ofa_idbprovider_new_editor( priv->provider, FALSE );
	g_return_if_fail( priv->infos && GTK_IS_WIDGET( priv->infos ));
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "infos-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->infos ));
	my_utils_size_group_add_size_group(
			group, ofa_idbeditor_get_size_group( priv->infos, 0 ));

	/* root credentials */
	priv->credentials = ofa_dbms_root_bin_new();
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "credentials-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->credentials ));
	ofa_dbms_root_bin_set_meta( priv->credentials, priv->meta );
	my_utils_size_group_add_size_group(
			group, ofa_dbms_root_bin_get_size_group( priv->credentials, 0 ));
	g_signal_connect( priv->credentials, "ofa-changed", G_CALLBACK( on_credentials_changed ), dialog );

	/* preferences */
	priv->prefs = ofa_dossier_delete_prefs_bin_new();
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "prefs-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->prefs ));

	/* other widgets */
	priv->err_msg = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "px-errmsg" );
	g_return_if_fail( priv->err_msg && GTK_IS_LABEL( priv->err_msg ));
	my_utils_widget_set_style( priv->err_msg, "labelerror" );

	priv->btn_delete = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->btn_delete && GTK_IS_BUTTON( priv->btn_delete ));

	g_object_unref( group );
}

static void
on_credentials_changed( ofaDBMSRootBin *bin, gchar *account, gchar *password, ofaDossierDelete *dialog )
{
	ofaDossierDeletePrivate *priv;

	priv = dialog->priv;

	g_free( priv->root_account );
	priv->root_account = g_strdup( account );
	g_free( priv->root_password );
	priv->root_password = g_strdup( password );

	check_for_enable_dlg( dialog );
}

static void
check_for_enable_dlg( ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	gboolean enabled;
	gchar *msg;

	priv = self->priv;

	enabled = ofa_dbms_root_bin_is_valid( priv->credentials, &msg );
	gtk_label_set_text( GTK_LABEL( priv->err_msg ), msg ? msg : "" );
	g_free( msg );

	gtk_widget_set_sensitive( priv->btn_delete, enabled );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierDeletePrivate *priv;

	priv = OFA_DOSSIER_DELETE( dialog )->priv;

	priv->deleted = do_delete_dossier( OFA_DOSSIER_DELETE( dialog ));

	ofa_dossier_delete_prefs_bin_set_settings( priv->prefs );

	return( TRUE );
}

static gboolean
do_delete_dossier( ofaDossierDelete *self )
{
#if 0
	static const gchar *thisfn = "ofa_dossier_delete_do_delete_dossier";
	ofaDossierDeletePrivate *priv;
	gint db_mode;
	gboolean drop_db, drop_accounts;

	priv = self->priv;

	db_mode = ofa_dossier_delete_prefs_bin_get_db_mode( priv->prefs );
	g_debug( "%s: db_mode=%u", thisfn, db_mode );
	drop_db = ( db_mode == DBMODE_REINIT );
	drop_accounts = ofa_dossier_delete_prefs_bin_get_account_mode( priv->prefs );

	ofa_idbms_delete_dossier(
			priv->idbms,
			priv->dname, priv->root_account, priv->root_password,
			drop_db, drop_accounts, FALSE );
#endif
	return( TRUE );
}
