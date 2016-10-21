/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-paimean-editable.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "recurrent/ofa-rec-period-properties.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;
	ofoRecPeriod   *rec_period;

	/* internals
	 */
	gboolean        is_writable;
	gboolean        is_new;

	/* UI
	 */
	GtkWidget      *p1_id_label;
	GtkWidget      *p1_order_label;
	GtkWidget      *p1_label_entry;
	GtkWidget      *p1_havedetails_btn;
	GtkWidget      *p1_addtype_box;
	GtkWidget      *p1_addcount_spin;
	GtkWidget      *p3_details_grid;
	GtkWidget      *msg_label;
	GtkWidget      *ok_btn;
}
	ofaRecPeriodPropertiesPrivate;

/* when selecting the type of data to be added
 */
typedef struct {
	const gchar *code;
	const gchar *label;
}
	sAddType;

static const sAddType st_add_type[] = {
		{ REC_PERIOD_DAY,   N_( "Day" ) },
		{ REC_PERIOD_WEEK,  N_( "Week" ) },
		{ REC_PERIOD_MONTH, N_( "Month" ) },
		{ 0 }
};

enum {
	TYPE_COL_CODE = 0,
	TYPE_COL_LABEL,
	TYPE_N_COLUMNS
};

/* details gridllist:
 *
 * each line of the grid is :
 * - button 'Add' (if last line)
 * - label
 * - button up
 * - button down
 * - button remove
 */
/* columns in the detail treeview
 */
enum {
	DET_COL_LABEL = 0,
	DET_N_COLUMNS
};

/* horizontal space between widgets in a detail line */
#define DETAIL_SPACE                    0

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-rec-period-properties.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      init_dialog( ofaRecPeriodProperties *self );
static void      init_properties( ofaRecPeriodProperties *self );
static void      init_details( ofaRecPeriodProperties *self );
static void      setup_properties( ofaRecPeriodProperties *self );
static void      igridlist_iface_init( myIGridListInterface *iface );
static guint     igridlist_get_interface_version( void );
static void      igridlist_setup_row( const myIGridList *instance, GtkGrid *grid, guint row );
static void      init_detail_widgets( ofaRecPeriodProperties *self, guint row );
static void      setup_detail_values( ofaRecPeriodProperties *self, guint row );
static void      on_label_changed( GtkEntry *entry, ofaRecPeriodProperties *self );
static void      on_have_details_toggled( GtkToggleButton *toggle, ofaRecPeriodProperties *self );
static void      on_add_type_changed( GtkComboBox *combo, ofaRecPeriodProperties *self );
static void      on_add_count_changed( GtkSpinButton *btn, ofaRecPeriodProperties *self );
static void      check_for_enable_dlg( ofaRecPeriodProperties *self );
static gboolean  is_dialog_validable( ofaRecPeriodProperties *self );
static gboolean  do_update( ofaRecPeriodProperties *self, gchar **msgerr );
static void      get_detail_list( ofaRecPeriodProperties *self, gint row );
static void      set_msgerr( ofaRecPeriodProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaRecPeriodProperties, ofa_rec_period_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRecPeriodProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ))

static void
rec_period_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rec_period_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_PROPERTIES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_properties_parent_class )->finalize( instance );
}

static void
rec_period_properties_dispose( GObject *instance )
{
	ofaRecPeriodPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_PROPERTIES( instance ));

	priv = ofa_rec_period_properties_get_instance_private( OFA_REC_PERIOD_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rec_period_properties_parent_class )->dispose( instance );
}

static void
ofa_rec_period_properties_init( ofaRecPeriodProperties *self )
{
	static const gchar *thisfn = "ofa_rec_period_properties_init";
	ofaRecPeriodPropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_REC_PERIOD_PROPERTIES( self ));

	priv = ofa_rec_period_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_rec_period_properties_class_init( ofaRecPeriodPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_rec_period_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rec_period_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = rec_period_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_rec_period_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent of this dialog.
 * @period: [allow-none]: a #ofoRecPeriod to be edited.
 *
 * Creates or represents a #ofaRecPeriodProperties non-modal dialog
 * to edit the @template.
 */
void
ofa_rec_period_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoRecPeriod *period )
{
	static const gchar *thisfn = "ofa_rec_period_properties_run";
	ofaRecPeriodProperties *self;
	ofaRecPeriodPropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, period=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) period );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( !period || OFO_IS_REC_PERIOD( period ));

	self = g_object_new( OFA_TYPE_REC_PERIOD_PROPERTIES, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_rec_period_properties_get_instance_private( self );

	priv->getter = getter;
	priv->rec_period = period;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_rec_period_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_rec_period_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_rec_period_properties_idialog_init";
	ofaRecPeriodPropertiesPrivate *priv;
	ofaHub *hub;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_rec_period_properties_get_instance_private( OFA_REC_PERIOD_PROPERTIES( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	init_dialog( OFA_REC_PERIOD_PROPERTIES( instance ));
	init_properties( OFA_REC_PERIOD_PROPERTIES( instance ));

	my_utils_container_notes_init( instance, rec_period );
	my_utils_container_updstamp_init( instance, rec_period );

	if( priv->is_writable ){
		gtk_widget_grab_focus(
				my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-entry" ));
	}

	/* if not the current exercice, then only have a 'Close' button */
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	/* init dialog detail rows after having globally set the fields
	 * sensitivity so that IGridList can individually adjust rows sensitivity */
	init_details( OFA_REC_PERIOD_PROPERTIES( instance ));

	/* last, setup the data */
	setup_properties( OFA_REC_PERIOD_PROPERTIES( instance ));

	check_for_enable_dlg( OFA_REC_PERIOD_PROPERTIES( instance ));
}

static void
init_dialog( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	ofxCounter id;
	gchar *title;

	priv = ofa_rec_period_properties_get_instance_private( self );

	id = ofo_rec_period_get_id( priv->rec_period );
	priv->is_new = ( id == 0 );

	if( priv->is_new ){
		title = g_strdup( _( "Defining a new periodicity" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » periodicity" ), ofo_rec_period_get_label( priv->rec_period ));
	}

	gtk_window_set_title( GTK_WINDOW( self ), title );
	g_free( title );
}

static void
init_properties( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	GtkWidget *label, *entry, *button, *combo, *spin;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	gint i;

	priv = ofa_rec_period_properties_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-id-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->p1_id_label = label;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-order-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->p1_order_label = label;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );
	priv->p1_label_entry = entry;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-details-btn" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_have_details_toggled ), self );
	priv->p1_havedetails_btn = button;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-addtype-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-addtype-combo" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	store = gtk_list_store_new( TYPE_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	for( i=0 ; st_add_type[i].code ; ++i ){
		gtk_list_store_insert_with_values( store, &iter,
				TYPE_COL_CODE, st_add_type[i].code,
				TYPE_COL_LABEL, st_add_type[i].label,
				-1 );
	}
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", TYPE_COL_LABEL );
	gtk_combo_box_set_id_column( GTK_COMBO_BOX( combo ), TYPE_COL_CODE );

	g_signal_connect( combo, "changed", G_CALLBACK( on_add_type_changed ), self );
	priv->p1_addtype_box = combo;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-addcount-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	spin = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-addcount-spin" );
	g_return_if_fail( spin && GTK_IS_SPIN_BUTTON( spin ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), spin );
	g_signal_connect( spin, "value-changed", G_CALLBACK( on_add_count_changed ), self );
	priv->p1_addcount_spin = spin;
}

/*
 * prepare the IGridList with one line per detail
 */
static void
init_details( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	gint count, i;

	priv = ofa_rec_period_properties_get_instance_private( self );

	priv->p3_details_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-grid" );
	g_return_if_fail( priv->p3_details_grid && GTK_IS_GRID( priv->p3_details_grid ));

	my_igridlist_init(
			MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ),
			TRUE, priv->is_writable, TYPE_N_COLUMNS );

	count = ofo_rec_period_detail_get_count( priv->rec_period );
	for( i=1 ; i<=count ; ++i ){
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ));
	}
}

static void
setup_properties( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	gchar *str;
	const gchar *cstr;

	priv = ofa_rec_period_properties_get_instance_private( self );

	str = g_strdup_printf( "%lu", ofo_rec_period_get_id( priv->rec_period ));
	gtk_label_set_text( GTK_LABEL( priv->p1_id_label ), str );
	g_free( str );

	str = g_strdup_printf( "%u", ofo_rec_period_get_order( priv->rec_period ));
	gtk_label_set_text( GTK_LABEL( priv->p1_order_label ), str );
	g_free( str );

	cstr = ofo_rec_period_get_label( priv->rec_period );
	gtk_entry_set_text( GTK_ENTRY( priv->p1_label_entry ), cstr );

	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( priv->p1_havedetails_btn ),
			ofo_rec_period_get_have_details( priv->rec_period ));

	cstr = ofo_rec_period_get_add_type( priv->rec_period );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->p1_addtype_box ), cstr );

	gtk_spin_button_set_value(
			GTK_SPIN_BUTTON( priv->p1_addcount_spin ),
			ofo_rec_period_get_add_count( priv->rec_period ));
}

/*
 * myIGridList interface management
 */
static void
igridlist_iface_init( myIGridListInterface *iface )
{
	static const gchar *thisfn = "ofa_ofa_rec_period_properties_igridlist_iface_init";

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
	ofaRecPeriodPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_REC_PERIOD_PROPERTIES( instance ));

	priv = ofa_rec_period_properties_get_instance_private( OFA_REC_PERIOD_PROPERTIES( instance ));
	g_return_if_fail( grid == GTK_GRID( priv->p3_details_grid ));

	init_detail_widgets( OFA_REC_PERIOD_PROPERTIES( instance ), row );
	setup_detail_values( OFA_REC_PERIOD_PROPERTIES( instance ), row );
}

static void
init_detail_widgets( ofaRecPeriodProperties *self, guint row )
{
	ofaRecPeriodPropertiesPrivate *priv;
	GtkWidget *entry;

	priv = ofa_rec_period_properties_get_instance_private( self );

	/* detail label */
	entry = gtk_entry_new();
	my_utils_widget_set_margin_left( entry, DETAIL_SPACE );
	gtk_widget_set_halign( entry, GTK_ALIGN_START );
	gtk_entry_set_alignment( GTK_ENTRY( entry ), 0 );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), REC_PERIOD_LABEL_MAX );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), REC_PERIOD_LABEL_MAX );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	if( priv->is_writable ){
		gtk_widget_grab_focus( entry );
	}
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ),
			entry, 1+DET_COL_LABEL, row, 1, 1 );
}

static void
setup_detail_values( ofaRecPeriodProperties *self, guint row )
{
	ofaRecPeriodPropertiesPrivate *priv;
	GtkWidget *entry;
	const gchar *cstr;

	priv = ofa_rec_period_properties_get_instance_private( self );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_LABEL, row );
	cstr = ofo_rec_period_detail_get_label( priv->rec_period, row-1 );
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr ? cstr : "" );
}

static void
on_label_changed( GtkEntry *entry, ofaRecPeriodProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_have_details_toggled( GtkToggleButton *toggle, ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	gboolean active;

	priv = ofa_rec_period_properties_get_instance_private( self );

	active = gtk_toggle_button_get_active( toggle );
	gtk_widget_set_sensitive( priv->p3_details_grid, active );

	check_for_enable_dlg( self );
}

static void
on_add_type_changed( GtkComboBox *combo, ofaRecPeriodProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_add_count_changed( GtkSpinButton *btn, ofaRecPeriodProperties *self )
{
	check_for_enable_dlg( self );
}

/*
 * we accept to save uncomplete detail lines
 */
static void
check_for_enable_dlg( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;

	priv = ofa_rec_period_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

/*
 * detail order is reinitialized from the current display
 */
static gboolean
is_dialog_validable( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	gboolean ok;
	const gchar *clabel, *caddtype;
	gboolean havedetails;
	guint addcount;
	gchar *msgerr;

	priv = ofa_rec_period_properties_get_instance_private( self );
	msgerr = NULL;

	clabel = gtk_entry_get_text( GTK_ENTRY( priv->p1_label_entry ));
	havedetails = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_havedetails_btn ));
	caddtype = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->p1_addtype_box ));
	addcount = ( guint ) gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( priv->p1_addcount_spin ));

	ok = ofo_rec_period_is_valid_data( clabel, havedetails, caddtype, addcount, &msgerr );

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static gboolean
do_update( ofaRecPeriodProperties *self, gchar **msgerr )
{
	ofaRecPeriodPropertiesPrivate *priv;
	const gchar *clabel, *caddtype;
	gboolean havedetails;
	guint addcount;
	guint i, count;
	gboolean ok;
	ofaHub *hub;

	priv = ofa_rec_period_properties_get_instance_private( self );

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	hub = ofa_igetter_get_hub( priv->getter );

	clabel = gtk_entry_get_text( GTK_ENTRY( priv->p1_label_entry ));
	havedetails = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( priv->p1_havedetails_btn ));
	caddtype = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->p1_addtype_box ));
	addcount = ( guint ) gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( priv->p1_addcount_spin ));
	my_utils_container_notes_get( GTK_WINDOW( self ), rec_period );

	ofo_rec_period_set_label( priv->rec_period, clabel );
	ofo_rec_period_set_have_details( priv->rec_period, havedetails );
	ofo_rec_period_set_add_type( priv->rec_period, caddtype );
	ofo_rec_period_set_add_count( priv->rec_period, addcount );

	ofo_rec_period_free_detail_all( priv->rec_period );
	count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ));
	for( i=1 ; i<=count ; ++i ){
		get_detail_list( self, i );
	}

	if( priv->is_new ){
		ok = ofo_rec_period_insert( priv->rec_period, hub );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new periodicity" ));
		}
	} else {
		ok = ofo_rec_period_update( priv->rec_period );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the periodicity" ));
		}
	}

	return( ok );
}

/*
 * @row: row index in the grid (starting from 1)
 */
static void
get_detail_list( ofaRecPeriodProperties *self, gint row )
{
	ofaRecPeriodPropertiesPrivate *priv;
	GtkWidget *entry;
	const gchar *clabel;

	priv = ofa_rec_period_properties_get_instance_private( self );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_LABEL, row );
	clabel = gtk_entry_get_text( GTK_ENTRY( entry ));

	ofo_rec_period_add_detail( priv->rec_period, row-1, clabel );
}

static void
set_msgerr( ofaRecPeriodProperties *self, const gchar *msg )
{
	ofaRecPeriodPropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_rec_period_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
