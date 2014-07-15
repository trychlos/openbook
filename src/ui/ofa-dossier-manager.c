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
#include <stdlib.h>

#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-utils.h"
#include "core/ofa-settings.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dbserver-login.h"
#include "ui/ofa-dossier-manager.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierManagerPrivate {

	/* UI
	 */
	GtkTreeView *tview;
	GtkWidget   *delete_btn;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dossier-manager.ui";
static const gchar  *st_ui_id  = "DossierManagerDlg";

/* column ordering in the selection listview
 */
enum {
	COL_NAME = 0,
	COL_PROVIDER,
	COL_HOST,
	COL_PORT,
	COL_SOCKET,
	COL_DBNAME,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaDossierManager, ofa_dossier_manager, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_treeview( ofaDossierManager *self );
static gint      on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void      on_dossier_selected( GtkTreeSelection *selection, ofaDossierManager *self );
static void      on_delete_clicked( GtkButton *button, ofaDossierManager *self );
static gboolean  confirm_delete( ofaDossierManager *self, const gchar *name, const gchar *provider, const gchar *dbname );
static gboolean  do_delete_dossier( ofaDossierManager *self, const gchar *name, const gchar *provider, const gchar *host, const gchar *port_str, const gchar *socket, const gchar *dbname );
static void      do_remove_admin_accounts( ofaDossierManager *self, const ofoSgbd *sgbd, const gchar *dbname );

static void
dossier_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_manager_finalize";
	ofaDossierManagerPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	priv = OFA_DOSSIER_MANAGER( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_manager_parent_class )->finalize( instance );
}

static void
dossier_manager_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

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

	self->private = g_new0( ofaDossierManagerPrivate, 1 );
}

static void
ofa_dossier_manager_class_init( ofaDossierManagerClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_manager_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_manager_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_manager_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
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

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new(
				OFA_TYPE_DOSSIER_MANAGER,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	GtkWindow *toplevel;
	GtkWidget *widget;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "delete-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_delete_clicked ), dialog );
	OFA_DOSSIER_MANAGER( dialog )->private->delete_btn = widget;

	setup_treeview( OFA_DOSSIER_MANAGER( dialog ));
}

static void
setup_treeview( ofaDossierManager *self )
{
	GtkWindow *toplevel;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GSList *dossiers, *id;
	const gchar *name;
	gchar *provider, *host, *socket, *dbname, *port_str;
	gint port;

	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	self->private->tview = GTK_TREE_VIEW( tview );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Dossier" ),
			cell,
			"text", COL_NAME,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Provider" ),
			cell,
			"text", COL_PROVIDER,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Host" ),
			cell,
			"text", COL_HOST,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	/*cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Port" ),
			cell,
			"text", COL_PORT,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Socket" ),
			cell,
			"text", COL_SOCKET,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );*/

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "DB name" ),
			cell,
			"text", COL_DBNAME,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(G_OBJECT( select ), "changed", G_CALLBACK( on_dossier_selected ), self );

	dossiers = ofa_settings_get_dossiers();

	for( id=dossiers ; id ; id=id->next ){
		name = ( const gchar * ) id->data;
		ofa_settings_get_dossier( name, &provider, &host, &port, &socket, &dbname );
		if( port > 0 ){
			port_str = g_strdup_printf( "%u", port );
		} else {
			port_str = g_strdup( "" );
		}

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_NAME,     name,
				COL_PROVIDER, provider,
				COL_HOST,     host,
				COL_PORT,     port_str,
				COL_SOCKET,   socket,
				COL_DBNAME,   dbname,
				-1 );

		g_free( dbname );
		g_free( socket );
		g_free( port_str );
		g_free( host );
		g_free( provider );
	}

	g_slist_free_full( dossiers, ( GDestroyNotify ) g_free );

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		gtk_tree_selection_select_iter( select, &iter );
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_NAME, &aname, -1 );
	gtk_tree_model_get( tmodel, b, COL_NAME, &bname, -1 );

	cmp = g_utf8_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
on_dossier_selected( GtkTreeSelection *selection, ofaDossierManager *self )
{
	gboolean ok;

	ok = gtk_tree_selection_get_selected( selection, NULL, NULL );

	gtk_widget_set_sensitive( self->private->delete_btn, ok );
}

static void
on_delete_clicked( GtkButton *button, ofaDossierManager *self )
{
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gchar *name, *provider, *host, *port_str, *socket, *dbname;

	select = gtk_tree_view_get_selection( self->private->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get(
				tmodel,
				&iter,
				COL_NAME,     &name,
				COL_PROVIDER, &provider,
				COL_HOST,     &host,
				COL_PORT,     &port_str,
				COL_SOCKET,   &socket,
				COL_DBNAME,   &dbname,
				-1 );

		if( confirm_delete( self, name, provider, dbname )){
			if( do_delete_dossier( self, name, provider, host, port_str, socket, dbname )){
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
			}
		}

		g_free( dbname );
		g_free( socket );
		g_free( port_str );
		g_free( host );
		g_free( provider );
		g_free( name );
	}
}

static gboolean
confirm_delete( ofaDossierManager *self, const gchar *name, const gchar *provider, const gchar *dbname )
{
	GtkWidget *dialog;
	gint response;
	gchar *str;

	str = g_strdup_printf(
			_( "You are about to remove the '%s' dossier (provider=%s, dbname=%s)\n"
				"Are your sure ?" ),
					name, provider, dbname );

	dialog = gtk_message_dialog_new(
			my_window_get_toplevel( MY_WINDOW( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", str );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_DELETE, GTK_RESPONSE_OK,
			NULL );

	g_free( str );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static gboolean
do_delete_dossier( ofaDossierManager *self, const gchar *name, const gchar *provider, const gchar *host, const gchar *port_str, const gchar *socket, const gchar *dbname )
{
	gboolean ok;
	gboolean remove_account;
	gchar *account, *password;
	ofoSgbd *sgbd;
	gint port;
	gchar *query;

	ok = FALSE;

	if( ofa_dbserver_login_run(
			MY_WINDOW( self )->protected->main_window,
			name, provider, host, port_str, socket, dbname, &account, &password, &remove_account )){

		sgbd = ofo_sgbd_new( provider );

		port = 0;
		if( port_str && g_utf8_strlen( port_str, -1 )){
			port = atoi( port_str );
		}

		if( ofo_sgbd_connect( sgbd, host, port, socket, "mysql", account, password )){

			if( remove_account ){
				do_remove_admin_accounts( self, sgbd, dbname );
			}

			query = g_strdup_printf( "DROP DATABASE %s", dbname );
			ofo_sgbd_query_ignore( sgbd, query );
			g_free( query );

			ofa_settings_remove_dossier( name );
		}

		g_object_unref( sgbd );
		g_free( account );
		g_free( password );

		ok = TRUE;
	}

	return( ok );
}

static void
do_remove_admin_accounts( ofaDossierManager *self, const ofoSgbd *sgbd, const gchar *dbname )
{
	gchar *query;
	GSList *result, *irow, *icol;
	const gchar *user;

	query = g_strdup_printf(
				"SELECT ROL_USER FROM %s.OFA_T_ROLES WHERE ROL_IS_ADMIN=1", dbname );
	result = ofo_sgbd_query_ex_ignore( sgbd, query );
	g_free( query );

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		user = ( const gchar * ) icol->data;
		query = g_strdup_printf( "delete from mysql.user where user='%s'", user );
		ofo_sgbd_query_ignore( sgbd, query );
		g_free( query );
	}
}
