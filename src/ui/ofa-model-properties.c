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
#include "ui/ofa-account-select.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-model-properties.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaModelPropertiesClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 *
 * each line of the grid is :
 * - button 'Add' or line number
 * - comment
 * - account entry
 * - account btn selection
 * - account locked
 * - label entry
 * - label locked
 * - debit entry
 * - debit locked
 * - credit entry
 * - credit locked
 * - button up
 * - button down
 * - button remove
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
	GtkGrid         *grid;				/* detail grid */
	gint             count;				/* count of added detail lines */

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
	DET_COL_ACCOUNT_SELECT,
	DET_COL_ACCOUNT_VER,
	DET_COL_LABEL,
	DET_COL_LABEL_VER,
	DET_COL_DEBIT,
	DET_COL_DEBIT_VER,
	DET_COL_CREDIT,
	DET_COL_CREDIT_VER,
	DET_COL_UP,
	DET_COL_DOWN,
	DET_COL_REMOVE,
	DET_N_COLUMNS
};

#define DET_COL_ADD                  DET_COL_RANG

/* each widget of the grid brings its row number */
#define DATA_ROW                     "ofa-data-row"

/* buttons also bring their column number */
#define DATA_COLUMN                  "ofa-data-column"

/* space between widgets in a detail line */
#define DETAIL_SPACE                 2

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
static void      init_dialog_detail( ofaModelProperties *self );
static void      insert_new_row( ofaModelProperties *self, gint row );
static void      add_empty_row( ofaModelProperties *self );
static void      add_button( ofaModelProperties *self, const gchar *stock_id, gint column, gint row, gint left_margin, gint right_margin );
static void      signal_row_added( ofaModelProperties *self );
static void      signal_row_removed( ofaModelProperties *self );
static gboolean  ok_to_terminate( ofaModelProperties *self, gint code );
static void      on_mnemo_changed( GtkEntry *entry, ofaModelProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaModelProperties *self );
static void      on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaModelProperties *self );
static void      on_journal_locked_toggled( GtkToggleButton *toggle, ofaModelProperties *self );
static void      on_account_selection( ofaModelProperties *self, gint row );
static void      check_for_enable_dlg( ofaModelProperties *self );
static void      on_button_clicked( GtkButton *button, ofaModelProperties *self );
static void      remove_row( ofaModelProperties *self, gint row );
static gboolean  do_update( ofaModelProperties *self );
static void      get_detail_list( ofaModelProperties *self, gint row );

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
	is_new = !mnemo || !g_utf8_strlen( mnemo, -1 );

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
	my_utils_init_notes_ex( model );
	init_dialog_detail( self );

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

	if( mnemo ){
		my_utils_init_maj_user_stamp_ex( model );
	}
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

/*
 * add one liine per
 */
static void
init_dialog_detail( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	gint count, i;

	priv = self->private;

	priv->grid = ( GtkGrid * ) my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-details" );
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));

	count = ofo_model_get_detail_count( self->private->model );
	for( i=1 ; i<=count ; ++i ){
		insert_new_row( self, i );
	}
	if( !count ){
		add_button( self, GTK_STOCK_ADD, DET_COL_ADD, 1, DETAIL_SPACE, 0 );
	}
}

static void
insert_new_row( ofaModelProperties *self, gint row )
{
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *str;

	add_empty_row( self );
	row = self->private->count;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_COMMENT, row ));
	str = ofo_model_get_detail_comment( self->private->model, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT, row ));
	str = ofo_model_get_detail_account( self->private->model, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT_VER, row ));
	gtk_toggle_button_set_active( toggle, ofo_model_get_detail_account_locked( self->private->model, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL, row ));
	str = ofo_model_get_detail_label( self->private->model, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL_VER, row ));
	gtk_toggle_button_set_active( toggle, ofo_model_get_detail_label_locked( self->private->model, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT, row ));
	str = ofo_model_get_detail_debit( self->private->model, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT_VER, row ));
	gtk_toggle_button_set_active( toggle, ofo_model_get_detail_debit_locked( self->private->model, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT, row ));
	str = ofo_model_get_detail_credit( self->private->model, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT_VER, row ));
	gtk_toggle_button_set_active( toggle, ofo_model_get_detail_credit_locked( self->private->model, row-1 ));
}

static void
add_empty_row( ofaModelProperties *self )
{
	gint row;
	GtkWidget *widget;
	GtkEntry *entry;
	GtkWidget *toggle;
	gchar *str;

	row = self->private->count + 1;
	g_return_if_fail( row >= 1 );

	/* the first time we enter for updating a model, the grid is fully
	 * empty */
	widget = gtk_grid_get_child_at( self->private->grid, DET_COL_ADD, row );
	if( widget ){
		gtk_widget_destroy( widget );
	}

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, 3 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_RANG, row, 1, 1 );
	str = g_strdup_printf( "%d", row );
	gtk_entry_set_text( entry, str );
	g_free( str );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), 2*DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_COMMENT, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), 2*DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 20 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_ACCOUNT, row, 1, 1 );

	add_button( self, GTK_STOCK_INDEX, DET_COL_ACCOUNT_SELECT, row, DETAIL_SPACE, 0 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_ACCOUNT_VER, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), TRUE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 20 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_LABEL, row, 1, 1 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_LABEL_VER, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_DEBIT, row, 1, 1 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_DEBIT_VER, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_CREDIT, row, 1, 1 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_CREDIT_VER, row, 1, 1 );

	add_button( self, GTK_STOCK_GO_UP, DET_COL_UP, row, DETAIL_SPACE, 0 );
	add_button( self, GTK_STOCK_GO_DOWN, DET_COL_DOWN, row, DETAIL_SPACE, 0 );
	add_button( self, GTK_STOCK_REMOVE, DET_COL_REMOVE, row, DETAIL_SPACE, 2*DETAIL_SPACE );
	add_button( self, GTK_STOCK_ADD, DET_COL_ADD, row+1, DETAIL_SPACE, 0 );

	self->private->count = row;

	signal_row_added( self );
	gtk_widget_show_all( GTK_WIDGET( self->private->grid ));
}

static void
add_button( ofaModelProperties *self, const gchar *stock_id, gint column, gint row, gint left_margin, gint right_margin )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_stock( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( button ), left_margin );
	gtk_widget_set_margin_right( GTK_WIDGET( button ), right_margin );
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static void
update_detail_buttons( ofaModelProperties *self )
{
	GtkWidget *button;

	if( self->private->count >= 1 ){

		button = gtk_grid_get_child_at(
						self->private->grid, DET_COL_UP, 1 );
		gtk_widget_set_sensitive( button, FALSE );

		if( self->private->count >= 2 ){
			button = gtk_grid_get_child_at(
							self->private->grid, DET_COL_UP, 2 );
			gtk_widget_set_sensitive( button, TRUE );

			button = gtk_grid_get_child_at(
							self->private->grid, DET_COL_DOWN, self->private->count-1 );
			gtk_widget_set_sensitive( button, TRUE );
		}

		button = gtk_grid_get_child_at(
						self->private->grid, DET_COL_DOWN, self->private->count );
		gtk_widget_set_sensitive( button, FALSE );
	}
}

static void
signal_row_added( ofaModelProperties *self )
{
	update_detail_buttons( self );
}

static void
signal_row_removed( ofaModelProperties *self )
{
	update_detail_buttons( self );
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
on_account_selection( ofaModelProperties *self, gint row )
{
	GtkEntry *entry;
	gchar *number;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT, row ));
	number = ofa_account_select_run( self->private->main_window, gtk_entry_get_text( entry ));
	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( entry, number );
	}
	g_free( number );
}

/*
 * we accept to save uncomplete detail lines
 */
static void
check_for_enable_dlg( ofaModelProperties *self )
{
	ofaModelPropertiesPrivate *priv;
	GtkWidget *button;
	ofoModel *exists;
	gboolean ok;

	priv = self->private;
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "btn-ok" );
	ok = ofo_model_is_valid( priv->mnemo, priv->label, priv->journal );

	if( ok ){
		exists = ofo_model_get_by_mnemo(
						ofa_main_window_get_dossier( self->private->main_window ),
						self->private->mnemo );
		ok &= !exists ||
				ofo_model_get_id( exists ) == ofo_model_get_id( self->private->model );
	}

	gtk_widget_set_sensitive( button, ok );
}

static void
exchange_rows( ofaModelProperties *self, gint row_a, gint row_b )
{
	gint i;
	GtkWidget *w_a, *w_b;

	for( i=0 ; i<DET_N_COLUMNS ; ++i ){

		w_a = gtk_grid_get_child_at( self->private->grid, i, row_a );
		g_object_ref( w_a );
		gtk_container_remove( GTK_CONTAINER( self->private->grid ), w_a );

		w_b = gtk_grid_get_child_at( self->private->grid, i, row_b );
		g_object_ref( w_b );
		gtk_container_remove( GTK_CONTAINER( self->private->grid ), w_b );

		gtk_grid_attach( self->private->grid, w_a, i, row_b, 1, 1 );
		g_object_set_data( G_OBJECT( w_a ), DATA_ROW, GINT_TO_POINTER( row_b ));

		gtk_grid_attach( self->private->grid, w_b, i, row_a, 1, 1 );
		g_object_set_data( G_OBJECT( w_b ), DATA_ROW, GINT_TO_POINTER( row_a ));

		g_object_unref( w_a );
		g_object_unref( w_b );
	}

	update_detail_buttons( self );
}

static void
on_button_clicked( GtkButton *button, ofaModelProperties *self )
{
	/*static const gchar *thisfn = "ofa_model_properties_on_button_clicked";*/
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));

	switch( column ){
		case DET_COL_ADD:
			add_empty_row( self );
			break;
		case DET_COL_ACCOUNT_SELECT:
			on_account_selection( self, row );
			break;
		case DET_COL_UP:
			g_return_if_fail( row > 1 );
			exchange_rows( self, row, row-1 );
			break;
		case DET_COL_DOWN:
			g_return_if_fail( row < self->private->count );
			exchange_rows( self, row, row+1 );
			break;
		case DET_COL_REMOVE:
			remove_row( self, row );
			break;
	}
}

static void
remove_row( ofaModelProperties *self, gint row )
{
	gint i, line;
	GtkWidget *widget;
	GtkEntry *entry;
	gchar *str;

	/* first remove the line */
	for( i=0 ; i<DET_N_COLUMNS ; ++i ){
		gtk_widget_destroy( gtk_grid_get_child_at( self->private->grid, i, row ));
	}

	/* then move the following lines one row up */
	for( line=row+1 ; line<=self->private->count+1 ; ++line ){
		for( i=0 ; i<DET_N_COLUMNS ; ++i ){
			widget = gtk_grid_get_child_at( self->private->grid, i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( self->private->grid ), widget );
				gtk_grid_attach( self->private->grid, widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
		if( line <= self->private->count ){
			/* update the rang number on each moved line */
			entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_RANG, line-1 ));
			str = g_strdup_printf( "%d", line-1 );
			gtk_entry_set_text( entry, str );
			g_free( str );
		}
	}

	self->private->count -= 1;
	signal_row_removed( self );

	gtk_widget_show_all( GTK_WIDGET( self->private->grid ));
}

static gboolean
do_update( ofaModelProperties *self )
{
	gchar *prev_mnemo;
	ofoDossier *dossier;
	ofoModel *exists;
	gint i;

	prev_mnemo = g_strdup( ofo_model_get_mnemo( self->private->model ));
	dossier = ofa_main_window_get_dossier( self->private->main_window );
	exists = ofo_model_get_by_mnemo( dossier, self->private->mnemo );
	g_return_val_if_fail(
			!exists || ofo_model_get_id( exists ) == ofo_model_get_id( self->private->model ), FALSE );

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même model (n'a pas été modifié)
	 */
	ofo_model_set_mnemo( self->private->model, self->private->mnemo );
	ofo_model_set_label( self->private->model, self->private->label );
	ofo_model_set_journal( self->private->model, self->private->journal );
	ofo_model_set_journal_locked( self->private->model, self->private->journal_locked );
	my_utils_getback_notes_ex( model );

	ofo_model_free_detail_all( self->private->model );
	for( i=1 ; i<=self->private->count ; ++i ){
		get_detail_list( self, i );
	}

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
get_detail_list( ofaModelProperties *self, gint row )
{
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *comment, *account, *label, *debit, *credit;
	gboolean account_locked, label_locked, debit_locked, credit_locked;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_COMMENT, row ));
	comment = gtk_entry_get_text( entry );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT, row ));
	account = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT_VER, row ));
	account_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL, row ));
	label = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL_VER, row ));
	label_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT, row ));
	debit = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT_VER, row ));
	debit_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT, row ));
	credit = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT_VER, row ));
	credit_locked = gtk_toggle_button_get_active( toggle );

	ofo_model_add_detail( self->private->model,
				comment,
				account, account_locked,
				label, label_locked,
				debit, debit_locked,
				credit, credit_locked );
}
