/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-misc-collector-ui.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	GtkWindow  *parent;

	/* runtime
	 */
	ofaHub     *hub;

	/* UI
	 */
	GtkWidget  *collection_tview;
	GtkWidget  *single_tview;
}
	ofaMiscCollectorUIPrivate;

/* columns of the Collections store
 */
enum {
	CCOL_NAME = 0,
	CCOL_COUNT,
	CN_COLUMNS
};

/* columns of the Singles store
 */
enum {
	SCOL_NAME = 0,
	SN_COLUMNS
};

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-misc-collector-ui.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void collection_setup_treeview( ofaMiscCollectorUI *self );
static gint collection_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void collection_set_data( ofaMiscCollectorUI *self );
static void single_setup_treeview( ofaMiscCollectorUI *self );
static gint single_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void single_set_data( ofaMiscCollectorUI *self );

G_DEFINE_TYPE_EXTENDED( ofaMiscCollectorUI, ofa_misc_collector_ui, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaMiscCollectorUI )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
misc_collector_ui_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MISC_COLLECTOR_UI( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_collector_ui_parent_class )->finalize( instance );
}

static void
misc_collector_ui_dispose( GObject *instance )
{
	ofaMiscCollectorUIPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MISC_COLLECTOR_UI( instance ));

	priv = ofa_misc_collector_ui_get_instance_private( OFA_MISC_COLLECTOR_UI( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_collector_ui_parent_class )->dispose( instance );
}

static void
ofa_misc_collector_ui_init( ofaMiscCollectorUI *self )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_init";
	ofaMiscCollectorUIPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MISC_COLLECTOR_UI( self ));

	priv = ofa_misc_collector_ui_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_misc_collector_ui_class_init( ofaMiscCollectorUIClass *klass )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = misc_collector_ui_dispose;
	G_OBJECT_CLASS( klass )->finalize = misc_collector_ui_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_misc_collector_ui_run:
 * @getter: a #ofaIGetter instance.
 *
 * Display the current content of the #myICollector interface.
 */
void
ofa_misc_collector_ui_run( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_run";
	ofaMiscCollectorUI *self;
	ofaMiscCollectorUIPrivate *priv;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_MISC_COLLECTOR_UI, NULL );

	priv = ofa_misc_collector_ui_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = GTK_WINDOW( ofa_igetter_get_main_window( getter ));

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_iwindow_init";
	ofaMiscCollectorUIPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_misc_collector_ui_get_instance_private( OFA_MISC_COLLECTOR_UI( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_geometry_settings( instance, ofa_hub_get_user_settings( priv->hub ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_misc_collector_ui_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	collection_setup_treeview( OFA_MISC_COLLECTOR_UI( instance ));
	single_setup_treeview( OFA_MISC_COLLECTOR_UI( instance ));

	collection_set_data( OFA_MISC_COLLECTOR_UI( instance ));
	single_set_data( OFA_MISC_COLLECTOR_UI( instance ));
}

static void
collection_setup_treeview( ofaMiscCollectorUI *self )
{
	ofaMiscCollectorUIPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	priv = ofa_misc_collector_ui_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "collection-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->collection_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			CN_COLUMNS,
			G_TYPE_STRING, G_TYPE_UINT ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) collection_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Name" ),
			cell,
			"text", CCOL_NAME,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Count" ),
			cell,
			"text", CCOL_COUNT,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
}

static gint
collection_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, CCOL_NAME, &aname, -1 );
	gtk_tree_model_get( tmodel, b, CCOL_NAME, &bname, -1 );

	cmp = my_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
collection_set_data( ofaMiscCollectorUI *self )
{
	ofaMiscCollectorUIPrivate *priv;
	myICollector *collector;
	GList *list, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *name;
	guint count;

	priv = ofa_misc_collector_ui_get_instance_private( self );

	collector = ofa_hub_get_collector( priv->hub );
	g_return_if_fail( collector && MY_IS_ICOLLECTOR( collector ));

	list = my_icollector_collection_get_list( collector );
	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->collection_tview ));

	for( it=list ; it ; it=it->next ){
		name = my_icollector_item_get_name( collector, it->data );
		count = my_icollector_item_get_count( collector, it->data );
		g_debug( "collection_set_data: name=%s, count=%d", name, count );

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ), &iter, -1,
				CCOL_NAME,  name,
				CCOL_COUNT, count,
				-1 );

		g_free( name );
	}

	g_list_free( list );
}

static void
single_setup_treeview( ofaMiscCollectorUI *self )
{
	ofaMiscCollectorUIPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	priv = ofa_misc_collector_ui_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "single-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->single_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			SN_COLUMNS,
			G_TYPE_STRING ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) single_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Name" ),
			cell,
			"text", CCOL_NAME,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
}

static gint
single_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, CCOL_NAME, &aname, -1 );
	gtk_tree_model_get( tmodel, b, CCOL_NAME, &bname, -1 );

	cmp = my_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
single_set_data( ofaMiscCollectorUI *self )
{
	ofaMiscCollectorUIPrivate *priv;
	myICollector *collector;
	GList *list, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *name;

	priv = ofa_misc_collector_ui_get_instance_private( self );

	collector = ofa_hub_get_collector( priv->hub );
	g_return_if_fail( collector && MY_IS_ICOLLECTOR( collector ));

	list = my_icollector_single_get_list( collector );
	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->single_tview ));

	for( it=list ; it ; it=it->next ){
		name = my_icollector_item_get_name( collector, it->data );

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ), &iter, -1,
				SCOL_NAME,  name,
				-1 );

		g_free( name );
	}

	g_list_free( list );
}
