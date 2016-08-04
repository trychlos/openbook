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

#include <stdlib.h>
#include <string.h>

#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-isortable.h"
#include "api/ofa-settings.h"

/* data structure associated to the instance
 */
typedef struct {

	/* input
	 */
	ofaIStore         *store;
	GtkTreeView       *tview;
	gint               def_column;
	GtkSortType        def_order;

	/* runtime
	 */
	GtkTreeModel      *sort_model;
	GtkTreeViewColumn *sort_column;
	gint               sort_column_id;
	gint               sort_order;
}
	sISortable;

#define ISORTABLE_LAST_VERSION          1
#define ISORTABLE_DATA                 "ofa-isortable-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( ofaISortableInterface *klass );
static void        interface_base_finalize( ofaISortableInterface *klass );
static void        setup_sort_model( ofaISortable *instance, sISortable *sdata );
static void        setup_columns_for_sort( ofaISortable *instance, sISortable *sdata );
static void        on_header_clicked( GtkTreeViewColumn *column, ofaISortable *instance );
static void        set_sort_indicator( ofaISortable *instance, sISortable *sdata );
static gint        on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaISortable *instance );
static gchar      *get_settings_key( const ofaISortable *instance );
static void        get_sort_settings( ofaISortable *instance, sISortable *sdata );
static void        set_sort_settings( ofaISortable *instance, sISortable *sdata );
static sISortable *get_isortable_data( ofaISortable *instance );
static void        on_instance_finalized( sISortable *sdata, GObject *finalized_instance );

/**
 * ofa_isortable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_isortable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_isortable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_isortable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaISortableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaISortable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaISortableInterface *klass )
{
	static const gchar *thisfn = "ofa_isortable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaISortableInterface *klass )
{
	static const gchar *thisfn = "ofa_isortable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_isortable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_isortable_get_interface_last_version( void )
{
	return( ISORTABLE_LAST_VERSION );
}

/**
 * ofa_isortable_get_interface_version:
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
ofa_isortable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISORTABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaISortableInterface * ) iface )->get_interface_version ){
		version = (( ofaISortableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaISortable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_isortable_sort_png:
 * @a.
 * @b
 *
 * Returns: -1, 1 or 0.
 */
gint
ofa_isortable_sort_png( const GdkPixbuf *a, const GdkPixbuf *b )
{
	gsize lena, lenb;

	if( !a ){
		return( -1 );
	}
	lena = gdk_pixbuf_get_byte_length( a );

	if( !b ){
		return( 1 );
	}
	lenb = gdk_pixbuf_get_byte_length( b );

	if( lena < lenb ){
		return( -1 );
	}
	if( lena > lenb ){
		return( 1 );
	}

	return( memcmp( a, b, lena ));
}

/**
 * ofa_isortable_sort_str_amount:
 * @a.
 * @b
 *
 * Returns: -1, 1 or 0.
 */
gint
ofa_isortable_sort_str_amount( const gchar *a, const gchar *b )
{
	ofxAmount amounta, amountb;

	if( !my_strlen( a )){
		if( !my_strlen( b )){
			return( 0 );
		}
		return( -1 );
	}
	amounta = ofa_amount_from_str( a );

	if( !my_strlen( b )){
		return( 1 );
	}
	amountb = ofa_amount_from_str( b );

	return( amounta < amountb ? -1 : ( amounta > amountb ? 1 : 0 ));
}

/**
 * ofa_isortable_sort_str_int:
 * @a.
 * @b
 *
 * Returns: -1, 1 or 0.
 */
gint
ofa_isortable_sort_str_int( const gchar *a, const gchar *b )
{
	int inta, intb;

	if( !my_strlen( a )){
		if( !my_strlen( b )){
			return( 0 );
		}
		return( -1 );
	}
	inta = atoi( a );

	if( !my_strlen( b )){
		return( 1 );
	}
	intb = atoi( b );

	return( inta < intb ? -1 : ( inta > intb ? 1 : 0 ));
}

/**
 * ofa_isortable_set_default_sort:
 * @instance: this #ofaIStorable instance.
 * @column_id: the identifier of the default sort column.
 * @order: the default sort order.
 *
 * Setup the default sort column, which is used when no settings are
 * found.
 *
 * If no default sort column is explicitely set, then it defaults to
 * ascending order on column #0.
 */
void
ofa_isortable_set_default_sort( ofaISortable *instance, gint column_id, GtkSortType order )
{
	sISortable *sdata;

	g_return_if_fail( instance && OFA_IS_ISORTABLE( instance ));
	g_return_if_fail( column_id >= 0 );
	g_return_if_fail( order == GTK_SORT_ASCENDING || order == GTK_SORT_DESCENDING );

	sdata = get_isortable_data( instance );

	sdata->def_column = column_id;
	sdata->def_order = order;
}

/**
 * ofa_isortable_set_store:
 * @instance: this #ofaIStorable instance.
 * @store: the underlying #ofaIStore instance.
 *
 * Setup the underlying store.
 *
 * If both treeview and store are set, then they are associated through
 * a sortable model, sort settings are read and a default sort function
 * is set.
 *
 * At that time, the model starts to sort itself. So it is better if all
 * configuration is set before calling this method.
 */
void
ofa_isortable_set_store( ofaISortable *instance, ofaIStore *store )
{
	sISortable *sdata;

	g_return_if_fail( instance && OFA_IS_ISORTABLE( instance ));
	g_return_if_fail( store && OFA_IS_ISTORE( store ));

	sdata = get_isortable_data( instance );
	sdata->store = store;

	setup_sort_model( instance, sdata );
}

/**
 * ofa_isortable_set_treeview:
 * @instance: this #ofaIStorable instance.
 * @treeview: the #GtkTreeView widget.
 *
 * Setup the treeview widget.
 */
void
ofa_isortable_set_treeview( ofaISortable *instance, GtkTreeView *tview )
{
	sISortable *sdata;

	g_return_if_fail( instance && OFA_IS_ISORTABLE( instance ));
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));

	sdata = get_isortable_data( instance );
	sdata->tview = tview;

	setup_sort_model( instance, sdata );
}

static void
setup_sort_model( ofaISortable *instance, sISortable *sdata )
{
	if( sdata->store && sdata->tview ){

		sdata->sort_model = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( sdata->store ));
		/* the sortable model maintains its own reference on the store */
		g_object_unref( sdata->store );

		setup_columns_for_sort( instance, sdata );
		get_sort_settings( instance, sdata );

		gtk_tree_view_set_model( sdata->tview, sdata->sort_model );
		/* the treeview maintains its own reference on the sortable model */
		g_object_unref( sdata->sort_model );

		set_sort_indicator( instance, sdata );
	}
}

static void
setup_columns_for_sort( ofaISortable *instance, sISortable *sdata )
{
	GList *it, *columns;
	GtkTreeViewColumn *column;
	gint column_id;

	columns = gtk_tree_view_get_columns( sdata->tview );

	for( it=columns ; it ; it=it->next ){
		column = GTK_TREE_VIEW_COLUMN( it->data );

		g_signal_connect( column, "clicked", G_CALLBACK( on_header_clicked ), instance );

		column_id = gtk_tree_view_column_get_sort_column_id( column );

		gtk_tree_sortable_set_sort_func(
				GTK_TREE_SORTABLE( sdata->sort_model ), column_id,
				( GtkTreeIterCompareFunc ) on_sort_model, instance, NULL );
	}

	g_list_free( columns );
}

/*
 * Gtk+ efault behavior:
 *	initial display: order of insertion in the store
 *	click 1: ascending order, indicator v
 *	click 2: descending order, indicator ^
 *	click 3: ascending order, no indicator
 *	click 4: ascending order, indicator v (back to click 1)
 */
static void
on_header_clicked( GtkTreeViewColumn *column, ofaISortable *instance )
{
	sISortable *sdata;

	sdata = get_isortable_data( instance );

	if( column != sdata->sort_column ){
		gtk_tree_view_column_set_sort_indicator( sdata->sort_column, FALSE );
		sdata->sort_column = column;
		sdata->sort_column_id = gtk_tree_view_column_get_sort_column_id( column );
		sdata->sort_order = GTK_SORT_ASCENDING;

	} else {
		sdata->sort_order = sdata->sort_order == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
	}

	set_sort_settings( instance, sdata );
	set_sort_indicator( instance, sdata );
}

/*
 * It happens that Gtk+ makes use of up arrow '^' (resp. a down arrow 'v')
 * to indicate a descending (resp. ascending) sort order. This is counter-
 * intuitive as we are expecting the arrow pointing to the smallest item.
 *
 * So inverse the sort order of the sort indicator.
 */
static void
set_sort_indicator( ofaISortable *instance, sISortable *sdata )
{
	if( sdata->sort_model ){
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( sdata->sort_model ), sdata->sort_column_id, sdata->sort_order );
	}

	if( sdata->sort_column ){
		gtk_tree_view_column_set_sort_indicator(
				sdata->sort_column, TRUE );
		gtk_tree_view_column_set_sort_order(
				sdata->sort_column,
				sdata->sort_order == GTK_SORT_ASCENDING ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING );
	}
}

static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaISortable *instance )
{
	static const gchar *thisfn = "ofa_isortable_on_sort_model";
	sISortable *sdata;

	if( OFA_ISORTABLE_GET_INTERFACE( instance )->sort_model ){
		sdata = get_isortable_data( instance );
		return( OFA_ISORTABLE_GET_INTERFACE( instance )->sort_model( instance, tmodel, a, b, sdata->sort_column_id ));
	}

	g_info( "%s: ofaISortable's %s implementation does not provide 'sort_model()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 0 );
}

static gchar *
get_settings_key( const ofaISortable *instance )
{
	const gchar *ckey;

	ckey = NULL;

	if( OFA_ISORTABLE_GET_INTERFACE( instance )->get_settings_key ){
		ckey = OFA_ISORTABLE_GET_INTERFACE( instance )->get_settings_key( instance );
	}

	return( g_strdup( my_strlen( ckey ) ? ckey : G_OBJECT_TYPE_NAME( instance )));
}

/*
 * sort_settings: sort_column_id;sort_order;
 *
 * Note that we record the actual sort order (gtk_sort_ascending for
 * ascending order); only the *display* of the sort indicator of the
 * column is reversed.
 */
static void
get_sort_settings( ofaISortable *instance, sISortable *sdata )
{
	GList *slist, *it, *columns;
	const gchar *cstr;
	gchar *prefix_key, *sort_key;
	GtkTreeViewColumn *column;

	/* setup default sort order
	 */
	sdata->sort_column = NULL;
	sdata->sort_column_id = sdata->def_column;
	sdata->sort_order = sdata->def_order;

	/* get the settings (if any)
	 */
	prefix_key = get_settings_key( instance );
	sort_key = g_strdup_printf( "%s-sort", prefix_key );
	slist = ofa_settings_user_get_string_list( sort_key );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		sdata->sort_column_id = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		sdata->sort_order = atoi( cstr );
	}

	ofa_settings_free_string_list( slist );
	g_free( sort_key );
	g_free( prefix_key );

	/* setup the initial sort column
	 */
	columns = gtk_tree_view_get_columns( sdata->tview );
	for( it=columns ; it ; it=it->next ){
		column = GTK_TREE_VIEW_COLUMN( it->data );
		if( gtk_tree_view_column_get_sort_column_id( column ) == sdata->sort_column_id ){
			sdata->sort_column = column;
			break;
		}
	}
	g_list_free( columns );
}

static void
set_sort_settings( ofaISortable *instance, sISortable *sdata )
{
	gchar *str, *prefix_key, *sort_key;

	prefix_key = get_settings_key( instance );
	sort_key = g_strdup_printf( "%s-sort", prefix_key );
	str = g_strdup_printf( "%d;%d;", sdata->sort_column_id, sdata->sort_order );

	ofa_settings_user_set_string( sort_key, str );

	g_free( str );
	g_free( sort_key );
	g_free( prefix_key );
}

static sISortable *
get_isortable_data( ofaISortable *instance )
{
	sISortable *sdata;

	sdata = ( sISortable * ) g_object_get_data( G_OBJECT( instance ), ISORTABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sISortable, 1 );
		g_object_set_data( G_OBJECT( instance ), ISORTABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		sdata->def_column = 0;
		sdata->def_order = GTK_SORT_ASCENDING;
	}

	return( sdata );
}

/*
 * ofaISortable instance finalization
 */
static void
on_instance_finalized( sISortable *sdata, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_isortable_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_instance=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_instance );

	g_free( sdata );
}