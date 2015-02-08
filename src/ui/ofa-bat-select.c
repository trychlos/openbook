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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"

#include "core/my-window-prot.h"

#include "ui/ofa-bat-properties-bin.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaBatSelectPrivate {

	/* UI
	 */
	GtkPaned            *paned;
	GtkTreeView         *tview;
	ofaBatPropertiesBin *bat_bin;
	guint                pane_pos;

	/* returned value
	 */
	gint                 bat_id;
};

/* column ordering in the treeview
 */
enum {
	COL_ID = 0,
	COL_URI,
	COL_OBJECT,
	N_COLUMNS
};

static const gchar *st_settings         = "BatSelect-settings";

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-bat-select.ui";
static const gchar *st_ui_id            = "BatSelectDlg";

G_DEFINE_TYPE( ofaBatSelect, ofa_bat_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_pane( ofaBatSelect *self, GtkContainer *parent );
static void      setup_treeview( ofaBatSelect *self, GtkContainer *parent );
static void      init_treeview( ofaBatSelect *self );
static void      insert_new_row( ofaBatSelect *self, ofoBat *bat, gboolean with_selection );
static void      setup_first_selection( ofaBatSelect *self );
static void      setup_properties( ofaBatSelect *self, GtkContainer *parent );
static void      on_selection_changed( GtkTreeSelection *selection, ofaBatSelect *self );
static ofoBat   *get_selected_object( const ofaBatSelect *self, GtkTreeSelection *selection );
static void      on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatSelect *self );
static void      check_for_enable_dlg( ofaBatSelect *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static void      get_settings( ofaBatSelect *self );
static void      set_settings( ofaBatSelect *self );

static void
bat_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_select_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_select_parent_class )->finalize( instance );
}

static void
bat_select_dispose( GObject *instance )
{
	ofaBatSelectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_BAT_SELECT( instance )->priv;

		priv->pane_pos = gtk_paned_get_position( priv->paned );
		set_settings( OFA_BAT_SELECT( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_select_parent_class )->dispose( instance );
}

static void
ofa_bat_select_init( ofaBatSelect *self )
{
	static const gchar *thisfn = "ofa_bat_select_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_SELECT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_SELECT, ofaBatSelectPrivate );
}

static void
ofa_bat_select_class_init( ofaBatSelectClass *klass )
{
	static const gchar *thisfn = "ofa_bat_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_select_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaBatSelectPrivate ));
}

/**
 * ofa_bat_select_run:
 *
 * Returns the selected Bank Account Transaction list (BAT) identifier,
 * or -1.
 */
gint
ofa_bat_select_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_bat_select_run";
	ofaBatSelect *self;
	gint bat_id;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
			OFA_TYPE_BAT_SELECT,
			MY_PROP_MAIN_WINDOW, main_window,
			MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
			MY_PROP_WINDOW_XML,  st_ui_xml,
			MY_PROP_WINDOW_NAME, st_ui_id,
			NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	bat_id = self->priv->bat_id;

	g_object_unref( self );

	return( bat_id );
}

static void
v_init_dialog( myDialog *dialog )
{
	GtkWindow *toplevel;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	get_settings( OFA_BAT_SELECT( dialog ));

	setup_pane( OFA_BAT_SELECT( dialog ), GTK_CONTAINER( toplevel ));
	setup_properties( OFA_BAT_SELECT( dialog ), GTK_CONTAINER( toplevel ));

	setup_treeview( OFA_BAT_SELECT( dialog ), GTK_CONTAINER( toplevel ));
	init_treeview( OFA_BAT_SELECT( dialog ));
	setup_first_selection( OFA_BAT_SELECT( dialog ));

	check_for_enable_dlg( OFA_BAT_SELECT( dialog ));
}

static void
setup_pane( ofaBatSelect *self, GtkContainer *parent )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *pane;

	priv = self->priv;

	pane = my_utils_container_get_child_by_name( parent, "p-paned" );
	g_return_if_fail( pane && GTK_IS_PANED( pane ));
	priv->paned = GTK_PANED( pane );

	gtk_paned_set_position( priv->paned, priv->pane_pos );
}

static void
setup_treeview( ofaBatSelect *self, GtkContainer *parent )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = self->priv;

	tview = my_utils_container_get_child_by_name( parent, "p0-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = GTK_TREE_VIEW( tview );

	g_signal_connect(
			G_OBJECT( tview ), "row-activated", G_CALLBACK( on_row_activated), self );

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
init_treeview( ofaBatSelect *self )
{
	GList *dataset, *it;

	dataset = ofo_bat_get_dataset( MY_WINDOW( self )->prot->dossier );

	for( it=dataset ; it ; it=it->next ){
		insert_new_row( self, OFO_BAT( it->data ), FALSE );
	}
}

static void
insert_new_row( ofaBatSelect *self, ofoBat *bat, gboolean with_selection )
{
	ofaBatSelectPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreePath *path;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( priv->tview );

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
		gtk_tree_view_set_cursor( GTK_TREE_VIEW( priv->tview ), path, NULL, FALSE );
		gtk_tree_path_free( path );
		gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
	}
}

static void
setup_first_selection( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;
	tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->tview ));

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->tview ));
		gtk_tree_selection_select_iter( select, &iter );
	}

	gtk_widget_grab_focus( GTK_WIDGET( priv->tview ));
}

static void
setup_properties( ofaBatSelect *self, GtkContainer *parent )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *container;

	priv = self->priv;

	container = my_utils_container_get_child_by_name( parent, "properties-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->bat_bin = ofa_bat_properties_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->bat_bin ));
}

static void
on_selection_changed( GtkTreeSelection *selection, ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	ofoBat *bat;

	priv = self->priv;
	priv->bat_id = -1;

	bat = get_selected_object( self, selection );
	if( bat ){
		priv->bat_id = ofo_bat_get_id( bat );
		ofa_bat_properties_bin_set_bat(
				priv->bat_bin, bat, MY_WINDOW( self )->prot->dossier, FALSE );
	}
}

static ofoBat *
get_selected_object( const ofaBatSelect *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBat *bat;

	bat = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, COL_OBJECT, &bat, -1 );
		g_object_unref( bat );
	}

	return( bat );
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaBatSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *btn;

	priv = self->priv;

	btn = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( btn, priv->bat_id > 0 );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaBatSelectPrivate *priv;

	priv = OFA_BAT_SELECT( dialog )->priv;

	return( priv->bat_id > 0 );
}

/*
 * pane_position;
 */
static void
get_settings( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = self->priv;
	slist = ofa_settings_get_string_list( st_settings );

	it = slist ? slist : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->pane_pos = cstr ? atoi( cstr ) : 200;

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%u;", priv->pane_pos );
	ofa_settings_set_string( st_settings, str );
	g_free( str );
}
