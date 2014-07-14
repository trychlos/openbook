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

#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-utils.h"
#include "core/ofa-settings.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewPrivate {

	/* p1: SGDB Provider
	 */
	gchar         *p1_sgdb_provider;

	/* p2: MySQL as SGBD provider
	 */
	gchar         *p2_host;
	guint          p2_port;
	gchar         *p2_socket;
	gchar         *p2_account;
	gchar         *p2_password;

	gboolean       p2_successful;

	/* p3: dossier properties
	 */
	gchar         *p3_dbname;
	gchar         *p3_label;
	gchar         *p3_account;
	gchar         *p3_password;
	gchar         *p3_bis;
	gboolean       p3_open_dossier;
	gboolean       p3_update_properties;

	gboolean       p3_are_equals;

	/* UI
	 */
	GtkWidget     *p2_check_btn;
	GtkWidget     *p3_browse_btn;
	GtkWidget     *p3_properties_btn;
	GtkLabel      *msg_label;
	GtkWidget     *apply_btn;
};

/* columns in SGDB provider combo box
 */
enum {
	SGDB_COL_PROVIDER = 0,
	SGDB_N_COLUMNS
};

static const gchar *st_ui_xml   = PKGUIDIR "/ofa-dossier-new.ui";
static const gchar *st_ui_id    = "DossierNewDlg";
static const gchar *st_ui_mysql = "MySQLWindow";

G_DEFINE_TYPE( ofaDossierNew, ofa_dossier_new, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_p1_sgdb_provider( ofaDossierNew *self );
static void      on_sgdb_provider_changed( GtkComboBox *combo, ofaDossierNew *self );
static void      setup_mysql_provider( ofaDossierNew *self );
static void      on_mysql_host_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_mysql_port_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_mysql_socket_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_mysql_admin_account_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_mysql_admin_password_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_mysql_check_clicked( GtkButton *button, ofaDossierNew *self );
static gboolean  do_mysql_check( ofaDossierNew *self );
static gboolean  check_for_mysql_provider( ofaDossierNew *self );
static void      check_for_enable_connectivity_check( ofaDossierNew *self );
static void      init_p3_dossier_properties( ofaDossierNew *self );
static void      on_db_name_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_db_find_clicked( GtkButton *button, ofaDossierNew *self );
static void      on_db_label_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_db_account_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_db_password_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_db_bis_changed( GtkEntry *entry, ofaDossierNew *self );
static gboolean  db_passwords_are_equals( ofaDossierNew *self );
static void      on_db_open_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      on_db_properties_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      set_message( ofaDossierNew *self, const gchar *msg );
static void      check_for_enable_dlg( ofaDossierNew *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_apply( ofaDossierNew *self );
static gboolean  make_db_global( ofaDossierNew *self );
static gboolean  setup_new_dossier( ofaDossierNew *self );
static gboolean  create_db_model( ofaDossierNew *self );

static void
dossier_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_finalize";
	ofaDossierNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	priv = OFA_DOSSIER_NEW( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->p1_sgdb_provider );

	g_free( priv->p2_host );
	g_free( priv->p2_socket );
	g_free( priv->p2_account );
	g_free( priv->p2_password );

	g_free( priv->p3_dbname );
	g_free( priv->p3_label );
	g_free( priv->p3_account );
	g_free( priv->p3_password );
	g_free( priv->p3_bis );

	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->finalize( instance );
}

static void
dossier_new_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_init( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_init";

	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaDossierNewPrivate, 1 );
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
}

/**
 * ofa_dossier_new_run:
 * @main: the main window of the application.
 */
ofaOpenDossier *
ofa_dossier_new_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_new_new";
	ofaDossierNew *self;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );

	return( NULL );
}

/*
 * the provided 'page' is the toplevel widget of the asistant's page
 */
static void
v_init_dialog( myDialog *dialog )
{
	init_p3_dossier_properties( OFA_DOSSIER_NEW( dialog ));
	init_p1_sgdb_provider( OFA_DOSSIER_NEW( dialog ));
}

static void
init_p1_sgdb_provider( ofaDossierNew *self )
{
	GtkContainer *container;
	GtkComboBox *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	combo = ( GtkComboBox * ) my_utils_container_get_child_by_name( container, "p1-provider" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			SGDB_N_COLUMNS,
			G_TYPE_STRING ));
	gtk_combo_box_set_model( combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SGDB_COL_PROVIDER );

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			SGDB_COL_PROVIDER, SGBD_PROVIDER_MYSQL,
			-1 );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_sgdb_provider_changed ), self );

	gtk_combo_box_set_active( combo, 0 );
}

static void
on_sgdb_provider_changed( GtkComboBox *combo, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_sgdb_provider_changed";
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gchar *provider;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) self );

	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter,
				SGDB_COL_PROVIDER, &provider,
				-1 );
		if( !g_utf8_collate( provider, SGBD_PROVIDER_MYSQL )){
			setup_mysql_provider( self );
		}
		g_free( provider );
	}
}

static void
setup_mysql_provider( ofaDossierNew *self )
{
	GtkContainer *container;
	GtkWidget *parent;
	GtkWidget *child;
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *entry, *button;
	gchar *value;
	gint ivalue;
	GtkSizeGroup *group;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	parent = my_utils_container_get_child_by_name( container, "sgdb-container" );
	g_return_if_fail( parent && GTK_IS_BIN( parent ));

	/* remove previous child (if any) */
	child = gtk_bin_get_child( GTK_BIN( parent ));
	if( child ){
		gtk_container_remove( GTK_CONTAINER( parent ), child );
	}

	/* have a size group */
	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
	grid = my_utils_container_get_child_by_name( container, "p1-provider-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));
	gtk_size_group_add_widget( group, grid );

	/* attach our sgdb provider grid */
	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_mysql );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));
	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "mysql-properties" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	gtk_widget_reparent( grid, parent );
	gtk_size_group_add_widget( group, grid );
	g_object_unref( group );

	/* init the grid */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "p2-check" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_mysql_check_clicked ), self );
	self->private->p2_check_btn = button;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "p2-host" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mysql_host_changed ), self );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-host" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "p2-port" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mysql_port_changed ), self );
	ivalue = ofa_settings_get_uint( "DossierNewDlg-MySQL-port" );
	if( ivalue > 0 ){
		value = g_strdup_printf( "%u", ivalue );
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
		g_free( value );
	}

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "p2-socket" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mysql_socket_changed ), self );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-socket" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "p2-account" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mysql_admin_account_changed ), self );
	value = ofa_settings_get_string( "DossierNewDlg-MySQL-account" );
	if( value && g_utf8_strlen( value, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), value );
	}
	g_free( value );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( grid ), "p2-password" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mysql_admin_password_changed ), self );
}

static void
on_mysql_host_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *host;

	priv = self->private;

	host = gtk_entry_get_text( entry );
	g_free( priv->p2_host );
	priv->p2_host = g_strdup( host );

	priv->p2_successful = FALSE;
	check_for_enable_connectivity_check( self );
	check_for_enable_dlg( self );
}

static void
on_mysql_port_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *port;

	priv = self->private;

	port = gtk_entry_get_text( entry );
	if( port && g_utf8_strlen( port, -1 )){
		priv->p2_port = atoi( port );
	} else {
		priv->p2_port = 0;
	}

	priv->p2_successful = FALSE;
	check_for_enable_connectivity_check( self );
	check_for_enable_dlg( self );
}

static void
on_mysql_socket_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *socket;

	priv = self->private;

	socket = gtk_entry_get_text( entry );
	g_free( priv->p2_socket );
	priv->p2_socket = g_strdup( socket );

	priv->p2_successful = FALSE;
	check_for_enable_connectivity_check( self );
	check_for_enable_dlg( self );
}

static void
on_mysql_admin_account_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *account;

	priv = self->private;

	account = gtk_entry_get_text( entry );
	g_free( priv->p2_account );
	priv->p2_account = g_strdup( account );

	priv->p2_successful = FALSE;
	check_for_enable_connectivity_check( self );
	check_for_enable_dlg( self );
}

static void
on_mysql_admin_password_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *password;

	priv = self->private;

	password = gtk_entry_get_text( entry );
	g_free( priv->p2_password );
	priv->p2_password = g_strdup( password );

	priv->p2_successful = FALSE;
	check_for_enable_connectivity_check( self );
	check_for_enable_dlg( self );
}

static void
on_mysql_check_clicked( GtkButton *button, ofaDossierNew *self )
{
	check_for_mysql_provider( self );
	check_for_enable_connectivity_check( self );
}

static gboolean
do_mysql_check( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean connect_ok;
	ofoSgbd *sgbd;

	priv = self->private;
	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	connect_ok = ofo_sgbd_connect(
					sgbd,
					priv->p2_host,
					priv->p2_port,
					priv->p2_socket,
					NULL,
					priv->p2_account,
					priv->p2_password );

	g_object_unref( sgbd );

	return( connect_ok );
}

/*
 * MySQL provides a suitable default value for all fields,
 * so all combinations are valid here
 */
static gboolean
check_for_mysql_provider( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->private;

	if( !priv->p2_successful ){
		priv->p2_successful = do_mysql_check( self );
		if( !priv->p2_successful ){
			set_message( self, _( "Unsuccessful connect to DB server" ));
		} else {
			set_message( self, NULL );
		}
	}

	return( priv->p2_successful );
}

static void
check_for_enable_connectivity_check( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->private;

	g_return_if_fail( priv->p2_check_btn && GTK_IS_BUTTON( priv->p2_check_btn ));
	gtk_widget_set_sensitive( priv->p2_check_btn, !priv->p2_successful );

	if( priv->p2_successful ){
		gtk_button_set_label( GTK_BUTTON( priv->p2_check_btn ), _( "Connection is OK" ));
	} else {
		gtk_button_set_label( GTK_BUTTON( priv->p2_check_btn ), _( "_Connectivity check" ));
	}

	g_return_if_fail( priv->p3_browse_btn && GTK_IS_BUTTON( priv->p3_browse_btn ));
	gtk_widget_set_sensitive( priv->p3_browse_btn, priv->p2_successful );
}

static void
init_p3_dossier_properties( ofaDossierNew *self )
{
	GtkContainer *container;
	GtkWidget *widget;
	gboolean value;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	widget = my_utils_container_get_child_by_name( container, "p3-label" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_label_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-name" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_name_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-find" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_db_find_clicked ), self );
	self->private->p3_browse_btn = widget;

	widget = my_utils_container_get_child_by_name( container, "p3-account" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_account_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-password" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_password_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-bis" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_bis_changed ), self );

	/* before p3-open so that the later may update the former */
	widget = my_utils_container_get_child_by_name( container, "p3-properties" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_db_properties_toggled ), self );
	value = ofa_settings_get_boolean( "DossierNewDlg-properties" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), value );
	self->private->p3_properties_btn = widget;

	widget = my_utils_container_get_child_by_name( container, "p3-open" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_db_open_toggled ), self );
	/* force a signal to be triggered */
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), TRUE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), FALSE );
	value = ofa_settings_get_boolean( "DossierNewDlg-opendossier" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), value );
}

static void
on_db_name_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *name;

	priv = self->private;

	name = gtk_entry_get_text( entry );
	g_free( priv->p3_dbname );
	priv->p3_dbname = g_strdup( name );

	check_for_enable_dlg( self );
}

static void
on_db_find_clicked( GtkButton *button, ofaDossierNew *self )
{

}

static void
on_db_label_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *label;

	priv = self->private;

	label = gtk_entry_get_text( entry );
	g_free( priv->p3_label );
	priv->p3_label = g_strdup( label );

	check_for_enable_dlg( self );
}

static void
on_db_account_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *account;

	priv = self->private;

	account = gtk_entry_get_text( entry );
	g_free( priv->p3_account );
	priv->p3_account = g_strdup( account );

	check_for_enable_dlg( self );
}

static void
on_db_password_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *password;

	priv = self->private;

	password = gtk_entry_get_text( entry );
	g_free( priv->p3_password );
	priv->p3_password = g_strdup( password );

	priv->p3_are_equals = db_passwords_are_equals( self );
	check_for_enable_dlg( self );
}

static void
on_db_bis_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *bis;

	priv = self->private;

	bis = gtk_entry_get_text( entry );
	g_free( priv->p3_bis );
	priv->p3_bis = g_strdup( bis );

	priv->p3_are_equals = db_passwords_are_equals( self );
	check_for_enable_dlg( self );
}

static gboolean
db_passwords_are_equals( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean are_equals;

	priv = self->private;

	are_equals = (( !priv->p3_password && !priv->p3_bis ) ||
				( priv->p3_password && g_utf8_strlen( priv->p3_password, -1 ) &&
					priv->p3_bis && g_utf8_strlen( priv->p3_bis, -1 ) &&
					!g_utf8_collate( priv->p3_password, priv->p3_bis )));

	return( are_equals );
}

static void
on_db_open_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->private;

	priv->p3_open_dossier = gtk_toggle_button_get_active( button );

	g_return_if_fail( priv->p3_properties_btn && GTK_IS_CHECK_BUTTON( priv->p3_properties_btn ));
	gtk_widget_set_sensitive( priv->p3_properties_btn, priv->p3_open_dossier );
}

static void
on_db_properties_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	self->private->p3_update_properties = gtk_toggle_button_get_active( button );
}

static void
set_message( ofaDossierNew *self, const gchar *msg )
{
	ofaDossierNewPrivate *priv;
	GdkRGBA color;

	priv = self->private;

	if( !priv->msg_label ){
		priv->msg_label = ( GtkLabel * )
								my_utils_container_get_child_by_name(
										GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
										"px-msg" );
	}

	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));

	if( msg && g_utf8_strlen( msg, -1 )){
		gtk_label_set_text( priv->msg_label, msg );
		if( gdk_rgba_parse( &color, "#FF0000" )){
			gtk_widget_override_color( GTK_WIDGET( priv->msg_label ), GTK_STATE_FLAG_NORMAL, &color );
		}
	} else {
		gtk_label_set_text( priv->msg_label, "" );
	}
}

static void
check_for_enable_dlg( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean enabled;

	priv = self->private;

	enabled = priv->p2_successful &&
				priv->p3_account && g_utf8_strlen( priv->p3_account, -1 ) &&
				priv->p3_password &&
				priv->p3_bis &&
				priv->p3_are_equals;

	if( !priv->apply_btn ){
		priv->apply_btn = my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
									"btn-ok" );
	}

	g_return_if_fail( priv->apply_btn && GTK_IS_BUTTON( priv->apply_btn ));

	gtk_widget_set_sensitive( priv->apply_btn, enabled );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;

	ok = do_apply( OFA_DOSSIER_NEW( dialog ));

	if( ok ){
		priv = OFA_DOSSIER_NEW( dialog )->private;
		if( priv->p2_host && g_utf8_strlen( priv->p2_host, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-host", priv->p2_host );
		}
		if( priv->p2_port > 0 ){
			ofa_settings_set_uint( "DossierNewDlg-MySQL-port", priv->p2_port );
		}
		if( priv->p2_socket && g_utf8_strlen( priv->p2_socket, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-socket", priv->p2_socket );
		}
		if( priv->p2_account && g_utf8_strlen( priv->p2_account, -1 )){
			ofa_settings_set_string( "DossierNewDlg-MySQL-account", priv->p2_account );
		}
		ofa_settings_set_boolean( "DossierNewDlg-opendossier", priv->p3_open_dossier );
		ofa_settings_set_boolean( "DossierNewDlg-properties", priv->p3_update_properties );
	}

	return( ok );
}

static gboolean
do_apply( ofaDossierNew *self )
{
	gboolean apply_ok;
	ofaOpenDossier *ood;

	apply_ok = make_db_global( self ) &&
				setup_new_dossier( self ) &&
				create_db_model( self );

	if( apply_ok ){
		if( self->private->p3_open_dossier ){

			ood = g_new0( ofaOpenDossier, 1 );
			ood->dossier = g_strdup( self->private->p3_label );
			ood->host = g_strdup( self->private->p2_host );
			ood->port = self->private->p2_port;
			ood->socket = g_strdup( self->private->p2_socket );
			ood->dbname = g_strdup( self->private->p3_dbname );
			ood->account = g_strdup( self->private->p3_account );
			ood->password = g_strdup( self->private->p3_password );

			g_signal_emit_by_name(
					MY_WINDOW( self )->protected->main_window,
					OFA_SIGNAL_OPEN_DOSSIER, ood );

			if( self->private->p3_update_properties ){
				g_signal_emit_by_name(
						MY_WINDOW( self )->protected->main_window,
						OFA_SIGNAL_UPDATE_PROPERTIES );
			}
		}
	}

	return( apply_ok );
}

/*
 * Create the empty database with a global connection to dataserver
 * - create database
 * - create admin user
 * - grant admin user
 */
static gboolean
make_db_global( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_make_db_global";
	ofoSgbd *sgbd;
	GString *stmt;
	gboolean db_created;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	db_created = FALSE;
	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofo_sgbd_connect( sgbd,
			self->private->p2_host,
			self->private->p2_port,
			self->private->p2_socket,
			"mysql",
			self->private->p2_account,
			self->private->p2_password )){

		return( FALSE );
	}

	stmt = g_string_new( "" );

	g_string_printf( stmt,
			"CREATE DATABASE %s",
			self->private->p3_dbname );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS OFA_T_AUDIT ("
			"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
			"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query actual timestamp',"
			"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query')" );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE USER '%s' IDENTIFIED BY '%s'",
			self->private->p3_account, self->private->p3_password );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE USER '%s'@'localhost' IDENTIFIED BY '%s'",
			self->private->p3_account, self->private->p3_password );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s' WITH GRANT OPTION",
			self->private->p3_dbname,
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s'@'localhost' WITH GRANT OPTION",
			self->private->p3_dbname,
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'",
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'@'localhost'",
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		goto free_stmt;
	}

	db_created = TRUE;

free_stmt:
	g_string_free( stmt, TRUE );
	g_object_unref( sgbd );

	return( db_created );
}

static gboolean
setup_new_dossier( ofaDossierNew *self )
{
	return(
		ofa_settings_set_dossier(
			self->private->p3_label,
			"Provider",    SETTINGS_TYPE_STRING, "MySQL",
			"Host",        SETTINGS_TYPE_STRING, self->private->p2_host,
			"Port",        SETTINGS_TYPE_INT,    self->private->p2_port,
			"Socket",      SETTINGS_TYPE_STRING, self->private->p2_socket,
			"Database",    SETTINGS_TYPE_STRING, self->private->p3_dbname,
			NULL ));
}

static gboolean
create_db_model( ofaDossierNew *self )
{
	ofoSgbd *sgbd;
	gboolean model_created;

	model_created = FALSE;
	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofo_sgbd_connect( sgbd,
			self->private->p2_host,
			self->private->p2_port,
			self->private->p2_socket,
			self->private->p3_dbname,
			self->private->p3_account,
			self->private->p3_password )){

		goto free_sgbd;
	}

	if( !ofo_dossier_dbmodel_update( sgbd, self->private->p3_label, self->private->p3_account )){
		goto free_sgbd;
	}

	model_created = TRUE;

free_sgbd:
	g_object_unref( sgbd );

	return( model_created );
}
