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

#include "core/my-utils.h"
#include "ui/my-window-prot.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-taux-properties.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-taux.h"

/* private instance data
 */
struct _ofaTauxPropertiesPrivate {

	/* internals
	 */
	ofoTaux       *taux;
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

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-taux-properties.ui";
static const gchar  *st_ui_id        = "TauxPropertiesDlg";

G_DEFINE_TYPE( ofaTauxProperties, ofa_taux_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      insert_new_row( ofaTauxProperties *self, gint idx );
static void      add_empty_row( ofaTauxProperties *self );
static void      add_button( ofaTauxProperties *self, const gchar *stock_id, gint column, gint row );
static void      on_mnemo_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaTauxProperties *self );
static gboolean  on_date_focus_in( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self );
static gboolean  on_focus_out( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self );
static void      on_date_changed( GtkEntry *entry, ofaTauxProperties *self );
static gboolean  on_rate_focus_in( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self );
static void      on_rate_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      set_grid_line_comment( ofaTauxProperties *self, GtkWidget *widget, const gchar *comment );
static void      on_button_clicked( GtkButton *button, ofaTauxProperties *self );
static void      remove_row( ofaTauxProperties *self, gint row );
static void      check_for_enable_dlg( ofaTauxProperties *self );
static gboolean  is_dialog_validable( ofaTauxProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaTauxProperties *self );

static void
taux_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_taux_properties_finalize";
	ofaTauxPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( instance ));

	priv = OFA_TAUX_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_taux_properties_parent_class )->finalize( instance );
}

static void
taux_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_taux_properties_parent_class )->dispose( instance );
}

static void
ofa_taux_properties_init( ofaTauxProperties *self )
{
	static const gchar *thisfn = "ofa_taux_properties_init";

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaTauxPropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
	self->private->count = 0;
}

static void
ofa_taux_properties_class_init( ofaTauxPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_taux_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = taux_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = taux_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_taux_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an taux
 */
gboolean
ofa_taux_properties_run( ofaMainWindow *main_window, ofoTaux *taux )
{
	static const gchar *thisfn = "ofa_taux_properties_run";
	ofaTauxProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, taux=%p",
			thisfn, ( void * ) main_window, ( void * ) taux );

	self = g_object_new(
					OFA_TYPE_TAUX_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->private->taux = taux;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaTauxProperties *self;
	ofaTauxPropertiesPrivate *priv;
	gint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkContainer *container;

	self = OFA_TAUX_PROPERTIES( dialog );
	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	mnemo = ofo_taux_get_mnemo( priv->taux );
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

	priv->label = g_strdup( ofo_taux_get_label( priv->taux ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

	priv->grid = GTK_GRID( my_utils_container_get_child_by_name( container, "p2-grid" ));
	add_button( self, GTK_STOCK_ADD, COL_ADD, 1 );

	count = ofo_taux_get_val_count( priv->taux );
	for( idx=0 ; idx<count ; ++idx ){
		insert_new_row( self, idx );
	}

	my_utils_init_notes_ex( container, taux );
	my_utils_init_maj_user_stamp_ex( container, taux );

	check_for_enable_dlg( self );
}

static void
insert_new_row( ofaTauxProperties *self, gint idx )
{
	ofaTauxPropertiesPrivate *priv;
	GtkEntry *entry;
	const GDate *d;
	gdouble rate;
	gchar *str;
	gint row;

	priv = self->private;
	add_empty_row( self );
	row = self->private->count;

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_BEGIN, row ));
	d = ofo_taux_get_val_begin( priv->taux, idx );
	if( d && g_date_valid( d )){
		str = my_utils_display_from_date( d, MY_UTILS_DATE_DDMM );
	} else {
		str = g_strdup( "" );
	}
	gtk_entry_set_text( entry, str );
	g_free( str );

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_END, row ));
	d = ofo_taux_get_val_end( priv->taux, idx );
	if( d && g_date_valid( d )){
		str = my_utils_display_from_date( d, MY_UTILS_DATE_DDMM );
	} else {
		str = g_strdup( "" );
	}
	gtk_entry_set_text( entry, str );
	g_free( str );

	entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_RATE, row ));
	rate = ofo_taux_get_val_rate( priv->taux, idx );
	str = g_strdup_printf( "%.2lf", rate );
	gtk_entry_set_text( entry, str );
	g_free( str );
}

/*
 * idx is counted from zero
 */
static void
add_empty_row( ofaTauxProperties *self )
{
	ofaTauxPropertiesPrivate *priv;
	GtkEntry *entry;
	GtkLabel *label;
	gint row;

	priv = self->private;
	row = self->private->count + 1;

	gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, COL_ADD, row ));

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_date_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_entry_set_max_length( entry, 10 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( entry ), COL_BEGIN, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_date_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_entry_set_max_length( entry, 10 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( entry ), COL_END, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_rate_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_rate_changed ), self );
	gtk_entry_set_max_length( entry, 10 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( entry ), COL_RATE, row, 1, 1 );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_hexpand( GTK_WIDGET( label ), TRUE );
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_grid_attach( priv->grid, GTK_WIDGET( label ), COL_MESSAGE, row, 1, 1 );

	add_button( self, GTK_STOCK_REMOVE, COL_REMOVE, row );
	add_button( self, GTK_STOCK_ADD, COL_ADD, row+1 );

	self->private->count = row;
	gtk_widget_show_all( GTK_WIDGET( priv->grid ));
}

static void
add_button( ofaTauxProperties *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_stock( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->private->grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static gboolean
on_date_focus_in( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self )
{
	on_date_changed( GTK_ENTRY( entry ), self );
	return( FALSE );
}

static gboolean
on_focus_out( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self )
{
	set_grid_line_comment( self, entry, "" );
	return( FALSE );
}

static void
on_date_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	const gchar *content;
	GDate date;
	gchar *str;

	content = gtk_entry_get_text( entry );
	g_date_set_parse( &date, content );

	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );
	} else if( g_date_valid( &date )){
		str = my_utils_display_from_date( &date, MY_UTILS_DATE_DMMM );
	} else {
		str = g_strdup( _( "invalid" ));
	}
	set_grid_line_comment( self, GTK_WIDGET( entry ), str );
	g_free( str );

	check_for_enable_dlg( self );
}

static gboolean
on_rate_focus_in( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self )
{
	on_rate_changed( GTK_ENTRY( entry ), self );
	return( FALSE );
}

static void
on_rate_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	const gchar *content;
	gchar *str;
	gdouble value;

	content = gtk_entry_get_text( entry );
	value = g_ascii_strtod( content, NULL );

	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );
	} else {
		str = g_strdup_printf( "%.3lf", value );
	}
	set_grid_line_comment( self, GTK_WIDGET( entry ), str );
	g_free( str );

	check_for_enable_dlg( self );
}

static void
set_grid_line_comment( ofaTauxProperties *self, GtkWidget *widget, const gchar *comment )
{
	gint row;
	GtkLabel *label;
	gchar *markup;

	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( widget ), DATA_ROW ));
	label = GTK_LABEL( gtk_grid_get_child_at( self->private->grid, COL_MESSAGE, row ));
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", comment );
	gtk_label_set_markup( label, markup );
	g_free( markup );
}

static void
on_button_clicked( GtkButton *button, ofaTauxProperties *self )
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
remove_row( ofaTauxProperties *self, gint row )
{
	ofaTauxPropertiesPrivate *priv;
	gint i, line;
	GtkWidget *widget;

	priv = self->private;

	/* first remove the line
	 * note that there is no 'add' button in a used line */
	for( i=0 ; i<N_COLUMNS ; ++i ){
		if( i != COL_ADD ){
			gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, i, row ));
		}
	}

	/* then move the follow lines one row up */
	for( line=row+1 ; line<=self->private->count+1 ; ++line ){
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
	self->private->count -= 1;
}

/*
 * are we able to validate this rate, and all its validities
 */
static void
check_for_enable_dlg( ofaTauxProperties *self )
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
is_dialog_validable( ofaTauxProperties *self )
{
	ofaTauxPropertiesPrivate *priv;
	GList *valids;
	sTauxVData *vdata;
	gint i;
	GtkEntry *entry;
	const gchar *sbegin, *send, *srate;
	gboolean ok;
	ofoTaux *exists;

	priv = self->private;

	for( i=1, valids=NULL ; i<=priv->count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_BEGIN, i ));
		sbegin = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_END, i ));
		send = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_RATE, i ));
		srate = gtk_entry_get_text( entry );
		if(( sbegin && g_utf8_strlen( sbegin, -1 )) ||
			( send && g_utf8_strlen( send, -1 )) ||
			( srate && g_utf8_strlen( srate, -1 ))){

			vdata = g_new0( sTauxVData, 1 );
			g_date_set_parse( &vdata->begin, sbegin );
			g_date_set_parse( &vdata->end, send );
			vdata->rate = g_ascii_strtod( srate, NULL );
			valids = g_list_prepend( valids, vdata );
		}
	}

	ok = ofo_taux_is_valid( priv->mnemo, priv->label, g_list_reverse( valids ));

	g_list_free_full( valids, ( GDestroyNotify ) g_free );

	if( ok ){
		exists = ofo_taux_get_by_mnemo(
				MY_WINDOW( self )->protected->dossier,
				self->private->mnemo );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( self->private->mnemo, ofo_taux_get_mnemo( self->private->taux )));
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_TAUX_PROPERTIES( dialog )));
}

/*
 * either creating a new taux (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaTauxProperties *self )
{
	ofaTauxPropertiesPrivate *priv;
	gint i;
	GtkEntry *entry;
	const gchar *sbegin, *send, *srate;
	gchar *prev_mnemo;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->private;

	prev_mnemo = g_strdup( ofo_taux_get_mnemo( priv->taux ));

	ofo_taux_set_mnemo( priv->taux, priv->mnemo );
	ofo_taux_set_label( priv->taux, priv->label );
	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), taux );

	ofo_taux_free_val_all( priv->taux );

	for( i=1 ; i<=priv->count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_BEGIN, i ));
		sbegin = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_END, i ));
		send = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( priv->grid, COL_RATE, i ));
		srate = gtk_entry_get_text( entry );
		if(( sbegin && g_utf8_strlen( sbegin, -1 )) ||
			( send && g_utf8_strlen( send, -1 )) ||
			( srate && g_utf8_strlen( srate, -1 ))){

			ofo_taux_add_val( priv->taux, sbegin, send, srate );
		}
	}

	if( priv->is_new ){
		priv->updated =
				ofo_taux_insert( priv->taux );
	} else {
		priv->updated =
				ofo_taux_update( priv->taux, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
