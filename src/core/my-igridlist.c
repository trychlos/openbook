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

#include "api/my-igridlist.h"
#include "api/my-utils.h"

/* some data attached to each managed #GtkGrid
 */
typedef struct {

	/* initialization
	 */
	const myIGridList *instance;
	gboolean           is_current;
	guint              columns_count;

	/* runtime
	 */
	guint              rows_count;
}
	sIGridList;

#define IGRIDLIST_DATA                  "igridlist-data"
#define IGRIDLIST_LAST_VERSION          1

#define DATA_COLUMN                     "igridlist-column"
#define DATA_ROW                        "igridlist-row"
#define COL_ADD                         0
#define COL_ROW                         0
#define COL_UP                          (data->columns_count+1)
#define COL_DOWN                        (data->columns_count+2)
#define COL_REMOVE                      (data->columns_count+3)
#define RANG_WIDTH                      3

static guint st_initializations         = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( myIGridListInterface *klass );
static void        interface_base_finalize( myIGridListInterface *klass );
static GtkWidget  *add_button( GtkGrid *grid, const gchar *stock_id, guint column, guint row, guint right_margin, GCallback cb, void *user_data, sIGridList *data );
static void        on_button_clicked( GtkButton *button, GtkGrid *grid );
static void        exchange_rows( GtkGrid *grid, gint row_a, gint row_b, sIGridList *data );
static void        remove_row( GtkGrid *grid, gint row, sIGridList *data );
static void        update_detail_buttons( GtkGrid *grid, sIGridList *data );
static void        signal_row_added( GtkGrid *grid, sIGridList *data );
static void        signal_row_removed( GtkGrid *grid, sIGridList *data );
static void        add_empty_row( GtkGrid *grid, guint row, sIGridList *data );
static sIGridList *get_igridlist_data( GtkGrid *grid );
static void        on_grid_finalized( sIGridList *data, GObject *finalized_grid );

/**
 * my_igridlist_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_igridlist_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_igridlist_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_igridlist_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIGridListInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "myIGridList", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myIGridListInterface *klass )
{
	static const gchar *thisfn = "my_igridlist_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIGridListInterface *klass )
{
	static const gchar *thisfn = "my_igridlist_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_igridlist_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_igridlist_get_interface_last_version( void )
{
	return( IGRIDLIST_LAST_VERSION );
}

/**
 * my_igridlist_get_interface_version:
 * @instance: this #myIGridList instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
my_igridlist_get_interface_version( const myIGridList *instance )
{
	static const gchar *thisfn = "my_igridlist_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IGRIDLIST( instance ), 0 );

	if( MY_IGRIDLIST_GET_INTERFACE( instance )->get_interface_version ){
		return( MY_IGRIDLIST_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: myIGridList instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * my_igridlist_init:
 * @instance: this #myIGridList instance.
 * @grid: the #GtkGrid container.
 * @is_current: whether the currently opened dossier is current.
 * @columns_count: the count of widget columns that the implementation
 *  expects to provide.
 *
 * Initialize the containing @grid, creating the very first Add button.
 */
void
my_igridlist_init( const myIGridList *instance, GtkGrid *grid, gboolean is_current, guint columns_count )
{
	static const gchar *thisfn = "my_igridlist_init";
	sIGridList *data;

	g_debug( "%s: instance=%p, grid=%p", thisfn, ( void * ) instance, ( void * ) grid );

	g_return_if_fail( instance && MY_IS_IGRIDLIST( instance ));
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	data = get_igridlist_data( grid );
	data->instance = instance;
	data->is_current = is_current;
	data->columns_count = columns_count;

	add_button( grid, "gtk-add", COL_ADD, 1, 4, G_CALLBACK( on_button_clicked ), grid, data );
}

static GtkWidget *
add_button( GtkGrid *grid, const gchar *stock_id, guint column, guint row, guint right_margin,
					GCallback cb, void *user_data, sIGridList *data )
{
	GtkWidget *image, *button;

	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	button = gtk_button_new();
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GUINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GUINT_TO_POINTER( row ));
	gtk_widget_set_halign( button, GTK_ALIGN_END );
	my_utils_widget_set_margins( GTK_WIDGET( button ), 0, 0, 0, right_margin );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( button, "clicked", cb, user_data );
	gtk_grid_attach( grid, button, column, row, 1, 1 );
	gtk_widget_set_sensitive( button, data->is_current );

	return( button );
}

static void
on_button_clicked( GtkButton *button, GtkGrid *grid )
{
	static const gchar *thisfn = "my_igridlist_on_button_clicked";
	sIGridList *data;
	guint column, row;

	data = get_igridlist_data( grid );
	column = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));

	g_debug( "%s: button=%p, grid=%p, column=%u, row=%u",
			thisfn, ( void * ) button, ( void * ) grid, column, row );

	if( column == COL_ADD ){
		my_igridlist_add_row( data->instance, grid );

	} else if( column == COL_UP ){
		g_return_if_fail( row > 1 );
		exchange_rows( grid, row, row-1, data );

	} else if( column == COL_DOWN ){
		g_return_if_fail( row < data->rows_count );
		exchange_rows( grid, row, row+1, data );

	} else if( column == COL_REMOVE ){
		remove_row( grid, row, data );

	} else {
		g_warning( "%s: invalid column=%u", thisfn, column );
	}
}

static void
exchange_rows( GtkGrid *grid, gint row_a, gint row_b, sIGridList *data )
{
	gint i;
	GtkWidget *w_a, *w_b;

	/* do not move the row number */
	for( i=1 ; i<data->columns_count+3 ; ++i ){

		w_a = gtk_grid_get_child_at( grid, i, row_a );
		g_object_ref( w_a );
		gtk_container_remove( GTK_CONTAINER( grid ), w_a );

		w_b = gtk_grid_get_child_at( grid, i, row_b );
		g_object_ref( w_b );
		gtk_container_remove( GTK_CONTAINER( grid ), w_b );

		gtk_grid_attach( grid, w_a, i, row_b, 1, 1 );
		g_object_set_data( G_OBJECT( w_a ), DATA_ROW, GUINT_TO_POINTER( row_b ));

		gtk_grid_attach( grid, w_b, i, row_a, 1, 1 );
		g_object_set_data( G_OBJECT( w_b ), DATA_ROW, GUINT_TO_POINTER( row_a ));

		g_object_unref( w_a );
		g_object_unref( w_b );
	}

	update_detail_buttons( grid, data );
}

static void
remove_row( GtkGrid *grid, gint row, sIGridList *data )
{
	gint i, line;
	GtkWidget *widget, *label;
	gchar *str;

	/* first remove the line */
	for( i=0 ; i<=data->columns_count+3 ; ++i ){
		widget = gtk_grid_get_child_at( grid, i, row );
		g_return_if_fail( widget && GTK_IS_WIDGET( widget ));
		gtk_widget_destroy( widget );
	}

	/* then move the following lines one row up */
	for( line=row+1 ; line<=data->rows_count+1 ; ++line ){
		for( i=0 ; i<=data->columns_count+3 ; ++i ){
			widget = gtk_grid_get_child_at( grid, i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( grid ), widget );
				gtk_grid_attach( grid, widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GUINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
		if( line <= data->rows_count ){
			/* update the rang number on each moved line */
			label = gtk_grid_get_child_at( grid, COL_ROW, line-1 );
			g_return_if_fail( label && GTK_IS_LABEL( label ));
			str = g_strdup_printf( "%d", line-1 );
			gtk_label_set_text( GTK_LABEL( label ), str );
			g_free( str );
		}
	}

	/* last update the lines count */
	data->rows_count -= 1;
	signal_row_removed( grid, data );
	gtk_widget_show_all( GTK_WIDGET( grid ));
}

static void
update_detail_buttons( GtkGrid *grid, sIGridList *data )
{
	GtkWidget *up_btn, *down_btn;
	guint i;

	for( i=1 ; i<=data->rows_count ; ++i ){

		up_btn = gtk_grid_get_child_at( grid, COL_UP, i );
		g_return_if_fail( up_btn && GTK_IS_WIDGET( up_btn ));

		down_btn = gtk_grid_get_child_at( grid, COL_DOWN, i );
		g_return_if_fail( down_btn && GTK_IS_WIDGET( down_btn ));

		gtk_widget_set_sensitive( up_btn, data->is_current );
		gtk_widget_set_sensitive( down_btn, data->is_current );

		if( i == 1 ){
			gtk_widget_set_sensitive( up_btn, FALSE );
		}

		if( i == data->rows_count ){
			gtk_widget_set_sensitive( down_btn, FALSE );
		}
	}
}

static void
signal_row_added( GtkGrid *grid, sIGridList *data )
{
	update_detail_buttons( grid, data );
}

static void
signal_row_removed( GtkGrid *grid, sIGridList *data )
{
	update_detail_buttons( grid, data );
}

/**
 * my_igridlist_add_row:
 * @instance: this #myIGridList instance.
 * @grid: the target #GtkGrid.
 *
 * Adds a new empty row at the end of the @grid.
 *
 * Returns: the index of the newly added row, counted from zero.
 */
guint
my_igridlist_add_row( const myIGridList *instance, GtkGrid *grid )
{
	static const gchar *thisfn = "my_igridlist_add_row";
	sIGridList *data;
	guint row;

	g_debug( "%s: instance=%p, grid=%p", thisfn, ( void * ) instance, ( void * ) grid );

	g_return_val_if_fail( instance && MY_IS_IGRIDLIST( instance ), 0 );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), 0 );

	data = get_igridlist_data( grid );
	row = 1+data->rows_count;
	add_empty_row( grid, row, data );
	add_button( grid, "gtk-go-up", COL_UP, row, 0, G_CALLBACK( on_button_clicked ), grid, data );
	add_button( grid, "gtk-go-down", COL_DOWN, row, 0, G_CALLBACK( on_button_clicked ), grid, data );
	add_button( grid, "gtk-remove", COL_REMOVE, row, 0, G_CALLBACK( on_button_clicked ), grid, data );
	add_button( grid, "gtk-add", COL_ADD, row+1, 4, G_CALLBACK( on_button_clicked ), grid, data );

	if( MY_IGRIDLIST_GET_INTERFACE( instance )->set_row ){
		MY_IGRIDLIST_GET_INTERFACE( instance )->set_row( instance, grid, row );

	} else {
		g_info( "%s: myIGridList instance %p does not provide 'set_row()' method",
				thisfn, ( void * ) instance );
	}

	data->rows_count = row;
	signal_row_added( grid, data );
	gtk_widget_show_all( GTK_WIDGET( grid ));

	return( row );
}

static void
add_empty_row( GtkGrid *grid, guint row, sIGridList *data )
{
	gchar *str;
	GtkWidget *label;

	/* remove the Add button */
	gtk_widget_destroy( gtk_grid_get_child_at( grid, COL_ADD, row ));

	/* add the row number */
	label = gtk_label_new( NULL );
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	my_utils_widget_set_margins( label, 0, 0, 0, 4 );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( grid, label, COL_ROW, row, 1, 1 );
	str = g_strdup_printf( "<i>%u</i>", row );
	gtk_label_set_markup( GTK_LABEL( label ), str );
	g_free( str );
}

/**
 * my_igridlist_add_button:
 * @instance: this #myIGridList instance.
 * @grid: the target #GtkGrid.
 * @stock_id: the name of the stock image of the button.
 * @column: the column index in the @grid, counted from zero.
 * @row: the row index in the @grid, counted from zero.
 * @right_margin: the right margin.
 * @cb: the callback function to be called when the the button is clicked.
 * @user_data: the user data to be sent to the signal handler.
 *
 * Returns: the newly added button.
 */
GtkWidget *
my_igridlist_add_button( const myIGridList *instance, GtkGrid *grid,
							const gchar *stock_id, guint column, guint row, guint right_margin,
							GCallback cb, void *user_data )
{
	static const gchar *thisfn = "my_igridlist_add_button";
	sIGridList *data;
	GtkWidget *button;

	g_debug( "%s: instance=%p, grid=%p, stock_id=%s, column=%u, row=%u, right_margin=%u, cb=%p, user_data=%p",
			thisfn, ( void * ) instance, ( void * ) grid, stock_id, column, row, right_margin,
			( void * ) cb, ( void * ) user_data );

	g_return_val_if_fail( instance && MY_IS_IGRIDLIST( instance ), NULL );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), NULL );

	data = get_igridlist_data( grid );
	button = add_button( grid, stock_id, column, row, right_margin, cb, user_data, data );

	return( button );
}

/**
 * my_igridlist_get_rows_count:
 * @instance: this #myIGridList instance.
 * @grid: the target #GtkGrid.
 *
 * Returns: the count of added rows, not counting the header nor the
 * last row with only the Add button.
 */
guint
my_igridlist_get_rows_count( const myIGridList *instance, GtkGrid *grid )
{
	static const gchar *thisfn = "my_igridlist_get_rows_count";
	sIGridList *data;

	g_debug( "%s: instance=%p, grid=%p", thisfn, ( void * ) instance, ( void * ) grid );

	g_return_val_if_fail( instance && MY_IS_IGRIDLIST( instance ), 0 );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), 0 );

	data = get_igridlist_data( grid );

	return( data->rows_count );
}

static sIGridList *
get_igridlist_data( GtkGrid *grid )
{
	sIGridList *data;

	data = ( sIGridList * ) g_object_get_data( G_OBJECT( grid ), IGRIDLIST_DATA );

	if( !data ){
		data = g_new0( sIGridList, 1 );
		g_object_set_data( G_OBJECT( grid ), IGRIDLIST_DATA, data );
		g_object_weak_ref( G_OBJECT( grid ), ( GWeakNotify ) on_grid_finalized, data );
	}

	return( data );
}

static void
on_grid_finalized( sIGridList *data, GObject *finalized_grid )
{
	static const gchar *thisfn = "my_igridlist_on_grid_finalized";

	g_debug( "%s: data=%p, finalized_grid=%p",
			thisfn, ( void * ) data, ( void * ) finalized_grid );

	g_free( data );
}
