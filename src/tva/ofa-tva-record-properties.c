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
#include <stdarg.h>
#include <stdlib.h>

#include "my/my-date-editable.h"
#include "my/my-double-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-formula-engine.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-account-balance.h"

#include "tva/ofa-tva-record-properties.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaIGetter   *getter;
	ofoTVARecord *tva_record;

	/* internals
	 */
	gboolean      is_current;

	/* UI
	 */
	GtkWidget    *label_entry;
	GtkWidget    *begin_editable;
	GtkWidget    *end_editable;
	GtkWidget    *boolean_grid;
	GtkWidget    *detail_grid;
	GtkWidget    *textview;
	GtkWidget    *compute_btn;
	GtkWidget    *validate_btn;
	GtkWidget    *ok_btn;
	GtkWidget    *msg_label;

	/* runtime data
	 */
	GDate         init_end_date;
	gchar        *mnemo;
	GDate         begin_date;
	GDate         end_date;
	gboolean      has_correspondence;
	gboolean      is_validated;
}
	ofaTVARecordPropertiesPrivate;

static gboolean st_debug                = TRUE;
#define DEBUG                           if( st_debug ) g_debug

enum {
	BOOL_COL_LABEL = 0,
	DET_COL_CODE = 1,
	DET_COL_LABEL,
	DET_COL_BASE,
	DET_COL_AMOUNT,
	DET_COL_PADDING
};

/* sEvalDef:
 * @name: the name of the function.
 * @args_count: the expected count of arguments, -1 for do not check.
 * @eval: the evaluation function which provides the replacement string.
 *
 * Defines the evaluation callback functions.
 */
typedef struct {
	const gchar *name;
	gint         args_count;
	gchar *   ( *eval )( ofsFormulaHelper * );
}
	sEvalDef;

static ofaFormulaEngine *st_engine      = NULL;

static const gchar      *st_resource_ui = "/org/trychlos/openbook/tva/ofa-tva-record-properties.ui";

static void             iwindow_iface_init( myIWindowInterface *iface );
static gchar           *iwindow_get_identifier( const myIWindow *instance );
static void             idialog_iface_init( myIDialogInterface *iface );
static void             idialog_init( myIDialog *instance );
static void             init_properties( ofaTVARecordProperties *self );
static void             init_booleans( ofaTVARecordProperties *self );
static void             init_taxes( ofaTVARecordProperties *self );
static void             init_correspondence( ofaTVARecordProperties *self );
static void             on_begin_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void             on_end_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void             on_boolean_toggled( GtkToggleButton *button, ofaTVARecordProperties *self );
static void             on_detail_base_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void             on_detail_amount_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void             check_for_enable_dlg( ofaTVARecordProperties *self );
static void             set_dialog_title( ofaTVARecordProperties *self );
static gboolean         do_update( ofaTVARecordProperties *self, gchar **msgerr );
static void             on_compute_clicked( GtkButton *button, ofaTVARecordProperties *self );
static ofaFormulaEvalFn get_formula_eval_fn( const gchar *name, gint *count, GMatchInfo *match_info, ofaTVARecordProperties *self );
static gchar           *eval_account( ofsFormulaHelper *helper );
static gchar           *eval_amount( ofsFormulaHelper *helper );
static gchar           *eval_base( ofsFormulaHelper *helper );
static gchar           *eval_code( ofsFormulaHelper *helper );
static void             on_validate_clicked( GtkButton *button, ofaTVARecordProperties *self );
static void             set_msgerr( ofaTVARecordProperties *self, const gchar *msg );

static const sEvalDef st_formula_fns[] = {
		{ "ACCOUNT", 1, eval_account },
		{ "AMOUNT",  1, eval_amount },
		{ "BASE",    1, eval_base },
		{ "CODE",    1, eval_code },
		{ 0 }
};

G_DEFINE_TYPE_EXTENDED( ofaTVARecordProperties, ofa_tva_record_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaTVARecordProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
tva_record_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_properties_finalize";
	ofaTVARecordPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( instance ));

	g_free( priv->mnemo );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_properties_parent_class )->finalize( instance );
}

static void
tva_record_properties_dispose( GObject *instance )
{
	ofaTVARecordPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_PROPERTIES( instance ));

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_properties_parent_class )->dispose( instance );
}

static void
ofa_tva_record_properties_init( ofaTVARecordProperties *self )
{
	static const gchar *thisfn = "ofa_tva_record_properties_init";
	ofaTVARecordPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_RECORD_PROPERTIES( self ));

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_tva_record_properties_class_init( ofaTVARecordPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_tva_record_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 * @record: the #ofoTVARecord to be displayed/updated.
 *
 * Update the properties of a tva_form.
 */
void
ofa_tva_record_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoTVARecord *record )
{
	static const gchar *thisfn = "ofa_tva_record_properties_run";
	ofaTVARecordProperties *self;
	ofaTVARecordPropertiesPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	g_debug( "%s: getter=%p, parent=%p, record=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) record );

	self = g_object_new( OFA_TYPE_TVA_RECORD_PROPERTIES, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->getter = getter;
	priv->tva_record = record;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_record_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_identifier = iwindow_get_identifier;
}

/*
 * identifier is built with class name and VAT record mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_tva_record_get_mnemo( priv->tva_record ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_record_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * VAT record
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_tva_record_properties_idialog_init";
	ofaTVARecordPropertiesPrivate *priv;
	ofoDossier *dossier;
	ofaHub *hub;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	priv->compute_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "compute-btn" );
	g_return_if_fail( priv->compute_btn && GTK_IS_BUTTON( priv->compute_btn ));
	g_signal_connect( priv->compute_btn, "clicked", G_CALLBACK( on_compute_clicked ), instance );

	priv->validate_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "validate-btn" );
	g_return_if_fail( priv->validate_btn && GTK_IS_BUTTON( priv->validate_btn ));
	g_signal_connect( priv->validate_btn, "clicked", G_CALLBACK( on_validate_clicked ), instance );

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->is_current = ofo_dossier_is_current( dossier );

	my_date_set_from_date( &priv->init_end_date, ofo_tva_record_get_end( priv->tva_record ));

	init_properties( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_booleans( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_taxes( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_correspondence( OFA_TVA_RECORD_PROPERTIES( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	set_dialog_title( OFA_TVA_RECORD_PROPERTIES( instance ));
	check_for_enable_dlg( OFA_TVA_RECORD_PROPERTIES( instance ));
}

static void
init_properties( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *entry, *label, *button;
	const gchar *cstr;

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->is_validated = ofo_tva_record_get_is_validated( priv->tva_record );

	/* mnemonic: invariant */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->mnemo = g_strdup( ofo_tva_record_get_mnemo( priv->tva_record ));
	g_return_if_fail( my_strlen( priv->mnemo ));
	gtk_entry_set_text( GTK_ENTRY( entry ), priv->mnemo );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* label */
	priv->label_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( priv->label_entry && GTK_IS_ENTRY( priv->label_entry ));
	cstr = ofo_tva_record_get_label( priv->tva_record );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), cstr );
	}
	my_utils_widget_set_editable( priv->label_entry, priv->is_current );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* has correspondence: invariant */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-has-corresp" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( button ),
			ofo_tva_record_get_has_correspondence( priv->tva_record ));
	my_utils_widget_set_editable( button, FALSE );

	/* is validated: invariant */
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-validated" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), priv->is_validated );
	my_utils_widget_set_editable( button, FALSE );

	/* begin date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->begin_editable = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-begin-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());

	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_changed ), self );

	my_date_set_from_date( &priv->begin_date, ofo_tva_record_get_begin( priv->tva_record ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin_date );
	my_utils_widget_set_editable( entry, priv->is_current && !priv->is_validated );

	/* do not let the user edit the ending date of the declaration
	 * because this is a key of the record
	 * if the ending date has to be modified, then the user should
	 * create a new declaration
	 */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->end_editable = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());

	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );

	my_date_set_from_date( &priv->end_date, ofo_tva_record_get_end( priv->tva_record ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end_date );
	my_utils_widget_set_editable( entry, FALSE );
}

static void
init_booleans( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *grid, *button;
	guint idx, count, row;
	const gchar *cstr;
	gboolean is_true;

	priv = ofa_tva_record_properties_get_instance_private( self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-grid" );
	priv->boolean_grid = grid;

	count = ofo_tva_record_boolean_get_count( priv->tva_record );
	for( idx=0 ; idx<count ; ++idx ){
		row = idx;
		cstr = ofo_tva_record_boolean_get_label( priv->tva_record, idx );
		button = gtk_check_button_new_with_label( cstr );
		my_utils_widget_set_editable( button, priv->is_current && !priv->is_validated );
		gtk_grid_attach( GTK_GRID( grid ), button, BOOL_COL_LABEL, row, 1, 1 );
		g_signal_connect( button, "toggled", G_CALLBACK( on_boolean_toggled ), self );
		is_true = ofo_tva_record_boolean_get_is_true( priv->tva_record, idx );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), is_true );
	}
}

static void
init_taxes( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *grid, *label, *entry;
	guint idx, count, row;
	const gchar *cstr;
	gchar *str;
	gboolean has_base, has_amount;
	ofxAmount amount;

	priv = ofa_tva_record_properties_get_instance_private( self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-grid" );
	priv->detail_grid = grid;

	count = ofo_tva_record_detail_get_count( priv->tva_record );
	for( idx=0 ; idx<count ; ++idx ){
		row = idx+1;

		label = gtk_label_new( NULL );
		gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
		my_utils_widget_set_margins( label, 0, 0, 0, 4 );
		my_utils_widget_set_xalign( label, 1.0 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, row, 1, 1 );
		str = g_strdup_printf( "<i>%u</i>", row );
		gtk_label_set_markup( GTK_LABEL( label ), str );
		g_free( str );

		/* code */
		entry = gtk_entry_new();
		my_utils_widget_set_editable( entry, FALSE );
		gtk_entry_set_width_chars( GTK_ENTRY( entry ), 4 );
		gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 4 );
		gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_CODE, row, 1, 1 );

		cstr = ofo_tva_record_detail_get_code( priv->tva_record, idx );
		gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );

		/* label */
		entry = gtk_entry_new();
		my_utils_widget_set_editable( entry, FALSE );
		gtk_widget_set_hexpand( entry, TRUE );
		gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_LABEL, row, 1, 1 );

		cstr = ofo_tva_record_detail_get_label( priv->tva_record, idx );
		gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );

		/* base */
		has_base = ofo_tva_record_detail_get_has_base( priv->tva_record, idx );
		if( has_base ){
			entry = gtk_entry_new();
			my_utils_widget_set_editable( entry, priv->is_current && !priv->is_validated );
			my_double_editable_init_ex( GTK_EDITABLE( entry ),
					g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
					ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), 0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_BASE, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_base_changed ), self );

			gtk_widget_set_tooltip_text(
					entry, ofo_tva_record_detail_get_base_rule( priv->tva_record, idx ));

			amount = ofo_tva_record_detail_get_base( priv->tva_record, idx );
			my_double_editable_set_amount( GTK_EDITABLE( entry ), amount );
		}

		/* amount */
		has_amount = ofo_tva_record_detail_get_has_amount( priv->tva_record, idx );
		if( has_amount ){
			entry = gtk_entry_new();
			my_utils_widget_set_editable( entry, priv->is_current && !priv->is_validated );
			my_double_editable_init_ex( GTK_EDITABLE( entry ),
					g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
					ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), 0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_AMOUNT, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_amount_changed ), self );

			gtk_widget_set_tooltip_text(
					entry, ofo_tva_record_detail_get_amount_rule( priv->tva_record, idx ));

			amount = ofo_tva_record_detail_get_amount( priv->tva_record, idx );
			my_double_editable_set_amount( GTK_EDITABLE( entry ), amount );
		}

		/* padding on the right so that the scrollbar does not hide the
		 * amount */
		label = gtk_label_new( "   " );
		gtk_grid_attach( GTK_GRID( grid ), label, DET_COL_PADDING, row, 1, 1 );
	}
}

static void
init_correspondence( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *book, *label, *scrolled;
	const gchar *cstr;

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->has_correspondence = ofo_tva_record_get_has_correspondence( priv->tva_record );
	if( priv->has_correspondence ){
		book = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tva-book" );
		g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));
		label = gtk_label_new_with_mnemonic( _( "_Correspondence" ));
		scrolled = gtk_scrolled_window_new( NULL, NULL );
		gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scrolled ), GTK_SHADOW_IN );
		gtk_notebook_append_page( GTK_NOTEBOOK( book ), scrolled, label );
		priv->textview = gtk_text_view_new();
		gtk_container_add( GTK_CONTAINER( scrolled ), priv->textview );

		cstr = ofo_tva_record_get_notes( priv->tva_record );
		my_utils_container_notes_setup_ex( GTK_TEXT_VIEW( priv->textview ), cstr, TRUE );
	}
}

static void
on_begin_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	my_date_set_from_date( &priv->begin_date, my_date_editable_get_date( entry, NULL ));

	check_for_enable_dlg( self );
}

static void
on_end_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	my_date_set_from_date( &priv->end_date, my_date_editable_get_date( entry, NULL ));

	set_dialog_title( self );
	check_for_enable_dlg( self );
}

static void
on_boolean_toggled( GtkToggleButton *button, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_detail_base_changed( GtkEntry *entry, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_detail_amount_changed( GtkEntry *entry, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
}

/*
 * must have both begin and end dates to validate it
 */
static void
check_for_enable_dlg( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	gboolean is_valid, is_validated, end_date_has_changed, exists, is_validable;
	const gchar *mnemo;
	const GDate *dend;
	gchar *msgerr;
	ofaHub *hub;

	priv = ofa_tva_record_properties_get_instance_private( self );

	msgerr = NULL;
	hub = ofa_igetter_get_hub( priv->getter );

	if( priv->is_current ){

		is_valid = ofo_tva_record_is_valid_data( priv->mnemo, &priv->begin_date, &priv->end_date, &msgerr );

		if( is_valid ){
			/* the ending date is no more modifiable */
			if( 0 ){
				dend = ofo_tva_record_get_end( priv->tva_record );
				end_date_has_changed = my_date_compare( &priv->init_end_date, dend ) != 0;
				if( end_date_has_changed ){
					mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
					exists = ( ofo_tva_record_get_by_key( hub, mnemo, dend ) != NULL );
					if( exists ){
						set_msgerr( self, _( "Same declaration is already defined" ));
						is_valid = FALSE;
					}
				}
			}
		}

		gtk_widget_set_sensitive( priv->ok_btn, is_valid );

		is_validated = ofo_tva_record_get_is_validated( priv->tva_record );
		is_validable = ofo_tva_record_is_validable_by_data( priv->mnemo, &priv->begin_date, &priv->end_date );

		gtk_widget_set_sensitive(
				priv->compute_btn,
				priv->is_current && is_valid && is_validable );

		gtk_widget_set_sensitive(
				priv->validate_btn,
				priv->is_current && is_valid && !is_validated && is_validable );
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );
}

/*
 * Update dialog title each time the end date is changed
 * (the mnemonic is an invariant)
 */
static void
set_dialog_title( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *send, *title;

	priv = ofa_tva_record_properties_get_instance_private( self );

	send = my_date_to_str( &priv->end_date, MY_DATE_SQL );
	title = g_strdup_printf( _( "Updating « %s - %s » TVA declaration" ), priv->mnemo, send );
	gtk_window_set_title( GTK_WINDOW( self ), title );
	g_free( title );
	g_free( send );
}

/*
 * Recording the updates done to the declaration
 * The record is uniquely identified by the mnemo + the end date
 * Though the mnemo is an invariant, the end date may have been changed.
 * If this is the case, then the original record must be deleted and the
 * new one be re-inserted.
 */
static gboolean
do_update( ofaTVARecordProperties *self, gchar **msgerr )
{
	ofaTVARecordPropertiesPrivate *priv;
	guint idx, row, count;
	GtkWidget *button, *entry;
	const gchar *clabel;
	gboolean ok, is_true;
	ofxAmount amount;

	priv = ofa_tva_record_properties_get_instance_private( self );

	if( priv->has_correspondence ){
		my_utils_container_notes_get_ex( GTK_TEXT_VIEW( priv->textview ), tva_record );
	}

	ofo_tva_record_set_label( priv->tva_record,
			gtk_entry_get_text( GTK_ENTRY( priv->label_entry )));

	ofo_tva_record_set_begin( priv->tva_record,
			my_date_editable_get_date( GTK_EDITABLE( priv->begin_editable ), NULL ));

	ofo_tva_record_set_end( priv->tva_record,
			my_date_editable_get_date( GTK_EDITABLE( priv->end_editable ), NULL ));

	count = ofo_tva_record_boolean_get_count( priv->tva_record );
	ofo_tva_record_boolean_free_all( priv->tva_record );
	for( idx=0, row=0 ; idx<count ; ++idx, ++row ){
		button = gtk_grid_get_child_at( GTK_GRID( priv->boolean_grid ), BOOL_COL_LABEL, row );
		g_return_val_if_fail( button && GTK_IS_CHECK_BUTTON( button ), FALSE );
		clabel = gtk_button_get_label( GTK_BUTTON( button ));
		is_true = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button ));
		ofo_tva_record_boolean_add( priv->tva_record, clabel, is_true );
	}

	count = ofo_tva_record_detail_get_count( priv->tva_record );
	for( idx=0, row=1 ; idx<count ; ++idx, ++row ){
		if( ofo_tva_record_detail_get_has_base( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_BASE, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
			amount = my_double_editable_get_amount( GTK_EDITABLE( entry ));
			ofo_tva_record_detail_set_base( priv->tva_record, idx, amount );
		}
		if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
			amount = my_double_editable_get_amount( GTK_EDITABLE( entry ));
			ofo_tva_record_detail_set_amount( priv->tva_record, idx, amount );
		}
	}

	ok = ofo_tva_record_update( priv->tva_record );
	if( !ok ){
		*msgerr = g_strdup( _( "Unable to update the VAT declaration" ));
	}

	return( ok );
}

/*
 * Compute the declaration on demand
 */
static void
on_compute_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *dialog, *entry;
	gint resp;
	guint idx, row, count;
	const gchar *rule;
	gchar *result;
	ofxAmount amount;

	priv = ofa_tva_record_properties_get_instance_private( self );

	dialog = gtk_message_dialog_new(
					GTK_WINDOW( self ),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_NONE,
					_( "Caution: computing the declaration will erase all possible"
							"manual modifications you may have done.\n"
							"Are your sure you want this ?" ));
	gtk_dialog_add_buttons(
			GTK_DIALOG( dialog ),
			_( "Cancel" ), GTK_RESPONSE_CANCEL,
			_( "Compute" ), GTK_RESPONSE_OK,
			NULL );
	resp = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	if( resp == GTK_RESPONSE_OK ){
		if( !st_engine ){
			st_engine = ofa_formula_engine_new();
		}
		count = ofo_tva_record_detail_get_count( priv->tva_record );

		for( idx=0, row=1 ; idx<count ; ++idx, ++row ){
			if( ofo_tva_record_detail_get_has_base( priv->tva_record, idx )){
				rule = ofo_tva_record_detail_get_base_rule( priv->tva_record, idx );
				if( my_strlen( rule )){
					result = ofa_formula_engine_eval( st_engine, rule, ( ofaFormulaFindFn ) get_formula_eval_fn, self, NULL );
					entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_BASE, row );
					g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
					my_double_editable_set_string( GTK_EDITABLE( entry ), result );
					g_free( result );
					amount = my_double_editable_get_amount( GTK_EDITABLE( entry ));
					ofo_tva_record_detail_set_base( priv->tva_record, idx, amount );
				}
			}
			if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
				rule = ofo_tva_record_detail_get_amount_rule( priv->tva_record, idx );
				if( my_strlen( rule )){
					result = ofa_formula_engine_eval( st_engine, rule, ( ofaFormulaFindFn ) get_formula_eval_fn, self, NULL );
					entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
					g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
					my_double_editable_set_string( GTK_EDITABLE( entry ), result );
					g_free( result );
					amount = my_double_editable_get_amount( GTK_EDITABLE( entry ));
					ofo_tva_record_detail_set_amount( priv->tva_record, idx, amount );
				}
			}
		}
	}
}

/*
 * this is a ofaFormula callback
 * Returns: the evaluation function for the name + expected args count
 */
static ofaFormulaEvalFn
get_formula_eval_fn( const gchar *name, gint *count, GMatchInfo *match_info, ofaTVARecordProperties *self )
{
	static const gchar *thisfn = "ofa_tva_record_properties_get_formula_eval_fn";
	gint i;

	*count = -1;

	for( i=0 ; st_formula_fns[i].name ; ++i ){
		if( !my_collate( st_formula_fns[i].name, name )){
			*count = st_formula_fns[i].args_count;
			g_debug( "%s: found name=%s, expected args count=%d", thisfn, name, *count );
			return(( ofaFormulaEvalFn ) st_formula_fns[i].eval );
		}
	}

	return( NULL );
}

/*
 * %ACCOUNT(begin[;end])
 * Returns: the rough+validated balances for the begin[;end] account(s)
 */
static gchar *
eval_account( ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_account";
	ofaTVARecordPropertiesPrivate *priv;
	gchar *res;
	GList *it, *dataset;
	const gchar *cstr;
	gchar **tokens, **iter;
	gchar *begin, *end;
	ofaHub *hub;
	ofxAmount amount;
	ofsAccountBalance  *sbal;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( helper->user_data ));

	res = NULL;
	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	tokens = cstr ? g_strsplit( cstr, OFA_FORMULA_ARG_SEP, 2 ) : NULL;

	/* extract begin and end accounts */
	iter = tokens;
	begin = NULL;
	end = NULL;
	if( *iter ){
		begin = g_strdup( *iter );
		iter++;
		if( *iter ){
			end = g_strdup( *iter );
		}
	}
	g_strfreev( tokens );
	if( !end ){
		end = g_strdup( begin );
	}
	DEBUG( "%s: begin=%s, end=%s", thisfn, begin, end );

	hub = ofa_igetter_get_hub( priv->getter );

	dataset = ofo_entry_get_dataset_balance_rough_validated(
					hub, begin, end, &priv->begin_date, &priv->end_date );
	amount = 0;
	for( it=dataset ; it ; it=it->next ){
		sbal = ( ofsAccountBalance * ) it->data;
		/* credit is -, debit is + */
		amount -= sbal->credit;
		amount += sbal->debit;
	}

	g_free( begin );
	g_free( end );

	res = ofa_amount_to_str( amount, NULL );

	DEBUG( "%s: ACCOUNT(%s)=%s", thisfn, cstr, res );

	return( res );
}

/*
 * %AMOUNT(i])
 * Returns: the amount found at row i.
 */
static gchar *
eval_amount( ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_amount";
	ofaTVARecordPropertiesPrivate *priv;
	gchar *res;
	GList *it;
	const gchar *cstr;
	ofxAmount amount;
	gint row;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( helper->user_data ));

	res = NULL;
	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	row = cstr ? atoi( cstr ) : 0;
	if( row > 0 && ofo_tva_record_detail_get_has_amount( priv->tva_record, row-1 )){
		amount = ofo_tva_record_detail_get_amount( priv->tva_record, row-1 );
		res = ofa_amount_to_str( amount, NULL );
	}

	DEBUG( "%s: cstr=%s, res=%s", thisfn, cstr, res );

	return( res );
}

/*
 * %BASE(i)
 * Returns: the base amount found at row i.
 */
static gchar *
eval_base( ofsFormulaHelper *helper )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *res;
	GList *it;
	const gchar *cstr;
	gint row;
	ofxAmount amount;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( helper->user_data ));

	res = NULL;
	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	row = cstr ? atoi( cstr ) : 0;
	if( row > 0 && ofo_tva_record_detail_get_has_base( priv->tva_record, row-1 )){
		amount = ofo_tva_record_detail_get_base( priv->tva_record, row-1 );
		res = ofa_amount_to_str( amount, NULL );
	}

	return( res );
}

/*
 * %CODE(s)
 * Returns: the row number which holds the code
 */
static gchar *
eval_code( ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_code";
	ofaTVARecordPropertiesPrivate *priv;
	gchar *res;
	GList *it;
	const gchar *cstr, *code;
	guint i, count;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( helper->user_data ));

	res = NULL;
	it = helper->args_list;
	cstr = it ? ( const gchar * ) it->data : NULL;

	count = ofo_tva_record_detail_get_count( priv->tva_record );

	for( i=0 ; i<count ; ++i ){
		code = ofo_tva_record_detail_get_code( priv->tva_record, i );
		if( !my_collate( code, cstr )){
			res = g_strdup_printf( "%u", i+1 );
			break;
		}
	}

	DEBUG( "%s: cstr=%s, res=%s", thisfn, cstr, res );

	return( res );
}

/*
 * Validating is actually same than recording;
 * just the 'validated' flag is set
 */
static void
on_validate_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *msgerr;

	priv = ofa_tva_record_properties_get_instance_private( self );

	ofo_tva_record_set_is_validated( priv->tva_record, TRUE );

	if( do_update( self, &msgerr )){
		my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_INFO,
				_( "The VAT declaration has been successfully validated." ));
		/* close the Properties dialog box
		 * with Cancel for not trigger another update */
		my_iwindow_close( MY_IWINDOW( self ));

	} else {
		my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}
}

static void
set_msgerr( ofaTVARecordProperties *self, const gchar *msg )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	if( !priv->msg_label ){
		priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
		my_utils_widget_set_style( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
