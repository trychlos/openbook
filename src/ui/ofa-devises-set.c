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

#include "ui/ofa-main-page.h"
#include "ui/ofa-devise-properties.h"
#include "ui/ofa-devises-set.h"
#include "ui/ofo-devise.h"

/* private class data
 */
struct _ofaDevisesSetClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaDevisesSetPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* UI
	 */
	GtkTreeView   *view;				/* the treeview on the devises set */
};

/* column ordering in the selection listview
 */
enum {
	COL_CODE = 0,
	COL_LABEL,
	COL_SYMBOL,
	COL_OBJECT,
	N_COLUMNS
};

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaDevisesSetClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static GtkWidget *v_setup_view( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       insert_new_row( ofaDevisesSet *self, ofoDevise *devise, gboolean with_selection );
static gint       on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDevisesSet *self );
static void       setup_first_selection( ofaDevisesSet *self );
static void       store_set_devise( GtkTreeModel *model, GtkTreeIter *iter, const ofoDevise *devise );
static void       on_devise_selected( GtkTreeSelection *selection, ofaDevisesSet *self );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaDevisesSet *self, ofoDevise *devise );

GType
ofa_devises_set_get_type( void )
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
	static const gchar *thisfn = "ofa_devises_set_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaDevisesSetClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaDevisesSet ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaDevisesSet", &info, 0 );

	return( type );
}

static void
class_init( ofaDevisesSetClass *klass )
{
	static const gchar *thisfn = "ofa_devises_set_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaDevisesSetClassPrivate, 1 );

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_devises_set_instance_init";
	ofaDevisesSet *self;

	g_return_if_fail( OFA_IS_DEVISES_SET( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",

			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_DEVISES_SET( instance );

	self->private = g_new0( ofaDevisesSetPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_devises_set_instance_dispose";
	ofaDevisesSetPrivate *priv;

	g_return_if_fail( OFA_IS_DEVISES_SET( instance ));

	priv = ( OFA_DEVISES_SET( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_devises_set_instance_finalize";
	ofaDevisesSet *self;

	g_return_if_fail( OFA_IS_DEVISES_SET( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_DEVISES_SET( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_devises_set_run:
 *
 * When called by the main_window, the page has been created, showed
 * and activated - there is nothing left to do here....
 */
void
ofa_devises_set_run( ofaMainPage *this )
{
	static const gchar *thisfn = "ofa_devises_set_run";

	g_return_if_fail( this && OFA_IS_DEVISES_SET( this ));

	g_debug( "%s: this=%p (%s)",
			thisfn, ( void * ) this, G_OBJECT_TYPE_NAME( this ));
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	ofaDevisesSet *self;
	GtkFrame *frame;
	GtkScrolledWindow *scroll;
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	self = OFA_DEVISES_SET( page );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ));
	gtk_container_set_border_width( GTK_CONTAINER( scroll ), 4 );
	gtk_scrolled_window_set_policy( scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( frame ), GTK_WIDGET( scroll ));

	view = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_widget_set_vexpand( GTK_WIDGET( view ), TRUE );
	gtk_tree_view_set_headers_visible( view, TRUE );
	gtk_container_add( GTK_CONTAINER( scroll ), GTK_WIDGET( view ));
	self->private->view = GTK_TREE_VIEW( view );

	model = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( view, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "ISO 3A code" ),
			text_cell, "text", COL_CODE,
			NULL );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			text_cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_append_column( view, column );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Symbol" ),
			text_cell, "text", COL_SYMBOL,
			NULL );
	gtk_tree_view_append_column( view, column );

	select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect( G_OBJECT( select ), "changed", G_CALLBACK( on_devise_selected ), self );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( model ), ( GtkTreeIterCompareFunc ) on_sort_model, self, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( model ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( GTK_WIDGET( frame ));
}

static void
v_init_view( ofaMainPage *page )
{
	ofaDevisesSet *self;
	ofoDossier *dossier;
	GList *dataset, *iset;
	ofoDevise *devise;

	self = OFA_DEVISES_SET( page );
	dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	dataset = ofo_devise_get_dataset( dossier );

	for( iset=dataset ; iset ; iset=iset->next ){

		devise = OFO_DEVISE( iset->data );
		insert_new_row( self, devise, FALSE );
	}

	setup_first_selection( self );
}

static void
insert_new_row( ofaDevisesSet *self, ofoDevise *devise, gboolean with_selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	tmodel = gtk_tree_view_get_model( self->private->view );
	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_CODE,   ofo_devise_get_code( devise ),
			COL_LABEL,  ofo_devise_get_label( devise ),
			COL_SYMBOL, ofo_devise_get_symbol( devise ),
			COL_OBJECT, devise,
			-1 );

	/* select the newly added devise */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( self->private->view, path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDevisesSet *self )
{
	gchar *acod, *bcod, *afold, *bfold;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_CODE, &acod, -1 );
	afold = g_utf8_casefold( acod, -1 );

	gtk_tree_model_get( tmodel, b, COL_CODE, &bcod, -1 );
	bfold = g_utf8_casefold( bcod, -1 );

	cmp = g_utf8_collate( afold, bfold );

	g_free( acod );
	g_free( afold );
	g_free( bcod );
	g_free( bfold );

	return( cmp );
}

static void
setup_first_selection( ofaDevisesSet *self )
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	model = gtk_tree_view_get_model( self->private->view );
	if( gtk_tree_model_get_iter_first( model, &iter )){
		select = gtk_tree_view_get_selection( self->private->view );
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
}

static void
store_set_devise( GtkTreeModel *model, GtkTreeIter *iter, const ofoDevise *devise )
{
	gtk_list_store_set(
			GTK_LIST_STORE( model ),
			iter,
			COL_CODE,  ofo_devise_get_code( devise ),
			COL_LABEL,  ofo_devise_get_label( devise ),
			COL_SYMBOL, ofo_devise_get_symbol( devise ),
			COL_OBJECT, devise,
			-1 );
}

static void
on_devise_selected( GtkTreeSelection *selection, ofaDevisesSet *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoDevise *devise;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &devise, -1 );
		g_object_unref( devise );
	}

	gtk_widget_set_sensitive(
			ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
			devise && OFO_IS_DEVISE( devise ));

	gtk_widget_set_sensitive(
			ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
			devise && OFO_IS_DEVISE( devise ) && ofo_devise_is_deletable( devise ));
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	ofoDevise *devise;

	devise = ofo_devise_new();

	if( ofa_devise_properties_run(
			ofa_main_page_get_main_window( page ), devise )){

		insert_new_row( OFA_DEVISES_SET( page ), devise, TRUE );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	ofaDevisesSet *self;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoDevise *devise;

	self = OFA_DEVISES_SET( page );
	select = gtk_tree_view_get_selection( self->private->view );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &devise, -1 );
		g_object_unref( devise );

		if( ofa_devise_properties_run(
				ofa_main_page_get_main_window( page ), devise )){

			store_set_devise( tmodel, &iter, devise );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
}

static void
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	ofaDevisesSet *self;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoDevise *devise;

	self = OFA_DEVISES_SET( page );
	select = gtk_tree_view_get_selection( self->private->view );

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &devise, -1 );
		g_object_unref( devise );

		if( delete_confirmed( self, devise ) &&
				ofo_devise_delete( devise, ofa_main_page_get_dossier( page ))){

			/* remove the row from the tmodel
			 * this will cause an automatic new selection */
			gtk_list_store_remove( GTK_LIST_STORE( tmodel ), &iter );
		}
	}

	gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
}

static gboolean
delete_confirmed( ofaDevisesSet *self, ofoDevise *devise )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' currency ?" ),
			ofo_devise_get_code( devise ),
			ofo_devise_get_label( devise ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}
