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

#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-exercice-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierOpenPrivate {

	/* data
	 */
	gchar              *dname;			/* name of the dossier (from settings) */
	gchar              *label;			/* label of the exercice (from settings) */
	char               *dbname;
	gchar              *account;
	gchar              *password;

	/* UI
	 */
	ofaDossierTreeview *dossier_tview;
	ofaExerciceCombo   *exercice_combo;
	GtkWidget          *ok_btn;

	/* return value
	 * the structure itself, along with its datas, will be freed by the
	 * MainWindow signal final handler
	 */
	ofsDossierOpen     *sdo;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dossier-open.ui";
static const gchar  *st_ui_id  = "DossierOpenDlg";

G_DEFINE_TYPE( ofaDossierOpen, ofa_dossier_open, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_dossier_changed( ofaDossierTreeview *tview, const gchar *name, ofaDossierOpen *self );
static void      on_exercice_changed( ofaExerciceCombo *combo, const gchar *label, const gchar *dbname, ofaDossierOpen *self );
static void      on_account_changed( GtkEntry *entry, ofaDossierOpen *self );
static void      on_password_changed( GtkEntry *entry, ofaDossierOpen *self );
static void      check_for_enable_dlg( ofaDossierOpen *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_open( ofaDossierOpen *self );

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

	g_free( priv->dbname );
	g_free( priv->label );
	g_free( priv->dname );
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_open_parent_class )->finalize( instance );
}

static void
dossier_open_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_OPEN( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
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
 * @main: the main window of the application.
 *
 * Run the selection dialog to choose a dossier to be opened
 */
ofsDossierOpen *
ofa_dossier_open_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_open_run";
	ofaDossierOpen *self;
	ofsDossierOpen *sdo;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new(
				OFA_TYPE_DOSSIER_OPEN,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	sdo = self->priv->sdo;
	g_object_unref( self );

	return( sdo );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierOpenPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *container, *entry, *button, *frame;

	priv = OFA_DOSSIER_OPEN( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	/* do this first to be available as soon as the first signal
	 * triggers */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-open" );
	priv->ok_btn = button;

	/* setup exercice combobox (before dossier) */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "parent-exercice" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	priv->exercice_combo = ofa_exercice_combo_new();
	ofa_exercice_combo_attach_to( priv->exercice_combo, GTK_CONTAINER( container ));
	g_signal_connect(
			G_OBJECT( priv->exercice_combo ), "changed", G_CALLBACK( on_exercice_changed ), dialog );

	/* setup dossier treeview */
	container = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "parent-dossier" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );
	gtk_container_add( GTK_CONTAINER( container ), frame );

	priv->dossier_tview = ofa_dossier_treeview_new();
	ofa_dossier_istore_attach_to(
			OFA_DOSSIER_ISTORE( priv->dossier_tview ), GTK_CONTAINER( frame ));
	ofa_dossier_istore_set_columns(
			OFA_DOSSIER_ISTORE( priv->dossier_tview ), DOSSIER_COL_DNAME );
	g_signal_connect(
			G_OBJECT( priv->dossier_tview ), "changed", G_CALLBACK( on_dossier_changed ), dialog );

	/* setup account and password */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "account" );
	g_signal_connect(G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), dialog );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "password" );
	g_signal_connect(G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), dialog );

	check_for_enable_dlg( OFA_DOSSIER_OPEN( dialog ));
}

static void
on_dossier_changed( ofaDossierTreeview *tview, const gchar *name, ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_on_dossier_changed";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: tview=%p, name=%s, self=%p", thisfn, ( void * ) tview, name, ( void * ) self );

	priv = self->priv;

	g_free( priv->dname );
	priv->dname = g_strdup( name );

	ofa_exercice_combo_init_view( priv->exercice_combo, name );

	check_for_enable_dlg( self );
}

static void
on_exercice_changed( ofaExerciceCombo *combo, const gchar *label, const gchar *dbname, ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_exercice_combo_on_exercice_changed";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: combo=%p, label=%s, dbname=%s, self=%p",
			thisfn, ( void * ) combo, label, dbname, ( void * ) self );

	priv = self->priv;

	g_free( priv->label );
	priv->label = g_strdup( label );
	g_free( priv->dbname );
	priv->dbname = g_strdup( dbname );

	check_for_enable_dlg( self );
}

static void
on_account_changed( GtkEntry *entry, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEntry *entry, ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;

	priv = self->priv;

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;
	gboolean ok_enable;
	ofaDbms *dbms;

	priv = self->priv;

	ok_enable = priv->dname && g_utf8_strlen( priv->dname, -1 ) &&
				priv->label && g_utf8_strlen( priv->label, -1 ) &&
				priv->dbname && g_utf8_strlen( priv->dbname, -1 ) &&
				priv->account && g_utf8_strlen( priv->account, -1 ) &&
				priv->password && g_utf8_strlen( priv->password, -1 );

	if( ok_enable ){
		dbms = ofa_dbms_new();
		ok_enable &= ofa_dbms_connect( dbms, priv->dname, priv->dbname, priv->account, priv->password, FALSE );
		g_object_unref( dbms );
	}

	gtk_widget_set_sensitive( priv->ok_btn, ok_enable );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_open( OFA_DOSSIER_OPEN( dialog )));
}

/*
 * is called when the user click on the 'Open' button
 * return %TRUE if we can open a connection, %FALSE else
 */
static gboolean
do_open( ofaDossierOpen *self )
{
	ofaDossierOpenPrivate *priv;
	ofsDossierOpen *sdo;

	priv = self->priv;

	sdo = g_new0( ofsDossierOpen, 1 );
	sdo->dname = g_strdup( priv->dname );
	sdo->dbname = g_strdup( priv->dbname );
	sdo->account = g_strdup( priv->account );
	sdo->password = g_strdup( priv->password );
	priv->sdo = sdo;

	return( TRUE );
}
