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

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ope-template-editable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "tva/ofa-tva-form-properties.h"
#include "tva/ofo-tva-form.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIGetter    *getter;
	GtkWindow     *parent;

	/* runtime
	 */
	gboolean       is_writable;
	ofoTVAForm    *tva_form;
	gboolean       is_new;

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
	GList         *orig_templates;
}
	ofaTVAFormPropertiesPrivate;

#define DET_SPIN_WIDTH                    2
#define DET_SPIN_MAX_WIDTH                2
#define DET_CODE_MAX_LENGTH              64
#define DET_TEMPLATE_MAX_LENGTH          64
#define DET_LABEL_MAX_LENGTH            256
#define DET_BASE_MAX_LENGTH             256
#define DET_AMOUNT_MAX_LENGTH           256
#define BOOL_LABEL_MAX_LENGTH           256

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
	COL_DET_HAS_TEMPLATE,
	COL_DET_TEMPLATE,
	N_DET_COLUMNS
};

enum {
	COL_BOOL_LABEL = 0,
	N_BOOL_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/tva/ofa-tva-form-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     igridlist_iface_init( myIGridListInterface *iface );
static guint    igridlist_get_interface_version( void );
static void     igridlist_setup_row( const myIGridList *instance, GtkGrid *grid, guint row );
static void     setup_detail_widgets( ofaTVAFormProperties *self, guint row );
static void     set_detail_values( ofaTVAFormProperties *self, guint row );
static void     setup_boolean_widgets( ofaTVAFormProperties *self, guint row );
static void     set_boolean_values( ofaTVAFormProperties *self, guint row );
static void     on_mnemo_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_code_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_has_base_toggled( GtkToggleButton *button, ofaTVAFormProperties *self );
static void     on_det_base_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_has_amount_toggled( GtkToggleButton *button, ofaTVAFormProperties *self );
static void     on_det_amount_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_det_has_template_toggled( GtkToggleButton *button, ofaTVAFormProperties *self );
static void     on_det_template_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     on_bool_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void     check_for_enable_dlg( ofaTVAFormProperties *self );
static gboolean is_dialog_validable( ofaTVAFormProperties *self );
static void     on_ok_clicked( ofaTVAFormProperties *self );
static gboolean do_update( ofaTVAFormProperties *self, gchar **msgerr );
static void     set_msgerr( ofaTVAFormProperties *dialog, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaTVAFormProperties, ofa_tva_form_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaTVAFormProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ))

static void
tva_form_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_form_properties_finalize";
	ofaTVAFormPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_FORM_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_tva_form_properties_get_instance_private( OFA_TVA_FORM_PROPERTIES( instance ));

	g_free( priv->mnemo );
	g_free( priv->label );
	g_list_free_full( priv->orig_templates, ( GDestroyNotify ) g_free );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_properties_parent_class )->finalize( instance );
}

static void
tva_form_properties_dispose( GObject *instance )
{
	ofaTVAFormPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_FORM_PROPERTIES( instance ));

	priv = ofa_tva_form_properties_get_instance_private( OFA_TVA_FORM_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_properties_parent_class )->dispose( instance );
}

static void
ofa_tva_form_properties_init( ofaTVAFormProperties *self )
{
	static const gchar *thisfn = "ofa_tva_form_properties_init";
	ofaTVAFormPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_FORM_PROPERTIES( self ));

	priv = ofa_tva_form_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;
	priv->orig_templates = NULL;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_tva_form_properties_class_init( ofaTVAFormPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_tva_form_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_tva_form_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 * @form: the #ofoTVAForm to be displayed/updated.
 *
 * Update the properties of a tva_form.
 */
void
ofa_tva_form_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoTVAForm *form )
{
	static const gchar *thisfn = "ofa_tva_form_properties_run";
	ofaTVAFormProperties *self;
	ofaTVAFormPropertiesPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	g_debug( "%s: getter=%p, parent=%p, form=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) form );

	self = g_object_new( OFA_TYPE_TVA_FORM_PROPERTIES, NULL );

	priv = ofa_tva_form_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->tva_form = form;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_form_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
	iface->get_identifier = iwindow_get_identifier;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_tva_form_properties_iwindow_init";
	ofaTVAFormPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_form_properties_get_instance_private( OFA_TVA_FORM_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * identifier is built with class name and VAT form mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaTVAFormPropertiesPrivate *priv;
	gchar *id;

	priv = ofa_tva_form_properties_get_instance_private( OFA_TVA_FORM_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_tva_form_get_mnemo( priv->tva_form ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_form_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * tva_form
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_tva_form_properties_idialog_init";
	ofaTVAFormPropertiesPrivate *priv;
	ofaHub *hub;
	guint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkWidget *label, *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_form_properties_get_instance_private( OFA_TVA_FORM_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	mnemo = ofo_tva_form_get_mnemo( priv->tva_form );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new TVA form" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » TVA form" ), mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	/* mnemonic */
	priv->mnemo = g_strdup( mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-entry" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_mnemo_changed ), instance );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	priv->label = g_strdup( ofo_tva_form_get_label( priv->tva_form ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), instance );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	priv->corresp_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-has-corresp" );
	g_return_if_fail( priv->corresp_btn && GTK_TOGGLE_BUTTON( priv->corresp_btn ));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->corresp_btn ),
			ofo_tva_form_get_has_correspondence( priv->tva_form ));

	my_utils_container_notes_init( GTK_CONTAINER( instance ), tva_form );
	my_utils_container_updstamp_init( GTK_CONTAINER( instance ), tva_form );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );

	/* set the detail rows after having set editability for current
	 * dossier (because my_utils_container_set_editable() set the
	 * sensitivity flag without considering the has_amount flag or
	 * the row number - which is ok in general but not here) */
	priv->det_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p2-grid" );
	g_return_if_fail( priv->det_grid && GTK_IS_GRID( priv->det_grid ));
	my_igridlist_init(
			MY_IGRIDLIST( instance ), GTK_GRID( priv->det_grid ),
			TRUE, priv->is_writable, N_DET_COLUMNS );
	count = ofo_tva_form_detail_get_count( priv->tva_form );
	for( idx=0 ; idx<count ; ++idx ){
		my_igridlist_add_row( MY_IGRIDLIST( instance ), GTK_GRID( priv->det_grid ));
	}

	priv->bool_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p3-grid" );
	g_return_if_fail( priv->bool_grid && GTK_IS_GRID( priv->bool_grid ));
	my_igridlist_init(
			MY_IGRIDLIST( instance ), GTK_GRID( priv->bool_grid ),
			TRUE, priv->is_writable, N_BOOL_COLUMNS );
	count = ofo_tva_form_boolean_get_count( priv->tva_form );
	for( idx=0 ; idx<count ; ++idx ){
		my_igridlist_add_row( MY_IGRIDLIST( instance ), GTK_GRID( priv->bool_grid ));
	}

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	check_for_enable_dlg( OFA_TVA_FORM_PROPERTIES( instance ));
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
	iface->setup_row = igridlist_setup_row;
}

static guint
igridlist_get_interface_version( void )
{
	return( 1 );
}

static void
igridlist_setup_row( const myIGridList *instance, GtkGrid *grid, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_FORM_PROPERTIES( instance ));

	priv = ofa_tva_form_properties_get_instance_private( OFA_TVA_FORM_PROPERTIES( instance ));

	if( grid == GTK_GRID( priv->det_grid )){
		setup_detail_widgets( OFA_TVA_FORM_PROPERTIES( instance ), row );
		set_detail_values( OFA_TVA_FORM_PROPERTIES( instance ), row );
	}
	if( grid == GTK_GRID( priv->bool_grid )){
		setup_boolean_widgets( OFA_TVA_FORM_PROPERTIES( instance ), row );
		set_boolean_values( OFA_TVA_FORM_PROPERTIES( instance ), row );
	}
}

/*
 * row is the index of the current row, counted from zero (including headers)
 * so corresponding ofoTVAForm detail index is row-1
 */
static void
setup_detail_widgets( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry, *toggle, *spin;
	GtkAdjustment *adjustment;

	priv = ofa_tva_form_properties_get_instance_private( self );

	/* level */
	adjustment = gtk_adjustment_new( 1.0, 1.0, ( gdouble ) G_MAXUINT, 1.0, 10.0, 0.0 );
	spin = gtk_spin_button_new( adjustment, 1.0, 0 );
	gtk_entry_set_width_chars( GTK_ENTRY( spin ), DET_SPIN_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( spin ), DET_SPIN_MAX_WIDTH );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );
	gtk_widget_set_sensitive( spin, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			spin, 1+COL_DET_LEVEL, row, 1, 1 );

	/* code */
	entry = gtk_entry_new();
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_code_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), 4 );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_CODE_MAX_LENGTH );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			entry, 1+COL_DET_CODE, row, 1, 1 );

	/* label */
	entry = gtk_entry_new();
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_label_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_LABEL_MAX_LENGTH );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			entry, 1+COL_DET_LABEL, row, 1, 1 );

	/* has base */
	toggle = gtk_check_button_new();
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_det_has_base_toggled ), self );
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			toggle, 1+COL_DET_HAS_BASE, row, 1, 1 );

	/* base */
	entry = gtk_entry_new();
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_base_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_BASE_MAX_LENGTH );
	gtk_widget_set_sensitive( entry, FALSE );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			entry, 1+COL_DET_BASE, row, 1, 1 );

	/* has amount */
	toggle = gtk_check_button_new();
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_det_has_amount_toggled ), self );
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			toggle, 1+COL_DET_HAS_AMOUNT, row, 1, 1 );

	/* amount */
	entry = gtk_entry_new();
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_amount_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_AMOUNT_MAX_LENGTH );
	gtk_widget_set_sensitive( entry, FALSE );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			entry, 1+COL_DET_AMOUNT, row, 1, 1 );

	/* has template */
	toggle = gtk_check_button_new();
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_det_has_template_toggled ), self );
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			toggle, 1+COL_DET_HAS_TEMPLATE, row, 1, 1 );

	/* template */
	entry = gtk_entry_new();
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_template_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_TEMPLATE_MAX_LENGTH );
	gtk_widget_set_sensitive( entry, FALSE );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->det_grid ),
			entry, 1+COL_DET_TEMPLATE, row, 1, 1 );
	ofa_ope_template_editable_init( GTK_EDITABLE( entry ), priv->getter );
}

static void
set_detail_values( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *spin, *entry, *toggle, *previous_spin;
	const gchar *cstr;
	guint idx, level;
	gboolean has_base, has_amount, has_template;

	priv = ofa_tva_form_properties_get_instance_private( self );

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

	toggle = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_TEMPLATE, row );
	g_return_if_fail( toggle && GTK_IS_TOGGLE_BUTTON( toggle ));
	has_template = ofo_tva_form_detail_get_has_template( priv->tva_form, idx );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), has_template );
	on_det_has_template_toggled( GTK_TOGGLE_BUTTON( toggle ), self );

	cstr = ofo_tva_form_detail_get_template( priv->tva_form, idx );
	if( my_strlen( cstr )){
		priv->orig_templates = g_list_prepend( priv->orig_templates, g_strdup( cstr ));
	}
	if( my_strlen( cstr )){
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_TEMPLATE, row );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
}

static void
setup_boolean_widgets( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry;

	priv = ofa_tva_form_properties_get_instance_private( self );

	/* label */
	entry = gtk_entry_new();
	g_signal_connect( entry, "changed", G_CALLBACK( on_bool_label_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), BOOL_LABEL_MAX_LENGTH );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->bool_grid ),
			entry, 1+COL_BOOL_LABEL, row, 1, 1 );
}

static void
set_boolean_values( ofaTVAFormProperties *self, guint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry;
	const gchar *cstr;
	guint idx;

	priv = ofa_tva_form_properties_get_instance_private( self );

	idx = row-1;

	entry = gtk_grid_get_child_at( GTK_GRID( priv->bool_grid ), 1+COL_BOOL_LABEL, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_form_boolean_get_label( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
}

static void
on_mnemo_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = ofa_tva_form_properties_get_instance_private( self );

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = ofa_tva_form_properties_get_instance_private( self );

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

	priv = ofa_tva_form_properties_get_instance_private( self );

	checked = gtk_toggle_button_get_active( button );
	row = my_igridlist_get_row_index( GTK_WIDGET( button ));
	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_BASE, row );
	gtk_widget_set_sensitive( entry, checked && priv->is_writable );
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

	priv = ofa_tva_form_properties_get_instance_private( self );

	checked = gtk_toggle_button_get_active( button );
	row = my_igridlist_get_row_index( GTK_WIDGET( button ));
	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_AMOUNT, row );
	gtk_widget_set_sensitive( entry, checked && priv->is_writable );
	//g_debug( "on_det_has_amount_toggled: row=%u, checked=%s", row, checked ? "True":"False" );

	check_for_enable_dlg( self );
}

static void
on_det_amount_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_det_has_template_toggled( GtkToggleButton *button, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	guint row;
	GtkWidget *entry;
	gboolean checked;

	priv = ofa_tva_form_properties_get_instance_private( self );

	checked = gtk_toggle_button_get_active( button );
	row = my_igridlist_get_row_index( GTK_WIDGET( button ));
	entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_TEMPLATE, row );
	gtk_widget_set_sensitive( entry, checked && priv->is_writable );
	//g_debug( "on_det_has_template_toggled: row=%u, checked=%s", row, checked ? "True":"False" );

	check_for_enable_dlg( self );
}

static void
on_det_template_changed( GtkEntry *entry, ofaTVAFormProperties *self )
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

	priv = ofa_tva_form_properties_get_instance_private( self );

	if( priv->is_writable ){
		ok = is_dialog_validable( self );
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
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

	priv = ofa_tva_form_properties_get_instance_private( self );

	msgerr = NULL;

	ok = ofo_tva_form_is_valid_data( priv->mnemo, priv->label, &msgerr );

	if( ok ){
		exists = ( ofo_tva_form_get_by_mnemo( priv->getter, priv->mnemo ) != NULL );
		subok = !priv->is_new &&
						!g_utf8_collate( priv->mnemo, ofo_tva_form_get_mnemo( priv->tva_form ));
		ok = !exists || subok;
		if( !ok ){
			msgerr = g_strdup( _( "Mnemonic is already defined" ));
		}
	}
	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

/*
 * either creating a new tva_form (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static void
on_ok_clicked( ofaTVAFormProperties *self )
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
do_update( ofaTVAFormProperties *self, gchar **msgerr )
{
	ofaTVAFormPropertiesPrivate *priv;
	ofaISignaler *signaler;
	gint i;
	GtkWidget *entry, *base_check, *amount_check, *template_check, *spin;
	const gchar *code, *label, *base, *amount, *template;
	gchar *prev_mnemo;
	guint rows_count, level;
	gboolean ok;
	GList *it;
	ofoOpeTemplate *template_obj;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_tva_form_properties_get_instance_private( self );

	prev_mnemo = g_strdup( ofo_tva_form_get_mnemo( priv->tva_form ));

	ofo_tva_form_set_mnemo( priv->tva_form, priv->mnemo );
	ofo_tva_form_set_label( priv->tva_form, priv->label );
	ofo_tva_form_set_has_correspondence(
			priv->tva_form,
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->corresp_btn )));
	my_utils_container_notes_get( GTK_WINDOW( self ), tva_form );

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
		entry = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_TEMPLATE, i );
		g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
		template = gtk_entry_get_text( GTK_ENTRY( entry ));
		if( my_strlen( code ) || my_strlen( label ) || my_strlen( base ) || my_strlen( amount ) || my_strlen( template )){
			base_check = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_BASE, i );
			amount_check = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_AMOUNT, i );
			template_check = gtk_grid_get_child_at( GTK_GRID( priv->det_grid ), 1+COL_DET_HAS_TEMPLATE, i );
			ofo_tva_form_detail_add(
					priv->tva_form,
					level, code, label,
					gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( base_check )), base,
					gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( amount_check )), amount,
					gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( template_check )), template );
		}
		/* add to orig_templates the new values */
		if( my_strlen( template )){
			if( !g_list_find_custom( priv->orig_templates, template, ( GCompareFunc ) my_collate )){
				priv->orig_templates = g_list_prepend( priv->orig_templates, g_strdup( template ));
			}
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
		ok = ofo_tva_form_insert( priv->tva_form );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new VAT form" ));
		}
	} else {
		ok = ofo_tva_form_update( priv->tva_form, prev_mnemo );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the VAT form" ));
		}
	}

	g_free( prev_mnemo );

	/* asks the template store to auto-update
	 * targets all templates which were initially used + those added during the update */

	for( it=priv->orig_templates ; it ; it=it->next ){
		template_obj = ofo_ope_template_get_by_mnemo( priv->getter, ( const gchar * ) it->data );

		if( template_obj ){
			signaler = ofa_igetter_get_signaler( priv->getter );
			g_signal_emit_by_name( signaler, SIGNALER_BASE_UPDATED, template_obj, NULL );
		}
	}

	return( ok );
}

static void
set_msgerr( ofaTVAFormProperties *dialog, const gchar *msg )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = ofa_tva_form_properties_get_instance_private( dialog );

	if( !priv->msg_label ){
		priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
		my_style_add( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
