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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-journal.h"
#include "api/ofo-model.h"
#include "api/ofo-taux.h"

#include "core/my-window-prot.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-guided-common.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaGuidedCommonPrivate {
	gboolean         dispose_has_run;

	/* input parameters at instanciation time
	 */
	ofaMainWindow   *main_window;
	ofoDossier      *dossier;
	GtkContainer    *parent;

	/* when selecting a model
	 */
	const ofoModel  *model;

	/* data
	 */
	GDate            last_closed_exe;	/* last closed exercice of the dossier */
	gchar           *journal;
	GDate            last_closing;		/* max of closed exercice and closed journal */
	GDate            dope;
	GDate            deff;
	gdouble          total_debits;
	gdouble          total_credits;

	/* UI
	 */
	GtkLabel        *model_label;
	ofaJournalCombo *journal_combo;
	GtkEntry        *dope_entry;
	GtkEntry        *deffet_entry;
	gboolean         deffet_has_focus;
	gboolean         deffet_changed_while_focus;
	GtkGrid         *entries_grid;					/* entries view container */
	gint             entries_count;
	GtkEntry        *comment;
	GtkButton       *ok_btn;
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
	gboolean        is_double;
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
				ofo_model_get_detail_account_locked, 10,            0, FALSE, FALSE },
		{ COL_ACCOUNT_SELECT,
				NULL,
				NULL,
				NULL, 0, 0, FALSE },
		{ COL_LABEL,
				"L",
				ofo_model_get_detail_label,
				ofo_model_get_detail_label_locked,   20,            0, TRUE,  FALSE },
		{ COL_DEBIT,
				"D",
				ofo_model_get_detail_debit,
				ofo_model_get_detail_debit_locked,   AMOUNTS_WIDTH, 1, FALSE, TRUE },
		{ COL_CREDIT,
				"C",
				ofo_model_get_detail_credit,
				ofo_model_get_detail_credit_locked,  AMOUNTS_WIDTH, 1, FALSE, TRUE },
		{ 0 }
};

/* operators in formula
 */
enum {
	OPE_MINUS = 1,
	OPE_PLUS,
	OPE_PROD,
	OPE_DIV
};

#define DATA_COLUMN				    "data-entry-left"
#define DATA_ROW				    "data-entry-top"

static GDate st_last_dope = { 0 };
static GDate st_last_deff = { 0 };

G_DEFINE_TYPE( ofaGuidedCommon, ofa_guided_common, G_TYPE_OBJECT )

static void              setup_from_dossier( ofaGuidedCommon *self );
static void              setup_journal_combo( ofaGuidedCommon *self );
static void              setup_dates( ofaGuidedCommon *self );
static void              setup_misc( ofaGuidedCommon *self );
static void              init_journal_combo( ofaGuidedCommon *self );
static void              setup_model_data( ofaGuidedCommon *self );
static void              setup_entries_grid( ofaGuidedCommon *self );
static void              add_entry_row( ofaGuidedCommon *self, gint i );
static void              add_entry_row_set( ofaGuidedCommon *self, gint col_id, gint row );
static void              add_entry_row_button( ofaGuidedCommon *self, const gchar *stock_id, gint column, gint row );
static void              remove_entry_row( ofaGuidedCommon *self, gint row );
static void              on_journal_changed( const gchar *mnemo, ofaGuidedCommon *self );
static void              on_dope_changed( GtkEntry *entry, ofaGuidedCommon *self );
static gboolean          on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static gboolean          on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static void              on_deffet_changed( GtkEntry *entry, ofaGuidedCommon *self );
static gboolean          on_deffet_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static gboolean          on_deffet_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static void              on_entry_changed( GtkEntry *entry, ofaGuidedCommon *self );
static gboolean          on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static gboolean          on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static gboolean          on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedCommon *self );
static void              on_button_clicked( GtkButton *button, ofaGuidedCommon *self );
static void              on_account_selection( ofaGuidedCommon *self, gint row );
static void              check_for_account( ofaGuidedCommon *self, GtkEntry *entry  );
static void              set_date_comment( ofaGuidedCommon *self, const gchar *label, const GDate *date );
static void              set_comment( ofaGuidedCommon *self, const gchar *comment );
static const sColumnDef *find_column_def_from_col_id( ofaGuidedCommon *self, gint col_id );
static const sColumnDef *find_column_def_from_letter( ofaGuidedCommon *self, gchar letter );
static void              check_for_enable_dlg( ofaGuidedCommon *self );
static gboolean          is_dialog_validable( ofaGuidedCommon *self );
static void              update_all_formulas( ofaGuidedCommon *self );
static void              update_formula( ofaGuidedCommon *self, const gchar *formula, GtkEntry *entry );
static gint              formula_parse_operator( ofaGuidedCommon *self, const gchar *formula, const gchar *token );
static gdouble           formula_compute_solde( ofaGuidedCommon *self, GtkEntry *entry );
static void              formula_set_entry_idem( ofaGuidedCommon *self, GtkEntry *entry );
static gdouble           formula_parse_token( ofaGuidedCommon *self, const gchar *formula, const gchar *token, GtkEntry *entry, gboolean *display );
static void              formula_error( ofaGuidedCommon *self, const gchar *str );
static void              update_all_totals( ofaGuidedCommon *self );
static gdouble           get_amount( ofaGuidedCommon *self, gint col_id, gint row );
static gboolean          check_for_journal( ofaGuidedCommon *self );
static gboolean          check_for_dates( ofaGuidedCommon *self );
static gboolean          check_for_all_entries( ofaGuidedCommon *self );
static gboolean          check_for_entry( ofaGuidedCommon *self, gint row );
static gboolean          do_validate( ofaGuidedCommon *self );
static ofoEntry          *entry_from_detail( ofaGuidedCommon *self, gint row, const gchar *piece );
static void              display_ok_message( ofaGuidedCommon *self, gint count );
static void              do_reset_entries_rows( ofaGuidedCommon *self );
static void              on_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedCommon *self );
static void              on_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedCommon *self );

static void
guided_common_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_common_finalize";
	ofaGuidedCommonPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_GUIDED_COMMON( instance ));

	priv = OFA_GUIDED_COMMON( instance )->private;

	/* free data members here */
	g_free( priv->journal );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_common_parent_class )->finalize( instance );
}

static void
guided_common_dispose( GObject *instance )
{
	ofaGuidedCommonPrivate *priv;

	g_return_if_fail( instance && OFA_IS_GUIDED_COMMON( instance ));

	priv = OFA_GUIDED_COMMON( instance )->private;

	if( !priv->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_common_parent_class )->dispose( instance );
}

static void
ofa_guided_common_init( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_GUIDED_COMMON( self ));

	self->private = g_new0( ofaGuidedCommonPrivate, 1 );

	self->private->dispose_has_run = FALSE;

	self->private->deffet_changed_while_focus = FALSE;
	self->private->entries_count = 0;
}

static void
ofa_guided_common_class_init( ofaGuidedCommonClass *klass )
{
	static const gchar *thisfn = "ofa_guided_common_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_common_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_common_finalize;
}

/**
 * ofa_guided_common_new:
 * @main_window: the main window of the application.
 *
 * Update the properties of an journal
 */
ofaGuidedCommon *
ofa_guided_common_new( ofaMainWindow *main_window, GtkContainer *parent )
{
	ofaGuidedCommon *self;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	self = g_object_new( OFA_TYPE_GUIDED_COMMON, NULL );

	self->private->main_window = main_window;
	self->private->parent = parent;
	self->private->dossier = ofa_main_window_get_dossier( main_window );

	setup_from_dossier( self );
	setup_journal_combo( self );
	setup_dates( self );
	setup_misc( self );

	return( self );
}

/*
 * datas which come from the dossier are read once
 * they are supposed to stay unchanged while the window is alive
 */
static void
setup_from_dossier( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	const GDate *date;

	priv = self->private;

	date = ofo_dossier_get_last_closed_exercice( priv->dossier );
	g_date_clear( &priv->last_closed_exe, 1 );
	if( date && g_date_valid( date )){
		memcpy( &priv->last_closed_exe, date, sizeof( GDate ));
	}

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
}

static void
setup_journal_combo( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	ofaJournalComboParms parms;

	priv = self->private;

	parms.container = priv->parent;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-journal";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaJournalComboCb ) on_journal_changed;
	parms.user_data = self;
	parms.initial_mnemo = priv->journal;

	priv->journal_combo = ofa_journal_combo_new( &parms );
}

/*
 * when opening the window, dates are set to the last used (from the
 * global static variables)
 * if the window stays alive after a validation (the case of the main
 * page), then the dates stays untouched
 */
static void
setup_dates( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	myDateParse parms;

	priv = self->private;

	my_date_set_from_date( &priv->dope, &st_last_dope );

	memset( &parms, '\0', sizeof( parms ));
	parms.entry = my_utils_container_get_child_by_name( priv->parent, "p1-dope" );
	parms.entry_format = MY_DATE_DMYY;
	parms.date = &priv->dope;
	parms.on_changed_cb = G_CALLBACK( on_dope_changed );
	parms.user_data = self;
	my_date_parse_from_entry( &parms );

	priv->dope_entry = GTK_ENTRY( parms.entry );

	g_signal_connect(
			G_OBJECT( parms.entry ), "focus-in-event", G_CALLBACK( on_dope_focus_in ), self );
	g_signal_connect(
			G_OBJECT( parms.entry ), "focus-out-event", G_CALLBACK( on_dope_focus_out ), self );

	my_date_set_from_date( &priv->deff, &st_last_deff );

	memset( &parms, '\0', sizeof( parms ));
	parms.entry = my_utils_container_get_child_by_name( priv->parent, "p1-deffet" );
	parms.entry_format = MY_DATE_DMYY;
	parms.date = &priv->deff;
	parms.on_changed_cb = G_CALLBACK( on_deffet_changed );
	parms.user_data = self;
	my_date_parse_from_entry( &parms );

	priv->deffet_entry = GTK_ENTRY( parms.entry );

	g_signal_connect(
			G_OBJECT( parms.entry ), "focus-in-event", G_CALLBACK( on_deffet_focus_in ), self );
	g_signal_connect(
			G_OBJECT( parms.entry ), "focus-out-event", G_CALLBACK( on_deffet_focus_out ), self );
}

static void
setup_misc( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	GtkWidget *widget, *view;

	priv = self->private;

	widget = my_utils_container_get_child_by_name( priv->parent, "p1-model-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->model_label = GTK_LABEL( widget );

	widget = my_utils_container_get_child_by_name( priv->parent, "p1-comment" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->comment = GTK_ENTRY( widget );

	view = my_utils_container_get_child_by_name( priv->parent, "p1-entries" );
	g_return_if_fail( view && GTK_IS_GRID( view ));
	priv->entries_grid = GTK_GRID( view );

	widget = my_utils_container_get_child_by_name( priv->parent, "box-ok" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	gtk_widget_set_sensitive( widget, FALSE );
	priv->ok_btn = GTK_BUTTON( widget );
}

/**
 * ofa_guided_common_set_model:
 */
void
ofa_guided_common_set_model( ofaGuidedCommon *self, const ofoModel *model )
{
	ofaGuidedCommonPrivate *priv;
	gint i;

	g_return_if_fail( self && OFA_IS_GUIDED_COMMON( self ));
	g_return_if_fail( model && OFO_IS_MODEL( model ));

	priv = self->private;

	if( !priv->dispose_has_run ){

		for( i=0 ; i<priv->entries_count ; ++i ){
			remove_entry_row( self, i );
		}

		priv->model = model;
		priv->entries_count = 0;

		init_journal_combo( self );
		setup_model_data( self );
		setup_entries_grid( self );

		gtk_widget_show_all( GTK_WIDGET( priv->parent ));
		check_for_enable_dlg( self );
	}
}

static void
init_journal_combo( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	GtkWidget *combo;

	priv = self->private;
	priv->journal = g_strdup( ofo_model_get_journal( priv->model ));

	ofa_journal_combo_set_selection( priv->journal_combo, priv->journal );

	combo = my_utils_container_get_child_by_name( priv->parent, "p1-journal" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_widget_set_sensitive( combo, !ofo_model_get_journal_locked( priv->model ));
}

static void
setup_model_data( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;

	priv = self->private;
	gtk_label_set_text( priv->model_label, ofo_model_get_label( priv->model ));
}

static void
setup_entries_grid( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gint count,i;
	GtkLabel *label;
	GtkEntry *entry;

	priv = self->private;

	count = ofo_model_get_detail_count( priv->model );
	for( i=0 ; i<count ; ++i ){
		add_entry_row( self, i );
	}

	label = GTK_LABEL( gtk_label_new( _( "Total :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( label ), TOTAUX_TOP_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( label ), COL_LABEL, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_DEBIT, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_CREDIT, count+1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Diff :" )));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( label ), COL_LABEL, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_DEBIT, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_CREDIT, count+2, 1, 1 );

	priv->entries_count = count+2;
}

static void
add_entry_row( ofaGuidedCommon *self, gint i )
{
	GtkEntry *entry;
	gchar *str;

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", i+1 );
	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1 );
	gtk_entry_set_text( entry, str );
	gtk_entry_set_width_chars( entry, RANG_WIDTH );
	gtk_grid_attach( self->private->entries_grid, GTK_WIDGET( entry ), COL_RANG, i+1, 1, 1 );
	g_free( str );

	/* other columns starting with COL_ACCOUNT=1 */
	add_entry_row_set( self, COL_ACCOUNT, i+1 );
	add_entry_row_button( self, GTK_STOCK_INDEX, COL_ACCOUNT_SELECT, i+1 );
	add_entry_row_set( self, COL_LABEL, i+1 );
	add_entry_row_set( self, COL_DEBIT, i+1 );
	add_entry_row_set( self, COL_CREDIT, i+1 );
}

static void
add_entry_row_set( ofaGuidedCommon *self, gint col_id, gint row )
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

	gtk_grid_attach( self->private->entries_grid, GTK_WIDGET( entry ), col_id, row, 1, 1 );
}

static void
add_entry_row_button( ofaGuidedCommon *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_stock( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->private->entries_grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static void
remove_entry_row( ofaGuidedCommon *self, gint row )
{
	gint i;
	GtkWidget *widget;

	for( i=0 ; i<N_COLUMNS ; ++i ){
		widget = gtk_grid_get_child_at( self->private->entries_grid, i, row );
		if( widget ){
			gtk_widget_destroy( widget );
		}
	}
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
on_journal_changed( const gchar *mnemo, ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	ofoJournal *journal;
	gint exe_id;
	const GDate *date;

	priv = self->private;

	g_free( priv->journal );
	priv->journal = g_strdup( mnemo );

	journal = ofo_journal_get_by_mnemo( priv->dossier, mnemo );
	memcpy( &priv->last_closing, &priv->last_closed_exe, sizeof( GDate ));

	if( journal ){
		exe_id = ofo_dossier_get_current_exe_id( priv->dossier );
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

static void
on_dope_changed( GtkEntry *entry, ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gchar *str;

	priv = self->private;

	/* check the operation date */
	set_date_comment( self, _( "Operation date" ), &priv->dope );

	/* setup the effect date if it has not been manually changed */
	if( g_date_valid( &priv->dope ) && !priv->deffet_changed_while_focus ){

		if( g_date_valid( &priv->last_closing ) &&
			g_date_compare( &priv->last_closing, &priv->dope ) > 0 ){

			my_date_set_from_date( &priv->deff, &priv->last_closing );
			g_date_add_days( &priv->deff, 1 );

		} else {
			my_date_set_from_date( &priv->deff, &priv->dope );
		}

		str = my_date_to_str( &priv->deff, MY_DATE_DMYY );
		gtk_entry_set_text( priv->deffet_entry, str );
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
on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
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
on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
{
	set_comment( self, "" );

	return( FALSE );
}

static void
on_deffet_changed( GtkEntry *entry, ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;

	priv = self->private;

	if( priv->deffet_has_focus ){

		priv->deffet_changed_while_focus = TRUE;
		set_date_comment( self, _( "Effect date" ), &priv->deff );

		check_for_enable_dlg( self );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffet_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
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
on_deffet_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
{
	self->private->deffet_has_focus = FALSE;
	set_comment( self, "" );

	return( FALSE );
}

/*
 * any of the GtkEntry field of an entry row has changed -> recheck all
 */
static void
on_entry_changed( GtkEntry *entry, ofaGuidedCommon *self )
{
	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
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
on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
{
	set_comment( self, "" );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * We automatically open a selection dialog box for the account if we
 * are leaving the field with a Tab key while it is invalid
 *
 * Note that if we decide to open the selection dialog box, then the
 * Gtk toolkit will complain as we return too late from this function
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedCommon *self )
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
 * click on a button in an entry row
 */
static void
on_button_clicked( GtkButton *button, ofaGuidedCommon *self )
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
on_account_selection( ofaGuidedCommon *self, gint row )
{
	GtkEntry *entry;
	gchar *number;

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->private->entries_grid, COL_ACCOUNT, row ));

	number = ofa_account_select_run( self->private->main_window, gtk_entry_get_text( entry ));

	if( number && g_utf8_strlen( number, -1 )){
		gtk_entry_set_text( entry, number );
	}

	g_free( number );
}

/*
 * check that the account exists
 * else open a dialog for selection
 *
 * Note that the existancy of the account doesn't mean the account is
 * valid - e.g. a root account is not allowed here
 */
static void
check_for_account( ofaGuidedCommon *self, GtkEntry *entry  )
{
	ofoAccount *account;
	const gchar *asked_account;
	gchar *number;

	asked_account = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number(
						self->private->dossier,
						asked_account );
	if( !account ){
		number = ofa_account_select_run(
							self->private->main_window,
							asked_account );
		if( number ){
			gtk_entry_set_text( entry, number );
			g_free( number );
		}
	}
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
set_date_comment( ofaGuidedCommon *self, const gchar *label, const GDate *date )
{
	gchar *str, *comment;

	str = my_date_to_str( date, MY_DATE_DMMM );
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
set_comment( ofaGuidedCommon *self, const gchar *comment )
{
	gtk_entry_set_text( self->private->comment, comment );
}

static const sColumnDef *
find_column_def_from_col_id( ofaGuidedCommon *self, gint col_id )
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
find_column_def_from_letter( ofaGuidedCommon *self, gchar letter )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].letter && st_col_defs[i].letter[0] == letter ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

/*
 * this is called after each field changes
 * so a good place to handle all modifications
 *
 * Note that we control *all* fields so that we are able to visually
 * highlight the erroneous ones
 */
static void
check_for_enable_dlg( ofaGuidedCommon *self )
{
	gboolean ok;

	if( self->private->entries_grid ){

		ok = is_dialog_validable( self );

		gtk_widget_set_sensitive( GTK_WIDGET( self->private->ok_btn ), ok );
	}
}

static gboolean
is_dialog_validable( ofaGuidedCommon *self )
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
update_all_formulas( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gint count, idx, col_id;
	const sColumnDef *col_def;
	const gchar *str;
	GtkWidget *entry;

	priv = self->private;

	if( priv->model ){
		count = ofo_model_get_detail_count( priv->model );
		for( idx=0 ; idx<count ; ++idx ){
			for( col_id=FIRST_COLUMN ; col_id<N_COLUMNS ; ++col_id ){
				col_def = find_column_def_from_col_id( self, col_id );
				if( col_def && col_def->get_label ){
					str = ( *col_def->get_label )( priv->model, idx );
					if( ofo_model_detail_is_formula( str )){
						entry = gtk_grid_get_child_at( priv->entries_grid, col_id, idx+1 );
						if( entry && GTK_IS_ENTRY( entry )){
							update_formula( self, str, GTK_ENTRY( entry ));
						}
					}
				}
			}
		}
	}
}

/*
 * a formula is something like '=[operator]<token><operator><token>...'
 * i.e. an equal sign '=', followed by a list of pairs '<operator><token>'
 * apart maybe the first operator which defaults to '+'
 *
 * operators are '-', '+', '*' and '/'
 *
 * tokens are:
 * - [ALDC]<row_number>
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
static void
update_formula( ofaGuidedCommon *self, const gchar *formula, GtkEntry *entry )
{
	static const gchar *thisfn = "ofa_guided_common_update_formula";
	gchar **tokens, **iter;
	gdouble solde, solde_iter;
	gchar *str;
	gboolean expect_operator;
	gboolean first_iter;
	gboolean display_solde;
	gint operator;

	g_debug( "%s: formula='%s'", thisfn, formula );

	tokens = g_regex_split_simple( "([-+*/])", formula+1, 0, 0 );
	iter = tokens;
	solde = 0;
	first_iter = TRUE;
	display_solde = TRUE;
	expect_operator = TRUE;
	operator = 0;

	while( *iter ){
		if( expect_operator ){
			operator = formula_parse_operator( self, formula, *iter );
			if( !operator ){
				if( first_iter ){
					operator = OPE_PLUS;
					expect_operator = FALSE;
				} else {
					str = g_strdup_printf(
							"invalid formula='%s': found token='%s' while operator expected",
							formula, *iter );
					formula_error( self, str );
					g_free( str );
					break;
				}
			}
		}
		if( !expect_operator ){
			if( !g_utf8_collate( *iter, "SOLDE" )){
				solde_iter = formula_compute_solde( self, entry );

			} else if( !g_utf8_collate( *iter, "IDEM" )){
				/* to be used only to duplicate a line - not really as a formula */
				formula_set_entry_idem( self, entry );
				display_solde = FALSE;
				break;

			/* have a token D1, L2 or so, or a rate mnemonic */
			} else {
				solde_iter = formula_parse_token( self, formula, *iter, entry, &display_solde );
			}
			switch( operator ){
				case OPE_MINUS:
					solde -= solde_iter;
					break;
				case OPE_PLUS:
					solde += solde_iter;
					break;
				case OPE_PROD:
					solde *= solde_iter;
					break;
				case OPE_DIV:
					solde /= solde_iter;
					break;
			}
			solde_iter = 0;
		}
		first_iter = FALSE;
		expect_operator = !expect_operator;
		iter++;
	}

	g_strfreev( tokens );

	if( display_solde ){
		/* do not use a funny display here as this string will be parsed later */
		str = g_strdup_printf( "%.2lf", solde );
		gtk_entry_set_text( entry, str );
		g_free( str );
	}
}

static gint
formula_parse_operator( ofaGuidedCommon *self, const gchar *formula, const gchar *token )
{
	gint oper;

	oper = 0;

	if( !g_utf8_collate( token, "-" )){
		oper = OPE_MINUS;
	} else if( !g_utf8_collate( token, "+" )){
		oper = OPE_PLUS;
	} else if( !g_utf8_collate( token, "*" )){
		oper = OPE_PROD;
	} else if( !g_utf8_collate( token, "/" )){
		oper = OPE_DIV;
	}

	return( oper );
}

static gdouble
formula_compute_solde( ofaGuidedCommon *self, GtkEntry *entry )
{
	gint col, row;
	gdouble dsold, csold;
	gint count, idx;

	col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		if( col != COL_DEBIT || row != idx+1 ){
			dsold += get_amount( self, COL_DEBIT, idx+1 );
		}
		if( col != COL_CREDIT || row != idx+1 ){
			csold += get_amount( self, COL_CREDIT, idx+1 );
		}
	}

	return( col == COL_DEBIT ? csold-dsold : dsold-csold );
}

static void
formula_set_entry_idem( ofaGuidedCommon *self, GtkEntry *entry )
{
	gint col, row;
	GtkWidget *widget;

	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ROW ));
	col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_COLUMN ));
	widget = gtk_grid_get_child_at( self->private->entries_grid, col, row-1 );
	if( widget && GTK_IS_ENTRY( widget )){
		gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
	}
}

static gdouble
formula_parse_token( ofaGuidedCommon *self, const gchar *formula, const gchar *token, GtkEntry *entry, gboolean *display )
{
	ofaGuidedCommonPrivate *priv;
	gchar init;
	gint row;
	GtkWidget *widget;
	gdouble amount;
	const gchar *content;
	const sColumnDef *col_def;
	ofoTaux *rate;
	gchar *str;

	priv = self->private;
	init = token[0];
	row = atoi( token+1 );
	amount = 0;

	if( row > 0 && row <= ofo_model_get_detail_count( priv->model )){
		col_def = find_column_def_from_letter( self, init );
		if( col_def ){
			widget = gtk_grid_get_child_at( priv->entries_grid, col_def->column_id, row );
			if( widget && GTK_IS_ENTRY( widget )){
				content = gtk_entry_get_text( GTK_ENTRY( widget ));
				/*g_debug( "token='%s' content='%s'", token, content );*/
				if( col_def->is_double ){
					amount = my_double_from_string( content );
				} else {
					/* we do not manage a formula on a string */
					gtk_entry_set_text( entry, content );
					*display = FALSE;
					/*g_debug( "setting display to FALSE" );*/
				}
			} else {
				str = g_strdup_printf( "no entry found at col=%d, row=%d",col_def->column_id, row );
				formula_error( self, str );
				g_free( str );
			}
		} else {
			str = g_strdup_printf( "no column definition found for '%c' letter", init );
			formula_error( self, str );
			g_free( str );
		}

	} else {
		/*g_debug( "%s: searching for taux %s", thisfn, *iter );*/
		rate = ofo_taux_get_by_mnemo( priv->dossier, token );
		if( rate && OFO_IS_TAUX( rate )){
			if( g_date_valid( &priv->deff )){
				amount = ofo_taux_get_rate_at_date( rate, &priv->deff )/100;
			}
		} else {
			str = g_strdup_printf( "rate not found: '%s'", token );
			formula_error( self, str );
			g_free( str );
		}
	}

	return( amount );
}

static void
formula_error( ofaGuidedCommon *self, const gchar *str )
{
	set_comment( self, str );
	g_warning( "ofa_guided_common_formula_error: %s", str );
}

/*
 * totals and diffs are set at rows (count+1) and (count+2) respectively
 */
static void
update_all_totals( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gdouble dsold, csold;
	gint count, idx;
	GtkWidget *entry;
	gchar *str, *str2;

	priv = self->private;

	if( priv->model ){
		count = ofo_model_get_detail_count( priv->model );
		dsold = 0.0;
		csold = 0.0;
		for( idx=0 ; idx<count ; ++idx ){
			dsold += get_amount( self, COL_DEBIT, idx+1 );
			csold += get_amount( self, COL_CREDIT, idx+1 );
		}

		priv->total_debits = dsold;
		priv->total_credits = csold;

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_DEBIT, count+1 );
		if( entry && GTK_IS_ENTRY( entry )){
			str = g_strdup_printf( "%'.2lf", dsold );
			gtk_entry_set_text( GTK_ENTRY( entry ), str );
			g_free( str );
		}

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_CREDIT, count+1 );
		if( entry && GTK_IS_ENTRY( entry )){
			str = g_strdup_printf( "%'.2lf", csold );
			gtk_entry_set_text( GTK_ENTRY( entry ), str );
			g_free( str );
		}

		if( dsold > csold ){
			str = g_strdup_printf( "%'.2lf", dsold-csold );
			str2 = g_strdup( "" );
		} else if( dsold < csold ){
			str = g_strdup( "" );
			str2 = g_strdup_printf( "%'.2lf", csold-dsold );
		} else {
			str = g_strdup( "" );
			str2 = g_strdup( "" );
		}

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_DEBIT, count+2 );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), str2 );
		}

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_CREDIT, count+2 );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), str );
		}

		g_free( str );
		g_free( str2 );
	}
}

static gdouble
get_amount( ofaGuidedCommon *self, gint col_id, gint row )
{
	const sColumnDef *col_def;
	GtkWidget *entry;

	col_def = find_column_def_from_col_id( self, col_id );
	if( col_def ){
		entry = gtk_grid_get_child_at( self->private->entries_grid, col_def->column_id, row );
		if( entry && GTK_IS_ENTRY( entry )){
			return( my_double_from_string( gtk_entry_get_text( GTK_ENTRY( entry ))));
		}
	}

	return( 0.0 );
}

/*
 * Returns TRUE if a journal is set
 */
static gboolean
check_for_journal( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_check_for_journal";
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
check_for_dates( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_check_for_dates";
	gboolean ok, oki;

	ok = TRUE;

	oki = g_date_valid( &self->private->dope );
	my_utils_entry_set_valid( GTK_ENTRY( self->private->dope_entry ), oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: operation date is invalid", thisfn );
	}

	oki = g_date_valid( &self->private->deff );
	my_utils_entry_set_valid( GTK_ENTRY( self->private->deffet_entry ), oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: effect date is invalid", thisfn );

	} else if( g_date_valid( &self->private->last_closing )){
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
check_for_all_entries( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_check_for_all_entries";
	ofaGuidedCommonPrivate *priv;
	gboolean ok, oki;
	gint count, idx;
	gdouble deb, cred;

	ok = TRUE;
	priv = self->private;

	if( priv->model ){
		count = ofo_model_get_detail_count( priv->model );

		for( idx=0 ; idx<count ; ++idx ){
			deb = get_amount( self, COL_DEBIT, idx+1 );
			cred = get_amount( self, COL_CREDIT, idx+1 );
			if( deb+cred != 0.0 ){
				if( !check_for_entry( self, idx+1 )){
					ok = FALSE;
				}
			}
		}

		oki = priv->total_debits == priv->total_credits;
		ok &= oki;
		if( !oki ){
			g_debug( "%s: totals are not equal: debits=%2.lf, credits=%.2lf",
					thisfn, priv->total_debits, priv->total_credits );
		}

		oki= (priv->total_debits != 0.0 || priv->total_credits != 0.0 );
		ok &= oki;
		if( !oki ){
			g_debug( "%s: one total is nul: debits=%2.lf, credits=%.2lf",
					thisfn, priv->total_debits, priv->total_credits );
		}
	}

	return( ok );
}

static gboolean
check_for_entry( ofaGuidedCommon *self, gint row )
{
	static const gchar *thisfn = "ofa_guided_common_check_for_entry";
	ofaGuidedCommonPrivate *priv;
	gboolean ok, oki;
	GtkWidget *entry;
	ofoAccount *account;
	const gchar *str;

	ok = TRUE;
	priv = self->private;

	entry = gtk_grid_get_child_at( priv->entries_grid, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

	account = ofo_account_get_by_number(
						priv->dossier,
						gtk_entry_get_text( GTK_ENTRY( entry )));
	oki = ( account && OFO_IS_ACCOUNT( account ) && !ofo_account_is_root( account ));
	ok &= oki;
	if( !oki ){
		g_debug( "%s: invalid or unsuitable account number=%s",
				thisfn, gtk_entry_get_text( GTK_ENTRY( entry )));
	}

	entry = gtk_grid_get_child_at( priv->entries_grid, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	str = gtk_entry_get_text( GTK_ENTRY( entry ));
	oki = ( str && g_utf8_strlen( str, -1 ));
	ok &= oki;
	if( !oki ){
		g_debug( "%s: label=%s", thisfn, gtk_entry_get_text( GTK_ENTRY( entry )));
	}

	return( ok );
}

/**
 * ofa_guided_common_validate:
 *
 * Generate the entries.
 * All the entries are created in memory and checked before being
 * serialized. Only after that, journal and accounts are updated.
 *
 * Returns %TRUE if ok.
 */
gboolean
ofa_guided_common_validate( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( self && OFA_IS_GUIDED_COMMON( self ), FALSE );
	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	ok = FALSE;
	priv = self->private;

	if( !priv->dispose_has_run ){

		if( do_validate( self )){
			ok = TRUE;
			do_reset_entries_rows( self );
		}
	}

	return( ok );
}

/**
 * ofa_guided_common_validate:
 *
 * Generate the entries.
 * All the entries are created in memory and checked before being
 * serialized. Only after that, journal and accounts are updated.
 *
 * Returns %TRUE if ok.
 */
static gboolean
do_validate( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gboolean ok;
	GtkWidget *entry;
	gint count, idx;
	gdouble deb, cred;
	const gchar *piece;
	GList *entries, *it;
	ofoEntry *record;
	gint errors;

	ok = FALSE;
	priv = self->private;

	piece = NULL;
	entry = my_utils_container_get_child_by_name( priv->parent, "p1-piece" );
	if( entry && GTK_IS_ENTRY( entry )){
		piece = gtk_entry_get_text( GTK_ENTRY( entry ));
	}

	count = ofo_model_get_detail_count( priv->model );
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
		for( it=entries ; it ; it=it->next ){
			ok &= ofo_entry_insert( OFO_ENTRY( it->data ), priv->dossier );
			/* TODO:
			 * in case of an error, remove the already recorded entries
			 * of the list, decrementing the journals and the accounts
			 * then restore the last ecr number of the dossier
			 */
		}
		if( ok ){
			display_ok_message( self, g_list_length( entries ));
		}
	}

	g_list_free_full( entries, g_object_unref );

	memcpy( &st_last_dope, &priv->dope, sizeof( GDate ));
	memcpy( &st_last_deff, &priv->deff, sizeof( GDate ));

	return( ok );
}

/*
 * create an entry in memory, adding it to the list
 */
static ofoEntry *
entry_from_detail( ofaGuidedCommon *self, gint row, const gchar *piece )
{
	ofaGuidedCommonPrivate *priv;
	GtkWidget *entry;
	const gchar *account_number;
	ofoAccount *account;
	const gchar *label;
	gdouble deb, cre;

	priv = self->private;

	entry = gtk_grid_get_child_at( priv->entries_grid, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	account_number = gtk_entry_get_text( GTK_ENTRY( entry ));
	account = ofo_account_get_by_number( priv->dossier, account_number );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	entry = gtk_grid_get_child_at( priv->entries_grid, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	label = gtk_entry_get_text( GTK_ENTRY( entry ));
	g_return_val_if_fail( label && g_utf8_strlen( label, -1 ), FALSE );

	deb = get_amount( self, COL_DEBIT, row );
	cre = get_amount( self, COL_CREDIT, row );

	return( ofo_entry_new_with_data(
					priv->dossier,
					&priv->deff, &priv->dope, label,
					piece, account_number,
					ofo_account_get_devise( account ),
					priv->journal,
					deb, cre ));
}

static void
display_ok_message( ofaGuidedCommon *self, gint count )
{
	GtkWidget *dialog;
	gchar *message;

	message = g_strdup_printf( _( "%d entries have been succesffully created" ), count );

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( self->private->main_window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"%s", message );

	g_free( message );
	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

/**
 * ofa_guided_common_reset:
 *
 * Reset the input fields, keeping the dates and the same entry model
 */
void
ofa_guided_common_reset( ofaGuidedCommon *common )
{
	g_return_if_fail( common && OFA_IS_GUIDED_COMMON( common ));

	if( !common->private->dispose_has_run ){

		do_reset_entries_rows( common );
	}
}

/*
 * nb: entries_count = count of entries + 2 (for totals and diff)
 * Only the LABEL entries may be non present on the last two lines
 */
static void
do_reset_entries_rows( ofaGuidedCommon *self )
{
	gint i;
	GtkWidget *entry;

	for( i=1 ; i<=self->private->entries_count ; ++i ){
		entry = gtk_grid_get_child_at( self->private->entries_grid, COL_LABEL, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
		entry = gtk_grid_get_child_at( self->private->entries_grid, COL_DEBIT, i );
		gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		entry = gtk_grid_get_child_at( self->private->entries_grid, COL_CREDIT, i );
		gtk_entry_set_text( GTK_ENTRY( entry ), "" );
	}
}

/*
 * OFA_SIGNAL_UPDATED_OBJECT signal handler
 */
static void
on_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_MODEL( object )){
		if( OFO_MODEL( object ) == self->private->model ){
			ofa_guided_common_set_model( self, OFO_MODEL( object ));
		}
	}
}

/*
 * OFA_SIGNAL_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_on_deleted_object";
	gint i;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_MODEL( object )){
		if( OFO_MODEL( object ) == self->private->model ){

			for( i=0 ; i < self->private->entries_count ; ++i ){
				remove_entry_row( self, i );
			}
			self->private->model = NULL;
			self->private->entries_count = 0;
		}
	}
}
