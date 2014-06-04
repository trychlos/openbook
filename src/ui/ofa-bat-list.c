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
#include "ui/ofa-bat-list.h"
#include "ui/ofo-base.h"
#include "ui/ofo-bat.h"
#include "ui/ofo-dossier.h"

/* private instance data
 */
struct _ofaBatListPrivate {
	gboolean         dispose_has_run;

	/* input data
	 */
	GtkContainer    *container;
	ofoDossier      *dossier;
	gboolean         with_tree_view;
	gboolean         editable;
	ofaBatListCb     pfnSelection;
	ofaBatListCb     pfnActivation;
	gpointer         user_data;

	/* UI
	 */
	GtkTreeView     *tview;					/* the BAT treeview */
	GtkBox          *box;					/* the main container of the detail widgets */
	GtkEntry        *id;
	GtkEntry        *format;
	GtkEntry        *count;
	GtkEntry        *begin;
	GtkEntry        *end;
	GtkEntry        *rib;
	GtkEntry        *devise;
	GtkEntry        *solde;

	/* just to make the my_utils_init_..._ex macros happy
	 */
	const ofoBat    *bat;
	gboolean         is_new;
};

/* column ordering in the treeview
 */
enum {
	COL_ID = 0,
	COL_URI,
	COL_OBJECT,
	N_COLUMNS
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-bat-list.ui";
static const gchar *st_ui_id  = "BatListWindow";

G_DEFINE_TYPE( ofaBatList, ofa_bat_list, G_TYPE_OBJECT )

static gboolean      do_move_between_containers( ofaBatList *self );
static void          setup_treeview( ofaBatList *self );
static void          init_treeview( ofaBatList *self );
static void          insert_new_row( ofaBatList *self, ofoBat *bat, gboolean with_selection );
static void          setup_first_selection( ofaBatList *self );
static void          set_editable_widgets( ofaBatList *self );
static void          on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatList *self );
static void          on_selection_changed( GtkTreeSelection *selection, ofaBatList *self );
static void          setup_bat_properties( const ofaBatList *self, const ofoBat *bat );
static const ofoBat *get_selected_object( const ofaBatList *self, GtkTreeSelection *selection );

static void
bat_list_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_list_finalize";
	ofaBatListPrivate *priv;

	g_return_if_fail( OFA_IS_BAT_LIST( instance ));

	priv = OFA_BAT_LIST( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_list_parent_class )->finalize( instance );
}

static void
bat_list_dispose( GObject *instance )
{
	ofaBatListPrivate *priv;

	g_return_if_fail( OFA_IS_BAT_LIST( instance ));

	priv = ( OFA_BAT_LIST( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_list_parent_class )->dispose( instance );
}

static void
ofa_bat_list_init( ofaBatList *self )
{
	static const gchar *thisfn = "ofa_bat_list_init";

	g_return_if_fail( OFA_IS_BAT_LIST( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaBatListPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_bat_list_class_init( ofaBatListClass *klass )
{
	static const gchar *thisfn = "ofa_bat_list_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_list_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_list_finalize;
}

static void
on_container_finalized( ofaBatList *self, gpointer this_was_the_container )
{
	g_return_if_fail( self && OFA_IS_BAT_LIST( self ));
	g_object_unref( self );
}

/**
 * ofa_bat_list_init_dialog:
 */
ofaBatList *
ofa_bat_list_init_dialog( const ofaBatListParms *parms )
{
	static const gchar *thisfn = "ofa_bat_list_init_dialog";
	ofaBatList *self;
	ofaBatListPrivate *priv;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( GTK_IS_CONTAINER( parms->container ), NULL );
	g_return_val_if_fail( OFO_IS_DOSSIER( parms->dossier ), NULL );

	self = g_object_new( OFA_TYPE_BAT_LIST, NULL );

	priv = self->private;

	/* parms data */
	priv->container = parms->container;
	priv->dossier = parms->dossier;
	priv->with_tree_view = parms->with_tree_view;
	priv->editable = parms->editable;
	priv->pfnSelection = parms->pfnSelection;
	priv->pfnActivation = parms->pfnActivation;
	priv->user_data = parms->user_data;

	/* setup a weak reference on the dialog to auto-unref */
	g_object_weak_ref( G_OBJECT( priv->container ), ( GWeakNotify ) on_container_finalized, self );

	/* then initialize the dialog */
	if( do_move_between_containers( self )){

		if( priv->with_tree_view ){
			setup_treeview( self );
			init_treeview( self );
			setup_first_selection( self );
		}

		set_editable_widgets( self );
	}

	return( self );
}

static gboolean
do_move_between_containers( ofaBatList *self )
{
	ofaBatListPrivate *priv;
	GtkWidget *window;
	GtkWidget *tview;
	GtkWidget *box;
	GtkWidget *entry;

	priv = self->private;

	/* load our fake window */
	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), FALSE );

	/* identify our main containers */
	if( priv->with_tree_view ){
		tview = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "p0-treeview" );
		g_return_val_if_fail( tview && GTK_IS_TREE_VIEW( tview ), FALSE );
		priv->tview = GTK_TREE_VIEW( tview );
	}

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "p0-box" );
	g_return_val_if_fail( box && GTK_IS_BOX( box ), FALSE );
	priv->box = GTK_BOX( box );

	/* identifier the widgets for the properties */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-id" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->id = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-format" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->format = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-count" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	gtk_entry_set_alignment( GTK_ENTRY( entry ), 1.0 );
	priv->count = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-begin" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->begin = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-end" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->end = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-rib" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->rib = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-devise" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->devise = GTK_ENTRY( entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-solde" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->solde = GTK_ENTRY( entry );

	/* attach our grid container to the client's one */
	if( priv->with_tree_view ){
		box = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-box" );
	} else {
		box = GTK_WIDGET( priv->box );
	}
	g_return_val_if_fail( box && GTK_IS_BOX( box ), FALSE );

	gtk_widget_reparent( box, GTK_WIDGET( priv->container ));

	return( TRUE );
}

static void
setup_treeview( ofaBatList *self )
{
	ofaBatListPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->private;

	g_signal_connect(
			G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_row_activated), self );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_tree_view_set_model( priv->tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "ellipsize", PANGO_ELLIPSIZE_START, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "URI" ),
			text_cell, "text", COL_URI,
			NULL );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( priv->tview, column );

	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_selection_changed ), self );
}

static void
init_treeview( ofaBatList *self )
{
	GList *dataset, *iset;

	dataset = ofo_bat_get_dataset( self->private->dossier );

	for( iset=dataset ; iset ; iset=iset->next ){
		insert_new_row( self, OFO_BAT( iset->data ), FALSE );
	}
}

static void
insert_new_row( ofaBatList *self, ofoBat *bat, gboolean with_selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	tmodel = gtk_tree_view_get_model( self->private->tview );

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tmodel ),
			&iter,
			-1,
			COL_URI,    ofo_bat_get_uri( bat ),
			COL_OBJECT, bat,
			-1 );

	/* select the newly inserted row */
	if( with_selection ){
		path = gtk_tree_model_get_path( tmodel, &iter );
		gtk_tree_view_set_cursor( GTK_TREE_VIEW( self->private->tview ), path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( self->private->tview ));
	}
}

static void
setup_first_selection( ofaBatList *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( self->private->tview ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( self->private->tview ));
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( self->private->tview ));
}

/*
 * only notes are editable by the user
 */
static void
set_editable_widgets( ofaBatList *self )
{
	GtkWidget *notes;

	notes = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->box ), "pn-notes" );
	g_return_if_fail( notes && GTK_IS_TEXT_VIEW( notes ));
	gtk_widget_set_sensitive( notes, self->private->editable );
}

static void
on_row_activated( GtkTreeView *tview,
		GtkTreePath *path, GtkTreeViewColumn *column, ofaBatList *self )
{
	GtkTreeSelection *select;
	const ofoBat *bat;

	select = gtk_tree_view_get_selection( self->private->tview );
	bat = get_selected_object( self, select );
	if( bat && self->private->pfnActivation ){
			( *self->private->pfnActivation )( bat, self->private->user_data );
	}
}

static void
on_selection_changed( GtkTreeSelection *selection, ofaBatList *self )
{
	const ofoBat *bat;

	bat = get_selected_object( self, selection );

	setup_bat_properties( self, bat );

	if( bat && self->private->pfnSelection ){
			( *self->private->pfnSelection )( bat, self->private->user_data );
	}
}

static void
setup_bat_properties( const ofaBatList *self, const ofoBat *bat )
{
	ofaBatListPrivate *priv;
	const gchar *conststr;
	gchar *str;
	const GDate *begin, *end;

	priv = self->private;

	str = g_strdup_printf( "%d", ofo_bat_get_id( bat ));
	gtk_entry_set_text( priv->id, str );
	g_free( str );

	conststr = ofo_bat_get_format( bat );
	if( conststr ){
		gtk_entry_set_text( priv->format, conststr );
	} else {
		gtk_entry_set_text( priv->format, "" );
	}

	str = g_strdup_printf( "%d", ofo_bat_get_count( bat ));
	gtk_entry_set_text( priv->count, str );
	g_free( str );

	begin = ofo_bat_get_begin( bat );
	if( g_date_valid( begin )){
		str = my_utils_display_from_date( begin, MY_UTILS_DATE_DDMM );
		gtk_entry_set_text( priv->begin, str );
		g_free( str );
	} else {
		gtk_entry_set_text( priv->begin, "" );
	}

	end = ofo_bat_get_end( bat );
	if( g_date_valid( end )){
		str = my_utils_display_from_date( end, MY_UTILS_DATE_DDMM );
		gtk_entry_set_text( priv->end, str );
		g_free( str );
	} else {
		gtk_entry_set_text( priv->end, "" );
	}

	conststr = ofo_bat_get_rib( bat );
	if( conststr ){
		gtk_entry_set_text( priv->rib, conststr );
	} else {
		gtk_entry_set_text( priv->rib, "" );
	}

	conststr = ofo_bat_get_currency( bat );
	if( conststr ){
		gtk_entry_set_text( priv->devise, conststr );
	} else {
		gtk_entry_set_text( priv->devise, "" );
	}

	if( ofo_bat_get_solde_set( bat )){
		str = g_strdup_printf( "%.2lf", ofo_bat_get_solde( bat ));
		gtk_entry_set_text( priv->solde, str );
		g_free( str );
	} else {
		gtk_entry_set_text( priv->solde, "" );
	}

	priv->bat = bat;
	my_utils_init_notes_ex( priv->box, bat );
	my_utils_init_maj_user_stamp_ex( priv->box, bat );
}

static const ofoBat *
get_selected_object( const ofaBatList *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	bat = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter,
				COL_OBJECT, &bat,
				-1 );
		g_object_unref( bat );
	}

	return( bat );
}

/**
 * ofa_bat_list_set_bat:
 */
void
ofa_bat_list_set_bat( const ofaBatList *self, const ofoBat *bat )
{
	g_return_if_fail( OFA_IS_BAT_LIST( self ));
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !self->private->dispose_has_run ){

		setup_bat_properties( self, bat );
	}
}

/**
 * ofa_bat_list_get_selection:
 * @self:
 *
 * Returns the currently selected object.
 */
const ofoBat *
ofa_bat_list_get_selection( const ofaBatList *self )
{
	GtkTreeSelection *select;
	const ofoBat *bat;

	g_return_val_if_fail( self && OFA_IS_BAT_LIST( self ), NULL );

	bat = NULL;

	if( !self->private->dispose_has_run ){

		select = gtk_tree_view_get_selection( self->private->tview );
		bat = get_selected_object( self, select );
	}

	return( bat );
}
