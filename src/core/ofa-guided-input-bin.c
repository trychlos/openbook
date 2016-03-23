/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-double-editable.h"
#include "my/my-date-editable.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

#include "core/ofa-account-select.h"
#include "core/ofa-ledger-combo.h"
#include "core/ofa-main-window.h"
#include "core/ofa-guided-input-bin.h"

/* private instance data
 */
struct _ofaGuidedInputBinPrivate {
	gboolean              dispose_has_run;

	/* input parameters at initialization time
	 */
	const ofaMainWindow  *main_window;
	ofaHub               *hub;
	GList                *hub_handlers;

	/* from dossier
	 */
	const gchar          *def_currency;

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
	GtkWidget            *dope_entry;
	GtkWidget            *deffect_entry;
	gboolean              deffect_has_focus;
	GtkWidget            *ref_entry;
	GtkGrid              *entries_grid;			/* entries grid container */
	/* total count of rows = count of entries + 2 * count of currencies
	 * rows are at position starting with 1 (as row 0 is for headers) */
	gint                  rows_count;
	GtkWidget            *comment;
	GtkWidget            *message;

	/* check that on_entry_changed is not recursively called */
	gint                  on_changed_count;
	gboolean              check_allowed;

	/* keep trace of current row/column so that we do not recompute
	 * the currently modified entry (only for debit and credit) */
	gint                  focused_row;
	gint                  focused_column;

	/* a list of ofsCurrency structs which keeps trace of used currencies */
	GList                *currency_list;
};

#define RANG_WIDTH                      3
#define LABEL_MAX_WIDTH               256
#define AMOUNTS_WIDTH                  10
#define AMOUNTS_MAX_WIDTH              10
#define CURRENCY_WIDTH                  4

#define TOTAUX_TOP_MARGIN               8

/* space between widgets in a detail line */
#define DETAIL_SPACE                    2

enum {
	TYPE_NONE = 0,
	TYPE_ENTRY,
	TYPE_LABEL,
	TYPE_IMAGE
};

/* definition of the columns
 */
typedef struct {
	gint	        column_id;
	gint            column_type;
	const gchar * (*get_label)( const ofoOpeTemplate *, gint );
	gboolean      (*is_locked)( const ofoOpeTemplate *, gint );
	gint            width;					/* entry, label */
	gint            max_width;
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
				-1, -1, FALSE, 0, FALSE, NULL
		},
		{ OPE_COL_LABEL,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_label,
				ofo_ope_template_get_detail_label_locked,
				-1, LABEL_MAX_WIDTH, FALSE, 0, TRUE, NULL
		},
		{ OPE_COL_DEBIT,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_debit,
				ofo_ope_template_get_detail_debit_locked,
				AMOUNTS_WIDTH, AMOUNTS_MAX_WIDTH, TRUE, 0, FALSE, NULL
		},
		{ OPE_COL_CREDIT,
				TYPE_ENTRY,
				ofo_ope_template_get_detail_credit,
				ofo_ope_template_get_detail_credit_locked,
				AMOUNTS_WIDTH, AMOUNTS_MAX_WIDTH, TRUE, 0, FALSE, NULL
		},
		{ OPE_COL_CURRENCY,
				TYPE_LABEL,
				NULL,
				NULL,
				CURRENCY_WIDTH, CURRENCY_WIDTH, FALSE, 0, FALSE, NULL
		},
		{ OPE_COL_VALID,
				TYPE_IMAGE,
				NULL,
				NULL,
				-1, -1, FALSE, 0.5, FALSE, NULL
		},
		{ 0 }
};

/* a structure attached to each dynamically created entry field
 */
typedef struct {
	gint              row_id;			/* counted from 1 */
	const sColumnDef *col_def;
	gboolean          locked;
	gchar            *initial;			/* initial content when focusing into the entry */
	gboolean          modified;			/* whether the entry has been manually modified */
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

static guint st_signals[ N_SIGNALS ]        = { 0 };

static GDate st_last_dope                   = { 0 };
static GDate st_last_deff                   = { 0 };

static const gchar *st_resource_image_empty = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_image_check = "/org/trychlos/openbook/core/ofa-guided-input-bin-green-checkmark.png";
static const gchar *st_resource_ui          = "/org/trychlos/openbook/core/ofa-guided-input-bin.ui";

static void              setup_main_window( ofaGuidedInputBin *self );
static void              setup_dialog( ofaGuidedInputBin *self );
static void              init_model_data( ofaGuidedInputBin *self );
static void              add_entry_row( ofaGuidedInputBin *self, gint i );
static void              add_entry_row_widget( ofaGuidedInputBin *self, gint col_id, gint row );
static GtkWidget        *row_widget_entry( ofaGuidedInputBin *self, const sColumnDef *col_def, gint row );
static GtkWidget        *row_widget_label( ofaGuidedInputBin *self, const sColumnDef *col_def, gint row );
static GtkWidget        *row_widget_image( ofaGuidedInputBin *self, const sColumnDef *col_def, gint row );
static void              on_entry_finalized( sEntryData *sdata, GObject *finalized_entry );
static void              on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaGuidedInputBin *self );
static void              on_dope_changed( GtkEntry *entry, ofaGuidedInputBin *self );
static gboolean          on_deffect_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self );
static gboolean          on_deffect_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self );
static void              on_deffect_changed( GtkEntry *entry, ofaGuidedInputBin *self );
static void              on_piece_changed( GtkEditable *editable, ofaGuidedInputBin *self );
static gboolean          on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedInputBin *self );
static void              do_account_selection( ofaGuidedInputBin *self, GtkEntry *entry, gint row );
static gchar            *on_account_postselect( GtkEditable *editable, ofeAccountAllowed allowed, const gchar *account_id, ofaGuidedInputBin *self );
static void              check_for_account( ofaGuidedInputBin *self, GtkEntry *entry, gint row );
static gboolean          on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self );
static gboolean          on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self );
static void              on_entry_changed( GtkEntry *entry, ofaGuidedInputBin *self );
static void              setup_account_label_in_comment( ofaGuidedInputBin *self, gint row_id );
static const sColumnDef *find_column_def_from_col_id( const ofaGuidedInputBin *self, gint col_id );
static void              check_for_enable_dlg( ofaGuidedInputBin *self );
static gboolean          is_dialog_validable( ofaGuidedInputBin *self );
static void              set_ope_to_ui( const ofaGuidedInputBin *self, gint row, gint col_id, const gchar *content );
static void              display_currency( ofaGuidedInputBin *self, gint row, ofsOpeDetail *detail );
static void              draw_valid_coche( ofaGuidedInputBin *self, gint row, gboolean bvalid );
static void              set_comment( ofaGuidedInputBin *self, const gchar *comment );
static void              set_message( ofaGuidedInputBin *self, const gchar *errmsg );
static gboolean          update_totals( ofaGuidedInputBin *self );
static void              add_total_diff_lines( ofaGuidedInputBin *self, gint model_count );
static void              total_display_diff( ofaGuidedInputBin *self, ofoCurrency *currency, gint row, gdouble ddiff, gdouble cdiff );
static gboolean          do_validate( ofaGuidedInputBin *self );
static void              display_ok_message( ofaGuidedInputBin *self, gint count );
static void              do_reset_entries_rows( ofaGuidedInputBin *self );
static void              on_hub_updated_object( const ofaHub *hub, const ofoBase *object, const gchar *prev_id, ofaGuidedInputBin *self );
static void              on_hub_deleted_object( const ofaHub *hub, const ofoBase *object, ofaGuidedInputBin *self );

G_DEFINE_TYPE_EXTENDED( ofaGuidedInputBin, ofa_guided_input_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaGuidedInputBin ))

static void
guided_input_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_input_bin_finalize";
	ofaGuidedInputBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT_BIN( instance ));

	/* free data members here */
	priv = ofa_guided_input_bin_get_instance_private( OFA_GUIDED_INPUT_BIN( instance ));

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

	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT_BIN( instance ));

	priv = ofa_guided_input_bin_get_instance_private( OFA_GUIDED_INPUT_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );
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

	priv = ofa_guided_input_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	my_date_clear( &priv->deffect_min );
	priv->currency_list = NULL;
}

static void
ofa_guided_input_bin_class_init( ofaGuidedInputBinClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_input_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_input_bin_finalize;

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
				"ofa-changed",
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
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Returns: a new #ofaGuidedInputBin instance.
 */
ofaGuidedInputBin *
ofa_guided_input_bin_new( const ofaMainWindow *main_window )
{
	ofaGuidedInputBin *self;
	ofaGuidedInputBinPrivate *priv;

	self = g_object_new( OFA_TYPE_GUIDED_INPUT_BIN, NULL );

	priv = ofa_guided_input_bin_get_instance_private( self );

	priv->main_window = main_window;

	my_utils_container_attach_from_resource( GTK_CONTAINER( self ), st_resource_ui, "gib-window", "top" );
	setup_main_window( self );
	setup_dialog( self );

	return( self );
}

static void
setup_main_window( ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	GtkApplication *application;
	ofoDossier *dossier;
	gulong handler;

	priv = ofa_guided_input_bin_get_instance_private( self );

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	/* setup from dossier
	 * datas which come from the dossier are read once
	 * they are supposed to stay unchanged while the window is alive */
	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->def_currency = ofo_dossier_get_default_currency( dossier );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

static void
setup_dialog( ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label, *widget, *view;

	priv = ofa_guided_input_bin_get_instance_private( self );

	/* set ledger combo */
	priv->ledger_combo = ofa_ledger_combo_new();
	priv->ledger_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-parent" );
	g_return_if_fail( priv->ledger_parent && GTK_IS_CONTAINER( priv->ledger_parent ));
	gtk_container_add( GTK_CONTAINER( priv->ledger_parent ), GTK_WIDGET( priv->ledger_combo ));
	ofa_ledger_combo_set_columns( priv->ledger_combo, LEDGER_DISP_LABEL );
	ofa_ledger_combo_set_hub( priv->ledger_combo, priv->hub );
	gtk_widget_set_sensitive( priv->ledger_parent, FALSE );

	g_signal_connect(
			G_OBJECT( priv->ledger_combo ), "ofa-changed", G_CALLBACK( on_ledger_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->ledger_combo ));

	/* when opening the window, dates are set to the last used (from the
	 * global static variables)
	 * if the window stays alive after a validation (the case of the main
	 * page), then the dates stays untouched
	 */
	/* operation date */
	priv->dope_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-entry" );
	g_return_if_fail( priv->dope_entry && GTK_IS_ENTRY( priv->dope_entry ));
	gtk_widget_set_sensitive( priv->dope_entry, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->dope_entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->dope_entry ));
	my_date_editable_set_label( GTK_EDITABLE( priv->dope_entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( priv->dope_entry ), &st_last_dope );

	g_signal_connect( priv->dope_entry, "changed", G_CALLBACK( on_dope_changed ), self );

	/* effect date */
	priv->deffect_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-entry" );
	g_return_if_fail( priv->deffect_entry && GTK_IS_ENTRY( priv->deffect_entry ));
	gtk_widget_set_sensitive( priv->deffect_entry, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->deffect_entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->deffect_entry ));
	my_date_editable_set_label( GTK_EDITABLE( priv->deffect_entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( priv->deffect_entry ), &st_last_deff );

	g_signal_connect( priv->deffect_entry, "focus-in-event", G_CALLBACK( on_deffect_focus_in ), self );
	g_signal_connect( priv->deffect_entry, "focus-out-event", G_CALLBACK( on_deffect_focus_out ), self );
	g_signal_connect( priv->deffect_entry, "changed", G_CALLBACK( on_deffect_changed ), self );

	/* piece ref */
	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-piece-entry" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( widget, "changed", G_CALLBACK( on_piece_changed ), self );
	gtk_widget_set_sensitive( widget, FALSE );
	priv->ref_entry = widget;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-piece-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->ref_entry );

	/* setup other widgets */
	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-model-label" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->model_label = GTK_LABEL( widget );

	view = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-entries" );
	g_return_if_fail( view && GTK_IS_GRID( view ));
	priv->entries_grid = GTK_GRID( view );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-comment" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->comment = widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-message" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	priv->message = widget;

	gtk_widget_show_all( GTK_WIDGET( self ));
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

	priv = ofa_guided_input_bin_get_instance_private( bin );
	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));
	g_return_if_fail( !priv->dispose_has_run );

	priv->check_allowed = FALSE;

	/* remove previous entries if any */
	for( i=priv->rows_count ; i>=1 ; --i ){
		gtk_grid_remove_row( priv->entries_grid, i );
	}
	ofs_ope_free( priv->ope );
	priv->rows_count = 0;

	priv->model = template;
	priv->ope = ofs_ope_new( template );

	count = ofo_ope_template_get_detail_count( priv->model );
	for( i=1 ; i<=count ; ++i ){
		add_entry_row( bin, i );
	}

	init_model_data( bin );

	gtk_widget_show_all( GTK_WIDGET( bin ));

	priv->check_allowed = TRUE;
	check_for_enable_dlg( bin );
}

static void
init_model_data( ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	gchar *str;
	const gchar *cstr;

	priv = ofa_guided_input_bin_get_instance_private( self );

	/* operation and effect dates */
	gtk_widget_set_sensitive( priv->dope_entry, TRUE );
	gtk_widget_set_sensitive( priv->deffect_entry, TRUE );

	/* operation template mnemo and label */
	str = g_strdup_printf( "%s - %s",
			ofo_ope_template_get_mnemo( priv->model ), ofo_ope_template_get_label( priv->model ));
	gtk_label_set_text( priv->model_label, str );
	g_free( str );

	/* initialize the new operation data */
	my_date_editable_set_date( GTK_EDITABLE( priv->dope_entry ), &st_last_dope );
	my_date_set_from_date( &priv->ope->dope, &st_last_dope );

	my_date_editable_set_date( GTK_EDITABLE( priv->deffect_entry ), &st_last_deff );
	my_date_set_from_date( &priv->ope->deffect, &st_last_deff );

	/* ledger */
	ofa_ledger_combo_set_selected( priv->ledger_combo, ofo_ope_template_get_ledger( priv->model ));
	gtk_widget_set_sensitive(
			priv->ledger_parent, !ofo_ope_template_get_ledger_locked( priv->model ));

	/* piece ref */
	cstr = ofo_ope_template_get_ref( priv->model );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->ref_entry ), cstr );
	}
	gtk_widget_set_sensitive(
			priv->ref_entry, !ofo_ope_template_get_ref_locked( priv->model ));
}

/*
 * add one row for each entry registered in the operation template
 * row number start from 1 as row 0 is used by the headers
 */
static void
add_entry_row( ofaGuidedInputBin *self, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label;
	gchar *str;

	priv = ofa_guided_input_bin_get_instance_private( self );

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", row );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	my_utils_widget_set_margins( label, 0, 2, 0, 4 );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), RANG_WIDTH );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_RANG, row, 1, 1 );

	/* other columns starting with OPE_COL_ACCOUNT=1 */
	add_entry_row_widget( self, OPE_COL_ACCOUNT, row );
	add_entry_row_widget( self, OPE_COL_LABEL, row );
	add_entry_row_widget( self, OPE_COL_DEBIT, row );
	add_entry_row_widget( self, OPE_COL_CREDIT, row );
	add_entry_row_widget( self, OPE_COL_CURRENCY, row );
	add_entry_row_widget( self, OPE_COL_VALID, row );

	priv->rows_count += 1;
}

static void
add_entry_row_widget( ofaGuidedInputBin *self, gint col_id, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *widget;
	const sColumnDef *col_def;
	const gchar *comment;

	priv = ofa_guided_input_bin_get_instance_private( self );

	col_def = find_column_def_from_col_id( self, col_id );
	g_return_if_fail( col_def );

	widget = NULL;

	switch( col_def->column_type ){

		case TYPE_ENTRY:
			widget = row_widget_entry( self, col_def, row );
			break;

		case TYPE_LABEL:
			widget = row_widget_label( self, col_def, row );
			break;

		case TYPE_IMAGE:
			widget = row_widget_image( self, col_def, row );
			break;

		default:
			break;
	}

	if( widget ){
		comment = ofo_ope_template_get_detail_comment( priv->model, row-1 );
		gtk_widget_set_tooltip_text( widget, comment );
		gtk_grid_attach( priv->entries_grid, widget, col_id, row, 1, 1 );
	}
}

static GtkWidget *
row_widget_entry( ofaGuidedInputBin *self, const sColumnDef *col_def, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *widget;
	const gchar *str;
	gboolean locked;
	sEntryData *sdata;

	priv = ofa_guided_input_bin_get_instance_private( self );

	widget = NULL;

	/* only create the entry if the field is not empty or not locked
	 * (because an empty locked field will obviously never be set) */
	str = col_def->get_label ? (*col_def->get_label)( priv->model, row-1 ) : NULL;
	locked = col_def->is_locked ? (*col_def->is_locked)( priv->model, row-1 ) : FALSE;
	if(( my_strlen( str )) || !locked ){

		widget = gtk_entry_new();
		gtk_widget_set_hexpand( widget, col_def->expand );
		gtk_widget_set_sensitive( widget, !locked );

		if( col_def->width > 0 ){
			gtk_entry_set_width_chars( GTK_ENTRY( widget ), col_def->width );
		}
		if( col_def->max_width > 0 ){
			gtk_entry_set_max_width_chars( GTK_ENTRY( widget ), col_def->max_width );
		}

		if( col_def->is_double ){
			my_double_editable_init_ex( GTK_EDITABLE( widget ),
					g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
					ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), CUR_DEFAULT_DIGITS );
			my_double_editable_set_changed_cb(
					GTK_EDITABLE( widget ), G_CALLBACK( on_entry_changed ), self );
		} else {
			gtk_entry_set_alignment( GTK_ENTRY( widget ), col_def->xalign );
		}

		sdata = g_new0( sEntryData, 1 );
		sdata->row_id = row;
		sdata->col_def = col_def;
		sdata->locked = locked;
		sdata->initial = NULL;
		sdata->modified = FALSE;

		g_object_set_data( G_OBJECT( widget ), DATA_ENTRY_DATA, sdata );
		g_object_weak_ref( G_OBJECT( widget ), ( GWeakNotify ) on_entry_finalized, sdata );

		if( !locked ){
			g_signal_connect( widget, "changed", G_CALLBACK( on_entry_changed ), self );
			g_signal_connect( widget, "focus-in-event", G_CALLBACK( on_entry_focus_in ), self );
			g_signal_connect( widget, "focus-out-event", G_CALLBACK( on_entry_focus_out ), self );
			g_signal_connect( widget, "key-press-event", G_CALLBACK( on_key_pressed ), self );
		}

		if( col_def->column_id == OPE_COL_ACCOUNT && !locked ){
			ofa_account_editable_init(
					GTK_EDITABLE( widget ), OFA_MAIN_WINDOW( priv->main_window ), ACCOUNT_ALLOW_DETAIL );
			ofa_account_editable_set_postselect_cb(
					GTK_EDITABLE( widget ), ( AccountPostSelectCb ) on_account_postselect, self );
		}
	}

	return( widget );
}

static GtkWidget *
row_widget_label( ofaGuidedInputBin *self, const sColumnDef *col_def, gint row )
{
	GtkWidget *widget;

	widget = gtk_label_new( "" );
	my_utils_widget_set_xalign( widget, col_def->xalign );
	if( col_def->width ){
		gtk_label_set_width_chars( GTK_LABEL( widget ), col_def->width );
	}

	return( widget );
}

static GtkWidget *
row_widget_image( ofaGuidedInputBin *self, const sColumnDef *col_def, gint row )
{
	GtkWidget *image;

	image = gtk_image_new_from_resource( st_resource_image_empty );

	return( image );
}

static void
on_entry_finalized( sEntryData *sdata, GObject *finalized_entry )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_finalized";

	g_debug( "%s: sdata=%p, finalized_entry=%p", thisfn, ( void * ) sdata, ( void * ) finalized_entry );

	g_free( sdata->initial );
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
on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;
	ofoLedger *ledger;
	ofoDossier *dossier;

	priv = ofa_guided_input_bin_get_instance_private( self );

	ope = priv->ope;
	dossier = ofa_hub_get_dossier( priv->hub );

	ledger = ofo_ledger_get_by_mnemo( priv->hub, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	g_free( ope->ledger );
	ope->ledger = g_strdup( mnemo );

	ofo_dossier_get_min_deffect( dossier, ledger, &priv->deffect_min );

	check_for_enable_dlg( self );
}

static void
on_dope_changed( GtkEntry *entry, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;

	priv = ofa_guided_input_bin_get_instance_private( self );

	ope = priv->ope;

	/* check the operation date */
	my_date_set_from_date( &ope->dope,
			my_date_editable_get_date( GTK_EDITABLE( priv->dope_entry ), NULL ));

	/* setup the effect date if it has not been manually changed */
	if( my_date_is_valid( &ope->dope )){
		ope->dope_user_set = TRUE;

		if( !ope->deffect_user_set ){
			if( my_date_is_valid( &priv->deffect_min ) &&
				my_date_compare( &priv->deffect_min, &ope->dope ) > 0 ){

				my_date_set_from_date( &ope->deffect, &priv->deffect_min );

			} else {
				my_date_set_from_date( &ope->deffect, &ope->dope );
			}

			my_date_editable_set_date( GTK_EDITABLE( priv->deffect_entry ), &ope->deffect );
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
on_deffect_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;

	priv = ofa_guided_input_bin_get_instance_private( self );

	priv->deffect_has_focus = TRUE;

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_deffect_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;

	priv = ofa_guided_input_bin_get_instance_private( self );

	priv->deffect_has_focus = FALSE;

	return( FALSE );
}

static void
on_deffect_changed( GtkEntry *entry, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;

	priv = ofa_guided_input_bin_get_instance_private( self );

	ope = priv->ope;

	if( priv->deffect_has_focus ){
		my_date_set_from_date( &ope->deffect,
				my_date_editable_get_date( GTK_EDITABLE( priv->deffect_entry ), NULL ));
		ope->deffect_user_set = TRUE;
	}

	check_for_enable_dlg( self );
}

static void
on_piece_changed( GtkEditable *editable, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	const gchar *content;

	priv = ofa_guided_input_bin_get_instance_private( self );

	content = gtk_entry_get_text( GTK_ENTRY( editable ));

	g_free( priv->ope->ref );
	priv->ope->ref = g_strdup( content );
	priv->ope->ref_user_set = TRUE;

	check_for_enable_dlg( self );
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
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaGuidedInputBin *self )
{
	sEntryData *sdata;

	/* check the entry we are leaving
	 */
	sdata = g_object_get_data( G_OBJECT( widget ), DATA_ENTRY_DATA );

	switch( sdata->col_def->column_id ){
		case OPE_COL_ACCOUNT:
			if( event->state == 0 && event->keyval == GDK_KEY_Tab ){
				check_for_account( self, GTK_ENTRY( widget ), sdata->row_id );
			}
			break;
	}

	return( FALSE );
}

/*
 * unconditionnally open the selection account dialog box
 */
static void
do_account_selection( ofaGuidedInputBin *self, GtkEntry *entry, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	gchar *number;
	GtkWidget *toplevel;

	priv = ofa_guided_input_bin_get_instance_private( self );

	toplevel = gtk_widget_get_toplevel( GTK_WIDGET( entry ));

	number = ofa_account_select_run(
					OFA_MAIN_WINDOW( priv->main_window ),
					toplevel ? GTK_WINDOW( toplevel ) : NULL,
					gtk_entry_get_text( entry ), ACCOUNT_ALLOW_DETAIL );

	if( my_strlen( number )){
		priv->focused_row = row;
		priv->focused_column = OPE_COL_ACCOUNT;
		gtk_entry_set_text( entry, number );
	}
	g_free( number );
}

static gchar *
on_account_postselect( GtkEditable *editable, ofeAccountAllowed allowed, const gchar *account_id, ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	sEntryData *sdata;

	priv = ofa_guided_input_bin_get_instance_private( self );

	if( my_strlen( account_id )){
		sdata = g_object_get_data( G_OBJECT( editable ), DATA_ENTRY_DATA );
		priv->focused_row = sdata->row_id;
		priv->focused_column = OPE_COL_ACCOUNT;
	}

	return( NULL );
}

/*
 * quitting the account entry with tab key:
 * check that the account exists and is not a root account
 * else open a dialog for selection
 */
static void
check_for_account( ofaGuidedInputBin *self, GtkEntry *entry, gint row  )
{
	ofaGuidedInputBinPrivate *priv;
	ofoAccount *account;
	const gchar *asked_account;

	priv = ofa_guided_input_bin_get_instance_private( self );

	asked_account = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( priv->hub, asked_account );
	if( !account || ofo_account_is_root( account )){
		do_account_selection( self, entry, row );
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
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_focus_in";
	ofaGuidedInputBinPrivate *priv;
	sEntryData *sdata;

	priv = ofa_guided_input_bin_get_instance_private( self );

	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	priv->on_changed_count = 0;
	priv->focused_row = sdata->row_id;
	priv->focused_column = sdata->col_def->column_id;

	g_debug( "%s: entry=%p, row=%u, column=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column );

	/* get a copy of the initial content */
	g_free( sdata->initial );
	if( sdata->col_def->is_double ){
		sdata->initial = my_double_editable_get_string( GTK_EDITABLE( entry ));
	} else {
		sdata->initial = g_strdup( gtk_entry_get_text( entry ));
	}

	/* update the account label when changing the row */
	setup_account_label_in_comment( self, sdata->row_id );

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_focus_out";
	ofaGuidedInputBinPrivate *priv;
	sEntryData *sdata;
	const gchar *current;

	priv = ofa_guided_input_bin_get_instance_private( self );

	sdata = g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_DATA );

	g_debug( "%s: entry=%p, row=%u, column=%u",
			thisfn, ( void * ) entry, priv->focused_row, priv->focused_column );

	/* compare the current content with the saved copy of the initial one */
	current = gtk_entry_get_text( entry );
	sdata->modified = ( g_utf8_collate( sdata->initial, current ) != 0 );

	/* reset focus and recursivity indicators */
	priv->on_changed_count = 0;
	priv->focused_row = 0;
	priv->focused_column = 0;

	set_comment( self, "" );

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
on_entry_changed( GtkEntry *entry, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_entry_changed";
	ofaGuidedInputBinPrivate *priv;
	ofsOpeDetail *detail;
	sEntryData *sdata;
	const gchar *cstr;

	priv = ofa_guided_input_bin_get_instance_private( self );

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
				setup_account_label_in_comment( self, sdata->row_id );
				break;
			case OPE_COL_LABEL:
				detail->label = g_strdup( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->label_user_set = TRUE;
				}
				break;
			case OPE_COL_DEBIT:
				detail->debit = ofa_amount_from_str( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->debit_user_set = TRUE;
					g_debug( "%s: row=%d, setting debit_user_set to %s", thisfn, sdata->row_id, detail->debit_user_set ? "True":"False" );
				}
				break;
			case OPE_COL_CREDIT:
				detail->credit = ofa_amount_from_str( cstr );
				if( priv->focused_row == sdata->row_id && priv->focused_column == sdata->col_def->column_id ){
					detail->credit_user_set = TRUE;
				}
				break;
			default:
				break;
		}

		check_for_enable_dlg( self );

	} else {
		g_debug( "%s: field at row=%d, column=%d changed but not checked",
				thisfn, sdata->row_id, sdata->col_def->column_id );
	}

	priv->on_changed_count -= 1;
}

static void
setup_account_label_in_comment( ofaGuidedInputBin *self, gint row_id )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *entry;
	const gchar *acc_number;
	gchar *comment;
	ofoAccount *account;

	priv = ofa_guided_input_bin_get_instance_private( self );

	comment = NULL;

	entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_ACCOUNT, row_id );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( acc_number ){
		account = ofo_account_get_by_number( priv->hub, acc_number );
		if( account && OFO_IS_ACCOUNT( account )){
			comment = g_strdup_printf( "%s - %s ", acc_number, ofo_account_get_label( account ));
		} else {
			comment = g_strdup_printf( "%s", acc_number );
		}
	}
	set_comment( self, comment ? comment : "" );
	g_free( comment );
}

static const sColumnDef *
find_column_def_from_col_id( const ofaGuidedInputBin *self, gint col_id )
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
check_for_enable_dlg( ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	gboolean ok;

	priv = ofa_guided_input_bin_get_instance_private( self );

	if( priv->entries_grid && priv->check_allowed ){

		priv->check_allowed = FALSE;

		ok = is_dialog_validable( self );
		g_signal_emit_by_name( self, "ofa-changed", ok );

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

	priv = ofa_guided_input_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = is_dialog_validable( bin );

	return( ok );
}

/*
 * We do not re-check nor recompute any thing while just moving from a
 * field to another - this would be not only waste of time, but also
 * keep the interface changing while doing anything else that moving
 * the focus...
 */
static gboolean
is_dialog_validable( ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_is_dialog_validable";
	ofaGuidedInputBinPrivate *priv;
	ofsOpe *ope;
	ofsOpeDetail *detail;
	gchar *message, *amount_str;
	gboolean ok;
	gint i;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_guided_input_bin_get_instance_private( self );

	ope = priv->ope;
	ofs_currency_list_free( &priv->currency_list );
	ofs_ope_apply_template( ope );

	/* update the self dialog with the new content of operation */
	for( i=0 ; i<g_list_length( ope->detail ) ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		if( !detail->account_user_set ){
			set_ope_to_ui( self, i+1, OPE_COL_ACCOUNT, detail->account );
		}
		if( !detail->label_user_set ){
			set_ope_to_ui( self, i+1, OPE_COL_LABEL, detail->label );
		}
		if( !detail->debit_user_set ){
			amount_str = ofa_amount_to_str( detail->debit, detail->currency );
			set_ope_to_ui( self, i+1, OPE_COL_DEBIT, amount_str );
			g_free( amount_str );
		}
		if( !detail->credit_user_set ){
			amount_str = ofa_amount_to_str( detail->credit, detail->currency );
			set_ope_to_ui( self, i+1, OPE_COL_CREDIT, amount_str );
			g_free( amount_str );
		}
	}

	ok = ofs_ope_is_valid( ope, &message, &priv->currency_list );
	g_debug( "%s: ofs_ope_is_valid() returns ok=%s", thisfn, ok ? "True":"False" );

	/* update the self dialog with the new content of operation */
	for( i=0 ; i<g_list_length( ope->detail ) ; ++i ){
		detail = ( ofsOpeDetail * ) g_list_nth_data( ope->detail, i );
		display_currency( self, i+1,
				detail );
		draw_valid_coche( self, i+1,
				detail->account_is_valid && detail->label_is_valid && detail->amounts_are_valid );
	}

	update_totals( self );
	set_message( self, message );

	g_free( message );

	return( ok );
}

static void
set_ope_to_ui( const ofaGuidedInputBin *self, gint row, gint col_id, const gchar *content )
{
	ofaGuidedInputBinPrivate *priv;
	const sColumnDef *def;
	GtkWidget *entry, *label;

	priv = ofa_guided_input_bin_get_instance_private( self );

	def = find_column_def_from_col_id( self, col_id );
	g_return_if_fail( def );

	if( content ){
		if( def->column_type == TYPE_ENTRY ){
			entry = gtk_grid_get_child_at( priv->entries_grid, col_id, row );
			if( entry ){
				g_return_if_fail( GTK_IS_ENTRY( entry ));

				if( def->is_double ){
					g_signal_handlers_block_by_func( entry, on_entry_changed, ( gpointer ) self );
					my_double_editable_set_string( GTK_EDITABLE( entry ), content );
					g_signal_handlers_unblock_by_func( entry, on_entry_changed, ( gpointer ) self );

				} else {
					gtk_entry_set_text( GTK_ENTRY( entry ), content );
				}
			}

		} else if( def->column_type == TYPE_LABEL ){
			label = gtk_grid_get_child_at( priv->entries_grid, col_id, row );

			if( label ){
				g_return_if_fail( GTK_IS_LABEL( label ));
				gtk_label_set_text( GTK_LABEL( label ), content );
			}
		}
	}
}

/*
 * display the currency iso code in front of each line
 * only display the currency if different from default currency
 */
static void
display_currency( ofaGuidedInputBin *self, gint row, ofsOpeDetail *detail )
{
	ofaGuidedInputBinPrivate *priv;
	ofoAccount *account;
	const gchar *currency, *display_cur;
	GtkWidget *label;

	priv = ofa_guided_input_bin_get_instance_private( self );

	display_cur = "";

	if( detail->account ){
		account = ofo_account_get_by_number( priv->hub, detail->account );
		if( account ){
			currency = ofo_account_get_currency( account );
			if( currency && g_utf8_collate( currency, priv->def_currency )){
				display_cur = currency;
			}
		}
	}
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CURRENCY, row );
	gtk_label_set_text( GTK_LABEL( label ), display_cur );
}

static void
draw_valid_coche( ofaGuidedInputBin *self, gint row, gboolean bvalid )
{
	ofaGuidedInputBinPrivate *priv;
	const sColumnDef *def;
	GtkWidget *image;

	//g_debug( "draw_valid_coche: row=%d, bvalid=%s", row, bvalid ? "True":"False" );

	priv = ofa_guided_input_bin_get_instance_private( self );

	def = find_column_def_from_col_id( self, OPE_COL_VALID );
	g_return_if_fail( def && def->column_type == TYPE_IMAGE );

	image = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_VALID, row );
	g_return_if_fail( image && GTK_IS_IMAGE( image ));

	if( bvalid ){
		gtk_image_set_from_resource( GTK_IMAGE( image ), st_resource_image_check );
	} else {
		gtk_image_set_from_resource( GTK_IMAGE( image ), st_resource_image_empty );
	}

	gtk_widget_queue_draw( image );
}

static void
set_comment( ofaGuidedInputBin *self, const gchar *comment )
{
	ofaGuidedInputBinPrivate *priv;

	priv = ofa_guided_input_bin_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->comment ), comment );
}

static void
set_message( ofaGuidedInputBin *self, const gchar *errmsg )
{
	ofaGuidedInputBinPrivate *priv;

	priv = ofa_guided_input_bin_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->message ), errmsg );
	my_utils_widget_set_style( priv->message, my_strlen( errmsg ) ? "labelerror" : "labelnormal" );
}

/*
 * do not remove/re-create total/diff lines per currency as this make
 * the display flickering
 * instead:
 * - the 'total xxx' label is reset to the currency value
 * - at the end, the remaining lines are removed
 */
static gboolean
update_totals( ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_update_totals";
	ofaGuidedInputBinPrivate *priv;
	gint model_count, i, needed;
	GList *it;
	ofsCurrency *sbal;
	GtkWidget *label, *entry;
	gboolean ok, oki;
	gdouble ddiff, cdiff;
	gchar *total_str;

	priv = ofa_guided_input_bin_get_instance_private( self );

	if( !priv->model ){
		return( FALSE );
	}

	ok = TRUE;
	model_count = ofo_ope_template_get_detail_count( priv->model );

	g_debug( "%s: model_count=%d, rows_count=%d, currencies_length=%d",
			thisfn, model_count, priv->rows_count, g_list_length( priv->currency_list ));

	for( i=priv->rows_count ; i>=1+model_count ; --i ){
		gtk_grid_remove_row( priv->entries_grid, i );
	}
	priv->rows_count = model_count;

	/* i is the row position (starting at 0 with headers) */
	for( it=priv->currency_list, i=1+model_count ; it ; it=it->next, i+=2 ){

		/* insert total and diff lines */
		if( i > priv->rows_count ){
			add_total_diff_lines( self, i );
		}

		sbal = ( ofsCurrency * ) it->data;

		/* setup currency, totals and diffs */
		label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_LABEL, i );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
		total_str = g_strdup_printf( _( "Total %s :"), ofo_currency_get_code( sbal->currency ));
		gtk_label_set_text( GTK_LABEL( label ), total_str );
		g_free( total_str );

		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_DEBIT, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		my_double_editable_set_amount( GTK_EDITABLE( entry ), sbal->debit );

		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CREDIT, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		my_double_editable_set_amount( GTK_EDITABLE( entry ), sbal->credit );

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

		total_display_diff( self, sbal->currency, i+1, ddiff, cdiff );

		ok &= oki;
	}

	/* at the end, remove the remaining lines */
	needed = model_count + 2*g_list_length( priv->currency_list );
	for( i=priv->rows_count ; i>needed ; --i ){
		gtk_grid_remove_row( priv->entries_grid, i );
	}

	return( ok );
}

/*
 * we insert two lines for total and diff for each used currency
 */
static void
add_total_diff_lines( ofaGuidedInputBin *self, gint row )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label, *entry;

	priv = ofa_guided_input_bin_get_instance_private( self );

	label = gtk_label_new( NULL );
	gtk_widget_set_margin_top( label, TOTAUX_TOP_MARGIN );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_LABEL, row, 1, 1 );

	entry = gtk_entry_new();
	my_double_editable_init_ex( GTK_EDITABLE( entry ),
			g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
			ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), CUR_DEFAULT_DIGITS );
	gtk_widget_set_can_focus( entry, FALSE );
	gtk_widget_set_margin_top( entry, TOTAUX_TOP_MARGIN );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), AMOUNTS_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, entry, OPE_COL_DEBIT, row, 1, 1 );

	entry = gtk_entry_new();
	my_double_editable_init_ex( GTK_EDITABLE( entry ),
			g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
			ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), CUR_DEFAULT_DIGITS );
	gtk_widget_set_can_focus( entry, FALSE );
	gtk_widget_set_margin_top( entry, TOTAUX_TOP_MARGIN );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), AMOUNTS_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), AMOUNTS_WIDTH );
	gtk_grid_attach( priv->entries_grid, entry, OPE_COL_CREDIT, row, 1, 1 );

	label = gtk_label_new( _( "Diff :" ));
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_LABEL, row+1, 1, 1 );

	label = gtk_label_new( NULL );
	my_utils_widget_set_xalign( label, 1.0 );
	my_utils_widget_set_margin_right( label, 2 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_DEBIT, row+1, 1, 1 );

	label = gtk_label_new( NULL );
	my_utils_widget_set_xalign( label, 1.0 );
	my_utils_widget_set_margin_right( label, 2 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_CREDIT, row+1, 1, 1 );

	label = gtk_label_new( NULL );
	my_utils_widget_set_xalign( label, 0 );
	gtk_grid_attach( priv->entries_grid, label, OPE_COL_CURRENCY, row+1, 1, 1 );

	gtk_widget_show_all( GTK_WIDGET( priv->entries_grid ));
	priv->rows_count += 2;
}

static void
total_display_diff( ofaGuidedInputBin *self, ofoCurrency *currency, gint row, gdouble ddiff, gdouble cdiff )
{
	ofaGuidedInputBinPrivate *priv;
	GtkWidget *label;
	gchar *amount_str;
	gboolean has_diff;

	priv = ofa_guided_input_bin_get_instance_private( self );

	has_diff = FALSE;

	/* set the debit diff amount (or empty) */
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_DEBIT, row );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	amount_str = NULL;
	if( ddiff > 0.001 ){
		amount_str = ofa_amount_to_str( ddiff, currency );
		has_diff = TRUE;
	}
	gtk_label_set_text( GTK_LABEL( label ), amount_str );
	g_free( amount_str );
	my_utils_widget_set_style( label, "labelerror" );

	/* set the credit diff amount (or empty) */
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CREDIT, row );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	amount_str = NULL;
	if( cdiff > 0.001 ){
		amount_str = ofa_amount_to_str( cdiff, currency );
		has_diff = TRUE;
	}
	gtk_label_set_text( GTK_LABEL( label ), amount_str );
	g_free( amount_str );
	my_utils_widget_set_style( label, "labelerror" );

	/* set the currency */
	label = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CURRENCY, row );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), has_diff ? ofo_currency_get_label( currency ) : "" );
	my_utils_widget_set_style( label, "labelerror" );
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

	priv = ofa_guided_input_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = FALSE;

	if( do_validate( bin )){
		ok = TRUE;
		do_reset_entries_rows( bin );
	}

	return( ok );
}

static gboolean
do_validate( ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	gboolean ok;
	GList *entries, *it;

	priv = ofa_guided_input_bin_get_instance_private( self );

	ok = TRUE;
	entries = ofs_ope_generate_entries( priv->ope );

	for( it=entries ; it ; it=it->next ){
		ok &= ofo_entry_insert( OFO_ENTRY( it->data ), priv->hub );
		/* TODO:
		 * in case of an error, remove the already recorded entries
		 * of the list, decrementing the ledgers and the accounts
		 * then restore the last ecr number of the dossier
		 */
	}
	if( ok ){
		display_ok_message( self, g_list_length( entries ));
	}

	g_list_free_full( entries, g_object_unref );

	my_date_set_from_date( &st_last_dope, &priv->ope->dope );
	my_date_set_from_date( &st_last_deff, &priv->ope->deffect );

	return( ok );
}

static void
display_ok_message( ofaGuidedInputBin *self, gint count )
{
	ofaGuidedInputBinPrivate *priv;
	gchar *message;

	priv = ofa_guided_input_bin_get_instance_private( self );

	message = g_strdup_printf( _( "%d entries have been successfully created" ), count );

	my_utils_msg_dialog( GTK_WINDOW( priv->main_window ), GTK_MESSAGE_INFO, message );

	g_free( message );
}

/**
 * ofa_guided_input_bin_reset:
 *
 * Reset the input fields, keeping the dates and the same entry model
 */
void
ofa_guided_input_bin_reset( ofaGuidedInputBin *bin )
{
	ofaGuidedInputBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_GUIDED_INPUT_BIN( bin ));

	priv = ofa_guided_input_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	do_reset_entries_rows( bin );
}


/*
 * nb: entries_count = count of entries + 2 (for totals and diff)
 * Only the LABEL entries may be non present on the last two lines
 */
static void
do_reset_entries_rows( ofaGuidedInputBin *self )
{
	ofaGuidedInputBinPrivate *priv;
	gint i, model_count;
	GtkWidget *entry;
	ofsOpeDetail *detail;

	priv = ofa_guided_input_bin_get_instance_private( self );

	model_count = ofo_ope_template_get_detail_count( priv->model );
	for( i=priv->rows_count ; i>=1+model_count ; --i ){
		gtk_grid_remove_row( priv->entries_grid, i );
	}
	priv->rows_count = model_count;

	for( i=1 ; i<=priv->rows_count ; ++i ){
		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_DEBIT, i );
		if( entry && GTK_IS_ENTRY( entry )){
			g_signal_handlers_block_by_func( entry, on_entry_changed, self );
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
			g_signal_handlers_unblock_by_func( entry, on_entry_changed, self );
		}
		entry = gtk_grid_get_child_at( priv->entries_grid, OPE_COL_CREDIT, i );
		if( entry && GTK_IS_ENTRY( entry )){
			g_signal_handlers_block_by_func( entry, on_entry_changed, self );
			gtk_entry_set_text( GTK_ENTRY( entry ), "" );
			g_signal_handlers_unblock_by_func( entry, on_entry_changed, self );
		}
		draw_valid_coche( self, i, FALSE );

		detail = ( ofsOpeDetail * ) g_list_nth_data( priv->ope->detail, i-1 );
		detail->debit = 0;
		detail->debit_user_set = FALSE;
		detail->credit = 0;
		detail->credit_user_set = FALSE;
	}

	check_for_enable_dlg( self );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( const ofaHub *hub, const ofoBase *object, const gchar *prev_id, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_hub_updated_object";
	ofaGuidedInputBinPrivate *priv;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	priv = ofa_guided_input_bin_get_instance_private( self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( OFO_OPE_TEMPLATE( object ) == priv->model ){
			ofa_guided_input_bin_set_ope_template( self, OFO_OPE_TEMPLATE( object ));
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( const ofaHub *hub, const ofoBase *object, ofaGuidedInputBin *self )
{
	static const gchar *thisfn = "ofa_guided_input_bin_on_hub_deleted_object";
	ofaGuidedInputBinPrivate *priv;
	gint i;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	priv = ofa_guided_input_bin_get_instance_private( self );

	if( OFO_IS_OPE_TEMPLATE( object )){
		if( OFO_OPE_TEMPLATE( object ) == priv->model ){

			for( i=1 ; i<=priv->rows_count ; ++i ){
				gtk_grid_remove_row( priv->entries_grid, i );
			}
			priv->model = NULL;
			priv->rows_count = 0;
		}
	}
}
