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

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-dossier-manager.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierManagerPrivate {

	/* UI
	 */
	GtkTreeView *tview;
	GtkWidget   *open_btn;
	GtkWidget   *update_btn;
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
	COL_DBNAME,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaDossierManager, ofa_dossier_manager, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_treeview( ofaDossierManager *self );
static void      load_in_treeview( ofaDossierManager *self );
static gint      on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void      on_dossier_selected( GtkTreeSelection *selection, ofaDossierManager *self );
static void      on_new_clicked( GtkButton *button, ofaDossierManager *self );
static void      on_open_clicked( GtkButton *button, ofaDossierManager *self );
static void      on_update_clicked( GtkButton *button, ofaDossierManager *self );
static void      on_delete_clicked( GtkButton *button, ofaDossierManager *self );
static gboolean  confirm_delete( ofaDossierManager *self, const gchar *name, const gchar *provider, const gchar *dbname );

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
	ofaDossierManagerPrivate *priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	priv = OFA_DOSSIER_MANAGER( dialog )->private;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "new-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_new_clicked ), dialog );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "open-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_open_clicked ), dialog );
	priv->open_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "update-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_update_clicked ), dialog );
	priv->update_btn = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "delete-btn" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_delete_clicked ), dialog );
	priv->delete_btn = widget;

	setup_treeview( OFA_DOSSIER_MANAGER( dialog ));
	load_in_treeview( OFA_DOSSIER_MANAGER( dialog ));
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

	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	self->private->tview = GTK_TREE_VIEW( tview );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING ));
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
}

static void
load_in_treeview( ofaDossierManager *self )
{
	GtkTreeModel *tmodel;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GSList *dossiers, *id;
	const gchar *name;
	gchar *provider, *host, *dbname;
	ofaIDbms *module;

	tmodel = gtk_tree_view_get_model( self->private->tview );
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	dossiers = ofa_settings_get_dossiers();

	for( id=dossiers ; id ; id=id->next ){
		name = ( const gchar * ) id->data;
		provider = ofa_settings_get_dossier_provider( name );
		module = ofa_idbms_get_provider_by_name( provider );
		host = ofa_idbms_get_dossier_host( module, name );
		dbname = ofa_idbms_get_dossier_dbname( module, name );
		g_object_unref( module );

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_NAME,     name,
				COL_PROVIDER, provider,
				COL_HOST,     host,
				COL_DBNAME,   dbname,
				-1 );

		g_free( dbname );
		g_free( host );
		g_free( provider );
	}

	g_slist_free_full( dossiers, ( GDestroyNotify ) g_free );

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( self->private->tview );
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

	gtk_widget_set_sensitive( self->private->open_btn, ok );
	gtk_widget_set_sensitive( self->private->update_btn, ok );
	gtk_widget_set_sensitive( self->private->delete_btn, ok );
}

static void
on_new_clicked( GtkButton *button, ofaDossierManager *self )
{
	gboolean dossier_opened;

	dossier_opened = ofa_dossier_new_run( MY_WINDOW( self )->protected->main_window );

	if( dossier_opened ){
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_CANCEL );

	} else {
		load_in_treeview( self );
	}
}

static void
on_open_clicked( GtkButton *button, ofaDossierManager *self )
{
	ofsDossierOpen *sdo;

	sdo = ofa_dossier_open_run(
			MY_WINDOW( self )->protected->main_window );

	if( sdo ){
		g_signal_emit_by_name(
				MY_WINDOW( self )->protected->main_window, OFA_SIGNAL_OPEN_DOSSIER, sdo );
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_CANCEL );
	}
}

static void
on_update_clicked( GtkButton *button, ofaDossierManager *self )
{
}

static void
on_delete_clicked( GtkButton *button, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_on_delete_clicked";
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gchar *name, *provider, *host, *dbname;
	gboolean deleted;

	select = gtk_tree_view_get_selection( self->private->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get(
				tmodel,
				&iter,
				COL_NAME,     &name,
				COL_PROVIDER, &provider,
				COL_HOST,     &host,
				COL_DBNAME,   &dbname,
				-1 );

		if( confirm_delete( self, name, provider, dbname )){
			deleted = ofa_dossier_delete_run(
							MY_WINDOW( self )->protected->main_window,
							name, provider, host, dbname );
			g_debug( "%s: deleted=%s", thisfn, deleted ? "True":"False" );
			if( deleted ){
				gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
			}
		}

		g_free( dbname );
		g_free( host );
		g_free( provider );
		g_free( name );

	} else {
		g_warning( "%s: no current selection", thisfn );
	}
}

static gboolean
confirm_delete( ofaDossierManager *self, const gchar *name, const gchar *provider, const gchar *dbname )
{
	GtkWidget *dialog;
	gint response;
	gchar *str;

	str = g_strdup_printf(
			_( "You are about to remove the '%s' dossier (provider=%s, dbname=%s).\n"
				"This operation will not be recoverable.\n"
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
