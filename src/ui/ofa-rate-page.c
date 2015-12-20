/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"
#include "api/ofo-rate.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-buttons-box.h"
#include "ui/ofa-rate-properties.h"
#include "ui/ofa-rate-page.h"

/* private instance data
 */
struct _ofaRatePagePrivate {

	/* internals
	 */
	GList        *handlers;
	gboolean      is_current;

	/* UI
	 */
	GtkTreeView  *tview;
	GtkTreeModel *tmodel;
	GtkWidget    *new_btn;
	GtkWidget    *update_btn;
	GtkWidget    *delete_btn;
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

G_DEFINE_TYPE( ofaRatePage, ofa_rate_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static void       setup_dossier_signaling( ofaRatePage *self );
static GtkWidget *setup_tree_view( ofaRatePage *self );
static GtkWidget *v_setup_buttons( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_dataset( ofaRatePage *self );
static void       insert_new_row( ofaRatePage *self, ofoRate *rate, gboolean with_selection );
static void       set_row_by_iter( ofaRatePage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoRate *rate );
static gboolean   find_row_by_mnemo( ofaRatePage *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter );
static gchar     *get_min_val_date( ofoRate *rate );
static gchar     *get_max_val_date( ofoRate *rate );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRatePage *self );
static void       setup_first_selection( ofaRatePage *self );
static gboolean   on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRatePage *self );
static void       on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page );
static void       on_row_selected( GtkTreeSelection *selection, ofaRatePage *self );
static void       on_new_clicked( GtkButton *button, ofaRatePage *page );
static void       on_new_object( ofoDossier *dossier, ofoBase *object, ofaRatePage *self );
static void       on_update_clicked( GtkButton *button, ofaRatePage *page );
static void       on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaRatePage *self );
static void       on_delete_clicked( GtkButton *button, ofaRatePage *page );
static gboolean   delete_confirmed( ofaRatePage *self, ofoRate *rate );
static void       on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaRatePage *self );
static void       on_reloaded_dataset( ofoDossier *dossier, GType type, ofaRatePage *self );

static void
rates_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rate_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_RATE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_page_parent_class )->finalize( instance );
}

static void
rates_page_dispose( GObject *instance )
{
	ofaRatePagePrivate *priv;
	gulong handler_id;
	GList *iha;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_RATE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = ( OFA_RATE_PAGE( instance ))->priv;

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		dossier = ofa_page_get_dossier( OFA_PAGE( instance ));
		if( OFO_IS_DOSSIER( dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_page_parent_class )->dispose( instance );
}

static void
ofa_rate_page_init( ofaRatePage *self )
{
	static const gchar *thisfn = "ofa_rate_page_instance_init";

	g_return_if_fail( OFA_IS_RATE_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RATE_PAGE, ofaRatePagePrivate );

	self->priv->handlers = NULL;
}

static void
ofa_rate_page_class_init( ofaRatePageClass *klass )
{
	static const gchar *thisfn = "ofa_rate_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rates_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = rates_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaRatePagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_rate_page_v_setup_view";
	ofaRatePagePrivate *priv;
	GtkWidget *tview;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_RATE_PAGE( page )->priv;

	setup_dossier_signaling( OFA_RATE_PAGE( page ));
	tview = setup_tree_view( OFA_RATE_PAGE( page ));
	priv->is_current = ofo_dossier_is_current( ofa_page_get_dossier( page ));
	insert_dataset( OFA_RATE_PAGE( page ));

	return( tview );
}

static void
setup_dossier_signaling( ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = self->priv;
	dossier = ofa_page_get_dossier( OFA_PAGE( self ));

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
						G_OBJECT( dossier ),
						SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reloaded_dataset ), self );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

static GtkWidget *
setup_tree_view( ofaRatePage *self )
{
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	my_utils_widget_set_margin( GTK_WIDGET( frame ), 4, 4, 4, 0 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_hexpand( GTK_WIDGET( tview ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( tview ), TRUE );
	gtk_tree_view_set_headers_visible( tview, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( tview ));
	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect(
			G_OBJECT( tview ), "key-press-event", G_CALLBACK( on_tview_key_pressed ), self );
	self->priv->tview = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );
	self->priv->tmodel = tmodel;

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

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaRatePagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = OFA_RATE_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );
	priv->new_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_NEW, TRUE, G_CALLBACK( on_new_clicked ), page );
	priv->update_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_PROPERTIES, FALSE, G_CALLBACK( on_update_clicked ), page );
	priv->delete_btn = ofa_buttons_box_add_button(
			buttons_box, BUTTON_DELETE, FALSE, G_CALLBACK( on_delete_clicked ), page );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_RATE_PAGE( page ), NULL );

	return( GTK_WIDGET( OFA_RATE_PAGE( page )->priv->tview ));
}

static void
insert_dataset( ofaRatePage *self )
{
	GList *dataset, *it;
	ofoRate *rate;

	dataset = ofo_rate_get_dataset( ofa_page_get_dossier( OFA_PAGE( self )));

	for( it=dataset ; it ; it=it->next ){

		rate = OFO_RATE( it->data );
		insert_new_row( self, rate, FALSE );
	}

	setup_first_selection( self );
}

/*
 * we insert the mnemo as soon as the row is created, so that the
 * on_sort_model() function does not complain about null strings
 */
static void
insert_new_row( ofaRatePage *self, ofoRate *rate, gboolean with_selection )
{
	ofaRatePagePrivate *priv;
	GtkTreeIter iter;
	GtkTreePath *path;

	priv = self->priv;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( priv->tmodel ),
			&iter,
			-1,
			COL_MNEMO,  ofo_rate_get_mnemo( rate ),
			COL_OBJECT, rate,
			-1 );

	set_row_by_iter( self, priv->tmodel, &iter, rate );

	/* select the newly added rate */
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
set_row_by_iter( ofaRatePage *self, GtkTreeModel *tmodel, GtkTreeIter *iter, ofoRate *rate )
{
	gchar *sbegin, *send;

	sbegin = get_min_val_date( rate );
	send = get_max_val_date( rate );

	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			iter,
			COL_MNEMO,  ofo_rate_get_mnemo( rate ),
			COL_LABEL,  ofo_rate_get_label( rate ),
			COL_BEGIN,  sbegin,
			COL_END,    send,
			-1 );

	g_free( sbegin );
	g_free( send );
}

static gboolean
find_row_by_mnemo( ofaRatePage *self, const gchar *mnemo, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	gchar *row_mnemo;
	gint cmp;

	*tmodel = gtk_tree_view_get_model( self->priv->tview );

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
get_min_val_date( ofoRate *rate )
{
	const GDate *dmin;
	gchar *str, *sbegin;

	dmin = ofo_rate_get_min_valid( rate );

	if( my_date_is_valid( dmin )){
		str = my_date_to_str( dmin, ofa_prefs_date_check());
		sbegin = g_strdup_printf( _( "from %s" ), str );
		g_free( str );

	} else {
		sbegin = g_strdup_printf( _( "from infinite" ));
	}

	return( sbegin );
}

static gchar *
get_max_val_date( ofoRate *rate )
{
	const GDate *dmax;
	gchar *str, *send;

	dmax = ofo_rate_get_max_valid( rate );

	if( my_date_is_valid( dmax )){
		str = my_date_to_str( dmax, ofa_prefs_date_check());
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
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaRatePage *self )
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
setup_first_selection( ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;

	if( gtk_tree_model_get_iter_first( priv->tmodel, &iter )){
		select = gtk_tree_view_get_selection( priv->tview );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_tview_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaRatePage *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Insert ){
			on_new_clicked( NULL, self );

		} else if( event->keyval == GDK_KEY_Delete ){
			on_delete_clicked( NULL, self );
		}
	}

	return( stop );
}

/*
 * double click on a row opens the rate properties
 */
static void
on_row_activated( GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, ofaPage *page )
{
	on_update_clicked( NULL, OFA_RATE_PAGE( page ));
}

static void
on_row_selected( GtkTreeSelection *selection, ofaRatePage *self )
{
	ofaRatePagePrivate *priv;
	GtkTreeIter iter;
	ofoRate *rate;

	priv = self->priv;

	if( gtk_tree_selection_get_selected( selection, NULL, &iter )){
		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &rate, -1 );
		g_object_unref( rate );
	}

	if( priv->new_btn ){
		gtk_widget_set_sensitive(
				priv->new_btn,
				rate && OFO_IS_RATE( rate ) && priv->is_current );
	}

	if( priv->update_btn ){
		gtk_widget_set_sensitive(
				priv->update_btn,
				rate && OFO_IS_RATE( rate ));
	}

	if( priv->delete_btn ){
		gtk_widget_set_sensitive(
				priv->delete_btn,
				rate && OFO_IS_RATE( rate ) &&
					ofo_rate_is_deletable( rate, ofa_page_get_dossier( OFA_PAGE( self ))));
	}
}

static void
on_new_clicked( GtkButton *button, ofaRatePage *page )
{
	ofoRate *rate;

	rate = ofo_rate_new();

	if( ofa_rate_properties_run(
			ofa_page_get_main_window( OFA_PAGE( page )), rate )){

		/* nothing to do here as all is managed by dossier signaling
		 * system */

	} else {
		g_object_unref( rate );
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaRatePage *self )
{
	static const gchar *thisfn = "ofa_rate_page_on_new_object";

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
on_update_clicked( GtkButton *button, ofaRatePage *page )
{
	ofaRatePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoRate *rate;

	priv = page->priv;

	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &rate, -1 );
		g_object_unref( rate );

		if( ofa_rate_properties_run(
				ofa_page_get_main_window( OFA_PAGE( page )), rate )){

			/* nothing to do here as all is managed by dossier
			 * signaling system */
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaRatePage *self )
{
	static const gchar *thisfn = "ofa_rate_page_on_updated_object";
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
on_delete_clicked( GtkButton *button, ofaRatePage *page )
{
	ofaRatePagePrivate *priv;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	ofoRate *rate;

	priv = page->priv;

	select = gtk_tree_view_get_selection( priv->tview );

	if( gtk_tree_selection_get_selected( select, NULL, &iter )){

		gtk_tree_model_get( priv->tmodel, &iter, COL_OBJECT, &rate, -1 );
		g_object_unref( rate );

		if( delete_confirmed( page, rate ) &&
				ofo_rate_delete( rate, ofa_page_get_dossier( OFA_PAGE( page )))){

			/* nothing to do here as all is managed by dossier signaling
			 * system */
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static gboolean
delete_confirmed( ofaRatePage *self, ofoRate *rate )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' rate ?" ),
			ofo_rate_get_mnemo( rate ),
			ofo_rate_get_label( rate ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaRatePage *self )
{
	static const gchar *thisfn = "ofa_rate_page_on_deleted_object";
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
on_reloaded_dataset( ofoDossier *dossier, GType type, ofaRatePage *self )
{
	static const gchar *thisfn = "ofa_rate_page_on_reloaded_dataset";
	GtkTreeModel *tmodel;

	g_debug( "%s: dossier=%p, type=%lu, self=%p",
			thisfn, ( void * ) dossier, type, ( void * ) self );

	if( type == OFO_TYPE_RATE ){
		tmodel = gtk_tree_view_get_model( self->priv->tview );
		gtk_list_store_clear( GTK_LIST_STORE( tmodel ));
		insert_dataset( self );
	}
}
