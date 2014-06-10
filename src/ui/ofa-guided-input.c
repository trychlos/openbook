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
#include <stdlib.h>

#include "ui/my-utils.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-account.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-model.h"
#include "ui/ofo-taux.h"

/* private instance data
 */
struct _ofaGuidedInputPrivate {

	/* internals
	 */
	const ofoModel *model;
	GtkGrid        *view;				/* entries view container */
	gboolean        deffet_has_focus;
	gboolean        deffet_changed_while_focus;
	GDate           last_closed_exe;
	GDate           last_closing;		/* max of closed exercice and closed journal */

	/* data
	 */
	gchar          *journal;
	GDate           dope;
	GDate           deff;
	gdouble         total_debits;
	gdouble         total_credits;
};

/*
 * columns in the grid view
 */
enum {
	COL_RANG = 0,
	FIRST_COLUMN,
	COL_ACCOUNT = FIRST_COLUMN,
	COL_ACCOUNT_SELECT,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	N_COLUMNS
};

/*
 * helpers
 */
typedef struct {
	gint	        column_id;
	gchar          *letter;
	const gchar * (*get_label)( const ofoModel *, gint );
	gboolean      (*is_locked)( const ofoModel *, gint );
	gint            width;
	float           xalign;
	gboolean        expand;
}
	sColumnDef;

#define AMOUNTS_WIDTH                  10
#define RANG_WIDTH                      3
#define TOTAUX_TOP_MARGIN               8

/* space between widgets in a detail line */
#define DETAIL_SPACE                    2

/*
 * this works because column_id is greater than zero
 * and this is ok because the column #0 is used by the number of the row
 */
static sColumnDef st_col_defs[] = {
		{ COL_ACCOUNT,
				"A",
				ofo_model_get_detail_account,
				ofo_model_get_detail_account_locked, 10, 0.0, FALSE },
		{ COL_ACCOUNT_SELECT,
				NULL,
				NULL,
				NULL, 0, 0, FALSE },
		{ COL_LABEL,
				"L",
				ofo_model_get_detail_label,
				ofo_model_get_detail_label_locked,   20, 0.0, TRUE },
		{ COL_DEBIT,
				"D",
				ofo_model_get_detail_debit,
				ofo_model_get_detail_debit_locked,   AMOUNTS_WIDTH, 1.0, FALSE },
		{ COL_CREDIT,
				"C",
				ofo_model_get_detail_credit,
				ofo_model_get_detail_credit_locked,  AMOUNTS_WIDTH, 1.0, FALSE },
		{ 0 }
};

#define DATA_COLUMN				    "data-entry-left"
#define DATA_ROW				    "data-entry-top"

static const gchar  *st_ui_xml    = PKGUIDIR "/ofa-guided-input.ui";
static const gchar  *st_ui_id     = "GuidedInputDlg";

static GDate         st_last_dope = { 0 };
static GDate         st_last_deff = { 0 };

G_DEFINE_TYPE( ofaGuidedInput, ofa_guided_input, OFA_TYPE_BASE_DIALOG )

static void      v_init_dialog( ofaBaseDialog *dialog );
static void      init_dialog_journal( ofaGuidedInput *self );
static void      init_dialog_entries( ofaGuidedInput *self );
static void      add_row_entry( ofaGuidedInput *self, gint i );
static void      add_row_entry_set( ofaGuidedInput *self, gint col_id, gint row );
static void      add_button( ofaGuidedInput *self, const gchar *stock_id, gint column, gint row );
static const sColumnDef *find_column_def_from_col_id( ofaGuidedInput *self, gint col_id );
static const sColumnDef *find_column_def_from_letter( ofaGuidedInput *self, gchar letter );
static void      on_journal_changed( const gchar *mnemo, ofaGuidedInput *self );
static void      on_dope_changed( GtkEntry *entry, ofaGuidedInput *self );
static gboolean  on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static gboolean  on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static void      on_deffet_changed( GtkEntry *entry, ofaGuidedInput *self );
static gboolean  on_deffet_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static gboolean  on_deffet_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static void      on_account_selection( ofaGuidedInput *self, gint row );
static void      on_button_clicked( GtkButton *button, ofaGuidedInput *self );
static void      on_entry_changed( GtkEntry *entry, ofaGuidedInput *self );
static gboolean  on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static gboolean  on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static gboolean  on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedInput *self );
static void      check_for_account( ofaGuidedInput *self, GtkEntry *entry  );
static void      check_for_enable_dlg( ofaGuidedInput *self );
static gboolean  is_dialog_validable( ofaGuidedInput *self );
static void      update_all_formulas( ofaGuidedInput *self );
static void      update_formula( ofaGuidedInput *self, const gchar *formula, GtkEntry *entry );
static gdouble   compute_formula_solde( ofaGuidedInput *self, gint column_id, gint row );
static void      update_all_totals( ofaGuidedInput *self );
static gdouble   get_amount( ofaGuidedInput *self, gint col_id, gint row );
static gboolean  check_for_journal( ofaGuidedInput *self );
static gboolean  check_for_dates( ofaGuidedInput *self );
static gboolean  check_for_all_entries( ofaGuidedInput *self );
static gboolean  check_for_entry( ofaGuidedInput *self, gint row );
static void      set_comment( ofaGuidedInput *self, const gchar *comment );
static gboolean  v_quit_on_ok( ofaBaseDialog *dialog );
static gboolean  do_update( ofaGuidedInput *self );
static ofoEntry *entry_from_detail( ofaGuidedInput *self, gint row, const gchar *piece );

static void
guided_input_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_input_finalize";
	ofaGuidedInputPrivate *priv;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( instance ));

	priv = OFA_GUIDED_INPUT( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->journal );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_parent_class )->finalize( instance );
}

static void
guided_input_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_GUIDED_INPUT( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_parent_class )->dispose( instance );
}

static void
ofa_guided_input_init( ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_init";

	g_return_if_fail( OFA_IS_GUIDED_INPUT( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaGuidedInputPrivate, 1 );

	g_date_clear( &self->private->dope, 1 );
	g_date_clear( &self->private->deff, 1 );
}

static void
ofa_guided_input_class_init( ofaGuidedInputClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_input_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_input_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_guided_input_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
void
ofa_guided_input_run( ofaMainWindow *main_window, const ofoModel *model )
{
	static const gchar *thisfn = "ofa_guided_input_run";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, model=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new(
					OFA_TYPE_GUIDED_INPUT,
					OFA_PROP_MAIN_WINDOW, main_window,
					OFA_PROP_DIALOG_XML,  st_ui_xml,
					OFA_PROP_DIALOG_NAME, st_ui_id,
					NULL );

	self->private->model = model;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( ofaBaseDialog *dialog )
{
	ofaGuidedInputPrivate *priv;
	GtkWidget *entry;
	gchar *str;
	const GDate *date;

	priv = OFA_GUIDED_INPUT( dialog )->private;

	init_dialog_journal( OFA_GUIDED_INPUT( dialog ));

	date = ofo_dossier_get_last_closed_exercice(
						ofa_base_dialog_get_dossier( dialog ));
	if( date ){
		memcpy( &priv->last_closed_exe, date, sizeof( GDate ));
	}

	memcpy( &priv->last_closing, &priv->last_closed_exe, sizeof( GDate ));

	memcpy( &priv->dope, &st_last_dope, sizeof( GDate ));
	str = my_utils_display_from_date( &priv->dope, MY_UTILS_DATE_DDMM );
	entry = my_utils_container_get_child_by_name(
					GTK_CONTAINER( dialog->prot->dialog ), "p1-dope" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_dope_changed ), dialog );
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_dope_focus_in ), dialog );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_dope_focus_out ), dialog );

	memcpy( &priv->deff, &st_last_deff, sizeof( GDate ));
	str = my_utils_display_from_date( &priv->deff, MY_UTILS_DATE_DDMM );
	entry = my_utils_container_get_child_by_name(
					GTK_CONTAINER( dialog->prot->dialog ), "p1-deffet" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_deffet_changed ), dialog );
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_deffet_focus_in ), dialog );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_deffet_focus_out ), dialog );

	init_dialog_entries( OFA_GUIDED_INPUT( dialog ));

	check_for_enable_dlg( OFA_GUIDED_INPUT( dialog ));
}

static void
init_dialog_journal( ofaGuidedInput *self )
{
	ofaGuidedInputPrivate *priv;
	GtkWidget *combo;
	ofaJournalComboParms parms;

	priv = self->private;

	priv->journal = g_strdup( ofo_model_get_journal( priv->model ));

	parms.container = GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog );
	parms.dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));
	parms.combo_name = "p1-journal";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfn = ( ofaJournalComboCb ) on_journal_changed;
	parms.user_data = self;
	parms.initial_mnemo = priv->journal;

	ofa_journal_combo_init_combo( &parms );

	combo = my_utils_container_get_child_by_name( parms.container, "p1-journal" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	gtk_widget_set_sensitive( combo, !ofo_model_get_journal_locked( priv->model ));
}

static void
init_dialog_entries( ofaGuidedInput *self )
{
	GtkWidget *view;
	gint count,i;
	GtkLabel *label;
	GtkEntry *entry;

	view = my_utils_container_get_child_by_name(
						GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-entries" );
	g_return_if_fail( view && GTK_IS_GRID( view ));
	self->private->view = GTK_GRID( view );

	count = ofo_model_get_detail_count( self->private->model );
	for( i=0 ; i<count ; ++i ){
		add_row_entry( self, i );
	}

	label = GTK_LABEL( gtk_label_new( _( "Total :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( label ), TOTAUX_TOP_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_LABEL, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_DEBIT, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_CREDIT, count+1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Diff :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_LABEL, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_DEBIT, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_CREDIT, count+2, 1, 1 );
}

static void
add_row_entry( ofaGuidedInput *self, gint i )
{
	GtkEntry *entry;
	gchar *str;

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", i+1 );
	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_text( entry, str );
	gtk_entry_set_width_chars( entry, RANG_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_RANG, i+1, 1, 1 );
	g_free( str );

	/* other columns starting with COL_ACCOUNT=1 */
	add_row_entry_set( self, COL_ACCOUNT, i+1 );
	add_button( self, GTK_STOCK_INDEX, COL_ACCOUNT_SELECT, i+1 );
	add_row_entry_set( self, COL_LABEL, i+1 );
	add_row_entry_set( self, COL_DEBIT, i+1 );
	add_row_entry_set( self, COL_CREDIT, i+1 );
}

static void
add_row_entry_set( ofaGuidedInput *self, gint col_id, gint row )
{
	GtkEntry *entry;
	const sColumnDef *col_def;
	const gchar *str;
	gboolean locked;

	col_def = find_column_def_from_col_id( self, col_id );
	g_return_if_fail( col_def );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), col_def->expand );
	gtk_entry_set_width_chars( entry, col_def->width );
	gtk_entry_set_alignment( entry, col_def->xalign );

	str = (*col_def->get_label)( self->private->model, row-1 );

	if( str && !ofo_model_detail_is_formula( str )){
		gtk_entry_set_text( entry, str );
	}

	locked = (*col_def->is_locked)( self->private->model, row-1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), !locked );

	g_object_set_data( G_OBJECT( entry ), DATA_COLUMN, GINT_TO_POINTER( col_id ));
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));

	if( !locked ){
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_entry_changed ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_entry_focus_in ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_entry_focus_out ), self );
		g_signal_connect( G_OBJECT( entry ), "key-press-event", G_CALLBACK( on_key_pressed ), self );
	}

	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), col_id, row, 1, 1 );
}

static void
add_button( ofaGuidedInput *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_stock( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->private->view, GTK_WIDGET( button ), column, row, 1, 1 );
}

static const sColumnDef *
find_column_def_from_col_id( ofaGuidedInput *self, gint col_id )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].column_id == col_id ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

static const sColumnDef *
find_column_def_from_letter( ofaGuidedInput *self, gchar letter )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].letter[0] == letter ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

/*
 * ofaJournalCombo callback
 *
 * setup the last closing date as the maximum of :
 * - the last exercice closing date
 * - the last journal closing date
 *
 * this last closing date is the lower limit of the effect dates
 */
static void
on_journal_changed( const gchar *mnemo, ofaGuidedInput *self )
{
	ofaGuidedInputPrivate *priv;
	ofoDossier *dossier;
	ofoJournal *journal;
	gint exe_id;
	const GDate *date;

	priv = self->private;

	g_free( priv->journal );
	priv->journal = g_strdup( mnemo );

	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));
	journal = ofo_journal_get_by_mnemo( dossier, mnemo );
	memcpy( &priv->last_closing, &priv->last_closed_exe, sizeof( GDate ));

	if( journal ){
		exe_id = ofo_dossier_get_current_exe_id( dossier );
		date = ofo_journal_get_cloture( journal, exe_id );
		if( date && g_date_valid( date )){
			if( g_date_valid( &priv->last_closed_exe )){
				if( g_date_compare( date, &priv->last_closed_exe ) > 0 ){
					memcpy( &priv->last_closing, date, sizeof( GDate ));
				}
			} else {
				memcpy( &priv->last_closing, date, sizeof( GDate ));
			}
		}
	}

	check_for_enable_dlg( self );
}

/*
 * setting the deffet also triggers the change signal of the deffet
 * field (and so the comment)
 * => we should only react to the content while the focus is in the
 *    field
 * More, we shouldn't triggers an automatic changes to a field which
 * has been manually modified
 */
static void
set_date_comment( ofaGuidedInput *self, const gchar *label, const GDate *date )
{
	gchar *str, *comment;

	str = my_utils_display_from_date( date, MY_UTILS_DATE_DMMM );
	if( !g_utf8_strlen( str, -1 )){
		g_free( str );
		str = g_strdup( _( "invalid" ));
	}
	comment = g_strdup_printf( "%s : %s", label, str );
	set_comment( self, comment );
	g_free( comment );
	g_free( str );
}

static void
on_dope_changed( GtkEntry *entry, ofaGuidedInput *self )
{
	/*static const gchar *thisfn = "ofa_guided_input_on_dope_changed";*/
	GtkWidget *wdeff;
	gchar *str;

	/*g_debug( "%s: entry=%p, self=%p", thisfn, ( void * ) entry, ( void * ) self );*/

	/* check the operation date */
	g_date_set_parse( &self->private->dope, gtk_entry_get_text( entry ));
	set_date_comment( self, _( "Operation date" ), &self->private->dope );

	/* setup the effect date if it has not been manually changed */
	if( g_date_valid( &self->private->dope ) &&
			!self->private->deffet_changed_while_focus ){

		if( g_date_valid( &self->private->last_closing ) &&
			g_date_compare( &self->private->last_closing, &self->private->dope ) > 0 ){

			memcpy( &self->private->deff, &self->private->last_closing, sizeof( GDate ));
			g_date_add_days( &self->private->deff, 1 );

		} else {
			memcpy( &self->private->deff, &self->private->dope, sizeof( GDate ));
		}

		str = my_utils_display_from_date( &self->private->deff, MY_UTILS_DATE_DDMM );
		wdeff = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-deffet" );
		if( entry && GTK_IS_WIDGET( wdeff )){
			gtk_entry_set_text( GTK_ENTRY( wdeff ), str );
		}
		g_free( str );
	}

	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	set_date_comment( self, _( "Operation date" ), &self->private->dope );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	set_comment( self, "" );

	return( FALSE );
}

static void
on_deffet_changed( GtkEntry *entry, ofaGuidedInput *self )
{
	/*static const gchar *thisfn = "ofa_guided_input_on_deffet_changed";*/

	/* the focus is not set to the entry
	widget = gtk_window_get_focus( GTK_WINDOW( self->private->main_window ));
	g_debug( "%s: entry=%p, self=%p, focus_widget=%p",
			thisfn, ( void * ) entry, ( void * ) self, ( void * ) widget );*/

	if( self->private->deffet_has_focus ){

		self->private->deffet_changed_while_focus = TRUE;
		g_date_set_parse( &self->private->deff, gtk_entry_get_text( entry ));
		set_date_comment( self, _( "Effect date" ), &self->private->deff );

		check_for_enable_dlg( self );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffet_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	self->private->deffet_has_focus = TRUE;
	set_date_comment( self, _( "Effect date" ), &self->private->deff );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffet_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	self->private->deffet_has_focus = FALSE;
	set_comment( self, "" );

	return( FALSE );
}

static void
on_account_selection( ofaGuidedInput *self, gint row )
{
	GtkEntry *entry;
	gchar *number;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->view, COL_ACCOUNT, row ));

	number = ofa_account_select_run(
						ofa_base_dialog_get_main_window( OFA_BASE_DIALOG( self )),
						gtk_entry_get_text( entry ));

	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( entry, number );
	}

	g_free( number );
}

static void
on_button_clicked( GtkButton *button, ofaGuidedInput *self )
{
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));

	switch( column ){
		case COL_ACCOUNT_SELECT:
			on_account_selection( self, row );
			break;
	}
}

static void
on_entry_changed( GtkEntry *entry, ofaGuidedInput *self )
{
	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	gint row;
	const gchar *comment;

	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
	if( row > 0 ){
		comment = ofo_model_get_detail_comment( self->private->model, row-1 );
		set_comment( self, comment ? comment : "" );
	}

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	set_comment( self, "" );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedInput *self )
{
	gint left;

	/* check the entry we are leaving
	 */
	left = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( widget ), DATA_COLUMN ));

	switch( left ){
		case COL_ACCOUNT:
			if( event->state == 0 && event->keyval == GDK_KEY_Tab ){
				check_for_account( self, GTK_ENTRY( widget ));
			}
			break;
	}

	return( FALSE );
}

/*
 * check that the account exists
 * else open a dialog for selection
 */
static void
check_for_account( ofaGuidedInput *self, GtkEntry *entry  )
{
	ofoDossier *dossier;
	ofoAccount *account;
	const gchar *asked_account;
	gchar *number;

	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));
	asked_account = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( dossier, asked_account );
	if( !account || ofo_account_is_root( account )){
		number = ofa_account_select_run(
							ofa_base_dialog_get_main_window( OFA_BASE_DIALOG( self )),
							asked_account );
		if( number ){
			gtk_entry_set_text( entry, number );
			g_free( number );
		}
	}
}

/*
 * this is called after each field changes
 * so a good place to handle all modifications
 *
 * Note that we control *all* fields so that we are able to visually
 * highlight the erroneous ones
 */
static void
check_for_enable_dlg( ofaGuidedInput *self )
{
	gboolean ok;
	GtkWidget *btn;

	if( self->private->view ){
		g_return_if_fail( GTK_IS_GRID( self->private->view ));

		ok = is_dialog_validable( self );

		btn = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );
		g_return_if_fail( btn && GTK_IS_BUTTON( btn ));

		gtk_widget_set_sensitive( btn, ok );
	}
}

static gboolean
is_dialog_validable( ofaGuidedInput *self )
{
	gboolean ok;

	update_all_formulas( self );
	update_all_totals( self );

	ok = TRUE;
	ok &= check_for_journal( self );
	ok &= check_for_dates( self );
	ok &= check_for_all_entries( self );

	return( ok );
}

static void
update_all_formulas( ofaGuidedInput *self )
{
	gint count, idx, col_id;
	const sColumnDef *col_def;
	const gchar *str;
	GtkWidget *entry;

	count = ofo_model_get_detail_count( self->private->model );
	for( idx=0 ; idx<count ; ++idx ){
		for( col_id=FIRST_COLUMN ; col_id<N_COLUMNS ; ++col_id ){
			col_def = find_column_def_from_col_id( self, col_id );
			if( col_def && col_def->get_label ){
				str = ( *col_def->get_label )( self->private->model, idx );
				if( ofo_model_detail_is_formula( str )){
					entry = gtk_grid_get_child_at( self->private->view, col_id, idx+1 );
					if( entry && GTK_IS_ENTRY( entry )){
						update_formula( self, str, GTK_ENTRY( entry ));
					}
				}
			}
		}
	}
}

static void
update_formula( ofaGuidedInput *self, const gchar *formula, GtkEntry *entry )
{
	static const gchar *thisfn = "ofa_guided_input_update_formula";
	gchar **tokens, **iter;
	gchar init;
	ofoTaux *rate;
	gdouble solde;
	gchar *str;
	gint col, row;
	GtkWidget *widget;
	const sColumnDef *col_def;

	tokens = g_regex_split_simple( "[-+*/]", formula+1, 0, 0 );
	iter = tokens;
	g_debug( "%s: formula='%s'", thisfn, formula );
	while( *iter ){
		/*g_debug( "%s: iter='%s'", thisfn, *iter );*/
		/*
		 * we have:
		 * - [ALDC]<number>
		 *    A: account
		 *     L: label
		 *      D: debit
		 *       C: credit
		 * or:
		 * - a token:
		 *   SOLDE
		 *   IDEM (same column, previous row)
		 * or:
		 * - a rate mnemonic
		 */
		if( !g_utf8_collate( *iter, "SOLDE" )){
			col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_COLUMN ));
			row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
			solde = compute_formula_solde( self, col, row );
			str = g_strdup_printf( "%.2lf", solde );
			gtk_entry_set_text( entry, str );
			g_free( str );

		} else if( !g_utf8_collate( *iter, "IDEM" )){
			row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
			col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_COLUMN ));
			widget = gtk_grid_get_child_at( self->private->view, col, row-1 );
			if( widget && GTK_IS_ENTRY( widget )){
				gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
			}

		} else {
			init = *iter[0];
			row = atoi(( *iter )+1 );
			/*g_debug( "%s: init=%c row=%d", thisfn, init, row );*/
			if( row > 0 && row <= ofo_model_get_detail_count( self->private->model )){
				col_def = find_column_def_from_letter( self, init );
				if( col_def ){
					widget = gtk_grid_get_child_at( self->private->view, col_def->column_id, row );
					if( widget && GTK_IS_ENTRY( widget )){
						gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
					} else {
						g_warning( "%s: no entry found at col=%d, row=%d", thisfn, col_def->column_id, row );
					}
				} else {
					g_warning( "%s: col_def not found for '%c' letter", thisfn, init );
				}

			} else {
				g_debug( "%s: searching for taux %s", thisfn, *iter );
				rate = ofo_taux_get_by_mnemo(
								ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )),
								*iter );
				if( rate && OFO_IS_TAUX( rate )){
					g_warning( "%s: TODO", thisfn );
				} else {
					g_warning( "%s: taux %s not found", thisfn, *iter );
				}
			}
		}
		iter++;
	}

	g_strfreev( tokens );
}

static gdouble
compute_formula_solde( ofaGuidedInput *self, gint column_id, gint row )
{
	gdouble dsold, csold;
	gint count, idx;

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		if( column_id != COL_DEBIT || row != idx+1 ){
			dsold += get_amount( self, COL_DEBIT, idx+1 );
		}
		if( column_id != COL_CREDIT || row != idx+1 ){
			csold += get_amount( self, COL_CREDIT, idx+1 );
		}
	}

	return( column_id == COL_DEBIT ? csold-dsold : dsold-csold );
}

/*
 * totals and diffs are set are rows (count+1) and (count+2) respectively
 */
static void
update_all_totals( ofaGuidedInput *self )
{
	gdouble dsold, csold;
	gint count, idx;
	GtkWidget *entry;
	gchar *str, *str2;

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		dsold += get_amount( self, COL_DEBIT, idx+1 );
		csold += get_amount( self, COL_CREDIT, idx+1 );
	}

	self->private->total_debits = dsold;
	self->private->total_credits = csold;

	entry = gtk_grid_get_child_at( self->private->view, COL_DEBIT, count+1 );
	if( entry && GTK_IS_ENTRY( entry )){
		str = g_strdup_printf( "%.2lf", dsold );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_CREDIT, count+1 );
	if( entry && GTK_IS_ENTRY( entry )){
		str = g_strdup_printf( "%.2lf", csold );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}

	if( dsold > csold ){
		str = g_strdup_printf( "%.2lf", dsold-csold );
		str2 = g_strdup( "" );
	} else if( dsold < csold ){
		str = g_strdup( "" );
		str2 = g_strdup_printf( "%.2lf", csold-dsold );
	} else {
		str = g_strdup( "" );
		str2 = g_strdup( "" );
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_DEBIT, count+2 );
	if( entry && GTK_IS_ENTRY( entry )){
		gtk_entry_set_text( GTK_ENTRY( entry ), str2 );
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_CREDIT, count+2 );
	if( entry && GTK_IS_ENTRY( entry )){
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
	}

	g_free( str );
	g_free( str2 );
}

static gdouble
get_amount( ofaGuidedInput *self, gint col_id, gint row )
{
	const sColumnDef *col_def;
	GtkWidget *entry;

	col_def = find_column_def_from_col_id( self, col_id );
	if( col_def ){
		entry = gtk_grid_get_child_at( self->private->view, col_def->column_id, row );
		if( entry && GTK_IS_ENTRY( entry )){
			return( g_strtod( gtk_entry_get_text( GTK_ENTRY( entry )), NULL ));
		}
	}

	return( 0.0 );
}

/*
 * Returns TRUE if a journal is set
 */
static gboolean
check_for_journal( ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_check_for_journal";
	gboolean ok;

	ok = self->private->journal && g_utf8_strlen( self->private->journal, -1 );
	if( !ok ){
		g_debug( "%s: journal=%s", thisfn, self->private->journal );
	}

	return( ok );
}

/*
 * Returns TRUE if the dates are set and valid
 *
 * The first valid effect date is older than:
 * - the last exercice closing date of the dossier (if set)
 * - the last closing date of the journal (if set)
 */
static gboolean
check_for_dates( ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_check_for_dates";
	gboolean ok, oki;
	GtkWidget *entry;

	ok = TRUE;

	entry = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-dope" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	oki = g_date_valid( &self->private->dope );
	my_utils_entry_set_valid( GTK_ENTRY( entry ), oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: operation date is invalid", thisfn );
	}

	entry = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-deffet" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	oki = g_date_valid( &self->private->deff );
	my_utils_entry_set_valid( GTK_ENTRY( entry ), oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: effect date is invalid", thisfn );
	}

	if( g_date_valid( &self->private->last_closing )){
		oki = g_date_compare( &self->private->last_closing, &self->private->deff ) < 0;
		ok &= oki;
		if( !oki ){
			g_debug( "%s: effect date less than last closing", thisfn );
		}
	}

	return( ok );
}

/*
 * Returns TRUE if the entries are valid:
 * - for entries which have a not-nul balance:
 *   > account is valid
 *   > label is set
 * - totals are the same (no diff) and not nuls
 *
 * Note that have to check *all* entries in order to be able to visually
 * highlight the erroneous fields
 */
static gboolean
check_for_all_entries( ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_check_for_all_entries";
	gboolean ok, oki;
	gint count, idx;
	gdouble deb, cred;

	ok = TRUE;
	count = ofo_model_get_detail_count( self->private->model );

	for( idx=0 ; idx<count ; ++idx ){
		deb = get_amount( self, COL_DEBIT, idx+1 );
		cred = get_amount( self, COL_CREDIT, idx+1 );
		if( deb+cred != 0.0 ){
			if( !check_for_entry( self, idx+1 )){
				ok = FALSE;
			}
		}
	}

	oki = self->private->total_debits == self->private->total_credits;
	ok &= oki;
	if( !oki ){
		g_debug( "%s: totals are not equal: debits=%2.lf, credits=%.2lf",
				thisfn, self->private->total_debits, self->private->total_credits );
	}

	oki= (self->private->total_debits != 0.0 || self->private->total_credits != 0.0 );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: one total is nul: debits=%2.lf, credits=%.2lf",
				thisfn, self->private->total_debits, self->private->total_credits );
	}

	return( ok );
}

static gboolean
check_for_entry( ofaGuidedInput *self, gint row )
{
	static const gchar *thisfn = "ofa_guided_input_check_for_entry";
	gboolean ok, oki;
	GtkWidget *entry;
	ofoAccount *account;
	const gchar *str;

	ok = TRUE;

	entry = gtk_grid_get_child_at( self->private->view, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

	account = ofo_account_get_by_number(
						ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )),
						gtk_entry_get_text( GTK_ENTRY( entry )));
	oki = ( account && OFO_IS_ACCOUNT( account ));
	ok &= oki;
	if( !oki ){
		g_debug( "%s: account number=%s", thisfn, gtk_entry_get_text( GTK_ENTRY( entry )));
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	str = gtk_entry_get_text( GTK_ENTRY( entry ));
	oki = ( str && g_utf8_strlen( str, -1 ));
	ok &= oki;
	if( !oki ){
		g_debug( "%s: label=%s", thisfn, gtk_entry_get_text( GTK_ENTRY( entry )));
	}

	return( ok );
}

static void
set_comment( ofaGuidedInput *self, const gchar *comment )
{
	GtkWidget *widget;

	widget = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-comment" );
	if( widget && GTK_IS_ENTRY( widget )){
		gtk_entry_set_text( GTK_ENTRY( widget ), comment );
	}
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_GUIDED_INPUT( dialog )));
}

/*
 * generate the entries
 * all the entries are created in memory and checked before being
 * serialized. Only after that, journal and accounts are updated.
 */
static gboolean
do_update( ofaGuidedInput *self )
{
	gboolean ok;
	GtkWidget *entry;
	gint count, idx;
	gdouble deb, cred;
	const gchar *piece;
	ofoDossier *dossier;
	GList *entries, *it;
	ofoEntry *record;
	gint errors;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	piece = NULL;
	entry = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-piece" );
	if( entry && GTK_IS_ENTRY( entry )){
		piece = gtk_entry_get_text( GTK_ENTRY( entry ));
	}

	count = ofo_model_get_detail_count( self->private->model );
	entries = NULL;
	errors = 0;
	ok = FALSE;

	for( idx=0 ; idx<count ; ++idx ){
		deb = get_amount( self, COL_DEBIT, idx+1 );
		cred = get_amount( self, COL_CREDIT, idx+1 );
		if( deb+cred != 0.0 ){
			record = entry_from_detail( self, idx+1, piece );
			if( record ){
				entries = g_list_append( entries, record );
			} else {
				errors += 1;
			}
		}
	}

	if( !errors ){
		ok = TRUE;
		dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));
		for( it=entries ; it ; it=it->next ){
			ok &= ofo_entry_insert( OFO_ENTRY( it->data ), dossier );
			/* TODO:
			 * in case of an error, remove the already recorded entries
			 * of the list, decrementing the journals and the accounts
			 * then restore the last ecr number of the dossier
			 */
		}
	}

	g_list_free_full( entries, g_object_unref );

	memcpy( &st_last_dope, &self->private->dope, sizeof( GDate ));
	memcpy( &st_last_deff, &self->private->deff, sizeof( GDate ));

	return( ok );
}

/*
 * create an entry in memory, adding it to the list
 */
static ofoEntry *
entry_from_detail( ofaGuidedInput *self, gint row, const gchar *piece )
{
	ofaGuidedInputPrivate *priv;
	GtkWidget *entry;
	const gchar *account_number;
	ofoAccount *account;
	const gchar *label;
	gdouble deb, cre;

	priv = self->private;

	entry = gtk_grid_get_child_at( priv->view, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	account_number = gtk_entry_get_text( GTK_ENTRY( entry ));
	account = ofo_account_get_by_number(
						ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )),
						account_number );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	entry = gtk_grid_get_child_at( priv->view, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	label = gtk_entry_get_text( GTK_ENTRY( entry ));
	g_return_val_if_fail( label && g_utf8_strlen( label, -1 ), FALSE );

	deb = get_amount( self, COL_DEBIT, row );
	cre = get_amount( self, COL_CREDIT, row );

	return( ofo_entry_new_with_data(
					ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )),
							&priv->deff, &priv->dope, label,
							piece, account_number,
							ofo_account_get_devise( account ),
							priv->journal,
							deb, cre ));
}
