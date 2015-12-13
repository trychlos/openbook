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
#include "api/ofa-idbmeta.h"
#include "api/ofo-dossier.h"

#include "core/ofa-dbms-root-bin.h"

#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-dossier-new-mini.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewMiniPrivate {

	/* UI
	 */
	ofaDossierNewBin *new_bin;
	GtkWidget        *ok_btn;
	GtkWidget        *msg_label;

	/* result
	 */
	gboolean          dossier_defined;
	ofaIDBMeta       *meta;
	gchar            *account;
	gchar            *password;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-new-mini.ui";
static const gchar *st_ui_id            = "DossierNewMiniDialog";

G_DEFINE_TYPE( ofaDossierNewMini, ofa_dossier_new_mini, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, void *infos, const gchar *account, const gchar *password, ofaDossierNewMini *self );
static void      check_for_enable_dlg( ofaDossierNewMini *self );
static gboolean  is_validable( ofaDossierNewMini *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static void      set_message( ofaDossierNewMini *self, const gchar *message );

static void
dossier_new_mini_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_finalize";
	ofaDossierNewMiniPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_MINI( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_NEW_MINI( instance )->priv;

	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_mini_parent_class )->finalize( instance );
}

static void
dossier_new_mini_dispose( GObject *instance )
{
	ofaDossierNewMiniPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_MINI( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_DOSSIER_NEW_MINI( instance )->priv;

		g_clear_object( &priv->meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_mini_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_mini_init( ofaDossierNewMini *self )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW_MINI( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMiniPrivate );
}

static void
ofa_dossier_new_mini_class_init( ofaDossierNewMiniClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_mini_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_mini_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierNewMiniPrivate ));
}

/**
 * ofa_dossier_new_mini_run:
 * @main_window: the main window of the application.
 * @parent: [allow-none]: the window parent; defaults to the @main_window.
 * @meta: [out]: an #ofaIDBMeta object which defines the newly created
 *  dossier.
 * @account: [out]: the DBMS superuser account.
 * @password: [out]: the corresponding password.
 *
 * Returns: %TRUE if a new dossier has been defined in the settings.
 */
gboolean
ofa_dossier_new_mini_run( ofaMainWindow *main_window, GtkWindow *parent,
							ofaIDBMeta **meta, gchar **account, gchar **password )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_run";
	ofaDossierNewMini *self;
	ofaDossierNewMiniPrivate *priv;
	gboolean dossier_defined;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), FALSE );
	g_return_val_if_fail( meta, FALSE );
	g_return_val_if_fail( account, FALSE );
	g_return_val_if_fail( password, FALSE );

	g_debug( "%s: main_window=%p, parent=%p, meta=%p, account=%p, password=%p",
			thisfn, ( void * ) main_window, ( void * ) parent,
			( void * ) meta, ( void * ) account, ( void * ) password );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW_MINI,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_PARENT,      parent,
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	priv = self->priv;
	dossier_defined = priv->dossier_defined;
	if( dossier_defined ){
		*meta = g_object_ref( priv->meta );
		*account = g_strdup( priv->account );
		*password = g_strdup( priv->password );
	}

	g_object_unref( self );

	return( dossier_defined );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierNewMiniPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *parent, *label;
	GtkSizeGroup *group;

	priv = OFA_DOSSIER_NEW_MINI( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "new-bin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->new_bin = ofa_dossier_new_bin_new(
							OFA_MAIN_WINDOW( my_window_get_main_window( MY_WINDOW( dialog ))));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->new_bin ));
	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	my_utils_size_group_add_size_group(
			group, ofa_dossier_new_bin_get_size_group( priv->new_bin, 0 ));
	g_object_unref( group );

	g_signal_connect( priv->new_bin, "ofa-changed", G_CALLBACK( on_new_bin_changed ), dialog );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "err-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelerror" );
	priv->msg_label = label;

	check_for_enable_dlg( OFA_DOSSIER_NEW_MINI( dialog ));
}

static void
on_new_bin_changed( ofaDossierNewBin *bin, const gchar *dname, void *infos, const gchar *account, const gchar *password, ofaDossierNewMini *self )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_on_new_bin_changed";

	g_debug( "%s: bin=%p, infos=%p", thisfn, ( void * ) bin, ( void * ) infos );

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierNewMini *self )
{
	ofaDossierNewMiniPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = is_validable( self );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
is_validable( ofaDossierNewMini *self )
{
	ofaDossierNewMiniPrivate *priv;
	gboolean ok;
	gchar *str;

	priv = self->priv;
	ok = ofa_dossier_new_bin_get_valid( priv->new_bin, &str );
	set_message( self, str );

	return( ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierNewMiniPrivate *priv;
	ofaDBMSRootBin *root_bin;

	priv = OFA_DOSSIER_NEW_MINI( dialog )->priv;
	priv->meta = ofa_dossier_new_bin_apply( priv->new_bin );
	if( priv->meta ){
		priv->dossier_defined = TRUE;
		root_bin = ofa_dossier_new_bin_get_dbms_root_bin( priv->new_bin );
		ofa_dbms_root_bin_get_credentials( root_bin, &priv->account, &priv->password );
	}

	return( TRUE );
}

static void
set_message( ofaDossierNewMini *self, const gchar *message )
{
	ofaDossierNewMiniPrivate *priv;

	priv = self->priv;

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ), message );
	}
}
