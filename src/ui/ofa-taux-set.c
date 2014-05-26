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

#include "ui/my-utils.h"
#include "ui/ofa-main-page.h"
#include "ui/ofa-taux-properties.h"
#include "ui/ofa-taux-set.h"
#include "ui/ofo-taux.h"

/* private instance data
 */
struct _ofaTauxSetPrivate {
	gboolean   dispose_has_run;
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO,
	COL_LABEL,
	COL_BEGIN,			/* minimum begin of all validities */
	COL_END,			/* max end */
	COL_OBJECT,
	N_COLUMNS
};

static ofaMainPageClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaTauxSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static GtkWidget *v_setup_view( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       insert_new_row( ofaTauxSet *self, ofoTaux *taux, gboolean with_selection );
static gchar     *get_min_val_date( ofoTaux *taux );
static gchar     *get_max_val_date( ofoTaux *taux );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTauxSet *self );
static void       setup_first_selection( ofaTauxSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       on_taux_selected( GtkTreeSelection *selection, ofaTauxSet *self );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaTauxSet *self, ofoTaux *taux );

GType
ofa_taux_set_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_taux_set_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaTauxSetClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaTauxSet ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaTauxSet", &info, 0 );

	return( type );
}

static void
class_init( ofaTauxSetClass *klass )
{
	static const gchar *thisfn = "ofa_taux_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = ( ofaMainPageClass * ) g_type_class_peek_parent( klass );
	g_return_if_fail( st_parent_class && OFA_IS_MAIN_PAGE_CLASS( st_parent_class ));

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_taux_set_instance_init";
	ofaTauxSet *self;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",

			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_TAUX_SET( instance );

	self->private = g_new0( ofaTauxSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_set_instance_dispose";
	ofaTauxSetPrivate *priv;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	priv = ( OFA_TAUX_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( instance );
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_set_instance_finalize";
	ofaTauxSet *self;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_TAUX_SET( instance );

	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( instance );
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), page );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Mnemo" ),
			text_cell, "text", COL_MNEMO,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_sensitive( text_cell, FALSE );
	g_object_set( G_OBJECT( text_cell ), "style", PANGO_STYLE_ITALIC, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Val. begin" ),
			text_cell, "text", COL_BEGIN,
			NULL );
	gtk_tree_view_append_column( tview, column );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_sensitive( text_cell, FALSE );
	g_object_set( G_OBJECT( text_cell ), "style", PANGO_STYLE_ITALIC, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Val. end" ),
			text_cell, "text", COL_END,
			NULL );
	gtk_tree_view_append_column( tview, column );

	select = gtk_tree_view_get_selection( tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_taux_selected ), page );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, page, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static void
v_init_view( ofaMainPage *page )
{
	ofaTauxSet *self;
	ofoDossier *dossier;
	GList *dataset, *it;
	ofoTaux *taux;

	self = OFA_TAUX_SET( page );
	dossier = ofa_main_page_get_dossier( page );
	dataset = ofo_taux_get_dataset( dossier );

	for( it=dataset ; it ; it=it->next ){

		taux = OFO_TAUX( it->data );
		insert_new_row( self, taux, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaTauxSet *self, ofoTaux *taux, gboolean with_selection )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *sbegin, *send;

	sbegin = get_min_val_date( taux );
	send = get_max_val_date( taux );

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
	tmodel = gtk_tree_view_get_model( tview );
	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_MNEMO,  ofo_taux_get_mnemo( taux ),
			COL_LABEL,  ofo_taux_get_label( taux ),
			COL_BEGIN,  sbegin,
			COL_END,    send,
			COL_OBJECT, taux,
			-1 );

	g_free( sbegin );
	g_free( send );

	/* select the newly added taux */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( tview ));
	}
}

static gchar *
get_min_val_date( ofoTaux *taux )
{
	const GDate *dmin;
	gchar *str, *sbegin;

	dmin = ofo_taux_get_min_valid( taux );

	if( dmin && g_date_valid( dmin )){
		str = my_utils_display_from_date( dmin, MY_UTILS_DATE_DMMM );
		sbegin = g_strdup_printf( _( "from %s" ), str );
		g_free( str );
	} else {
		sbegin = g_strdup_printf( _( "from infinite" ));
	}

	return( sbegin );
}

static gchar *
get_max_val_date( ofoTaux *taux )
{
	const GDate *dmax;
	gchar *str, *send;

	dmax = ofo_taux_get_max_valid( taux );

	if( dmax && g_date_valid( dmax )){
		str = my_utils_display_from_date( dmax, MY_UTILS_DATE_DMMM );
		send = g_strdup_printf( _( "to %s" ), str );
		g_free( str );
	} else {
		send = g_strdup_printf( _( "to infinite" ));
	}

	return( send );
}

/*
 * sorting the treeview in only sorting per mnemo
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTauxSet *self )
{
	gchar *amnemo, *bmnemo, *afold, *bfold;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_MNEMO, &amnemo, -1 );
	afold = g_utf8_casefold( amnemo, -1 );

	gtk_tree_model_get( tmodel, b, COL_MNEMO, &bmnemo, -1 );
	bfold = g_utf8_casefold( bmnemo, -1 );

	cmp = g_utf8_collate( afold, bfold );

	g_free( amnemo );
	g_free( afold );
	g_free( bmnemo );
	g_free( bfold );

	return( cmp );
}

static void
setup_first_selection( ofaTauxSet *self )
{
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( OFA_MAIN_PAGE( self )));
	tmodel = gtk_tree_view_get_model( tview );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

/*
 * double click on a row opens the rate properties
 */
static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page )
{
	v_on_update_clicked( NULL, page );
}

static void
on_taux_selected( GtkTreeSelection *selection, ofaTauxSet *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTaux *taux;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );
	}

	gtk_widget_set_sensitive(
			ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
			taux && OFO_IS_TAUX( taux ));

	gtk_widget_set_sensitive(
			ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
			taux && OFO_IS_TAUX( taux ) && ofo_taux_is_deletable( taux ));
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	ofoTaux *taux;

	g_return_if_fail( page && OFA_IS_TAUX_SET( page ));

	taux = ofo_taux_new();

	if( ofa_taux_properties_run(
			ofa_main_page_get_main_window( page ), taux )){

		insert_new_row( OFA_TAUX_SET( page ), taux, TRUE );

	} else {
		g_object_unref( taux );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTaux *taux;
	gchar *sbegin, *send;

	g_return_if_fail( page && OFA_IS_TAUX_SET( page ));

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );

		if( ofa_taux_properties_run(
				ofa_main_page_get_main_window( page ), taux )){

			sbegin = get_min_val_date( taux );
			send = get_max_val_date( taux );

			gtk_list_store_set(
					GTK_LIST_STORE( tmodel ),
					&iter,
					COL_MNEMO, ofo_taux_get_mnemo( taux ),
					COL_LABEL, ofo_taux_get_label( taux ),
					COL_BEGIN, sbegin,
					COL_END,   send,
					-1 );

			g_free( sbegin );
			g_free( send );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static void
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	GtkTreeView *tview;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoTaux *taux;

	g_return_if_fail( page && OFA_IS_TAUX_SET( page ));

	tview = GTK_TREE_VIEW( ofa_main_page_get_treeview( page ));
	select = gtk_tree_view_get_selection( tview );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );

		if( delete_confirmed( OFA_TAUX_SET( page ), taux ) &&
				ofo_taux_delete( taux, ofa_main_page_get_dossier( page ))){

			/* remove the row from the model
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( tview ));
}

static gboolean
delete_confirmed( ofaTauxSet *self, ofoTaux *taux )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' rate ?" ),
			ofo_taux_get_mnemo( taux ),
			ofo_taux_get_label( taux ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}
