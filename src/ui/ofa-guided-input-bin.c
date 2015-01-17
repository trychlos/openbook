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
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-amount.h"
#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-guided-input-bin.h"
#include "ui/ofa-ledger-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaGuidedInputBinPrivate {
	gboolean              dispose_has_run;

	/* input parameters at initialization time
	 */
	const ofaMainWindow  *main_window;

	/* from dossier
	 */
	ofoDossier           *dossier;
	const gchar          *def_currency;
	GList                *handlers;

	/* when selecting an operation template
	 */
	const ofoOpeTemplate *model;
	ofsOpe               *ope;
	GDate                 deffect_min;			/* max of begin exercice and closed ledger +1 */

	/* UI
	 */
	GtkLabel             *model_label;
	ofaLedgerCombo       *ledger_combo;
	GtkWidget            *ledger_parent;
	GtkEntry             *dope_entry;
	GtkEntry             *deffect_entry;
	gboolean              deffect_has_focus;
	gboolean              deffect_changed_while_focus;
	GtkGrid              *entries_grid;			/* entries grid container */
	gint                  entries_count;		/* count of added entry rows */
	gint                  totals_count;			/* count of total/diff lines */
	GtkWidget            *comment;
	GtkWidget            *message;

	/* check that on_entry_changed is not recursively called */
	gint                  on_changed_count;
	gboolean              check_allowed;

	/* keep trace of current row/column so that we do not recompute
	 * the currently modified entry (only for debit and credit) */
	gint                  focused_row;
	gint                  focused_column;

	/* a list which keeps trace of used currencies
	 * one list item is created for each used currency */
	GList                *currency_list;
};

#define RANG_WIDTH                      3
#define ACCOUNT_WIDTH                  10
#define LABEL_WIDTH                    20
#define AMOUNTS_WIDTH                  10
#define CURRENCY_WIDTH                  4

#define TOTAUX_TOP_MARGIN               8

/* space between widgets in a detail line */
#define DETAIL_SPACE                    2

enum {
	TYPE_NONE = 0,
	TYPE_ENTRY,
	TYPE_LABEL,
	TYPE_BUTTON
};

/* definition of the columns
 */
typedef struct {
	gint	        column_id;
	gint            column_type;
	const gchar * (*get_label)( const ofoOpeTemplate *, gint );
	gboolean      (*is_locked)( const ofoOpeTemplate *, gint );
	gint            width;					/* entry, label */
	gboolean        is_double;				/* whether entry is managed by myEditableAmount */
	float           xalign;					/* entry not double, label */
	gboolean        expand;					/* entry not double */
	const gchar *   stock_id;				/* button */
}
	sColumnDef;

/* this works because column_id is greater than zero
 * and this is ok because the column #0 is used by the number of the row
 */
static sColumnDef st_col_defs[] = {
		{ OPE_COL_ACCOUNT,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_account,
				ofo_ope_template_get_detail_account_locked,
				ACCOUNT_WIDTH, FALSE, 0, FALSE, NULL
		},
		{ OPE_COL_ACCOUNT_SELECT,
				TYPE_BUTTON,
				NULL,
				NULL,
				0, FALSE, 0, FALSE, "gtk-index"
		},
		{ OPE_COL_LABEL,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_label,
				ofo_ope_template_get_detail_label_locked,
				LABEL_WIDTH, FALSE, 0, TRUE, NULL
		},
		{ OPE_COL_DEBIT,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_debit,
				ofo_ope_template_get_detail_debit_locked,
				AMOUNTS_WIDTH, TRUE, 0, FALSE, NULL
		},
		{ OPE_COL_CREDIT,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_credit,
				ofo_ope_template_get_detail_credit_locked,
				AMOUNTS_WIDTH, TRUE, 0, FALSE, NULL
		},
		{ OPE_COL_CURRENCY,
				TYPE_LABEL,
				NULL,
				NULL,
				CURRENCY_WIDTH, FALSE, 0, FALSE, NULL
		},
		{ 0 }
};

/* a structure attached to each dynamically created entry field
 */
typedef struct {
	gint              row_id;		/* counted from 1 */
	const sColumnDef *col_def;
	gboolean          locked;
	gchar            *previous;		/* initial content when focusing in */
	gboolean          modified;		/* whether an automatic field has been manually modified */
}
	sEntryData;

#define DATA_ENTRY_DATA                 "data-entry-data"
#define DATA_COLUMN                     "data-column-id"
#define DATA_ROW                        "data-row-id"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static GDate st_last_dope               = { 0 };
static GDate st_last_deff               = { 0 };

static const gchar *st_bin_xml        = PKGUIDIR "/ofa-guided-input-bin.ui";
static const gchar *st_bin_id         = "GuidedInputBin";

G_DEFINE_TYPE( ofaGuidedInputBin, ofa_guided_input_bin, GTK_TYPE_BIN )

static void              load_dialog( ofaGuidedInputBin *bin );
static void              setup_dialog( ofaGuidedInputBin *bin );
static void              init_model_data( ofaGuidedInputBin *bin );
static void              add_entry_row( ofaGuidedInputBin *bin, gint i );
static void              add_entry_row_widget( ofaGuidedInputBin *bin, gint col_id, gint row );
static GtkWidget        *row_widget_entry( ofaGuidedInputBin *bin, const sColumnDef *col_def, gint row );
static GtkWidget        *row_widget_label( ofaGuidedInputBin *bin, const sColumnDef *col_def, gint row );
static GtkWidget        *row_widget_button( ofaGuidedInputBin *bin, const sColumnDef *col_def, gint row );
static void              remove_entry_row( ofaGuidedInputBin *bin, gint row );
static void              on_entry_finalized( sEntryData *sdata, GObject *finalized_entry );
static void              on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaGuidedInputBin *bin );
static void              on_dope_changed( GtkEntry *entry, ofaGuidedInputBin *bin );
static gboolean          on_deffect_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin );
static gboolean          on_deffect_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin );
static void              on_deffect_changed( GtkEntry *entry, ofaGuidedInputBin *bin );
static void              on_piece_changed( GtkEditable *editable, ofaGuidedInputBin *bin );
static gboolean          on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedInputBin *bin );
static void              on_button_clicked( GtkButton *button, ofaGuidedInputBin *bin );
static void              on_account_selection( ofaGuidedInputBin *bin, gint row );
static void              check_for_account( ofaGuidedInputBin *bin, GtkEntry *entry, gint row );
static gboolean          on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin );
static gboolean          on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin );
static void              on_entry_changed( GtkEntry *entry, ofaGuidedInputBin *bin );
static const sColumnDef *find_column_def_from_col_id( const ofaGuidedInputBin *bin, gint col_id );
static void              check_for_enable_dlg( ofaGuidedInputBin *bin );
static gboolean          is_dialog_validable( ofaGuidedInputBin *bin );
static void              set_ope_to_ui( const ofaGuidedInputBin *bin, gint row, gint col_id, const gchar *content );
static void              set_comment( ofaGuidedInputBin *bin, const gchar *comment );
static void              set_message( ofaGuidedInputBin *bin, const gchar *errmsg );
static void              display_currencies( ofaGuidedInputBin *bin );
static gboolean          update_totals( ofaGuidedInputBin *bin );
static void              total_add_diff_lines( ofaGuidedInputBin *bin, gint model_count );
static void              total_display_diff( ofaGuidedInputBin *bin, const gchar *currency, gint row, gdouble ddiff, gdouble cdiff );
static gboolean          do_validate( ofaGuidedInputBin *bin );
static void              display_ok_message( ofaGuidedInputBin *bin, gint count );
static void              do_reset_entries_rows( ofaGuidedInputBin *bin );
static void              on_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedInputBin *self );
static void              on_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedInputBin *self );

static void
guided_input_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_input_bin_finalize";
	ofaGuidedInputBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT_BIN( instance ));

	/* free data members here */
	priv = OFA_GUIDED_INPUT_BIN( instance )->priv;

	if( priv->currency_list ){
		ofs_currency_list_free( &priv->currency_list );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_bin_parent_class )->finalize( instance );
}

static void
guided_input_bin_dispose( GObject *instance )
{
	ofaGuidedInputBinPrivate *priv;
	GList *iha;
	gulong handler_id;

	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT_BIN( instance ));

	priv = OFA_GUIDED_INPUT_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */

		/* note when deconnecting the handlers that the dossier may
		 * have been already finalized (e.g. when the application
		 * terminates) */
		if( priv->dossier &&
				OFO_IS_DOSSIER( priv->dossier ) && !ofo_dossier_has_dispose_run( priv->dossier )){
			for( iha=priv->handlers ; iha ; iha=iha->next ){
				handler_id = ( gulong ) iha->data;
				g_signal_handler_disconnect( priv->dossier, handler_id );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_bin_parent_class )->dispose( instance );
}

static void
ofa_guided_input_bin_init( ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_init";
	ofaGuidedInputBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_GUIDED_INPUT_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_GUIDED_INPUT_BIN, ofaGuidedInputBinPrivate );
	priv = self->priv;

	priv->dispose_has_run = FALSE;
	my_date_clear( &priv->deffect_min );
	priv->deffect_changed_while_focus = FALSE;
	priv->entries_count = 0;
	priv->currency_list = NULL;
}

static void
ofa_guided_input_bin_class_init( ofaGuidedInputBinClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_input_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_input_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaGuidedInputBinPrivate ));

	my_date_clear( &st_last_dope );
	my_date_clear( &st_last_deff );

	/**
	 * ofaGuidedInputBin::changed:
	 *
	 * This signal is sent after all the fields have been checked,
	 * reacting to a field change.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaGuidedInputBin *bin,
	 * 						gboolean           is_valid,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_GUIDED_INPUT_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_BOOLEAN );
}

/**
 * ofa_guided_input_bin_new:
 */
ofaGuidedInputBin *
ofa_guided_input_bin_new( void )
{
	ofaGuidedInputBin *self;

	self = g_object_new( OFA_TYPE_GUIDED_INPUT_BIN, NULL );

	load_dialog( self );

	return( self );
}

static void
load_dialog( ofaGuidedInputBin *bin )
{
	GtkWidget *window, *top_widget;

	window = my_utils_builder_load_from_path( st_bin_xml, st_bin_id );
	g_return_if_fail( window && GTK_IS_CONTAINER( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-widget" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));
}

/**
 * ofa_guided_input_bin_set_main_window:
 */
void
ofa_guided_input_bin_set_main_window( ofaGuidedInputBin *bin, const ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_guided_input_bin_set_main_window";
	ofaGuidedInputBinPrivate *priv;
	gulong handler;

	g_debug( "%s: bin=%p, main_window=%p", thisfn, ( void * ) bin, ( void * ) main_window );

	g_return_if_fail( bin && OFA_IS_GUIDED_INPUT_BIN( bin ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->main_window = main_window;

		/* setup from dossier
		 * datas which come from the dossier are read once
		 * they are supposed to stay unchanged while the window is alive */
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->def_currency = ofo_dossier_get_default_currency( priv->dossier );

		handler = g_signal_connect(
						G_OBJECT( priv->dossier ),
						SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), bin );
		priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

		handler = g_signal_connect(
						G_OBJECT( priv->dossier ),
						SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), bin );
		priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

		/* setup the dialog part which do not depend of the operation
		 * template */
		setup_dialog( bin );
	}
}

static void
setup_dialog( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label, *widget, *view;

	priv = bin->priv;

	/* set ledger combo */
	priv->ledger_combo = ofa_ledger_combo_new();

	priv->ledger_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-ledger-parent" );
	g_return_if_fail( priv->ledger_parent && GTK_IS_CONTAINER( priv->ledger_parent ));

	ofa_ledger_combo_attach_to( priv->ledger_combo, GTK_CONTAINER( priv->ledger_parent ));
	ofa_ledger_combo_set_columns( priv->ledger_combo, LEDGER_DISP_LABEL );
	ofa_ledger_combo_set_main_window( priv->ledger_combo, priv->main_window );

	g_signal_connect(
			G_OBJECT( priv->ledger_combo ), "ofa-changed", G_CALLBACK( on_ledger_changed ), bin );

	/* when opening the window, dates are set to the last used (from the
	 * global static variables)
	 * if the window stays alive after a validation (the case of the main
	 * page), then the dates stays untouched */
	priv->dope_entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-dope" ));
	my_editable_date_init( GTK_EDITABLE( priv->dope_entry ));
	my_editable_date_set_date( GTK_EDITABLE( priv->dope_entry ), &st_last_dope );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-dope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( priv->dope_entry ), label, MY_DATE_DMYY );

	g_signal_connect(
			G_OBJECT( priv->dope_entry ), "changed", G_CALLBACK( on_dope_changed ), bin );

	priv->deffect_entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-deffect" ));
	my_editable_date_init( GTK_EDITABLE( priv->deffect_entry ));
	my_editable_date_set_date( GTK_EDITABLE( priv->deffect_entry ), &st_last_deff );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-deffect-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( priv->deffect_entry ), label, MY_DATE_DMYY );

	g_signal_connect(
			G_OBJECT( priv->deffect_entry ), "focus-in-event", G_CALLBACK( on_deffect_focus_in ), bin );
	g_signal_connect(
			G_OBJECT( priv->deffect_entry ), "focus-out-event", G_CALLBACK( on_deffect_focus_out ), bin );
	g_signal_connect(
			G_OBJECT( priv->deffect_entry ), "changed", G_CALLBACK( on_deffect_changed ), bin );

	/* as this is easier, we only have here a single 'piece ref' entry
	 * which will be duplicated on each detail ref */
	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-piece" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( widget, "changed", G_CALLBACK( on_piece_changed ), bin );

	/* setup other widgets */
	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-model-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->model_label = GTK_LABEL( widget );

	view = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-entries" );
	g_return_if_fail( view && GTK_IS_GRID( view ));
	priv->entries_grid = GTK_GRID( view );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p3-comment" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->comment = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p3-message" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->message = widget;

	gtk_widget_show_all( GTK_WIDGET( bin ));
}

/**
 * ofa_guided_input_bin_set_ope_template:
 *
 * This must be called *after* having be attached to a parent, and the
 * main window be set
 */
void
ofa_guided_input_bin_set_ope_template( ofaGuidedInputBin *bin, const ofoOpeTemplate *template )
{
	ofaGuidedInputBinPrivate *priv;
	gint i, count;

	g_return_if_fail( bin && OFA_IS_GUIDED_INPUT_BIN( bin ));
	g_return_if_fail( template && OFO_IS_OPE_TEMPLATE( template ));

	priv = bin->priv;
	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	if( !priv->dispose_has_run ){

		priv->check_allowed = FALSE;

		/* remove previous entries if any */
		for( i=1 ; i<=priv->entries_count ; ++i ){
			remove_entry_row( bin, i );
		}
		ofs_ope_free( priv->ope );

		priv->model = template;
		priv->ope = ofs_ope_new( template );

		priv->entries_count = 0;
		count = ofo_ope_template_get_detail_count( priv->model );
		for( i=1 ; i<=count ; ++i ){
			add_entry_row( bin, i );
		}

		init_model_data( bin );

		gtk_widget_show_all( GTK_WIDGET( bin ));

		priv->check_allowed = TRUE;
		check_for_enable_dlg( bin );
	}
}

static void
init_model_data( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gchar *str;

	priv = bin->priv;

	/* operation template mnemo and label */
	str = g_strdup_printf( "%s - %s",
			ofo_ope_template_get_mnemo( priv->model ), ofo_ope_template_get_label( priv->model ));
	gtk_label_set_text( priv->model_label, str );
	g_free( str );

	/* initialize the new operation data */
	my_editable_date_set_date( GTK_EDITABLE( priv->dope_entry ), &st_last_dope );
	my_editable_date_set_date( GTK_EDITABLE( priv->deffect_entry ), &st_last_deff );

	ofa_ledger_combo_set_selected( priv->ledger_combo, ofo_ope_template_get_ledger( priv->model ));

	gtk_widget_set_sensitive(
			priv->ledger_parent, !ofo_ope_template_get_ledger_locked( priv->model ));
}

/*
 * add one row for each entry registered in the operation template
 * row number start from 1 as row 0 is used by the headers
 */
static void
add_entry_row( ofaGuidedInputBin *bin, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label;
	gchar *str;

	priv = bin->priv;

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", row );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_margin_right( label, 4 );
	gtk_widget_set_margin_bottom( label, 2 );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_label_set_width_chars( GTK_LABEL( label ), RANG_WIDTH );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_RANG, row, 1, 1 );

	/* other columns starting with OPE_COL_ACCOUNT=1 */
	add_entry_row_widget( bin, OPE_COL_ACCOUNT, row );
	add_entry_row_widget( bin, OPE_COL_ACCOUNT_SELECT, row );
	add_entry_row_widget( bin, OPE_COL_LABEL, row );
	add_entry_row_widget( bin, OPE_COL_DEBIT, row );
	add_entry_row_widget( bin, OPE_COL_CREDIT, row );
	add_entry_row_widget( bin, OPE_COL_CURRENCY, row );

	priv->entries_count += 1;
}

static void
add_entry_row_widget( ofaGuidedInputBin *bin, gint col_id, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *widget;
	const sColumnDef *col_def;

	priv = bin->priv;
	col_def = find_column_def_from_col_id( bin, col_id );
	g_return_if_fail( col_def );

	widget = NULL;

	switch( col_def->column_type ){

		case TYPE_ENTRY:
			widget = row_widget_entry( bin, col_def, row );
			break;

		case TYPE_LABEL:
			widget = row_widget_label( bin, col_def, row );
			break;

		case TYPE_BUTTON:
			widget = row_widget_button( bin, col_def, row );
			break;

		default:
			break;
	}

	if( widget ){
		gtk_grid_attach( priv->entries_grid, widget, col_id, row, 1, 1 );
	}
}

static GtkWidget *
row_widget_entry( ofaGuidedInputBin *bin, const sColumnDef *col_def, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *widget;
	const gchar *str;
	gboolean locked;
	sEntryData *sdata;

	priv = bin->priv;
	widget = NULL;

	/* only create the entry if the field is not empty or not locked
	 * (because an empty locked field will obviously never be set) */
	str = col_def->get_label ? (*col_def->get_label)( priv->model, row-1 ) : NULL;
	locked = col_def->is_locked ? (*col_def->is_locked)( priv->model, row-1 ) : FALSE;
	if(( str && g_utf8_strlen( str, -1 )) || !locked ){

		widget = gtk_entry_new();
		gtk_widget_set_hexpand( widget, col_def->expand );
		gtk_widget_set_sensitive( widget, !locked );

		if( col_def->width ){
			gtk_entry_set_width_chars( GTK_ENTRY( widget ), col_def->width );
		}

		if( col_def->is_double ){
			my_editable_amount_init(
					GTK_EDITABLE( widget ));
			my_editable_amount_set_changed_cb(
					GTK_EDITABLE( widget ), G_CALLBACK( on_entry_changed ), bin );
		} else {
			gtk_entry_set_alignment( GTK_ENTRY( widget ), col_def->xalign );
		}

		sdata = g_new0( sEntryData, 1 );
		sdata->row_id = row;
		sdata->col_def = col_def;
		sdata->locked = locked;
		sdata->previous = NULL;
		sdata->modified = FALSE;

		g_object_set_data( G_OBJECT( widget ), DATA_ENTRY_DATA, sdata );
		g_object_weak_ref( G_OBJECT( widget ), ( GWeakNotify ) on_entry_finalized, sdata );

		if( !locked ){
			g_signal_connect(
					G_OBJECT( widget ), "changed", G_CALLBACK( on_entry_changed ), bin );
			g_signal_connect(
					G_OBJECT( widget ), "focus-in-event", G_CALLBACK( on_entry_focus_in ), bin );
			g_signal_connect(
					G_OBJECT( widget ), "focus-out-event", G_CALLBACK( on_entry_focus_out ), bin );
			g_signal_connect(
					G_OBJECT( widget ), "key-press-event", G_CALLBACK( on_key_pressed ), bin );
		}
	}

	return( widget );
}

static GtkWidget *
row_widget_label( ofaGuidedInputBin *bin, const sColumnDef *col_def, gint row )
{
	GtkWidget *widget;

	widget = gtk_label_new( "" );

	if( col_def->width ){
		gtk_label_set_width_chars( GTK_LABEL( widget ), col_def->width );
	}

	return( widget );
}

static GtkWidget *
row_widget_button( ofaGuidedInputBin *bin, const sColumnDef *col_def, gint row )
{
	GtkWidget *button;

	button = gtk_button_new_from_icon_name( col_def->stock_id, GTK_ICON_SIZE_MENU );
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( col_def->column_id ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));

	g_signal_connect(
			G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), bin );

	return( button );
}

static void
remove_entry_row( ofaGuidedInputBin *bin, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	gint i;
	GtkWidget *widget;

	priv = bin->priv;

	for( i=0 ; i<OPE_N_COLUMNS ; ++i ){
		widget = gtk_grid_get_child_at( priv->entries_grid, i, row );
		if( widget ){
			gtk_widget_destroy( widget );
		}
	}
}

static void
on_entry_finalized( sEntryData *sdata, GObject *finalized_entry )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_finalized";

	g_debug( "%s: sdata=%p, finalized_entry=%p", thisfn, ( void * ) sdata, ( void * ) finalized_entry );

	g_free( sdata->previous );
	g_free( sdata );
}

/*
 * ofaLedgerCombo signal callback
 *
 * setup the minimal effect date as the greater of:
 * - the begin of the exercice (if set)
 * - the next day after the last close of the ledger (if any)
 */
static void
on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;
	ofoLedger *ledger;

	priv = bin->priv;
	ope = priv->ope;

	ledger = ofo_ledger_get_by_mnemo( priv->dossier, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	g_free( ope->ledger );
	ope->ledger = g_strdup( mnemo );

	ofo_dossier_get_min_deffect( &priv->deffect_min, priv->dossier, ledger );

	check_for_enable_dlg( bin );
}

static void
on_dope_changed( GtkEntry *entry, ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;

	priv = bin->priv;
	ope = priv->ope;

	/* check the operation date */
	my_date_set_from_date( &ope->dope,
			my_editable_date_get_date( GTK_EDITABLE( priv->dope_entry ), NULL ));

	/* setup the effect date if it has not been manually changed */
	if( my_date_is_valid( &ope->dope ) && !priv->deffect_changed_while_focus ){

		if( my_date_is_valid( &priv->deffect_min ) &&
			my_date_compare( &priv->deffect_min, &ope->dope ) > 0 ){

			my_date_set_from_date( &ope->deffect, &priv->deffect_min );

		} else {
			my_date_set_from_date( &ope->deffect, &ope->dope );
		}

		my_editable_date_set_date( GTK_EDITABLE( priv->deffect_entry ), &ope->deffect );
	}

	check_for_enable_dlg( bin );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffect_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin )
{
	bin->priv->deffect_has_focus = TRUE;
	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffect_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin )
{
	bin->priv->deffect_has_focus = FALSE;
	return( FALSE );
}

static void
on_deffect_changed( GtkEntry *entry, ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;

	priv = bin->priv;
	ope = priv->ope;

	if( priv->deffect_has_focus ){

		my_date_set_from_date( &ope->deffect,
				my_editable_date_get_date( GTK_EDITABLE( priv->deffect_entry ), NULL ));

		priv->deffect_changed_while_focus = TRUE;
	}

	check_for_enable_dlg( bin );
}

static void
on_piece_changed( GtkEditable *editable, ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	const gchar *content;

	priv = bin->priv;

	content = gtk_entry_get_text( GTK_ENTRY( editable ));

	g_free( priv->ope->ref );
	priv->ope->ref = g_strdup( content );
	priv->ope->ref_user_set = TRUE;

	check_for_enable_dlg( bin );
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
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedInputBin *bin )
{
	sEntryData *sdata;

	/* check the entry we are leaving
	 */
	sdata = g_object_get_data( G_OBJECT( widget ), DATA_ENTRY_DATA );

	switch( sdata->col_def->column_id ){
		case OPE_COL_ACCOUNT:
			if( event->state == 0 && event->keyval == GDK_KEY_Tab ){
				check_for_account( bin, GTK_ENTRY( widget ), sdata->row_id );
			}
			break;
	}

	return( FALSE );
}

/*
 * click on a button in an entry row
 */
static void
on_button_clicked( GtkButton *button, ofaGuidedInputBin *bin )
{
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));

	switch( column ){
		case OPE_COL_ACCOUNT_SELECT:
			on_account_selection( bin, row );
			break;
	}
}

/*
 * we have clicked on the 'Account selection' button
 */
static void
on_account_selection( ofaGuidedInputBin *bin, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkEntry *entry;
	gchar *number;

	priv = bin->priv;

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->entries_grid, OPE_COL_ACCOUNT, row ));
	number = ofa_account_select_run( priv->main_window, gtk_entry_get_text( entry ));
	if( number && g_utf8_strlen( number, -1 )){
		priv->focused_row = row;
		priv->focused_column = OPE_COL_ACCOUNT;
		gtk_entry_set_text( entry, number );
	}
	g_free( number );
}

/*
 * quitting the account entry with tab key:
 * check that the account exists and is not a root account
 * else open a dialog for selection
 */
static void
check_for_account( ofaGuidedInputBin *bin, GtkEntry *entry, gint row  )
{
	ofaGuidedInputBinPrivate *priv;
	ofoAccount *account;
	const gchar *asked_account;
	gchar *number;

	priv = bin->priv;

	asked_account = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( priv->dossier, asked_account );
	if( !account || ofo_account_is_root( account )){
		number = ofa_account_select_run( priv->main_window, asked_account );
		if( number ){
			priv->focused_row = row;
			priv->focused_column = OPE_COL_ACCOUNT;
			gtk_entry_set_text( entry, number );
			g_free( number );
		}
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 *
 * setting the deffect also triggers the change signal of the deffect
 * field (and so the comment)
 * => we should only react to the content while the focus is in the
 *    field
 * More, we shouldn't triggers an automatic changes to a field which
 * has been manually modified
 */
static gboolean
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_focus_in";
	ofaGuidedInputBinPrivate *priv;
	sEntryData *sdata;
	const gchar *comment;

	priv = bin->priv;
	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	priv->on_changed_count = 0;
	priv->focused_row = sdata->row_id;
	priv->focused_column = sdata->col_def->column_id;

	g_debug( "%s: entry=%p, row=%u, column=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column );

	/* get a copy of the current content */
	g_free( sdata->previous );
	sdata->previous = g_strdup( gtk_entry_get_text( entry ));

	comment = ofo_ope_template_get_detail_comment( priv->model, priv->focused_row-1 );
	set_comment( bin, comment ? comment : "" );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *bin )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_focus_out";
	ofaGuidedInputBinPrivate *priv;
	sEntryData *sdata;
	const gchar *current;

	priv = bin->priv;
	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	g_debug( "%s: entry=%p, row=%u, column=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column );

	/* compare the current content with the saved copy of the initial one */
	current = gtk_entry_get_text( entry );
	sdata->modified = ( g_utf8_collate( sdata->previous, current ) != 0 );

	/* reset focus and recursivity indicators */
	priv->on_changed_count = 0;
	priv->focused_row = 0;
	priv->focused_column = 0;

	set_comment( bin, "" );

	return( FALSE );
}

/*
 * some of the GtkEntry field of an entry row has changed -> recheck all
 * but:
 * - do not recursively recheck all the field because we have modified
 *   an automatic field
 *
 * keep trace of manual modifications of automatic fields, so that we
 * then block all next automatic recomputes
 */
static void
on_entry_changed( GtkEntry *entry, ofaGuidedInputBin *bin )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_changed";
	ofaGuidedInputBinPrivate *priv;
	ofsOpeDetail *detail;
	sEntryData *sdata;
	const gchar *cstr;

	priv = bin->priv;
	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	g_debug( "%s: entry=%p, row=%d, column=%d, focused_row=%u, focused_column=%u, on_changed_count=%u",
			thisfn, ( void * ) entry,
			sdata->row_id, sdata->col_def->column_id,
			priv->focused_row, priv->focused_column, priv->on_changed_count );

	priv->on_changed_count += 1;

	/* not in recursion: the entry is changed either during the
	 * initialization of the dialog, or because the user changes it */
	if( priv->on_changed_count == 1 ){

		cstr = gtk_entry_get_text( entry );
		detail = g_list_nth_data( priv->ope->detail, sdata->row_id-1 );

		switch( sdata->col_def->column_id ){
			case OPE_COL_ACCOUNT:
				detail->account = g_strdup( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->account_user_set = TRUE;
				}
				break;
			case OPE_COL_LABEL:
				detail->label = g_strdup( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->label_user_set = TRUE;
				}
				break;
			case OPE_COL_DEBIT:
				detail->debit = my_double_set_from_str( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->debit_user_set = TRUE;
					g_debug( "%s: row=%d, setting debit_user_set to %s", thisfn, sdata->row_id, detail->debit_user_set ? "True":"False" );
				}
				break;
			case OPE_COL_CREDIT:
				detail->credit = my_double_set_from_str( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->credit_user_set = TRUE;
				}
				break;
			default:
				break;
		}

		check_for_enable_dlg( bin );

	} else {
		g_debug( "%s: field at row=%d, column=%d changed but not checked",
				thisfn, sdata->row_id, sdata->col_def->column_id );
	}

	priv->on_changed_count -= 1;
}

static const sColumnDef *
find_column_def_from_col_id( const ofaGuidedInputBin *bin, gint col_id )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].column_id == col_id ){
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
check_for_enable_dlg( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gboolean ok;

	priv = bin->priv;

	if( priv->entries_grid && priv->check_allowed ){

		priv->check_allowed = FALSE;

		ok = is_dialog_validable( bin );
		g_signal_emit_by_name( bin, "changed", ok );

		priv->check_allowed = TRUE;
	}
}

/**
 *
 */
gboolean
ofa_guided_input_bin_is_valid( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( bin && OFA_IS_GUIDED_INPUT_BIN( bin ), FALSE );

	priv = bin->priv;
	ok = FALSE;

	if( !priv->dispose_has_run ){

		ok = is_dialog_validable( bin );
	}

	return( ok );
}

/*
 * We do not re-check nor recompute any thing while just moving from a
 * field to another - this would be not only waste of time, but also
 * keep the interface changing while doing anything else that moving
 * the focus...
 */
static gboolean
is_dialog_validable( ofaGuidedInputBin *bin )
{
	static const gchar *thisfn = "ofa_guided_input_bin_is_dialog_validable";
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;
	ofsOpeDetail *detail;
	gchar *message, *amount;
	gboolean ok;
	gint i;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	priv = bin->priv;
	ope = priv->ope;
	ofs_currency_list_free( &priv->currency_list );

	ofs_ope_apply_template( ope, priv->dossier );

	/* update the bin dialog with the new content of operation */
	for( i=0 ; i<g_list_length( ope->detail ) ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		if( !detail->account_user_set ){
			set_ope_to_ui( bin, i+1, OPE_COL_ACCOUNT, detail->account );
		}
		if( !detail->label_user_set ){
			set_ope_to_ui( bin, i+1, OPE_COL_LABEL, detail->label );
		}
		if( !detail->debit_user_set ){
			amount = my_double_to_str( detail->debit );
			set_ope_to_ui( bin, i+1, OPE_COL_DEBIT, amount );
			g_free( amount );
		}
		if( !detail->credit_user_set ){
			amount = my_double_to_str( detail->credit );
			set_ope_to_ui( bin, i+1, OPE_COL_CREDIT, amount );
			g_free( amount );
		}
	}

	ok = ofs_ope_is_valid( ope, priv->dossier, &message, &priv->currency_list );

	display_currencies( bin );
	update_totals( bin );
	set_message( bin, message );

	g_free( message );

	return( ok );
}

static void
set_ope_to_ui( const ofaGuidedInputBin *bin, gint row, gint col_id, const gchar *content )
{
	ofaGuidedInputBinPrivate *priv;
	const sColumnDef *def;
	GtkWidget *entry;

	priv = bin->priv;
	def = find_column_def_from_col_id( bin, col_id );
	g_return_if_fail( def );

	if( def->column_type == TYPE_ENTRY && content ){
		entry = gtk_grid_get_child_at( priv->entries_grid, col_id, row );
		/*g_debug( "set_ope_to_ui: row=%d, col_id=%d, content=%s, entry=%p", row, col_id, content, ( void * ) entry );*/

		if( entry ){
			g_return_if_fail( GTK_IS_ENTRY( entry ));

			if( def->is_double ){
				g_signal_handlers_block_by_func( entry, on_entry_changed, ( gpointer ) bin );
				my_editable_amount_set_string( GTK_EDITABLE( entry ), content );
				g_signal_handlers_unblock_by_func( entry, on_entry_changed, ( gpointer ) bin );

			} else {
				gtk_entry_set_text( GTK_ENTRY( entry ), content );
			}
		}
	}
}

static void
set_comment( ofaGuidedInputBin *bin, const gchar *comment )
{
	ofaGuidedInputBinPrivate *priv;

	priv = bin->priv;

	gtk_label_set_text( GTK_LABEL( priv->comment ), comment );
}

static void
set_message( ofaGuidedInputBin *bin, const gchar *errmsg )
{
	ofaGuidedInputBinPrivate *priv;
	GdkRGBA color;

	priv = bin->priv;

	gtk_label_set_text( GTK_LABEL( priv->message ), errmsg );

	if( gdk_rgba_parse(
			&color, errmsg && g_utf8_strlen( errmsg, -1 ) ? "#ff0000":"#000000" )){
		gtk_widget_override_color( priv->message, GTK_STATE_FLAG_NORMAL, &color );
	}
}

/*
 * only display the currency if different from default currency
 */
static void
display_currencies( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpeDetail *detail;
	ofoAccount *account;
	gint i, count;
	const gchar *currency, *display_cur;
	GtkWidget *label;

	priv = bin->priv;
	count = g_list_length( priv->ope->detail );

	for( i=0 ; i<count ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( priv->ope->detail, i );
		display_cur = "";
		if( detail->account ){
			account = ofo_account_get_by_number( priv->dossier, detail->account );
			if( account ){
				currency = ofo_account_get_currency( account );
				if( g_utf8_collate( currency, priv->def_currency )){
					display_cur = currency;
				}
			}
		}
		label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CURRENCY, i+1 );
		gtk_label_set_text( GTK_LABEL( label ), display_cur );
	}
}

/*
 * entries_count is the current count of entry rows added in the grid
 * (may be lesser than the count of entries in the model during the
 *  initialization)
 *
 * totals_count is the count of total and diff lines added in the grid
 * (may be zero the first time) - is usually equal to 2 x previous count
 * of currencies
 */
static gboolean
update_totals( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gint model_count, i;
	GList *it;
	ofsCurrency *sbal;
	GtkWidget *label, *entry;
	gboolean ok, oki;
	gdouble ddiff, cdiff;
	gchar *total_str;

	priv = bin->priv;

	if( !priv->model ){
		return( FALSE );
	}

	ok = TRUE;
	model_count = ofo_ope_template_get_detail_count( priv->model );

	for( it=priv->currency_list, i=0 ; it ; it=it->next, i+=2 ){

		/* insert total and diff lines */
		if( priv->totals_count < i+2 ){
			total_add_diff_lines( bin, model_count );
		}

		sbal = ( ofsCurrency * ) it->data;

		/* setup currency, totals and diffs */
		label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_LABEL, model_count+i+1 );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
		total_str = g_strdup_printf( _( "Total %s :"), sbal->currency );
		gtk_label_set_text( GTK_LABEL( label ), total_str );
		g_free( total_str );

		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_DEBIT, model_count+i+1 );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		my_editable_amount_set_amount( GTK_EDITABLE( entry ), sbal->debit );

		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CREDIT, model_count+i+1 );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		my_editable_amount_set_amount( GTK_EDITABLE( entry ), sbal->credit );

		ddiff = 0;
		cdiff = 0;
		oki = FALSE;

		if( sbal->debit > sbal->credit ){
			cdiff = sbal->debit - sbal->credit;

		} else if( sbal->debit < sbal->credit ){
			ddiff = sbal->credit - sbal->debit;

		} else {
			oki = TRUE;
		}

		total_display_diff( bin, sbal->currency, model_count+i+2, ddiff, cdiff );

		ok &= oki;
	}

	/* at the end, remove the unneeded supplementary lines */
	for( ; i<priv->totals_count ; ++i ){
		remove_entry_row( bin, model_count+i+1 );
	}
	priv->totals_count = 2*g_list_length( priv->currency_list );

	return( ok );
}

/*
 * we insert two lines for total and diff when the entries_count is
 * equal to the count of the lines of the model (i.e. there is not
 * enough total/diff lines to hold the next currency)
 */
static void
total_add_diff_lines( ofaGuidedInputBin *bin, gint model_count )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label, *entry;
	gint row;

	priv = bin->priv;
	row = model_count + priv->totals_count;

	label = gtk_label_new( NULL );
	gtk_widget_set_margin_top( label, TOTAUX_TOP_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_LABEL, row+1, 1, 1 );

	entry = gtk_entry_new();
	my_editable_amount_init( GTK_EDITABLE( entry ));
	gtk_widget_set_can_focus( entry, FALSE );
	gtk_widget_set_margin_top( entry, TOTAUX_TOP_MARGIN );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, entry, OPE_COL_DEBIT, row+1, 1, 1 );

	entry = gtk_entry_new();
	my_editable_amount_init( GTK_EDITABLE( entry ));
	gtk_widget_set_can_focus( entry, FALSE );
	gtk_widget_set_margin_top( entry, TOTAUX_TOP_MARGIN );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, entry, OPE_COL_CREDIT, row+1, 1, 1 );

	label = gtk_label_new( _( "Diff :" ));
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_LABEL, row+2, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_widget_set_margin_right( label, 2 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_DEBIT, row+2, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_widget_set_margin_right( label, 2 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_CREDIT, row+2, 1, 1 );

	label = gtk_label_new( NULL );
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_CURRENCY, row+2, 1, 1 );

	gtk_widget_show_all( GTK_WIDGET( priv->entries_grid ));
	priv->totals_count += 2;
}

static void
total_display_diff( ofaGuidedInputBin *bin, const gchar *currency, gint row, gdouble ddiff, gdouble cdiff )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label;
	gchar *amount_str;
	GdkRGBA color;
	gboolean has_diff;

	priv = bin->priv;
	has_diff = FALSE;

	gdk_rgba_parse( &color, "#FF0000" );

	/* set the debit diff amount (or empty) */
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_DEBIT, row );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	amount_str = NULL;
	if( ddiff ){
		amount_str = my_double_to_str( ddiff );
		has_diff = TRUE;
	}
	gtk_label_set_text( GTK_LABEL( label ), amount_str );
	g_free( amount_str );
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );

	/* set the credit diff amount (or empty) */
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CREDIT, row );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	amount_str = NULL;
	if( cdiff ){
		amount_str = my_double_to_str( cdiff );
		has_diff = TRUE;
	}
	gtk_label_set_text( GTK_LABEL( label ), amount_str );
	g_free( amount_str );
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );

	/* set the currency */
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CURRENCY, row );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), has_diff ? currency : NULL );
	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
}

/**
 * ofa_guided_input_bin_apply:
 *
 * Generate the entries.
 * All the entries are created in memory and checked before being
 * serialized. Only after that, ledger and accounts are updated.
 *
 * Returns %TRUE if ok.
 */
gboolean
ofa_guided_input_bin_apply( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( bin && OFA_IS_GUIDED_INPUT_BIN( bin ), FALSE );
	g_return_val_if_fail( is_dialog_validable( bin ), FALSE );

	ok = FALSE;
	priv = bin->priv;

	if( !priv->dispose_has_run ){

		if( do_validate( bin )){
			ok = TRUE;
			do_reset_entries_rows( bin );
		}
	}

	return( ok );
}

static gboolean
do_validate( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gboolean ok;
	GList *entries, *it;

	priv = bin->priv;
	ok = TRUE;
	entries = ofs_ope_generate_entries( priv->ope, priv->dossier );

	for( it=entries ; it ; it=it->next ){
		ok &= ofo_entry_insert( OFO_ENTRY( it->data ), priv->dossier );
		/* TODO:
		 * in case of an error, remove the already recorded entries
		 * of the list, decrementing the ledgers and the accounts
		 * then restore the last ecr number of the dossier
		 */
	}
	if( ok ){
		display_ok_message( bin, g_list_length( entries ));
	}

	g_list_free_full( entries, g_object_unref );

	my_date_set_from_date( &st_last_dope, &priv->ope->dope );
	my_date_set_from_date( &st_last_deff, &priv->ope->deffect );

	return( ok );
}

static void
display_ok_message( ofaGuidedInputBin *bin, gint count )
{
	GtkWidget *dialog;
	gchar *message;

	message = g_strdup_printf( _( "%d entries have been successfully created" ), count );

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( bin->priv->main_window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"%s", message );

	g_free( message );
	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

/**
 * ofa_guided_input_bin_reset:
 *
 * Reset the input fields, keeping the dates and the same entry model
 */
void
ofa_guided_input_bin_reset( ofaGuidedInputBin *bin )
{
	g_return_if_fail( bin && OFA_IS_GUIDED_INPUT_BIN( bin ));

	if( !bin->priv->dispose_has_run ){

		do_reset_entries_rows( bin );
	}
}

/*
 * nb: entries_count = count of entries + 2 (for totals and diff)
 * Only the LABEL entries may be non present on the last two lines
 */
static void
do_reset_entries_rows( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;
	gint i;
	GtkWidget *entry;

	priv = bin->priv;

	for( i=1 ; i<=priv->entries_count ; ++i ){
		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_LABEL, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_DEBIT, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CREDIT, i );
		if( entry && GTK_IS_ENTRY( entry )){
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
		}
	}
}

/*
 * SIGNAL_DOSSIER_UPDATED_OBJECT signal handler
 */
static void
on_updated_object( const ofoDossier *dossier, const ofoBase *object, const gchar *prev_id, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_updated_object";

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( OFO_OPE_TEMPLATE( object ) == self->priv->model ){
			ofa_guided_input_bin_set_ope_template( self, OFO_OPE_TEMPLATE( object ));
		}
	}
}

/*
 * SIGNAL_DOSSIER_DELETED_OBJECT signal handler
 */
static void
on_deleted_object( const ofoDossier *dossier, const ofoBase *object, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_deleted_object";
	ofaGuidedInputBinPrivate *priv;
	gint i;

	g_debug( "%s: dossier=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	priv = self->priv;

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( OFO_OPE_TEMPLATE( object ) == priv->model ){

			for( i=0 ; i<priv->entries_count ; ++i ){
				remove_entry_row( self, i );
			}
			priv->model = NULL;
			priv->entries_count = 0;
		}
	}
}
