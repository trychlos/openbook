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
#include "api/my-utils.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-currency-combo.h"
#include "ui/ofa-dosexe-combo.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierPropertiesPrivate {

	/* internals
	 */
	ofoDossier      *dossier;
	gboolean         is_new;
	gboolean         updated;

	/* data
	 */
	gchar           *label;
	gint             duree;
	gchar           *currency;
	const GDate     *last_closed;

	/* UI
	 */
	ofaDosexeCombo  *dosexe_combo;

	/* current exercice data
	 */
	gint             exe_id;			/* id of the current exercice */
	GDate            begin;
	gboolean         begin_empty;
	GDate            end;
	gboolean         end_empty;
	gchar           *notes;
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-dossier-properties.ui";
static const gchar *st_ui_id  = "DossierPropertiesDlg";

G_DEFINE_TYPE( ofaDossierProperties, ofa_dossier_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_properties_page( ofaDossierProperties *self );
static void      init_exercices_page( ofaDossierProperties *self );
static void      on_exercice_changed( ofaDosexeCombo *combo, gint exe_id, ofaDossierProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_currency_changed( const gchar *code, ofaDossierProperties *self );
static void      on_begin_changed( GtkEditable *editable, ofaDossierProperties *self );
static void      on_end_changed( GtkEditable *editable, ofaDossierProperties *self );
static void      on_date_changed( ofaDossierProperties *self, GtkEditable *editable, GDate *date, gboolean *is_empty );
static void      on_notes_changed( GtkTextBuffer *buffer, ofaDossierProperties *self );
static void      check_for_enable_dlg( ofaDossierProperties *self );
static gboolean  is_dialog_valid( ofaDossierProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaDossierProperties *self );

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
	g_free( priv->notes );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->finalize( instance );
}

static void
dossier_properties_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
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
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	init_properties_page( self );
	init_exercices_page( self );
	priv->last_closed = ofo_dossier_get_last_closed_exercice( priv->dossier );

	my_utils_init_notes_ex( container, dossier );
	my_utils_init_upd_user_stamp_ex( container, dossier );

	check_for_enable_dlg( self );
}

static void
init_properties_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	GtkWidget *entry;
	gchar *str;
	ofsCurrencyComboParms parms;
	const gchar *costr;
	gint ivalue;

	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	entry = my_utils_container_get_child_by_name( container, "p1-label" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );
	costr = ofo_dossier_get_label( priv->dossier );
	if( costr ){
		gtk_entry_set_text( GTK_ENTRY( entry ), costr );
	}

	entry = my_utils_container_get_child_by_name( container, "p1-exe-length" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_duree_changed ), self );
	ivalue = ofo_dossier_get_exercice_length( priv->dossier );
	str = g_strdup_printf( "%d", ivalue );
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );

	parms.container = container;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-currency";
	parms.label_name = NULL;
	parms.disp_code = TRUE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaCurrencyComboCb ) on_currency_changed;
	parms.user_data = self;
	parms.initial_code = ofo_dossier_get_default_currency( priv->dossier );

	ofa_currency_combo_new( &parms );
}

/*
 * - initialize exercices combobox
 * - connect signals (our entry after having initialized myEditableDate)
 * - store current exercice datas
 */
static void
init_exercices_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	ofaDosexeComboParms parms;
	GtkWidget *widget;
	GtkTextBuffer *buffer;

	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	parms.container = container;
	parms.combo_name = "pexe-combobox";
	parms.main_window = MY_WINDOW( self )->prot->main_window;
	parms.exe_id = -1;

	priv->dosexe_combo = ofa_dosexe_combo_new( &parms );

	g_signal_connect(
			G_OBJECT( priv->dosexe_combo ),
			DOSEXE_SIGNAL_EXE_SELECTED, G_CALLBACK( on_exercice_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "pexe-begin" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	my_editable_date_init( GTK_EDITABLE( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_begin_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "pexe-end" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	my_editable_date_init( GTK_EDITABLE( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_end_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "pexe-notes" );
	g_return_if_fail( widget && GTK_IS_TEXT_VIEW( widget ));
	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( widget ));
	g_signal_connect( G_OBJECT( buffer ), "changed", G_CALLBACK( on_notes_changed ), self );

	priv->exe_id = ofo_dossier_get_current_exe_id( priv->dossier );

	my_date_set_from_date( &priv->begin, ofo_dossier_get_exe_begin( priv->dossier, priv->exe_id ));
	priv->begin_empty = !my_date_is_valid( &priv->begin );

	my_date_set_from_date( &priv->end, ofo_dossier_get_exe_end( priv->dossier, priv->exe_id ));
	priv->end_empty = !my_date_is_valid( &priv->end );

	priv->notes = g_strdup( ofo_dossier_get_exe_notes( priv->dossier, priv->exe_id ));
}

static void
on_exercice_changed( ofaDosexeCombo *combo, gint exe_id, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	GtkWidget *widget;
	ofaDossierStatus status;
	gboolean readonly;
	gchar *str;

	priv = self->priv;
	status = ofo_dossier_get_exe_status( priv->dossier, exe_id );
	readonly = ( status == DOS_STATUS_CLOSED );

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	widget = my_utils_container_get_child_by_name( container, "pexe-id" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	str = g_strdup_printf( "%d", exe_id );
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );

	widget = my_utils_container_get_child_by_name( container, "pexe-begin" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	if( readonly ){
		str = my_date_to_str( ofo_dossier_get_exe_begin( priv->dossier, exe_id ), MY_DATE_DMYY );
		gtk_entry_set_text( GTK_ENTRY( widget ), str );
		g_free( str );
	} else {
		my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
		my_editable_date_set_date( GTK_EDITABLE( widget ), &priv->begin );
	}
	gtk_widget_set_can_focus( widget, !readonly );

	widget = my_utils_container_get_child_by_name( container, "pexe-end" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	if( readonly ){
		str = my_date_to_str( ofo_dossier_get_exe_end( priv->dossier, exe_id ), MY_DATE_DMYY );
		gtk_entry_set_text( GTK_ENTRY( widget ), str );
		g_free( str );
	} else {
		my_editable_date_set_mandatory( GTK_EDITABLE( widget ), FALSE );
		my_editable_date_set_date( GTK_EDITABLE( widget ), &priv->end );
	}
	gtk_widget_set_can_focus( widget, !readonly );

	widget = my_utils_container_get_child_by_name( container, "pexe-status" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	gtk_label_set_text( GTK_LABEL( widget ), ofo_dossier_get_status_label( status ));

	widget = my_utils_container_get_child_by_name( container, "pexe-last-entry" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	str = g_strdup_printf( "%'d", ofo_dossier_get_exe_last_entry( priv->dossier, exe_id ));
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );

	widget = my_utils_container_get_child_by_name( container, "pexe-last-settlement" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	str = g_strdup_printf( "%'d", ofo_dossier_get_exe_last_settlement( priv->dossier, exe_id ));
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );

	widget = my_utils_container_get_child_by_name( container, "pexe-last-bat" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	str = g_strdup_printf( "%'d", ofo_dossier_get_exe_last_bat( priv->dossier, exe_id ));
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );

	widget = my_utils_container_get_child_by_name( container, "pexe-last-bat-line" );
	g_return_if_fail( widget && GTK_IS_LABEL( widget ));
	str = g_strdup_printf( "%'d", ofo_dossier_get_exe_last_bat_line( priv->dossier, exe_id ));
	gtk_label_set_text( GTK_LABEL( widget ), str );
	g_free( str );

	widget = my_utils_container_get_child_by_name( container, "pexe-notes" );
	my_utils_init_notes(
			container, "pexe-notes",
			readonly ? ofo_dossier_get_exe_notes( priv->dossier, exe_id ) : priv->notes );
	gtk_widget_set_can_focus( widget, !readonly );
}

static void
on_label_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_duree_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	const gchar *text;

	text = gtk_entry_get_text( entry );
	if( text && g_utf8_strlen( text, -1 )){
		self->priv->duree = atoi( text );
	}

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyComboCb
 */
static void
on_currency_changed( const gchar *code, ofaDossierProperties *self )
{
	g_free( self->priv->currency );
	self->priv->currency = g_strdup( code );

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
	if( content && g_utf8_strlen( content, -1 )){
		*is_empty = FALSE;
		my_date_set_from_date( date, my_editable_date_get_date( GTK_EDITABLE( editable ), &valid ));
		/*g_debug( "ofa_dossier_properties_on_date_changed: is_empty=False, valid=%s", valid ? "True":"False" );*/

	} else {
		*is_empty = TRUE;
	}
	g_free( content );

	check_for_enable_dlg( self );
}

static void
on_notes_changed( GtkTextBuffer *buffer, ofaDossierProperties *self )
{
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	g_free( self->priv->notes );
	self->priv->notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );

	g_debug( "on_notes_changed: notes='%s'", self->priv->notes );
}

static void
check_for_enable_dlg( ofaDossierProperties *self )
{
	GtkWidget *button;
	gboolean ok;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	/*g_debug( "label=%s, duree=%u, currency=%s", priv->label, priv->duree, priv->currency );*/
	ok = is_dialog_valid( self );

	gtk_widget_set_sensitive( button, ok );
}

static gboolean
is_dialog_valid( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = priv->begin_empty ||
			( my_date_is_valid( &priv->begin ) &&
				( !my_date_is_valid( priv->last_closed ) ||
					my_date_compare( &priv->begin, priv->last_closed ) > 0 ));

	ok &= priv->end_empty ||
			( my_date_is_valid( &priv->end ) &&
				( !my_date_is_valid( &priv->begin ) ||
					my_date_compare( &priv->end, &priv->begin ) > 0 ));

	ok &= ofo_dossier_is_valid( priv->label, priv->duree, priv->currency, &priv->begin, &priv->end );

	return( ok );
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
	GtkContainer *container;

	g_return_val_if_fail( is_dialog_valid( self ), FALSE );

	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_exercice_length( priv->dossier, priv->duree );
	ofo_dossier_set_default_currency( priv->dossier, priv->currency );

	my_utils_getback_notes_ex( container, dossier );

	ofo_dossier_set_current_exe_begin( priv->dossier, &priv->begin );
	ofo_dossier_set_current_exe_end( priv->dossier, &priv->end );
	ofo_dossier_set_current_exe_notes( priv->dossier, priv->notes );

	priv->updated = ofo_dossier_update( priv->dossier );

	return( priv->updated );
}
