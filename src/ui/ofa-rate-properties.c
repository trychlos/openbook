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

#include "my/my-date-editable.h"
#include "my/my-double-editable.h"
#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-rate.h"

#include "ui/ofa-rate-properties.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	GtkWindow  *parent;
	ofoRate    *rate;

	/* runtime
	 */
	gboolean    is_writable;
	gboolean    is_new;

	/* UI
	 */
	GtkWidget  *grid;				/* the grid which handles the validity rows */
	GtkWidget  *ok_btn;
	GtkWidget  *msg_label;

	/* data
	 */
	gchar      *mnemo;
	gchar      *label;
}
	ofaRatePropertiesPrivate;

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"

/* the columns in the dynamic grid
 */
enum {
	COL_BEGIN = 0,
	COL_BEGIN_LABEL,
	COL_END,
	COL_END_LABEL,
	COL_RATE,
	COL_RATE_LABEL,
	N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-rate-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     igridlist_iface_init( myIGridlistInterface *iface );
static guint    igridlist_get_interface_version( void );
static void     igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *empty );
static void     setup_detail_widgets( ofaRateProperties *self, guint row );
static void     set_detail_values( ofaRateProperties *self, guint row );
static void     on_mnemo_changed( GtkEntry *entry, ofaRateProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaRateProperties *self );
static void     on_date_changed( GtkEntry *entry, ofaRateProperties *self );
static void     on_rate_changed( GtkEntry *entry, ofaRateProperties *self );
static void     set_grid_line_comment( ofaRateProperties *self, GtkWidget *widget, const gchar *comment, gint column );
static void     check_for_enable_dlg( ofaRateProperties *self );
static gboolean is_dialog_validable( ofaRateProperties *self );
static void     on_ok_clicked( ofaRateProperties *self );
static gboolean do_update( ofaRateProperties *self, gchar **msgerr );
static void     set_msgerr( ofaRateProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaRateProperties, ofa_rate_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRateProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ))

static void
rate_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rate_properties_finalize";
	ofaRatePropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RATE_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_rate_properties_get_instance_private( OFA_RATE_PROPERTIES( instance ));

	g_free( priv->mnemo );
	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_properties_parent_class )->finalize( instance );
}

static void
rate_properties_dispose( GObject *instance )
{
	ofaRatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RATE_PROPERTIES( instance ));

	priv = ofa_rate_properties_get_instance_private( OFA_RATE_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_properties_parent_class )->dispose( instance );
}

static void
ofa_rate_properties_init( ofaRateProperties *self )
{
	static const gchar *thisfn = "ofa_rate_properties_init";
	ofaRatePropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_RATE_PROPERTIES( self ));

	priv = ofa_rate_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_rate_properties_class_init( ofaRatePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_rate_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_rate_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @rate: the #ofoRate to be displayed/updated.
 *
 * Update the properties of a rate.
 */
void
ofa_rate_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoRate *rate )
{
	static const gchar *thisfn = "ofa_rate_properties_run";
	ofaRateProperties *self;
	ofaRatePropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, rate=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) rate );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_RATE_PROPERTIES, NULL );

	priv = ofa_rate_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->rate = rate;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_rate_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_rate_properties_iwindow_init";
	ofaRatePropertiesPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_rate_properties_get_instance_private( OFA_RATE_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ), ofo_rate_get_mnemo( priv->rate ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_rate_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * rate
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_rate_properties_idialog_init";
	ofaRatePropertiesPrivate *priv;
	ofaHub *hub;
	gint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkWidget *label, *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_rate_properties_get_instance_private( OFA_RATE_PROPERTIES( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	mnemo = ofo_rate_get_mnemo( priv->rate );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new rate" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » rate" ), mnemo );
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

	priv->label = g_strdup( ofo_rate_get_label( priv->rate ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), instance );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( instance, rate );
	my_utils_container_updstamp_init( instance, rate );

	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	/* set detail rows after general sensitivity */
	priv->grid = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p2-grid" );
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));

	my_igridlist_init(
			MY_IGRIDLIST( instance ), GTK_GRID( priv->grid ),
			TRUE, priv->is_writable, N_COLUMNS );

	count = ofo_rate_get_val_count( priv->rate );
	for( idx=0 ; idx<count ; ++idx ){
		my_igridlist_add_row( MY_IGRIDLIST( instance ), GTK_GRID( priv->grid ), NULL );
	}

	check_for_enable_dlg( OFA_RATE_PROPERTIES( instance ));
}

/*
 * myIGridlist interface management
 */
static void
igridlist_iface_init( myIGridlistInterface *iface )
{
	static const gchar *thisfn = "ofa_rate_properties_igridlist_iface_init";

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
igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *empty )
{
	ofaRatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RATE_PROPERTIES( instance ));

	priv = ofa_rate_properties_get_instance_private( OFA_RATE_PROPERTIES( instance ));

	g_return_if_fail( grid == GTK_GRID( priv->grid ));

	setup_detail_widgets( OFA_RATE_PROPERTIES( instance ), row );
	set_detail_values( OFA_RATE_PROPERTIES( instance ), row );
}

static void
setup_detail_widgets( ofaRateProperties *self, guint row )
{
	ofaRatePropertiesPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_rate_properties_get_instance_private( self );

	entry = gtk_entry_new();
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_date_changed ), self );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->grid ),
			entry, 1+COL_BEGIN, row, 1, 1 );

	label = gtk_label_new( "" );
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	gtk_widget_set_sensitive( label, FALSE );
	my_utils_widget_set_margin_right( label, 4 );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 10 );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->grid ),
			label, 1+COL_BEGIN_LABEL, row, 1, 1 );

	entry = gtk_entry_new();
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_date_changed ), self );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->grid ),
			entry, 1+COL_END, row, 1, 1 );

	label = gtk_label_new( "" );
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	gtk_widget_set_sensitive( label, FALSE );
	my_utils_widget_set_margin_right( label, 4 );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 10 );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->grid ),
			label, 1+COL_END_LABEL, row, 1, 1 );

	entry = gtk_entry_new();
	my_double_editable_init_ex( GTK_EDITABLE( entry ),
			g_utf8_get_char( ofa_prefs_amount_get_thousand_sep( priv->getter )),
			g_utf8_get_char( ofa_prefs_amount_get_decimal_sep( priv->getter )),
			ofa_prefs_amount_get_accept_dot( priv->getter ),
			ofa_prefs_amount_get_accept_comma( priv->getter ),
			HUB_DEFAULT_DECIMALS_RATE );
	g_signal_connect( entry, "changed", G_CALLBACK( on_rate_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), 10 );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), 10 );
	gtk_widget_set_sensitive( entry, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->grid ),
			entry, 1+COL_RATE, row, 1, 1 );

	label = gtk_label_new( "" );
	gtk_widget_set_sensitive( label, FALSE );
	gtk_widget_set_hexpand( label, TRUE );
	my_utils_widget_set_margin_right( label, 4 );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 7 );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->grid ),
			label, 1+COL_RATE_LABEL, row, 1, 1 );
}

static void
set_detail_values( ofaRateProperties *self, guint row )
{
	ofaRatePropertiesPrivate *priv;
	GtkWidget *entry;
	const GDate *d;
	gdouble rate;
	guint idx;

	priv = ofa_rate_properties_get_instance_private( self );

	idx = row - 1;

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_BEGIN, row );
	d = ofo_rate_get_val_begin( priv->rate, idx );
	my_date_editable_set_date( GTK_EDITABLE( entry ), d );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_END, row );
	d = ofo_rate_get_val_end( priv->rate, idx );
	my_date_editable_set_date( GTK_EDITABLE( entry ), d );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_RATE, row );
	rate = ofo_rate_get_val_rate( priv->rate, idx );
	my_double_editable_set_amount( GTK_EDITABLE( entry ), rate );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;

	priv = ofa_rate_properties_get_instance_private( self );

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;

	priv = ofa_rate_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_date_changed( GtkEntry *entry, ofaRateProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_rate_changed( GtkEntry *entry, ofaRateProperties *self )
{
	const gchar *content;
	gchar *text, *str;

	content = gtk_entry_get_text( entry );

	if( !my_strlen( content )){
		str = g_strdup( "" );
	} else {
		text = my_double_editable_get_string( GTK_EDITABLE( entry ));
		str = g_strdup_printf( "%s %%", text );
		g_free( text );
	}

	set_grid_line_comment( self, GTK_WIDGET( entry ), str, 1+COL_RATE_LABEL );
	g_free( str );

	check_for_enable_dlg( self );
}

static void
set_grid_line_comment( ofaRateProperties *self, GtkWidget *widget, const gchar *comment, gint column )
{
	ofaRatePropertiesPrivate *priv;
	guint row;
	GtkLabel *label;
	gchar *markup;

	priv = ofa_rate_properties_get_instance_private( self );

	row = my_igridlist_get_row_index( widget );
	label = GTK_LABEL( gtk_grid_get_child_at( GTK_GRID( priv->grid ), column, row ));
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", comment );
	gtk_label_set_markup( label, markup );
	g_free( markup );
}

/*
 * are we able to validate this rate, and all its validities
 */
static void
check_for_enable_dlg( ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;

	priv = ofa_rate_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

/*
 * are we able to validate this rate, and all its validities
 */
static gboolean
is_dialog_validable( ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;
	GList *valids;
	ofsRateValidity *validity;
	gint i;
	GtkEntry *entry;
	gdouble vrate;
	gboolean ok;
	ofoRate *exists;
	const GDate *dbegin, *dend;
	guint count;
	gchar *msgerr;

	priv = ofa_rate_properties_get_instance_private( self );

	count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->grid ));
	for( i=1, valids=NULL ; i<=count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_BEGIN, i ));
		dbegin = my_date_editable_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_END, i ));
		dend = my_date_editable_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_RATE, i ));
		vrate = my_double_editable_get_amount( GTK_EDITABLE( entry ));

		if( my_date_is_valid( dbegin ) || my_date_is_valid( dend ) || vrate > 0 ){

			validity = g_new0( ofsRateValidity, 1 );
			my_date_set_from_date( &validity->begin, dbegin );
			my_date_set_from_date( &validity->end, dend );
			validity->rate = vrate;
			valids = g_list_prepend( valids, validity );
		}
	}

	ok = ofo_rate_is_valid_data( priv->mnemo, priv->label, g_list_reverse( valids ), &msgerr );

	g_list_free_full( valids, ( GDestroyNotify ) g_free );

	if( ok ){
		exists = ofo_rate_get_by_mnemo( priv->getter, priv->mnemo );
		ok &= !exists ||
				( !priv->is_new &&
						!g_utf8_collate( priv->mnemo, ofo_rate_get_mnemo( priv->rate )));
		if( !ok ){
			msgerr = g_strdup( _( "Rate already exists" ));
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

/*
 * either creating a new rate (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static void
on_ok_clicked( ofaRateProperties *self )
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
do_update( ofaRateProperties *self, gchar **msgerr )
{
	ofaRatePropertiesPrivate *priv;
	guint i, count;
	GtkEntry *entry;
	const GDate *dbegin, *dend;
	gchar *prev_mnemo, *str;
	gdouble rate;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = ofa_rate_properties_get_instance_private( self );

	prev_mnemo = g_strdup( ofo_rate_get_mnemo( priv->rate ));

	ofo_rate_set_mnemo( priv->rate, priv->mnemo );
	ofo_rate_set_label( priv->rate, priv->label );
	my_utils_container_notes_get( self, rate );

	ofo_rate_free_all_val( priv->rate );
	count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->grid ));

	for( i=1 ; i<=count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_BEGIN, i ));
		dbegin = my_date_editable_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_END, i ));
		dend = my_date_editable_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_RATE, i ));
		rate = my_double_editable_get_amount( GTK_EDITABLE( entry ));
		str = my_double_editable_get_string( GTK_EDITABLE( entry ));
		g_debug( "do_update: amount=%.5lf, str=%s", rate, str );
		if( my_date_is_valid( dbegin ) ||
			my_date_is_valid( dend ) ||
			( rate > 0 )){

			ofo_rate_add_val( priv->rate, dbegin, dend, rate );
		}
	}

	if( priv->is_new ){
		ok = ofo_rate_insert( priv->rate );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new rate" ));
		}
	} else {
		ok = ofo_rate_update( priv->rate, prev_mnemo );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the rate" ));
		}
	}

	g_free( prev_mnemo );

	return( ok );
}

static void
set_msgerr( ofaRateProperties *self, const gchar *msg )
{
	ofaRatePropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_rate_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
