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
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/my-window-prot.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ope-template-properties.h"

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
struct _ofaOpeTemplatePropertiesPrivate {

	/* internals
	 */
	ofoOpeTemplate   *ope_template;
	ofaJournalCombo  *ledger_combo;
	GtkGrid          *grid;				/* detail grid */
	gint              count;			/* count of added detail lines */

	/* result
	 */
	gboolean          is_new;
	gboolean          updated;

	/* data
	 */
	gchar            *mnemo;
	gchar            *label;
	gchar            *ledger;			/* ledger mnemo */
	gboolean          ledger_locked;
	gchar            *upd_user;
	GTimeVal          upd_stamp;
};

/* columns in the detail treeview
 */
enum {
	DET_COL_ROW = 0,
	DET_COL_COMMENT,
	DET_COL_ACCOUNT,
	DET_COL_ACCOUNT_SELECT,
	DET_COL_ACCOUNT_LOCKED,
	DET_COL_LABEL,
	DET_COL_LABEL_LOCKED,
	DET_COL_DEBIT,
	DET_COL_DEBIT_LOCKED,
	DET_COL_CREDIT,
	DET_COL_CREDIT_LOCKED,
	DET_COL_UP,
	DET_COL_DOWN,
	DET_COL_REMOVE,
	DET_N_COLUMNS
};

#define DET_COL_ADD                  DET_COL_ROW

/* each widget of the grid brings its row number */
#define DATA_ROW                     "ofa-data-row"

/* buttons also bring their column number */
#define DATA_COLUMN                  "ofa-data-column"

/* space between widgets in a detail line */
#define DETAIL_SPACE                 2

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-entry-template-properties.ui";
static const gchar  *st_ui_id        = "OpeTemplatePropertiesDlg";

G_DEFINE_TYPE( ofaOpeTemplateProperties, ofa_ope_template_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_dialog_title( ofaOpeTemplateProperties *self );
static void      init_dialog_mnemo( ofaOpeTemplateProperties *self );
static void      init_dialog_label( ofaOpeTemplateProperties *self );
static void      init_dialog_ledger_locked( ofaOpeTemplateProperties *self );
static void      init_dialog_detail( ofaOpeTemplateProperties *self );
static void      insert_new_row( ofaOpeTemplateProperties *self, gint row );
static void      add_empty_row( ofaOpeTemplateProperties *self );
static void      add_button( ofaOpeTemplateProperties *self, const gchar *stock_id, gint column, gint row, gint left_margin, gint right_margin );
static void      signal_row_added( ofaOpeTemplateProperties *self );
static void      signal_row_removed( ofaOpeTemplateProperties *self );
static void      on_mnemo_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void      on_ledger_changed( const gchar *mnemo, ofaOpeTemplateProperties *self );
static void      on_ledger_locked_toggled( GtkToggleButton *toggle, ofaOpeTemplateProperties *self );
static void      on_account_selection( ofaOpeTemplateProperties *self, gint row );
static void      on_button_clicked( GtkButton *button, ofaOpeTemplateProperties *self );
static void      remove_row( ofaOpeTemplateProperties *self, gint row );
static void      check_for_enable_dlg( ofaOpeTemplateProperties *self );
static gboolean  is_dialog_validable( ofaOpeTemplateProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaOpeTemplateProperties *self );
static void      get_detail_list( ofaOpeTemplateProperties *self, gint row );

static void
ope_template_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_properties_finalize";
	ofaOpeTemplatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFA_OPE_TEMPLATE_PROPERTIES( instance )->private;

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->ledger );
	g_free( priv->upd_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_properties_parent_class )->finalize( instance );
}

static void
ope_template_properties_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_properties_parent_class )->dispose( instance );
}

static void
ofa_ope_template_properties_init( ofaOpeTemplateProperties *self )
{
	static const gchar *thisfn = "ofa_ope_template_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_PROPERTIES( self ));

	self->private = g_new0( ofaOpeTemplatePropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_ope_template_properties_class_init( ofaOpeTemplatePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_ope_template_properties_run:
 * @main: the main window of the application.
 * @model:
 * @ledger_id: set to the current ledger when creating a new model,
 *  left to OFO_BASE_UNSET_ID when updating an existing model.
 *
 * Update the properties of an model
 */
gboolean
ofa_ope_template_properties_run( ofaMainWindow *main_window, ofoOpeTemplate *model, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_template_properties_run";
	ofaOpeTemplateProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, model=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new(
					OFA_TYPE_OPE_TEMPLATE_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->private->ope_template = model;
	self->private->ledger = g_strdup( ledger );

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaOpeTemplateProperties *self;
	ofaOpeTemplatePropertiesPrivate *priv;
	ofaJournalComboParms parms;
	const gchar *mnemo;
	GtkWindow *toplevel;

	self = OFA_OPE_TEMPLATE_PROPERTIES( dialog );
	priv = self->private;
	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));

	init_dialog_title( self );
	init_dialog_mnemo( self );
	init_dialog_label( self );

	mnemo = ofo_ope_template_get_mnemo( priv->ope_template );
	priv->is_new = !mnemo || !g_utf8_strlen( mnemo, -1 );

	parms.container = GTK_CONTAINER( toplevel );
	parms.dossier = MY_WINDOW( dialog )->protected->dossier;
	parms.combo_name = "p1-ledger";
	parms.label_name = NULL;
	parms.disp_mnemo = TRUE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaJournalComboCb ) on_ledger_changed;
	parms.user_data = self;
	parms.initial_mnemo = priv->is_new ? priv->ledger : ofo_ope_template_get_ledger( priv->ope_template );

	priv->ledger_combo = ofa_journal_combo_new( &parms );

	init_dialog_ledger_locked( self );

	my_utils_init_notes_ex( toplevel, ope_template );
	my_utils_init_upd_user_stamp_ex( toplevel, ope_template );

	init_dialog_detail( self );
	check_for_enable_dlg( self );

	gtk_widget_grab_focus(
			my_utils_container_get_child_by_name(
						GTK_CONTAINER( toplevel ), "p1-mnemo" ));
}

static void
init_dialog_title( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	const gchar *mnemo;
	gchar *title;

	priv = self->private;
	mnemo = ofo_ope_template_get_mnemo( priv->ope_template );

	if( !mnemo ){
		title = g_strdup( _( "Defining a new entry template" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » entry template" ), mnemo );
	}

	gtk_window_set_title( my_window_get_toplevel( MY_WINDOW( self )), title );
	g_free( title );
}

static void
init_dialog_mnemo( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkEntry *entry;

	priv = self->private;

	priv->mnemo = g_strdup( ofo_ope_template_get_mnemo( priv->ope_template ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "p1-mnemo" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );
}

static void
init_dialog_label( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkEntry *entry;

	priv = self->private;

	priv->label = g_strdup( ofo_ope_template_get_label( priv->ope_template ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );
}

static void
init_dialog_ledger_locked( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkToggleButton *btn;

	priv = self->private;

	priv->ledger_locked = ofo_ope_template_get_ledger_locked( priv->ope_template );
	btn = GTK_TOGGLE_BUTTON( my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "p1-jou-locked" ));
	gtk_toggle_button_set_active( btn, priv->ledger_locked );

	g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_ledger_locked_toggled ), self );
}

/*
 * add one line per detail record
 */
static void
init_dialog_detail( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gint count, i;

	priv = self->private;

	priv->grid = ( GtkGrid * ) my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "p1-details" );
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));

	add_button( self, GTK_STOCK_ADD, DET_COL_ADD, 1, DETAIL_SPACE, 0 );
	count = ofo_ope_template_get_detail_count( self->private->ope_template );
	for( i=1 ; i<=count ; ++i ){
		insert_new_row( self, i );
	}
}

static void
insert_new_row( ofaOpeTemplateProperties *self, gint row )
{
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *str;

	add_empty_row( self );
	row = self->private->count;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_COMMENT, row ));
	str = ofo_ope_template_get_detail_comment( self->private->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT, row ));
	str = ofo_ope_template_get_detail_account( self->private->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_account_locked( self->private->ope_template, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL, row ));
	str = ofo_ope_template_get_detail_label( self->private->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_label_locked( self->private->ope_template, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT, row ));
	str = ofo_ope_template_get_detail_debit( self->private->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_debit_locked( self->private->ope_template, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT, row ));
	str = ofo_ope_template_get_detail_credit( self->private->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_credit_locked( self->private->ope_template, row-1 ));
}

static void
add_empty_row( ofaOpeTemplateProperties *self )
{
	gint row;
	GtkWidget *widget;
	GtkEntry *entry;
	GtkWidget *toggle;
	gchar *str;

	row = self->private->count + 1;
	g_return_if_fail( row >= 1 );

	widget = gtk_grid_get_child_at( self->private->grid, DET_COL_ADD, row );
	gtk_widget_destroy( widget );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, 3 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_ROW, row, 1, 1 );
	str = g_strdup_printf( "%d", row );
	gtk_entry_set_text( entry, str );
	g_free( str );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), 2*DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_COMMENT, row, 1, 1 );
	gtk_widget_grab_focus( GTK_WIDGET( entry ));

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), 2*DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 20 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_ACCOUNT, row, 1, 1 );

	add_button( self, GTK_STOCK_INDEX, DET_COL_ACCOUNT_SELECT, row, DETAIL_SPACE, 0 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_ACCOUNT_LOCKED, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), TRUE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 20 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_LABEL, row, 1, 1 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_LABEL_LOCKED, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_DEBIT, row, 1, 1 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_DEBIT_LOCKED, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( entry ), DET_COL_CREDIT, row, 1, 1 );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_grid_attach( self->private->grid, toggle, DET_COL_CREDIT_LOCKED, row, 1, 1 );

	add_button( self, GTK_STOCK_GO_UP, DET_COL_UP, row, DETAIL_SPACE, 0 );
	add_button( self, GTK_STOCK_GO_DOWN, DET_COL_DOWN, row, DETAIL_SPACE, 0 );
	add_button( self, GTK_STOCK_REMOVE, DET_COL_REMOVE, row, DETAIL_SPACE, 2*DETAIL_SPACE );
	add_button( self, GTK_STOCK_ADD, DET_COL_ADD, row+1, DETAIL_SPACE, 0 );

	self->private->count = row;

	signal_row_added( self );
	gtk_widget_show_all( GTK_WIDGET( self->private->grid ));
}

static void
add_button( ofaOpeTemplateProperties *self, const gchar *stock_id, gint column, gint row, gint left_margin, gint right_margin )
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
update_detail_buttons( ofaOpeTemplateProperties *self )
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
signal_row_added( ofaOpeTemplateProperties *self )
{
	update_detail_buttons( self );
}

static void
signal_row_removed( ofaOpeTemplateProperties *self )
{
	update_detail_buttons( self );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaOpeTemplateProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaOpeTemplateProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_ledger_changed( const gchar *mnemo, ofaOpeTemplateProperties *self )
{
	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_PROPERTIES( self ));

	g_free( self->private->ledger );
	self->private->ledger = g_strdup( mnemo );

	check_for_enable_dlg( self );
}

static void
on_ledger_locked_toggled( GtkToggleButton *btn, ofaOpeTemplateProperties *self )
{
	self->private->ledger_locked = gtk_toggle_button_get_active( btn );
	check_for_enable_dlg( self );
}

static void
on_account_selection( ofaOpeTemplateProperties *self, gint row )
{
	GtkEntry *entry;
	gchar *number;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT, row ));
	number = ofa_account_select_run(
					MY_WINDOW( self )->protected->main_window,
					gtk_entry_get_text( entry ));
	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( entry, number );
	}
	g_free( number );
}

static void
exchange_rows( ofaOpeTemplateProperties *self, gint row_a, gint row_b )
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
on_button_clicked( GtkButton *button, ofaOpeTemplateProperties *self )
{
	/*static const gchar *thisfn = "ofa_ope_template_properties_on_button_clicked";*/
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
remove_row( ofaOpeTemplateProperties *self, gint row )
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
			entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ROW, line-1 ));
			str = g_strdup_printf( "%d", line-1 );
			gtk_entry_set_text( entry, str );
			g_free( str );
		}
	}

	self->private->count -= 1;
	signal_row_removed( self );

	gtk_widget_show_all( GTK_WIDGET( self->private->grid ));
}

/*
 * we accept to save uncomplete detail lines
 */
static void
check_for_enable_dlg( ofaOpeTemplateProperties *self )
{
	GtkWidget *button;
	gboolean ok;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	ok = is_dialog_validable( self );

	gtk_widget_set_sensitive( button, ok );
}

/*
 * we accept to save uncomplete detail lines
 */
static gboolean
is_dialog_validable( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	ofoOpeTemplate *exists;
	gboolean ok;

	priv = self->private;

	ok = ofo_ope_template_is_valid( priv->mnemo, priv->label, priv->ledger );

	if( ok ){
		exists = ofo_ope_template_get_by_mnemo(
						MY_WINDOW( self )->protected->dossier,
						priv->mnemo );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_ope_template_get_mnemo( priv->ope_template )));
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_OPE_TEMPLATE_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gchar *prev_mnemo;
	gint i;

	prev_mnemo = g_strdup( ofo_ope_template_get_mnemo( self->private->ope_template ));
	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->private;

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même model (n'a pas été modifié)
	 */
	ofo_ope_template_set_mnemo( priv->ope_template, priv->mnemo );
	ofo_ope_template_set_label( priv->ope_template, priv->label );
	ofo_ope_template_set_ledger( priv->ope_template, priv->ledger );
	ofo_ope_template_set_ledger_locked( priv->ope_template, priv->ledger_locked );
	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), ope_template );

	ofo_ope_template_free_detail_all( priv->ope_template );
	for( i=1 ; i<=priv->count ; ++i ){
		get_detail_list( self, i );
	}

	if( !prev_mnemo ){
		priv->updated =
				ofo_ope_template_insert( priv->ope_template );
	} else {
		priv->updated =
				ofo_ope_template_update( priv->ope_template, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}

static void
get_detail_list( ofaOpeTemplateProperties *self, gint row )
{
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *comment, *account, *label, *debit, *credit;
	gboolean account_locked, label_locked, debit_locked, credit_locked;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_COMMENT, row ));
	comment = gtk_entry_get_text( entry );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT, row ));
	account = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_ACCOUNT_LOCKED, row ));
	account_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL, row ));
	label = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_LABEL_LOCKED, row ));
	label_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT, row ));
	debit = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_DEBIT_LOCKED, row ));
	debit_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT, row ));
	credit = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( self->private->grid, DET_COL_CREDIT_LOCKED, row ));
	credit_locked = gtk_toggle_button_get_active( toggle );

	ofo_ope_template_add_detail( self->private->ope_template,
				comment,
				account, account_locked,
				label, label_locked,
				debit, debit_locked,
				credit, credit_locked );
}
