/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-extender-module.h"
#include "api/ofa-hub.h"
#include "api/ofa-iabout.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-application.h"
#include "ui/ofa-plugin-manager.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIGetter    *getter;

	/* UI
	 */
	GtkTreeView   *tview;
	GtkWidget     *properties_btn;
}
	ofaPluginManagerPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-plugin-manager.ui";

/* column ordering in the selection listview
 */
enum {
	COL_NAME = 0,
	COL_VERSION,
	COL_PLUGIN,
	N_COLUMNS
};

static void iwindow_iface_init( myIWindowInterface *iface );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void setup_treeview( ofaPluginManager *self );
static void load_in_treeview( ofaPluginManager *self );
static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void on_plugin_selected( GtkTreeSelection *selection, ofaPluginManager *self );
static void on_properties_clicked( GtkButton *button, ofaPluginManager *self );

G_DEFINE_TYPE_EXTENDED( ofaPluginManager, ofa_plugin_manager, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPluginManager )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
plugin_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_plugin_manager_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PLUGIN_MANAGER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_plugin_manager_parent_class )->finalize( instance );
}

static void
plugin_manager_dispose( GObject *instance )
{
	ofaPluginManagerPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PLUGIN_MANAGER( instance ));

	priv = ofa_plugin_manager_get_instance_private( OFA_PLUGIN_MANAGER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_plugin_manager_parent_class )->dispose( instance );
}

static void
ofa_plugin_manager_init( ofaPluginManager *self )
{
	static const gchar *thisfn = "ofa_plugin_manager_init";
	ofaPluginManagerPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PLUGIN_MANAGER( self ));

	priv = ofa_plugin_manager_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_plugin_manager_class_init( ofaPluginManagerClass *klass )
{
	static const gchar *thisfn = "ofa_plugin_manager_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = plugin_manager_dispose;
	G_OBJECT_CLASS( klass )->finalize = plugin_manager_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_plugin_manager_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run the dialog to manage the plugins
 */
void
ofa_plugin_manager_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_plugin_manager_run";
	ofaPluginManager *self;
	ofaPluginManagerPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_PLUGIN_MANAGER, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_plugin_manager_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_plugin_manager_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_plugin_manager_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_plugin_manager_idialog_init";
	ofaPluginManagerPrivate *priv;
	GtkWidget *button;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_plugin_manager_get_instance_private( OFA_PLUGIN_MANAGER( instance ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "properties-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_properties_clicked ), instance );
	priv->properties_btn = button;

	setup_treeview( OFA_PLUGIN_MANAGER( instance ));
	load_in_treeview( OFA_PLUGIN_MANAGER( instance ));
}

static void
setup_treeview( ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_plugin_manager_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = GTK_TREE_VIEW( tview );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
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
	g_signal_connect( select, "changed", G_CALLBACK( on_plugin_selected ), self );
}

static void
load_in_treeview( ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofaHub *hub;
	ofaExtenderCollection *extenders;
	const GList *modules, *it;
	ofaExtenderModule *plugin;
	gchar *name, *version;

	priv = ofa_plugin_manager_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( priv->tview );
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	hub = ofa_igetter_get_hub( priv->getter );
	extenders = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_modules( extenders );

	for( it=modules ; it ; it=it->next ){
		plugin = OFA_EXTENDER_MODULE( it->data );
		name = ofa_extender_module_get_display_name( plugin );
		version = ofa_extender_module_get_version( plugin );
		//g_debug( "load_in_treeview: name=%s, version=%s", name, version );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				COL_NAME,    name,
				COL_VERSION, version,
				COL_PLUGIN,  plugin,
				-1 );

		g_free( version );
		g_free( name );
	}

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
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

	cmp = my_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
on_plugin_selected( GtkTreeSelection *selection, ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	GList *objects;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofaExtenderModule *plugin;

	priv = ofa_plugin_manager_get_instance_private( self );

	objects = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_PLUGIN, &plugin, -1 );
		g_object_unref( plugin );

		objects = ofa_extender_module_get_for_type( plugin, OFA_TYPE_IABOUT );
	}

	gtk_widget_set_sensitive( priv->properties_btn, g_list_length( objects ) > 0 );

	ofa_extender_collection_free_types( objects );
}

/*
 * display the preferences for the first object type implemented by the
 * plugin
 */
static void
on_properties_clicked( GtkButton *button, ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofaExtenderModule *plugin;
	GList *objects;
	GtkWidget *page, *dialog, *content;

	priv = ofa_plugin_manager_get_instance_private( self );

	selection = gtk_tree_view_get_selection( priv->tview );
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_PLUGIN, &plugin, -1 );
		g_object_unref( plugin );

		objects = ofa_extender_module_get_for_type( plugin, OFA_TYPE_IABOUT );
		g_return_if_fail( objects && g_list_length( objects ) > 0 );
		page = ofa_iabout_do_init( OFA_IABOUT( objects->data ));

		if( page && GTK_IS_WIDGET( page )){
			dialog = gtk_dialog_new_with_buttons(
					_( "Properties" ),
					GTK_WINDOW( self ),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					_( "Close" ),
					GTK_RESPONSE_ACCEPT,
					NULL );
			content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));
			g_return_if_fail( content && GTK_IS_CONTAINER( content ));
			gtk_container_add( GTK_CONTAINER( content ), page );
			g_signal_connect_swapped(
					dialog, "response", G_CALLBACK( gtk_widget_destroy ), dialog );
			gtk_widget_show_all( dialog );
		}

		ofa_extender_collection_free_types( objects );
	}
}
