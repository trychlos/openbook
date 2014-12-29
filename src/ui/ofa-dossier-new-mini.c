/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/ofa-dossier-new-mini.h"
#include "ui/ofa-dossier-new-piece.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewMiniPrivate {

	/* UI
	 */
	ofaDossierNewPiece *new_piece;
	GtkWidget          *ok_btn;

	/* result
	 */
	gboolean            dossier_defined;
	gchar              *dname;
	gchar              *account;
	gchar              *password;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-new-mini.ui";
static const gchar *st_ui_id            = "DossierNewMiniDialog";

G_DEFINE_TYPE( ofaDossierNewMini, ofa_dossier_new_mini, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_new_piece_changed( ofaDossierNewPiece *piece, const gchar *dname, void *infos, const gchar *account, const gchar *password, ofaDossierNewMini *self );
static void      check_for_enable_dlg( ofaDossierNewMini *self );
static gboolean  is_validable( ofaDossierNewMini *self );
static gboolean  v_quit_on_ok( myDialog *dialog );

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

	g_free( priv->dname );
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_mini_parent_class )->finalize( instance );
}

static void
dossier_new_mini_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_MINI( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
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
 * @main: the main window of the application.
 * @dname: the name of the dossier.
 * @account: the DBMS root credentials.
 * @password: the corresponding password.
 *
 * Returns: %TRUE if a new dossier has been defined in the settings.
 */
gboolean
ofa_dossier_new_mini_run( ofaMainWindow *main_window, gchar **dname, gchar **account, gchar **password )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_run";
	ofaDossierNewMini *self;
	ofaDossierNewMiniPrivate *priv;
	gboolean dossier_defined;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	g_return_val_if_fail(( account && password ) || ( !account && !password ), FALSE );

	g_debug( "%s: main_window=%p, account=%p, password=%p",
			thisfn, ( void * ) main_window, ( void * ) account, ( void * ) password );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW_MINI,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	priv = self->priv;
	dossier_defined = priv->dossier_defined;
	if( dossier_defined ){
		if( dname ){
			*dname = g_strdup( priv->dname );
		}
		if( account ){
			*account = g_strdup( priv->account );
			*password = g_strdup( priv->password );
		}
	}

	g_object_unref( self );

	return( dossier_defined );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierNewMiniPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *parent;
	GtkSizeGroup *group;

	priv = OFA_DOSSIER_NEW_MINI( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "new-piece-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->new_piece = ofa_dossier_new_piece_new();
	ofa_dossier_new_piece_attach_to( priv->new_piece, GTK_CONTAINER( parent ), group );
	g_object_unref( group );

	g_signal_connect( priv->new_piece, "changed", G_CALLBACK( on_new_piece_changed ), dialog );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	check_for_enable_dlg( OFA_DOSSIER_NEW_MINI( dialog ));
}

static void
on_new_piece_changed( ofaDossierNewPiece *piece, const gchar *dname, void *infos, const gchar *account, const gchar *password, ofaDossierNewMini *self )
{
	static const gchar *thisfn = "ofa_dossier_new_mini_on_new_piece_changed";

	g_debug( "%s: piece=%p, infos=%p", thisfn, ( void * ) piece, ( void * ) infos );

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

	priv = self->priv;
	ok = ofa_dossier_new_piece_is_valid( priv->new_piece );

	return( ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierNewMiniPrivate *priv;

	priv = OFA_DOSSIER_NEW_MINI( dialog )->priv;

	if( ofa_dossier_new_piece_apply( priv->new_piece )){
		priv->dossier_defined = TRUE;
		ofa_dossier_new_piece_get_dname( priv->new_piece, &priv->dname );
		ofa_dossier_new_piece_get_credentials( priv->new_piece, &priv->account, &priv->password );
	}

	return( TRUE );
}
