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

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-misc.h"
#include "ui/ofa-restore.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaRestorePrivate {
	gboolean       dispose_has_run;

	ofaMainWindow *main_window;

	GtkWidget     *dialog;
	GtkWidget     *grid;
	GtkWidget     *toggle;
	GtkWidget     *open_btn;

	gchar         *fname;
	gint           dest_mode;
	gchar         *label;
};

/* where do we want restore the database ?
 */
enum {
	DEST_COL_LABEL,
	DEST_COL_INT,
	DEST_N_COLUMNS
};

enum {
	DEST_EXISTING_DOSSIER = 1,
	DEST_NEW_DOSSIER
};

typedef struct {
	const gchar *label;
	gint         code;
}
	destRestore;

static destRestore st_dest_restore[]  = {
		{ N_( "Restore to an existing dossier" ), DEST_EXISTING_DOSSIER },
		/*{ N_( "Restore to a new dossier" ), DEST_NEW_DOSSIER },*/
		{ 0 }
};

/* restoring to an existing dossier
 */
enum {
	EXIST_COL_LABEL,
	EXIST_N_COLUMNS
};

static const gchar *st_dialog_name    = "RestoreDlg";
static const gchar *st_restore_folder = "LastRestoreFolder";
static const gchar *st_open_dossier   = "OpenRestoredDossier";

G_DEFINE_TYPE( ofaRestore, ofa_restore, G_TYPE_OBJECT )

static void       init_dialog( ofaRestore *self );
static void       init_combo( ofaRestore *self, GtkComboBox *box );
static void       on_dest_changed( GtkComboBox *box, ofaRestore *self );
static void       remove_current_dest( ofaRestore *self );
static void       setup_existing_dossier( ofaRestore *self );
static void       on_dossier_selection_changed( GtkTreeSelection *select, ofaRestore *self );
static void       setup_new_dossier( ofaRestore *self );
static void       on_chooser_selection_changed( GtkFileChooser *chooser, ofaRestore *self );
static void       on_chooser_file_activated( GtkFileChooser *chooser, ofaRestore *self );
static void       check_for_enable_dlg( ofaRestore *self );
static gboolean   do_restore( ofaRestore *self );
static gboolean   do_restore_existing( ofaRestore *self );

static void
restore_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_restore_finalize";
	ofaRestorePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RESTORE( instance ));

	/* free data members here */
	priv = OFA_RESTORE( instance )->priv;

	g_free( priv->fname );
	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_restore_parent_class )->finalize( instance );
}

static void
restore_dispose( GObject *instance )
{
	ofaRestorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_RESTORE( instance ));

	priv = OFA_RESTORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		my_utils_window_save_position( GTK_WINDOW( priv->dialog ), st_dialog_name );
		gtk_widget_destroy( priv->dialog );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_restore_parent_class )->dispose( instance );
}

static void
ofa_restore_init( ofaRestore *self )
{
	static const gchar *thisfn = "ofa_restore_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RESTORE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RESTORE, ofaRestorePrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_restore_class_init( ofaRestoreClass *klass )
{
	static const gchar *thisfn = "ofa_restore_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = restore_dispose;
	G_OBJECT_CLASS( klass )->finalize = restore_finalize;

	g_type_class_add_private( klass, sizeof( ofaRestorePrivate ));
}

/**
 * ofa_restore_run:
 * @main: the main window of the application.
 *
 * Restore a database backup.
 */
void
ofa_restore_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_restore_run";
	ofaRestore *self;
	ofaRestorePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_RESTORE, NULL );
	priv = self->priv;
	priv->main_window = main_window;

	init_dialog( self );

	if( gtk_dialog_run( GTK_DIALOG( priv->dialog )) == GTK_RESPONSE_OK ){

		do_restore( self );
	}

	g_object_unref( self );
}

static void
init_dialog( ofaRestore *self )
{
	ofaRestorePrivate *priv;
	gchar *last_folder;
	GtkWidget *combo;
	gboolean open;

	priv = self->priv;

	priv->dialog = gtk_file_chooser_dialog_new(
							_( "Restore a dossier's database" ),
							GTK_WINDOW( priv->main_window ),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							_( "_Cancel" ), GTK_RESPONSE_CANCEL,
							_( "_Open" ), GTK_RESPONSE_OK,
							NULL );

	my_utils_window_restore_position( GTK_WINDOW( priv->dialog ), st_dialog_name );

	last_folder = ofa_settings_get_string( st_restore_folder );
	if( last_folder && g_utf8_strlen( last_folder, -1 )){
		gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( priv->dialog ), last_folder );
	}
	g_free( last_folder );

	g_signal_connect(
			G_OBJECT( priv->dialog ),
			"selection-changed",
			G_CALLBACK( on_chooser_selection_changed ),
			self );

	g_signal_connect(
			G_OBJECT( priv->dialog ),
			"file-activated",
			G_CALLBACK( on_chooser_file_activated ),
			self );

	priv->open_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( priv->dialog ), GTK_RESPONSE_OK );
	gtk_widget_set_sensitive( priv->open_btn, FALSE );

	priv->grid = gtk_grid_new();
	gtk_grid_set_column_spacing( GTK_GRID( priv->grid ), 4 );
	gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER( priv->dialog ), priv->grid );

	combo = gtk_combo_box_new();
	gtk_widget_set_valign( combo, GTK_ALIGN_START );
	gtk_grid_attach( GTK_GRID( priv->grid ), combo, 0, 0, 1, 1 );
	init_combo( self, GTK_COMBO_BOX( combo ));

	priv->toggle = gtk_check_button_new_with_mnemonic( _( "Open the _restored dossier" ));
	gtk_widget_set_valign( priv->toggle, GTK_ALIGN_START );
	gtk_grid_attach( GTK_GRID( priv->grid ), priv->toggle, 2, 0, 1, 1 );
	open = ofa_settings_get_boolean( st_open_dossier );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->toggle ), open );

	gtk_widget_show_all( priv->dialog );
}

static void
init_combo( ofaRestore *self, GtkComboBox *box )
{
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	gint i;

	tmodel = ( GtkTreeModel * ) gtk_list_store_new( DEST_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT );
	gtk_combo_box_set_model( box, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( box ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( box ), cell, "text", DEST_COL_LABEL );

	g_signal_connect( G_OBJECT( box ), "changed", G_CALLBACK( on_dest_changed ), self );

	for( i=0 ; st_dest_restore[i].code ; ++i ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DEST_COL_INT, st_dest_restore[i].code,
				DEST_COL_LABEL, st_dest_restore[i].label,
				-1 );
	}

	gtk_combo_box_set_active( box, 0 );
}

static void
on_dest_changed( GtkComboBox *box, ofaRestore *self )
{
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	gint code;

	self->priv->dest_mode = -1;

	if( gtk_combo_box_get_active_iter( box, &iter )){

		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter,
				DEST_COL_INT, &code,
				-1 );

		switch( code ){
			case DEST_EXISTING_DOSSIER:
				remove_current_dest( self );
				self->priv->dest_mode = code;
				setup_existing_dossier( self );
				break;

			case DEST_NEW_DOSSIER:
				remove_current_dest( self );
				self->priv->dest_mode = code;
				setup_new_dossier( self );
				break;
		}
	}
}

static void
remove_current_dest( ofaRestore *self )
{
	GtkWidget *child;

	child = gtk_grid_get_child_at( GTK_GRID( self->priv->grid ), 1, 0 );
	if( child ){
		g_return_if_fail( GTK_IS_WIDGET( child ));
		gtk_widget_destroy( child );
	}
}

static void
setup_existing_dossier( ofaRestore *self )
{
	ofaRestorePrivate *priv;
	GtkWidget *frame, *alignment, *scrolled, *treeview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GSList *list, *it;
	GtkTreeIter iter;
	GtkTreeSelection *select;

	priv = self->priv;

	frame = gtk_frame_new( NULL );
	gtk_widget_set_size_request( frame, 200, -1 );
	gtk_grid_attach( GTK_GRID( priv->grid ), frame, 1, 0, 1, 1 );

	alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_container_add( GTK_CONTAINER( frame ), alignment );

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( alignment ), scrolled );

	treeview = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( treeview ), FALSE );
	gtk_container_add( GTK_CONTAINER( scrolled ), treeview );

	tmodel = ( GtkTreeModel * ) gtk_list_store_new( EXIST_N_COLUMNS, G_TYPE_STRING );
	gtk_tree_view_set_model( GTK_TREE_VIEW( treeview ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"",
			cell, "text", EXIST_COL_LABEL,
			NULL );
	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), column );

	list = ofa_dossier_misc_get_dossiers();

	for( it=list ; it ; it=it->next ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				EXIST_COL_LABEL, ( const gchar * ) it->data,
				-1 );
	}

	ofa_dossier_misc_free_dossiers( list );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( treeview ));
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );

	g_signal_connect(
			G_OBJECT( select ),
			"changed",
			G_CALLBACK( on_dossier_selection_changed ),
			self );
}

static void
on_dossier_selection_changed( GtkTreeSelection *select, ofaRestore *self )
{
	ofaRestorePrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;

	priv = self->priv;

	g_free( priv->label );
	priv->label = NULL;

	if( gtk_tree_selection_get_selected( select, &tmodel, &iter )){

		gtk_tree_model_get( tmodel, &iter, EXIST_COL_LABEL, &priv->label, -1 );
	}

	check_for_enable_dlg( self );
}

static void
setup_new_dossier( ofaRestore *self )
{
	GtkWidget *frame, *alignment;

	frame = gtk_frame_new( NULL );
	gtk_grid_attach( GTK_GRID( self->priv->grid ), frame, 1, 0, 1, 1 );

	alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_container_add( GTK_CONTAINER( frame ), alignment );
}

static void
on_chooser_selection_changed( GtkFileChooser *chooser, ofaRestore *self )
{
	ofaRestorePrivate *priv;

	priv = self->priv;

	g_free( priv->fname );
	priv->fname = NULL;

	priv->fname = gtk_file_chooser_get_filename( chooser );

	check_for_enable_dlg( self );
}

static void
on_chooser_file_activated( GtkFileChooser *chooser, ofaRestore *self )
{
	on_chooser_selection_changed( chooser, self );
}

static void
check_for_enable_dlg( ofaRestore *self )
{
	ofaRestorePrivate *priv;
	gboolean enable;

	priv = self->priv;

	enable = priv->fname && g_utf8_strlen( priv->fname, -1 );

	switch( priv->dest_mode ){
		case DEST_EXISTING_DOSSIER:
			enable &= priv->label && g_utf8_strlen( priv->label, -1 );
			break;

		case DEST_NEW_DOSSIER:
			enable = FALSE;
			break;

		default:
			enable = FALSE;
	}

	gtk_widget_set_sensitive( priv->open_btn, enable );
}

static gboolean
do_restore( ofaRestore *self )
{
	ofaRestorePrivate *priv;
	gboolean ok;
	gboolean open;
	gchar *dirname;
	ofsDossierOpen *sdo;

	ok = FALSE;
	priv = self->priv;

	dirname = g_path_get_dirname( priv->fname );
	ofa_settings_set_string( st_restore_folder, dirname );
	g_free( dirname );

	switch( priv->dest_mode ){
		case DEST_EXISTING_DOSSIER:
			ok = do_restore_existing( self );
			break;

		case DEST_NEW_DOSSIER:
		default:
			break;
	}

	open = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->toggle ));
	ofa_settings_set_boolean( st_open_dossier, open );

	if( ok && open ){
		sdo = g_new0( ofsDossierOpen, 1 );
		sdo->dname = g_strdup( priv->label );
		g_signal_emit_by_name( priv->main_window, OFA_SIGNAL_ACTION_DOSSIER_OPEN, sdo );
	}

	return( ok );
}

static gboolean
do_restore_existing( ofaRestore *self )
{
	ofaRestorePrivate *priv;
	gboolean ok;
	ofoDossier *dossier;
	gchar *provider;
	ofaIDbms *dbms;

	ok = FALSE;
	priv = self->priv;

	/* first close the currently opened dossier if we are going to
	 * restore to this same dossier */
	dossier = ofa_main_window_get_dossier( priv->main_window );
	if( dossier ){
		g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
		if( !g_utf8_collate( priv->label, ofo_dossier_get_name( dossier ))){
			ofa_main_window_close_dossier( priv->main_window );
		}
	}

	/* restore the backup */
	provider = ofa_settings_get_dossier_provider( priv->label );
	if( provider && g_utf8_strlen( provider, -1 )){
		dbms = ofa_idbms_get_provider_by_name( provider );
		if( dbms ){
			g_return_val_if_fail( OFA_IS_IDBMS( dbms ), FALSE );
			ok = ofa_idbms_restore( dbms, priv->label, priv->fname );
			g_object_unref( dbms );
		}
	}

	return( ok );
}
