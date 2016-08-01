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

#include "api/ofa-itvcolumnable.h"
#include "api/ofa-settings.h"

#define ITVCOLUMNABLE_LAST_VERSION  1

/* a data structure attached to the instance
 */
#define ITVCOLUMNABLE_DATA          "ofa-itreeview-display-data"

typedef struct {
	GSimpleActionGroup *action_group;
	gchar              *group_name;
	GList              *columns_list;
	GMenu              *submenu;
	GtkTreeView        *treeview;
	gint                visible_count;
}
	sITVColumnable;

/* the data stored for each displayable column
 */
typedef struct {
	gint               id;
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
ofa_itvcolumnable_add_column( ofaITVColumnable *instance, GtkTreeViewColumn *column, const gchar *menu_label )
{
	sITVColumnable *sdata;
	sColumn *scol;
	GSimpleAction *action;
	gint column_id;
	gboolean visible;
	GMenuItem *menu_item;
	gchar *action_name;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );

	column_id = gtk_tree_view_column_get_sort_column_id( column );
	visible = gtk_tree_view_column_get_visible( column );

	scol = get_column_data_by_id( instance, sdata, column_id );
	g_return_if_fail( scol == NULL );

	/* define new column properties */
	scol = g_new0( sColumn, 1 );
	sdata->columns_list = g_list_append( sdata->columns_list, scol );

	scol->id = column_id;
	scol->name = id_to_name( scol->id );
	scol->label = g_strdup( menu_label ? menu_label : gtk_tree_view_column_get_title( column ));
	scol->column = column;
	scol->def_visible = FALSE;

	/* define a new action and attach it to the action group
	 * default visibility state is set */
	action = g_simple_action_new_stateful( scol->name, NULL, g_variant_new_boolean( visible ));
	g_signal_connect( action, "change-state", G_CALLBACK( on_action_changed_state ), instance );
	g_action_map_add_action( G_ACTION_MAP( sdata->action_group ), G_ACTION( action ));
	g_object_unref( action );
	scol->action = action;

	/* update the columns menu */
	action_name = g_strdup_printf( "%s.%s", sdata->group_name, scol->name );
	menu_item = g_menu_item_new( scol->label, action_name );
	g_menu_append_item( sdata->submenu, menu_item );
	g_object_unref( menu_item );
	g_free( action_name );
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
 * ofa_itvcolumnable_set_context_menu:
 * @instance: the #ofaITVColumnable instance.
 * @parent_menu: the context menu provided by the implementation.
 * @action_group: the group in which the action are to be set.
 * @group_name: the name of the action group.
 *
 * Let the #ofaITVColumnable interface initialize its part of the
 * context menu.
 */
void
ofa_itvcolumnable_set_context_menu( ofaITVColumnable *instance, GMenu *parent_menu, GActionGroup *action_group, const gchar *group_name )
{
	static const gchar *thisfn = "ofa_itvcolumnable_set_context_menu";
	sITVColumnable *sdata;

	g_debug( "%s: instance=%p (%s), parent_menu=%p, action_group=%p, group_name=%s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) parent_menu, ( void * ) action_group, group_name );

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));
	g_return_if_fail( parent_menu && G_IS_MENU( parent_menu ));
	g_return_if_fail( action_group && G_IS_ACTION_GROUP( action_group ));
	g_return_if_fail( group_name && my_strlen( group_name ));

	sdata = get_instance_data( instance );

	sdata->submenu = g_menu_new();
	g_menu_append_submenu( parent_menu, _( "Visible columns" ), G_MENU_MODEL( sdata->submenu ));
	g_object_unref( sdata->submenu );

	sdata->action_group = g_object_ref( action_group );
	sdata->group_name = g_strdup( group_name );
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

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	sdata->treeview = treeview;
	sdata->visible_count = 0;

	count = get_settings( instance, sdata );

	if( count == 0 ){
		for( it=sdata->columns_list ; it ; it=it->next ){
			scol = ( sColumn * ) it->data;
			if( scol->def_visible ){
				g_action_group_change_action_state(
						G_ACTION_GROUP( sdata->action_group ), scol->name, g_variant_new_boolean( TRUE ));
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

	g_object_unref( sdata->action_group );
	g_free( sdata->group_name );
	g_list_free_full( sdata->columns_list, ( GDestroyNotify ) free_column );

	g_free( sdata );
}

static void
free_column( sColumn *scol )
{
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
				g_action_group_change_action_state(
						G_ACTION_GROUP( sdata->action_group ), scol->name, g_variant_new_boolean( TRUE ));
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

#if 0

static const gchar *st_resource_arrow_down  = "/org/trychlos/openbook/ui/ofa-itreeview-display-arrow-down.png";

static gboolean           has_column_id( GList *list, gint column_id );
static GtkWidget         *setup_button( const ofaITVColumnable *instance, sITVColumnable *sdata );
static void               update_settings( ofaITVColumnable *instance, sITVColumnable *sdata );

/**
 * ofa_itvcolumnable_add_column:
 * @instance: the #ofaITVColumnable instance.
 * @id: the column identifier in the store.
 * @label: [allow-none]: the localized label for the selection menu.
 * @visible: whether the column defaults to be displayed.
 * @column: [allow-none]: the #GtkTreeViewColumn column.
 *
 * Define a new displayable column.
 *
 * The function fails is a column with the same @id has already been
 * defined.
 */
void
ofa_itvcolumnable_add_column( ofaITVColumnable *instance, GtkTreeViewColumn *column, guint id )
{
	sITVColumnable *sdata;
	sColumn *scol;
	GSimpleAction *action;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	scol = get_column_by_id( instance, sdata, id );
	g_return_if_fail( scol == NULL );

	/* define new column properties */
	scol = g_new0( sColumn, 1 );
	sdata->columns_list = g_list_append( sdata->columns_list, scol );

	scol->id = id;
	scol->name = id_to_name( id );
	scol->column = column;

	if( OFA_ITVCOLUMNABLE_GET_INTERFACE( instance )->get_label ){
		scol->label = OFA_ITVCOLUMNABLE_GET_INTERFACE( instance )->get_label( instance, id );
	}

	scol->visible_count = TRUE;
	if( my_strlen( scol->label )){
		if( OFA_ITVCOLUMNABLE_GET_INTERFACE( instance )->get_def_visible ){
			scol->visible_count = OFA_ITVCOLUMNABLE_GET_INTERFACE( instance )->get_def_visible( instance, id );
		}
	}

	/* define a new action and attach it to the action group
	 * default visibility state is set */
	action = g_simple_action_new_stateful(
					scol->name, NULL, g_variant_new_boolean( scol->visible_count ));
	g_signal_connect( action, "change-state", G_CALLBACK( on_action_changed_state ), instance );
	g_action_map_add_action( G_ACTION_MAP( sdata->action_group ), G_ACTION( action ));
	g_object_unref( action );
}

/**
 * ofa_itvcolumnable_init_visible:
 * @instance: this #ofaITVColumnable instance.
 * @key: the settings key where displayed columns are stored.
 *
 * Initialize the visible columns on treeview initialization.
 *
 * This make sure the #GtkTreeViewColumn columns previously defined
 * are visible, depending of their default visibility state, and the
 * value read from the settings.
 */
void
ofa_itvcolumnable_init_visible( const ofaITVColumnable *instance, const gchar *key )
{
	static const gchar *thisfn = "ofa_itvcolumnable_init_visible";
	sITVColumnable *sdata;
	GList *list, *it;
	sColumn *scol;
	gboolean visible;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	sdata->initialization = TRUE;
	g_free( sdata->settings_key );
	sdata->settings_key = g_strdup( key );

	list = ofa_settings_user_get_uint_list( sdata->settings_key );

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		visible = list ? has_column_id( list, scol->id ) : scol->visible_count;
		g_debug( "%s: action=%s, visible=%s", thisfn, scol->label, visible ? "True":"False" );
		g_action_group_change_action_state(
				G_ACTION_GROUP( sdata->action_group ), scol->name, g_variant_new_boolean( visible ));
	}

	ofa_settings_free_uint_list( list );
	sdata->initialization = FALSE;
}

/*
 * is the column identifier found in the list read from user prefs ?
 */
static gboolean
has_column_id( GList *list, gint column_id )
{
	GList *it;

	for( it=list ; it ; it=it->next ){
		if( GPOINTER_TO_INT( it->data ) == column_id ){
			return( TRUE );
		}
	}

	return( FALSE );
}

/**
 * ofa_itvcolumnable_get_visible:
 * @instance: this #ofaITVColumnable instance.
 * @id: a column identifier.
 *
 * Returns: whether the column is visible.
 * Returns: %TRUE if the @id column has not been previously defined
 *  (the visible status of the column cannot be toggled by the user).
 */
gboolean
ofa_itvcolumnable_get_visible( const ofaITVColumnable *instance, guint id )
{
	sITVColumnable *sdata;
	sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), FALSE );

	sdata = get_instance_data( instance );
	scol = get_column_by_id( instance, sdata, id );
	if( scol && scol->column ){
		return( gtk_tree_view_column_get_visible( scol->column ));
	}

	return( TRUE );
}

/**
 * ofa_itvcolumnable_set_visible:
 * @instance: this #ofaITVColumnable instance.
 * @id: a column identifier.
 * @visible: whether the column must be set visible or not.
 *
 * Toggle the column visibility.
 */
void
ofa_itvcolumnable_set_visible( const ofaITVColumnable *instance, guint id, gboolean visible )
{
	sITVColumnable *sdata;
	gchar *name;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	name = id_to_name( id );
	g_action_group_change_action_state(
			G_ACTION_GROUP( sdata->action_group ), name, g_variant_new_boolean( visible ));
	g_free( name );
}

/**
 * ofa_itvcolumnable_attach_menu_button:
 * @instance: this #ofaITVColumnable instance.
 * @parent: a #GtkContainer to which attach the menu button.
 *
 * Attach a menu button to the @parent container.
 * The menu contains one toggle action for each previously defined
 * column.
 *
 * Returns: the attached combo box
 */
GtkWidget *
ofa_itvcolumnable_attach_menu_button( const ofaITVColumnable *instance, GtkContainer *parent )
{
	sITVColumnable *sdata;
	GtkWidget *button;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	button = setup_button( instance, sdata );
	gtk_container_add( parent, button );

	return( button );
}

/*
 * Defines a new menu button with the attached menu, and returns it
 */
static GtkWidget *
setup_button( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	GList *it;
	sColumn *scol;
	GtkWidget *button, *box, *label, *image;
	GMenu *menu;
	gchar *action_name;
	GMenuItem *item;

	button = gtk_menu_button_new();

	/* setup menu button */
	gtk_widget_set_halign( button, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( button, TRUE );
	gtk_menu_button_set_direction( GTK_MENU_BUTTON( button ), GTK_ARROW_DOWN );
	gtk_menu_button_set_use_popover( GTK_MENU_BUTTON( button ), FALSE );
	gtk_menu_button_set_align_widget( GTK_MENU_BUTTON( button ), NULL );

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
	gtk_container_add( GTK_CONTAINER( button ), box );

	label = gtk_label_new_with_mnemonic( _( "_Columns selection" ));
	gtk_box_pack_start( GTK_BOX( box ), label, FALSE, TRUE, 0 );

	image = gtk_image_new_from_resource( st_resource_arrow_down );
	gtk_box_pack_end( GTK_BOX( box ), image, FALSE, TRUE, 0 );

	/* create the menu */
	menu = g_menu_new();
	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		if( scol->label ){
			action_name = g_strdup_printf( "%s.%s", st_prefix, scol->name );
			item = g_menu_item_new( scol->label, action_name );
			g_menu_append_item( menu, item );
			g_free( action_name );
			g_object_unref( item );
		}
	}
	gtk_menu_button_set_menu_model( GTK_MENU_BUTTON( button ), G_MENU_MODEL( menu ));
	gtk_widget_insert_action_group( button, st_prefix, G_ACTION_GROUP( sdata->action_group ));

	return( button );
}

static void
update_settings( ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar **actions_list, **it;
	GList *id_list;
	GVariant *value;
	gboolean visible;

	if( my_strlen( sdata->settings_key )){
		actions_list = g_action_group_list_actions( G_ACTION_GROUP( sdata->action_group ));
		it = actions_list;
		id_list = NULL;
		while( *it ){
			value = g_action_group_get_action_state( G_ACTION_GROUP( sdata->action_group ), *it );
			visible = g_variant_get_boolean( value );
			if( visible ){
				id_list = g_list_prepend( id_list, GINT_TO_POINTER( name_to_id( *it )));
			}
			g_variant_unref( value );
			it++;
		}
		ofa_settings_user_set_uint_list( sdata->settings_key, id_list );
		g_list_free( id_list );
		g_strfreev( actions_list );
	}
}
#endif
