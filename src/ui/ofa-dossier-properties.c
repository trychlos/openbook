/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "core/ofo-dossier-ddl.h"

#include "ui/my-editable-date.h"
#include "ui/my-progress-bar.h"
#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-currency-combo.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ledger-combo.h"

/* private instance data
 */
struct _ofaDossierPropertiesPrivate {

	/* internals
	 */
	ofoDossier         *dossier;
	gboolean            is_new;
	gboolean            updated;
	gboolean            is_current;
	GDate               begin_init;
	GDate               end_init;
	GDate               min_end;

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
	GtkWidget          *msgerr;
	GtkWidget          *ok_btn;

	/* when remediating entries
	 */
	GtkWidget          *dialog;
	GtkWidget          *button;
	myProgressBar      *bar;
	gulong              total;
	gulong              count;
	GList              *dos_handlers;
};

#define MSG_NORMAL                      "#000000"
#define MSG_WARNING                     "#ff8000"
#define MSG_ERROR                       "#ff0000"

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-properties.ui";
static const gchar *st_ui_id            = "DossierPropertiesDlg";

G_DEFINE_TYPE( ofaDossierProperties, ofa_dossier_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_properties_page( ofaDossierProperties *self, GtkContainer *container );
static void      init_forward_page( ofaDossierProperties *self, GtkContainer *container );
static void      init_exe_notes_page( ofaDossierProperties *self, GtkContainer *container );
static void      init_counters_page( ofaDossierProperties *self, GtkContainer *container );
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
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaDossierProperties *self );
static gboolean  confirm_remediation( ofaDossierProperties *self, gint count );
static void      display_progress_init( ofaDossierProperties *self );
static void      display_progress_end( ofaDossierProperties *self );
static void      on_entry_status_count( ofoDossier *dossier, ofaEntryStatus new_status, gulong count, ofaDossierProperties *self );
static void      on_entry_status_changed( ofoDossier *dossier, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaDossierProperties *self );

static void
dossier_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_finalize";
	ofaDossierPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_PROPERTIES( instance )->priv;

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
	GList *it;

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_DOSSIER_PROPERTIES( instance )->priv;

		if( priv->dossier && OFO_IS_DOSSIER( priv->dossier )){
			for( it=priv->dos_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->dossier, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->dispose( instance );
}

static void
ofa_dossier_properties_init( ofaDossierProperties *self )
{
	static const gchar *thisfn = "ofa_dossier_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_DOSSIER_PROPERTIES, ofaDossierPropertiesPrivate );

	self->priv->updated = FALSE;
	my_date_clear( &self->priv->begin );
	my_date_clear( &self->priv->end );
	self->priv->dos_handlers = NULL;
}

static void
ofa_dossier_properties_class_init( ofaDossierPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierPropertiesPrivate ));
}

/**
 * ofa_dossier_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an dossier
 */
gboolean
ofa_dossier_properties_run( ofaMainWindow *main_window, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofa_dossier_properties_run";
	ofaDossierProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, dossier=%p",
			thisfn, ( void * ) main_window, ( void * ) dossier );

	self = g_object_new(
				OFA_TYPE_DOSSIER_PROPERTIES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->dossier = dossier;

	my_dialog_run_dialog( MY_DIALOG( self ));
	g_debug( "ofa_dossier_properties_run: return from run" );

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierProperties *self;
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;

	self = OFA_DOSSIER_PROPERTIES( dialog );
	priv = self->priv;
	priv->is_current = ofo_dossier_is_current( priv->dossier );

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	init_properties_page( self, container );
	init_forward_page( self, container );
	init_exe_notes_page( self, container );
	init_counters_page( self, container );

	/* these are main notes of the dossier */
	my_utils_init_notes_ex( container, dossier, priv->is_current );
	my_utils_init_upd_user_stamp_ex( container, dossier );

	priv->msgerr = my_utils_container_get_child_by_name( container, "px-msgerr" );

	if( !priv->is_current ){
		my_dialog_set_readonly_buttons( dialog );
	}

	check_for_enable_dlg( self );
}

static void
init_properties_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkWidget *entry, *label, *parent;
	gchar *str;
	ofaCurrencyCombo *c_combo;
	ofaLedgerCombo *l_combo;
	const gchar *cstr;
	gint ivalue;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	entry = my_utils_container_get_child_by_name( container, "p1-label" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );
	cstr = ofo_dossier_get_label( priv->dossier );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
	gtk_widget_set_can_focus( entry, priv->is_current );

	entry = my_utils_container_get_child_by_name( container, "p1-siren" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->siren = g_strdup( ofo_dossier_get_siren( priv->dossier ));
	if( priv->siren ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->siren );
	}
	priv->siren_entry = entry;
	gtk_widget_set_can_focus( entry, priv->is_current );

	parent = my_utils_container_get_child_by_name( container, "p1-currency-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	c_combo = ofa_currency_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( c_combo ));
	ofa_currency_combo_set_columns( c_combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_main_window( c_combo, OFA_MAIN_WINDOW( main_window ));
	g_signal_connect( c_combo, "ofa-changed", G_CALLBACK( on_currency_changed ), self );
	ofa_currency_combo_set_selected( c_combo, ofo_dossier_get_default_currency( priv->dossier ));
	if( !priv->is_current ){
		gtk_combo_box_set_button_sensitivity( GTK_COMBO_BOX( c_combo ), GTK_SENSITIVITY_OFF );
	}

	parent = my_utils_container_get_child_by_name( container, "p1-ledger-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	l_combo = ofa_ledger_combo_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( l_combo ));
	ofa_ledger_combo_set_columns( l_combo, LEDGER_DISP_LABEL );
	ofa_ledger_combo_set_main_window( l_combo, OFA_MAIN_WINDOW( main_window ));
	g_signal_connect( l_combo, "ofa-changed", G_CALLBACK( on_import_ledger_changed ), self );
	ofa_ledger_combo_set_selected( l_combo, ofo_dossier_get_import_ledger( priv->dossier ));
	if( !priv->is_current ){
		gtk_combo_box_set_button_sensitivity( GTK_COMBO_BOX( l_combo ), GTK_SENSITIVITY_OFF );
	}

	label = my_utils_container_get_child_by_name( container, "p1-status" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), ofo_dossier_get_status_str( priv->dossier ));

	entry = my_utils_container_get_child_by_name( container, "p1-exe-length" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_duree_changed ), self );
	ivalue = ofo_dossier_get_exe_length( priv->dossier );
	str = g_strdup_printf( "%d", ivalue );
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	gtk_widget_set_can_focus( entry, priv->is_current );

	entry = my_utils_container_get_child_by_name( container, "pexe-begin" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_begin_changed ), self );
	my_date_set_from_date( &priv->begin, ofo_dossier_get_exe_begin( priv->dossier ));
	priv->begin_empty = !my_date_is_valid( &priv->begin );
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->begin );
	/* beginning date of the exercice cannot be modified if at least one
	 * account has an opening balance (main reason is that we do not know
	 * how to remediate this ;) */
	gtk_widget_set_can_focus( entry,
			priv->is_current || ofo_account_has_open_balance( priv->dossier ));
	my_date_set_from_date( &priv->begin_init, ofo_dossier_get_exe_begin( priv->dossier ));

	entry = my_utils_container_get_child_by_name( container, "pexe-end" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_end_changed ), self );
	my_date_set_from_date( &priv->end, ofo_dossier_get_exe_end( priv->dossier ));
	priv->end_empty = !my_date_is_valid( &priv->end );
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->end );
	gtk_widget_set_can_focus( entry, priv->is_current );
	my_date_set_from_date( &priv->end_init, ofo_dossier_get_exe_end( priv->dossier ));

	/* the end of the exercice cannot be rewinf back before the last
	 * close of the ledgers */
	ofo_ledger_get_max_last_close( &priv->min_end, priv->dossier );
}

static void
init_forward_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkWidget *parent;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	parent = my_utils_container_get_child_by_name( container, "p5-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->closing_parms = ofa_closing_parms_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->closing_parms ));
	ofa_closing_parms_bin_set_main_window( priv->closing_parms, OFA_MAIN_WINDOW( main_window ));

	g_signal_connect( priv->closing_parms, "changed", G_CALLBACK( on_closing_parms_changed ), self );
}

static void
init_exe_notes_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GObject *buffer;

	priv = self->priv;
	priv->exe_notes = g_strdup( ofo_dossier_get_exe_notes( priv->dossier ));
	buffer = my_utils_init_notes( container, "pexe-notes", priv->exe_notes, priv->is_current );
	g_return_if_fail( buffer && GTK_IS_TEXT_BUFFER( buffer ));

	if( priv->is_current ){
		g_signal_connect( buffer, "changed", G_CALLBACK( on_notes_changed ), self );
	}
}

static void
init_counters_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *label;
	gchar *str;

	priv = self->priv;

	label = my_utils_container_get_child_by_name( container, "p4-last-bat" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_bigint_to_str( ofo_dossier_get_last_bat( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p4-last-batline" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_bigint_to_str( ofo_dossier_get_last_batline( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p4-last-entry" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_bigint_to_str( ofo_dossier_get_last_entry( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p4-last-settlement" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_bigint_to_str( ofo_dossier_get_last_settlement( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p4-last-concil" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_bigint_to_str( ofo_dossier_get_last_concil( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p5-version" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%u", ( ofo_dossier_ddl_get_version( priv->dossier )));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );
}

static void
on_label_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyCombo signal cb
 */
static void
on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

	g_free( priv->import_ledger );
	priv->import_ledger = g_strdup( mnemo );

	check_for_enable_dlg( self );
}

static void
on_duree_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	const gchar *text;

	text = gtk_entry_get_text( entry );
	if( my_strlen( text )){
		self->priv->duree = atoi( text );
	}

	check_for_enable_dlg( self );
}

static void
on_begin_changed( GtkEditable *editable, ofaDossierProperties *self )
{
	on_date_changed( self, editable, &self->priv->begin, &self->priv->begin_empty );
}

static void
on_end_changed( GtkEditable *editable, ofaDossierProperties *self )
{
	on_date_changed( self, editable, &self->priv->end, &self->priv->end_empty );
}

static void
on_date_changed( ofaDossierProperties *self, GtkEditable *editable, GDate *date, gboolean *is_empty )
{
	gchar *content;
	gboolean valid;

	content = gtk_editable_get_chars( editable, 0, -1 );
	if( my_strlen( content )){
		*is_empty = FALSE;
		my_date_set_from_date( date, my_editable_date_get_date( GTK_EDITABLE( editable ), &valid ));
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

	priv = self->priv;

	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	g_free( priv->exe_notes );
	priv->exe_notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
}

static void
check_for_enable_dlg( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gboolean ok;

	priv = self->priv;

	if( priv->is_current ){

		if( !priv->ok_btn ){
			priv->ok_btn = my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );
			g_return_if_fail( priv->ok_btn && GTK_IS_WIDGET( priv->ok_btn ));
		}

		ok = is_dialog_valid( self );
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

static gboolean
is_dialog_valid( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gchar *msg, *sdate;

	priv = self->priv;
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

	if( !ofo_dossier_is_valid(
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
	GdkRGBA color;

	priv = self->priv;

	if( priv->msgerr ){
		gtk_label_set_text( GTK_LABEL( priv->msgerr ), msg );
		gdk_rgba_parse( &color, spec );
		gtk_widget_override_color( priv->msgerr, GTK_STATE_FLAG_NORMAL, &color );
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_DOSSIER_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkContainer *container;
	gboolean date_has_changed;
	gint count;

	g_return_val_if_fail( is_dialog_valid( self ), FALSE );

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_siren( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->siren_entry )));
	ofo_dossier_set_default_currency( priv->dossier, priv->currency );
	ofo_dossier_set_import_ledger( priv->dossier, priv->import_ledger );
	ofo_dossier_set_exe_length( priv->dossier, priv->duree );
	ofo_dossier_set_exe_begin( priv->dossier, &priv->begin );
	ofo_dossier_set_exe_end( priv->dossier, &priv->end );

	ofa_closing_parms_bin_apply( priv->closing_parms );

	ofo_dossier_set_exe_notes( priv->dossier, priv->exe_notes );
	my_utils_getback_notes_ex( container, dossier );

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
				priv->dossier, &priv->begin_init, &priv->end_init, &priv->begin, &priv->end );
		if( count > 0 && !confirm_remediation( self, count )){
			return( FALSE );
		}
	}

	/* first update the dossier, and only then send the advertising signal */
	priv->updated = ofo_dossier_update( priv->dossier );

	if( count > 0 ){
		ofa_main_window_update_title( OFA_MAIN_WINDOW( main_window ));
		display_progress_init( self );
		g_signal_emit_by_name(
				priv->dossier, SIGNAL_DOSSIER_EXE_DATE_CHANGED, &priv->begin_init, &priv->end_init );
		display_progress_end( self );
	}

	return( priv->updated );
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
	GtkWidget *content, *grid, *alignment;
	gulong handler;

	priv = self->priv;

	priv->dialog = gtk_dialog_new_with_buttons(
					_( "Remediating entries" ),
					my_window_get_toplevel( MY_WINDOW( self )),
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

	alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_alignment_set_padding( GTK_ALIGNMENT( alignment ), 2, 2, 10, 10 );
	gtk_grid_attach( GTK_GRID( grid ), alignment, 0, 0, 1, 1 );

	priv->bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( priv->bar ));

	handler = g_signal_connect( priv->dossier,
			SIGNAL_DOSSIER_ENTRY_STATUS_COUNT, G_CALLBACK( on_entry_status_count ), self );
	priv->dos_handlers = g_list_prepend( priv->dos_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->dossier,
			SIGNAL_DOSSIER_ENTRY_STATUS_CHANGED, G_CALLBACK( on_entry_status_changed ), self );
	priv->dos_handlers = g_list_prepend( priv->dos_handlers, ( gpointer ) handler );

	gtk_widget_show_all( priv->dialog );
}

static void
display_progress_end( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = self->priv;

	gtk_widget_set_sensitive( priv->button, TRUE );

	gtk_dialog_run( GTK_DIALOG( priv->dialog ));
	gtk_widget_destroy( priv->dialog );
}

static void
on_entry_status_count( ofoDossier *dossier, ofaEntryStatus new_status, gulong count, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	priv = self->priv;

	priv->total = count;
	priv->count = 0;
}

static void
on_entry_status_changed( ofoDossier *dossier, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gdouble progress;
	gchar *text;

	priv = self->priv;

	priv->count += 1;
	progress = ( gdouble ) priv->count / ( gdouble ) priv->total;
	text = g_strdup_printf( "%lu/%lu", priv->count, priv->total );

	g_signal_emit_by_name( priv->bar, "ofa-double", progress );
	g_signal_emit_by_name( priv->bar, "ofa-text", text );

	g_free( text );
}
