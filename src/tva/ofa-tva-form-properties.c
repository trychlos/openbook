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
#include "api/my-igridlist.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-form-properties.h"
#include "tva/ofo-tva-form.h"

/* private instance data
 */
struct _ofaTVAFormPropertiesPrivate {

	/* internals
	 */
	ofoDossier    *dossier;
	gboolean       is_current;
	ofoTVAForm    *tva_form;
	gboolean       is_new;
	gboolean       updated;

	/* UI
	 */
	GtkWidget     *corresp_btn;
	GtkWidget     *bool_grid;
	GtkWidget     *det_grid;
	GtkWidget     *ok_btn;
	GtkWidget     *msg_label;

	/* data
	 */
	gchar         *mnemo;
	gchar         *label;
};

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"
#define DET_SPIN_WIDTH                  2
#define DET_SPIN_MAX_WIDTH              2
#define DET_CODE_WIDTH                  4
#define DET_CODE_MAX_WIDTH              6
#define DET_CODE_MAX_LENGTH             10
#define DET_LABEL_MAX_WIDTH             80
#define DET_LABEL_MAX_LENGTH            192
#define DET_BASE_WIDTH                  8
#define DET_BASE_MAX_WIDTH              16
#define DET_BASE_MAX_LENGTH             80
#define DET_AMOUNT_WIDTH                8
#define DET_AMOUNT_MAX_WIDTH            16
#define DET_AMOUNT_MAX_LENGTH           80
#define BOOL_LABEL_MAX_WIDTH            80
#define BOOL_LABEL_MAX_LENGTH           192

/* the columns in the dynamic grid
 * thy are numbered from zero, so that the N_DET_COLUMNS is the count
 */
enum {
	COL_DET_LEVEL = 0,
	COL_DET_CODE,
	COL_DET_LABEL,
	COL_DET_HAS_BASE,
	COL_DET_BASE,
	COL_DET_HAS_AMOUNT,
	COL_DET_AMOUNT,
	N_DET_COLUMNS
};

enum {
	COL_BOOL_LABEL = 0,
	N_BOOL_COLUMNS
};

static const gchar  *st_ui_xml       = PLUGINUIDIR "/ofa-tva-form-properties.ui";
static const gchar  *st_ui_id        = "TVAFormPropertiesDlg";

static void     igridlist_iface_init( myIGridListInterface *iface );
static guint    igridlist_get_interface_version( const myIGridList *instance );
static void     igridlist_set_row( const myIGridList *instance, GtkGrid *grid, guint row );
static void     set_detail_widgets( ofaTVAFormProperties *self, guint row );
static void     set_detail_values( ofaTVAFormProperties *self, guint row );
static void     set_boolean_widgets( ofaTVAFormProperties *self, guint row );
static void     set_boolean_values( ofaTVAFormProperties *self, guint row );
static void     v_init_dialog( myDialog *dialog );
static void     on_mnemo_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_code_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_has_base_toggled( GtkToggleButton *button, ofaTVAFormProperties *self );
static void     on_det_base_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_has_amount_toggled( GtkToggleButton *button, ofaTVAFormProperties *self );
static void     on_det_amount_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_bool_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     check_for_enable_dlg( ofaTVAFormProperties *self );
static gboolean is_dialog_validable( ofaTVAFormProperties *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_update( ofaTVAFormProperties *self );
static void     set_message( ofaTVAFormProperties *dialog, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaTVAFormProperties, ofa_tva_form_properties, MY_TYPE_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ));

static void
tva_form_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_form_properties_finalize";
	ofaTVAFormPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_TVA_FORM_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_TVA_FORM_PROPERTIES( instance )->priv;
	g_free( priv->mnemo );
	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_properties_parent_class )->finalize( instance );
}

static void
tva_form_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_TVA_FORM_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_properties_parent_class )->dispose( instance );
}

static void
ofa_tva_form_properties_init( ofaTVAFormProperties *self )
{
	static const gchar *thisfn = "ofa_tva_form_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_TVA_FORM_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_FORM_PROPERTIES, ofaTVAFormPropertiesPrivate );

	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
}

static void
ofa_tva_form_properties_class_init( ofaTVAFormPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_tva_form_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaTVAFormPropertiesPrivate ));
}

/*
 * myIGridList interface management
 */
static void
igridlist_iface_init( myIGridListInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_form_properties_igridlist_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = igridlist_get_interface_version;
	iface->set_row = igridlist_set_row;
}

static guint
igridlist_get_interface_version( const myIGridList *instance )
{
	return( 1 );
}

static void
igridlist_set_row( const myIGridList *instance, GtkGrid *grid, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_FORM_PROPERTIES( instance ));

	priv = OFA_TVA_FORM_PROPERTIES( instance )->priv;
	if( grid == GTK_GRID( priv->det_grid )){
		set_detail_widgets( OFA_TVA_FORM_PROPERTIES( instance ), row );
		set_detail_values( OFA_TVA_FORM_PROPERTIES( instance ), row );
	}
	if( grid == GTK_GRID( priv->bool_grid )){
		set_boolean_widgets( OFA_TVA_FORM_PROPERTIES( instance ), row );
		set_boolean_values( OFA_TVA_FORM_PROPERTIES( instance ), row );
	}
}

/*
 * row is the index of the current row, counted from zero (including headers)
 * so corresponding ofoTVAForm detail index is row-1
 */
static void
set_detail_widgets( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry, *toggle, *spin;
	GtkAdjustment *adjustment;

	priv = self->priv;

	adjustment = gtk_adjustment_new( 1.0, 1.0, ( gdouble ) G_MAXUINT, 1.0, 10.0, 0.0 );
	spin = gtk_spin_button_new( adjustment, 1.0, 0 );
	g_object_set_data( G_OBJECT( spin ), DATA_ROW, GUINT_TO_POINTER( row ));
	gtk_entry_set_width_chars( GTK_ENTRY( spin ), DET_SPIN_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( spin ), DET_SPIN_MAX_WIDTH );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), spin, 1+COL_DET_LEVEL, row, 1, 1 );
	gtk_widget_set_sensitive( spin, priv->is_current );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_code_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), DET_CODE_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_CODE_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_CODE_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), entry, 1+COL_DET_CODE, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_label_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_LABEL_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_LABEL_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), entry, 1+COL_DET_LABEL, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( toggle ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_det_has_base_toggled ), self );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), toggle, 1+COL_DET_HAS_BASE, row, 1, 1 );
	gtk_widget_set_sensitive( toggle, priv->is_current );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_base_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), DET_BASE_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_BASE_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_BASE_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), entry, 1+COL_DET_BASE, row, 1, 1 );
	gtk_widget_set_sensitive( entry, FALSE );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( toggle ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_det_has_amount_toggled ), self );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), toggle, 1+COL_DET_HAS_AMOUNT, row, 1, 1 );
	gtk_widget_set_sensitive( toggle, priv->is_current );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_amount_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), DET_AMOUNT_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_AMOUNT_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_AMOUNT_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->det_grid ), entry, 1+COL_DET_AMOUNT, row, 1, 1 );
	gtk_widget_set_sensitive( entry, FALSE );
}

static void
set_detail_values( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *spin, *entry, *toggle, *previous_spin;
	const gchar *cstr;
	guint idx, level;
	gboolean has_base, has_amount;

	priv = self->priv;
	idx = row-1;

	spin = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_LEVEL, row );
	g_return_if_fail( spin && GTK_IS_SPIN_BUTTON( spin ));
	level = ofo_tva_form_detail_get_level( priv->tva_form, idx );
	/* set a default value equal to those of the previous line */
	if( level == 0 && row > 1 ){
		previous_spin = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_LEVEL, row-1 );
		g_return_if_fail( previous_spin && GTK_IS_SPIN_BUTTON( previous_spin ));
		level = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( previous_spin ));
	}
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( spin ), level );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_CODE, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_form_detail_get_code( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_LABEL, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_form_detail_get_label( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	toggle = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_BASE, row );
	g_return_if_fail( toggle && GTK_IS_TOGGLE_BUTTON( toggle ));
	has_base = ofo_tva_form_detail_get_has_base( priv->tva_form, idx );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), has_base );
	on_det_has_base_toggled( GTK_TOGGLE_BUTTON( toggle ), self );

	cstr = ofo_tva_form_detail_get_base( priv->tva_form, idx );
	if( my_strlen( cstr )){
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_BASE, row );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	toggle = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_AMOUNT, row );
	g_return_if_fail( toggle && GTK_IS_TOGGLE_BUTTON( toggle ));
	has_amount = ofo_tva_form_detail_get_has_amount( priv->tva_form, idx );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), has_amount );
	on_det_has_amount_toggled( GTK_TOGGLE_BUTTON( toggle ), self );

	cstr = ofo_tva_form_detail_get_amount( priv->tva_form, idx );
	if( my_strlen( cstr )){
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_AMOUNT, row );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
}

static void
set_boolean_widgets( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry;

	priv = self->priv;

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_bool_label_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), BOOL_LABEL_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), BOOL_LABEL_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->bool_grid ), entry, 1+COL_BOOL_LABEL, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );
}

static void
set_boolean_values( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry;
	const gchar *cstr;
	guint idx;

	priv = self->priv;
	idx = row-1;

	entry = gtk_grid_get_child_at( GTK_GRID( priv->bool_grid ), 1+COL_BOOL_LABEL, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_form_boolean_get_label( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
}

/**
 * ofa_tva_form_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @form: the #ofoTVAForm to be displayed/updated.
 *
 * Update the properties of a tva_form.
 */
gboolean
ofa_tva_form_properties_run( const ofaMainWindow *main_window, ofoTVAForm *form )
{
	static const gchar *thisfn = "ofa_tva_form_properties_run";
	ofaTVAFormProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, form=%p",
			thisfn, ( void * ) main_window, ( void * ) form );

	self = g_object_new(
					OFA_TYPE_TVA_FORM_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->tva_form = form;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaTVAFormProperties *self;
	ofaTVAFormPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	guint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkWidget *label;
	GtkContainer *container;

	self = OFA_TVA_FORM_PROPERTIES( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	priv->dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	priv->is_current = ofo_dossier_is_current( priv->dossier );

	mnemo = ofo_tva_form_get_mnemo( priv->tva_form );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new TVA form" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » TVA form" ), mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->ok_btn = my_utils_container_get_child_by_name( container, "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	/* mnemonic */
	priv->mnemo = g_strdup( mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-mnemo-entry" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	priv->label = g_strdup( ofo_tva_form_get_label( priv->tva_form ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	priv->corresp_btn = my_utils_container_get_child_by_name( container, "p1-has-corresp" );
	g_return_if_fail( priv->corresp_btn && GTK_TOGGLE_BUTTON( priv->corresp_btn ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->corresp_btn ),
			ofo_tva_form_get_has_correspondence( priv->tva_form ));

	my_utils_container_notes_init( container, tva_form );
	my_utils_container_updstamp_init( container, tva_form );
	my_utils_container_set_editable( container, priv->is_current );

	/* set the detail rows after having set editability for current
	 * dossier (because my_utils_container_set_editable() set the
	 * sensitivity flag without considering the has_amount flag or
	 * the row number - which is ok in general but not here) */
	priv->det_grid = my_utils_container_get_child_by_name( container, "p2-grid" );
	g_return_if_fail( priv->det_grid && GTK_IS_GRID( priv->det_grid ));
	my_igridlist_init(
			MY_IGRIDLIST( dialog ), GTK_GRID( priv->det_grid ), priv->is_current, N_DET_COLUMNS );
	count = ofo_tva_form_detail_get_count( priv->tva_form );
	for( idx=0 ; idx<count ; ++idx ){
		my_igridlist_add_row( MY_IGRIDLIST( dialog ), GTK_GRID( priv->det_grid ));
	}

	priv->bool_grid = my_utils_container_get_child_by_name( container, "p3-grid" );
	g_return_if_fail( priv->bool_grid && GTK_IS_GRID( priv->bool_grid ));
	my_igridlist_init(
			MY_IGRIDLIST( dialog ), GTK_GRID( priv->bool_grid ), priv->is_current, N_BOOL_COLUMNS );
	count = ofo_tva_form_boolean_get_count( priv->tva_form );
	for( idx=0 ; idx<count ; ++idx ){
		my_igridlist_add_row( MY_IGRIDLIST( dialog ), GTK_GRID( priv->bool_grid ));
	}

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		priv->ok_btn = my_dialog_set_readonly_buttons( dialog );
	}

	check_for_enable_dlg( self );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = self->priv;

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = self->priv;

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_det_code_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_det_label_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_det_has_base_toggled( GtkToggleButton *button, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	guint row;
	GtkWidget *entry;
	gboolean checked;

	priv = self->priv;
	checked = gtk_toggle_button_get_active( button );
	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_BASE, row );
	gtk_widget_set_sensitive( entry, checked && priv->is_current );
	//g_debug( "on_det_has_base_toggled: row=%u, checked=%s", row, checked ? "True":"False" );

	check_for_enable_dlg( self );
}

static void
on_det_base_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_det_has_amount_toggled( GtkToggleButton *button, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	guint row;
	GtkWidget *entry;
	gboolean checked;

	priv = self->priv;
	checked = gtk_toggle_button_get_active( button );
	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_AMOUNT, row );
	gtk_widget_set_sensitive( entry, checked && priv->is_current );
	//g_debug( "on_det_has_amount_toggled: row=%u, checked=%s", row, checked ? "True":"False" );

	check_for_enable_dlg( self );
}

static void
on_det_amount_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_bool_label_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

/*
 * are we able to validate this tva form
 */
static void
check_for_enable_dlg( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = is_dialog_validable( self );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

/*
 * are we able to validate this tva form
 */
static gboolean
is_dialog_validable( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	gboolean ok, exists, subok;
	gchar *msgerr;

	priv = self->priv;
	msgerr = NULL;
	ok = ofo_tva_form_is_valid( priv->dossier, priv->mnemo, &msgerr );
	//g_debug( "is_dialog_validable: is_valid=%s", ok ? "True":"False" );

	if( ok ){
		exists = ( ofo_tva_form_get_by_mnemo( priv->dossier, priv->mnemo ) != NULL );
		subok = !priv->is_new &&
						!g_utf8_collate( priv->mnemo, ofo_tva_form_get_mnemo( priv->tva_form ));
		ok = !exists || subok;
		if( !ok ){
			msgerr = g_strdup( _( "Mnemonic is already defined" ));
		}
	}
	set_message( self, msgerr );
	g_free( msgerr );

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_TVA_FORM_PROPERTIES( dialog )));
}

/*
 * either creating a new tva_form (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	gint i;
	GtkWidget *entry, *base_check, *amount_check, *spin;
	const gchar *code, *label, *base, *amount;
	gchar *prev_mnemo;
	guint rows_count, level;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;

	prev_mnemo = g_strdup( ofo_tva_form_get_mnemo( priv->tva_form ));

	ofo_tva_form_set_mnemo( priv->tva_form, priv->mnemo );
	ofo_tva_form_set_label( priv->tva_form, priv->label );
	ofo_tva_form_set_has_correspondence(
			priv->tva_form,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->corresp_btn )));
	my_utils_container_notes_get( my_window_get_toplevel( MY_WINDOW( self )), tva_form );

	rows_count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ));
	ofo_tva_form_detail_free_all( priv->tva_form );
	for( i=1 ; i<=rows_count ; ++i ){
		spin = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_LEVEL, i );
		g_return_val_if_fail( spin && GTK_IS_SPIN_BUTTON( spin ), FALSE );
		level = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( spin ));
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_CODE, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		code = gtk_entry_get_text( GTK_ENTRY( entry ));
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_LABEL, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		label = gtk_entry_get_text( GTK_ENTRY( entry ));
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_BASE, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		base = gtk_entry_get_text( GTK_ENTRY( entry ));
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_AMOUNT, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		amount = gtk_entry_get_text( GTK_ENTRY( entry ));
		if( my_strlen( code ) || my_strlen( label ) || my_strlen( base ) || my_strlen( amount )){
			base_check = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_BASE, i );
			amount_check = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_AMOUNT, i );
			ofo_tva_form_detail_add(
					priv->tva_form,
					level, code, label,
					gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( base_check )), base,
					gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( amount_check )), amount );
		}
	}

	rows_count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->bool_grid ));
	ofo_tva_form_boolean_free_all( priv->tva_form );
	for( i=1 ; i<=rows_count ; ++i ){
		entry = gtk_grid_get_child_at( GTK_GRID( priv->bool_grid ), 1+COL_BOOL_LABEL, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		label = gtk_entry_get_text( GTK_ENTRY( entry ));
		if( my_strlen( label )){
			ofo_tva_form_boolean_add( priv->tva_form, label );
		}
	}

	if( priv->is_new ){
		priv->updated =
				ofo_tva_form_insert( priv->tva_form, priv->dossier );
	} else {
		priv->updated =
				ofo_tva_form_update( priv->tva_form, priv->dossier, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}

static void
set_message( ofaTVAFormProperties *dialog, const gchar *msg )
{
	ofaTVAFormPropertiesPrivate *priv;

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
