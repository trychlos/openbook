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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-editable-amount.h"
#include "api/my-editable-date.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofs-account-balance.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-record-properties.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
struct _ofaTVARecordPropertiesPrivate {

	/* initialization
	 */
	const ofaMainWindow *main_window;
	ofoTVARecord        *tva_record;

	/* internals
	 */
	ofaHub              *hub;
	gboolean             is_current;
	gboolean             updated;

	/* UI
	 */
	GtkWidget           *label_entry;
	GtkWidget           *begin_editable;
	GtkWidget           *end_editable;
	GtkWidget           *boolean_grid;
	GtkWidget           *detail_grid;
	GtkWidget           *textview;
	GtkWidget           *compute_btn;
	GtkWidget           *validate_btn;
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;

	/* runtime data
	 */
	GDate                init_end_date;
	gchar               *mnemo;
	GDate                begin_date;
	GDate                end_date;
	gboolean             has_correspondence;
	gboolean             is_validated;

	/* computing the declaration
	 */
	GRegex              *regex_fn;
};

static gboolean st_debug                = FALSE;
#define DEBUG                           if( st_debug ) g_debug

enum {
	BOOL_COL_LABEL = 0,
	DET_COL_CODE = 1,
	DET_COL_LABEL,
	DET_COL_BASE,
	DET_COL_AMOUNT,
	DET_COL_PADDING
};

static const gchar *st_ui_xml           = PLUGINUIDIR "/ofa-tva-record-properties.ui";
static const gchar *st_ui_id            = "TVARecordPropertiesDlg";

static void      v_init_dialog( myDialog *dialog );
static void      init_properties( ofaTVARecordProperties *self, GtkContainer *container );
static void      init_booleans( ofaTVARecordProperties *self, GtkContainer *container );
static void      init_taxes( ofaTVARecordProperties *self, GtkContainer *container );
static void      init_correspondence( ofaTVARecordProperties *self, GtkContainer *container );
static void      on_begin_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void      on_end_changed( GtkEditable *entry, ofaTVARecordProperties *self );
static void      on_boolean_toggled( GtkToggleButton *button, ofaTVARecordProperties *self );
static void      on_detail_base_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void      on_detail_amount_changed( GtkEntry *entry, ofaTVARecordProperties *self );
static void      check_for_enable_dlg( ofaTVARecordProperties *self );
static void      set_dialog_title( ofaTVARecordProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaTVARecordProperties *self );
static void      on_compute_clicked( GtkButton *button, ofaTVARecordProperties *self );
static void      alloc_regex( ofaTVARecordProperties *self );
static gchar    *eval_rule( ofaTVARecordProperties *self, const gchar *rule );
static gboolean  eval_function_cb( const GMatchInfo *match_info, GString *result, ofaTVARecordProperties *self );
static gboolean  is_function( const gchar *token, ofaTVARecordProperties *self, gchar **str );
static gchar    *get_code_amount( ofaTVARecordProperties *self, const gchar *content );
static gchar    *get_account_balance( ofaTVARecordProperties *self, const gchar *content );
static gdouble   eval_opes( ofaTVARecordProperties *self, const gchar *content );
static gchar   **eval_opes_rec( const gchar *content, gchar **iter, gdouble *amount, gint count );
static void      on_validate_clicked( GtkButton *button, ofaTVARecordProperties *self );
static void      set_msgerr( ofaTVARecordProperties *dialog, const gchar *msg );

G_DEFINE_TYPE( ofaTVARecordProperties, ofa_tva_record_properties, MY_TYPE_DIALOG )

static void
tva_record_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_properties_finalize";
	ofaTVARecordPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_TVA_RECORD_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_TVA_RECORD_PROPERTIES( instance )->priv;

	g_free( priv->mnemo );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_properties_parent_class )->finalize( instance );
}

static void
tva_record_properties_dispose( GObject *instance )
{
	ofaTVARecordPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_TVA_RECORD_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_TVA_RECORD_PROPERTIES( instance )->priv;

		if( priv->regex_fn ){
			g_regex_unref( priv->regex_fn );
		}
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

	self->priv->main_window = main_window;
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
	ofoDossier *dossier;
	GtkContainer *container;

	self = OFA_TVA_RECORD_PROPERTIES( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	priv->hub = ofa_main_window_get_hub( priv->main_window );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->is_current = ofo_dossier_is_current( dossier );

	my_date_set_from_date( &priv->init_end_date, ofo_tva_record_get_end( priv->tva_record ));

	priv->ok_btn = my_utils_container_get_child_by_name( container, "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	priv->compute_btn = my_utils_container_get_child_by_name( container, "compute-btn" );
	g_return_if_fail( priv->compute_btn && GTK_IS_BUTTON( priv->compute_btn ));
	g_signal_connect( priv->compute_btn, "clicked", G_CALLBACK( on_compute_clicked ), self );

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

	set_dialog_title( self );
	check_for_enable_dlg( self );
}

static void
init_properties( ofaTVARecordProperties *self, GtkContainer *container )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *entry, *label, *button;
	const gchar *cstr;

	priv = self->priv;

	priv->is_validated = ofo_tva_record_get_is_validated( priv->tva_record );

	/* mnemonic: invariant */
	entry = my_utils_container_get_child_by_name( container, "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->mnemo = g_strdup( ofo_tva_record_get_mnemo( priv->tva_record ));
	g_return_if_fail( my_strlen( priv->mnemo ));
	gtk_entry_set_text( GTK_ENTRY( entry ), priv->mnemo );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( container, "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* label */
	priv->label_entry = my_utils_container_get_child_by_name( container, "p1-label-entry" );
	g_return_if_fail( priv->label_entry && GTK_IS_ENTRY( priv->label_entry ));
	cstr = ofo_tva_record_get_label( priv->tva_record );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), cstr );
	}
	my_utils_widget_set_editable( priv->label_entry, priv->is_current );

	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* has correspondence: invariant */
	button = my_utils_container_get_child_by_name( container, "p1-has-corresp" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( button ),
			ofo_tva_record_get_has_correspondence( priv->tva_record ));
	my_utils_widget_set_editable( button, FALSE );

	/* is validated: invariant */
	button = my_utils_container_get_child_by_name( container, "p1-validated" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), priv->is_validated );
	my_utils_widget_set_editable( button, FALSE );

	/* begin date */
	entry = my_utils_container_get_child_by_name( container, "p1-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_changed ), self );
	priv->begin_editable = entry;

	label = my_utils_container_get_child_by_name( container, "p1-begin-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());

	my_date_set_from_date( &priv->begin_date, ofo_tva_record_get_begin( priv->tva_record ));
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->begin_date );
	my_utils_widget_set_editable( entry, priv->is_current && !priv->is_validated );

	label = my_utils_container_get_child_by_name( container, "p1-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* do not let the user edit the ending date of the declaration
	 * because this is a key of the record
	 * if the ending date has to be modified, then the user should
	 * create a new declaration
	 */
	entry = my_utils_container_get_child_by_name( container, "p1-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
	priv->end_editable = entry;

	label = my_utils_container_get_child_by_name( container, "p1-end-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());

	my_date_set_from_date( &priv->end_date, ofo_tva_record_get_end( priv->tva_record ));
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->end_date );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( container, "p1-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
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
	ofxAmount amount;

	priv = self->priv;
	grid = my_utils_container_get_child_by_name( container, "p2-grid" );
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
			my_editable_amount_init_ex( GTK_EDITABLE( entry ), 0 );
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, DET_COL_BASE, row, 1, 1 );
			g_signal_connect( entry, "changed", G_CALLBACK( on_detail_base_changed ), self );

			gtk_widget_set_tooltip_text(
					entry, ofo_tva_record_detail_get_base_rule( priv->tva_record, idx ));

			amount = ofo_tva_record_detail_get_base( priv->tva_record, idx );
			my_editable_amount_set_amount( GTK_EDITABLE( entry ), amount );
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

			gtk_widget_set_tooltip_text(
					entry, ofo_tva_record_detail_get_amount_rule( priv->tva_record, idx ));

			amount = ofo_tva_record_detail_get_amount( priv->tva_record, idx );
			my_editable_amount_set_amount( GTK_EDITABLE( entry ), amount );
		}

		/* padding on the right so that the scrollbar does not hide the
		 * amount */
		label = gtk_label_new( "   " );
		gtk_grid_attach( GTK_GRID( grid ), label, DET_COL_PADDING, row, 1, 1 );
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
on_begin_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = self->priv;

	my_date_set_from_date( &priv->begin_date, my_editable_date_get_date( entry, NULL ));

	check_for_enable_dlg( self );
}

static void
on_end_changed( GtkEditable *entry, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = self->priv;

	my_date_set_from_date( &priv->end_date, my_editable_date_get_date( entry, NULL ));

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

	priv = self->priv;
	msgerr = NULL;

	if( priv->is_current ){

		is_valid = ofo_tva_record_is_valid( priv->mnemo, &priv->begin_date, &priv->end_date, &msgerr );

		if( is_valid ){
			/* the ending date is no more modifiable */
			if( 0 ){
				dend = ofo_tva_record_get_end( priv->tva_record );
				end_date_has_changed = my_date_compare( &priv->init_end_date, dend ) != 0;
				if( end_date_has_changed ){
					mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
					exists = ( ofo_tva_record_get_by_key( priv->hub, mnemo, dend ) != NULL );
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
	GtkWindow *toplevel;
	gchar *send, *title;

	priv = self->priv;

	send = my_date_to_str( &priv->end_date, MY_DATE_SQL );
	title = g_strdup_printf( _( "Updating « %s - %s » TVA declaration" ), priv->mnemo, send );
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));
	gtk_window_set_title( toplevel, title );
	g_free( title );
	g_free( send );
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
 * Recording the updates done to the declaration
 * The record is uniquely identified by the mnemo + the end date
 * Though the mnemo is an invariant, the end date may have been changed.
 * If this is the case, then the original record must be deleted and the
 * new one be re-inserted.
 */
static gboolean
do_update( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	guint idx, row, count;
	GtkWidget *button, *entry;
	const gchar *clabel;
	gboolean is_true;
	ofxAmount amount;

	priv = self->priv;

	if( priv->has_correspondence ){
		my_utils_container_notes_get_ex( GTK_TEXT_VIEW( priv->textview ), tva_record );
	}

	ofo_tva_record_set_label( priv->tva_record,
			gtk_entry_get_text( GTK_ENTRY( priv->label_entry )));

	ofo_tva_record_set_begin( priv->tva_record,
			my_editable_date_get_date( GTK_EDITABLE( priv->begin_editable ), NULL ));

	ofo_tva_record_set_end( priv->tva_record,
			my_editable_date_get_date( GTK_EDITABLE( priv->end_editable ), NULL ));

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
			amount = my_editable_amount_get_amount( GTK_EDITABLE( entry ));
			ofo_tva_record_detail_set_base( priv->tva_record, idx, amount );
		}
		if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
			amount = my_editable_amount_get_amount( GTK_EDITABLE( entry ));
			ofo_tva_record_detail_set_amount( priv->tva_record, idx, amount );
		}
	}

	priv->updated = ofo_tva_record_update( priv->tva_record );

	return( priv->updated );
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

	priv = self->priv;

	dialog = gtk_message_dialog_new(
					GTK_WINDOW( priv->main_window ),
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
		alloc_regex( self );
		count = ofo_tva_record_detail_get_count( priv->tva_record );

		for( idx=0, row=1 ; idx<count ; ++idx, ++row ){
			if( ofo_tva_record_detail_get_has_base( priv->tva_record, idx )){
				rule = ofo_tva_record_detail_get_base_rule( priv->tva_record, idx );
				if( my_strlen( rule )){
					result = eval_rule( self, rule );
					entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_BASE, row );
					g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
					my_editable_amount_set_string( GTK_EDITABLE( entry ), result );
					g_free( result );
				}
			}
			if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
				rule = ofo_tva_record_detail_get_amount_rule( priv->tva_record, idx );
				if( my_strlen( rule )){
					result = eval_rule( self, rule );
					entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
					g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
					my_editable_amount_set_string( GTK_EDITABLE( entry ), result );
					g_free( result );
				}
			}
		}
	}
}

static void
alloc_regex( ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	static const gchar *st_functions = "%(COD|ACC)\\(\\s*([^()]+)\\s*\\)";

	priv = self->priv;

	if( !priv->regex_fn ){
		/* a regex to identify functions */
		priv->regex_fn = g_regex_new( st_functions, G_REGEX_EXTENDED, 0, NULL );
	}
}

static gchar *
eval_rule( ofaTVARecordProperties *self, const gchar *rule )
{
	ofaTVARecordPropertiesPrivate *priv;
	gchar *str1, *str2;

	priv = self->priv;
	str1 = g_regex_replace_eval( priv->regex_fn,
				rule, -1, 0, 0, ( GRegexEvalCallback ) eval_function_cb, self, NULL );
	str2 = my_double_to_str( eval_opes( self, str1 ));

	g_free( str1 );
	return( str2 );
}

static gboolean
eval_function_cb( const GMatchInfo *match_info, GString *result, ofaTVARecordProperties *self )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_function_cb";
	gchar *match;
	gchar *str;

	str = NULL;
	match = g_match_info_fetch( match_info, 0 );
	DEBUG( "%s: match=%s", thisfn, match );

	if( is_function( match, self, &str )){
		if( str ){
			g_string_append( result, str );
			g_free( str );
		}

	} else {
		g_string_append( result, match );
	}

	g_free( match );

	return( FALSE );
}

static gboolean
is_function( const gchar *token, ofaTVARecordProperties *self, gchar **str )
{
	static const gchar *thisfn = "ofa_tva_record_properties_is_function";
	ofaTVARecordPropertiesPrivate *priv;
	gboolean ok;
	GMatchInfo *info;
	gchar *field, *content;

	field = NULL;
	priv = self->priv;

	ok = g_regex_match( priv->regex_fn, token, 0, &info );
	DEBUG( "%s: token=%s, g_regex_match=%s", thisfn, token, ok ? "True":"False" );
	if( ok ){
		ok = g_match_info_matches( info );
		DEBUG( "%s: g_match_info_matches=%s", thisfn, ok ? "True":"False" );
	}
	if( ok ){
		field = g_match_info_fetch( info, 1 );
		content = g_match_info_fetch( info, 2 );
		DEBUG( "%s: field=%s, content=%s", thisfn, field, content );

		if( !g_utf8_collate( field, "COD" )){
			*str = get_code_amount( self, content );

		} else if( !g_utf8_collate( field, "ACC" )){
			*str = get_account_balance( self, content );

		} else {
			ok = FALSE;
		}
	}

	g_free( field );
	g_match_info_free( info );

	DEBUG( "%s: token=%s, str=%s, ok=%s", thisfn, token, *str, ok ? "True":"False" );
	return( ok );
}

/*
 * return the amount of the row whose the code is provided by @content
 */
static gchar *
get_code_amount( ofaTVARecordProperties *self, const gchar *content )
{
	static const gchar *thisfn = "ofa_tva_record_properties_get_code_amount";
	ofaTVARecordPropertiesPrivate *priv;
	guint idx, row, count;
	GtkWidget *entry;
	const gchar *code, *ctext;

	priv = self->priv;
	count = ofo_tva_record_detail_get_count( priv->tva_record );

	for( idx=0, row=1 ; idx<count ; ++idx, ++row ){
		if( ofo_tva_record_detail_get_has_amount( priv->tva_record, idx )){
			entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_CODE, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
			code = gtk_entry_get_text( GTK_ENTRY( entry ));
			if( !g_utf8_collate( code, content )){
				entry = gtk_grid_get_child_at( GTK_GRID( priv->detail_grid ), DET_COL_AMOUNT, row );
				g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
				ctext = gtk_entry_get_text( GTK_ENTRY( entry ));
				DEBUG( "%s: COD(%s)=%s", thisfn, content, ctext );
				return( g_strdup( ctext ));
			}
		}
	}

	return( NULL );
}

/*
 * returns the rough+validated balance of the account specified by
 * @content, between beginning and ending dates
 */
static gchar *
get_account_balance( ofaTVARecordProperties *self, const gchar *content )
{
	static const gchar *thisfn = "ofa_tva_record_properties_get_account_balance";
	ofaTVARecordPropertiesPrivate *priv;
	ofaHub *hub;
	GList *list, *it;
	ofsAccountBalance *sbal;
	ofxAmount amount;
	gchar *begin_id, *end_id;
	gchar **array;

	priv = self->priv;

	array = g_strsplit( content, "-", 2 );
	begin_id = g_strdup( *array );
	end_id = g_strdup( *( array+1 ));
	g_strfreev( array );
	if( !my_strlen( end_id )){
		g_free( end_id );
		end_id = g_strdup( begin_id );
	}
	DEBUG( "%s: begin_id=%s, endid=%s", thisfn, begin_id, end_id );

	hub = ofa_main_window_get_hub( priv->main_window );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	list = ofo_entry_get_dataset_balance_rough_validated(
				hub, begin_id, end_id, &priv->begin_date, &priv->end_date );
	amount = 0;
	for( it=list ; it ; it=it->next ){
		sbal = ( ofsAccountBalance * ) it->data;
		/* credit is -, debit is + */
		amount -= sbal->credit;
		amount += sbal->debit;
	}

	g_free( begin_id );
	g_free( end_id );

	DEBUG( "%s: ACC(%s)=%lf", thisfn, content, amount );
	return( my_double_to_str( amount ));
}

/*
 * %EVAL(...) recursively evaluates the (..) between parenthesis content
 */
static gdouble
eval_opes( ofaTVARecordProperties *self, const gchar *content )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_opes";
	gchar **tokens, **iter;
	gdouble amount;

	DEBUG( "%s: content=%s", thisfn, content );
	if( g_str_has_prefix( content, "%EVAL(" )){
		tokens = g_regex_split_simple( "([-+*/\\(\\)])", content+5, 0, 0 );
	} else {
		tokens = g_regex_split_simple( "([-+*/\\(\\)])", content, 0, 0 );
	}
	iter = tokens;
	eval_opes_rec( content, iter, &amount, 1 );
	g_strfreev( tokens );
	DEBUG( "%s: amount=%lf", thisfn, amount );

	return( amount );
}

/*
 * this is used to evaluate the content between two parenthesis:
 * the first time, we enter here even if not parenthesis is found (count=1)
 * then, we re-enter for each opening parenthesis
 *  and we return from here for each closing parenthesis, returning
 *   the new iter position, with the computed amount
 */
static gchar **
eval_opes_rec( const gchar *content, gchar **iter, gdouble *amount, gint count )
{
	static const gchar *thisfn = "ofa_tva_record_properties_eval_opes_rec";
	gdouble amount_iter;
	gboolean first_iter, expect_operator;
	const gchar *oper;

	DEBUG( "%s: count=%d", thisfn, count );
	*amount = 0;
	first_iter = TRUE;
	expect_operator = TRUE;
	oper = NULL;

	while( *iter ){
		if( my_strlen( *iter )){
			DEBUG( "%s: *iter=%s", thisfn, *iter );
			if( expect_operator ){
				if( !g_utf8_collate( *iter, "-" )){
					oper = "-";
				} else if( !g_utf8_collate( *iter, "+" )){
					oper = "+";
				} else if( !g_utf8_collate( *iter, "*" )){
					oper = "*";
				} else if( !g_utf8_collate( *iter, "/" )){
					oper = "/";
				} else if( first_iter ){
					oper = "+";
					expect_operator = FALSE;
				} else if( !g_utf8_collate( *iter, ")" )){
					iter++;
					return( iter );
				} else {
					g_warning( "%s: formula='%s': found token='%s' while an operator was expected",
							thisfn, content, *iter );
					break;
				}
			}
			if( !expect_operator ){
				if( !g_utf8_collate( *iter, "(" )){
					iter++;
					iter = eval_opes_rec( content, iter, &amount_iter, 1+count );
					DEBUG( "%s: count=%d, amount=%lf, oper=%s, amount_iter=%lf", thisfn, count, *amount, oper, amount_iter );
				} else {
					amount_iter = my_double_set_from_str( *iter );
				}
				if( !g_utf8_collate( oper, "-" )){
					*amount -= amount_iter;
				} else if( !g_utf8_collate( oper, "+" )){
					*amount += amount_iter;
				} else if( !g_utf8_collate( oper, "*" )){
					*amount *= amount_iter;
				} else if( !g_utf8_collate( oper, "/" )){
					*amount /= amount_iter;
				}
				amount_iter = 0;
				DEBUG( "%s: count=%d, amount=%lf", thisfn, count, *amount );
			}
			first_iter = FALSE;
			expect_operator = !expect_operator;
		}
		if( *iter ){
			iter++;
		}
	}

	return( iter );
}

/*
 * Validating is actually same than recording;
 * just the 'validated' flag is set
 */
static void
on_validate_clicked( GtkButton *button, ofaTVARecordProperties *self )
{
	ofaTVARecordPropertiesPrivate *priv;
	GtkWidget *dialog;

	priv = self->priv;

	ofo_tva_record_set_is_validated( priv->tva_record, TRUE );
	if( do_update( self )){
		/* display an informational message */
		dialog = gtk_message_dialog_new(
						GTK_WINDOW( priv->main_window ),
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_INFO,
						GTK_BUTTONS_OK,
						_( "The VAT declaration has been successfully validated." ));
		gtk_dialog_run( GTK_DIALOG( dialog ));
		gtk_widget_destroy( dialog );
		/* close the Properties dialog box
		 * with Cancel to not trigger another update */
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))), GTK_RESPONSE_CANCEL );
	}
}

static void
set_msgerr( ofaTVARecordProperties *dialog, const gchar *msg )
{
	ofaTVARecordPropertiesPrivate *priv;

	priv = dialog->priv;

	if( !priv->msg_label ){
		priv->msg_label =
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
						"px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
		my_utils_widget_set_style( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
