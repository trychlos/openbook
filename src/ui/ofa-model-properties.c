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
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-model-properties.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaModelPropertiesClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaModelPropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow   *main_window;
	GtkDialog       *dialog;
	ofoModel        *model;
	ofaJournalCombo *journal_combo;
	GtkTreeView     *view;				/* the treeview which display the detail lines */
	gint             count;				/* count of detail lines */

	/* result
	 */
	gboolean       updated;

	/* data
	 */
	gchar         *mnemo;
	gchar         *label;
	gint           journal;				/* journal id */
	gboolean       journal_locked;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
};

/* column ordering in the journal combobox
 */
enum {
	JOU_COL_ID = 0,
	JOU_COL_MNEMO,
	JOU_COL_LABEL,
	JOU_N_COLUMNS
};

/* columns in the detail treeview
 */
enum {
	DET_COL_RANG = 0,
	DET_COL_COMMENT,
	DET_COL_ACCOUNT,
	DET_COL_ACCOUNT_VER,
	DET_COL_LABEL,
	DET_COL_LABEL_VER,
	DET_COL_DEBIT,
	DET_COL_DEBIT_VER,
	DET_COL_CREDIT,
	DET_COL_CREDIT_VER,
	DET_N_COLUMNS
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-model-properties.ui";
static const gchar  *st_ui_id        = "ModelPropertiesDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaModelPropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaModelProperties *self, ofaMainWindow *main, ofoModel *model, gint journal_id );
static gboolean  init_dialog_from_builder( ofaModelProperties *self );
static void      init_dialog_title( ofaModelProperties *self );
static void      init_dialog_mnemo( ofaModelProperties *self );
static void      init_dialog_label( ofaModelProperties *self );
static void      init_dialog_journal_locked( ofaModelProperties *self );
static void      init_dialog_notes( ofaModelProperties *self );
static void      init_dialog_detail( ofaModelProperties *self );
static gboolean  ok_to_terminate( ofaModelProperties *self, gint code );
static void      on_mnemo_changed( GtkEntry *entry, ofaModelProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaModelProperties *self );
static void      on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaModelProperties *self );
static void      on_journal_locked_toggled( GtkToggleButton *toggle, ofaModelProperties *self );
static void      add_detail_row( ofaModelProperties *self, gint idx );
static void      on_add_clicked( GtkButton *button, ofaModelProperties *self );
static void      on_description_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self );
static void      on_account_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self );
static void      on_label_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self );
static void      on_debit_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self );
static void      on_credit_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self );
static void      on_account_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self );
static void      on_label_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self );
static void      on_debit_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self );
static void      on_credit_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self );
static void      check_for_enable_dlg( ofaModelProperties *self );
static gboolean  do_update( ofaModelProperties *self );
static void      get_detail_list( ofaModelProperties *self );
static void      error_duplicate( ofaModelProperties *self, ofoModel *existing );

GType
ofa_model_properties_get_type( void )
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
	static const gchar *thisfn = "ofa_model_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaModelPropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaModelProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaModelProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaModelPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_model_properties_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaModelPropertiesClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_model_properties_instance_init";
	ofaModelProperties *self;

	g_return_if_fail( OFA_IS_MODEL_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_MODEL_PROPERTIES( instance );

	self->private = g_new0( ofaModelPropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_model_properties_instance_dispose";
	ofaModelPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_MODEL_PROPERTIES( window ));

	priv = ( OFA_MODEL_PROPERTIES( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
		g_free( priv->label );
		g_free( priv->maj_user );

		gtk_widget_destroy( GTK_WIDGET( priv->dialog ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_model_properties_instance_finalize";
	ofaModelProperties *self;

	g_return_if_fail( OFA_IS_MODEL_PROPERTIES( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_MODEL_PROPERTIES( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_model_properties_run:
 * @main: the main window of the application.
 * @model:
 * @journal_id: set to the current journal when creating a new model,
 *  left to OFO_BASE_UNSET_ID when updating an existing model.
 *
 * Update the properties of an model
 */
gboolean
ofa_model_properties_run( ofaMainWindow *main_window, ofoModel *model, gint journal_id )
{
	static const gchar *thisfn = "ofa_model_properties_run";
	ofaModelProperties *self;
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, model=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new( OFA_TYPE_MODEL_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, model, journal_id );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
do_initialize_dialog( ofaModelProperties *self, ofaMainWindow *main, ofoModel *model, gint journal_id )
{
	ofaModelPropertiesPrivate *priv;
	GtkButton *button;
	ofaJournalComboParms parms;
	gboolean is_new;
	const gchar *mnemo;

	priv = self->private;
	priv->main_window = main;
	priv->model = model;

	if( !init_dialog_from_builder( self )){
		return;
	}
	g_return_if_fail( priv->dialog && GTK_IS_DIALOG( priv->dialog ));

	init_dialog_title( self );
	init_dialog_mnemo( self );
	init_dialog_label( self );

	mnemo = ofo_model_get_mnemo( model );
	is_new = !mnemo || g_utf8_strlen( mnemo, -1 );

	parms.dialog = priv->dialog;
	parms.dossier = ofa_main_window_get_dossier( main );
	parms.combo_name = "p1-journal";
	parms.label_name = "p1-jou-label";
	parms.disp_mnemo = TRUE;
	parms.disp_label = FALSE;
	parms.pfn = ( ofaJournalComboCb ) on_journal_changed;
	parms.user_data = self;
	parms.initial_id = is_new ? journal_id : ofo_model_get_journal( model );

	priv->journal_combo = ofa_journal_combo_init_dialog( &parms );

	init_dialog_journal_locked( self );
	init_dialog_notes( self );
	init_dialog_detail( self );

	button = GTK_BUTTON( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "btn-add" ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_add_clicked ), self );

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

static gboolean
init_dialog_from_builder( ofaModelProperties *self )
{
	static const gchar *thisfn = "ofa_model_properties_do_init_dialog_from_builder";
	GError *error;
	GtkBuilder *builder;
	ofaModelPropertiesPrivate *priv;
	gboolean ok;

	ok = FALSE;
	priv = self->private;

	error = NULL;
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
		priv->dialog = GTK_DIALOG( gtk_builder_get_object( builder, st_ui_id ));
		if( !priv->dialog ){
			g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
		} else {
			ok = TRUE;
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}
	g_object_unref( builder );

	g_debug( "%s: dialog=%p (%s)",
			thisfn, ( void * ) priv->dialog, G_OBJECT_TYPE_NAME( priv->dialog ));

	return( ok );
}

static void
init_dialog_title( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	const gchar *mnemo;
	gchar *title;

	priv = self->private;
	mnemo = ofo_model_get_mnemo( priv->model );

	if( !mnemo ){
		title = g_strdup( _( "Defining a new entry model" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » entry model" ), mnemo );
	}

	gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );
	g_free( title );
}

static void
init_dialog_mnemo( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	GtkEntry *entry;

	priv = self->private;

	priv->mnemo = g_strdup( ofo_model_get_mnemo( priv->model ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-mnemo" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );
}

static void
init_dialog_label( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	GtkEntry *entry;

	priv = self->private;

	priv->label = g_strdup( ofo_model_get_label( priv->model ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );
}

static void
init_dialog_journal_locked( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	GtkToggleButton *btn;

	priv = self->private;

	priv->journal_locked = ofo_model_get_journal_locked( priv->model );
	btn = GTK_TOGGLE_BUTTON( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-jou-locked" ));
	gtk_toggle_button_set_active( btn, priv->journal_locked );

	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_journal_locked_toggled ), self );
}

static void
init_dialog_notes( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	gchar *notes;
	GtkTextView *text;
	GtkTextBuffer *buffer;

	priv = self->private;

	notes = g_strdup( ofo_model_get_notes( priv->model ));
	if( notes ){
		text = GTK_TEXT_VIEW(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( priv->dialog ), "p3-notes" ));
		buffer = gtk_text_buffer_new( NULL );
		gtk_text_buffer_set_text( buffer, notes, -1 );
		gtk_text_view_set_buffer( text, buffer );
	}
}

static void
init_dialog_detail( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	GtkTreeView *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell, *toggle_cell;
	GtkTreeViewColumn *column;
	gint count, i;

	priv = self->private;

	tview = GTK_TREE_VIEW(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( priv->dialog ), "p1-detail" ));
	self->private->view = tview;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
				DET_N_COLUMNS,
				G_TYPE_INT, G_TYPE_STRING,
				G_TYPE_STRING, G_TYPE_BOOLEAN,
				G_TYPE_STRING, G_TYPE_BOOLEAN,
				G_TYPE_STRING, G_TYPE_BOOLEAN,
				G_TYPE_STRING, G_TYPE_BOOLEAN ));

	gtk_tree_view_set_model( tview, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"",
			text_cell, "text", DET_COL_RANG,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_alignment( column, 1.0 );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "editable", TRUE, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Description" ),
			text_cell, "text", DET_COL_COMMENT,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_description_edited ), self );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "editable", TRUE, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Account" ),
			text_cell, "text", DET_COL_ACCOUNT,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_min_width( column, 90 );
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_account_edited ), self );

	toggle_cell = gtk_cell_renderer_toggle_new();
	gtk_cell_renderer_toggle_set_activatable( GTK_CELL_RENDERER_TOGGLE( toggle_cell ), TRUE );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Lock" ),
			toggle_cell, "active", DET_COL_ACCOUNT_VER,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_signal_connect( G_OBJECT( toggle_cell ), "toggled", G_CALLBACK( on_account_toggled ), self );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "editable", TRUE, NULL );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Entry label" ),
			text_cell, "text", DET_COL_LABEL,
			NULL );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_expand( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_label_edited ), self );

	toggle_cell = gtk_cell_renderer_toggle_new();
	gtk_cell_renderer_toggle_set_activatable( GTK_CELL_RENDERER_TOGGLE( toggle_cell ), TRUE );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Lock" ),
			toggle_cell, "active", DET_COL_LABEL_VER,
			NULL );
	gtk_tree_view_append_column( tview, column );
	g_signal_connect( G_OBJECT( toggle_cell ), "toggled", G_CALLBACK( on_label_toggled ), self );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "editable", TRUE, NULL );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Debit" ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", DET_COL_DEBIT );
	gtk_tree_view_column_set_min_width( column, 90 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_debit_edited ), self );

	toggle_cell = gtk_cell_renderer_toggle_new();
	gtk_cell_renderer_toggle_set_activatable( GTK_CELL_RENDERER_TOGGLE( toggle_cell ), TRUE );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, toggle_cell, FALSE );
	gtk_tree_view_column_set_title( column, _( "Lock" ));
	gtk_tree_view_column_add_attribute( column, toggle_cell, "active", DET_COL_DEBIT_VER );
	gtk_tree_view_append_column( tview, column );
	g_signal_connect( G_OBJECT( toggle_cell ), "toggled", G_CALLBACK( on_debit_toggled ), self );

	text_cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( text_cell ), "editable", TRUE, NULL );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, text_cell, TRUE );
	gtk_tree_view_column_set_title( column, _( "Credit" ));
	gtk_tree_view_column_add_attribute( column, text_cell, "text", DET_COL_CREDIT );
	gtk_tree_view_column_set_min_width( column, 90 );
	gtk_tree_view_append_column( tview, column );
	gtk_tree_view_column_set_resizable( column, TRUE );
	g_signal_connect( G_OBJECT( text_cell ), "edited", G_CALLBACK( on_credit_edited ), self );

	toggle_cell = gtk_cell_renderer_toggle_new();
	gtk_cell_renderer_toggle_set_activatable( GTK_CELL_RENDERER_TOGGLE( toggle_cell ), TRUE );
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_end( column, toggle_cell, FALSE );
	gtk_tree_view_column_set_title( column, _( "Lock" ));
	gtk_tree_view_column_add_attribute( column, toggle_cell, "active", DET_COL_CREDIT_VER );
	gtk_tree_view_append_column( tview, column );
	g_signal_connect( G_OBJECT( toggle_cell ), "toggled", G_CALLBACK( on_credit_toggled ), self );

	count = ofo_model_get_detail_count( self->private->model );
	for( i=0 ; i<count ; ++i ){
		add_detail_row( self, i );
	}
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaModelProperties *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_NONE:
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_CANCEL:
			quit = TRUE;
			break;

		case GTK_RESPONSE_OK:
			quit = do_update( self );
			break;
	}

	return( quit );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaModelProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaModelProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
select_row( ofaModelProperties *self, GtkTreeModel *model, GtkTreeIter *iter )
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path( model, iter );
	gtk_tree_view_set_cursor( self->private->view, path, NULL, FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( self->private->view ));
	gtk_tree_path_free( path );
}

static void
add_empty_row( ofaModelProperties *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	tmodel = gtk_tree_view_get_model( self->private->view );
	gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
	self->private->count += 1;
	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			&iter,
			DET_COL_RANG,        self->private->count,
			DET_COL_COMMENT,     "",
			DET_COL_ACCOUNT,     "",
			DET_COL_ACCOUNT_VER, FALSE,
			DET_COL_LABEL,       "",
			DET_COL_LABEL_VER,   FALSE,
			DET_COL_DEBIT,       "",
			DET_COL_DEBIT_VER,   FALSE,
			DET_COL_CREDIT,      "",
			DET_COL_CREDIT_VER,  FALSE,
			-1 );

	select_row( self, tmodel, &iter );
}

/*
 * idx here is the index of the detail line in the list, starting with 0
 */
static void
add_detail_row( ofaModelProperties *self, gint idx )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	tmodel = gtk_tree_view_get_model( self->private->view );
	gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
	self->private->count += 1;
	gtk_list_store_set(
			GTK_LIST_STORE( tmodel ),
			&iter,
			DET_COL_RANG,        idx+1,
			DET_COL_COMMENT,     ofo_model_get_detail_comment( self->private->model, idx ),
			DET_COL_ACCOUNT,     ofo_model_get_detail_account( self->private->model, idx ),
			DET_COL_ACCOUNT_VER, ofo_model_get_detail_account_locked( self->private->model, idx ),
			DET_COL_LABEL,       ofo_model_get_detail_label( self->private->model, idx ),
			DET_COL_LABEL_VER,   ofo_model_get_detail_label_locked( self->private->model, idx ),
			DET_COL_DEBIT,       ofo_model_get_detail_debit( self->private->model, idx ),
			DET_COL_DEBIT_VER,   ofo_model_get_detail_debit_locked( self->private->model, idx ),
			DET_COL_CREDIT,      ofo_model_get_detail_credit( self->private->model, idx ),
			DET_COL_CREDIT_VER,  ofo_model_get_detail_credit_locked( self->private->model, idx ),
			-1 );
}

static void
on_add_clicked( GtkButton *button, ofaModelProperties *self )
{
	add_empty_row( self );
}

static void
on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaModelProperties *self )
{
	g_return_if_fail( self && OFA_IS_MODEL_PROPERTIES( self ));

	self->private->journal = id;

	check_for_enable_dlg( self );
}

static void
on_journal_locked_toggled( GtkToggleButton *btn, ofaModelProperties *self )
{
	self->private->journal_locked = gtk_toggle_button_get_active( btn );
	check_for_enable_dlg( self );
}

static void
on_detail_editing( ofaModelProperties *self, const gchar *path, const gchar *text, gint col )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	tmodel = gtk_tree_view_get_model( self->private->view );
	if( gtk_tree_model_get_iter_from_string( tmodel, &iter, path )){
		gtk_list_store_set( GTK_LIST_STORE( tmodel ), &iter, col, text, -1 );
	}
}

static void
on_description_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self )
{
	on_detail_editing( self, path, new_text, DET_COL_COMMENT );
}

static void
on_account_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self )
{
	on_detail_editing( self, path, new_text, DET_COL_ACCOUNT );
}

static void
on_label_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self )
{
	on_detail_editing( self, path, new_text, DET_COL_LABEL );
}

static void
on_debit_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self )
{
	on_detail_editing( self, path, new_text, DET_COL_DEBIT );
}

static void
on_credit_edited( GtkCellRendererText *cell, gchar *path, gchar *new_text, ofaModelProperties *self )
{
	on_detail_editing( self, path, new_text, DET_COL_CREDIT );
}

static void
on_detail_toggling( ofaModelProperties *self, GtkCellRendererToggle *cell, gchar *path, gint col )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gboolean toggled;

	tmodel = gtk_tree_view_get_model( self->private->view );
	if( gtk_tree_model_get_iter_from_string( tmodel, &iter, path )){
		gtk_tree_model_get( tmodel, &iter, col, &toggled, -1 );
		gtk_cell_renderer_toggle_set_active( cell, !toggled );
		gtk_list_store_set( GTK_LIST_STORE( tmodel ), &iter, col, !toggled, -1 );
	}
}

static void
on_account_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self )
{
	on_detail_toggling( self, cell, path, DET_COL_ACCOUNT_VER );
}

static void
on_label_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self )
{
	on_detail_toggling( self, cell, path, DET_COL_LABEL_VER );
}

static void
on_debit_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self )
{
	on_detail_toggling( self, cell, path, DET_COL_DEBIT_VER );
}

static void
on_credit_toggled( GtkCellRendererToggle *cell, gchar *path, ofaModelProperties *self )
{
	on_detail_toggling( self, cell, path, DET_COL_CREDIT_VER );
}

/*
 * we accept to save uncomplete detail lines
 */
static void
check_for_enable_dlg( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	GtkWidget *button;

	priv = self->private;
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button,
			priv->mnemo && g_utf8_strlen( priv->mnemo, -1 ) &&
			priv->label && g_utf8_strlen( priv->label, -1 ) &&
			priv->journal > 0 );
}

static gboolean
do_update( ofaModelProperties *self )
{
	gchar *prev_mnemo;
	ofoDossier *dossier;
	ofoModel *existing;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *notes;

	prev_mnemo = g_strdup( ofo_model_get_mnemo( self->private->model ));
	dossier = ofa_main_window_get_dossier( self->private->main_window );
	existing = ofo_model_get_by_mnemo( dossier, self->private->mnemo );

	if( existing ){
		/* c'est un nouveau model, ou bien un model existant dont on
		 * veut changer le numéro: no luck, le nouveau mnemo de model
		 * existe déjà
		 */
		if( !prev_mnemo || g_utf8_collate( prev_mnemo, self->private->mnemo )){
			error_duplicate( self, existing );
			g_free( prev_mnemo );
			return( FALSE );
		}
	}

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même model (n'a pas été modifié)
	 */
	ofo_model_set_mnemo( self->private->model, self->private->mnemo );
	ofo_model_set_label( self->private->model, self->private->label );
	ofo_model_set_journal( self->private->model, self->private->journal );
	ofo_model_set_journal_locked( self->private->model, self->private->journal_locked );

	text = GTK_TEXT_VIEW(
			my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p3-notes" ));
	buffer = gtk_text_view_get_buffer( text );
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
	ofo_model_set_notes( self->private->model, notes );
	g_free( notes );

	get_detail_list( self );

	if( !prev_mnemo ){
		self->private->updated =
				ofo_model_insert( self->private->model, dossier );
	} else {
		self->private->updated =
				ofo_model_update( self->private->model, dossier, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( self->private->updated );
}

static void
get_detail_list( ofaModelProperties *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *comment, *account, *label, *debit, *credit;
	gboolean account_locked, label_locked, debit_locked, credit_locked;
	gint i;

	tmodel = gtk_tree_view_get_model( self->private->view );
	if( gtk_tree_model_get_iter_first( tmodel, &iter )){
		i = 0;
		while( TRUE ){
			gtk_tree_model_get( tmodel, &iter,
					DET_COL_COMMENT,     &comment,
					DET_COL_ACCOUNT,     &account,
					DET_COL_ACCOUNT_VER, &account_locked,
					DET_COL_LABEL,       &label,
					DET_COL_LABEL_VER,   &label_locked,
					DET_COL_DEBIT,       &debit,
					DET_COL_DEBIT_VER,   &debit_locked,
					DET_COL_CREDIT,      &credit,
					DET_COL_CREDIT_VER,  &credit_locked,
					-1 );

			/* do not record empty lines */
			if(( comment && g_utf8_strlen( comment, -1 )) ||
				( label && g_utf8_strlen( label, -1 )) ||
				( debit && g_utf8_strlen( debit, -1 )) ||
				( credit && g_utf8_strlen( credit, -1 ))){

				ofo_model_set_detail( self->private->model, i, comment,
						account, account_locked, label, label_locked,
						debit, debit_locked, credit, credit_locked );

				i += 1;
			}

			g_free( comment );
			g_free( account );
			g_free( debit );
			g_free( credit );

			if( !gtk_tree_model_iter_next( tmodel, &iter )){
				break;
			}
		}
	}
}

static void
error_duplicate( ofaModelProperties *self, ofoModel *existing )
{
	GtkMessageDialog *dlg;
	gchar *msg;

	msg = g_strdup_printf(
			_( "Unable to set the entry model mnemonic to '%s' "
				"as this one is already used by the existing '%s'" ),
				ofo_model_get_mnemo( existing ),
				ofo_model_get_label( existing ));

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( self->private->dialog ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
	g_free( msg );
}
