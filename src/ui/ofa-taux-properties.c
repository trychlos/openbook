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

#include "ui/my-utils.h"
#include "ui/ofo-base.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-taux-properties.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-taux.h"

/* private instance data
 */
struct _ofaTauxPropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow *main_window;
	GtkDialog     *dialog;
	ofoTaux       *taux;
	gboolean       updated;
	gint           count;				/* count of rows added to the grid */

	/* data
	 */
	gint           id;
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

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaTauxPropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaTauxProperties *self, ofaMainWindow *main, ofoTaux *taux );
static gboolean  ok_to_terminate( ofaTauxProperties *self, gint code );
static void      insert_new_row( ofaTauxProperties *self, gint idx );
static void      add_empty_row( ofaTauxProperties *self, GtkGrid *grid );
static void      add_button( ofaTauxProperties *self, GtkGrid *grid, const gchar *stock_id, gint column, gint row );
static void      on_mnemo_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaTauxProperties *self );
static gboolean  on_date_focus_in( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self );
static gboolean  on_focus_out( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self );
static void      on_date_changed( GtkEntry *entry, ofaTauxProperties *self );
static gboolean  on_rate_focus_in( GtkWidget *entry, GdkEvent *event, ofaTauxProperties *self );
static void      on_rate_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      set_grid_line_comment( ofaTauxProperties *self, GtkWidget *widget, const gchar *comment );
static void      on_button_clicked( GtkButton *button, ofaTauxProperties *self );
static void      remove_row( ofaTauxProperties *self, GtkGrid *grid, gint row );
static void      check_for_enable_dlg( ofaTauxProperties *self );
static gboolean  do_update( ofaTauxProperties *self );

GType
ofa_taux_properties_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_taux_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaTauxPropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaTauxProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaTauxProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaTauxPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_taux_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_taux_properties_instance_init";
	ofaTauxProperties *self;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_TAUX_PROPERTIES( instance );

	self->private = g_new0( ofaTauxPropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;

	self->private->id = OFO_BASE_UNSET_ID;
	self->private->count = 0;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_taux_properties_instance_dispose";
	ofaTauxPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( window ));

	priv = ( OFA_TAUX_PROPERTIES( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
		g_free( priv->label );

		gtk_widget_destroy( GTK_WIDGET( priv->dialog ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( window );
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_taux_properties_instance_finalize";
	ofaTauxProperties *self;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_TAUX_PROPERTIES( window );

	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( window );
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
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, taux=%p",
			thisfn, ( void * ) main_window, ( void * ) taux );

	self = g_object_new( OFA_TYPE_TAUX_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, taux );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
do_initialize_dialog( ofaTauxProperties *self, ofaMainWindow *main, ofoTaux *taux )
{
	static const gchar *thisfn = "ofa_taux_properties_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaTauxPropertiesPrivate *priv;
	gint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkGrid *grid;

	priv = self->private;
	priv->main_window = main;
	priv->taux = taux;

	/* create the GtkDialog */
	error = NULL;
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
		priv->dialog = GTK_DIALOG( gtk_builder_get_object( builder, st_ui_id ));
		if( !priv->dialog ){
			g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}
	g_object_unref( builder );

	/* initialize the newly created dialog */
	if( priv->dialog ){

		/*gtk_window_set_transient_for( GTK_WINDOW( priv->dialog ), GTK_WINDOW( main ));*/

		mnemo = ofo_taux_get_mnemo( taux );
		if( !mnemo ){
			title = g_strdup( _( "Defining a new rate" ));
		} else {
			title = g_strdup_printf( _( "Updating « %s » rate" ), mnemo );
			self->private->id = ofo_taux_get_id( taux );
		}
		gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );

		priv->mnemo = g_strdup( mnemo );
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-mnemo" ));
		if( priv->mnemo ){
			gtk_entry_set_text( entry, priv->mnemo );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );

		priv->label = g_strdup( ofo_taux_get_label( taux ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
		if( priv->label ){
			gtk_entry_set_text( entry, priv->label );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

		count = ofo_taux_get_val_count( priv->taux );
		for( idx=0 ; idx<count ; ++idx ){
			insert_new_row( self, idx );
		}
		if( !count ){
			grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-grid" ));
			add_button( self, grid, GTK_STOCK_ADD, COL_ADD, 1 );
		}

		my_utils_init_notes_ex( taux );

		if( mnemo ){
			my_utils_init_maj_user_stamp_ex( taux );
		}
	}

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaTauxProperties *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_NONE:
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_CANCEL:
			quit = TRUE;
			break;

		case GTK_RESPONSE_OK:
			quit = do_update( self );
			break;
	}

	return( quit );
}

static void
insert_new_row( ofaTauxProperties *self, gint idx )
{
	GtkGrid *grid;
	GtkEntry *entry;
	const GDate *d;
	gdouble rate;
	gchar *str;
	gint row;

	grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-grid" ));
	add_empty_row( self, grid );
	row = self->private->count;

	entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_BEGIN, row ));
	d = ofo_taux_get_val_begin( self->private->taux, idx );
	if( d && g_date_valid( d )){
		str = my_utils_display_from_date( d, MY_UTILS_DATE_DDMM );
	} else {
		str = g_strdup( "" );
	}
	gtk_entry_set_text( entry, str );
	g_free( str );

	entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_END, row ));
	d = ofo_taux_get_val_end( self->private->taux, idx );
	if( d && g_date_valid( d )){
		str = my_utils_display_from_date( d, MY_UTILS_DATE_DDMM );
	} else {
		str = g_strdup( "" );
	}
	gtk_entry_set_text( entry, str );
	g_free( str );

	entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_RATE, row ));
	rate = ofo_taux_get_val_rate( self->private->taux, idx );
	str = g_strdup_printf( "%.2lf", rate );
	gtk_entry_set_text( entry, str );
	g_free( str );
}

/*
 * idx is counted from zero
 */
static void
add_empty_row( ofaTauxProperties *self, GtkGrid *grid )
{
	GtkEntry *entry;
	GtkLabel *label;
	gint row;

	row = self->private->count + 1;

	gtk_widget_destroy( gtk_grid_get_child_at( grid, COL_ADD, row ));

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_date_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_entry_set_max_length( entry, 10 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( grid, GTK_WIDGET( entry ), COL_BEGIN, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_date_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), self );
	gtk_entry_set_max_length( entry, 10 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( grid, GTK_WIDGET( entry ), COL_END, row, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GINT_TO_POINTER( row ));
	g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_rate_focus_in ), self );
	g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_focus_out ), self );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_rate_changed ), self );
	gtk_entry_set_max_length( entry, 10 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( grid, GTK_WIDGET( entry ), COL_RATE, row, 1, 1 );

	label = GTK_LABEL( gtk_label_new( "" ));
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	gtk_widget_set_hexpand( GTK_WIDGET( label ), TRUE );
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), COL_MESSAGE, row, 1, 1 );

	add_button( self, grid, GTK_STOCK_REMOVE, COL_REMOVE, row );
	add_button( self, grid, GTK_STOCK_ADD, COL_ADD, row+1 );

	self->private->count = row;
	gtk_widget_show_all( GTK_WIDGET( grid ));
}

static void
add_button( ofaTauxProperties *self, GtkGrid *grid, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	image = gtk_image_new_from_stock( stock_id, GTK_ICON_SIZE_BUTTON );
	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( grid, GTK_WIDGET( button ), column, row, 1, 1 );
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
	GtkGrid *grid;
	gint row;
	GtkLabel *label;
	gchar *markup;

	grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-grid" ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( widget ), DATA_ROW ));
	label = GTK_LABEL( gtk_grid_get_child_at( grid, COL_MESSAGE, row ));
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", comment );
	gtk_label_set_markup( label, markup );
	g_free( markup );
}

static void
on_button_clicked( GtkButton *button, ofaTauxProperties *self )
{
	GtkGrid *grid;
	gint column, row;

	grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-grid" ));
	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	switch( column ){
		case COL_ADD:
			add_empty_row( self, grid );
			break;
		case COL_REMOVE:
			remove_row( self, grid, row );
			break;
	}
}

static void
remove_row( ofaTauxProperties *self, GtkGrid *grid, gint row )
{
	gint i, line;
	GtkWidget *widget;

	/* first remove the line
	 * note that there is no 'add' button in a used line */
	for( i=0 ; i<N_COLUMNS ; ++i ){
		if( i != COL_ADD ){
			gtk_widget_destroy( gtk_grid_get_child_at( grid, i, row ));
		}
	}

	/* then move the follow lines one row up */
	for( line=row+1 ; line<=self->private->count+1 ; ++line ){
		for( i=0 ; i<N_COLUMNS ; ++i ){
			widget = gtk_grid_get_child_at( grid, i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( grid ), widget );
				gtk_grid_attach( grid, widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
	}

	gtk_widget_show_all( GTK_WIDGET( grid ));

	/* last update the lines count */
	self->private->count -= 1;
}

/*
 * are we able to validate this rate, and all its validities
 */
static void
check_for_enable_dlg( ofaTauxProperties *self )
{
	ofaTauxPropertiesPrivate *priv;
	GtkWidget *button;
	GList *valids;
	sTauxVData *vdata;
	gint i;
	GtkGrid *grid;
	GtkEntry *entry;
	const gchar *sbegin, *send, *srate;
	gboolean ok;
	ofoTaux *exists;

	priv = self->private;
	grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-grid" ));

	for( i=1, valids=NULL ; i<=priv->count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_BEGIN, i ));
		sbegin = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_END, i ));
		send = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_RATE, i ));
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
				ofa_main_window_get_dossier( self->private->main_window ),
				self->private->mnemo );
		ok &= !exists ||
				ofo_taux_get_id( exists ) == ofo_taux_get_id( self->private->taux );
	}

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button, ok );
}

/*
 * either creating a new taux (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaTauxProperties *self )
{
	ofoDossier *dossier;
	ofoTaux *exists;
	gint i;
	GtkGrid *grid;
	GtkEntry *entry;
	const gchar *sbegin, *send, *srate;

	/* - we are defining a new mnemo (either by creating a new record
	 *   or by modifying an existing record to a new mnemo)
	 *   then all is fine
	 * - we are defining a mnemo which already exists
	 *   so have to check that this new validity period is consistant
	 *   with he periods already defined by the existing mnemo
	 *
	 * As a conclusion, the only case of an error may occur is when
	 * there is an already existing mnemo which covers the desired
	 * validity period
	 *
	 */
	dossier = ofa_main_window_get_dossier( self->private->main_window );
	exists = ofo_taux_get_by_mnemo( dossier, self->private->mnemo );
	g_return_val_if_fail( !exists  ||
			ofo_taux_get_id( exists ) == ofo_taux_get_id( self->private->taux ), FALSE );

	ofo_taux_set_mnemo( self->private->taux, self->private->mnemo );
	ofo_taux_set_label( self->private->taux, self->private->label );

	my_utils_getback_notes_ex( taux );

	grid = GTK_GRID( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-grid" ));
	ofo_taux_free_val_all( self->private->taux );

	for( i=1 ; i<=self->private->count ; ++i ){
		entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_BEGIN, i ));
		sbegin = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_END, i ));
		send = gtk_entry_get_text( entry );
		entry = GTK_ENTRY( gtk_grid_get_child_at( grid, COL_RATE, i ));
		srate = gtk_entry_get_text( entry );
		if(( sbegin && g_utf8_strlen( sbegin, -1 )) ||
			( send && g_utf8_strlen( send, -1 )) ||
			( srate && g_utf8_strlen( srate, -1 ))){

			ofo_taux_add_val( self->private->taux, sbegin, send, srate );
		}
	}

	if( self->private->id == -1 ){
		self->private->updated =
				ofo_taux_insert( self->private->taux, dossier );
	} else {
		self->private->updated =
				ofo_taux_update( self->private->taux, dossier );
	}

	return( self->private->updated );
}
