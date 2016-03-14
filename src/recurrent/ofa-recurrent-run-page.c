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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ofa-recurrent-run-page.h"
#include "ofo-recurrent-run.h"

/* priv instance data
 */
struct _ofaRecurrentRunPagePrivate {

	/* internals
	 */
	ofaHub            *hub;
	gboolean           is_current;

	/* UI
	 */
	GtkWidget         *top_paned;
	GtkWidget         *tview;

	/* sorting the view
	 */
	gint               sort_column_id;
	gint               sort_sens;
	GtkTreeViewColumn *sort_column;
};

/* columns in the operation store
 */
enum {
	COL_MNEMO = 0,
	COL_DATE,
	COL_STATUS,
	COL_OBJECT,
	COL_N_COLUMNS
};

/* the id of the column is set against sortable columns */
#define DATA_COLUMN_ID                  "ofa-data-column-id"

/* it appears that Gtk+ displays a counter intuitive sort indicator:
 * when asking for ascending sort, Gtk+ displays a 'v' indicator
 * while we would prefer the '^' version -
 * we are defining the inverse indicator, and we are going to sort
 * in reverse order to have our own illusion
 */
#define OFA_SORT_ASCENDING              GTK_SORT_DESCENDING
#define OFA_SORT_DESCENDING             GTK_SORT_ASCENDING

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-run-page.ui";
static const gchar *st_page_settings    = "ofaRecurrentRunPage-settings";

static GtkWidget       *v_setup_view( ofaPage *page );
static void             setup_treeview( ofaRecurrentRunPage *self, GtkContainer *parent );
static void             setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent );
static GtkWidget       *v_get_top_focusable_widget( const ofaPage *page );
static gboolean         tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentRunPage *self );
static void             tview_on_header_clicked( GtkTreeViewColumn *column, ofaRecurrentRunPage *self );
static void             tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentRunPage *self );
static gint             tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentRunPage *self );
static gint             tview_cmp_strings( ofaRecurrentRunPage *self, const gchar *stra, const gchar *strb );
//static ofoRecurrentRun *treeview_get_selected( ofaRecurrentRunPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void             get_settings( ofaRecurrentRunPage *self );
static void             set_settings( ofaRecurrentRunPage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunPage, ofa_recurrent_run_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentRunPage ))

static void
recurrent_run_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_page_parent_class )->finalize( instance );
}

static void
recurrent_run_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		set_settings( OFA_RECURRENT_RUN_PAGE( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_run_page_init( ofaRecurrentRunPage *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_RUN_PAGE( self ));
}

static void
ofa_recurrent_run_page_class_init( ofaRecurrentRunPageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_v_setup_view";
	ofaRecurrentRunPagePrivate *priv;
	ofoDossier *dossier;
	GtkWidget *page_widget, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	priv->is_current = ofo_dossier_is_current( dossier );

	page_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	widget = my_utils_container_attach_from_resource( GTK_CONTAINER( page_widget ), st_resource_ui, "RecurrentRunPageWindow", "top" );
	g_return_val_if_fail( widget && GTK_IS_PANED( widget ), NULL );
	priv->top_paned = widget;

	setup_treeview( OFA_RECURRENT_RUN_PAGE( page ), GTK_CONTAINER( priv->top_paned ));
	setup_actions( OFA_RECURRENT_RUN_PAGE( page ), GTK_CONTAINER( priv->top_paned ));

	get_settings( OFA_RECURRENT_RUN_PAGE( page ));

	return( page_widget );
}

/*
 * initialize the treeview
 */
static void
setup_treeview( ofaRecurrentRunPage *self, GtkContainer *parent )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_setup_treeview";
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *tview;
	GtkListStore *store;
	GtkTreeModel *tfilter, *tsort;
	GtkTreeViewColumn *sort_column;
	gint column_id;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( parent, "treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = tview;

	store = gtk_list_store_new(
					COL_N_COLUMNS,
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* mnemo, date, status */
					G_TYPE_OBJECT );									/* ofoRecurrentRun object */

	tfilter = gtk_tree_model_filter_new( GTK_TREE_MODEL( store ), NULL );
	g_object_unref( store );

	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( tfilter ),
			( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row,
			self,
			NULL );

	tsort = gtk_tree_model_sort_new_with_model( tfilter );
	g_object_unref( tfilter );

	gtk_tree_view_set_model( GTK_TREE_VIEW( tview ), tsort );
	g_object_unref( tsort );

	g_debug( "%s: store=%p, tfilter=%p, tsort=%p",
			thisfn, ( void * ) store, ( void * ) tfilter, ( void * ) tsort );

	/* default is to sort by ascending operation date
	 */
	sort_column = NULL;
	if( priv->sort_column_id < 0 ){
		priv->sort_column_id = COL_DATE;
	}
	if( priv->sort_sens < 0 ){
		priv->sort_sens = OFA_SORT_ASCENDING;
	}

	column_id = COL_MNEMO;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Operation" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = COL_DATE;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Date" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	column_id = COL_STATUS;
	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Status" ),
			text_cell, "text", column_id,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( tview ), column );
	g_object_set_data( G_OBJECT( column ), DATA_COLUMN_ID, GINT_TO_POINTER( column_id ));
	gtk_tree_view_column_set_sort_column_id( column, column_id );
	g_signal_connect( G_OBJECT( column ), "clicked", G_CALLBACK( tview_on_header_clicked ), self );
	gtk_tree_sortable_set_sort_func(
			GTK_TREE_SORTABLE( tsort ), column_id, ( GtkTreeIterCompareFunc ) tview_on_sort_model, self, NULL );
	if( priv->sort_column_id == column_id ){
		sort_column = column;
	}

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( tview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( tview_on_selection_changed ), self );

	/* default is to sort by ascending operation date
	 */
	g_return_if_fail( sort_column && GTK_IS_TREE_VIEW_COLUMN( sort_column ));
	gtk_tree_view_column_set_sort_indicator( sort_column, TRUE );
	priv->sort_column = sort_column;
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tsort ), priv->sort_column_id, priv->sort_sens );
}

/*
 * initialize the actions area
 */
static void
setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent )
{

}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentRunPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_RUN_PAGE( page ), NULL );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	return( priv->tview );
}

/*
 * a row is visible depending of the status filter
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaRecurrentRunPage *self )
{
	//ofaRecurrentRunPagePrivate *priv;
	gboolean visible;
	ofoRecurrentRun *object;

	//priv = ofa_recurrent_run_page_get_instance_private( self );

	visible = FALSE;

	gtk_tree_model_get( tmodel, iter, COL_OBJECT, &object, -1 );
	if( object ){
		g_return_val_if_fail( OFO_IS_RECURRENT_RUN( object ), FALSE );
		g_object_unref( object );
		visible = TRUE;
	}

	return( visible );
}

/*
 * Gtk+ changes automatically the sort order
 * we reset yet the sort column id
 *
 * as a side effect of our inversion of indicators, clicking on a new
 * header makes the sort order descending as the default
 */
static void
tview_on_header_clicked( GtkTreeViewColumn *column, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gint sort_column_id, new_column_id;
	GtkSortType sort_order;
	GtkTreeModel *tsort;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	gtk_tree_view_column_set_sort_indicator( priv->sort_column, FALSE );
	gtk_tree_view_column_set_sort_indicator( column, TRUE );
	priv->sort_column = column;

	tsort = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->tview ));
	gtk_tree_sortable_get_sort_column_id( GTK_TREE_SORTABLE( tsort ), &sort_column_id, &sort_order );

	new_column_id = gtk_tree_view_column_get_sort_column_id( column );
	gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( tsort ), new_column_id, sort_order );

	priv->sort_column_id = new_column_id;
	priv->sort_sens = sort_order;

	set_settings( self );
}

/*
 * sorting the store
 */
static gint
tview_on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRecurrentRunPage *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_tview_on_sort_model";
	ofaRecurrentRunPagePrivate *priv;
	gint cmp;
	gchar *smnemoa, *sdatea, *statusa;
	gchar *smnemob, *sdateb, *statusb;

	gtk_tree_model_get( tmodel, a,
			COL_MNEMO,  &smnemoa,
			COL_DATE,   &sdatea,
			COL_STATUS, &statusa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			COL_MNEMO,  &smnemob,
			COL_DATE,   &sdateb,
			COL_STATUS, &statusb,
			-1 );

	cmp = 0;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	switch( priv->sort_column_id ){
		case COL_MNEMO:
			cmp = tview_cmp_strings( self, smnemoa, smnemob );
			break;
		case COL_DATE:
			cmp = my_date_compare_by_str( sdatea, sdateb, ofa_prefs_date_display());
			break;
		case COL_STATUS:
			cmp = tview_cmp_strings( self, statusa, statusb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, priv->sort_column_id );
			break;
	}

	g_free( statusa );
	g_free( sdatea );
	g_free( smnemoa );

	g_free( statusb );
	g_free( sdateb );
	g_free( smnemob );

	/* return -1 if a > b, so that the order indicator points to the smallest:
	 * ^: means from smallest to greatest (ascending order)
	 * v: means from greatest to smallest (descending order)
	 */
	return( -cmp );
}

static gint
tview_cmp_strings( ofaRecurrentRunPage *self, const gchar *stra, const gchar *strb )
{
	return( my_collate( stra, strb ));
}

static void
tview_on_selection_changed( GtkTreeSelection *selection, ofaRecurrentRunPage *self )
{
}

#if 0
static ofoRecurrentRun  *
treeview_get_selected( ofaRecurrentRunPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkTreeSelection *select;
	ofoRecurrentRun  *object;

	priv = ofa_recurrent_run_page_get_instance_private( page );

	object = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->record_treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, RECURRENT_RUN_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && OFO_IS_RECURRENT_RUN( object ), NULL );
		g_object_unref( object );
	}

	return( object );
}
#endif

/*
 * settings: sort_column_id;sort_sens;paned_position;
 */
static void
get_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	gint pos;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_page_settings );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->sort_sens = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	pos = 0;
	if( my_strlen( cstr )){
		pos = atoi( cstr );
	}
	if( pos == 0 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->top_paned ), pos );

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gchar *str;
	gint pos;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	pos = gtk_paned_get_position( GTK_PANED( priv->top_paned ));

	str = g_strdup_printf( "%d;%d;%d;", priv->sort_column_id, priv->sort_sens, pos );

	ofa_settings_user_set_string( st_page_settings, str );

	g_free( str );
}
