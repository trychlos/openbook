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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-editable-amount.h"
#include "api/my-editable-date.h"
#include "api/my-igridlist.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-rate.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-rate-properties.h"

/* private instance data
 */
struct _ofaRatePropertiesPrivate {

	/* internals
	 */
	ofaHub        *hub;
	gboolean       is_current;
	ofoRate       *rate;
	gboolean       is_new;
	gboolean       updated;

	/* UI
	 */
	GtkWidget     *grid;				/* the grid which handles the validity rows */
	GtkWidget     *ok_btn;

	/* data
	 */
	gchar         *mnemo;
	gchar         *label;
};

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"
#define DEFAULT_RATE_DECIMALS           3

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

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-rate-properties.ui";
static const gchar  *st_ui_id        = "RatePropertiesDlg";

static void      igridlist_iface_init( myIGridListInterface *iface );
static guint     igridlist_get_interface_version( const myIGridList *instance );
static void      igridlist_set_row( const myIGridList *instance, GtkGrid *grid, guint row );
static void      set_detail_widgets( ofaRateProperties *self, guint row );
static void      set_detail_values( ofaRateProperties *self, guint row );
static void      v_init_dialog( myDialog *dialog );
static void      on_mnemo_changed( GtkEntry *entry, ofaRateProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaRateProperties *self );
static void      on_date_changed( GtkEntry *entry, ofaRateProperties *self );
static void      on_rate_changed( GtkEntry *entry, ofaRateProperties *self );
static void      set_grid_line_comment( ofaRateProperties *self, GtkWidget *widget, const gchar *comment, gint column );
static void      check_for_enable_dlg( ofaRateProperties *self );
static gboolean  is_dialog_validable( ofaRateProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaRateProperties *self );

G_DEFINE_TYPE_EXTENDED( ofaRateProperties, ofa_rate_properties, MY_TYPE_DIALOG, 0, \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ));

static void
rate_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_rate_properties_finalize";
	ofaRatePropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_RATE_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_RATE_PROPERTIES( instance )->priv;
	g_free( priv->mnemo );
	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_properties_parent_class )->finalize( instance );
}

static void
rate_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_RATE_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_rate_properties_parent_class )->dispose( instance );
}

static void
ofa_rate_properties_init( ofaRateProperties *self )
{
	static const gchar *thisfn = "ofa_rate_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_RATE_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_RATE_PROPERTIES, ofaRatePropertiesPrivate );

	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
}

static void
ofa_rate_properties_class_init( ofaRatePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_rate_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = rate_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaRatePropertiesPrivate ));
}

/*
 * myIGridList interface management
 */
static void
igridlist_iface_init( myIGridListInterface *iface )
{
	static const gchar *thisfn = "ofa_rate_properties_igridlist_iface_init";

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
	ofaRatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RATE_PROPERTIES( instance ));

	priv = OFA_RATE_PROPERTIES( instance )->priv;
	g_return_if_fail( grid == GTK_GRID( priv->grid ));

	set_detail_widgets( OFA_RATE_PROPERTIES( instance ), row );
	set_detail_values( OFA_RATE_PROPERTIES( instance ), row );
}

static void
set_detail_widgets( ofaRateProperties *self, guint row )
{
	ofaRatePropertiesPrivate *priv;
	GtkWidget *entry, *label;

	priv = self->priv;

	entry = gtk_entry_new();
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_grid_attach( GTK_GRID( priv->grid ), entry, 1+COL_BEGIN, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	label = gtk_label_new( "" );
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	gtk_widget_set_sensitive( label, FALSE );
	my_utils_widget_set_margin_right( label, 4 );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 10 );
	gtk_grid_attach( GTK_GRID( priv->grid ), label, 1+COL_BEGIN_LABEL, row, 1, 1 );

	entry = gtk_entry_new();
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_grid_attach( GTK_GRID( priv->grid ), entry, 1+COL_END, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	label = gtk_label_new( "" );
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	gtk_widget_set_sensitive( label, FALSE );
	my_utils_widget_set_margin_right( label, 4 );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 10 );
	gtk_grid_attach( GTK_GRID( priv->grid ), label, 1+COL_END_LABEL, row, 1, 1 );

	entry = gtk_entry_new();
	my_editable_amount_init_ex( GTK_EDITABLE( entry ), DEFAULT_RATE_DECIMALS );
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_rate_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), 10 );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), 10 );
	gtk_grid_attach( GTK_GRID( priv->grid ), entry, 1+COL_RATE, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	label = gtk_label_new( "" );
	gtk_widget_set_sensitive( label, FALSE );
	gtk_widget_set_hexpand( label, TRUE );
	my_utils_widget_set_margin_right( label, 4 );
	my_utils_widget_set_xalign( label, 0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), 7 );
	gtk_grid_attach( GTK_GRID( priv->grid ), label, 1+COL_RATE_LABEL, row, 1, 1 );
}

static void
set_detail_values( ofaRateProperties *self, guint row )
{
	ofaRatePropertiesPrivate *priv;
	GtkWidget *entry;
	const GDate *d;
	gdouble rate;
	guint idx;

	priv = self->priv;
	idx = row - 1;

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_BEGIN, row );
	d = ofo_rate_get_val_begin( priv->rate, idx );
	my_editable_date_set_date( GTK_EDITABLE( entry ), d );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_END, row );
	d = ofo_rate_get_val_end( priv->rate, idx );
	my_editable_date_set_date( GTK_EDITABLE( entry ), d );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_RATE, row );
	rate = ofo_rate_get_val_rate( priv->rate, idx );
	my_editable_amount_set_amount( GTK_EDITABLE( entry ), rate );
}

/**
 * ofa_rate_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @rate: the #ofoRate to be displayed/updated.
 *
 * Update the properties of a rate.
 */
gboolean
ofa_rate_properties_run( const ofaMainWindow *main_window, ofoRate *rate )
{
	static const gchar *thisfn = "ofa_rate_properties_run";
	ofaRateProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, rate=%p",
			thisfn, ( void * ) main_window, ( void * ) rate );

	self = g_object_new(
					OFA_TYPE_RATE_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->rate = rate;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaRateProperties *self;
	ofaRatePropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkApplication *application;
	ofoDossier *dossier;
	gint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkWidget *label;
	GtkContainer *container;

	self = OFA_RATE_PROPERTIES( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	application = gtk_window_get_application( GTK_WINDOW( main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->is_current = ofo_dossier_is_current( dossier );

	mnemo = ofo_rate_get_mnemo( priv->rate );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new rate" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » rate" ), mnemo );
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

	priv->label = g_strdup( ofo_rate_get_label( priv->rate ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( container, rate );
	my_utils_container_updstamp_init( container, rate );
	my_utils_container_set_editable( container, priv->is_current );

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		priv->ok_btn = my_dialog_set_readonly_buttons( dialog );
	}

	/* set detail rows after general sensitivity */
	priv->grid = my_utils_container_get_child_by_name( container, "p2-grid" );
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));
	my_igridlist_init(
			MY_IGRIDLIST( dialog ), GTK_GRID( priv->grid ), priv->is_current, N_COLUMNS );
	count = ofo_rate_get_val_count( priv->rate );
	for( idx=0 ; idx<count ; ++idx ){
		my_igridlist_add_row( MY_IGRIDLIST( dialog ), GTK_GRID( priv->grid ));
	}

	check_for_enable_dlg( self );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;

	priv = self->priv;

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;

	priv = self->priv;

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
		text = my_editable_amount_get_string( GTK_EDITABLE( entry ));
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
	guint row;
	GtkLabel *label;
	gchar *markup;

	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( widget ), DATA_ROW ));
	label = GTK_LABEL( gtk_grid_get_child_at( GTK_GRID( self->priv->grid ), column, row ));
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
	gboolean ok;

	priv = self->priv;
	ok = is_dialog_validable( self );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
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

	priv = self->priv;

	count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->grid ));
	for( i=1, valids=NULL ; i<=count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_BEGIN, i ));
		dbegin = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_END, i ));
		dend = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_RATE, i ));
		vrate = my_editable_amount_get_amount( GTK_EDITABLE( entry ));

		if( my_date_is_valid( dbegin ) || my_date_is_valid( dend ) || vrate > 0 ){

			validity = g_new0( ofsRateValidity, 1 );
			my_date_set_from_date( &validity->begin, dbegin );
			my_date_set_from_date( &validity->end, dend );
			validity->rate = vrate;
			valids = g_list_prepend( valids, validity );
		}
	}

	ok = ofo_rate_is_valid( priv->mnemo, priv->label, g_list_reverse( valids ));

	g_list_free_full( valids, ( GDestroyNotify ) g_free );

	if( ok ){
		exists = ofo_rate_get_by_mnemo( priv->hub, priv->mnemo );
		ok &= !exists ||
				( !priv->is_new &&
						!g_utf8_collate( priv->mnemo, ofo_rate_get_mnemo( priv->rate )));
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_RATE_PROPERTIES( dialog )));
}

/*
 * either creating a new rate (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;
	guint i, count;
	GtkEntry *entry;
	const GDate *dbegin, *dend;
	gchar *prev_mnemo, *str;
	gdouble rate;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;

	prev_mnemo = g_strdup( ofo_rate_get_mnemo( priv->rate ));

	ofo_rate_set_mnemo( priv->rate, priv->mnemo );
	ofo_rate_set_label( priv->rate, priv->label );
	my_utils_container_notes_get( my_window_get_toplevel( MY_WINDOW( self )), rate );

	ofo_rate_free_all_val( priv->rate );
	count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->grid ));

	for( i=1 ; i<=count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_BEGIN, i ));
		dbegin = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_END, i ));
		dend = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->grid ), 1+COL_RATE, i ));
		rate = my_editable_amount_get_amount( GTK_EDITABLE( entry ));
		str = my_editable_amount_get_string( GTK_EDITABLE( entry ));
		g_debug( "do_update: amount=%.5lf, str=%s", rate, str );
		if( my_date_is_valid( dbegin ) ||
			my_date_is_valid( dend ) ||
			( rate > 0 )){

			ofo_rate_add_val( priv->rate, dbegin, dend, rate );
		}
	}

	if( priv->is_new ){
		priv->updated =
				ofo_rate_insert( priv->rate, priv->hub );
	} else {
		priv->updated =
				ofo_rate_update( priv->rate, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
