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
#include <math.h>

#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"

#include "ui/ofa-misc-audit-ui.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* UI
	 */
	GtkWidget     *audit_tview;
	GtkWidget     *scale;

	/* runtime
	 */
	gdouble     tick;
	gint        rows;
	guint       pages;
}
	ofaMiscAuditUIPrivate;

/* columns of the treeview
 */
enum {
	COL_STAMP = 0,
	COL_QUERY,
	N_COLUMNS
};

/* data structure
 */
typedef struct {
	gchar    *stamp;
	gchar    *query;
}
	sAudit;

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-misc-audit-ui.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      audit_setup_treeview( ofaMiscAuditUI *self );
static gint      audit_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void      audit_set_to_tview( ofaMiscAuditUI *self, guint pageno );
static GList    *audit_get_from_db( ofaMiscAuditUI *self, guint pageno );
static void      audit_free( sAudit *audit );
static void      scale_setup( ofaMiscAuditUI *self );
static void      scale_set_data( ofaMiscAuditUI *self );
static void      scale_on_value_changed( GtkRange *range, ofaMiscAuditUI *self );

G_DEFINE_TYPE_EXTENDED( ofaMiscAuditUI, ofa_misc_audit_ui, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaMiscAuditUI )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
misc_audit_ui_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_UI( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_ui_parent_class )->finalize( instance );
}

static void
misc_audit_ui_dispose( GObject *instance )
{
	ofaMiscAuditUIPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_UI( instance ));

	priv = ofa_misc_audit_ui_get_instance_private( OFA_MISC_AUDIT_UI( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_ui_parent_class )->dispose( instance );
}

static void
ofa_misc_audit_ui_init( ofaMiscAuditUI *self )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_init";
	ofaMiscAuditUIPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MISC_AUDIT_UI( self ));

	priv = ofa_misc_audit_ui_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->tick = 1000.0;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_misc_audit_ui_class_init( ofaMiscAuditUIClass *klass )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = misc_audit_ui_dispose;
	G_OBJECT_CLASS( klass )->finalize = misc_audit_ui_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_misc_audit_ui_run:
 * @getter: a #ofaIGetter instance.
 *
 * Display the current content of the #myICollector interface.
 */
void
ofa_misc_audit_ui_run( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_run";
	ofaMiscAuditUI *self;
	ofaMiscAuditUIPrivate *priv;
	GtkApplicationWindow *parent;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_MISC_AUDIT_UI, NULL );

	parent = ofa_igetter_get_main_window( getter );
	my_iwindow_set_parent( MY_IWINDOW( self ), GTK_WINDOW( parent ));
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_misc_audit_ui_get_instance_private( self );

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
	static const gchar *thisfn = "ofa_misc_audit_ui_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	audit_setup_treeview( OFA_MISC_AUDIT_UI( instance ));
	scale_setup( OFA_MISC_AUDIT_UI( instance ));
	scale_set_data( OFA_MISC_AUDIT_UI( instance ));
}

static void
audit_setup_treeview( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "audit-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->audit_tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING ));
	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tmodel );
	g_object_unref( tmodel );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) audit_on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Date" ),
			cell,
			"text", COL_STAMP,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Command" ),
			cell,
			"text", COL_QUERY,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
}

static gint
audit_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *astamp, *bstamp;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_STAMP, &astamp, -1 );
	gtk_tree_model_get( tmodel, b, COL_STAMP, &bstamp, -1 );

	cmp = my_collate( astamp, bstamp );

	g_free( astamp );
	g_free( bstamp );

	return( cmp );
}

/*
 * @pageno: page number, starting from 1.
 */
static void
audit_set_to_tview( ofaMiscAuditUI *self, guint pageno )
{
	ofaMiscAuditUIPrivate *priv;
	GList *dataset, *it;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	sAudit *audit;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->audit_tview ));
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	dataset = audit_get_from_db( self, pageno );
	for( it=dataset ; it ; it=it->next ){
		audit = ( sAudit * ) it->data;
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ), &iter, -1,
				COL_STAMP, audit->stamp,
				COL_QUERY, audit->query,
				-1 );
	}

	g_list_free_full( dataset, ( GDestroyNotify ) audit_free );
}

static GList *
audit_get_from_db( ofaMiscAuditUI *self, guint pageno )
{
	ofaMiscAuditUIPrivate *priv;
	GList *dataset;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query;
	GSList *result, *irow, *icol;
	sAudit *audit;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	dataset = NULL;
	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf(
					"SELECT AUD_STAMP,AUD_QUERY FROM OFA_T_AUDIT LIMIT %d,%d",
					( guint )(( pageno-1) * priv->tick ), ( guint ) priv->tick );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			audit = g_new0( sAudit, 1 );
			icol = ( GSList * ) irow->data;
			audit->stamp = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			audit->query = g_strdup(( const gchar * ) icol->data );
			dataset = g_list_prepend( dataset, audit );
		}
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	return( g_list_reverse( dataset ));
}

static void
audit_free( sAudit *audit )
{
	g_free( audit->stamp );
	g_free( audit->query );
	g_free( audit );
}

/*
 * page is numbered from 1 to n
 */
static void
scale_setup( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	GtkWidget *scale, *label;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	scale = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "page-scale" );
	g_return_if_fail( scale && GTK_IS_SCALE( scale ));
	priv->scale = scale;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "scale-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), scale );

	g_signal_connect( scale, "value-changed", G_CALLBACK( scale_on_value_changed ), self );
}

static void
scale_set_data( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GtkAdjustment *adjustment;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	ofa_idbconnect_query_int( connect, "SELECT COUNT(*) FROM OFA_T_AUDIT", &priv->rows, TRUE );
	priv->pages = ( guint ) ceil(( gdouble ) priv->rows/ priv->tick );

	adjustment = gtk_adjustment_new( 1, 1, priv->pages, 1, 10, 0 );
	gtk_range_set_adjustment( GTK_RANGE( priv->scale ), adjustment );
	//g_object_unref( adjustment );

	scale_on_value_changed( GTK_RANGE( priv->scale ), self );
}

static void
scale_on_value_changed( GtkRange *range, ofaMiscAuditUI *self )
{
	gdouble pageno;

	pageno = gtk_range_get_value( range );
	audit_set_to_tview( self, ( guint ) pageno );
}
