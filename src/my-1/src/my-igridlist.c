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

#include "my/my-igridlist.h"
#include "my/my-utils.h"

/* some data attached to each managed #GtkGrid
 */
typedef struct {

	/* initialization
	 */
	const myIGridList *instance;
	GtkGrid           *grid;
	gboolean           has_header;
	gboolean           writable;
	guint              columns_count;

	/* runtime
	 */
	guint              first_row;		/* is 1 if row zero is used by the headers */
	guint              rows_count;		/* count of added detail rows */
}
	sIGridList;

#define IGRIDLIST_LAST_VERSION            1
#define IGRIDLIST_DATA                  "igridlist-data"
#define IGRIDLIST_COLUMN                "igridlist-column"
#define IGRIDLIST_ROW                   "igridlist-row"

#define COL_ADD                           0
#define COL_ROW                           0
#define COL_UP                          (sdata->columns_count+1)
#define COL_DOWN                        (sdata->columns_count+2)
#define COL_REMOVE                      (sdata->columns_count+3)
#define RANG_WIDTH                        3

static guint st_initializations         = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( myIGridListInterface *klass );
static void        interface_base_finalize( myIGridListInterface *klass );
static GtkWidget  *add_button( sIGridList *sdata, const gchar *stock_id, guint column, guint row, guint right_margin, GCallback cb );
static void        on_button_clicked( GtkButton *button, sIGridList *sdata );
static void        exchange_rows( sIGridList *sdata, gint row_a, gint row_b );
static void        remove_row( sIGridList *sdata, gint row );
static void        remove_row_widget_remove( sIGridList *sdata, guint column, guint row );
static void        remove_row_widget_up( sIGridList *sdata, guint column, guint rowsrc );
static void        update_detail_buttons( sIGridList *sdata );
static void        signal_row_added( sIGridList *sdata );
static void        signal_row_removed( sIGridList *sdata );
static void        add_empty_row( sIGridList *sdata, guint row );
static void        set_widget( sIGridList *sdata, GtkWidget *widget, guint column, guint row, guint width, guint height );
static sIGridList *get_igridlist_data( const myIGridList *instance, GtkGrid *grid );
static void        on_grid_finalized( sIGridList *sdata, GObject *finalized_grid );

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
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
my_igridlist_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IGRIDLIST );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIGridListInterface * ) iface )->get_interface_version ){
		version = (( myIGridListInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIGridList::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_igridlist_init:
 * @instance: this #myIGridList instance.
 * @grid: the #GtkGrid container.
 * @has_header: whether row zero is used by the headers.
 * @writable: whether the data are updatable by the user.
 * @columns_count: the count of widget columns that the implementation
 *  expects to provide by itself.
 *
 * Initialize the containing @grid, creating the very first Add button.
 */
void
my_igridlist_init( const myIGridList *instance, GtkGrid *grid, gboolean has_header, gboolean writable, guint columns_count )
{
	static const gchar *thisfn = "my_igridlist_init";
	sIGridList *sdata;

	g_debug( "%s: instance=%p, grid=%p, has_header=%s, writable=%s, columns_count=%u",
			thisfn, ( void * ) instance, ( void * ) grid, has_header ? "True":"False",
			writable ? "True":"False", columns_count );

	g_return_if_fail( instance && MY_IS_IGRIDLIST( instance ));
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	sdata = get_igridlist_data( instance, grid );
	sdata->has_header = has_header;
	sdata->writable = writable;
	sdata->columns_count = columns_count;
	sdata->first_row = has_header ? 1 : 0;
	sdata->rows_count = 0;

	add_button( sdata, "gtk-add", COL_ADD, sdata->first_row+sdata->rows_count, 4, G_CALLBACK( on_button_clicked ));
}

/*
 * @column: column index, counted from zero
 * @row: row index, counted from zero
 */
static GtkWidget *
add_button( sIGridList *sdata, const gchar *stock_id, guint column, guint row, guint right_margin, GCallback cb )
{
	GtkWidget *image, *button;

	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	button = gtk_button_new();
	gtk_widget_set_halign( button, GTK_ALIGN_END );
	my_utils_widget_set_margins( GTK_WIDGET( button ), 0, 0, 0, right_margin );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( button, "clicked", cb, sdata );

	set_widget( sdata, button, column, row, 1, 1 );

	gtk_widget_show_all( button );
	gtk_widget_set_sensitive( button, sdata->writable );

	return( button );
}

static void
on_button_clicked( GtkButton *button, sIGridList *sdata )
{
	static const gchar *thisfn = "my_igridlist_on_button_clicked";
	guint column, row;

	column = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), IGRIDLIST_COLUMN ));
	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), IGRIDLIST_ROW ));

	g_debug( "%s: button=%p, sdata=%p, column=%u, row=%u",
			thisfn, ( void * ) button, ( void * ) sdata, column, row );

	if( column == COL_ADD ){
		my_igridlist_add_row( sdata->instance, sdata->grid, NULL );

	} else if( column == COL_UP ){
		g_return_if_fail( row > sdata->first_row );
		exchange_rows( sdata, row, row-1 );

	} else if( column == COL_DOWN ){
		g_return_if_fail( row < sdata->first_row + sdata->rows_count );
		exchange_rows( sdata, row, row+1 );

	} else if( column == COL_REMOVE ){
		remove_row( sdata, row );

	} else {
		g_warning( "%s: invalid column=%u", thisfn, column );
	}
}

static void
exchange_rows( sIGridList *sdata, gint row_a, gint row_b )
{
	gint i;
	GtkWidget *w_a, *w_b;

	/* do not move the row number */
	for( i=1 ; i<sdata->columns_count+3 ; ++i ){

		w_a = gtk_grid_get_child_at( sdata->grid, i, row_a );
		g_object_ref( w_a );
		gtk_container_remove( GTK_CONTAINER( sdata->grid ), w_a );

		w_b = gtk_grid_get_child_at( sdata->grid, i, row_b );
		g_object_ref( w_b );
		gtk_container_remove( GTK_CONTAINER( sdata->grid ), w_b );

		gtk_grid_attach( sdata->grid, w_a, i, row_b, 1, 1 );
		g_object_set_data( G_OBJECT( w_a ), IGRIDLIST_ROW, GUINT_TO_POINTER( row_b ));

		gtk_grid_attach( sdata->grid, w_b, i, row_a, 1, 1 );
		g_object_set_data( G_OBJECT( w_b ), IGRIDLIST_ROW, GUINT_TO_POINTER( row_a ));

		g_object_unref( w_a );
		g_object_unref( w_b );
	}

	update_detail_buttons( sdata );
}

static void
remove_row( sIGridList *sdata, gint row )
{
	gint ic, ir;

	/* first remove the detail line */
	for( ic=1 ; ic<=sdata->columns_count+3 ; ++ic ){
		remove_row_widget_remove( sdata, ic, row );
	}

	/* then move the following detail lines one row up */
	for( ir=row+1 ; ir<sdata->rows_count+sdata->first_row ; ++ir ){
		for( ic=1 ; ic<=sdata->columns_count+3 ; ++ic ){
			remove_row_widget_up( sdata, ic, ir );
		}
	}

	/* last, move the last row one row up */
	ir = sdata->rows_count+sdata->first_row;
	remove_row_widget_remove( sdata, 0, ir-1 );
	remove_row_widget_up( sdata, 0, ir );

	/* last update the lines count */
	sdata->rows_count -= 1;
	signal_row_removed( sdata );
	gtk_widget_show_all( GTK_WIDGET( sdata->grid ));
}

static void
remove_row_widget_remove( sIGridList *sdata, guint column, guint row )
{
	GtkWidget *widget;

	widget = gtk_grid_get_child_at( sdata->grid, column, row );
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));
	gtk_widget_destroy( widget );
}

static void
remove_row_widget_up( sIGridList *sdata, guint column, guint rowsrc )
{
	GtkWidget *widget;

	widget = gtk_grid_get_child_at( sdata->grid, column, rowsrc );
	if( widget ){
		g_object_ref( widget );
		gtk_container_remove( GTK_CONTAINER( sdata->grid ), widget );
		gtk_grid_attach( sdata->grid, widget, column, rowsrc-1, 1, 1 );
		g_object_set_data( G_OBJECT( widget ), IGRIDLIST_ROW, GUINT_TO_POINTER( rowsrc-1 ));
		g_object_unref( widget );
	}
}

static void
update_detail_buttons( sIGridList *sdata )
{
	GtkWidget *up_btn, *down_btn;
	guint i;

	for( i=1 ; i<=sdata->rows_count ; ++i ){

		up_btn = gtk_grid_get_child_at( sdata->grid, COL_UP, i );
		g_return_if_fail( up_btn && GTK_IS_WIDGET( up_btn ));

		down_btn = gtk_grid_get_child_at( sdata->grid, COL_DOWN, i );
		g_return_if_fail( down_btn && GTK_IS_WIDGET( down_btn ));

		gtk_widget_set_sensitive( up_btn, sdata->writable );
		gtk_widget_set_sensitive( down_btn, sdata->writable );

		if( i == 1 ){
			gtk_widget_set_sensitive( up_btn, FALSE );
		}

		if( i == sdata->rows_count ){
			gtk_widget_set_sensitive( down_btn, FALSE );
		}
	}
}

static void
signal_row_added( sIGridList *sdata )
{
	update_detail_buttons( sdata );
}

static void
signal_row_removed( sIGridList *sdata )
{
	update_detail_buttons( sdata );
}

/**
 * my_igridlist_add_row:
 * @instance: this #myIGridList instance.
 * @grid: the target #GtkGrid.
 * @user_data: [allow-none]: user data to be passed to setup_row() method.
 *
 * Adds a new empty row at the end of the @grid
 * (actually at the user end, i.e. before the last row which holds the
 *  'Add' button).
 *
 * This function triggers the setup_row() method.
 *
 * Returns: the index of the newly added row, counted from zero.
 */
guint
my_igridlist_add_row( const myIGridList *instance, GtkGrid *grid, void *user_data )
{
	static const gchar *thisfn = "my_igridlist_add_row";
	sIGridList *sdata;
	guint row;

	if( 0 ){
		g_debug( "%s: instance=%p, grid=%p, user_data=%p",
				thisfn, ( void * ) instance, ( void * ) grid, user_data );
	}

	g_return_val_if_fail( instance && MY_IS_IGRIDLIST( instance ), 0 );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), 0 );

	sdata = get_igridlist_data( instance, grid );
	row = sdata->first_row+sdata->rows_count;
	add_empty_row( sdata, row );
	add_button( sdata, "gtk-go-up", COL_UP, row, 0, G_CALLBACK( on_button_clicked ));
	add_button( sdata, "gtk-go-down", COL_DOWN, row, 0, G_CALLBACK( on_button_clicked ));
	add_button( sdata, "gtk-remove", COL_REMOVE, row, 0, G_CALLBACK( on_button_clicked ));
	add_button( sdata, "gtk-add", COL_ADD, row+1, 4, G_CALLBACK( on_button_clicked ));

	if( MY_IGRIDLIST_GET_INTERFACE( instance )->setup_row ){
		MY_IGRIDLIST_GET_INTERFACE( instance )->setup_row( instance, grid, row, user_data );

	} else {
		g_info( "%s: myIGridList's %s implementation does not provide 'setup_row()' method",
				thisfn, G_OBJECT_TYPE_NAME( instance ));
	}

	sdata->rows_count = row;
	signal_row_added( sdata );
	gtk_widget_show_all( GTK_WIDGET( grid ));

	return( row );
}

static void
add_empty_row( sIGridList *sdata, guint row )
{
	gchar *str;
	GtkWidget *label;

	/* remove the Add button */
	gtk_widget_destroy( gtk_grid_get_child_at( sdata->grid, COL_ADD, row ));

	/* add the row number */
	label = gtk_label_new( NULL );
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	my_utils_widget_set_margins( label, 0, 0, 0, 4 );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_grid_attach( sdata->grid, label, COL_ROW, row, 1, 1 );
	str = g_strdup_printf( "<i>%u</i>", row );
	gtk_label_set_markup( GTK_LABEL( label ), str );
	g_free( str );
}

/**
 * my_igridlist_set_widget:
 * @instance: this #myIGridList instance.
 * @grid: the target #GtkGrid.
 * @widget: the widget to be added to the @grid.
 * @column: the target column index.
 * @row: the target row index.
 * @width: the width of the @widget.
 * @height: the height of the @widget.
 *
 * Set the @widget at its place in the @grid.
 * If a widget was already present as this place, it is destroyed.
 */
void
my_igridlist_set_widget( const myIGridList *instance, GtkGrid *grid,
							GtkWidget *widget, guint column, guint row, guint width, guint height )
{
	static const gchar *thisfn = "my_igridlist_set_widget";
	sIGridList *sdata;
	GtkWidget *child;

	if( 0 ){
		g_debug( "%s: instance=%p, grid=%p, widget=%p, column=%u, row=%u, width=%u, height=%u",
				thisfn, ( void * ) instance, ( void * ) grid, ( void * ) widget, column, row, width, height );
	}

	g_return_if_fail( instance && MY_IS_IGRIDLIST( instance ));
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	sdata = get_igridlist_data( instance, grid );

	child = gtk_grid_get_child_at( sdata->grid, column, row );
	if( child ){
		g_return_if_fail( GTK_IS_WIDGET( child ));
		gtk_widget_destroy( child );
	}

	set_widget( sdata, widget, column, row, width, height );
}

static void
set_widget( sIGridList *sdata, GtkWidget *widget, guint column, guint row, guint width, guint height )
{
	g_object_set_data( G_OBJECT( widget ), IGRIDLIST_COLUMN, GUINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( widget ), IGRIDLIST_ROW, GUINT_TO_POINTER( row ));
	gtk_grid_attach( sdata->grid, widget, column, row, width, height );
}

/**
 * my_igridlist_get_rows_count:
 * @instance: this #myIGridList instance.
 * @grid: the target #GtkGrid.
 *
 * Returns: the count of detail rows, not counting the header nor the
 * last row with only the Add button.
 */
guint
my_igridlist_get_rows_count( const myIGridList *instance, GtkGrid *grid )
{
	static const gchar *thisfn = "my_igridlist_get_rows_count";
	sIGridList *sdata;

	if( 0 ){
		g_debug( "%s: instance=%p, grid=%p", thisfn, ( void * ) instance, ( void * ) grid );
	}

	g_return_val_if_fail( instance && MY_IS_IGRIDLIST( instance ), 0 );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), 0 );

	sdata = get_igridlist_data( instance, grid );

	return( sdata->rows_count );
}

/**
 * my_igridlist_get_row_index:
 * @widget: the requested #GtkWidget.
 *
 * Returns: the row index of the @widget, counted from zero.
 */
guint
my_igridlist_get_row_index( GtkWidget *widget )
{
	g_return_val_if_fail( widget && GTK_IS_WIDGET( widget ), 0 );

	return( GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( widget ), IGRIDLIST_ROW )));

}

static sIGridList *
get_igridlist_data( const myIGridList *instance, GtkGrid *grid )
{
	sIGridList *sdata;

	sdata = ( sIGridList * ) g_object_get_data( G_OBJECT( grid ), IGRIDLIST_DATA );

	if( !sdata ){
		sdata = g_new0( sIGridList, 1 );
		g_object_set_data( G_OBJECT( grid ), IGRIDLIST_DATA, sdata );
		g_object_weak_ref( G_OBJECT( grid ), ( GWeakNotify ) on_grid_finalized, sdata );

		sdata->instance = instance;
		sdata->grid = grid;
	}

	return( sdata );
}

static void
on_grid_finalized( sIGridList *sdata, GObject *finalized_grid )
{
	static const gchar *thisfn = "my_igridlist_on_grid_finalized";

	g_debug( "%s: sdata=%p, finalized_grid=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_grid );

	g_free( sdata );
}
