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
#include <stdlib.h>

#include "my/my-date-editable.h"
#include "my/my-ibin.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-counter.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-counters.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "core/ofa-open-prefs.h"
#include "core/ofa-open-prefs-bin.h"
#include "core/ofa-currency-combo.h"
#include "core/ofa-ledger-combo.h"
#include "core/ofa-ledger-store.h"

#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;
	GtkWindow          *parent;

	/* runtime
	 */
	ofoDossier         *dossier;
	gboolean            is_new;
	gboolean            is_writable;
	GDate               begin_init;
	GDate               end_init;
	GDate               min_end;
	GDate               prevexe_end;
	myISettings        *settings_iface;		/* dossier settings */
	const gchar        *settings_group;

	/* data
	 */
	gchar              *label;
	gchar              *label2;
	gchar              *siren;
	gchar              *siret;
	gchar              *vatic;
	gchar              *naf;
	gchar              *currency;
	gchar              *import_ledger;
	GDate               begin;
	gboolean            begin_empty;
	GDate               end;
	gboolean            end_empty;
	gint                duree;
	gchar              *exe_notes;
	ofaOpenPrefs       *prefs;
	gchar              *background_orig_uri;

	/* UI
	 */
	GtkWidget          *siren_entry;
	GtkWidget          *siret_entry;
	GtkWidget          *vatic_entry;
	GtkWidget          *naf_entry;
	GtkWidget          *begin_entry;
	ofaClosingParmsBin *closing_parms;
	ofaOpenPrefsBin    *prefs_bin;
	GtkWidget          *background_btn;
	GtkWidget          *background_clear;
	GtkWidget          *background_preview;
	GtkWidget          *msgerr;
	GtkWidget          *ok_btn;

	/* when remediating entries
	 */
	GtkWidget          *dialog;
	GtkWidget          *button;
	myProgressBar      *bar;
	gulong              total;
	gulong              count;
	GList              *signaler_handlers;
}
	ofaDossierPropertiesPrivate;

#define MSG_NORMAL                      "labelnormal"
#define MSG_WARNING                     "labelwarning"
#define MSG_ERROR                       "labelerror"

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_properties_page( ofaDossierProperties *self );
static void     init_exercice_page( ofaDossierProperties *self );
static void     init_exe_notes_page( ofaDossierProperties *self );
static void     init_internals_page( ofaDossierProperties *self );
static void     init_preferences_page( ofaDossierProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void     on_label2_changed( GtkEntry *entry, ofaDossierProperties *self );
static void     on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierProperties *self );
static void     on_import_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaDossierProperties *self );
static void     on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
static void     on_begin_changed( GtkEditable *editable, ofaDossierProperties *self );
static void     on_end_changed( GtkEditable *editable, ofaDossierProperties *self );
static void     on_date_changed( ofaDossierProperties *self, GtkEditable *editable, GDate *date, gboolean *is_empty );
static void     on_closing_parms_changed( ofaClosingParmsBin *bin, ofaDossierProperties *self );
static void     on_notes_changed( GtkTextBuffer *buffer, ofaDossierProperties *self );
static void     background_image_on_file_set( GtkFileChooserButton *button, ofaDossierProperties *self );
static void     background_image_on_clear_clicked( GtkButton *button, ofaDossierProperties *self );
static void     background_image_on_preview_clicked( GtkButton *button, ofaDossierProperties *self );
static void     check_for_enable_dlg( ofaDossierProperties *self );
static gboolean is_dialog_valid( ofaDossierProperties *self );
static void     set_msgerr( ofaDossierProperties *self, const gchar *msg, const gchar *spec );
static void     on_cancel_clicked( ofaDossierProperties *self );
static void     on_ok_clicked( ofaDossierProperties *self );
static gboolean do_update( ofaDossierProperties *self, gchar **msgerr );
static gboolean confirm_remediation( ofaDossierProperties *self, gint count );
static void     display_progress_init( ofaDossierProperties *self );
static void     display_progress_end( ofaDossierProperties *self );
static void     signaler_connect_to_signaling_system( ofaDossierProperties *self );
static void     signaler_on_entry_status_count( ofaISignaler *signaler, ofeEntryStatus new_status, gulong count, ofaDossierProperties *self );
static void     signaler_on_entry_status_change( ofaISignaler *signaler, ofoEntry *entry, ofeEntryStatus prev_status, ofeEntryStatus new_status, ofaDossierProperties *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierProperties, ofa_dossier_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_finalize";
	ofaDossierPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_dossier_properties_get_instance_private( OFA_DOSSIER_PROPERTIES( instance ));

	g_free( priv->label );
	g_free( priv->label2 );
	g_free( priv->currency );
	g_free( priv->import_ledger );
	g_free( priv->exe_notes );
	g_free( priv->siren );
	g_free( priv->siret );
	g_free( priv->vatic );
	g_free( priv->naf );
	g_free( priv->background_orig_uri );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->finalize( instance );
}

static void
dossier_properties_dispose( GObject *instance )
{
	ofaDossierPropertiesPrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	priv = ofa_dossier_properties_get_instance_private( OFA_DOSSIER_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
		g_clear_object( &priv->prefs );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->dispose( instance );
}

static void
ofa_dossier_properties_init( ofaDossierProperties *self )
{
	static const gchar *thisfn = "ofa_dossier_properties_init";
	ofaDossierPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_PROPERTIES( self ));

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;
	my_date_clear( &priv->begin );
	my_date_clear( &priv->end );
	priv->signaler_handlers = NULL;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_properties_class_init( ofaDossierPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Update the properties of an dossier
 */
void
ofa_dossier_properties_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_dossier_properties_run";
	ofaDossierProperties *self;
	ofaDossierPropertiesPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	self = g_object_new( OFA_TYPE_DOSSIER_PROPERTIES, NULL );

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_iwindow_init";
	ofaDossierPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_properties_get_instance_private( OFA_DOSSIER_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * dossier
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_idialog_init";
	ofaDossierPropertiesPrivate *priv;
	ofaHub *hub;
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_properties_get_instance_private( OFA_DOSSIER_PROPERTIES( instance ));

	/* restore the original background on Cancel */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "cancel-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_cancel_clicked ), instance );

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	priv->msgerr = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "px-msgerr" );

	hub = ofa_igetter_get_hub( priv->getter );
	priv->dossier = ofa_hub_get_dossier( hub );
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	init_properties_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_exercice_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_exe_notes_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_preferences_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_internals_page( OFA_DOSSIER_PROPERTIES( instance ));

	/* these are main notes of the dossier */
	my_utils_container_notes_init( instance, dossier );
	my_utils_container_updstamp_init( instance, dossier );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );
	gtk_widget_set_sensitive( priv->begin_entry, priv->is_writable && !my_date_is_valid( &priv->prevexe_end ));

	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_DOSSIER_PROPERTIES( instance ));
}

static void
init_properties_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *entry, *label, *parent, *prompt;
	gchar *str;
	ofaCurrencyCombo *c_combo;
	ofaLedgerCombo *l_combo;
	const gchar *cstr;
	gint ivalue;
	const GDate *last_closed;
	static const gint st_currency_cols[] = { CURRENCY_COL_CODE, -1 };
	static const gint st_ledger_cols[] = { LEDGER_COL_LABEL, -1 };

	priv = ofa_dossier_properties_get_instance_private( self );

	my_date_set_from_date( &priv->prevexe_end, ofo_dossier_get_prevexe_end( priv->dossier ));

	/* dossier name */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );
	cstr = ofo_dossier_get_label( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* label 2 */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label2-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label2_changed ), self );
	cstr = ofo_dossier_get_label2( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label2-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* siren identifier */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-siren-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->siren = g_strdup( ofo_dossier_get_siren( priv->dossier ));
	if( priv->siren ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->siren );
	}
	priv->siren_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-siren-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* siret identifier */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-siret-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->siret = g_strdup( ofo_dossier_get_siret( priv->dossier ));
	if( priv->siret ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->siret );
	}
	priv->siret_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-siret-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* vat identifier */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-vat-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->vatic = g_strdup( ofo_dossier_get_vatic( priv->dossier ));
	if( priv->vatic ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->vatic );
	}
	priv->vatic_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-vat-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* naf identifier */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-naf-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->naf = g_strdup( ofo_dossier_get_naf( priv->dossier ));
	if( priv->naf ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->naf );
	}
	priv->naf_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-naf-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* default currency */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-currency-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	c_combo = ofa_currency_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( c_combo ));
	ofa_currency_combo_set_columns( c_combo, st_currency_cols );
	ofa_currency_combo_set_getter( c_combo, priv->getter );
	g_signal_connect( c_combo, "ofa-changed", G_CALLBACK( on_currency_changed ), self );
	ofa_currency_combo_set_selected( c_combo, ofo_dossier_get_default_currency( priv->dossier ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-currency-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( c_combo ));

	/* default import ledger */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	l_combo = ofa_ledger_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( l_combo ));
	ofa_ledger_combo_set_columns( l_combo, st_ledger_cols );
	ofa_ledger_combo_set_getter( l_combo, priv->getter );
	g_signal_connect( l_combo, "ofa-changed", G_CALLBACK( on_import_ledger_changed ), self );
	ofa_ledger_combo_set_selected( l_combo, ofo_dossier_get_import_ledger( priv->dossier ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( l_combo ));

	/* status */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-status" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), ofo_dossier_get_status( priv->dossier ));

	/* exercice length */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-length-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_duree_changed ), self );
	ivalue = ofo_dossier_get_exe_length( priv->dossier );
	str = g_strdup_printf( "%d", ivalue );
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-length-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* beginning date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->begin_entry = entry;

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-begin-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-begin-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_set_from_date( &priv->begin, ofo_dossier_get_exe_begin( priv->dossier ));
	priv->begin_empty = !my_date_is_valid( &priv->begin );
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_entry_format( GTK_EDITABLE( entry ), ofa_prefs_date_display( priv->getter ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_check( priv->getter ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite( priv->getter ));

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_begin_changed ), self );

	/* beginning date of the exercice cannot be modified if at least one
	 * account has an opening balance (main reason is that we do not know
	 * how to remediate this ;)
	 * it cannot be modified either if the end date of the previous exercice is set */
	my_date_set_from_date( &priv->begin_init, ofo_dossier_get_exe_begin( priv->dossier ));

	/* ending date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-end-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-end-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_set_from_date( &priv->end, ofo_dossier_get_exe_end( priv->dossier ));
	priv->end_empty = !my_date_is_valid( &priv->end );
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_entry_format( GTK_EDITABLE( entry ), ofa_prefs_date_display( priv->getter ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_check( priv->getter ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite( priv->getter ));

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_end_changed ), self );

	my_date_set_from_date( &priv->end_init, ofo_dossier_get_exe_end( priv->dossier ));

	/* last closed periode */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-per-closed-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	last_closed = ofo_dossier_get_last_closing_date( priv->dossier );
	str = my_date_is_valid( last_closed ) ? my_date_to_str( last_closed, ofa_prefs_date_display( priv->getter )) : NULL;
	gtk_label_set_text( GTK_LABEL( label ), str ? str : "" );
	g_free( str );

	/* the end of the exercice cannot be rewinded back before the last
	 * close of the ledgers or the last closed period */
	ofo_ledger_get_max_last_close( priv->getter, &priv->min_end );
	if( my_date_is_valid( last_closed ) && my_date_compare( last_closed, &priv->min_end ) > 0 ){
		my_date_set_from_date( &priv->min_end, last_closed );
	}
}

static void
init_exercice_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *parent, *label;
	gchar *str;
	const GDate *last_end;
	GtkSizeGroup *hgroup, *group_bin;

	priv = ofa_dossier_properties_get_instance_private( self );

	/* last closed exercice */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-exe-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	last_end = ofo_dossier_get_prevexe_end( priv->dossier );
	str = my_date_to_str( last_end, ofa_prefs_date_display( priv->getter ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	/* last entry from previous exercice */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-entry-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_prevexe_last_entry( priv->dossier ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	/* closing parms */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->closing_parms = ofa_closing_parms_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->closing_parms ));
	g_signal_connect(
			priv->closing_parms, "ofa-changed", G_CALLBACK( on_closing_parms_changed ), self );

	/* manage the horizontal alignment */
	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-exe-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-entry-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->closing_parms ), 0 ))){
		my_utils_size_group_add_size_group( hgroup, group_bin );
	}

	g_object_unref( hgroup );
}

static void
init_exe_notes_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *textview;
	GtkTextBuffer *buffer;

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->exe_notes = g_strdup( ofo_dossier_get_exe_notes( priv->dossier ));
	textview = my_utils_container_notes_setup_full(
			GTK_CONTAINER( self ), "pexe-notes", priv->exe_notes, priv->is_writable );
	g_return_if_fail( textview && GTK_IS_TEXT_VIEW( textview ));

	if( priv->is_writable ){
		buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( textview ));
		g_return_if_fail( buffer && GTK_IS_TEXT_BUFFER( buffer ));
		g_signal_connect( buffer, "changed", G_CALLBACK( on_notes_changed ), self );
	}
}

/*
 * when set, these preferences for the dossier override those of the
 * user preferences
 */
static void
init_preferences_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;
	ofaIDBDossierMeta *dossier_meta;
	GtkWidget *parent, *label;
	gchar *uri;

	priv = ofa_dossier_properties_get_instance_private( self );

	/* apply actions on open */
	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	dossier_meta = ofa_idbconnect_get_dossier_meta( connect );
	priv->settings_iface = ofa_idbdossier_meta_get_settings_iface( dossier_meta );
	priv->settings_group = ofa_idbdossier_meta_get_settings_group( dossier_meta );
	priv->prefs = ofa_open_prefs_new( priv->settings_iface, priv->settings_group, OPEN_PREFS_DOSSIER_KEY );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "prefs-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->prefs_bin = ofa_open_prefs_bin_new( priv->prefs );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->prefs_bin ));

	/* background image */
	priv->background_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-filechooserbutton" );
	g_return_if_fail( priv->background_btn && GTK_IS_FILE_CHOOSER_BUTTON( priv->background_btn ));
	g_signal_connect( priv->background_btn, "file-set", G_CALLBACK( background_image_on_file_set ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-background-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->background_btn );

	priv->background_clear = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-clear" );
	g_return_if_fail( priv->background_clear && GTK_IS_BUTTON( priv->background_clear ));
	g_signal_connect( priv->background_clear, "clicked", G_CALLBACK( background_image_on_clear_clicked ), self );

	priv->background_preview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-preview" );
	g_return_if_fail( priv->background_preview && GTK_IS_BUTTON( priv->background_preview ));
	g_signal_connect( priv->background_preview, "clicked", G_CALLBACK( background_image_on_preview_clicked ), self );

	uri = my_isettings_get_string( priv->settings_iface, priv->settings_group, DOSSIER_BACKGROUND_KEY );
	priv->background_orig_uri = g_strdup( uri );
	if( my_strlen( uri )){
		gtk_file_chooser_set_uri( GTK_FILE_CHOOSER( priv->background_btn ), uri );
	}
	g_free( uri );

	background_image_on_file_set( GTK_FILE_CHOOSER_BUTTON( priv->background_btn ), self );
}

static void
init_internals_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *label;
	ofaHub *hub;
	ofaIDBConnect *connect;
	ofaIDBModel *model;
	gchar *str;

	priv = ofa_dossier_properties_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-bat" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_bat_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-batline" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_batline_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-entry" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_entry_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-ope" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_ope_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-settlement" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_settlement_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-concil" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_concil_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-doc" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_doc_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-last-tiers" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_counters_get_last_tiers_id( priv->getter ), priv->getter );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	/* dbms version */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p6-version" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	model = ofa_idbmodel_get_by_name( priv->getter, "CORE" );
	if( model ){
		hub = ofa_igetter_get_hub( priv->getter );
		connect = ofa_hub_get_connect( hub );
		str = ofa_idbmodel_get_version( model, OFA_IDBCONNECT( connect ));
		if( my_strlen( str )){
			gtk_label_set_text( GTK_LABEL( label ), str );
		}
		g_free( str );
	}
}

static void
on_label_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label2_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	g_free( priv->label2 );
	priv->label2 = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyCombo signal cb
 */
static void
on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	g_free( priv->currency );
	priv->currency = g_strdup( code );

	check_for_enable_dlg( self );
}

/*
 * ofaLedgerCombo signal cb
 */
static void
on_import_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	g_free( priv->import_ledger );
	priv->import_ledger = g_strdup( mnemo );

	check_for_enable_dlg( self );
}

static void
on_duree_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	const gchar *text;

	priv = ofa_dossier_properties_get_instance_private( self );

	text = gtk_entry_get_text( entry );
	if( my_strlen( text )){
		priv->duree = atoi( text );
	}

	check_for_enable_dlg( self );
}

static void
on_begin_changed( GtkEditable *editable, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	on_date_changed( self, editable, &priv->begin, &priv->begin_empty );
}

static void
on_end_changed( GtkEditable *editable, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	on_date_changed( self, editable, &priv->end, &priv->end_empty );
}

static void
on_date_changed( ofaDossierProperties *self, GtkEditable *editable, GDate *date, gboolean *is_empty )
{
	gchar *content;
	gboolean valid;

	content = gtk_editable_get_chars( editable, 0, -1 );
	if( my_strlen( content )){
		*is_empty = FALSE;
		my_date_set_from_date( date, my_date_editable_get_date( GTK_EDITABLE( editable ), &valid ));
		/*g_debug( "ofa_dossier_properties_on_date_changed: is_empty=False, valid=%s", valid ? "True":"False" );*/

	} else {
		*is_empty = TRUE;
		my_date_clear( date );
	}
	g_free( content );

	check_for_enable_dlg( self );
}

static void
on_closing_parms_changed( ofaClosingParmsBin *bin, ofaDossierProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_notes_changed( GtkTextBuffer *buffer, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkTextIter start, end;

	priv = ofa_dossier_properties_get_instance_private( self );

	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	g_free( priv->exe_notes );
	priv->exe_notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
}

static void
background_image_on_file_set( GtkFileChooserButton *button, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gchar *uri;
	gboolean has_uri;

	priv = ofa_dossier_properties_get_instance_private( self );

	uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( button ));
	has_uri = ( my_strlen( uri ) > 0 );

	gtk_widget_set_sensitive( priv->background_clear, has_uri );

	g_free( uri );
}

static void
background_image_on_clear_clicked( GtkButton *button, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gchar *uri;

	priv = ofa_dossier_properties_get_instance_private( self );

	uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->background_btn ));
	if( uri ){
		gtk_file_chooser_unselect_uri( GTK_FILE_CHOOSER( priv->background_btn ), uri );
	}
	g_free( uri );
}

static void
background_image_on_preview_clicked( GtkButton *button, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	ofaISignaler *signaler;
	gchar *uri;

	priv = ofa_dossier_properties_get_instance_private( self );

	uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->background_btn ));

	signaler = ofa_igetter_get_signaler( priv->getter );

	g_signal_emit_by_name( signaler, SIGNALER_DOSSIER_PREVIEW, uri );

	g_free( uri );
}

static void
check_for_enable_dlg( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_valid( self ));
	}
}

static gboolean
is_dialog_valid( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gchar *msg, *sdate;

	priv = ofa_dossier_properties_get_instance_private( self );

	set_msgerr( self, "", MSG_NORMAL );

	if( !priv->begin_empty && !my_date_is_valid( &priv->begin )){
		set_msgerr( self, _( "Not empty and not valid exercice beginning date" ), MSG_ERROR );
		return( FALSE );
	}

	if( !priv->end_empty ){
		if( !my_date_is_valid( &priv->end )){
			set_msgerr( self, _( "Not empty and not valid exercice ending date" ), MSG_ERROR );
			return( FALSE );

		} else if( my_date_is_valid( &priv->min_end ) && my_date_compare( &priv->min_end, &priv->end ) >=0 ){
			sdate = my_date_to_str( &priv->min_end, ofa_prefs_date_display( priv->getter ));
			msg = g_strdup_printf( _( "Invalid end of the exercice before or equal to the ledger last closure %s" ), sdate );
			set_msgerr( self, msg, MSG_ERROR );
			g_free( msg );
			g_free( sdate );
			return( FALSE );
		}
	}

	if( !ofo_dossier_is_valid_data(
				priv->label, priv->duree, priv->currency, &priv->begin, &priv->end, &msg )){
		set_msgerr( self, msg, MSG_ERROR );
		g_free( msg );
		return( FALSE );
	}

	if( !my_ibin_is_valid( MY_IBIN( priv->prefs_bin ), &msg )){
		set_msgerr( self, msg, MSG_ERROR );
		g_free( msg );
		return( FALSE );
	}

	if( priv->closing_parms ){
		if( !my_ibin_is_valid( MY_IBIN( priv->closing_parms ), &msg )){
			set_msgerr( self, msg, MSG_WARNING );
			g_free( msg );
			/* doesn't refuse to validate the dialog here
			 * as this is only mandatory when closing the exercice */
			return( TRUE );
		}
	}

	if( !my_strlen( priv->import_ledger )){
		set_msgerr( self, _( "Default import ledger empty" ), MSG_WARNING );
	}

	return( TRUE );
}

static void
set_msgerr( ofaDossierProperties *self, const gchar *msg, const gchar *spec )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	my_style_remove( priv->msgerr, MSG_ERROR );
	my_style_remove( priv->msgerr, MSG_WARNING );

	if( priv->msgerr ){
		gtk_label_set_text( GTK_LABEL( priv->msgerr ), msg );
		my_style_add( priv->msgerr, spec );
	}
}

/*
 * on cancel, restore the original background image
 */
static void
on_cancel_clicked( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	ofaISignaler *signaler;

	priv = ofa_dossier_properties_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	g_signal_emit_by_name( signaler, SIGNALER_DOSSIER_PREVIEW, priv->background_orig_uri );

	/* do not close the window here as this will be done by myIDialog */
}

static void
on_ok_clicked( ofaDossierProperties *self )
{
	gchar *msgerr = NULL;

	do_update( self, &msgerr );

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

static gboolean
do_update( ofaDossierProperties *self, gchar **msgerr )
{
	ofaDossierPropertiesPrivate *priv;
	ofaISignaler *signaler;
	gboolean date_has_changed;
	gint count;
	gboolean ok;
	gchar *uri;

	g_return_val_if_fail( is_dialog_valid( self ), FALSE );

	priv = ofa_dossier_properties_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_label2( priv->dossier, priv->label2 );
	ofo_dossier_set_siren( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->siren_entry )));
	ofo_dossier_set_siret( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->siret_entry )));
	ofo_dossier_set_vatic( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->vatic_entry )));
	ofo_dossier_set_naf( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->naf_entry )));
	ofo_dossier_set_default_currency( priv->dossier, priv->currency );
	ofo_dossier_set_import_ledger( priv->dossier, priv->import_ledger );
	ofo_dossier_set_exe_length( priv->dossier, priv->duree );
	ofo_dossier_set_exe_begin( priv->dossier, &priv->begin );
	ofo_dossier_set_exe_end( priv->dossier, &priv->end );

	ofa_closing_parms_bin_apply( priv->closing_parms );

	ofo_dossier_set_exe_notes( priv->dossier, priv->exe_notes );
	my_utils_container_notes_get( self, dossier );

	/* have begin or end exe dates changed ? */
	date_has_changed = FALSE;
	count = 0;

	if( my_date_is_valid( &priv->begin_init )){
		if( !my_date_is_valid( &priv->begin ) || my_date_compare( &priv->begin_init, &priv->begin )){
			date_has_changed = TRUE;
		}
	} else if( my_date_is_valid( &priv->begin )){
		date_has_changed = TRUE;
	}

	if( my_date_is_valid( &priv->end_init )){
		if( !my_date_is_valid( &priv->end ) || my_date_compare( &priv->end_init, &priv->end )){
			date_has_changed = TRUE;
		}
	} else if( my_date_is_valid( &priv->end )){
		date_has_changed = TRUE;
	}

	if( date_has_changed ){
		count = ofo_entry_get_exe_changed_count(
				priv->getter, &priv->begin_init, &priv->end_init, &priv->begin, &priv->end );
		if( count > 0 && !confirm_remediation( self, count )){
			*msgerr = g_strdup( _( "Update has been cancelled by the user" ));
			return( FALSE );
		}
	}

	/* first update the dossier, and only then send the advertising signal */
	ok = ofo_dossier_update( priv->dossier );
	if( !ok ){
		*msgerr = g_strdup( _( "Unable to update the dossier" ));
		return( FALSE );
	}

	if( count > 0 ){
		display_progress_init( self );
		g_signal_emit_by_name(
				signaler, SIGNALER_EXERCICE_DATES_CHANGED, &priv->begin_init, &priv->end_init );
		display_progress_end( self );
	}

	/* record standard actions on open */
	my_ibin_apply( MY_IBIN( priv->prefs_bin ));

	/* background image */
	uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->background_btn ));
	my_isettings_set_string( priv->settings_iface, priv->settings_group, DOSSIER_BACKGROUND_KEY, uri );
	g_free( uri );

	/* last, advertize the dossier changes */
	g_signal_emit_by_name( signaler, SIGNALER_DOSSIER_CHANGED );

	return( ok );
}

static gboolean
confirm_remediation( ofaDossierProperties *self, gint count )
{
	gboolean ok;
	gchar *str;
	GtkWindow *toplevel;

	str = g_strdup_printf(
			_( "You have modified the begin and/or the end dates of the current exercice.\n"
				"This operation will lead to the remediation of %d entries, "
				"as each one must update its intern status and thus "
				"update the corresponding account and ledger balances.\n"
				"Are your sure ?" ), count );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "Con_firm" ));

	g_free( str );

	return( ok );
}

static void
display_progress_init( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *content, *grid, *widget;

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->dialog = gtk_dialog_new_with_buttons(
							_( "Remediating entries" ),
							GTK_WINDOW( self ),
							GTK_DIALOG_MODAL,
							_( "_Close" ), GTK_RESPONSE_OK,
							NULL );

	priv->button = gtk_dialog_get_widget_for_response( GTK_DIALOG( priv->dialog ), GTK_RESPONSE_OK );
	g_return_if_fail( priv->button && GTK_IS_BUTTON( priv->button ));
	gtk_widget_set_sensitive( priv->button, FALSE );

	content = gtk_dialog_get_content_area( GTK_DIALOG( priv->dialog ));
	g_return_if_fail( content && GTK_IS_CONTAINER( content ));

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 3 );
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );
	gtk_container_add( GTK_CONTAINER( content ), grid );

	widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_widget_set_margins( widget, 2, 2, 10, 10 );
	gtk_grid_attach( GTK_GRID( grid ), widget, 0, 0, 1, 1 );

	priv->bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->bar ));

	signaler_connect_to_signaling_system( self );

	gtk_widget_show_all( priv->dialog );
}

static void
display_progress_end( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	gtk_widget_set_sensitive( priv->button, TRUE );

	gtk_dialog_run( GTK_DIALOG( priv->dialog ));
	gtk_widget_destroy( priv->dialog );
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_dossier_properties_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_STATUS_COUNT, G_CALLBACK( signaler_on_entry_status_count ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_STATUS_CHANGE, G_CALLBACK( signaler_on_entry_status_change ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_STATUS_COUNT signal handler
 */
static void
signaler_on_entry_status_count( ofaISignaler *signaler, ofeEntryStatus new_status, gulong count, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->total = count;
	priv->count = 0;
}

/*
 * SIGNALER_STATUS_CHANGE signal handler
 */
static void
signaler_on_entry_status_change( ofaISignaler *signaler, ofoEntry *entry, ofeEntryStatus prev_status, ofeEntryStatus new_status, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gdouble progress;
	gchar *text;

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->count += 1;
	progress = ( gdouble ) priv->count / ( gdouble ) priv->total;
	text = g_strdup_printf( "%lu/%lu", priv->count, priv->total );

	g_signal_emit_by_name( priv->bar, "my-double", progress );
	g_signal_emit_by_name( priv->bar, "my-text", text );

	g_free( text );
}
