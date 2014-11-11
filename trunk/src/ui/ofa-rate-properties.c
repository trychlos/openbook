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
#include <stdarg.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-rate.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-amount.h"
#include "ui/my-editable-date.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-rate-properties.h"

/* private instance data
 */
struct _ofaRatePropertiesPrivate {

	/* internals
	 */
	ofoRate       *rate;
	gboolean       is_new;
	gboolean       updated;
	gint           count;				/* count of rows added to the grid */

	/* UI
	 */
	GtkGrid       *grid;				/* the grid which handles the validity rows */

	/* data
	 */
	gchar         *mnemo;
	gchar         *label;
};

#define DATA_COLUMN                  "ofa-data-column"
#define DATA_ROW                     "ofa-data-row"
#define DEFAULT_RATE_DECIMALS        3

/* the columns in the dynamic grid
 */
enum {
	COL_ADD = 0,
	COL_BEGIN,
	COL_END,
	COL_RATE,
	COL_MESSAGE,
	COL_REMOVE,
	N_COLUMNS
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-rate-properties.ui";
static const gchar  *st_ui_id        = "RatePropertiesDlg";

G_DEFINE_TYPE( ofaRateProperties, ofa_rate_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      insert_new_row( ofaRateProperties *self, gint idx );
static void      add_empty_row( ofaRateProperties *self );
static void      add_button( ofaRateProperties *self, const gchar *stock_id, gint column, gint row );
static void      on_mnemo_changed( GtkEntry *entry, ofaRateProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaRateProperties *self );
static gboolean  on_date_focus_in( GtkWidget *entry, GdkEvent *event, ofaRateProperties *self );
static gboolean  on_focus_out( GtkWidget *entry, GdkEvent *event, ofaRateProperties *self );
static void      on_date_changed( GtkEntry *entry, ofaRateProperties *self );
static void      on_rate_changed( GtkEntry *entry, ofaRateProperties *self );
static void      set_grid_line_comment( ofaRateProperties *self, GtkWidget *widget, const gchar *comment );
static void      on_button_clicked( GtkButton *button, ofaRateProperties *self );
static void      remove_row( ofaRateProperties *self, gint row );
static void      check_for_enable_dlg( ofaRateProperties *self );
static gboolean  is_dialog_validable( ofaRateProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaRateProperties *self );

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
	self->priv->count = 0;
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

/**
 * ofa_rate_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of a rate
 */
gboolean
ofa_rate_properties_run( ofaMainWindow *main_window, ofoRate *rate )
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
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
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
	gint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkContainer *container;

	self = OFA_RATE_PROPERTIES( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	mnemo = ofo_rate_get_mnemo( priv->rate );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new rate" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » rate" ), mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->mnemo = g_strdup( mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-mnemo" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );

	priv->label = g_strdup( ofo_rate_get_label( priv->rate ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

	priv->grid = GTK_GRID( my_utils_container_get_child_by_name( container, "p2-grid" ));
	add_button( self, "gtk-add", COL_ADD, 1 );

	count = ofo_rate_get_val_count( priv->rate );
	for( idx=0 ; idx<count ; ++idx ){
		insert_new_row( self, idx );
	}

	my_utils_init_notes_ex( container, rate );
	my_utils_init_upd_user_stamp_ex( container, rate );

	check_for_enable_dlg( self );
}

static void
insert_new_row( ofaRateProperties *self, gint idx )
{
	ofaRatePropertiesPrivate *priv;
	GtkEntry *entry;
	const GDate *d;
	gdouble rate;
	gint row;

	priv = self->priv;
	add_empty_row( self );
	row = self->priv->count;

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_BEGIN, row ));
	d = ofo_rate_get_val_begin( priv->rate, idx );
	my_editable_date_set_date( GTK_EDITABLE( entry ), d );

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_END, row ));
	d = ofo_rate_get_val_end( priv->rate, idx );
	my_editable_date_set_date( GTK_EDITABLE( entry ), d );

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_RATE, row ));
	rate = ofo_rate_get_val_rate( priv->rate, idx );
	my_editable_amount_set_amount( GTK_EDITABLE( entry ), rate );
}

/*
 * idx is counted from zero
 */
static void
add_empty_row( ofaRateProperties *self )
{
	ofaRatePropertiesPrivate *priv;
	GtkEntry *entry;
	GtkLabel *label;
	gint row;

	priv = self->priv;
	row = self->priv->count + 1;

	gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, COL_ADD, row ));

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_date_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( entry ), COL_BEGIN, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_date_init( GTK_EDITABLE( entry ));
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_date_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( entry ), COL_END, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	my_editable_amount_init_ex( GTK_EDITABLE( entry ), DEFAULT_RATE_DECIMALS );
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_rate_changed ), self );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( entry ), COL_RATE, row, 1, 1 );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_hexpand( GTK_WIDGET( label ), TRUE );
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( label ), COL_MESSAGE, row, 1, 1 );

	add_button( self, "gtk-remove", COL_REMOVE, row );
	add_button( self, "gtk-add", COL_ADD, row+1 );

	self->priv->count = row;
	gtk_widget_show_all( GTK_WIDGET( priv->grid ));
}

static void
add_button( ofaRateProperties *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->priv->grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaRateProperties *self )
{
	g_free( self->priv->mnemo );
	self->priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaRateProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static gboolean
on_date_focus_in( GtkWidget *entry, GdkEvent *event, ofaRateProperties *self )
{
	on_date_changed( GTK_ENTRY( entry ), self );
	return( FALSE );
}

static gboolean
on_focus_out( GtkWidget *entry, GdkEvent *event, ofaRateProperties *self )
{
	set_grid_line_comment( self, entry, "" );
	return( FALSE );
}

static void
on_date_changed( GtkEntry *entry, ofaRateProperties *self )
{
	const gchar *content;
	const GDate *date;
	gboolean valid;
	gchar *str;

	g_debug( "ofa_rate_properties_on_date_changed: entry=%p", ( void * ) entry );

	content = gtk_entry_get_text( entry );
	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );

	} else {
		date = my_editable_date_get_date( GTK_EDITABLE( entry ), &valid );
		if( valid ){
			str = my_date_to_str( date, MY_DATE_DMMM );
		} else {
			str = g_strdup( _( "invalid date" ));
		}
	}

	set_grid_line_comment( self, GTK_WIDGET( entry ), str );
	g_free( str );

	check_for_enable_dlg( self );
}

static void
on_rate_changed( GtkEntry *entry, ofaRateProperties *self )
{
	const gchar *content;
	gchar *text, *str;

	g_debug( "on_rate_changed" );
	content = gtk_entry_get_text( entry );

	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );

	} else {
		text = my_editable_amount_get_string( GTK_EDITABLE( entry ));
		str = g_strdup_printf( "%s %%", text );
		g_free( text );
	}

	set_grid_line_comment( self, GTK_WIDGET( entry ), str );
	g_free( str );

	check_for_enable_dlg( self );
}

static void
set_grid_line_comment( ofaRateProperties *self, GtkWidget *widget, const gchar *comment )
{
	gint row;
	GtkLabel *label;
	gchar *markup;

	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( widget ), DATA_ROW ));
	label = GTK_LABEL( gtk_grid_get_child_at( self->priv->grid, COL_MESSAGE, row ));
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", comment );
	gtk_label_set_markup( label, markup );
	g_free( markup );
}

static void
on_button_clicked( GtkButton *button, ofaRateProperties *self )
{
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	switch( column ){
		case COL_ADD:
			add_empty_row( self );
			break;
		case COL_REMOVE:
			remove_row( self, row );
			break;
	}
}

static void
remove_row( ofaRateProperties *self, gint row )
{
	ofaRatePropertiesPrivate *priv;
	gint i, line;
	GtkWidget *widget;

	priv = self->priv;

	/* first remove the line
	 * note that there is no 'add' button in a used line */
	for( i=0 ; i<N_COLUMNS ; ++i ){
		if( i != COL_ADD ){
			gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, i, row ));
		}
	}

	/* then move the follow lines one row up */
	for( line=row+1 ; line<=self->priv->count+1 ; ++line ){
		for( i=0 ; i<N_COLUMNS ; ++i ){
			widget = gtk_grid_get_child_at( priv->grid, i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( priv->grid ), widget );
				gtk_grid_attach( priv->grid, widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
	}

	gtk_widget_show_all( GTK_WIDGET( priv->grid ));

	/* last update the lines count */
	self->priv->count -= 1;
}

/*
 * are we able to validate this rate, and all its validities
 */
static void
check_for_enable_dlg( ofaRateProperties *self )
{
	GtkWidget *button;
	gboolean ok;

	ok = is_dialog_validable( self );

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( button, ok );
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

	priv = self->priv;

	for( i=1, valids=NULL ; i<=priv->count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_BEGIN, i ));
		dbegin = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_END, i ));
		dend = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_RATE, i ));
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
		exists = ofo_rate_get_by_mnemo(
				MY_WINDOW( self )->prot->dossier,
				self->priv->mnemo );
		ok &= !exists ||
				( !priv->is_new &&
						!g_utf8_collate( self->priv->mnemo, ofo_rate_get_mnemo( self->priv->rate )));
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
	gint i;
	GtkEntry *entry;
	const GDate *dbegin, *dend;
	gchar *prev_mnemo;
	gdouble rate;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;

	prev_mnemo = g_strdup( ofo_rate_get_mnemo( priv->rate ));

	ofo_rate_set_mnemo( priv->rate, priv->mnemo );
	ofo_rate_set_label( priv->rate, priv->label );
	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), rate );

	ofo_rate_free_all_val( priv->rate );

	for( i=1 ; i<=priv->count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_BEGIN, i ));
		dbegin = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_END, i ));
		dend = my_editable_date_get_date( GTK_EDITABLE( entry ), NULL );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_RATE, i ));
		rate = my_editable_amount_get_amount( GTK_EDITABLE( entry ));
		if( my_date_is_valid( dbegin ) ||
			my_date_is_valid( dend ) ||
			( rate > 0 )){

			ofo_rate_add_val( priv->rate, dbegin, dend, rate );
		}
	}

	if( priv->is_new ){
		priv->updated =
				ofo_rate_insert( priv->rate );
	} else {
		priv->updated =
				ofo_rate_update( priv->rate, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
