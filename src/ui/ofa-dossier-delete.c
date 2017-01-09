/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-dossier-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"

#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-dossier-delete-prefs-bin.h"

#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean                  dispose_has_run;

	/* initialization
	 */
	ofaIGetter               *getter;
	GtkWindow                *parent;
	ofaIDBDossierMeta        *dossier_meta;
	ofaIDBExerciceMeta       *exercice_meta;

	/* runtime
	 */
	ofaHub                   *hub;
	gchar                    *root_account;
	gchar                    *root_password;

	/* UI
	 */
	ofaIDBEditor             *infos;
	ofaDBMSRootBin           *credentials;
	ofaDossierDeletePrefsBin *prefs;
	GtkWidget                *err_msg;
	GtkWidget                *delete_btn;
}
	ofaDossierDeletePrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-delete.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     on_credentials_changed( ofaDBMSRootBin *bin, gchar *account, gchar *password, ofaDossierDelete *dialog );
static void     check_for_enable_dlg( ofaDossierDelete *self );
static gboolean do_delete_dossier( ofaDossierDelete *self, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( ofaDossierDelete, ofa_dossier_delete, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierDelete )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_delete_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_finalize";
	ofaDossierDeletePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE( instance ));

	/* free data members here */
	priv = ofa_dossier_delete_get_instance_private( OFA_DOSSIER_DELETE( instance ));

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

	priv = ofa_dossier_delete_get_instance_private( OFA_DOSSIER_DELETE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->dossier_meta );
		g_clear_object( &priv->exercice_meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_parent_class )->dispose( instance );
}

static void
ofa_dossier_delete_init( ofaDossierDelete *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_init";
	ofaDossierDeletePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DELETE( self ));

	priv = ofa_dossier_delete_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_delete_class_init( ofaDossierDeleteClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_delete_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_delete_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_delete_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_delete_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @dossier_meta: the meta data for the dossier.
 * @period: the period to be deleted.
 *
 * Run the selection dialog to delete a dossier.
 */
void
ofa_dossier_delete_run( ofaIGetter *getter, GtkWindow *parent, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_dossier_delete_run";
	ofaDossierDelete *self;
	ofaDossierDeletePrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, dossier_meta=%p, period=%p",
				thisfn, ( void * ) getter, ( void * ) parent, ( void * ) dossier_meta, ( void * ) period );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ));
	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	self = g_object_new( OFA_TYPE_DOSSIER_DELETE, NULL );

	priv = ofa_dossier_delete_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->dossier_meta = g_object_ref(( gpointer ) dossier_meta );
	priv->exercice_meta = g_object_ref(( gpointer ) period );

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_delete_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_iwindow_init";
	ofaDossierDeletePrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_delete_get_instance_private( OFA_DOSSIER_DELETE( instance ));

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
	static const gchar *thisfn = "ofa_dossier_delete_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_idialog_init";
	ofaDossierDeletePrivate *priv;
	GtkWidget *label, *parent;
	gchar *msg;
	GtkSizeGroup *group;
	const gchar *dossier_name;
	ofaIDBProvider *provider;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_delete_get_instance_private( OFA_DOSSIER_DELETE( instance ));

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	/* informational message */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->dossier_meta );
	msg = g_strdup_printf( _(
			"You are about to delete the '%s' dossier.\n"
			"Please provide below the connection informations "
			"for the DBserver administrative account." ),
				dossier_name );
	gtk_label_set_text( GTK_LABEL( label ), msg );
	g_free( msg );

	/* connection infos */
	provider = ofa_idbdossier_meta_get_provider( priv->dossier_meta );
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));
	priv->infos = ofa_idbprovider_new_editor( provider, FALSE );
	g_return_if_fail( priv->infos && GTK_IS_WIDGET( priv->infos ));
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "infos-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->infos ));
	my_utils_size_group_add_size_group(
			group, ofa_idbeditor_get_size_group( priv->infos, 0 ));

	/* root credentials */
	priv->credentials = ofa_dbms_root_bin_new( priv->hub );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "credentials-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->credentials ));
	ofa_dbms_root_bin_set_meta( priv->credentials, priv->dossier_meta );
	my_utils_size_group_add_size_group(
			group, ofa_dbms_root_bin_get_size_group( priv->credentials, 0 ));
	g_signal_connect( priv->credentials, "ofa-changed", G_CALLBACK( on_credentials_changed ), instance );

	/* preferences */
	priv->prefs = ofa_dossier_delete_prefs_bin_new( priv->hub );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "prefs-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->prefs ));

	/* other widgets */
	priv->err_msg = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "px-errmsg" );
	g_return_if_fail( priv->err_msg && GTK_IS_LABEL( priv->err_msg ));
	my_style_add( priv->err_msg, "labelerror" );

	priv->delete_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->delete_btn && GTK_IS_BUTTON( priv->delete_btn ));
	my_idialog_click_to_update( instance, priv->delete_btn, ( myIDialogUpdateCb ) do_delete_dossier );

	g_object_unref( group );
}

static void
on_credentials_changed( ofaDBMSRootBin *bin, gchar *account, gchar *password, ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;

	priv = ofa_dossier_delete_get_instance_private( self );

	g_free( priv->root_account );
	priv->root_account = g_strdup( account );
	g_free( priv->root_password );
	priv->root_password = g_strdup( password );

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierDelete *self )
{
	ofaDossierDeletePrivate *priv;
	gboolean enabled;
	gchar *msg;

	priv = ofa_dossier_delete_get_instance_private( self );

	enabled = ofa_dbms_root_bin_is_valid( priv->credentials, &msg );
	gtk_label_set_text( GTK_LABEL( priv->err_msg ), msg ? msg : "" );
	g_free( msg );

	gtk_widget_set_sensitive( priv->delete_btn, enabled );
}

static gboolean
do_delete_dossier( ofaDossierDelete *self, gchar **msgerr )
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
