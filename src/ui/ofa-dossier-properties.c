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

#include "my/my-date-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-counter.h"
#include "api/ofa-dossier-prefs.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "core/ofa-open-prefs-bin.h"
#include "core/ofa-currency-combo.h"
#include "core/ofa-ledger-combo.h"
#include "core/ofa-main-window.h"

#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-dossier-properties.h"

/* private instance data
 */
struct _ofaDossierPropertiesPrivate {
	gboolean            dispose_has_run;

	/* initialization
	 */

	/* runtime
	 */
	ofaHub             *hub;
	ofoDossier         *dossier;
	gboolean            is_new;
	gboolean            is_current;
	GDate               begin_init;
	GDate               end_init;
	GDate               min_end;
	ofaDossierPrefs    *prefs;

	/* data
	 */
	gchar              *label;
	gchar              *siren;
	gchar              *currency;
	gchar              *import_ledger;
	GDate               begin;
	gboolean            begin_empty;
	GDate               end;
	gboolean            end_empty;
	gint                duree;
	gchar              *exe_notes;

	/* UI
	 */
	GtkWidget          *siren_entry;
	ofaClosingParmsBin *closing_parms;
	ofaOpenPrefsBin    *prefs_bin;
	GtkWidget          *msgerr;
	GtkWidget          *ok_btn;

	/* when remediating entries
	 */
	GtkWidget          *dialog;
	GtkWidget          *button;
	myProgressBar      *bar;
	gulong              total;
	gulong              count;
	GList              *hub_handlers;
};

#define MSG_NORMAL                      "labelnormal"
#define MSG_WARNING                     "labelwarning"
#define MSG_ERROR                       "labelerror"

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      init_properties_page( ofaDossierProperties *self );
static void      init_forward_page( ofaDossierProperties *self );
static void      init_exe_notes_page( ofaDossierProperties *self );
static void      init_counters_page( ofaDossierProperties *self );
static void      init_preferences_page( ofaDossierProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierProperties *self );
static void      on_import_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaDossierProperties *self );
static void      on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_begin_changed( GtkEditable *editable, ofaDossierProperties *self );
static void      on_end_changed( GtkEditable *editable, ofaDossierProperties *self );
static void      on_date_changed( ofaDossierProperties *self, GtkEditable *editable, GDate *date, gboolean *is_empty );
static void      on_closing_parms_changed( ofaClosingParmsBin *bin, ofaDossierProperties *self );
static void      on_notes_changed( GtkTextBuffer *buffer, ofaDossierProperties *self );
static void      check_for_enable_dlg( ofaDossierProperties *self );
static gboolean  is_dialog_valid( ofaDossierProperties *self );
static void      set_msgerr( ofaDossierProperties *self, const gchar *msg, const gchar *spec );
static gboolean  do_update( ofaDossierProperties *self, gchar **msgerr );
static gboolean  confirm_remediation( ofaDossierProperties *self, gint count );
static void      display_progress_init( ofaDossierProperties *self );
static void      display_progress_end( ofaDossierProperties *self );
static void      on_hub_entry_status_count( ofaHub *hub, ofaEntryStatus new_status, gulong count, ofaDossierProperties *self );
static void      on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaDossierProperties *self );

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
	g_free( priv->currency );
	g_free( priv->import_ledger );
	g_free( priv->exe_notes );
	g_free( priv->siren );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->finalize( instance );
}

static void
dossier_properties_dispose( GObject *instance )
{
	ofaDossierPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	priv = ofa_dossier_properties_get_instance_private( OFA_DOSSIER_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		ofa_hub_disconnect_handlers( priv->hub, priv->hub_handlers );
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
	priv->hub_handlers = NULL;

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
 * @main_window: the main window of the application.
 *
 * Update the properties of an dossier
 */
void
ofa_dossier_properties_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_properties_run";
	ofaDossierProperties *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_PROPERTIES, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

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
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * dossier
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_idialog_init";
	ofaDossierPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_properties_get_instance_private( OFA_DOSSIER_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	priv->msgerr = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "px-msgerr" );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	priv->is_current = ofo_dossier_is_current( priv->dossier );

	init_properties_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_forward_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_exe_notes_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_counters_page( OFA_DOSSIER_PROPERTIES( instance ));
	init_preferences_page( OFA_DOSSIER_PROPERTIES( instance ));

	/* these are main notes of the dossier */
	my_utils_container_notes_init( instance, dossier );
	my_utils_container_updstamp_init( instance, dossier );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_current );
	if( !priv->is_current ){
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

	priv = ofa_dossier_properties_get_instance_private( self );

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

	/* default currency */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-currency-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	c_combo = ofa_currency_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( c_combo ));
	ofa_currency_combo_set_columns( c_combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_hub( c_combo, priv->hub );
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
	ofa_ledger_combo_set_columns( l_combo, LEDGER_DISP_LABEL );
	ofa_ledger_combo_set_hub( l_combo, priv->hub );
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

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-begin-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-begin-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_set_from_date( &priv->begin, ofo_dossier_get_exe_begin( priv->dossier ));
	priv->begin_empty = !my_date_is_valid( &priv->begin );
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin );

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_begin_changed ), self );

	/* beginning date of the exercice cannot be modified if at least one
	 * account has an opening balance (main reason is that we do not know
	 * how to remediate this ;) */
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
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end );

	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_end_changed ), self );

	my_date_set_from_date( &priv->end_init, ofo_dossier_get_exe_end( priv->dossier ));

	/* last closed periode */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-exe-closed-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	last_closed = ofo_dossier_get_last_closing_date( priv->dossier );
	str = my_date_is_valid( last_closed ) ? my_date_to_str( last_closed, ofa_prefs_date_display()) : NULL;
	gtk_label_set_text( GTK_LABEL( label ), str ? str : "" );
	g_free( str );

	/* the end of the exercice cannot be rewinded back before the last
	 * close of the ledgers or the last closed period */
	ofo_ledger_get_max_last_close( &priv->min_end, priv->hub );
	if( my_date_is_valid( last_closed ) && my_date_compare( last_closed, &priv->min_end ) > 0 ){
		my_date_set_from_date( &priv->min_end, last_closed );
	}
}

static void
init_forward_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *parent;

	priv = ofa_dossier_properties_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->closing_parms = ofa_closing_parms_bin_new( OFA_MAIN_WINDOW( my_iwindow_get_main_window( MY_IWINDOW( self ))));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->closing_parms ));
	g_signal_connect(
			priv->closing_parms, "ofa-changed", G_CALLBACK( on_closing_parms_changed ), self );
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
			GTK_CONTAINER( self ), "pexe-notes", priv->exe_notes, priv->is_current );
	g_return_if_fail( textview && GTK_IS_TEXT_VIEW( textview ));

	if( priv->is_current ){
		buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( textview ));
		g_return_if_fail( buffer && GTK_IS_TEXT_BUFFER( buffer ));
		g_signal_connect( buffer, "changed", G_CALLBACK( on_notes_changed ), self );
	}
}

static void
init_counters_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *label;
	ofaIDBModel *model;
	const ofaIDBConnect *connect;
	gchar *str;

	priv = ofa_dossier_properties_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-bat" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_last_bat( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-batline" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_last_batline( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-entry" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_last_entry( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-settlement" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_last_settlement( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p4-last-concil" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_last_concil( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-last-entry" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = ofa_counter_to_str( ofo_dossier_get_prev_exe_last_entry( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-version" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	model = ofa_idbmodel_get_by_name( "CORE" );
	if( model ){
		connect = ofa_hub_get_connect( priv->hub );
		str = g_strdup_printf( "%u", ( ofa_idbmodel_get_current_version( model, connect )));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
		g_object_unref( model );
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
	GtkWidget *parent;
	gboolean notes, nonempty, props, bals, integ;

	priv = ofa_dossier_properties_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "prefs-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->prefs_bin = ofa_open_prefs_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->prefs_bin ));

	priv->prefs = ofa_hub_get_dossier_prefs( priv->hub );

	notes = ofa_dossier_prefs_get_open_notes( priv->prefs );
	nonempty = ofa_dossier_prefs_get_nonempty( priv->prefs );
	props = ofa_dossier_prefs_get_properties( priv->prefs );
	bals = ofa_dossier_prefs_get_balances( priv->prefs );
	integ = ofa_dossier_prefs_get_integrity( priv->prefs );

	ofa_open_prefs_bin_set_data( priv->prefs_bin, notes, nonempty, props, bals, integ );
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
check_for_enable_dlg( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	if( priv->is_current ){
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
			sdate = my_date_to_str( &priv->min_end, ofa_prefs_date_display());
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

	if( priv->closing_parms ){
		if( !ofa_closing_parms_bin_is_valid( priv->closing_parms, &msg )){
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

	if( priv->msgerr ){
		gtk_label_set_text( GTK_LABEL( priv->msgerr ), msg );
		my_utils_widget_set_style( priv->msgerr, spec );
	}
}

static gboolean
do_update( ofaDossierProperties *self, gchar **msgerr )
{
	ofaDossierPropertiesPrivate *priv;
	gboolean date_has_changed;
	gint count;
	gboolean ok;
	gboolean prefs_notes, prefs_nonempty, prefs_props, prefs_bals, prefs_integ;

	g_return_val_if_fail( is_dialog_valid( self ), FALSE );

	priv = ofa_dossier_properties_get_instance_private( self );

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_siren( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->siren_entry )));
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
				priv->hub, &priv->begin_init, &priv->end_init, &priv->begin, &priv->end );
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
				priv->hub, SIGNAL_HUB_EXE_DATES_CHANGED, &priv->begin_init, &priv->end_init );
		display_progress_end( self );
	}

	g_signal_emit_by_name(
			my_iwindow_get_main_window( MY_IWINDOW( self )),
			OFA_SIGNAL_DOSSIER_CHANGED, priv->dossier );

	/* record settings */
	ofa_open_prefs_bin_get_data( priv->prefs_bin, &prefs_notes, &prefs_nonempty, &prefs_props, &prefs_bals, &prefs_integ );
	ofa_dossier_prefs_set_open_notes( priv->prefs, prefs_notes );
	ofa_dossier_prefs_set_nonempty( priv->prefs, prefs_nonempty );
	ofa_dossier_prefs_set_properties( priv->prefs, prefs_props );
	ofa_dossier_prefs_set_balances( priv->prefs, prefs_bals );
	ofa_dossier_prefs_set_integrity( priv->prefs, prefs_integ );

	return( ok );
}

static gboolean
confirm_remediation( ofaDossierProperties *self, gint count )
{
	gboolean ok;
	gchar *str;

	str = g_strdup_printf(
			_( "You have modified the begin and/or the end dates of the current exercice.\n"
				"This operation will lead to the remediation of %d entries, "
				"as each one must update its intern status and thus "
				"update the corresponding account and ledger balances.\n"
				"Are your sure ?" ), count );

	ok = my_utils_dialog_question( str, _( "Con_firm" ));

	g_free( str );

	return( ok );
}

static void
display_progress_init( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *content, *grid, *widget;
	gulong handler;

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

	handler = g_signal_connect(
					priv->hub,
					SIGNAL_HUB_STATUS_COUNT,
					G_CALLBACK( on_hub_entry_status_count ),
					self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					priv->hub,
					SIGNAL_HUB_STATUS_CHANGE,
					G_CALLBACK( on_hub_entry_status_change ),
					self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

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
 * SIGNAL_HUB_STATUS_COUNT signal handler
 */
static void
on_hub_entry_status_count( ofaHub *hub, ofaEntryStatus new_status, gulong count, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = ofa_dossier_properties_get_instance_private( self );

	priv->total = count;
	priv->count = 0;
}

/*
 * SIGNAL_HUB_STATUS_CHANGE signal handler
 */
static void
on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaDossierProperties *self )
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
