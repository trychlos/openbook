/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-formula-engine.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofa-operation-group.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-counters.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-account-balance.h"
#include "api/ofs-ope.h"

#include "tva/ofa-tva-record-properties.h"
#include "tva/ofa-tva-style.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaIGetter   *getter;
	GtkWindow    *parent;
	ofoTVARecord *tva_record;

	/* runtime
	 */
	GtkWindow    *actual_parent;
	gboolean      initialized;
	ofaTVAStyle  *style_provider;
	gboolean      is_writable;						/* whether the dossier is writable */
	gboolean      is_validated;						/* whether the VAT record is updatable */
	gboolean      is_new;
	gboolean      is_dirty;
	GDate         dope_init;

	/* UI
	 */
	GtkWidget    *begin_editable;
	GtkWidget    *end_editable;
	GtkWidget    *dope_editable;
	GtkWidget    *generated_label;
	GtkWidget    *boolean_grid;
	GtkWidget    *detail_grid;
	GtkWidget    *corresp_textview;
	GtkWidget    *notes_textview;
	GtkWidget    *compute_btn;
	GtkWidget    *generate_btn;
	GtkWidget    *viewopes_btn;
	GtkWidget    *delopes_btn;
	GtkWidget    *ok_btn;
	GtkWidget    *cancel_btn;
	GtkWidget    *msg_label;

	/* data
	 */
	GDate         init_end_date;
	gchar        *mnemo;
	gchar        *label;
	GDate         begin_date;
	GDate         end_date;
	GDate         dope_date;
	gboolean      has_correspondence;
	ofeVatStatus  status;
	GList        *generated_opes;
	GList        *generated_entries;
}
	ofaTVARecordPropertiesPrivate;

static gboolean st_debug                = TRUE;
#define DEBUG                           if( st_debug ) g_debug

enum {
	BOOL_COL_LABEL = 0,
	DET_COL_CODE = 0,
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
	gint         min_args;
	gint         max_args;
	gchar *   ( *eval )( ofsFormulaHelper * );
}
	sEvalDef;

static ofaFormulaEngine *st_engine      = NULL;

static const gchar      *st_resource_ui = "/org/trychlos/openbook/vat/ofa-tva-record-properties.ui";

static void             iwindow_iface_init( myIWindowInterface *iface );
static void             iwindow_init( myIWindow *instance );
static void             idialog_iface_init( myIDialogInterface *iface );
static void             idialog_init( myIDialog *instance );
static void             init_ui( ofaTVARecordProperties *self );
static void             init_properties( ofaTVARecordProperties *self );
static void             init_booleans( ofaTVARecordProperties *self );
static void             init_taxes( ofaTVARecordProperties *self );
static void             init_correspondence( ofaTVARecordProperties *self );
static void             init_editability( ofaTVARecordProperties *self );
static void             init_notes( ofaTVARecordProperties *self );
static void             on_label_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void             on_begin_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void             on_end_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void             on_dope_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void             on_corresp_changed( GtkTextBuffer *buffer, ofaTVARecordProperties *self );
static void             on_notes_changed( GtkTextBuffer *buffer, ofaTVARecordProperties *self );
static void             on_boolean_toggled( GtkToggleButton *button, ofaTVARecordProperties *self );
static void             on_detail_base_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void             on_detail_amount_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void             check_for_enable_dlg( ofaTVARecordProperties *self );
static void             set_dialog_title( ofaTVARecordProperties *self );
static void             set_dirty( ofaTVARecordProperties *self, gboolean dirty );
static void             setup_tva_record( ofaTVARecordProperties *self );
static void             on_ok_clicked( ofaTVARecordProperties *self );
static gboolean         do_update_dbms( ofaTVARecordProperties *self, gchar **msgerr );
static gboolean         do_update_dope( ofaTVARecordProperties *self, gboolean force, gchar **msgerr );
static void             on_compute_clicked( GtkButton *button, ofaTVARecordProperties *self );
static ofaFormulaEvalFn get_formula_eval_fn( const gchar *name, gint *min_count, gint *max_count, GMatchInfo *match_info, ofaTVARecordProperties *self );
static gchar           *eval_account( ofsFormulaHelper *helper );
static gchar           *eval_amount( ofsFormulaHelper *helper );
static gchar           *eval_balance( ofsFormulaHelper *helper );
static gchar           *eval_base( ofsFormulaHelper *helper );
static gchar           *eval_code( ofsFormulaHelper *helper );
static void             on_generate_clicked( GtkButton *button, ofaTVARecordProperties *self );
static gboolean         do_generate_opes( ofaTVARecordProperties *self, gchar **msgerr, guint *ope_count, guint *ent_count );
static void             on_generated_opes_changed( ofaTVARecordProperties *self );
static GList           *get_accounting_entries( ofaTVARecordProperties *self );
static void             on_viewopes_clicked( GtkButton *button, ofaTVARecordProperties *self );
static void             on_delopes_clicked( GtkButton *button, ofaTVARecordProperties *self );
static gboolean         delopes_user_confirm( ofaTVARecordProperties *self );
static void             set_msgerr( ofaTVARecordProperties *self, const gchar *msg );
static void             set_msgwarn( ofaTVARecordProperties *self, const gchar *msg );

static const sEvalDef st_formula_fns[] = {
		{ "ACCOUNT", 1, 2, eval_account },
		{ "AMOUNT",  1, 1, eval_amount },
		{ "BALANCE", 1, 2, eval_balance },
		{ "BASE",    1, 1, eval_base },
		{ "CODE",    1, 1, eval_code },
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
	g_free( priv->label );
	g_list_free( priv->generated_opes );
	g_list_free( priv->generated_entries );

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
	priv->is_writable = FALSE;
	priv->is_new = FALSE;
	priv->initialized = FALSE;
	priv->is_dirty = FALSE;
	priv->generated_opes = NULL;
	priv->generated_entries = NULL;

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

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->tva_record = record;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_record_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_tva_record_properties_iwindow_init";
	ofaTVARecordPropertiesPrivate *priv;
	gchar *id, *sdate;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	sdate = my_date_to_str( ofo_tva_record_get_end( priv->tva_record ), MY_DATE_SQL );
	id = g_strdup_printf( "%s-%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_tva_record_get_mnemo( priv->tva_record ),
				sdate );
	my_iwindow_set_identifier( instance, id );
	g_free( id );
	g_free( sdate );
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
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * VAT record
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_tva_record_properties_idialog_init";
	ofaTVARecordPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( instance ));

	init_ui( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_properties( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_booleans( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_taxes( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_correspondence( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_editability( OFA_TVA_RECORD_PROPERTIES( instance ));
	init_notes( OFA_TVA_RECORD_PROPERTIES( instance ));

	priv->generated_opes = ofo_tva_record_get_accounting_opes( priv->tva_record );
	priv->generated_entries = get_accounting_entries( OFA_TVA_RECORD_PROPERTIES( instance ));
	on_generated_opes_changed( OFA_TVA_RECORD_PROPERTIES( instance ));

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
		priv->cancel_btn = NULL;
	}

	priv->initialized = TRUE;
	set_dirty( OFA_TVA_RECORD_PROPERTIES( instance ), FALSE );
	check_for_enable_dlg( OFA_TVA_RECORD_PROPERTIES( instance ));
}

/*
 * setup the general data of the dialog
 */
static void
init_ui( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *btn;
	ofaHub *hub;

	priv = ofa_tva_record_properties_get_instance_private( self );

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), self );
	priv->ok_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "cancel-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->cancel_btn = btn;

	/* writability of the dossier */
	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	/* VAT style CSS */
	priv->style_provider = ofa_tva_style_new( priv->getter );

	/* writability of the record */
	priv->status = ofo_tva_record_get_status( priv->tva_record );
	priv->is_validated = ( priv->status != VAT_STATUS_NO );

	/* actions buttons */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "compute-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	gtk_widget_set_sensitive( btn, FALSE );
	g_signal_connect( btn, "clicked", G_CALLBACK( on_compute_clicked ), self );
	priv->compute_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "generate-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	gtk_widget_set_sensitive( btn, FALSE );
	g_signal_connect( btn, "clicked", G_CALLBACK( on_generate_clicked ), self );
	priv->generate_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-view-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	gtk_widget_set_sensitive( btn, FALSE );
	g_signal_connect( btn, "clicked", G_CALLBACK( on_viewopes_clicked ), self );
	priv->viewopes_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-delete-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	gtk_widget_set_sensitive( btn, FALSE );
	g_signal_connect( btn, "clicked", G_CALLBACK( on_delopes_clicked ), self );
	priv->delopes_btn = btn;

	my_utils_container_crestamp_init( GTK_CONTAINER( self ), tva_record );
	my_utils_container_updstamp_init( GTK_CONTAINER( self ), tva_record );
}

static void
init_properties( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *entry, *label;
	const gchar *cstr;
	gboolean is_true;

	priv = ofa_tva_record_properties_get_instance_private( self );

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
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );
	cstr = ofo_tva_record_get_label( priv->tva_record );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* has correspondence */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-has-corresp-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	is_true = ofo_tva_record_get_has_correspondence( priv->tva_record );
	gtk_label_set_text( GTK_LABEL( label ), is_true ? _( "Yes" ) : _( "No" ));

	/* is validated: invariant */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-validated-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	cstr = ofo_tva_record_status_get_label( priv->status );
	gtk_label_set_text( GTK_LABEL( label ), cstr );

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
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_changed ), self );

	my_date_set_from_date( &priv->begin_date, ofo_tva_record_get_begin( priv->tva_record ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin_date );

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
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );

	my_date_set_from_date( &priv->end_date, ofo_tva_record_get_end( priv->tva_record ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end_date );
	my_utils_widget_set_editable( entry, FALSE );

	my_date_set_from_date( &priv->init_end_date, ofo_tva_record_get_end( priv->tva_record ));

	/* operation date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->dope_editable = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_dope_changed ), self );

	my_date_set_from_date( &priv->dope_init, ofo_tva_record_get_dope( priv->tva_record ));
	my_date_set_from_date( &priv->dope_date, ofo_tva_record_get_dope( priv->tva_record ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->dope_date );
	my_utils_widget_set_editable( entry, priv->is_writable && !priv->is_validated );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-generated-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->generated_label = label;
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
		my_utils_widget_set_editable( button, priv->is_writable && !priv->is_validated );
		gtk_grid_attach( GTK_GRID( grid ), button, BOOL_COL_LABEL, row, 1, 1 );
		g_signal_connect( button, "toggled", G_CALLBACK( on_boolean_toggled ), self );
		is_true = ofo_tva_record_boolean_get_true( priv->tva_record, idx );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), is_true );
	}
}

static void
init_taxes( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *grid, *entry, *label;
	guint idx, level, count, row;
	const gchar *cstr;
	gboolean has_base, has_amount;
	ofxAmount amount;
	gchar *style;

	priv = ofa_tva_record_properties_get_instance_private( self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-grid" );
	priv->detail_grid = grid;

	count = ofo_tva_record_detail_get_count( priv->tva_record );
	for( idx=0 ; idx<count ; ++idx ){
		row = idx+1;
		level = ofo_tva_record_detail_get_level( priv->tva_record, idx );
		style = g_strdup_printf( "vat-level%d", level );

		/* code */
		label = gtk_label_new( "" );
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		gtk_grid_attach( GTK_GRID( grid ), label, DET_COL_CODE, row, 1, 1 );
		ofa_tva_style_set_style( priv->style_provider, label, style );

		cstr = ofo_tva_record_detail_get_code( priv->tva_record, idx );
		gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? cstr : "" );

		/* label */
		label = gtk_label_new( "" );
		gtk_widget_set_hexpand( label, TRUE );
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		gtk_grid_attach( GTK_GRID( grid ), label, DET_COL_LABEL, row, 1, 1 );
		ofa_tva_style_set_style( priv->style_provider, label, style );

		cstr = ofo_tva_record_detail_get_label( priv->tva_record, idx );
		gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? cstr : "" );

		/* base */
		has_base = ofo_tva_record_detail_get_has_base( priv->tva_record, idx );
		if( has_base ){
			entry = gtk_entry_new();
			my_utils_widget_set_editable( entry, priv->is_writable && !priv->is_validated );
			my_double_editable_init_ex( GTK_EDITABLE( entry ),
					g_utf8_get_char( ofa_prefs_amount_get_thousand_sep( priv->getter )),
					g_utf8_get_char( ofa_prefs_amount_get_decimal_sep( priv->getter )),
					ofa_prefs_amount_get_accept_dot( priv->getter ),
					ofa_prefs_amount_get_accept_comma( priv->getter ),
					0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 16 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_BASE, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_base_changed ), self );

			gtk_widget_set_tooltip_text(
					entry, ofo_tva_record_detail_get_base_formula( priv->tva_record, idx ));

			amount = ofo_tva_record_detail_get_base( priv->tva_record, idx );
			my_double_editable_set_amount( GTK_EDITABLE( entry ), amount );
		}

		/* amount */
		has_amount = ofo_tva_record_detail_get_has_amount( priv->tva_record, idx );
		if( has_amount ){
			entry = gtk_entry_new();
			my_utils_widget_set_editable( entry, priv->is_writable && !priv->is_validated );
			my_double_editable_init_ex( GTK_EDITABLE( entry ),
					g_utf8_get_char( ofa_prefs_amount_get_thousand_sep( priv->getter )),
					g_utf8_get_char( ofa_prefs_amount_get_decimal_sep( priv->getter )),
					ofa_prefs_amount_get_accept_dot( priv->getter ),
					ofa_prefs_amount_get_accept_comma( priv->getter ),
					0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 16 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_AMOUNT, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_amount_changed ), self );

			gtk_widget_set_tooltip_text(
					entry, ofo_tva_record_detail_get_amount_formula( priv->tva_record, idx ));

			amount = ofo_tva_record_detail_get_amount( priv->tva_record, idx );
			my_double_editable_set_amount( GTK_EDITABLE( entry ), amount );
		}

		g_free( style );
	}
}

static void
init_correspondence( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *book, *label, *scrolled;
	GtkTextBuffer *buffer;
	const gchar *cstr;

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->has_correspondence = ofo_tva_record_get_has_correspondence( priv->tva_record );

	if( priv->has_correspondence ){
		book = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tva-book" );
		g_return_if_fail( book && GTK_IS_NOTEBOOK( book ));
		label = gtk_label_new_with_mnemonic( _( "_Correspondence" ));
		scrolled = gtk_scrolled_window_new( NULL, NULL );
		gtk_notebook_insert_page( GTK_NOTEBOOK( book ), scrolled, label, 3 );
		priv->corresp_textview = gtk_text_view_new();
		gtk_text_view_set_left_margin( GTK_TEXT_VIEW( priv->corresp_textview ), 2 );
		gtk_container_add( GTK_CONTAINER( scrolled ), priv->corresp_textview );

		cstr = ofo_tva_record_get_correspondence( priv->tva_record );
		my_utils_container_notes_setup_ex( GTK_TEXT_VIEW( priv->corresp_textview ), cstr, TRUE );
		gtk_widget_set_sensitive( priv->corresp_textview, priv->is_writable && !priv->is_validated );

		buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->corresp_textview ));
		g_signal_connect( buffer, "changed", G_CALLBACK( on_corresp_changed ), self );
	}
}

static void
init_editability( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	my_utils_container_set_editable( GTK_CONTAINER( self ),
			priv->is_writable && !priv->is_validated);

	/* notes may be edited even after the declaration has been validated */
	my_utils_container_notes_setup_full( GTK_CONTAINER( self ),
			"pn-notes", ofo_tva_record_get_notes( priv->tva_record ), priv->is_writable );
}

static void
init_notes( ofaTVARecordProperties *self )
{
	GtkWidget *view;
	GtkTextBuffer *buffer;

	view = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "pn-notes" );
	g_return_if_fail( view && GTK_IS_TEXT_VIEW( view ));

	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( view ));
	g_return_if_fail( buffer && GTK_IS_TEXT_BUFFER( buffer ));

	g_signal_connect( buffer, "changed", G_CALLBACK( on_notes_changed ), self );
}

static void
on_label_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

static void
on_begin_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	my_date_set_from_date( &priv->begin_date, my_date_editable_get_date( entry, NULL ));

	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

/*
 * Ending date never changes.
 * This function is only triggered when initially setting up the ending
 * date.
 */
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
on_dope_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	my_date_set_from_date( &priv->dope_date, my_date_editable_get_date( entry, NULL ));

	check_for_enable_dlg( self );

	// does not set dirty flag as operation date update is managed separately
}

static void
on_corresp_changed( GtkTextBuffer *buffer, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

static void
on_notes_changed( GtkTextBuffer *buffer, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

static void
on_boolean_toggled( GtkToggleButton *button, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

static void
on_detail_base_changed( GtkEntry *entry, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

static void
on_detail_amount_changed( GtkEntry *entry, ofaTVARecordProperties *self )
{
	check_for_enable_dlg( self );
	set_dirty( self, TRUE );
}

/*
 * - must have both begin and end dates to be able to compute the declaration
 * - must have an operation date to generate the operations
 * - is saveable at any time
 */
static void
check_for_enable_dlg( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	gboolean is_valid, compute_ok, generate_ok, view_ok;
	gchar *msgerr;
	ofoEntry *entry;
	ofeEntryStatus status;
	guint undeletable_entries;
	GList *it;

	priv = ofa_tva_record_properties_get_instance_private( self );

	msgerr = NULL;
	is_valid = FALSE;
	compute_ok = FALSE;
	generate_ok = FALSE;
	view_ok = FALSE;
	undeletable_entries = 0;

	if( priv->is_writable ){

		if( priv->is_validated ){
			is_valid = TRUE;

		} else {
			is_valid = ofo_tva_record_is_valid_data( priv->mnemo, priv->label, &priv->begin_date, &priv->end_date, &msgerr );

			if( is_valid ){
				if( my_date_is_valid( &priv->begin_date )){
					if( ofo_tva_record_get_overlap( priv->getter, priv->mnemo, &priv->begin_date, &priv->end_date ) != NULL ){
						msgerr = g_strdup( _( "Current record overlaps with an already defined VAT declaration" ));
						is_valid = FALSE;

					} else {
						compute_ok = TRUE;
					}
				} else {
					/* do noting here: this is a warning */
				}
			}
		}

		gtk_widget_set_sensitive( priv->ok_btn, is_valid );

		/* until here, messages were errors */
		set_msgerr( self, msgerr );

		/* beginning from this, messages should be treated as warnings
		 * which do not prevent to record in dbms */
		if( !msgerr ){
			if( !my_date_is_valid( &priv->begin_date )){
				msgerr = g_strdup( _( "Beginning date is not set or invalid" ));

			} else if( compute_ok && !priv->is_validated ){
				if( !my_date_is_valid( &priv->dope_date )){
					msgerr = g_strdup( _( "Operation date is not set or invalid" ));

				} else if( my_date_compare( &priv->end_date, &priv->dope_date ) > 0 ){
					msgerr = g_strdup( _( "Operation date must be greater or equal to ending date" ));

				} else if( g_list_length( priv->generated_opes ) == 0 ){
					generate_ok = TRUE;

				} else {
					view_ok = TRUE;
					for( it=priv->generated_entries ; it ; it=it->next ){
						entry = OFO_ENTRY( it->data );
						status = ofo_entry_get_status( entry );
						if( status != ENT_STATUS_ROUGH ){
							undeletable_entries += 1;
						}
					}
				}
			}
			set_msgwarn( self, msgerr );
		}

		g_free( msgerr );
	}

	gtk_widget_set_sensitive( priv->compute_btn, compute_ok );
	gtk_widget_set_sensitive( priv->generate_btn, generate_ok );
	gtk_widget_set_sensitive( priv->viewopes_btn, view_ok );
	gtk_widget_set_sensitive( priv->delopes_btn, view_ok && undeletable_entries == 0 );
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
	title = g_strdup_printf( _( "Updating « %s - %s » VAT declaration" ), priv->mnemo, send );
	gtk_window_set_title( GTK_WINDOW( self ), title );
	g_free( title );
	g_free( send );
}

/*
 * After the DBMS has been updated, set dirty flag to %FALSE
 * and setup the buttons accordingly
 */
static void
set_dirty( ofaTVARecordProperties *self, gboolean dirty )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	priv->is_dirty = dirty;

	/*
	if( priv->cancel_btn ){
		gtk_widget_set_sensitive( priv->cancel_btn, priv->is_dirty );
		gtk_button_set_label( GTK_BUTTON( priv->ok_btn ), priv->is_dirty ? _( "_OK" ) : _( "Cl_ose" ));
	}
	*/
}

/*
 * Update the TVARecord object from UI data.
 */
static void
setup_tva_record( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	guint idx, row, count;
	GtkWidget *button, *entry;
	gboolean is_true;
	ofxAmount amount;
	gchar *str;

	priv = ofa_tva_record_properties_get_instance_private( self );

	ofo_tva_record_set_label( priv->tva_record, priv->label );
	ofo_tva_record_set_begin( priv->tva_record, &priv->begin_date );

	if( priv->has_correspondence ){
		GtkTextBuffer *buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->corresp_textview ));
		GtkTextIter start, end; gtk_text_buffer_get_start_iter( buffer, &start );
		gtk_text_buffer_get_end_iter( buffer, &end );
		gchar *notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
		ofo_tva_record_set_correspondence( priv->tva_record, notes );
		g_free( notes );
	}

	my_utils_container_notes_get( GTK_WINDOW( self ), tva_record );

	count = ofo_tva_record_boolean_get_count( priv->tva_record );
	for( idx=0, row=0 ; idx<count ; ++idx, ++row ){
		button = gtk_grid_get_child_at( GTK_GRID( priv->boolean_grid ), BOOL_COL_LABEL, row );
		g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
		is_true = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( button ));
		ofo_tva_record_boolean_set_true( priv->tva_record, idx, is_true );
	}

	count = ofo_tva_record_detail_get_count( priv->tva_record );
	for( idx=0, row=1 ; idx<count ; ++idx, ++row ){
		if( ofo_tva_record_detail_get_has_base( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_BASE, row );
			g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
			str = my_double_editable_get_string( GTK_EDITABLE( entry ));
			amount = ofa_amount_from_str( str, priv->getter );
			ofo_tva_record_detail_set_base( priv->tva_record, idx, amount );
			g_free( str );
		}
		if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
			g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
			str = my_double_editable_get_string( GTK_EDITABLE( entry ));
			amount = ofa_amount_from_str( str, priv->getter );
			ofo_tva_record_detail_set_amount( priv->tva_record, idx, amount );
			g_free( str );
		}
	}
}

static void
on_ok_clicked( ofaTVARecordProperties *self )
{
	gchar *msgerr;

	setup_tva_record( self );

	msgerr = NULL;

	do_update_dbms( self, &msgerr );
	if( !msgerr ){
		do_update_dope( self, FALSE, &msgerr );
	}

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

/*
 * the TVARecord object is expected to have been previously setup_tva_record().
 */
static gboolean
do_update_dbms( ofaTVARecordProperties *self, gchar **msgerr )
{
	ofaTVARecordPropertiesPrivate *priv;
	gboolean ok;

	priv = ofa_tva_record_properties_get_instance_private( self );

	if( priv->is_dirty ){
		ok = ofo_tva_record_update( priv->tva_record );

		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the VAT declaration" ));

		} else {
			set_dirty( self, FALSE );
		}
	} else {
		ok = TRUE;
	}

	return( ok );
}

/*
 * Update the operation date in DBMS if it has been modified in the UI
 */
static gboolean
do_update_dope( ofaTVARecordProperties *self, gboolean force, gchar **msgerr )
{
	ofaTVARecordPropertiesPrivate *priv;
	gboolean modified, ok;

	priv = ofa_tva_record_properties_get_instance_private( self );

	ok = TRUE;
	modified = force;

	if( !force ){
		if( my_date_is_valid( &priv->dope_init )){
			if( my_date_is_valid( &priv->dope_date )){
				if( my_date_compare( &priv->dope_init, &priv->dope_date ) != 0 ){
					modified = TRUE;
				}
			} else {
				modified = TRUE;
			}
		} else if( my_date_is_valid( &priv->dope_date )){
			modified = TRUE;
		}
	}

	if( modified && !ofo_tva_record_update_dope( priv->tva_record, &priv->dope_date )){
		ok = FALSE;
		*msgerr = g_strdup( _( "Unable to update the operation date in DBMS" ));
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
					_( "Caution: computing the declaration will erase all possible "
							"manual modifications you may have done.\n"
							"Are your sure you want this ?" ));
	gtk_dialog_add_buttons(
			GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "C_ompute" ), GTK_RESPONSE_OK,
			NULL );
	resp = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	if( resp == GTK_RESPONSE_OK ){
		if( !st_engine ){
			st_engine = ofa_formula_engine_new( priv->getter );
			ofa_formula_engine_set_auto_eval( st_engine, TRUE );
		}
		count = ofo_tva_record_detail_get_count( priv->tva_record );

		for( idx=0, row=1 ; idx<count ; ++idx, ++row ){
			if( ofo_tva_record_detail_get_has_base( priv->tva_record, idx )){
				rule = ofo_tva_record_detail_get_base_formula( priv->tva_record, idx );
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
				rule = ofo_tva_record_detail_get_amount_formula( priv->tva_record, idx );
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

	set_dirty( self, TRUE );
}

/*
 * this is a ofaFormula callback
 * Returns: the evaluation function for the name + expected args count
 */
static ofaFormulaEvalFn
get_formula_eval_fn( const gchar *name, gint *min_count, gint *max_count, GMatchInfo *match_info, ofaTVARecordProperties *self )
{
	static const gchar *thisfn = "ofa_tva_record_properties_get_formula_eval_fn";
	gint i;

	*min_count = 0;
	*max_count = -1;

	for( i=0 ; st_formula_fns[i].name ; ++i ){
		if( !my_collate( st_formula_fns[i].name, name )){
			*min_count = st_formula_fns[i].min_args;
			*max_count = st_formula_fns[i].max_args;
			g_debug( "%s: found name=%s, expected min count=%d, max_count=%d", thisfn, name, *min_count, *max_count );
			return(( ofaFormulaEvalFn ) st_formula_fns[i].eval );
		}
	}

	return( NULL );
}

/*
 * %ACCOUNT(begin[;end])
 * Returns: the rough+validated balances for the entries on the specified
 *  period on the begin[;end] account(s)
 */
static gchar *
eval_account( ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_account";
	ofaTVARecordPropertiesPrivate *priv;
	gchar *res;
	GList *it, *dataset;
	const gchar *cbegin, *cend;
	ofxAmount amount;
	ofsAccountBalance  *sbal;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( helper->user_data ));

	res = NULL;
	it = helper->args_list;
	cbegin = it ? ( const gchar * ) it->data : NULL;
	it = it ? it->next : NULL;
	cend = it ? ( const gchar * ) it->data : NULL;
	if( !cend ){
		cend = cbegin;
	}
	DEBUG( "%s: begin=%s, end=%s", thisfn, cbegin, cend );

	dataset = ofo_entry_get_dataset_account_balance(
					priv->getter, cbegin, cend, &priv->begin_date, &priv->end_date, NULL );
	amount = 0;
	for( it=dataset ; it ; it=it->next ){
		sbal = ( ofsAccountBalance * ) it->data;
		/* credit is -, debit is + */
		amount -= sbal->credit;
		amount += sbal->debit;
	}

	res = ofa_amount_to_str( amount, NULL, priv->getter );
	ofs_account_balance_list_free( &dataset );

	DEBUG( "%s: ACCOUNT(%s[;%s])=%s", thisfn, cbegin, cend, res );

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
		res = ofa_amount_to_str( amount, NULL, priv->getter );
	}

	DEBUG( "%s: cstr=%s, res=%s", thisfn, cstr, res );

	return( res );
}

/*
 * %BALANCE(begin[;end])
 * Returns: the current rough+validated balances for the begin[;end] account(s)
 */
static gchar *
eval_balance( ofsFormulaHelper *helper )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_balance";
	ofaTVARecordPropertiesPrivate *priv;
	gchar *res;
	GList *it, *dataset;
	const gchar *cbegin, *cend, *acc_id;
	ofxAmount amount;
	ofoAccount *account;
	gint cmp_begin, cmp_end;

	priv = ofa_tva_record_properties_get_instance_private( OFA_TVA_RECORD_PROPERTIES( helper->user_data ));

	res = NULL;
	it = helper->args_list;
	cbegin = it ? ( const gchar * ) it->data : NULL;
	it = it ? it->next : NULL;
	cend = it ? ( const gchar * ) it->data : NULL;
	if( !cend ){
		cend = cbegin;
	}
	DEBUG( "%s: begin=%s, end=%s", thisfn, cbegin, cend );

	dataset = ofo_account_get_dataset( priv->getter );
	amount = 0;
	for( it=dataset ; it ; it=it->next ){
		account = OFO_ACCOUNT( it->data );
		acc_id = ofo_account_get_number( account );
		cmp_begin = my_collate( cbegin, acc_id );
		cmp_end = my_collate( acc_id, cend );

		if( 0 ){
			DEBUG( "%s: acc_id=%s, my_collate( %s, acc_id )=%d, my_collate( acc_id, %s )=%d",
					thisfn, acc_id, cbegin, cmp_begin, cend, cmp_end );
		}

		if( cmp_begin <= 0 && cmp_end <= 0 ){
			/* credit is -, debit is + */
			amount -= ofo_account_get_current_rough_credit( account );
			amount += ofo_account_get_current_rough_debit( account );
			amount -= ofo_account_get_current_val_credit( account );
			amount += ofo_account_get_current_val_debit( account );
			amount -= ofo_account_get_futur_rough_credit( account );
			amount += ofo_account_get_futur_rough_debit( account );
		}
	}

	res = ofa_amount_to_str( amount, NULL, priv->getter );

	DEBUG( "%s: BALANCE(%s[;%s])=%s", thisfn, cbegin, cend, res );

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
		res = ofa_amount_to_str( amount, NULL, priv->getter );
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
 * Generate the accounting operations
 *
 * This is only possible when the VAT declaration is valid, but not yet
 * validated, and no operation has yet been generated.
 */
static void
on_generate_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *msgerr, *msg;
	guint ope_count, ent_count;

	priv = ofa_tva_record_properties_get_instance_private( self );

	setup_tva_record( self );

	if( do_generate_opes( self, &msgerr, &ope_count, &ent_count ) &&
			do_update_dope( self, TRUE, &msgerr )){

		my_date_set_from_date( &priv->dope_init, &priv->dope_date );

		msg = g_strdup_printf(
				_( "%u operations successfully generated (%u entries)" ), ope_count, ent_count );
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_INFO, msg );
		g_free( msg );

		g_list_free( priv->generated_opes );
		priv->generated_opes = ofo_tva_record_get_accounting_opes( priv->tva_record );
		g_return_if_fail( ope_count == g_list_length( priv->generated_opes ));

		g_list_free( priv->generated_entries );
		priv->generated_entries = get_accounting_entries( self );
		g_return_if_fail( ent_count == g_list_length( priv->generated_entries ));

		on_generated_opes_changed( self );

	} else {
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}
}

/*
 * when an operation template is recorded besides of an amount, it is
 * generated if the corresponding amount is greater than zero.
 * This amount is then injected in the operation template, first row and
 * first of available debit/credit.
 */
static gboolean
do_generate_opes( ofaTVARecordProperties *self, gchar **msgerr, guint *ope_count, guint *ent_count )
{
	static const gchar *thisfn = "ofa_tva_record_properties_do_generate_opes";
	ofaTVARecordPropertiesPrivate *priv;
	ofaHub *hub;
	guint count, rec_idx, tmpl_count, tmpl_idx;
	ofxAmount amount;
	const gchar *cstr;
	ofsOpe *ope;
	ofsOpeDetail *detail;
	ofoOpeTemplate *template;
	gboolean done, ok;
	GList *entries, *it;
	ofxCounter ope_number;
	ofaIDBConnect *connect;
	ofoEntry *entry;

	priv = ofa_tva_record_properties_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );

	*ope_count = 0;
	*ent_count = 0;

	count = ofo_tva_record_detail_get_count( priv->tva_record );
	for( rec_idx=0 ; rec_idx < count ; ++rec_idx ){
		if( ofo_tva_record_detail_get_has_amount( priv->tva_record, rec_idx )){
			amount = ofo_tva_record_detail_get_amount( priv->tva_record, rec_idx );
			cstr = ofo_tva_record_detail_get_template( priv->tva_record, rec_idx );
			if( amount > 0 && my_strlen( cstr )){
				g_debug( "%s: amount=%lf, template=%s", thisfn, amount, cstr );
				done = FALSE;
				ope = NULL;
				template = ofo_ope_template_get_by_mnemo( priv->getter, cstr );
				if( template && OFO_IS_OPE_TEMPLATE( template )){
					/*
					 * generate an operation when the amount is greater than zero
					 * and the operation template is set and found
					 * inject the positive amount into the first row/debit/credit
					 * slot
					 * + the period label is appended to the label of the
					 * first detail of the template
					 */
					ope = ofs_ope_new( template );
					my_date_set_from_date( &ope->dope, ofo_tva_record_get_dope( priv->tva_record ));
					ope->dope_user_set = TRUE;
					ope->ref = g_strdup( ofo_tva_record_get_mnemo( priv->tva_record ));
					ope->ref_user_set = TRUE;

					tmpl_count = ofo_ope_template_detail_get_count( template );
					for( tmpl_idx=0 ; tmpl_idx < tmpl_count ; ++tmpl_idx ){
						if( !ofo_ope_template_detail_get_debit_locked( template, tmpl_idx )){
							cstr = ofo_ope_template_detail_get_debit( template, tmpl_idx );
							if( !my_strlen( cstr )){
								detail = ( ofsOpeDetail * ) g_list_nth( ope->detail, tmpl_idx )->data;
								detail->debit = amount;
								detail->debit_user_set = TRUE;
								done = TRUE;
								break;

							}
						} else if( !ofo_ope_template_detail_get_credit_locked( template, tmpl_idx )){
							cstr = ofo_ope_template_detail_get_credit( template, tmpl_idx );
							if( !my_strlen( cstr )){
								detail = ( ofsOpeDetail * ) g_list_nth( ope->detail, tmpl_idx )->data;
								detail->credit = amount;
								detail->credit_user_set = TRUE;
								done = TRUE;
								break;

							}
						} else {
							g_warning( "%s: operation template %s does not have any placeholder to host an amount",
									thisfn, ofo_ope_template_get_mnemo( template ));
						}
					}
				} else {
					g_warning( "%s: invalid or unknown ope_template=%s", thisfn, cstr );
				}
				/*
				 * setup an operation number
				 * generate the entries and insert into the dbms
				 */
				if( done ){
					ofs_ope_apply_template( ope );
					if( ofs_ope_is_valid( ope, msgerr, NULL )){
						entries = ofs_ope_generate_entries( ope );
						ok = ofa_idbconnect_transaction_start( connect, FALSE, NULL );
						if( ok ){
							ope_number = ofo_counters_get_next_ope_id( priv->getter );
							for( it=entries ; it && ok ; it=it->next ){
								entry = OFO_ENTRY( it->data );
								ofo_entry_set_ope_number( entry, ope_number );
								ok = ofo_entry_insert( entry );
								*ent_count += 1;
							}
						}
						if( ok ){
							ofa_idbconnect_transaction_commit( connect, FALSE, NULL );
							g_list_free( entries );
							ofo_tva_record_detail_set_ope_number( priv->tva_record, rec_idx, ope_number );
							*ope_count += 1;

						} else {
							ok = ofa_idbconnect_transaction_cancel( connect, FALSE, NULL );
							g_list_free_full( entries, g_object_unref );
						}
					}
				}
				ofs_ope_free( ope );
			}
		}
	}

	return( TRUE );
}

static void
on_generated_opes_changed( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *str;
	ofeVatStatus status;
	guint count;

	priv = ofa_tva_record_properties_get_instance_private( self );

	count = g_list_length( priv->generated_opes );

	if( count == 0 ){
		status = ofo_tva_record_get_status( priv->tva_record );
		if( status != VAT_STATUS_NO ){
			str = g_strdup( _( "No generated operation, and the declaration is validated." ));
		} else if( !priv->is_writable ){
			str = g_strdup( _( "No generated operation, and the dossier is not writable." ));
		} else {
			str = g_strdup( _( "No generated operation yet, but this is not too late." ));
		}

	} else if( count == 1 ){
		str = g_strdup( _( "One operation has been generated." ));

	} else {
		str = g_strdup_printf( _( "%u operations have been generated." ), count );
	}

	gtk_label_set_text( GTK_LABEL( priv->generated_label ), str );
	g_free( str );

	check_for_enable_dlg( self );
}

static GList *
get_accounting_entries( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GList *dataset, *it, *list;
	ofoEntry *entry;
	ofxCounter openum;

	priv = ofa_tva_record_properties_get_instance_private( self );

	list = NULL;

	if( g_list_length( priv->generated_opes ) > 0 ){
		dataset = ofo_entry_get_dataset( priv->getter );
		for( it=dataset ; it ; it=it->next ){
			entry = OFO_ENTRY( it->data );
			openum = ofo_entry_get_ope_number( entry );
			if( g_list_find( priv->generated_opes, ( gpointer ) openum )){
				list = g_list_prepend( list, entry );
			}
		}
	}

	return( list );
}

/*
 * view the generated operations
 *
 * This is only possible when operations have been generated
 */
static void
on_viewopes_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	ofa_operation_group_run( priv->getter, priv->parent, priv->generated_opes );
}

/*
 * delete the generated operations
 * authorized while the entries have not been validated
 *
 * This implies:
 * - raz of the corresponding column on each detail lines of VAT declaration
 * - delete the entries
 */
static void
on_delopes_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	guint count, rec_idx;
	ofxCounter ope_number;

	priv = ofa_tva_record_properties_get_instance_private( self );

	if( delopes_user_confirm( self )){

		/* reinit operation date */
		gtk_entry_set_text( GTK_ENTRY( priv->dope_editable ), "" );

		/* remove operations from detail lines */
		count = ofo_tva_record_detail_get_count( priv->tva_record );
		for( rec_idx=0 ; rec_idx < count ; ++rec_idx ){
			ope_number = ofo_tva_record_detail_get_ope_number( priv->tva_record, rec_idx );
			if( ope_number > 0 ){
				ofo_tva_record_detail_set_ope_number( priv->tva_record, rec_idx, 0 );
			}
		}

		setup_tva_record( self );
		do_update_dbms( self, NULL );

		/* delete entries */
		ofo_tva_record_delete_accounting_entries( priv->tva_record, priv->generated_opes );

		g_list_free( priv->generated_opes );
		/* should be NULL */
		priv->generated_opes = ofo_tva_record_get_accounting_opes( priv->tva_record );

		g_list_free( priv->generated_entries );
		/* should be NULL */
		priv->generated_entries = get_accounting_entries( self );

		on_generated_opes_changed( self );
	}
}

static gboolean
delopes_user_confirm( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	guint opes_count, entries_count;
	GString *gstr;
	gboolean ok;

	priv = ofa_tva_record_properties_get_instance_private( self );

	opes_count = g_list_length( priv->generated_opes );
	g_return_val_if_fail( opes_count > 0, FALSE );

	entries_count = g_list_length( priv->generated_entries );
	gstr = g_string_new( "" );

	if( opes_count == 1 ){
		g_string_printf( gstr,
				_( "You are about to delete the generated accounting operation (%u entries)." ),
					entries_count );
	} else {
		g_string_printf( gstr,
				_( "You are about to delete %u accounting operations (%u entries)." ),
					opes_count, entries_count );
	}
	gstr = g_string_append( gstr, _( "\nAre you sure ?" ));

	ok = my_utils_dialog_question( GTK_WINDOW( self ), gstr->str, _( "_Delete" ));

	g_string_free( gstr, TRUE );

	return( ok );
}

static void
set_msgerr( ofaTVARecordProperties *self, const gchar *msg )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	if( !priv->msg_label ){
		priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	}

	my_style_remove( priv->msg_label, "labelwarning");
	my_style_add( priv->msg_label, "labelerror");
	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}

static void
set_msgwarn( ofaTVARecordProperties *self, const gchar *msg )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = ofa_tva_record_properties_get_instance_private( self );

	if( !priv->msg_label ){
		priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	}

	my_style_remove( priv->msg_label, "labelerror");
	my_style_add( priv->msg_label, "labelwarning");
	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
