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

#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-window-prot.h"

#include "ui/ofa-dossier-open.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierOpenPrivate {

	/* data
	 */
	gchar          *name;
	gchar          *account;
	gchar          *password;

	/* return value
	 * the structure itself, along with its datas, will be freed by the
	 * MainWindow signal final handler
	 */
	ofsDossierOpen *sdo;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dossier-open.ui";
static const gchar  *st_ui_id  = "DossierOpenDlg";

/* column ordering in the selection listview
 */
enum {
	COL_NAME = 0,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaDossierOpen, ofa_dossier_open, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static gint      on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data );
static void      on_dossier_selected( GtkTreeSelection *selection, ofaDossierOpen *self );
static void      on_account_changed( GtkEntry *entry, ofaDossierOpen *self );
static void      on_password_changed( GtkEntry *entry, ofaDossierOpen *self );
static void      check_for_enable_dlg( ofaDossierOpen *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_open( ofaDossierOpen *self );

static void
dossier_open_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_open_finalize";
	ofaDossierOpenPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_OPEN( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_OPEN( instance )->priv;
	g_free( priv->name );
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_open_parent_class )->finalize( instance );
}

static void
dossier_open_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_OPEN( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_open_parent_class )->dispose( instance );
}

static void
ofa_dossier_open_init( ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_OPEN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_OPEN, ofaDossierOpenPrivate );
}

static void
ofa_dossier_open_class_init( ofaDossierOpenClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_open_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_open_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_open_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierOpenPrivate ));
}

/**
 * ofa_dossier_open_run:
 * @main: the main window of the application.
 *
 * Run the selection dialog to choose a dossier to be opened
 */
ofsDossierOpen *
ofa_dossier_open_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_open_run";
	ofaDossierOpen *self;
	ofsDossierOpen *sdo;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new(
				OFA_TYPE_DOSSIER_OPEN,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	sdo = self->priv->sdo;
	g_object_unref( self );

	return( sdo );
}

static void
v_init_dialog( myDialog *dialog )
{
	GtkContainer *container;
	GtkTreeView *listview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GSList *dossiers, *id;
	GtkTreeSelection *select;
	GtkEntry *entry;
	GList *focus;

	focus = NULL;

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	dossiers = ofa_settings_get_dossiers();

	listview = GTK_TREE_VIEW(
					my_utils_container_get_child_by_name( container, "treeview" ));
	tmodel = GTK_TREE_MODEL( gtk_list_store_new( N_COLUMNS, G_TYPE_STRING ));
	gtk_tree_view_set_model( listview, tmodel );
	g_object_unref( tmodel );
	focus = g_list_append( focus, listview );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( tmodel ),
			( GtkTreeIterCompareFunc ) on_sort_model, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( tmodel ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
			GTK_SORT_ASCENDING );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"label",
			text_cell,
			"text", COL_NAME,
			NULL );
	gtk_tree_view_append_column( listview, column );

	select = gtk_tree_view_get_selection( listview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(G_OBJECT( select ), "changed", G_CALLBACK( on_dossier_selected ), dialog );

	for( id=dossiers ; id ; id=id->next ){
		gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
		gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				&iter,
				COL_NAME, ( const gchar * ) id->data,
				-1 );
	}

	g_slist_free_full( dossiers, ( GDestroyNotify ) g_free );

	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		gtk_tree_selection_select_iter( select, &iter );
	}

	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name( container, "account" ));
	g_signal_connect(G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), dialog );
	focus = g_list_append( focus, entry );

	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name( container, "password" ));
	g_signal_connect(G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), dialog );
	focus = g_list_append( focus, entry );

	/*  doesn't work: only the first widget of the grid get the focus !
	grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "grid-open" ));
	gtk_container_set_focus_chain( GTK_CONTAINER( grid ), focus );*/

	/* doesn't work either: this is as the default
	 * account -> listview -> password
	gtk_container_set_focus_chain( GTK_CONTAINER( dialog ), focus );*/

	check_for_enable_dlg( OFA_DOSSIER_OPEN( dialog ));
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data )
{
	gchar *aname, *bname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, COL_NAME, &aname, -1 );
	gtk_tree_model_get( tmodel, b, COL_NAME, &bname, -1 );

	cmp = g_utf8_collate( aname, bname );

	g_free( aname );
	g_free( bname );

	return( cmp );
}

static void
on_dossier_selected( GtkTreeSelection *selection, ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_on_dossier_selected";
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_debug( "%s: selection=%p, self=%p", thisfn, ( void * ) selection, ( void * ) self );

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){
		g_free( self->priv->name );
		gtk_tree_model_get( model, &iter, COL_NAME, &self->priv->name, -1 );
		g_debug( "%s: name=%s", thisfn, self->priv->name );
	}

	check_for_enable_dlg( self );
}

static void
on_account_changed( GtkEntry *entry, ofaDossierOpen *self )
{
	g_free( self->priv->account );
	self->priv->account = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEntry *entry, ofaDossierOpen *self )
{
	g_free( self->priv->password );
	self->priv->password = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierOpen *self )
{
	GtkWidget *button;
	gboolean ok_enable;

	ok_enable = self->priv->name && self->priv->account && self->priv->password;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-open" );
	gtk_widget_set_sensitive( button, ok_enable );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_open( OFA_DOSSIER_OPEN( dialog )));
}

/*
 * is called when the user click on the 'Open' button
 * return %TRUE if we can open a connection, %FALSE else
 */
static gboolean
do_open( ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_do_open";
	ofaDossierOpenPrivate *priv;
	ofsDossierOpen *sdo;
	ofoSgbd *sgbd;

	priv = self->priv;

	sgbd = ofo_sgbd_new( priv->name );
	if( !ofo_sgbd_connect( sgbd, priv->account, priv->password, FALSE )){
		g_object_unref( sgbd );
		return( FALSE );
	}

	g_debug( "%s: connection successfully opened", thisfn );
	g_object_unref( sgbd );

	sdo = g_new0( ofsDossierOpen, 1 );
	sdo->label = g_strdup( self->priv->name );
	sdo->account = g_strdup( self->priv->account );
	sdo->password = g_strdup( self->priv->password );
	self->priv->sdo = sdo;

	return( TRUE );
}
