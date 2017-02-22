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

#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-paimean-editable.h"
#include "api/ofo-dossier.h"

#include "recurrent/ofa-rec-period-properties.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;
	GtkWindow      *parent;
	ofoRecPeriod   *rec_period;

	/* internals
	 */
	gboolean        is_writable;
	gboolean        is_new;

	/* UI
	 */
	GtkWidget      *p1_id_label;
	GtkWidget      *p1_order_spin;
	GtkWidget      *p1_label_entry;
	GtkWidget      *p3_details_grid;
	GtkWidget      *msg_label;
	GtkWidget      *ok_btn;
}
	ofaRecPeriodPropertiesPrivate;

/* details gridllist:
 *
 * each line of the grid is :
 * - button 'Add' (if last line)
 * - type number
 * - label
 * - value
 * - button up
 * - button down
 * - button remove
 */
/* columns in the detail treeview
 */
enum {
	DET_COL_NUMBER = 0,
	DET_COL_LABEL,
	DET_COL_VALUE,
	DET_N_COLUMNS
};

/* horizontal space between widgets in a detail line */
#define DETAIL_SPACE                      0
#define DET_SPIN_WIDTH                    4
#define DET_SPIN_MAX_WIDTH                4

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-rec-period-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_dialog( ofaRecPeriodProperties *self );
static void     init_properties( ofaRecPeriodProperties *self );
static void     init_details( ofaRecPeriodProperties *self );
static void     setup_properties( ofaRecPeriodProperties *self );
static void     igridlist_iface_init( myIGridListInterface *iface );
static guint    igridlist_get_interface_version( void );
static void     igridlist_setup_row( const myIGridList *instance, GtkGrid *grid, guint row, void *empty );
static void     init_detail_widgets( ofaRecPeriodProperties *self, guint row );
static void     setup_detail_values( ofaRecPeriodProperties *self, guint row );
static void     on_order_changed( GtkSpinButton *btn, ofaRecPeriodProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaRecPeriodProperties *self );
static void     check_for_enable_dlg( ofaRecPeriodProperties *self );
static gboolean is_dialog_validable( ofaRecPeriodProperties *self );
static void     on_ok_clicked( ofaRecPeriodProperties *self );
static gboolean do_update( ofaRecPeriodProperties *self, gchar **msgerr );
static void     get_detail_list( ofaRecPeriodProperties *self, gint row );
static void     set_msgerr( ofaRecPeriodProperties *self, const gchar *msg );

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

	priv = ofa_rec_period_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
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

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_rec_period_properties_iwindow_init";
	ofaRecPeriodPropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_rec_period_properties_get_instance_private( OFA_REC_PERIOD_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
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
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_rec_period_properties_get_instance_private( OFA_REC_PERIOD_PROPERTIES( instance ));

	/* validate and record the properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	init_dialog( OFA_REC_PERIOD_PROPERTIES( instance ));
	init_properties( OFA_REC_PERIOD_PROPERTIES( instance ));

	my_utils_container_notes_init( instance, rec_period );
	my_utils_container_updstamp_init( instance, rec_period );

	if( priv->is_writable ){
		gtk_widget_grab_focus(
				my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-order-spin" ));
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
	const gchar *id;
	gchar *title;

	priv = ofa_rec_period_properties_get_instance_private( self );

	id = ofo_rec_period_get_id( priv->rec_period );
	priv->is_new = ( my_strlen( id ) == 0 );

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
	GtkWidget *label, *entry, *spin;

	priv = ofa_rec_period_properties_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-id-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->p1_id_label = label;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-order-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	spin = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-order-spin" );
	g_return_if_fail( spin && GTK_IS_SPIN_BUTTON( spin ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), spin );
	g_signal_connect( spin, "value-changed", G_CALLBACK( on_order_changed ), self );
	priv->p1_order_spin = spin;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );
	priv->p1_label_entry = entry;
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
			TRUE, priv->is_writable, DET_N_COLUMNS );

	count = ofo_rec_period_detail_get_count( priv->rec_period );
	for( i=1 ; i<=count ; ++i ){
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ), NULL );
	}
}

static void
setup_properties( ofaRecPeriodProperties *self )
{
	ofaRecPeriodPropertiesPrivate *priv;
	const gchar *cstr;

	priv = ofa_rec_period_properties_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p1_id_label ),
			ofo_rec_period_get_id( priv->rec_period ));

	gtk_spin_button_set_value( GTK_SPIN_BUTTON( priv->p1_order_spin ),
			ofo_rec_period_get_order( priv->rec_period ));

	cstr = ofo_rec_period_get_label( priv->rec_period );
	gtk_entry_set_text( GTK_ENTRY( priv->p1_label_entry ), cstr );
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
igridlist_setup_row( const myIGridList *instance, GtkGrid *grid, guint row, void *empty )
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
	GtkWidget *spin, *entry;
	GtkAdjustment *adjustment;

	priv = ofa_rec_period_properties_get_instance_private( self );

	/* detail type number */
	adjustment = gtk_adjustment_new( 1.0, 1.0, ( gdouble ) G_MAXUINT, 1.0, 10.0, 0.0 );
	spin = gtk_spin_button_new( adjustment, 1.0, 0 );
	gtk_entry_set_width_chars( GTK_ENTRY( spin ), DET_SPIN_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( spin ), DET_SPIN_MAX_WIDTH );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );
	my_utils_widget_set_margin_left( spin, DETAIL_SPACE );
	gtk_widget_set_sensitive( spin, priv->is_writable );
	if( priv->is_writable ){
		gtk_widget_grab_focus( spin );
	}
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ),
			spin, 1+DET_COL_NUMBER, row, 1, 1 );

	/* detail label */
	entry = gtk_entry_new();
	my_utils_widget_set_margin_left( entry, DETAIL_SPACE );
	gtk_widget_set_halign( entry, GTK_ALIGN_START );
	gtk_entry_set_alignment( GTK_ENTRY( entry ), 0 );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), REC_PERIOD_LABEL_MAX );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), REC_PERIOD_LABEL_MAX );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ),
			entry, 1+DET_COL_LABEL, row, 1, 1 );

	/* detail value */
	adjustment = gtk_adjustment_new( 1.0, 1.0, ( gdouble ) G_MAXUINT, 1.0, 10.0, 0.0 );
	spin = gtk_spin_button_new( adjustment, 1.0, 0 );
	gtk_entry_set_width_chars( GTK_ENTRY( spin ), DET_SPIN_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( spin ), DET_SPIN_MAX_WIDTH );
	gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( spin ), TRUE );
	my_utils_widget_set_margin_left( entry, DETAIL_SPACE );
	gtk_widget_set_sensitive( spin, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ),
			spin, 1+DET_COL_VALUE, row, 1, 1 );
}

static void
setup_detail_values( ofaRecPeriodProperties *self, guint row )
{
	ofaRecPeriodPropertiesPrivate *priv;
	GtkWidget *spin, *entry;
	const gchar *cstr;

	priv = ofa_rec_period_properties_get_instance_private( self );

	spin = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_NUMBER, row );
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( spin ), ofo_rec_period_detail_get_number( priv->rec_period, row-1 ));

	entry = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_LABEL, row );
	cstr = ofo_rec_period_detail_get_label( priv->rec_period, row-1 );
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr ? cstr : "" );

	spin = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_VALUE, row );
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( spin ), ofo_rec_period_detail_get_value( priv->rec_period, row-1 ));
}

static void
on_order_changed( GtkSpinButton *btn, ofaRecPeriodProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaRecPeriodProperties *self )
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
	const gchar *clabel;
	gchar *msgerr;

	priv = ofa_rec_period_properties_get_instance_private( self );
	msgerr = NULL;

	clabel = gtk_entry_get_text( GTK_ENTRY( priv->p1_label_entry ));

	ok = ofo_rec_period_is_valid_data( clabel, &msgerr );

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static void
on_ok_clicked( ofaRecPeriodProperties *self )
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
do_update( ofaRecPeriodProperties *self, gchar **msgerr )
{
	ofaRecPeriodPropertiesPrivate *priv;
	const gchar *clabel;
	guint i, count;
	gboolean ok;

	priv = ofa_rec_period_properties_get_instance_private( self );

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	clabel = gtk_entry_get_text( GTK_ENTRY( priv->p1_label_entry ));
	my_utils_container_notes_get( GTK_WINDOW( self ), rec_period );

	ofo_rec_period_set_label( priv->rec_period, clabel );

	ofo_rec_period_free_detail_all( priv->rec_period );
	count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->p3_details_grid ));
	for( i=1 ; i<=count ; ++i ){
		get_detail_list( self, i );
	}

	if( priv->is_new ){
		ok = ofo_rec_period_insert( priv->rec_period );
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
	GtkWidget *spin, *entry;
	const gchar *clabel;
	guint number, value;

	priv = ofa_rec_period_properties_get_instance_private( self );

	spin = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_NUMBER, row );
	number = ( guint ) gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( spin ));

	entry = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_LABEL, row );
	clabel = gtk_entry_get_text( GTK_ENTRY( entry ));

	spin = gtk_grid_get_child_at( GTK_GRID( priv->p3_details_grid ), 1+DET_COL_VALUE, row );
	value = ( guint ) gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( spin ));

	ofo_rec_period_add_detail( priv->rec_period, row-1, clabel, number, value );
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
