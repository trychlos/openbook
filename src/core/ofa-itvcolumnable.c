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
	gchar              *name;
	GList              *columns_list;
	GtkTreeView        *treeview;
	gint                visible_count;
}
	sITVColumnable;

/* the data stored for each GtkTreeViewColumn
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

static const gchar *st_action_prefix        = "itvcolumnable_";

static GType           register_type( void );
static void            interface_base_init( ofaITVColumnableInterface *klass );
static void            interface_base_finalize( ofaITVColumnableInterface *klass );
static void            on_action_changed_state( GSimpleAction *action, GVariant *value, ofaITVColumnable *instance );
static gint            get_column_id( const ofaITVColumnable *instance, sITVColumnable *sdata, GtkTreeViewColumn *column );
static void            do_propagate_visible_columns( ofaITVColumnable *source, sITVColumnable *sdata, ofaITVColumnable *target );
static void            itvcolumnable_clear_all( ofaITVColumnable *instance );
static sITVColumnable *get_instance_data( const ofaITVColumnable *instance );
static void            on_instance_finalized( sITVColumnable *sdata, void *instance );
static void            free_column( sColumn *scol );
static gchar          *get_actions_group_name( const ofaITVColumnable *instance, sITVColumnable *sdata );
static sColumn        *get_column_data_by_id( const ofaITVColumnable *instance, sITVColumnable *sdata, gint id );
static sColumn        *get_column_data_by_ptr( const ofaITVColumnable *instance, sITVColumnable *sdata, GtkTreeViewColumn *column );
static sColumn        *get_column_data_by_action_name( ofaITVColumnable *instance, sITVColumnable *sdata, const gchar *name );
static gchar          *column_id_to_action_name( gint id );
static gint            action_name_to_column_id( const gchar *name );
static gint            read_settings( const ofaITVColumnable *instance, sITVColumnable *sdata );
static void            write_settings( const ofaITVColumnable *instance, sITVColumnable *sdata );

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
 * ofa_itvcolumnable_set_name:
 * @instance: the #ofaITVColumnable instance.
 * @name: the name which identifies this @instance.
 *
 * Set the name of the instance.
 *
 * The provided @name both:
 * - identifies the actions group (an action is created for each
 *   toggable column),
 * - is used as the settings key (to record size and position of the
 *   columns).
 *
 * This identifier @name should be provided before any column be added
 * to the treeview, or will just be ignored. It defaults to the class
 * name of the implementation.
 */
void
ofa_itvcolumnable_set_name( ofaITVColumnable *instance, const gchar *name )
{
	sITVColumnable *sdata;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );

	g_return_if_fail( sdata->columns_list == NULL );

	g_free( sdata->name );
	sdata->name = g_strdup( name );
}

/**
 * ofa_itvcolumnable_set_treeview:
 * @instance: the #ofaITVColumnable instance.
 * @treeview: the #GtkTreeView to which columns are to be added.
 *
 * Set the managed #GtkTreeView.
 *
 * The managed @treeview must be set before any column try to be added
 * to it, or these will fail.
 */
void
ofa_itvcolumnable_set_treeview( ofaITVColumnable *instance, GtkTreeView *treeview )
{
	sITVColumnable *sdata;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));
	g_return_if_fail( treeview && GTK_IS_TREE_VIEW( treeview ));

	sdata = get_instance_data( instance );

	g_return_if_fail( sdata->columns_list == NULL );

	sdata->treeview = g_object_ref( treeview );
}

/**
 * ofa_itvcolumnable_add_column:
 * @instance: the #ofaITVColumnable instance.
 * @column: the #GtkTreeViewColumn column.
 * @column_id: the identifier of this column.
 * @menu_label: [allow-none]: the localized label for the selection menu;
 *  defaults to column title.
 *
 * Records a new displayable column.
 */
void
ofa_itvcolumnable_add_column( ofaITVColumnable *instance,
					GtkTreeViewColumn *column, gint column_id, const gchar *menu_label )
{
	sITVColumnable *sdata;
	sColumn *scol;
	GSimpleAction *action;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );

	scol = get_column_data_by_id( instance, sdata, column_id );
	g_return_if_fail( scol == NULL );

	/* define the column and add it to the treeview */
	gtk_tree_view_column_set_visible( column, FALSE );
	gtk_tree_view_column_set_reorderable( column, TRUE );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_column_set_sizing( column, GTK_TREE_VIEW_COLUMN_GROW_ONLY );
	gtk_tree_view_append_column( sdata->treeview, column );

	/* define new column properties */
	scol = g_new0( sColumn, 1 );
	sdata->columns_list = g_list_append( sdata->columns_list, scol );

	scol->id = column_id;
	scol->group_name = get_actions_group_name( instance, sdata );	// <name>
	scol->name = column_id_to_action_name( scol->id );				// itvcolumnable_<n>
	scol->label = g_strdup( menu_label ? menu_label : gtk_tree_view_column_get_title( column ));
	scol->column = column;
	scol->def_visible = FALSE;

	g_debug( "column_id=%u, menu_label=%s, action_group=%s, action_name=%s",
			column_id, menu_label, scol->group_name, scol->name );

	/* define a new action and attach it to the action group
	 * default visibility state is set */
	action = g_simple_action_new_stateful( scol->name, NULL, g_variant_new_boolean( FALSE ));
	g_signal_connect( action, "change-state", G_CALLBACK( on_action_changed_state ), instance );
	ofa_iactionable_set_menu_item( OFA_IACTIONABLE( instance ), scol->group_name, G_ACTION( action ), scol->label );
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

	g_debug( "%s: action_name=%s", thisfn, g_action_get_name( G_ACTION( action )));

	/* set the action state as requested (which is just the default) */
	g_simple_action_set_state( action, value);

	/* display the column or not */
	sdata = get_instance_data( instance );
	scol = get_column_data_by_action_name( instance, sdata, g_action_get_name( G_ACTION( action )));

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
 * ofa_itvcolumnable_get_column_id:
 * @instance: this #ofaITVColumnable instance.
 * @column: the #GtkTreeViewColumn column.
 *
 * Returns: the identifier of the @column.
 */
gint
ofa_itvcolumnable_get_column_id( ofaITVColumnable *instance, GtkTreeViewColumn *column )
{
	sITVColumnable *sdata;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), -1 );
	g_return_val_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ), -1 );

	sdata = get_instance_data( instance );

	return( get_column_id( instance, sdata, column ));
}

static gint
get_column_id( const ofaITVColumnable *instance, sITVColumnable *sdata, GtkTreeViewColumn *column )
{
	sColumn *scol;

	scol = get_column_data_by_ptr( instance, sdata, column );

	return( scol ? scol->id : -1 );
}

/**
 * ofa_itvcolumnable_get_column_id_renderer:
 * @instance: this #ofaITVColumnable instance.
 * @renderer: a #GtkCellRenderer.
 *
 * Returns: the identifier of the @column to which the @renderer is
 * attached.
 */
gint
ofa_itvcolumnable_get_column_id_renderer( ofaITVColumnable *instance, GtkCellRenderer *renderer )
{
	sITVColumnable *sdata;
	GList *it, *cells, *itc;
	sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), -1 );
	g_return_val_if_fail( renderer && GTK_IS_CELL_RENDERER( renderer ), -1 );

	sdata = get_instance_data( instance );

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( scol->column ));
		for( itc=cells ; itc ; itc=itc->next ){
			if( GTK_CELL_RENDERER( itc->data ) == renderer ){
				return( scol->id );
			}
		}
	}

	return( -1 );
}

/**
 * ofa_itvcolumnable_get_columns_count:
 * @instance: this #ofaITVColumnable instance.
 *
 * Returns: the count of defined columns.
 */
guint
ofa_itvcolumnable_get_columns_count( ofaITVColumnable *instance )
{
	sITVColumnable *sdata;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), 0 );

	sdata = get_instance_data( instance );

	return( g_list_length( sdata->columns_list ));
}

/**
 * ofa_itvcolumnable_get_menu:
 * @instance: this #ofaITVColumnable instance.
 *
 * Returns: the contextual menu associated with the added columns.
 */
GMenu *
ofa_itvcolumnable_get_menu( ofaITVColumnable *instance )
{
	sITVColumnable *sdata;
	gchar *group_name;
	GMenu *menu;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ), NULL );

	sdata = get_instance_data( instance );

	group_name = get_actions_group_name( instance, sdata );
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( instance ), group_name );
	g_free( group_name );

	return( menu );
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
ofa_itvcolumnable_show_columns( ofaITVColumnable *instance )
{
	sITVColumnable *sdata;
	GList *it;
	gint count;
	sColumn *scol;
	GActionGroup *action_group;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );
	sdata->visible_count = 0;

	count = read_settings( instance, sdata );

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

/**
 * ofa_itvcolumnable_propagate_visible_columns:
 * @instance: the #ofaITVColumnable instance.
 * @pages_list: a list of other #ofaITVColumnable pages.
 *
 * Propagate the columns visibility from @instance to each other page
 * of @pages_list.
 */
void
ofa_itvcolumnable_propagate_visible_columns( ofaITVColumnable *instance, GList *pages_list )
{
	static const gchar *thisfn = "ofa_itvcolumnable_propagate_visible_columns";
	sITVColumnable *sdata;
	GList *it;
	ofaITVColumnable *page;

	g_debug( "%s: instance=%p (%s), pages_list=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) pages_list );

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );

	for( it=pages_list ; it ; it=it->next ){
		page = ( ofaITVColumnable * ) it->data;
		if( page && page != instance ){
			do_propagate_visible_columns( instance, sdata, page );
		}
	}
}

static void
do_propagate_visible_columns( ofaITVColumnable *source, sITVColumnable *src_data, ofaITVColumnable *target )
{
	guint i, count;
	GtkTreeViewColumn *column, *prev;
	gint col_id, col_width;
	sITVColumnable *target_data;
	sColumn *target_scol;
	GActionGroup *action_group;

	itvcolumnable_clear_all( target );
	target_data = get_instance_data( target );
	prev = NULL;

	count = gtk_tree_view_get_n_columns( src_data->treeview );

	for( i=0 ; i<count ; ++i ){
		column = gtk_tree_view_get_column( src_data->treeview, i );
		if( gtk_tree_view_column_get_visible( column )){
			col_id = get_column_id( source, src_data, column );
			col_width = gtk_tree_view_column_get_width( column );

			target_scol = get_column_data_by_id( target, target_data, col_id );
			if( target_scol && target_scol->column ){
				gtk_tree_view_move_column_after( target_data->treeview, target_scol->column, prev );
				gtk_tree_view_column_set_fixed_width( target_scol->column, col_width );
				action_group = ofa_iactionable_get_action_group( OFA_IACTIONABLE( target ), target_scol->group_name );
				g_action_group_change_action_state( action_group, target_scol->name, g_variant_new_boolean( TRUE ));
				prev = target_scol->column;
			}
		}
	}
}

static void
itvcolumnable_clear_all( ofaITVColumnable *instance )
{
	sITVColumnable *sdata;
	GList *it;
	sColumn *scol;

	sdata = get_instance_data( instance );

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		g_simple_action_set_state( scol->action, g_variant_new_boolean( FALSE ));
	}
}

/**
 * ofa_itvcolumnable_write_columns_settings:
 * @instance: the #ofaITVColumnable instance.
 *
 * Records the current configuration in user settings.
 */
void
ofa_itvcolumnable_write_columns_settings( ofaITVColumnable *instance )
{
	static const gchar *thisfn = "ofa_itvcolumnable_write_columns_settings";
	sITVColumnable *sdata;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	write_settings( instance, sdata );
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

		sdata->name = NULL;
		sdata->columns_list = NULL;
		sdata->treeview = NULL;
		sdata->visible_count = 0;

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

	g_object_unref( sdata->treeview );
	g_free( sdata->name );
	g_free( sdata );
}

static void
free_column( sColumn *scol )
{
	g_free( scol->group_name );
	g_free( scol->name );
	g_free( scol );
}

static gchar *
get_actions_group_name( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar *group;

	group = g_strdup( my_strlen( sdata->name ) ? sdata->name : G_OBJECT_TYPE_NAME( instance ));

	return( group );
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

static sColumn *
get_column_data_by_ptr( const ofaITVColumnable *instance, sITVColumnable *sdata, GtkTreeViewColumn *column )
{
	GList *it;
	sColumn *scol;

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		if( scol->column == column ){
			return( scol );
		}
	}

	return( NULL );
}

/*
 * returns the sColumn data structure corresponding to the action name
 */
static sColumn *
get_column_data_by_action_name( ofaITVColumnable *instance, sITVColumnable *sdata, const gchar *name )
{
	return( get_column_data_by_id( instance, sdata, action_name_to_column_id( name )));
}

static gchar *
column_id_to_action_name( gint id )
{
	return( g_strdup_printf( "%s%u", st_action_prefix, id ));
}

static gint
action_name_to_column_id( const gchar *name )
{
	static guint len_prefix = 0;

	if( !len_prefix ){
		len_prefix = my_strlen( st_action_prefix );
	}
	return( atoi( name+len_prefix ));
}

/*
 * settings: pairs of <column_id;column_width;> in order of appearance
 *
 * Returns: count of found columns.
 */
static gint
read_settings( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar *settings_key;
	GList *slist, *it;
	const gchar *cstr;
	gint count, col_id, col_width;
	sColumn *scol;
	GtkTreeViewColumn *prev;
	GActionGroup *action_group;

	settings_key = g_strdup_printf( "%s-columns", sdata->name );
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
			if( my_strlen( cstr )){
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
		}
		it = it ? it->next : NULL;
	}

	ofa_settings_free_string_list( slist );
	g_free( settings_key );

	return( count );
}

static void
write_settings( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar *settings_key;
	guint i, count;
	GString *str;
	gint col_id, col_width;
	GtkTreeViewColumn *column;

	if( sdata->treeview ){

		str = g_string_new( "" );
		settings_key = g_strdup_printf( "%s-columns", sdata->name );
		count = gtk_tree_view_get_n_columns( sdata->treeview );

		for( i=0 ; i<count ; ++i ){
			column = gtk_tree_view_get_column( sdata->treeview, i );
			if( gtk_tree_view_column_get_visible( column )){
				col_id = get_column_id( instance, sdata, column );
				col_width = gtk_tree_view_column_get_width( column );
				g_string_append_printf( str, "%d;%d;", col_id, col_width );
			}
		}

		ofa_settings_user_set_string( settings_key, str->str );

		g_free( settings_key );
		g_string_free( str, TRUE );
	}
}
