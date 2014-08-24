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

#include "core/ofa-plugin.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-plugin-manager.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaPluginManagerPrivate {

	/* UI
	 */
	GtkTreeView *tview;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-plugin-manager.ui";
static const gchar  *st_ui_id  = "PluginManagerDlg";

/* column ordering in the selection listview
 */
enum {
	COL_NAME = 0,
	COL_VERSION,
	COL_PLUGIN,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaPluginManager, ofa_plugin_manager, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_treeview( ofaPluginManager *self );
static void      load_in_treeview( ofaPluginManager *self );
static gint      on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void      on_plugin_selected( GtkTreeSelection *selection, ofaPluginManager *self );
static void      on_properties_clicked( GtkButton *button, ofaPluginManager *self );

static void
plugin_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_plugin_manager_finalize";
	ofaPluginManagerPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PLUGIN_MANAGER( instance ));

	priv = OFA_PLUGIN_MANAGER( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_plugin_manager_parent_class )->finalize( instance );
}

static void
plugin_manager_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PLUGIN_MANAGER( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_plugin_manager_parent_class )->dispose( instance );
}

static void
ofa_plugin_manager_init( ofaPluginManager *self )
{
	static const gchar *thisfn = "ofa_plugin_manager_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PLUGIN_MANAGER( self ));

	self->private = g_new0( ofaPluginManagerPrivate, 1 );
}

static void
ofa_plugin_manager_class_init( ofaPluginManagerClass *klass )
{
	static const gchar *thisfn = "ofa_plugin_manager_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = plugin_manager_dispose;
	G_OBJECT_CLASS( klass )->finalize = plugin_manager_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
}

/**
 * ofa_plugin_manager_run:
 * @main: the main window of the application.
 *
 * Run the dialog to manage the dossiers
 */
void
ofa_plugin_manager_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_plugin_manager_run";
	ofaPluginManager *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new(
				OFA_TYPE_PLUGIN_MANAGER,
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
	GtkWidget *button;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "properties-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_properties_clicked ), dialog );

	setup_treeview( OFA_PLUGIN_MANAGER( dialog ));
	load_in_treeview( OFA_PLUGIN_MANAGER( dialog ));
}

static void
setup_treeview( ofaPluginManager *self )
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
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER ));
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
			_( "Plugin" ),
			cell,
			"text", COL_NAME,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Version" ),
			cell,
			"text", COL_VERSION,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(G_OBJECT( select ), "changed", G_CALLBACK( on_plugin_selected ), self );
}

static void
load_in_treeview( ofaPluginManager *self )
{
	GtkTreeModel *tmodel;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	const GList *modules;
	const GList *it;
	ofaPlugin *plugin;
	const gchar *name, *version;

	tmodel = gtk_tree_view_get_model( self->private->tview );
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	modules = ofa_plugin_get_modules();

	for( it=modules ; it ; it=it->next ){
		plugin = OFA_PLUGIN( it->data );
		name = ofa_plugin_get_name( plugin );
		version = ofa_plugin_get_version_number( plugin );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_NAME,    name,
				COL_VERSION, version,
				COL_PLUGIN,  plugin,
				-1 );
	}

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
on_plugin_selected( GtkTreeSelection *selection, ofaPluginManager *self )
{
}

static void
on_properties_clicked( GtkButton *button, ofaPluginManager *self )
{
	static const gchar *thisfn = "ofa_plugin_manager_on_properties_clicked";
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gchar *name;
	ofaPlugin *plugin;

	select = gtk_tree_view_get_selection( self->private->tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){
		gtk_tree_model_get(
				tmodel,
				&iter,
				COL_NAME,   &name,
				COL_PLUGIN, &plugin,
				-1 );

		g_free( name );

	} else {
		g_warning( "%s: no current selection", thisfn );
	}
}
