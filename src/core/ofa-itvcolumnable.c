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
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-iactionable.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-settings.h"

#define ITVCOLUMNABLE_LAST_VERSION  1

/* a data structure attached to the instance
 */
#define ITVCOLUMNABLE_DATA          "ofa-itvcolumnable-data"

typedef struct {
	GList              *columns_list;
	GtkTreeView        *treeview;
	gint                visible_count;
}
	sITVColumnable;

/* the data stored for each displayable column
 */
typedef struct {
	gint               id;
	gchar             *group_name;
	gchar             *name;			/* internal name of the action */
	gchar             *label;			/* menu item label */
	gboolean           def_visible;
	GtkTreeViewColumn *column;
	GSimpleAction     *action;
}
	sColumn;

/* signals defined here
 */
enum {
	TOGGLED = 0,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static guint        st_initializations      = 0;	/* interface initialization count */

static const gchar *st_prefixname           = "action";

static GType           register_type( void );
static void            interface_base_init( ofaITVColumnableInterface *klass );
static void            interface_base_finalize( ofaITVColumnableInterface *klass );
static void            on_action_changed_state( GSimpleAction *action, GVariant *value, ofaITVColumnable *instance );
static sITVColumnable *get_instance_data( const ofaITVColumnable *instance );
static void            on_instance_finalized( sITVColumnable *sdata, void *instance );
static void            free_column( sColumn *scol );
static sColumn        *get_column_data_by_id( const ofaITVColumnable *instance, sITVColumnable *sdata, gint id );
static sColumn        *get_column_data_by_name( ofaITVColumnable *instance, sITVColumnable *sdata, const gchar *name );
static gchar          *id_to_name( gint id );
static gint            name_to_id( const gchar *name );
static gchar          *get_settings_key( const ofaITVColumnable *instance );
static gint            get_settings( const ofaITVColumnable *instance, sITVColumnable *sdata );
static void            set_settings( const ofaITVColumnable *instance, sITVColumnable *sdata );

/**
 * ofa_itvcolumnable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itvcolumnable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itvcolumnable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itvcolumnable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaITVColumnableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITVColumnable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaITVColumnableInterface *klass )
{
	static const gchar *thisfn = "ofa_itvcolumnable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaITVColumnable::ofa-toggled:
		 *
		 * This signal is sent when the visibility state of a column is
		 * toggled.
		 * @column_id: the column identifier.
		 * @visible: the new visibility status.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaITVColumnable *instance,
		 * 						gint            column_id,
		 * 						gboolean        visible,
		 * 						gpointer        user_data );
		 */
		st_signals[ TOGGLED ] = g_signal_new_class_handler(
					"ofa-toggled",
					OFA_TYPE_ITVCOLUMNABLE,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_INT, G_TYPE_BOOLEAN );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaITVColumnableInterface *klass )
{
	static const gchar *thisfn = "ofa_itvcolumnable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_itvcolumnable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itvcolumnable_get_interface_last_version( void )
{
	return( ITVCOLUMNABLE_LAST_VERSION );
}

/**
 * ofa_itvcolumnable_get_interface_version:
 * @instance: this #ofaITVColumnable instance.
 *
 * Returns: the version number of this interface implemented by the
 * @instance.
 *
 * Defaults to 1.
 */
guint
ofa_itvcolumnable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ITVCOLUMNABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaITVColumnableInterface * ) iface )->get_interface_version ){
		version = (( ofaITVColumnableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaITVColumnable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_itvcolumnable_add_column:
 * @instance: the #ofaITVColumnable instance.
 * @column: the #GtkTreeViewColumn column.
 * @group_name: the name of the action group.
 * @menu_label: [allow-none]: the localized label for the selection menu;
 *  defaults to column title.
 *
 * Records a new displayable column.
 *
 * It is expected that this method be called after the column has been
 * appended to the GtkTreeView. It is also expected that each column has
 * received its own sort id.
 */
void
ofa_itvcolumnable_add_column( ofaITVColumnable *instance, GtkTreeViewColumn *column, const gchar *group_name, const gchar *menu_label )
{
	sITVColumnable *sdata;
	sColumn *scol;
	GSimpleAction *action;
	gint column_id;
	gboolean visible;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );

	column_id = gtk_tree_view_column_get_sort_column_id( column );
	visible = gtk_tree_view_column_get_visible( column );

	scol = get_column_data_by_id( instance, sdata, column_id );
	g_return_if_fail( scol == NULL );

	/* define new column properties */
	scol = g_new0( sColumn, 1 );
	sdata->columns_list = g_list_append( sdata->columns_list, scol );

	scol->id = column_id;
	scol->group_name = g_strdup( group_name );
	scol->name = id_to_name( scol->id );
	scol->label = g_strdup( menu_label ? menu_label : gtk_tree_view_column_get_title( column ));
	scol->column = column;
	scol->def_visible = FALSE;

	/* define a new action and attach it to the action group
	 * default visibility state is set */
	action = g_simple_action_new_stateful( scol->name, NULL, g_variant_new_boolean( visible ));
	g_signal_connect( action, "change-state", G_CALLBACK( on_action_changed_state ), instance );
	ofa_iactionable_set_menu_item( OFA_IACTIONABLE( instance ), group_name, G_ACTION( action ), scol->label );
	g_object_unref( action );
	scol->action = action;
}

/*
 * a request has been made to chage the state of the action
 * this request may have been sent from our code (see init_visible())
 * or after the action has been activated from the UI
 *
 * If we are not during init_visible(), and the key has been set, then
 * settings are updated with displayed columns
 */
static void
on_action_changed_state( GSimpleAction *action, GVariant *value, ofaITVColumnable *instance )
{
	static const gchar *thisfn = "ofa_itvcolumnable_on_action_changed_state";
	gboolean visible;
	sITVColumnable *sdata;
	sColumn *scol;
	GList *it;

	/* set the action state as requested (which is just the default) */
	g_simple_action_set_state( action, value);

	/* display the column or not */
	sdata = get_instance_data( instance );
	scol = get_column_data_by_name( instance, sdata, g_action_get_name( G_ACTION( action )));

	if( scol->column ){
		visible = g_variant_get_boolean( value );
		gtk_tree_view_column_set_visible( scol->column, visible );
		g_debug( "%s: column=%s, visible=%s", thisfn, scol->label, visible ? "True":"False" );
		g_signal_emit_by_name( instance, "ofa-toggled", scol->id, visible );
		sdata->visible_count += ( visible ? 1 : -1 );
		//g_debug( "%s: visible_count=%d", thisfn, sdata->visible_count );

		/* be sure that the last visible column will not be disabled */
		if( sdata->visible_count == 1 ){
			for( it=sdata->columns_list ; it ; it=it->next ){
				scol = ( sColumn * ) it->data;
				if( gtk_tree_view_column_get_visible( scol->column )){
					g_simple_action_set_enabled( scol->action, FALSE );
					break;
				}
			}
		} else if( visible && sdata->visible_count == 2 ){
			for( it=sdata->columns_list ; it ; it=it->next ){
				scol = ( sColumn * ) it->data;
				if( !g_action_get_enabled( G_ACTION( scol->action ))){
					//g_debug( "%s: enabling action %s", thisfn, scol->label );
					g_simple_action_set_enabled( scol->action, TRUE );
					break;
				}
			}
		}
	}
}

/**
 * ofa_itvcolumnable_record_settings:
 * @instance: the #ofaITVColumnable instance.
 *
 * Records the current configuration in user settings.
 */
void
ofa_itvcolumnable_record_settings( ofaITVColumnable *instance )
{
	static const gchar *thisfn = "ofa_itvcolumnable_record_settings";
	sITVColumnable *sdata;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	set_settings( instance, sdata );
}

/**
 * ofa_itvcolumnable_set_default_column:
 * @instance: the #ofaITVColumnable instance.
 * @column_id: the identifier of the column.
 *
 * Identifies a column to be made visible if not settings are found.
 */
void
ofa_itvcolumnable_set_default_column( ofaITVColumnable *instance, gint column_id )
{
	sITVColumnable *sdata;
	sColumn *scol;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	scol = get_column_data_by_id( instance, sdata, column_id );
	g_return_if_fail( scol );

	scol->def_visible = TRUE;
}

/**
 * ofa_itvcolumnable_show_columns:
 * @instance: the #ofaITVColumnable instance.
 *
 * Show the registered columns, either because they are recorded in the
 * settings, or (if no settings) because they are set as visible by default.
 */
void
ofa_itvcolumnable_show_columns( ofaITVColumnable *instance, GtkTreeView *treeview )
{
	sITVColumnable *sdata;
	GList *it;
	gint count;
	sColumn *scol;
	GActionGroup *action_group;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );
	sdata->treeview = treeview;
	sdata->visible_count = 0;

	count = get_settings( instance, sdata );

	if( count == 0 ){
		for( it=sdata->columns_list ; it ; it=it->next ){
			scol = ( sColumn * ) it->data;
			if( scol->def_visible ){
				action_group = ofa_iactionable_get_action_group( OFA_IACTIONABLE( instance ), scol->group_name );
				g_action_group_change_action_state( action_group, scol->name, g_variant_new_boolean( TRUE ));
			}
		}
	}
}

/*
 * returns the sITVColumnable data structure attached to the instance
 */
static sITVColumnable *
get_instance_data( const ofaITVColumnable *instance )
{
	sITVColumnable *sdata;

	sdata = ( sITVColumnable * ) g_object_get_data( G_OBJECT( instance ), ITVCOLUMNABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sITVColumnable, 1 );

		g_object_set_data( G_OBJECT( instance ), ITVCOLUMNABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sITVColumnable *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_itvcolumnable_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_list_free_full( sdata->columns_list, ( GDestroyNotify ) free_column );

	g_free( sdata );
}

static void
free_column( sColumn *scol )
{
	g_free( scol->group_name );
	g_free( scol->name );
	g_free( scol );
}

/*
 * returns the sColumn data structure defined for the column
 * or %NULL if the column has not yet been defined
 */
static sColumn *
get_column_data_by_id( const ofaITVColumnable *instance, sITVColumnable *sdata, gint id )
{
	GList *it;
	sColumn *scol;

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		if( scol->id == id ){
			return( scol );
		}
	}

	return( NULL );
}

/*
 * returns the sColumn data structure corresponding to the action name
 */
static sColumn *
get_column_data_by_name( ofaITVColumnable *instance, sITVColumnable *sdata, const gchar *name )
{
	return( get_column_data_by_id( instance, sdata, name_to_id( name )));
}

static gchar *
id_to_name( gint id )
{
	return( g_strdup_printf( "%s%u", st_prefixname, id ));
}

static gint
name_to_id( const gchar *name )
{
	static guint len_prefix = 0;

	if( !len_prefix ){
		len_prefix = my_strlen( st_prefixname );
	}
	return( atoi( name+len_prefix ));
}

static gchar *
get_settings_key( const ofaITVColumnable *instance )
{
	const gchar *ckey;

	ckey = NULL;

	if( OFA_ITVCOLUMNABLE_GET_INTERFACE( instance )->get_settings_key ){
		ckey = OFA_ITVCOLUMNABLE_GET_INTERFACE( instance )->get_settings_key( instance );
	}

	return( g_strdup( my_strlen( ckey ) ? ckey : G_OBJECT_TYPE_NAME( instance )));
}

/*
 * settings: pairs of <column_id;column_width;> in order of appearance
 *
 * Returns: count of found columns.
 */
static gint
get_settings( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar *prefix_key, *settings_key;
	GList *slist, *it;
	const gchar *cstr;
	gint count, col_id, col_width;
	sColumn *scol;
	GtkTreeViewColumn *prev;
	GActionGroup *action_group;

	prefix_key = get_settings_key( instance );
	settings_key = g_strdup_printf( "%s-columns", prefix_key );
	slist = ofa_settings_user_get_string_list( settings_key );

	count = 0;
	prev = NULL;
	it = slist ? slist : NULL;

	while( it ){
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			col_id = atoi( cstr );
			it = it->next;
			cstr = it ? it->data : NULL;
			g_return_val_if_fail( my_strlen( cstr ), 0 );
			col_width = atoi( cstr );
			scol = get_column_data_by_id( instance, sdata, col_id );
			if( scol && scol->column ){
				gtk_tree_view_move_column_after( sdata->treeview, scol->column, prev );
				gtk_tree_view_column_set_fixed_width( scol->column, col_width );
				action_group = ofa_iactionable_get_action_group( OFA_IACTIONABLE( instance ), scol->group_name );
				g_action_group_change_action_state( action_group, scol->name, g_variant_new_boolean( TRUE ));
				prev = scol->column;
				count += 1;
			}
		}
		it = it ? it->next : NULL;
	}

	ofa_settings_free_string_list( slist );
	g_free( settings_key );
	g_free( prefix_key );

	return( count );
}

static void
set_settings( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar *prefix_key, *settings_key;
	guint i, count;
	GString *str;
	gint col_id, col_width;
	GtkTreeViewColumn *column;

	if( sdata->treeview ){

		str = g_string_new( "" );
		prefix_key = get_settings_key( instance );
		settings_key = g_strdup_printf( "%s-columns", prefix_key );
		count = gtk_tree_view_get_n_columns( sdata->treeview );

		for( i=0 ; i<count ; ++i ){
			column = gtk_tree_view_get_column( sdata->treeview, i );
			if( gtk_tree_view_column_get_visible( column )){
				col_id = gtk_tree_view_column_get_sort_column_id( column );
				col_width = gtk_tree_view_column_get_width( column );
				g_string_append_printf( str, "%d;%d;", col_id, col_width );
			}
		}

		ofa_settings_user_set_string( settings_key, str->str );

		g_free( settings_key );
		g_free( prefix_key );
		g_string_free( str, TRUE );
	}
}
