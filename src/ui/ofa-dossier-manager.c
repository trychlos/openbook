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
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-idbms.h"
#include "api/ofa-ifile-meta.h"
#include "api/ofa-ifile-period.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-dossier-login.h"
#include "ui/ofa-dossier-manager.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-dossier-store.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierManagerPrivate {

	ofaMainWindow      *main_window;

	/* UI
	 */
	ofaDossierTreeview *tview;
	GtkWidget          *open_btn;
	GtkWidget          *delete_btn;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-manager.ui";
static const gchar *st_ui_id            = "DossierManagerDlg";

G_DEFINE_TYPE( ofaDossierManager, ofa_dossier_manager, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_treeview( ofaDossierManager *self );
static void      on_tview_changed( ofaDossierTreeview *tview, ofaIFileMeta *meta, ofaIFilePeriod *period, ofaDossierManager *self );
static void      on_tview_activated( ofaDossierTreeview *tview, ofaIFileMeta *meta, ofaIFilePeriod *period, ofaDossierManager *self );
static void      on_new_clicked( GtkButton *button, ofaDossierManager *self );
static void      on_open_clicked( GtkButton *button, ofaDossierManager *self );
static void      open_dossier( ofaDossierManager *self, ofaIFileMeta *meta, ofaIFilePeriod *period );
static void      on_delete_clicked( GtkButton *button, ofaDossierManager *self );
static gboolean  confirm_delete( ofaDossierManager *self, const gchar *dname, const gchar *dbname );

static void
dossier_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_manager_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_manager_parent_class )->finalize( instance );
}

static void
dossier_manager_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_manager_parent_class )->dispose( instance );
}

static void
ofa_dossier_manager_init( ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_MANAGER( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_DOSSIER_MANAGER, ofaDossierManagerPrivate );
}

static void
ofa_dossier_manager_class_init( ofaDossierManagerClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_manager_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_manager_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_manager_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;

	g_type_class_add_private( klass, sizeof( ofaDossierManagerPrivate ));
}

/**
 * ofa_dossier_manager_run:
 * @main: the main window of the application.
 *
 * Run the dialog to manage the dossiers
 */
void
ofa_dossier_manager_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_manager_run";
	ofaDossierManager *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
				OFA_TYPE_DOSSIER_MANAGER,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->main_window = main_window;

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierManagerPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *widget;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	priv = OFA_DOSSIER_MANAGER( dialog )->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "new-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_new_clicked ), dialog );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "open-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_open_clicked ), dialog );
	priv->open_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "delete-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_delete_clicked ), dialog );
	priv->delete_btn = widget;

	setup_treeview( OFA_DOSSIER_MANAGER( dialog ));
}

static void
setup_treeview( ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *parent;
	static ofaDossierDispColumn st_columns[] = {
			DOSSIER_DISP_DOSNAME, DOSSIER_DISP_BEGIN, DOSSIER_DISP_END,
			DOSSIER_DISP_STATUS, DOSSIER_DISP_DBNAME, DOSSIER_DISP_PROVNAME,
			0 };

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "tview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->tview = ofa_dossier_treeview_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_dossier_treeview_set_columns( priv->tview, st_columns );
	ofa_dossier_treeview_set_headers( priv->tview, TRUE );
	ofa_dossier_treeview_set_show( priv->tview, DOSSIER_SHOW_ALL );

	g_signal_connect(
			G_OBJECT( priv->tview ), "changed", G_CALLBACK( on_tview_changed ), self );
	g_signal_connect(
			G_OBJECT( priv->tview ), "activated", G_CALLBACK( on_tview_activated ), self );
}

static void
on_tview_changed( ofaDossierTreeview *tview, ofaIFileMeta *meta, ofaIFilePeriod *period, ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = ( meta && period );

	gtk_widget_set_sensitive( priv->open_btn, ok );
	gtk_widget_set_sensitive( priv->delete_btn, ok );
}

static void
on_tview_activated( ofaDossierTreeview *tview, ofaIFileMeta *meta, ofaIFilePeriod *period, ofaDossierManager *self )
{
	if( meta && period ){
		open_dossier( self, meta, period );
	}
}

static void
on_new_clicked( GtkButton *button, ofaDossierManager *self )
{
	GtkApplicationWindow *main_window;
	gboolean dossier_opened;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	dossier_opened = ofa_dossier_new_run( OFA_MAIN_WINDOW( main_window ));

	if( dossier_opened ){
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_CANCEL );
	}
}

static void
on_open_clicked( GtkButton *button, ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	gchar *dname, *dbname;

	priv = self->priv;
	dname = ofa_dossier_treeview_get_selected( priv->tview, DOSSIER_COL_DOSNAME );
	dbname = ofa_dossier_treeview_get_selected( priv->tview, DOSSIER_COL_DBNAME );
#if 0
	open_dossier( self, dname, dbname );
#endif
	g_free( dname );
	g_free( dbname );
}

static void
open_dossier( ofaDossierManager *self, ofaIFileMeta *meta, ofaIFilePeriod *period )
{
#if 0
	ofaDossierManagerPrivate *priv;
	ofsDossierOpen *sdo;
	gchar *account, *password;

	priv = self->priv;

	account = NULL;
	password = NULL;
	ofa_dossier_login_run( priv->main_window, dname, dbname, &account, &password );

	if( my_strlen( account ) && my_strlen( password )){
		sdo = g_new0( ofsDossierOpen, 1 );
		sdo->dname = g_strdup( dname );
		sdo->dbname = g_strdup( dbname );
		sdo->account = account;		/* will be free in the signal cleanup handler */
		sdo->password = password;
		g_signal_emit_by_name(
				priv->main_window, OFA_SIGNAL_DOSSIER_OPEN, sdo );
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_CANCEL );
	} else {
		g_free( account );
		g_free( password );
	}
#endif
}

static void
on_delete_clicked( GtkButton *button, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_on_delete_clicked";
	ofaDossierManagerPrivate *priv;
	ofaDossierStore *store;
	gchar *dname, *dbname;

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	priv = self->priv;
	dname = ofa_dossier_treeview_get_selected( priv->tview, DOSSIER_COL_DOSNAME );
	dbname = ofa_dossier_treeview_get_selected( priv->tview, DOSSIER_COL_DBNAME );

	if( confirm_delete( self, dname, dbname )){
		ofa_settings_remove_exercice( dname, dbname );
		store = ofa_dossier_treeview_get_store( priv->tview );
		ofa_dossier_store_remove_exercice( store, dname, dbname );
	}

	g_free( dname );
}

static gboolean
confirm_delete( ofaDossierManager *self, const gchar *dname, const gchar *dbname )
{
	gboolean ok;
	gchar *str;

	str = g_strdup_printf(
			_( "You are about to remove the '%s' database from the '%s' dossier.\n"
				"This operation will remove the referenced exercice from the settings, "
				"while letting the database itself unchanged.\n"
				"Are your sure ?" ),
					dbname, dname );

	ok = my_utils_dialog_question( str, _( "_Delete" ));

	g_free( str );

	return( ok );
}
