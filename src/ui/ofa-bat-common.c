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
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-common.h"

/* private instance data
 */
struct _ofaBatCommonPrivate {
	gboolean         dispose_has_run;

	/* input data
	 */
	GtkContainer    *container;
	ofoDossier      *dossier;
	gboolean         with_tree_view;
	gboolean         editable;
	ofaBatCommonCb   pfnSelection;
	ofaBatCommonCb   pfnActivation;
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
	GtkEntry        *currency;
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

static const gchar *st_ui_xml = PKGUIDIR "/ofa-bat-common.ui";
static const gchar *st_ui_id  = "BatCommonWindow";

G_DEFINE_TYPE( ofaBatCommon, ofa_bat_common, G_TYPE_OBJECT )

static gboolean      do_move_between_containers( ofaBatCommon *self );
static void          setup_treeview( ofaBatCommon *self );
static void          init_treeview( ofaBatCommon *self );
static void          insert_new_row( ofaBatCommon *self, ofoBat *bat, gboolean with_selection );
static void          setup_first_selection( ofaBatCommon *self );
static void          set_editable_widgets( ofaBatCommon *self );
static void          on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatCommon *self );
static void          on_selection_changed( GtkTreeSelection *selection, ofaBatCommon *self );
static void          setup_bat_properties( const ofaBatCommon *self, const ofoBat *bat );
static const ofoBat *get_selected_object( const ofaBatCommon *self, GtkTreeSelection *selection );

static void
bat_common_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_common_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_COMMON( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_common_parent_class )->finalize( instance );
}

static void
bat_common_dispose( GObject *instance )
{
	ofaBatCommonPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_COMMON( instance ));

	priv = ( OFA_BAT_COMMON( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_common_parent_class )->dispose( instance );
}

static void
ofa_bat_common_init( ofaBatCommon *self )
{
	static const gchar *thisfn = "ofa_bat_common_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_COMMON( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_COMMON, ofaBatCommonPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_bat_common_class_init( ofaBatCommonClass *klass )
{
	static const gchar *thisfn = "ofa_bat_common_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_common_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_common_finalize;

	g_type_class_add_private( klass, sizeof( ofaBatCommonPrivate ));
}

static void
on_container_finalized( ofaBatCommon *self, gpointer this_was_the_container )
{
	g_return_if_fail( self && OFA_IS_BAT_COMMON( self ));
	g_object_unref( self );
}

/**
 * ofa_bat_common_init_dialog:
 */
ofaBatCommon *
ofa_bat_common_init_dialog( const ofsBatCommonParms *parms )
{
	static const gchar *thisfn = "ofa_bat_common_init_dialog";
	ofaBatCommon *self;
	ofaBatCommonPrivate *priv;

	g_return_val_if_fail( parms, NULL );

	g_debug( "%s: parms=%p", thisfn, ( void * ) parms );

	g_return_val_if_fail( GTK_IS_CONTAINER( parms->container ), NULL );
	g_return_val_if_fail( OFO_IS_DOSSIER( parms->dossier ), NULL );

	self = g_object_new( OFA_TYPE_BAT_COMMON, NULL );

	priv = self->priv;

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
do_move_between_containers( ofaBatCommon *self )
{
	ofaBatCommonPrivate *priv;
	GtkWidget *window;
	GtkWidget *tview;
	GtkWidget *box;
	GtkWidget *entry;

	priv = self->priv;

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

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "p1-currency" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	priv->currency = GTK_ENTRY( entry );

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
setup_treeview( ofaBatCommon *self )
{
	ofaBatCommonPrivate *priv;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

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
init_treeview( ofaBatCommon *self )
{
	GList *dataset, *iset;

	dataset = ofo_bat_get_dataset( self->priv->dossier );

	for( iset=dataset ; iset ; iset=iset->next ){
		insert_new_row( self, OFO_BAT( iset->data ), FALSE );
	}
}

static void
insert_new_row( ofaBatCommon *self, ofoBat *bat, gboolean with_selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	tmodel = gtk_tree_view_get_model( self->priv->tview );

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
		gtk_tree_view_set_cursor( GTK_TREE_VIEW( self->priv->tview ), path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( self->priv->tview ));
	}
}

static void
setup_first_selection( ofaBatCommon *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( self->priv->tview ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( self->priv->tview ));
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( self->priv->tview ));
}

/*
 * only notes are editable by the user
 */
static void
set_editable_widgets( ofaBatCommon *self )
{
	GtkWidget *notes;

	notes = my_utils_container_get_child_by_name( GTK_CONTAINER( self->priv->box ), "pn-notes" );
	g_return_if_fail( notes && GTK_IS_TEXT_VIEW( notes ));
	gtk_widget_set_sensitive( notes, self->priv->editable );
}

static void
on_row_activated( GtkTreeView *tview,
		GtkTreePath *path, GtkTreeViewColumn *column, ofaBatCommon *self )
{
	GtkTreeSelection *select;
	const ofoBat *bat;

	select = gtk_tree_view_get_selection( self->priv->tview );
	bat = get_selected_object( self, select );
	if( bat && self->priv->pfnActivation ){
			( *self->priv->pfnActivation )( bat, self->priv->user_data );
	}
}

static void
on_selection_changed( GtkTreeSelection *selection, ofaBatCommon *self )
{
	const ofoBat *bat;
	ofaBatCommonPrivate *priv;

	bat = get_selected_object( self, selection );
	setup_bat_properties( self, bat );
	priv = self->priv;

	if( bat && priv->pfnSelection ){
			( *priv->pfnSelection )( bat, priv->user_data );
	}
}

static void
setup_bat_properties( const ofaBatCommon *self, const ofoBat *bat )
{
	ofaBatCommonPrivate *priv;
	const gchar *conststr;
	gchar *str;

	priv = self->priv;

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

	str = my_date_to_str( ofo_bat_get_begin( bat ), MY_DATE_DMYY );
	gtk_entry_set_text( priv->begin, str );
	g_free( str );

	str = my_date_to_str( ofo_bat_get_end( bat ), MY_DATE_DMYY );
	gtk_entry_set_text( priv->end, str );
	g_free( str );

	conststr = ofo_bat_get_rib( bat );
	if( conststr ){
		gtk_entry_set_text( priv->rib, conststr );
	} else {
		gtk_entry_set_text( priv->rib, "" );
	}

	conststr = ofo_bat_get_currency( bat );
	if( conststr ){
		gtk_entry_set_text( priv->currency, conststr );
	} else {
		gtk_entry_set_text( priv->currency, "" );
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
	my_utils_init_upd_user_stamp_ex( priv->box, bat );
}

static const ofoBat *
get_selected_object( const ofaBatCommon *self, GtkTreeSelection *selection )
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
 * ofa_bat_common_set_bat:
 */
void
ofa_bat_common_set_bat( const ofaBatCommon *self, const ofoBat *bat )
{
	g_return_if_fail( OFA_IS_BAT_COMMON( self ));
	g_return_if_fail( OFO_IS_BAT( bat ));

	if( !self->priv->dispose_has_run ){

		setup_bat_properties( self, bat );
	}
}

/**
 * ofa_bat_common_get_selection:
 * @self:
 *
 * Returns the currently selected object.
 */
const ofoBat *
ofa_bat_common_get_selection( const ofaBatCommon *self )
{
	GtkTreeSelection *select;
	const ofoBat *bat;

	g_return_val_if_fail( self && OFA_IS_BAT_COMMON( self ), NULL );

	bat = NULL;

	if( !self->priv->dispose_has_run ){

		select = gtk_tree_view_get_selection( self->priv->tview );
		bat = get_selected_object( self, select );
	}

	return( bat );
}
