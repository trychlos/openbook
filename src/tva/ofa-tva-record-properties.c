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
#include <stdarg.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-editable-amount.h"
#include "api/my-editable-date.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-record-properties.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
struct _ofaTVARecordPropertiesPrivate {

	/* internals
	 */
	ofoDossier    *dossier;
	gboolean       is_current;
	ofoTVARecord  *tva_record;
	gboolean       updated;

	/* UI
	 */
	GtkWidget     *begin_date;
	GtkWidget     *end_date;
	GtkWidget     *boolean_grid;
	GtkWidget     *detail_grid;
	GtkWidget     *textview;
	GtkWidget     *validate_btn;
	GtkWidget     *ok_btn;
	GtkWidget     *msg_label;

	/* runtime data
	 */
	gboolean       has_correspondence;
	gboolean       is_validated;
};

#define BOOL_COL_LABEL                  0
#define DET_COL_CODE                    1
#define DET_COL_LABEL                   (1+DET_COL_CODE)
#define DET_COL_BASE                    (2+DET_COL_CODE)
#define DET_COL_AMOUNT                  (3+DET_COL_CODE)

static const gchar *st_ui_xml           = PLUGINUIDIR "/ofa-tva-record-properties.ui";
static const gchar *st_ui_id            = "TVARecordPropertiesDlg";

static void     v_init_dialog( myDialog *dialog );
static void     init_properties( ofaTVARecordProperties *self, GtkContainer *container );
static void     init_booleans( ofaTVARecordProperties *self, GtkContainer *container );
static void     init_taxes( ofaTVARecordProperties *self, GtkContainer *container );
static void     init_correspondence( ofaTVARecordProperties *self, GtkContainer *container );
static void     on_begin_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void     on_end_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void     on_boolean_toggled( GtkToggleButton *button, ofaTVARecordProperties *self );
static void     on_detail_base_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void     on_detail_amount_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void     check_for_enable_dlg( ofaTVARecordProperties *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_update( ofaTVARecordProperties *self );
static void     on_validate_clicked( GtkButton *button, ofaTVARecordProperties *self );

G_DEFINE_TYPE( ofaTVARecordProperties, ofa_tva_record_properties, MY_TYPE_DIALOG )

static void
tva_record_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_TVA_RECORD_PROPERTIES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_properties_parent_class )->finalize( instance );
}

static void
tva_record_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_TVA_RECORD_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_properties_parent_class )->dispose( instance );
}

static void
ofa_tva_record_properties_init( ofaTVARecordProperties *self )
{
	static const gchar *thisfn = "ofa_tva_record_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_TVA_RECORD_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_RECORD_PROPERTIES, ofaTVARecordPropertiesPrivate );

	self->priv->updated = FALSE;
}

static void
ofa_tva_record_properties_class_init( ofaTVARecordPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaTVARecordPropertiesPrivate ));
}

/**
 * ofa_tva_record_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @record: the #ofoTVARecord to be displayed/updated.
 *
 * Update the properties of a tva_form.
 */
gboolean
ofa_tva_record_properties_run( const ofaMainWindow *main_window, ofoTVARecord *record )
{
	static const gchar *thisfn = "ofa_tva_record_properties_run";
	ofaTVARecordProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, record=%p",
			thisfn, ( void * ) main_window, ( void * ) record );

	self = g_object_new(
					OFA_TYPE_TVA_RECORD_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->tva_record = record;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaTVARecordProperties *self;
	ofaTVARecordPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	gchar *title, *send;
	const gchar *mnemo;
	GtkContainer *container;

	self = OFA_TVA_RECORD_PROPERTIES( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	priv->dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));
	priv->is_current = ofo_dossier_is_current( priv->dossier );

	mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
	send = my_date_to_str( ofo_tva_record_get_end( priv->tva_record ), MY_DATE_SQL );
	title = g_strdup_printf( _( "Updating « %s - %s » TVA declaration" ), mnemo, send );
	gtk_window_set_title( GTK_WINDOW( container ), title );
	g_free( send );

	priv->ok_btn = my_utils_container_get_child_by_name( container, "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	priv->validate_btn = my_utils_container_get_child_by_name( container, "validate-btn" );
	g_return_if_fail( priv->validate_btn && GTK_IS_BUTTON( priv->validate_btn ));
	g_signal_connect( priv->validate_btn, "clicked", G_CALLBACK( on_validate_clicked ), self );

	init_properties( self, container );
	init_booleans( self, container );
	init_taxes( self, container );
	init_correspondence( self, container );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		priv->ok_btn = my_dialog_set_readonly_buttons( dialog );
	}

	check_for_enable_dlg( self );
}

static void
init_properties( ofaTVARecordProperties *self, GtkContainer *container )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *entry, *label, *button;
	const gchar *cstr;
	const GDate *dend;

	priv = self->priv;

	priv->is_validated = ofo_tva_record_get_is_validated( priv->tva_record );

	entry = my_utils_container_get_child_by_name( container, "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_mnemo( priv->tva_record );
	g_return_if_fail( my_strlen( cstr ));
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( container, "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( container, "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_label( priv->tva_record );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	button = my_utils_container_get_child_by_name( container, "p1-has-corresp" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( button ),
			ofo_tva_record_get_has_correspondence( priv->tva_record ));
	my_utils_widget_set_editable( button, FALSE );

	button = my_utils_container_get_child_by_name( container, "p1-validated" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), priv->is_validated );
	my_utils_widget_set_editable( button, FALSE );

	entry = my_utils_container_get_child_by_name( container, "p1-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_editable_date_set_date( GTK_EDITABLE( entry ), ofo_tva_record_get_begin( priv->tva_record ));
	my_utils_widget_set_editable( entry, priv->is_current && !priv->is_validated );
	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_changed ), self );
	priv->begin_date = entry;

	label = my_utils_container_get_child_by_name( container, "p1-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( container, "p1-begin-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());

	entry = my_utils_container_get_child_by_name( container, "p1-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	dend = ofo_tva_record_get_end( priv->tva_record );
	my_editable_date_set_date( GTK_EDITABLE( entry ), dend );
	my_utils_widget_set_editable( entry, priv->is_current && !my_date_is_valid( dend ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
	priv->end_date = entry;

	label = my_utils_container_get_child_by_name( container, "p1-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( container, "p1-end-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
}

static void
init_booleans( ofaTVARecordProperties *self, GtkContainer *container )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *grid, *button;
	guint idx, count, row;
	const gchar *cstr;
	gboolean is_true;

	priv = self->priv;
	grid = my_utils_container_get_child_by_name( container, "p3-grid" );
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
init_taxes( ofaTVARecordProperties *self, GtkContainer *container )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *grid, *label, *entry;
	guint idx, count, row;
	const gchar *cstr;
	gchar *str;
	gboolean has_base, has_amount;

	priv = self->priv;
	grid = my_utils_container_get_child_by_name( container, "p2-grid" );
	priv->detail_grid = grid;

	count = ofo_tva_record_detail_get_count( priv->tva_record );
	for( idx=0 ; idx<count ; ++idx ){
		row = idx+1;

		label = gtk_label_new( NULL );
		gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
		my_utils_widget_set_margin( label, 0, 0, 0, 4 );
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
			my_editable_amount_init_ex( GTK_EDITABLE( entry ), 0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_BASE, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_base_changed ), self );

			cstr = ofo_tva_record_detail_get_base( priv->tva_record, idx );
			gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );
		}

		/* amount */
		has_amount = ofo_tva_record_detail_get_has_amount( priv->tva_record, idx );
		if( has_amount ){
			entry = gtk_entry_new();
			my_utils_widget_set_editable( entry, priv->is_current && !priv->is_validated );
			my_editable_amount_init_ex( GTK_EDITABLE( entry ), 0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_AMOUNT, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_amount_changed ), self );

			cstr = ofo_tva_record_detail_get_amount( priv->tva_record, idx );
			gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );
		}
	}
}

static void
init_correspondence( ofaTVARecordProperties *self, GtkContainer *container )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *book, *label, *scrolled;
	const gchar *cstr;

	priv = self->priv;
	priv->has_correspondence = ofo_tva_record_get_has_correspondence( priv->tva_record );
	if( priv->has_correspondence ){
		book = my_utils_container_get_child_by_name( container, "tva-book" );
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
on_begin_changed( GtkEntry *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	const GDate *date;

	priv = self->priv;
	date = my_editable_date_get_date( GTK_EDITABLE( priv->begin_date ), NULL );
	ofo_tva_record_set_begin( priv->tva_record, date );

	check_for_enable_dlg( self );
}

static void
on_end_changed( GtkEntry *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	const GDate *date;

	priv = self->priv;
	date = my_editable_date_get_date( GTK_EDITABLE( priv->end_date ), NULL );
	ofo_tva_record_set_end( priv->tva_record, date );

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
	gboolean is_validated;

	priv = self->priv;

	gtk_widget_set_sensitive(
			priv->ok_btn,
			TRUE );

	is_validated = ofo_tva_record_get_is_validated( priv->tva_record );
	gtk_widget_set_sensitive(
			priv->validate_btn,
			!is_validated && ofo_tva_record_is_validable( priv->tva_record ));
}

/*
 * OK button: records the update and quits the dialog
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_TVA_RECORD_PROPERTIES( dialog )));
}

/*
 * either creating a new tva_form (is_new is set)
 * or updating an existing one (mnemo is never modified here)
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	guint idx, row, count;
	GtkWidget *button, *entry;
	const gchar *clabel;
	gboolean is_true;
	gchar *str;

	priv = self->priv;

	if( priv->has_correspondence ){
		my_utils_container_notes_get_ex( GTK_TEXT_VIEW( priv->textview ), tva_record );
	}

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
			str = my_editable_amount_get_string( GTK_EDITABLE( entry ));
			ofo_tva_record_detail_set_base( priv->tva_record, idx, str );
			g_free( str );
		}
		if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
			str = my_editable_amount_get_string( GTK_EDITABLE( entry ));
			ofo_tva_record_detail_set_amount( priv->tva_record, idx, str );
			g_free( str );
		}
	}

	priv->updated = ofo_tva_record_update( priv->tva_record, priv->dossier );

	return( priv->updated );
}

/*
 * Validating is actually same than recording;
 * just the 'validated' flag is set
 */
static void
on_validate_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = self->priv;

	ofo_tva_record_set_is_validated( priv->tva_record, TRUE );
	if( do_update( self )){
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))), GTK_RESPONSE_OK );
	}
}
