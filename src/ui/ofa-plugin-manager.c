/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iident.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-extender-module.h"
#include "api/ofa-hub.h"
#include "api/ofa-iabout.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iproperties.h"
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
	GtkWidget     *plugin_pane;
	gint           plugin_pane_pos;
	GtkWidget     *plugin_tview;
	GtkWidget     *plugin_book;
	GtkWidget     *about_page;
	GtkWidget     *properties_page;
	GtkWidget     *objects_tview;
}
	ofaPluginManagerPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-plugin-manager.ui";

/* column ordering in the plugin listview
 */
enum {
	PLUG_COL_NAME = 0,
	PLUG_COL_VERSION,
	PLUG_COL_PLUGIN,
	PLUG_N_COLUMNS
};

/* column ordering in the objects listview
 */
enum {
	OBJ_COL_CLASS = 0,
	OBJ_COL_NAME,
	OBJ_COL_VERSION,
	OBJ_COL_OBJECT,
	OBJ_N_COLUMNS
};

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *key );
static void iwindow_write_settings( myIWindow *instance, myISettings *settings, const gchar *key );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void plugin_setup_treeview( ofaPluginManager *self );
static gint plugin_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void plugin_on_selection_changed( GtkTreeSelection *selection, ofaPluginManager *self );
static void plugin_set_about_page( ofaPluginManager *self, ofaExtenderModule *plugin, const GList *objects );
static void plugin_set_properties_page( ofaPluginManager *self, ofaExtenderModule *plugin, const GList *objects );
static void objects_setup_treeview( ofaPluginManager *self );
static gint objects_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void objects_on_selection_changed( GtkTreeSelection *selection, ofaPluginManager *self );
static void plugins_load( ofaPluginManager *self );
static void objects_load( ofaPluginManager *self, const GList *objects );

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

	priv->plugin_pane_pos = 0;
	priv->about_page = NULL;
	priv->properties_page = NULL;

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
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_hub_get_user_settings( ofa_igetter_get_hub( getter )));

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

	iface->read_settings = iwindow_read_settings;
	iface->write_settings = iwindow_write_settings;
}

/*
 * settings are: plugin_paned;
 */
static void
iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *key )
{
	ofaPluginManagerPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_plugin_manager_get_instance_private( OFA_PLUGIN_MANAGER( instance ));

	slist = my_isettings_get_string_list( settings, SETTINGS_GROUP_GENERAL, key );

	it = slist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->plugin_pane_pos = atoi( cstr );
	}

	my_isettings_free_string_list( settings, slist );

	if( priv->plugin_pane_pos < 150 ){
		priv->plugin_pane_pos = 150;
	}
}

static void
iwindow_write_settings( myIWindow *instance, myISettings *settings, const gchar *key )
{
	ofaPluginManagerPrivate *priv;
	gchar *str;

	priv = ofa_plugin_manager_get_instance_private( OFA_PLUGIN_MANAGER( instance ));

	str = g_strdup_printf( "%d;",
				gtk_paned_get_position( GTK_PANED( priv->plugin_pane )));

	my_isettings_set_string( settings, SETTINGS_GROUP_GENERAL, key, str );

	g_free( str );
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

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_plugin_manager_get_instance_private( OFA_PLUGIN_MANAGER( instance ));

	priv->plugin_pane = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "plugin-paned" );
	g_return_if_fail( priv->plugin_pane && GTK_IS_PANED( priv->plugin_pane ));
	gtk_paned_set_position( GTK_PANED( priv->plugin_pane ), priv->plugin_pane_pos );

	priv->plugin_book = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "object-notebook" );
	g_return_if_fail( priv->plugin_book && GTK_IS_NOTEBOOK( priv->plugin_book ));

	plugin_setup_treeview( OFA_PLUGIN_MANAGER( instance ));
	objects_setup_treeview( OFA_PLUGIN_MANAGER( instance ));
	plugins_load( OFA_PLUGIN_MANAGER( instance ));
}

static void
plugin_setup_treeview( ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_plugin_manager_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "plugin-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->plugin_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			PLUG_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) plugin_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Plugin" ),
			cell,
			"text", PLUG_COL_NAME,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Version" ),
			cell,
			"text", PLUG_COL_VERSION,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( select, "changed", G_CALLBACK( plugin_on_selection_changed ), self );
}

static gint
plugin_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, PLUG_COL_NAME, &aname, -1 );
	gtk_tree_model_get( tmodel, b, PLUG_COL_NAME, &bname, -1 );

	cmp = my_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
plugin_on_selection_changed( GtkTreeSelection *selection, ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	const GList *objects;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofaExtenderModule *plugin;

	priv = ofa_plugin_manager_get_instance_private( self );

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, PLUG_COL_PLUGIN, &plugin, -1 );
		g_object_unref( plugin );

		objects = ofa_extender_module_get_objects( plugin );
		objects_load( self, objects );
		plugin_set_about_page( self, plugin, objects );
		plugin_set_properties_page( self, plugin, objects );

		gtk_widget_show_all( priv->plugin_book );
	}
}

static void
plugin_set_about_page( ofaPluginManager *self, ofaExtenderModule *plugin, const GList *objects )
{
	ofaPluginManagerPrivate *priv;
	gint page_num;
	const GList *it;
	GtkWidget *label, *content;

	priv = ofa_plugin_manager_get_instance_private( self );

	if( priv->about_page ){
		page_num = gtk_notebook_page_num( GTK_NOTEBOOK( priv->plugin_book ), priv->about_page );
		gtk_notebook_remove_page( GTK_NOTEBOOK( priv->plugin_book ), page_num );
		priv->about_page = NULL;
	}

	for( it=objects ; it ; it=it->next ){
		if( OFA_IS_IABOUT( it->data )){
			label = gtk_label_new_with_mnemonic( _( "_About" ));
			priv->about_page = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
			gtk_notebook_prepend_page( GTK_NOTEBOOK( priv->plugin_book ), priv->about_page, label );
			content = ofa_iabout_do_init( OFA_IABOUT( it->data ));
			gtk_box_pack_start( GTK_BOX( priv->about_page ), content, TRUE, TRUE, 0 );
			break;
		}
	}
}

static void
plugin_set_properties_page( ofaPluginManager *self, ofaExtenderModule *plugin, const GList *objects )
{
	ofaPluginManagerPrivate *priv;
	gint page_num;
	const GList *it;
	GtkWidget *label, *content;

	priv = ofa_plugin_manager_get_instance_private( self );

	if( priv->properties_page ){
		page_num = gtk_notebook_page_num( GTK_NOTEBOOK( priv->plugin_book ), priv->properties_page );
		gtk_notebook_remove_page( GTK_NOTEBOOK( priv->plugin_book ), page_num );
		priv->properties_page = NULL;
	}

	for( it=objects ; it ; it=it->next ){
		if( OFA_IS_IPROPERTIES( it->data )){
			label = gtk_label_new_with_mnemonic( _( "_Properties" ));
			priv->properties_page = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
			gtk_notebook_prepend_page( GTK_NOTEBOOK( priv->plugin_book ), priv->properties_page, label );
			content = ofa_iproperties_init(
							OFA_IPROPERTIES( it->data ),
							ofa_settings_get_settings( SETTINGS_TARGET_USER ));
			gtk_box_pack_start( GTK_BOX( priv->properties_page ), content, TRUE, TRUE, 0 );
			break;
		}
	}
}

static void
objects_setup_treeview( ofaPluginManager *self )
{
	ofaPluginManagerPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_plugin_manager_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "object-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->objects_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			OBJ_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) objects_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Class" ),
			cell,
			"text", OBJ_COL_CLASS,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Name" ),
			cell,
			"text", OBJ_COL_NAME,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Version" ),
			cell,
			"text", OBJ_COL_VERSION,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( select, "changed", G_CALLBACK( objects_on_selection_changed ), self );
}

static gint
objects_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, OBJ_COL_CLASS, &aname, -1 );
	gtk_tree_model_get( tmodel, b, OBJ_COL_CLASS, &bname, -1 );

	cmp = my_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
objects_on_selection_changed( GtkTreeSelection *selection, ofaPluginManager *self )
{
}

static void
plugins_load( ofaPluginManager *self )
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

	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->plugin_tview ));
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	hub = ofa_igetter_get_hub( priv->getter );
	extenders = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_modules( extenders );

	for( it=modules ; it ; it=it->next ){
		plugin = OFA_EXTENDER_MODULE( it->data );
		name = ofa_extender_module_get_display_name( plugin );
		version = ofa_extender_module_get_version( plugin );
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				PLUG_COL_NAME,    name,
				PLUG_COL_VERSION, version,
				PLUG_COL_PLUGIN,  plugin,
				-1 );

		g_free( version );
		g_free( name );
	}

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->plugin_tview ));
		gtk_tree_selection_select_iter( select, &iter );
	}
}

/*
 * display the objects instanciated by the current plugin
 */
static void
objects_load( ofaPluginManager *self, const GList *objects )
{
	ofaPluginManagerPrivate *priv;
	GtkTreeModel *tmodel;
	const GList *it;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	gchar *name, *version;

	priv = ofa_plugin_manager_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->objects_tview ));
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	for( it=objects ; it ; it=it->next ){

		name = NULL;
		version = NULL;
		if( MY_IS_IIDENT( it->data )){
			name = my_iident_get_display_name( MY_IIDENT( it->data ), NULL );
			version = my_iident_get_version( MY_IIDENT( it->data ), NULL );
		}

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				OBJ_COL_CLASS,   G_OBJECT_TYPE_NAME( it->data ),
				OBJ_COL_NAME,    name ? name : "",
				OBJ_COL_VERSION, version ? version : "",
				OBJ_COL_OBJECT,  it->data,
				-1 );

		g_free( name );
		g_free( version );
	}

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->objects_tview ));
		gtk_tree_selection_select_iter( select, &iter );
	}
}

#if 0
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

	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->plugin_tview ));
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, PLUG_COL_PLUGIN, &plugin, -1 );
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
#endif
