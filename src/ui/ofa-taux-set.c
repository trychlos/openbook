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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-rate.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-main-page.h"
#include "ui/ofa-rate-properties.h"
#include "ui/ofa-taux-set.h"

/* private instance data
 */
struct _ofaTauxSetPrivate {
	gboolean      dispose_has_run;

	/* internals
	 */
	GList        *handlers;
	/* UI
	 */
	GtkTreeView  *tview;
	GtkTreeModel *tmodel;
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

G_DEFINE_TYPE( ofaTauxSet, ofa_taux_set, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static void       setup_dossier_signaling( ofaTauxSet *self );
static GtkWidget *setup_tree_view( ofaTauxSet *self );
static void       v_init_view( ofaMainPage *page );
static void       insert_dataset( ofaTauxSet *self );
static void       insert_new_row( ofaTauxSet *self, ofoRate *taux, gboolean with_selection );
static void       set_row_by_iter( ofaTauxSet *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoRate *taux );
static gboolean   find_row_by_mnemo( ofaTauxSet *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static gchar     *get_min_val_date( ofoRate *taux );
static gchar     *get_max_val_date( ofoRate *taux );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTauxSet *self );
static void       setup_first_selection( ofaTauxSet *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaMainPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaTauxSet *self );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaTauxSet *self );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaTauxSet *self );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaTauxSet *self, ofoRate *taux );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaTauxSet *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaTauxSet *self );

static void
taux_set_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_set_finalize";
	ofaTauxSetPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	priv = OFA_TAUX_SET( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_taux_set_parent_class )->finalize( instance );
}

static void
taux_set_dispose( GObject *instance )
{
	ofaTauxSetPrivate *priv;
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_TAUX_SET( instance ));

	priv = ( OFA_TAUX_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( instance ));
		if( OFO_IS_DOSSIER( dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_taux_set_parent_class )->dispose( instance );
}

static void
ofa_taux_set_init( ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_instance_init";

	g_return_if_fail( OFA_IS_TAUX_SET( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaTauxSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->handlers = NULL;
}

static void
ofa_taux_set_class_init( ofaTauxSetClass *klass )
{
	static const gchar *thisfn = "ofa_taux_set_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = taux_set_dispose;
	G_OBJECT_CLASS( klass )->finalize = taux_set_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	setup_dossier_signaling( OFA_TAUX_SET( page ));

	return( setup_tree_view( OFA_TAUX_SET( page )));
}

static void
setup_dossier_signaling( ofaTauxSet *self )
{
	ofaTauxSetPrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = self->private;
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						OFA_SIGNAL_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static GtkWidget *
setup_tree_view( ofaTauxSet *self )
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
	g_signal_connect(G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), self );
	self->private->tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );
	self->private->tmodel = tmodel;

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
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static void
v_init_view( ofaMainPage *page )
{
	insert_dataset( OFA_TAUX_SET( page ));
}

static void
insert_dataset( ofaTauxSet *self )
{
	GList *dataset, *it;
	ofoRate *taux;

	dataset = ofo_rate_get_dataset( ofa_main_page_get_dossier( OFA_MAIN_PAGE( self )));

	for( it=dataset ; it ; it=it->next ){

		taux = OFO_RATE( it->data );
		insert_new_row( self, taux, FALSE );
	}

	setup_first_selection( self );
}

/*
 * we insert the mnemo as soon as the row is created, so that the
 * on_sort_model() function does not complain about null strings
 */
static void
insert_new_row( ofaTauxSet *self, ofoRate *taux, gboolean with_selection )
{
	ofaTauxSetPrivate *priv;
	GtkTreeIter iter;
	GtkTreePath *path;

	priv = self->private;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( priv->tmodel ),
			&iter,
			-1,
			COL_MNEMO,  ofo_rate_get_mnemo( taux ),
			COL_OBJECT, taux,
			-1 );

	set_row_by_iter( self, priv->tmodel, &iter, taux );

	/* select the newly added taux */
	if( with_selection ){
		path = gtk_tree_model_get_path( priv->tmodel, &iter );
		gtk_tree_view_set_cursor( priv->tview, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

/*
 * the mnemo is set here even it is has been already set when creating
 * the row in order to takeninto account a possible identifier
 * modification
 */
static void
set_row_by_iter( ofaTauxSet *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoRate *taux )
{
	gchar *sbegin, *send;

	sbegin = get_min_val_date( taux );
	send = get_max_val_date( taux );

	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			iter,
			COL_MNEMO,  ofo_rate_get_mnemo( taux ),
			COL_LABEL,  ofo_rate_get_label( taux ),
			COL_BEGIN,  sbegin,
			COL_END,    send,
			-1 );

	g_free( sbegin );
	g_free( send );
}

static gboolean
find_row_by_mnemo( ofaTauxSet *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	gchar *row_mnemo;
	gint cmp;

	*tmodel = gtk_tree_view_get_model( self->private->tview );

	if( gtk_tree_model_get_iter_first( *tmodel, iter )){
		while( TRUE ){
			gtk_tree_model_get( *tmodel, iter, COL_MNEMO, &row_mnemo, -1 );
			cmp = g_utf8_collate( row_mnemo, mnemo );
			g_free( row_mnemo );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( *tmodel, iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static gchar *
get_min_val_date( ofoRate *taux )
{
	const myDate *dmin;
	gchar *str, *sbegin;

	dmin = ofo_rate_get_min_valid( taux );

	if( my_date_is_valid( dmin )){
		str = my_date_to_str( dmin, MY_DATE_DMMM );
		sbegin = g_strdup_printf( _( "from %s" ), str );
		g_free( str );
	} else {
		sbegin = g_strdup_printf( _( "from infinite" ));
	}

	return( sbegin );
}

static gchar *
get_max_val_date( ofoRate *taux )
{
	const myDate *dmax;
	gchar *str, *send;

	dmax = ofo_rate_get_max_valid( taux );

	if( my_date_is_valid( dmax )){
		str = my_date_to_str( dmax, MY_DATE_DMMM );
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
	ofaTauxSetPrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->private;

	if( gtk_tree_model_get_iter_first( priv->tmodel, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
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
on_row_selected( GtkTreeSelection *selection, ofaTauxSet *self )
{
	ofaTauxSetPrivate *priv;
	GtkTreeIter iter;
	ofoRate *taux;

	priv = self->private;

	if( gtk_tree_selection_get_selected( selection, NULL, &iter )){
		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );
	}

	gtk_widget_set_sensitive(
			ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
			taux && OFO_IS_RATE( taux ));

	gtk_widget_set_sensitive(
			ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
			taux && OFO_IS_RATE( taux ) && ofo_rate_is_deletable( taux ));
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	ofoRate *taux;

	g_return_if_fail( page && OFA_IS_TAUX_SET( page ));

	taux = ofo_rate_new();

	if( ofa_rate_properties_run(
			ofa_main_page_get_main_window( page ), taux )){

		/* nothing to do here as all is managed by dossier signaling
		 * system */

	} else {
		g_object_unref( taux );
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RATE( object )){
		insert_new_row( self, OFO_RATE( object ), TRUE );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	ofaTauxSetPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoRate *taux;

	g_return_if_fail( page && OFA_IS_TAUX_SET( page ));

	priv = OFA_TAUX_SET( page )->private;

	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );

		if( ofa_rate_properties_run(
				ofa_main_page_get_main_window( page ), taux )){

			/* nothing to do here as all is managed by dossier
			 * signaling system */
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_updated_object";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_RATE( object )){
		if( find_row_by_mnemo( self, prev_id, &tmodel, &iter )){
			set_row_by_iter( self, tmodel, &iter, OFO_RATE( object ));
		} else {
			g_warning( "%s: unable to find '%s' rate", thisfn, prev_id );
		}
	}
}

static void
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	ofaTauxSetPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoRate *taux;

	g_return_if_fail( page && OFA_IS_TAUX_SET( page ));

	priv = OFA_TAUX_SET( page )->private;

	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &taux, -1 );
		g_object_unref( taux );

		if( delete_confirmed( OFA_TAUX_SET( page ), taux ) &&
				ofo_rate_delete( taux )){

			/* nothing to do here as all is managed by dossier signaling
			 * system */
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static gboolean
delete_confirmed( ofaTauxSet *self, ofoRate *taux )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' rate ?" ),
			ofo_rate_get_mnemo( taux ),
			ofo_rate_get_label( taux ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_deleted_object";
	static const gchar *mnemo;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_RATE( object )){
		mnemo = ofo_rate_get_mnemo( OFO_RATE( object ));
		if( find_row_by_mnemo( self, mnemo, &tmodel, &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		} else {
			g_warning( "%s: unable to find '%s' rate", thisfn, mnemo );
		}
	}
}

static void
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaTauxSet *self )
{
	static const gchar *thisfn = "ofa_taux_set_on_reloaded_dataset";
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_RATE ){
		tmodel = gtk_tree_view_get_model( self->private->tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}
