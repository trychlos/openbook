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
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-currency-combo.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-exe-forward.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierPropertiesPrivate {

	/* internals
	 */
	ofoDossier      *dossier;
	gboolean         is_new;
	gboolean         updated;
	gboolean         is_current;

	/* data
	 */
	gchar           *label;
	gchar           *siren;
	gchar           *currency;
	GDate            begin;
	gboolean         begin_empty;
	GDate            end;
	gboolean         end_empty;
	gint             duree;
	gchar           *exe_notes;

	/* UI
	 */
	GtkWidget       *siren_entry;
	ofaExeForward   *forward;
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-dossier-properties.ui";
static const gchar *st_ui_id  = "DossierPropertiesDlg";

G_DEFINE_TYPE( ofaDossierProperties, ofa_dossier_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_properties_page( ofaDossierProperties *self, GtkContainer *container );
static void      init_forward_page( ofaDossierProperties *self, GtkContainer *container );
static void      init_exe_notes_page( ofaDossierProperties *self, GtkContainer *container );
static void      init_counters_page( ofaDossierProperties *self, GtkContainer *container );
static void      on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierProperties *self );
static void      on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
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
	g_free( priv->exe_notes );
	g_free( priv->siren );

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
	my_utils_init_notes_ex( container, dossier );
	my_utils_init_upd_user_stamp_ex( container, dossier );

	check_for_enable_dlg( self );
}

static void
init_properties_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *entry, *label, *parent;
	gchar *str;
	ofaCurrencyCombo *combo;
	const gchar *cstr;
	gint ivalue;

	priv = self->priv;

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
	combo = ofa_currency_combo_new();
	ofa_currency_combo_attach_to( combo, GTK_CONTAINER( parent ), CURRENCY_COL_CODE );
	g_signal_connect( combo, "changed", G_CALLBACK( on_currency_changed ), self );
	ofa_currency_combo_init_view( combo, priv->dossier, ofo_dossier_get_default_currency( priv->dossier ));

	label = my_utils_container_get_child_by_name( container, "p1-status" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), ofo_dossier_get_status( priv->dossier ));

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
	if( priv->is_current ){
		my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
		my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->begin );
	} else {
		str = my_date_to_str( &priv->begin, MY_DATE_DMYY );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}
	gtk_widget_set_can_focus( entry, priv->is_current );

	entry = my_utils_container_get_child_by_name( container, "pexe-end" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_end_changed ), self );
	my_date_set_from_date( &priv->end, ofo_dossier_get_exe_end( priv->dossier ));
	priv->end_empty = !my_date_is_valid( &priv->end );
	if( priv->is_current ){
		my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
		my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->end );
	} else {
		str = my_date_to_str( &priv->end, MY_DATE_DMYY );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}
	gtk_widget_set_can_focus( entry, priv->is_current );
}

static void
init_forward_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *parent;

	priv = self->priv;

	parent = my_utils_container_get_child_by_name( container, "p5-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->forward = ofa_exe_forward_new();
	ofa_exe_forward_attach_to(
			priv->forward, GTK_CONTAINER( parent ), MY_WINDOW( self )->prot->main_window );
}

static void
init_exe_notes_page( ofaDossierProperties *self, GtkContainer *container )
{
	ofaDossierPropertiesPrivate *priv;
	GObject *buffer;
	GtkWidget *widget;

	priv = self->priv;
	priv->exe_notes = g_strdup( ofo_dossier_get_exe_notes( priv->dossier ));
	buffer = my_utils_init_notes( container, "pexe-notes", priv->exe_notes );
	g_return_if_fail( buffer && GTK_IS_TEXT_BUFFER( buffer ));

	if( priv->is_current ){
		g_signal_connect( buffer, "changed", G_CALLBACK( on_notes_changed ), self );
	}

	widget = my_utils_container_get_child_by_name( container, "pexe-notes" );
	gtk_widget_set_can_focus( widget, priv->is_current );
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
}

static void
on_label_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
 * ofaCurrencyComboCb
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
		my_date_clear( date );
	}
	g_free( content );

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

	ok = priv->begin_empty || ( my_date_is_valid( &priv->begin ));
	g_debug( "is_dialog_valid 1: ok=%s", ok ? "True":"False" );

	ok &= priv->end_empty ||
			( my_date_is_valid( &priv->end ) &&
				( !my_date_is_valid( &priv->begin ) ||
					my_date_compare( &priv->end, &priv->begin ) > 0 ));
	g_debug( "is_dialog_valid 2: ok=%s", ok ? "True":"False" );

	ok &= ofo_dossier_is_valid( priv->label, priv->duree, priv->currency, &priv->begin, &priv->end );
	g_debug( "is_dialog_valid 3: ok=%s", ok ? "True":"False" );

	if( priv->forward ){
		ok &= ofa_exe_forward_check( priv->forward );
		g_debug( "is_dialog_valid 4: ok=%s", ok ? "True":"False" );
	}

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
	ofo_dossier_set_siren( priv->dossier, gtk_entry_get_text( GTK_ENTRY( priv->siren_entry )));
	ofo_dossier_set_default_currency( priv->dossier, priv->currency );
	ofo_dossier_set_exe_length( priv->dossier, priv->duree );
	ofo_dossier_set_exe_begin( priv->dossier, &priv->begin );
	ofo_dossier_set_exe_end( priv->dossier, &priv->end );

	ofa_exe_forward_apply( priv->forward );

	ofo_dossier_set_exe_notes( priv->dossier, priv->exe_notes );

	my_utils_getback_notes_ex( container, dossier );

	priv->updated = ofo_dossier_update( priv->dossier );

	return( priv->updated );
}
