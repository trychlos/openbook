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
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-amount.h"
#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-guided-common.h"
#include "ui/ofa-ledger-combo.h"
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
	const ofoOpeTemplate *model;

	/* data
	 */
	GDate           *last_closed_exe;	/* last closed exercice of the dossier, may be NULL */
	gchar           *ledger;
	GDate            last_closing;		/* max of closed exercice and closed ledger */
	GDate            dope;
	GDate            deff;
	gdouble          total_debits;
	gdouble          total_credits;

	/* UI
	 */
	GtkLabel        *model_label;
	ofaLedgerCombo *ledger_combo;
	GtkEntry        *dope_entry;
	GtkEntry        *deffect_entry;
	gboolean         deffect_has_focus;
	gboolean         deffect_changed_while_focus;
	GtkGrid         *entries_grid;		/* entries view container */
	gint             entries_count;
	GtkWidget       *comment;
	GtkButton       *ok_btn;

	/* check that on_entry_changed is not recursively called */
	gint             on_changed_count;

	/* keep trace of current row/column so that we do not recompute
	 * the currently modified entry (only for debit and credit) */
	gint             focused_row;
	gint             focused_column;
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
 * definition of the columns
 */
typedef struct {
	gint	        column_id;
	gchar          *letter;
	const gchar * (*get_label)( const ofoOpeTemplate *, gint );
	gboolean      (*is_locked)( const ofoOpeTemplate *, gint );
	gint            width;
	float           xalign;					/* managed by myEditableAmout if 'is_double' */
	gboolean        expand;
	gboolean        is_double;				/* managed by myEditableAmount */
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
				ofo_ope_template_get_detail_account,
				ofo_ope_template_get_detail_account_locked, 10,            0, FALSE, FALSE },
		{ COL_ACCOUNT_SELECT,
				NULL,
				NULL,
				NULL, 0, 0, FALSE },
		{ COL_LABEL,
				"L",
				ofo_ope_template_get_detail_label,
				ofo_ope_template_get_detail_label_locked,   20,            0, TRUE,  FALSE },
		{ COL_DEBIT,
				"D",
				ofo_ope_template_get_detail_debit,
				ofo_ope_template_get_detail_debit_locked,   AMOUNTS_WIDTH, 0, FALSE, TRUE },
		{ COL_CREDIT,
				"C",
				ofo_ope_template_get_detail_credit,
				ofo_ope_template_get_detail_credit_locked,  AMOUNTS_WIDTH, 0, FALSE, TRUE },
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

/* a structure attached to each dynamically created entry field
 */
typedef struct {
	gint              column_id;	/* counted from 1 */
	gint              row_id;		/* counted from 1 */
	const sColumnDef *col_def;
	const gchar      *formula;		/* only set if this is a formula, else %NULL */
	gboolean          locked;
	gchar            *previous;		/* initial content when focusing in */
	gboolean          modified;		/* whether an automatic field has been manually modified */
}
	sEntryData;

#define DATA_ENTRY_DATA             "data-entry-data"
#define DATA_COLUMN                 "data-column-id"
#define DATA_ROW                    "data-row-id"

static GDate st_last_dope = { 0 };
static GDate st_last_deff = { 0 };

G_DEFINE_TYPE( ofaGuidedCommon, ofa_guided_common, G_TYPE_OBJECT )

static void              setup_from_dossier( ofaGuidedCommon *self );
static void              setup_ledger_combo( ofaGuidedCommon *self );
static void              setup_dates( ofaGuidedCommon *self );
static void              setup_misc( ofaGuidedCommon *self );
static void              init_ledger_combo( ofaGuidedCommon *self );
static void              setup_model_data( ofaGuidedCommon *self );
static void              setup_entries_grid( ofaGuidedCommon *self );
static void              add_entry_row( ofaGuidedCommon *self, gint i );
static void              add_entry_row_set( ofaGuidedCommon *self, gint col_id, gint row );
static void              add_entry_row_button( ofaGuidedCommon *self, const gchar *stock_id, gint column, gint row );
static void              remove_entry_row( ofaGuidedCommon *self, gint row );
static void              on_ledger_changed( const gchar *mnemo, ofaGuidedCommon *self );
static gboolean          on_dope_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static gboolean          on_dope_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static void              on_dope_changed( GtkEntry *entry, ofaGuidedCommon *self );
static gboolean          on_deffect_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static gboolean          on_deffect_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self );
static void              on_deffect_changed( GtkEntry *entry, ofaGuidedCommon *self );
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
static gboolean          check_for_ledger( ofaGuidedCommon *self );
static gboolean          check_for_dates( ofaGuidedCommon *self );
static gboolean          check_for_all_entries( ofaGuidedCommon *self );
static gboolean          check_for_entry( ofaGuidedCommon *self, gint row );
static gboolean          do_validate( ofaGuidedCommon *self );
static ofoEntry         *entry_from_detail( ofaGuidedCommon *self, gint row, const gchar *piece );
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

	/* free data members here */
	priv = OFA_GUIDED_COMMON( instance )->priv;
	g_free( priv->ledger );
	g_free( priv->last_closed_exe );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_common_parent_class )->finalize( instance );
}

static void
guided_common_dispose( GObject *instance )
{
	ofaGuidedCommonPrivate *priv;

	g_return_if_fail( instance && OFA_IS_GUIDED_COMMON( instance ));

	priv = OFA_GUIDED_COMMON( instance )->priv;

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_GUIDED_COMMON, ofaGuidedCommonPrivate );

	self->priv->dispose_has_run = FALSE;
	my_date_clear( &self->priv->last_closing );
	my_date_clear( &self->priv->deff );
	my_date_clear( &self->priv->dope );
	self->priv->deffect_changed_while_focus = FALSE;
	self->priv->entries_count = 0;
}

static void
ofa_guided_common_class_init( ofaGuidedCommonClass *klass )
{
	static const gchar *thisfn = "ofa_guided_common_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_common_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_common_finalize;

	g_type_class_add_private( klass, sizeof( ofaGuidedCommonPrivate ));

	my_date_clear( &st_last_dope );
	my_date_clear( &st_last_deff );
}

static void
on_entry_weak_notify( sEntryData *sdata, GObject *entry_was_here )
{
	static const gchar *thisfn = "ofa_guided_common_on_entry_weak_notify";

	g_debug( "%s: sdata=%p, entry=%p", thisfn, ( void * ) sdata, ( void * ) entry_was_here );

	g_free( sdata->previous );
	g_free( sdata );
}

/**
 * ofa_guided_common_new:
 * @main_window: the main window of the application.
 *
 * Update the properties of an ledger
 */
ofaGuidedCommon *
ofa_guided_common_new( ofaMainWindow *main_window, GtkContainer *parent )
{
	ofaGuidedCommon *self;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	self = g_object_new( OFA_TYPE_GUIDED_COMMON, NULL );

	self->priv->main_window = main_window;
	self->priv->parent = parent;
	self->priv->dossier = ofa_main_window_get_dossier( main_window );

	setup_from_dossier( self );
	setup_ledger_combo( self );
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

	priv = self->priv;

	priv->last_closed_exe = ofo_dossier_get_last_closed_exercice( priv->dossier );
	g_debug( "ofa_guided_common_setup_from_dossier: last_closed_exe=%p", ( void * ) priv->last_closed_exe );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), self );

	g_signal_connect(
			G_OBJECT( priv->dossier ),
			OFA_SIGNAL_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), self );
}

static void
setup_ledger_combo( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	ofaLedgerComboParms parms;

	priv = self->priv;

	parms.container = priv->parent;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-ledger";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaLedgerComboCb ) on_ledger_changed;
	parms.user_data = self;
	parms.initial_mnemo = priv->ledger;

	priv->ledger_combo = ofa_ledger_combo_new( &parms );
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

	priv = self->priv;

	priv->dope_entry = GTK_ENTRY( my_utils_container_get_child_by_name( priv->parent, "p1-dope" ));
	my_editable_date_init( GTK_EDITABLE( priv->dope_entry ));
	my_date_set_from_date( &priv->dope, &st_last_dope );
	my_editable_date_set_date( GTK_EDITABLE( priv->dope_entry ), &priv->dope );

	g_signal_connect(
			G_OBJECT( priv->dope_entry ), "focus-in-event", G_CALLBACK( on_dope_focus_in ), self );
	g_signal_connect(
			G_OBJECT( priv->dope_entry ), "focus-out-event", G_CALLBACK( on_dope_focus_out ), self );
	g_signal_connect(
			G_OBJECT( priv->dope_entry ), "changed", G_CALLBACK( on_dope_changed ), self );

	priv->deffect_entry = GTK_ENTRY( my_utils_container_get_child_by_name( priv->parent, "p1-deffet" ));
	my_editable_date_init( GTK_EDITABLE( priv->deffect_entry ));
	my_date_set_from_date( &priv->deff, &st_last_deff );
	my_editable_date_set_date( GTK_EDITABLE( priv->deffect_entry ), &priv->deff );

	g_signal_connect(
			G_OBJECT( priv->deffect_entry ), "focus-in-event", G_CALLBACK( on_deffect_focus_in ), self );
	g_signal_connect(
			G_OBJECT( priv->deffect_entry ), "focus-out-event", G_CALLBACK( on_deffect_focus_out ), self );
	g_signal_connect(
			G_OBJECT( priv->deffect_entry ), "changed", G_CALLBACK( on_deffect_changed ), self );
}

static void
setup_misc( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	GtkWidget *widget, *view;

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( priv->parent, "p1-model-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->model_label = GTK_LABEL( widget );

	widget = my_utils_container_get_child_by_name( priv->parent, "p1-comment" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	priv->comment = widget;

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
ofa_guided_common_set_model( ofaGuidedCommon *self, const ofoOpeTemplate *model )
{
	ofaGuidedCommonPrivate *priv;
	gint i;

	g_return_if_fail( self && OFA_IS_GUIDED_COMMON( self ));
	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));

	priv = self->priv;

	if( !priv->dispose_has_run ){

		for( i=0 ; i<priv->entries_count ; ++i ){
			remove_entry_row( self, i );
		}

		priv->model = model;
		priv->entries_count = 0;

		init_ledger_combo( self );
		setup_model_data( self );
		setup_entries_grid( self );

		gtk_widget_show_all( GTK_WIDGET( priv->parent ));
		check_for_enable_dlg( self );
	}
}

static void
init_ledger_combo( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	GtkWidget *combo;

	priv = self->priv;
	priv->ledger = g_strdup( ofo_ope_template_get_ledger( priv->model ));

	ofa_ledger_combo_set_selection( priv->ledger_combo, priv->ledger );

	combo = my_utils_container_get_child_by_name( priv->parent, "p1-ledger" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_widget_set_sensitive( combo, !ofo_ope_template_get_ledger_locked( priv->model ));
}

static void
setup_model_data( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;

	priv = self->priv;
	gtk_label_set_text( priv->model_label, ofo_ope_template_get_label( priv->model ));
}

static void
setup_entries_grid( ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	gint count,i;
	GtkLabel *label;
	GtkEntry *entry;

	priv = self->priv;

	count = ofo_ope_template_get_detail_count( priv->model );
	for( i=0 ; i<count ; ++i ){
		add_entry_row( self, i );
	}

	label = GTK_LABEL( gtk_label_new( _( "Total :" )));
	gtk_widget_set_margin_top( GTK_WIDGET( label ), TOTAUX_TOP_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( label ), COL_LABEL, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_amount_init( GTK_EDITABLE( entry ));
	gtk_widget_set_can_focus( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_DEBIT, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_amount_init( GTK_EDITABLE( entry ));
	gtk_widget_set_can_focus( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_CREDIT, count+1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Diff :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( label ), COL_LABEL, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_amount_init( GTK_EDITABLE( entry ));
	gtk_widget_set_can_focus( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_DEBIT, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_amount_init( GTK_EDITABLE( entry ));
	gtk_widget_set_can_focus( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, GTK_WIDGET( entry ), COL_CREDIT, count+2, 1, 1 );

	priv->entries_count = count+2;
}

static void
add_entry_row( ofaGuidedCommon *self, gint i )
{
	GtkWidget *label;
	gchar *str;

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", i+1 );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_margin_right( label, 4 );
	gtk_widget_set_margin_bottom( label, 2 );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_label_set_width_chars( GTK_LABEL( label ), RANG_WIDTH );
	gtk_grid_attach( self->priv->entries_grid, label, COL_RANG, i+1, 1, 1 );

	/* other columns starting with COL_ACCOUNT=1 */
	add_entry_row_set( self, COL_ACCOUNT, i+1 );
	add_entry_row_button( self, "gtk-index", COL_ACCOUNT_SELECT, i+1 );
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
	sEntryData *sdata;

	col_def = find_column_def_from_col_id( self, col_id );
	g_return_if_fail( col_def );

	str = (*col_def->get_label)( self->priv->model, row-1 );
	locked = (*col_def->is_locked)( self->priv->model, row-1 );

	/* only create the entry if the field is not empty or not locked
	 * (because an empty locked field will obviously never be set)
	 */
	if(( !str || !g_utf8_strlen( str, -1 )) && locked ){
		return;
	}

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), col_def->expand );
	gtk_entry_set_width_chars( entry, col_def->width );

	if( col_def->is_double ){
		my_editable_amount_init( GTK_EDITABLE( entry ));
	} else {
		gtk_entry_set_alignment( entry, col_def->xalign );
	}

	if( str && !ofo_ope_template_detail_is_formula( str )){
		if( col_def->is_double ){
			my_editable_amount_set_string( GTK_EDITABLE( entry ), str );
		} else {
			gtk_entry_set_text( entry, str );
		}
	}

	gtk_widget_set_sensitive( GTK_WIDGET( entry ), !locked );

	sdata = g_new0( sEntryData, 1 );
	sdata->column_id = col_id;
	sdata->row_id = row;
	sdata->col_def = col_def;
	sdata->formula = str && ofo_ope_template_detail_is_formula( str ) ? str : NULL;
	sdata->locked = locked;
	sdata->previous = NULL;
	sdata->modified = FALSE;

	g_object_set_data( G_OBJECT( entry ), DATA_ENTRY_DATA, sdata );
	g_object_weak_ref( G_OBJECT( entry ), ( GWeakNotify ) on_entry_weak_notify, sdata );

	if( !locked ){
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_entry_changed ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_entry_focus_in ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_entry_focus_out ), self );
		g_signal_connect( G_OBJECT( entry ), "key-press-event", G_CALLBACK( on_key_pressed ), self );
	}

	gtk_grid_attach( self->priv->entries_grid, GTK_WIDGET( entry ), col_id, row, 1, 1 );
}

static void
add_entry_row_button( ofaGuidedCommon *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->priv->entries_grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static void
remove_entry_row( ofaGuidedCommon *self, gint row )
{
	gint i;
	GtkWidget *widget;

	for( i=0 ; i<N_COLUMNS ; ++i ){
		widget = gtk_grid_get_child_at( self->priv->entries_grid, i, row );
		if( widget ){
			gtk_widget_destroy( widget );
		}
	}
}

/*
 * ofaLedgerCombo callback
 *
 * setup the last closing date as the maximum of :
 * - the last exercice closing date
 * - the last ledger closing date
 *
 * this last closing date is the lower limit of the effect dates
 */
static void
on_ledger_changed( const gchar *mnemo, ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;
	ofoLedger *ledger;
	gint exe_id;
	const GDate *date;

	priv = self->priv;

	g_free( priv->ledger );
	priv->ledger = g_strdup( mnemo );

	ledger = ofo_ledger_get_by_mnemo( priv->dossier, mnemo );
	my_date_set_from_date( &priv->last_closing, priv->last_closed_exe );

	if( ledger ){
		exe_id = ofo_dossier_get_current_exe_id( priv->dossier );
		date = ofo_ledger_get_exe_closing( ledger, exe_id );
		if( my_date_is_valid( date )){
			if( my_date_is_valid( priv->last_closed_exe )){
				if( my_date_compare( date, priv->last_closed_exe ) > 0 ){
					my_date_set_from_date( &priv->last_closing, date );
				}
			} else {
				my_date_set_from_date( &priv->last_closing, date );
			}
		}
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
	set_date_comment( self, _( "Operation date" ), &self->priv->dope );

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
on_dope_changed( GtkEntry *entry, ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;

	priv = self->priv;

	/* check the operation date */
	my_date_set_from_date( &priv->dope,
			my_editable_date_get_date( GTK_EDITABLE( priv->dope_entry ), NULL ));

	set_date_comment( self, _( "Operation date" ), &priv->dope );

	/* setup the effect date if it has not been manually changed */
	if( my_date_is_valid( &priv->dope ) && !priv->deffect_changed_while_focus ){

		if( my_date_is_valid( &priv->last_closing ) &&
			my_date_compare( &priv->last_closing, &priv->dope ) > 0 ){

			my_date_set_from_date( &priv->deff, &priv->last_closing );
			g_date_add_days( &priv->deff, 1 );

		} else {
			my_date_set_from_date( &priv->deff, &priv->dope );
		}

		my_editable_date_set_date( GTK_EDITABLE( priv->deffect_entry ), &priv->deff );
	}

	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffect_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
{
	self->priv->deffect_has_focus = TRUE;
	set_date_comment( self, _( "Effect date" ), &self->priv->deff );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffect_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
{
	self->priv->deffect_has_focus = FALSE;
	set_comment( self, "" );

	return( FALSE );
}

static void
on_deffect_changed( GtkEntry *entry, ofaGuidedCommon *self )
{
	ofaGuidedCommonPrivate *priv;

	priv = self->priv;

	if( priv->deffect_has_focus ){

		my_date_set_from_date( &priv->deff,
				my_editable_date_get_date( GTK_EDITABLE( priv->deffect_entry ), NULL ));

		priv->deffect_changed_while_focus = TRUE;
		set_date_comment( self, _( "Effect date" ), &priv->deff );

		check_for_enable_dlg( self );
	}
}

/*
 * any of the GtkEntry field of an entry row has changed -> recheck all
 * but:
 * - do not recursively recheck all the field because we have modified
 *   an automatic field
 *
 * keep trace of manual modifications of automatic fields, so that we
 * then block all next automatic recomputes
 */
static void
on_entry_changed( GtkEntry *entry, ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_on_entry_changed";
	ofaGuidedCommonPrivate *priv;

	priv = self->priv;

	g_debug( "%s: entry=%p, row=%u, column=%u, on_changed_count=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column, priv->on_changed_count );

	priv->on_changed_count += 1;

	if( priv->on_changed_count == 1 &&
			priv->focused_row != 0 && priv->focused_column != 0 ){

		check_for_enable_dlg( self );
	}

	priv->on_changed_count -= 1;
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_on_entry_focus_in";
	ofaGuidedCommonPrivate *priv;
	sEntryData *sdata;
	const gchar *comment;

	priv = self->priv;
	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	priv->on_changed_count = 0;
	priv->focused_row = sdata->row_id;
	priv->focused_column = sdata->column_id;

	g_debug( "%s: entry=%p, row=%u, column=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column );

	/* get a copy of the current content */
	if( sdata->formula ){
		g_free( sdata->previous );
		sdata->previous = g_strdup( gtk_entry_get_text( entry ));
	}

	if( priv->focused_row > 0 ){
		comment = ofo_ope_template_get_detail_comment( priv->model, priv->focused_row-1 );
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
	static const gchar *thisfn = "ofa_guided_common_on_entry_focus_out";
	ofaGuidedCommonPrivate *priv;
	sEntryData *sdata;
	const gchar *current;

	priv = self->priv;
	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	g_debug( "%s: entry=%p, row=%u, column=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column );

	/* compare the current content with the saved copy of the initial one */
	if( sdata->formula ){
		current = gtk_entry_get_text( entry );
		sdata->modified = ( g_utf8_collate( sdata->previous, current ) != 0 );
	}

	/* reset focus and recursivity indicators */
	priv->on_changed_count = 0;
	priv->focused_row = 0;
	priv->focused_column = 0;

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

	entry = GTK_ENTRY( gtk_grid_get_child_at( self->priv->entries_grid, COL_ACCOUNT, row ));

	number = ofa_account_select_run( self->priv->main_window, gtk_entry_get_text( entry ));

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
						self->priv->dossier,
						asked_account );
	if( !account ){
		number = ofa_account_select_run(
							self->priv->main_window,
							asked_account );
		if( number ){
			gtk_entry_set_text( entry, number );
			g_free( number );
		}
	}
}

/*
 * setting the deffect also triggers the change signal of the deffect
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
		str = g_strdup( _( "invalid date" ));
	}
	comment = g_strdup_printf( "%s : %s", label, str );
	set_comment( self, comment );
	g_free( comment );
	g_free( str );
}

static void
set_comment( ofaGuidedCommon *self, const gchar *comment )
{
	gtk_entry_set_text( GTK_ENTRY( self->priv->comment ), comment );
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

	if( self->priv->entries_grid ){

		ok = is_dialog_validable( self );

		gtk_widget_set_sensitive( GTK_WIDGET( self->priv->ok_btn ), ok );
	}
}

/*
 * We do not re-check nor recompute any thing while just moving from a
 * field to another - this would be not only waste of time, but also
 * keep the interface changing while doing anything else that moving
 * the focus...
 */
static gboolean
is_dialog_validable( ofaGuidedCommon *self )
{
	gboolean ok;

	update_all_formulas( self );
	update_all_totals( self );

	ok = TRUE;
	ok &= check_for_ledger( self );
	ok &= check_for_dates( self );
	ok &= check_for_all_entries( self );

	return( ok );
}

/*
 * update all formulas, but the one we are currently modifying:
 * if we currently are on an entry, do not automatically modify
 * this same entry
 */
static void
update_all_formulas( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_update_all_formulas";
	ofaGuidedCommonPrivate *priv;
	sEntryData *sdata;
	gint count, idx, col_id;
	const sColumnDef *col_def;
	const gchar *str;
	GtkWidget *entry;

	priv = self->priv;

	if( priv->model ){
		count = ofo_ope_template_get_detail_count( priv->model );
		for( idx=0 ; idx<count ; ++idx ){
			for( col_id=FIRST_COLUMN ; col_id<N_COLUMNS ; ++col_id ){
				col_def = find_column_def_from_col_id( self, col_id );
				if( col_def && col_def->get_label ){
					str = ( *col_def->get_label )( priv->model, idx );
					if( ofo_ope_template_detail_is_formula( str )){
						if( idx+1 != priv->focused_row || col_id != priv->focused_column ){
							entry = gtk_grid_get_child_at( priv->entries_grid, col_id, idx+1 );
							if( entry && GTK_IS_ENTRY( entry )){
								sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );
								if( !sdata->modified ){
									update_formula( self, str, GTK_ENTRY( entry ));
								} else {
									g_debug( "%s: formula='%s' ignored as entry has been manually modified",
											thisfn, str );
								}
							}
						} else {
							g_debug( "%s: formula='%s' ignored while focus at row=%u, col_id=%u",
									thisfn, str, idx+1, col_id );
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
							"invalid formula='%s': found token='%s' while an operator was expected",
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
		my_editable_amount_set_amount( GTK_EDITABLE( entry ), solde );
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
	sEntryData *sdata;
	gdouble dsold, csold;
	gint count, idx;

	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	count = ofo_ope_template_get_detail_count( self->priv->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		if( sdata->column_id != COL_DEBIT || sdata->row_id != idx+1 ){
			dsold += get_amount( self, COL_DEBIT, idx+1 );
		}
		if( sdata->column_id != COL_CREDIT || sdata->row_id != idx+1 ){
			csold += get_amount( self, COL_CREDIT, idx+1 );
		}
	}

	return( sdata->column_id == COL_DEBIT ? csold-dsold : dsold-csold );
}

static void
formula_set_entry_idem( ofaGuidedCommon *self, GtkEntry *entry )
{
	sEntryData *sdata;
	GtkWidget *widget;

	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );
	widget = gtk_grid_get_child_at( self->priv->entries_grid, sdata->column_id, sdata->row_id-1 );
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
	const sColumnDef *col_def;
	ofoRate *rate;
	gchar *str;

	priv = self->priv;
	init = token[0];
	row = atoi( token+1 );
	amount = 0;

	if( row > 0 && row <= ofo_ope_template_get_detail_count( priv->model )){
		col_def = find_column_def_from_letter( self, init );
		if( col_def ){
			widget = gtk_grid_get_child_at( priv->entries_grid, col_def->column_id, row );
			if( widget && GTK_IS_ENTRY( widget )){
				if( col_def->is_double ){
					amount = my_editable_amount_get_amount( GTK_EDITABLE( widget ));
				} else {
					/* we do not manage a formula on a string */
					gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
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
		/*g_debug( "%s: searching for rate %s", thisfn, *iter );*/
		rate = ofo_rate_get_by_mnemo( priv->dossier, token );
		if( rate && OFO_IS_RATE( rate )){
			if( my_date_is_valid( &priv->dope )){
				amount = ofo_rate_get_rate_at_date( rate, &priv->dope )/100;
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
	gdouble dsold, csold, ddiff, cdiff;
	gint count, idx;
	GtkWidget *entry;

	priv = self->priv;

	if( priv->model ){
		count = ofo_ope_template_get_detail_count( priv->model );
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
			my_editable_amount_set_amount( GTK_EDITABLE( entry ), dsold );
		}

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_CREDIT, count+1 );
		if( entry && GTK_IS_ENTRY( entry )){
			my_editable_amount_set_amount( GTK_EDITABLE( entry ), csold );
		}

		if( dsold > csold ){
			ddiff = dsold - csold;
			cdiff = 0;
		} else if( dsold < csold ){
			ddiff = 0;
			cdiff = csold - dsold;
		} else {
			ddiff = 0;
			cdiff = 0;
		}

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_DEBIT, count+2 );
		if( entry && GTK_IS_ENTRY( entry )){
			my_editable_amount_set_amount( GTK_EDITABLE( entry ), ddiff );
		}

		entry = gtk_grid_get_child_at( priv->entries_grid, COL_CREDIT, count+2 );
		if( entry && GTK_IS_ENTRY( entry )){
			my_editable_amount_set_amount( GTK_EDITABLE( entry ), cdiff );
		}
	}
}

static gdouble
get_amount( ofaGuidedCommon *self, gint col_id, gint row )
{
	const sColumnDef *col_def;
	GtkWidget *entry;

	col_def = find_column_def_from_col_id( self, col_id );
	if( col_def ){
		entry = gtk_grid_get_child_at( self->priv->entries_grid, col_def->column_id, row );
		if( entry && GTK_IS_ENTRY( entry )){
			return( my_editable_amount_get_amount( GTK_EDITABLE( entry )));
		}
	}

	return( 0.0 );
}

/*
 * Returns TRUE if a ledger is set
 */
static gboolean
check_for_ledger( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_check_for_ledger";
	gboolean ok;

	ok = self->priv->ledger && g_utf8_strlen( self->priv->ledger, -1 );
	if( !ok ){
		g_debug( "%s: ledger=%s", thisfn, self->priv->ledger );
	}

	return( ok );
}

/*
 * Returns TRUE if the dates are set and valid
 *
 * The first valid effect date is older than:
 * - the last exercice closing date of the dossier (if set)
 * - the last closing date of the ledger (if set)
 */
static gboolean
check_for_dates( ofaGuidedCommon *self )
{
	static const gchar *thisfn = "ofa_guided_common_check_for_dates";
	ofaGuidedCommonPrivate *priv;
	gboolean ok, oki;

	ok = TRUE;
	priv = self->priv;

	oki = my_date_is_valid( &priv->dope );
	my_utils_entry_set_valid( GTK_ENTRY( priv->dope_entry ), oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: operation date is invalid", thisfn );
	}

	oki = my_date_is_valid( &priv->deff );
	my_utils_entry_set_valid( GTK_ENTRY( priv->deffect_entry ), oki );
	ok &= oki;
	if( !oki ){
		g_debug( "%s: effect date is invalid", thisfn );

	} else if( my_date_is_valid( &priv->last_closing )){
		oki = my_date_compare( &priv->last_closing, &priv->deff ) < 0;
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
	priv = self->priv;

	if( priv->model ){
		count = ofo_ope_template_get_detail_count( priv->model );

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
	priv = self->priv;

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
 * serialized. Only after that, ledger and accounts are updated.
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
	priv = self->priv;

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
 * serialized. Only after that, ledger and accounts are updated.
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
	priv = self->priv;

	piece = NULL;
	entry = my_utils_container_get_child_by_name( priv->parent, "p1-piece" );
	if( entry && GTK_IS_ENTRY( entry )){
		piece = gtk_entry_get_text( GTK_ENTRY( entry ));
	}

	count = ofo_ope_template_get_detail_count( priv->model );
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
			 * of the list, decrementing the ledgers and the accounts
			 * then restore the last ecr number of the dossier
			 */
		}
		if( ok ){
			display_ok_message( self, g_list_length( entries ));
		}
	}

	g_list_free_full( entries, g_object_unref );

	my_date_set_from_date( &st_last_dope, &priv->dope );
	my_date_set_from_date( &st_last_deff, &priv->deff );

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

	priv = self->priv;

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
					ofo_account_get_currency( account ),
					priv->ledger,
					ofo_ope_template_get_mnemo( priv->model ),
					deb, cre ));
}

static void
display_ok_message( ofaGuidedCommon *self, gint count )
{
	GtkWidget *dialog;
	gchar *message;

	message = g_strdup_printf( _( "%d entries have been succesffully created" ), count );

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( self->priv->main_window ),
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

	if( !common->priv->dispose_has_run ){

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

	for( i=1 ; i<=self->priv->entries_count ; ++i ){
		entry = gtk_grid_get_child_at( self->priv->entries_grid, COL_LABEL, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
		entry = gtk_grid_get_child_at( self->priv->entries_grid, COL_DEBIT, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
		entry = gtk_grid_get_child_at( self->priv->entries_grid, COL_CREDIT, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
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

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( OFO_OPE_TEMPLATE( object ) == self->priv->model ){
			ofa_guided_common_set_model( self, OFO_OPE_TEMPLATE( object ));
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

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( OFO_OPE_TEMPLATE( object ) == self->priv->model ){

			for( i=0 ; i < self->priv->entries_count ; ++i ){
				remove_entry_row( self, i );
			}
			self->priv->model = NULL;
			self->priv->entries_count = 0;
		}
	}
}
