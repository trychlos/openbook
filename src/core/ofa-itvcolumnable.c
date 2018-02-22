/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include <stdarg.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-iactionable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"

#define ITVCOLUMNABLE_LAST_VERSION  1

/* a data structure attached to the instance
 */
#define ITVCOLUMNABLE_DATA          "ofa-itvcolumnable-data"

typedef struct {
	gchar             *name;
	ofaIGetter        *getter;
	GList             *columns_list;
	GtkTreeView       *treeview;
	gint               visible_count;
	GHashTable        *twins;
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
	gboolean           invisible;
	ofeBoxType         type;
}
	sColumn;

/* the description of a twin group
 * this is the structure attached to the 'twins' hash, indexed by name
 */
typedef struct {
	ofaITVColumnable  *instance;
	gchar             *name;
	gint               width;
	GList             *columns;
	GList             *widgets;
}
	sTwinGroup;

/* the data attached to each column of a twin group
 */
typedef struct {
	gint               col_id;
	GtkTreeViewColumn *column;
	gint               x;
}
	sTwinColumn;

/* signals defined here
 */
enum {
	TOGGLED = 0,
	WIDTH,
	N_SIGNALS
};

#define ITVCOLUMNABLE_MIN_WIDTH                20

static guint        st_signals[ N_SIGNALS ] = { 0 };

static guint        st_initializations      =   0;	/* interface initialization count */

static const gchar *st_action_prefix        = "itvcolumnable_";

static GType           register_type( void );
static void            interface_base_init( ofaITVColumnableInterface *klass );
static void            interface_base_finalize( ofaITVColumnableInterface *klass );
static void            on_action_changed_state( GSimpleAction *action, GVariant *value, ofaITVColumnable *instance );
static gint            get_column_id( const ofaITVColumnable *instance, sITVColumnable *sdata, GtkTreeViewColumn *column );
static void            enable_action( ofaITVColumnable *instance, sITVColumnable *sdata, sColumn *scol );
static void            do_propagate_visible_columns( ofaITVColumnable *source, sITVColumnable *sdata, ofaITVColumnable *target );
static void            itvcolumnable_clear_all( ofaITVColumnable *instance );
static gboolean        twins_group_init( ofaITVColumnable *self, sITVColumnable *sdata, const gchar *name, GList *col_ids );
static void            tvcolumn_on_width_changed( GtkTreeViewColumn *column, GParamSpec *spec, sTwinGroup *twgroup );
static void            tvcolumn_on_xoffset_changed( GtkTreeViewColumn *column, GParamSpec *spec, sTwinGroup *twgroup );
static gchar          *get_actions_group_name( const ofaITVColumnable *instance, sITVColumnable *sdata );
static sColumn        *get_column_data_by_id( const ofaITVColumnable *instance, sITVColumnable *sdata, gint id );
static sColumn        *get_column_data_by_ptr( const ofaITVColumnable *instance, sITVColumnable *sdata, GtkTreeViewColumn *column );
static sColumn        *get_column_data_by_action_name( ofaITVColumnable *instance, sITVColumnable *sdata, const gchar *name );
static gchar          *column_id_to_action_name( gint id );
static gint            action_name_to_column_id( const gchar *name );
static guint           check_min_width( guint candidate_width );
static gint            read_settings( const ofaITVColumnable *instance, sITVColumnable *sdata );
static void            write_settings( const ofaITVColumnable *instance, sITVColumnable *sdata );
static sITVColumnable *get_instance_data( const ofaITVColumnable *instance );
static void            on_instance_finalized( sITVColumnable *sdata, void *instance );
static void            free_column( sColumn *scol );
static void            free_twin_group( sTwinGroup *twgroup );
static void            free_twin_column( sTwinColumn *twcol );

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

		/**
		 * ofaITVColumnable::ofa-twinwidth:
		 *
		 * This signal is sent when a twin-group column's width has changed.
		 * @column_id: the column identifier of the changed column.
		 * @name: the name of the twin group the column belongs to.
		 * @new_width: the new width of the column.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaITVColumnable *instance,
		 * 						gint            column_id,
		 * 						const gchar    *name,
		 * 						gint            new_width,
		 * 						gpointer        user_data );
		 */
		st_signals[ WIDTH ] = g_signal_new_class_handler(
					"ofa-twinwidth",
					OFA_TYPE_ITVCOLUMNABLE,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					2,
					G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT );
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
 *   toggable column); in other words, actions created by this interface
 *   are scoped to this @name
 *   (see also https://wiki.gnome.org/HowDoI/GAction).
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
 * ofa_itvcolumnable_set_getter:
 * @instance: the #ofaITVColumnable instance.
 * @getter: a #ofaIGetter instance.
 *
 * Set the @getter.
 *
 * This is needed in order to be able to access to user settings.
 */
void
ofa_itvcolumnable_set_getter( ofaITVColumnable *instance, ofaIGetter *getter )
{
	sITVColumnable *sdata;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = get_instance_data( instance );

	sdata->getter = getter;
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
 *
 * The treeview must have been set prior to adding column
 * (see #ofa_itvcolumnable_set_treeview() method).
 */
void
ofa_itvcolumnable_add_column( ofaITVColumnable *instance,
					GtkTreeViewColumn *column, gint column_id, const gchar *menu_label, ofeBoxType type )
{
	static const gchar *thisfn = "ofa_itvcolumnable_add_column";
	sITVColumnable *sdata;
	sColumn *scol;
	GSimpleAction *action;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );
	g_return_if_fail( sdata->treeview != NULL );

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
	scol->invisible = FALSE;
	scol->type = type;

	g_debug( "%s: column_id=%u, menu_label=%s, action_group=%s, action_name=%s, type=%u",
			thisfn, column_id, menu_label, scol->group_name, scol->name, type );

	/* define a new action and attach it to the action group
	 * default visibility state is cleared */
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

	/* set the action state as requested (which is just the default) */
	g_simple_action_set_state( action, value);

	/* display the column or not */
	sdata = get_instance_data( instance );
	scol = get_column_data_by_action_name( instance, sdata, g_action_get_name( G_ACTION( action )));

	if( scol->column ){
		visible = g_variant_get_boolean( value );
		gtk_tree_view_column_set_visible( scol->column, visible );
		g_debug( "%s: action_name=%s, column=%s, visible=%s",
				thisfn, g_action_get_name( G_ACTION( action )), scol->label, visible ? "True":"False" );
		g_signal_emit_by_name( instance, "ofa-toggled", scol->id, visible );
		sdata->visible_count += ( visible ? 1 : -1 );
		//g_debug( "%s: visible_count=%d", thisfn, sdata->visible_count );

		/* be sure that the last visible column will not be disabled */
		for( it=sdata->columns_list ; it ; it=it->next ){
			scol = ( sColumn * ) it->data;
			enable_action( instance, sdata, scol );
		}
	}
}

/**
 * ofa_itvcolumnable_get_column:
 * @instance: this #ofaITVColumnable instance.
 * @column_id: the #GtkTreeViewColumn column identifier.
 *
 * Returns: the #GtkTreeViewColumn @column.
 */
GtkTreeViewColumn *
ofa_itvcolumnable_get_column( ofaITVColumnable *instance, gint column_id )
{
	static const gchar *thisfn = "ofa_itvcolumnable_get_column";
	sITVColumnable *sdata;
	sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), NULL );

	sdata = get_instance_data( instance );
	scol = get_column_data_by_id( instance, sdata, column_id );

	if( scol ){
		return( scol->column );

	} else {
		g_warning( "%s: unknwon column_id=%u", thisfn, column_id );
	}

	return( NULL );
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
 * ofa_itvcolumnable_get_menu_label:
 * @instance: the #ofaITVColumnable instance.
 * @column: the #GtkTreeViewColumn.
 *
 * Returns: the menu label associated with the @column, or %NULL.
 */
const gchar *
ofa_itvcolumnable_get_menu_label( ofaITVColumnable *instance, GtkTreeViewColumn *column )
{
	sITVColumnable *sdata;
	sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), NULL );
	g_return_val_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ), NULL );

	sdata = get_instance_data( instance );
	scol = get_column_data_by_ptr( instance, sdata, column );

	return( scol ? scol->label : NULL );
}

/**
 * ofa_itvcolumnable_get_column_type:
 * @instance: the #ofaITVColumnable instance.
 * @column: the #GtkTreeViewColumn.
 *
 * Returns: the #ofeBoxType associated with the @column, or zero.
 */
ofeBoxType
ofa_itvcolumnable_get_column_type( ofaITVColumnable *instance, GtkTreeViewColumn *column )
{
	sITVColumnable *sdata;
	sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), 0 );
	g_return_val_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ), 0 );

	sdata = get_instance_data( instance );
	scol = get_column_data_by_ptr( instance, sdata, column );

	return( scol ? scol->type : 0 );
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
	static const gchar *thisfn = "ofa_itvcolumnable_set_default_column";
	sITVColumnable *sdata;
	sColumn *scol;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ));

	sdata = get_instance_data( instance );
	scol = get_column_data_by_id( instance, sdata, column_id );

	if( scol ){
		scol->def_visible = TRUE;

	} else {
		g_warning( "%s: unknown column_id=%u", thisfn, column_id );
	}
}

/**
 * ofa_itvcolumnable_set_invisible:
 * @instance: the #ofaITVColumnable instance.
 * @column_id: the identifier of the #GtkTreeViewColumn column.
 * @invisible: whether this column should be invisible.
 *
 * Determines if a column can be made visible.
 *
 * When invisible, the column will never be displayed.
 * The corresponding action will be disabled.
 */
void
ofa_itvcolumnable_set_invisible( ofaITVColumnable *instance, gint column_id, gboolean invisible )
{
	static const gchar *thisfn = "ofa_itvcolumnable_set_invisible";
	sITVColumnable *sdata;
	sColumn *scol;
	GActionGroup *action_group;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );
	scol = get_column_data_by_id( instance, sdata, column_id );

	if( scol ){
		scol->invisible = invisible;

		/* make the column not visible when disabled */
		if( invisible ){
			action_group = ofa_iactionable_get_action_group( OFA_IACTIONABLE( instance ), scol->group_name );
			g_action_group_change_action_state( action_group, scol->name, g_variant_new_boolean( FALSE ));
		}

		/* enable/disable the action */
		enable_action( instance, sdata, scol );

	} else {
		g_warning( "%s: unknwon column_id=%u", thisfn, column_id );
	}
}

/*
 * The action which let the user toggle the visiblity state of a GtkTreeViewColumn
 * is always enabled, unless:
 * - the column is the last visible
 * - the column has been set invisible.
 */
static void
enable_action( ofaITVColumnable *instance, sITVColumnable *sdata, sColumn *scol )
{
	gboolean enabled;

	enabled = TRUE;

	if( sdata->visible_count == 1 && gtk_tree_view_column_get_visible( scol->column )){
		enabled = FALSE;

	} else if( scol->invisible ){
		enabled = FALSE;
	}

	g_simple_action_set_enabled( scol->action, enabled );
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
 * ofa_itvcolumnable_show_columns_all:
 * @instance: the #ofaITVColumnable instance.
 *
 * Make all columns visible.
 */
void
ofa_itvcolumnable_show_columns_all( ofaITVColumnable *instance )
{
	sITVColumnable *sdata;
	GList *it;
	sColumn *scol;
	GActionGroup *action_group;

	g_return_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ) && OFA_IS_IACTIONABLE( instance ));

	sdata = get_instance_data( instance );
	sdata->visible_count = 0;

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		action_group = ofa_iactionable_get_action_group( OFA_IACTIONABLE( instance ), scol->group_name );
		g_action_group_change_action_state( action_group, scol->name, g_variant_new_boolean( TRUE ));
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
			col_width = check_min_width( gtk_tree_view_column_get_width( column ));

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

/**
 * ofa_itvcolumnable_twins_group_new:
 * @instance: the #ofaITVColumnable instance.
 * @name: the name of this twins group.
 *
 * Define a new twins group with @name identifier, and set the specified
 * columns as twins.
 *
 * Inside of a twins group:
 * - all columns keep the same width.
 *
 * Other widgets may later be added to this twins group, and wil share
 * the same width-keeper feature.
 *
 * The column identifiers must be specified as a -1 terminated list.
 *
 * Returns: %TRUE if the twins group has been successfully defined.
 */
gboolean
ofa_itvcolumnable_twins_group_new( ofaITVColumnable *instance, const gchar *name, ... )
{
	static const gchar *thisfn = "ofa_itvcolumnable_twins_group_new";
	sITVColumnable *sdata;
	GList *ids;
	va_list ap;
	gint col_id;
	gboolean ok;

	g_debug( "%s: instance=%p (%s), name=%s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), name );

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), FALSE );
	g_return_val_if_fail( my_strlen( name ), FALSE );

	sdata = get_instance_data( instance );

	if( g_hash_table_lookup( sdata->twins, name )){
		g_warning( "%s: trying to double-defined the '%s' twins group", thisfn, name );
		return( FALSE );
	}

	ids = NULL;
	va_start( ap, name );
	while( TRUE ){
		col_id = va_arg( ap, gint );
		if( col_id == -1 ){
			break;
		}
		ids = g_list_prepend( ids, GINT_TO_POINTER( col_id ));
	}
	va_end( ap );
	ok = twins_group_init( instance, sdata, name, ids );
	g_debug( "%s: group=%s defined with count=%d columns", thisfn, name, g_list_length( ids ));
	g_list_free( ids );

	return( ok );
}

static gboolean
twins_group_init( ofaITVColumnable *self, sITVColumnable *sdata, const gchar *name, GList *col_ids )
{
	sTwinGroup *twgroup;
	sTwinColumn *twcol;
	GList *it;
	gint column_id, width;
	GtkTreeViewColumn *column;

	twgroup = g_new0( sTwinGroup, 1 );
	twgroup->instance = self;
	twgroup->name = g_strdup( name );
	twgroup->columns = NULL;
	twgroup->widgets = NULL;
	twgroup->width = 0;

	for( it=col_ids ; it ; it=it->next ){
		column_id = GPOINTER_TO_INT( it->data );
		column = ofa_itvcolumnable_get_column( self, column_id );
		g_return_val_if_fail( column && GTK_IS_TREE_VIEW_COLUMN( column ), FALSE );

		g_signal_connect( column, "notify::width", G_CALLBACK( tvcolumn_on_width_changed ), twgroup );
		g_signal_connect( column, "notify::x-offset", G_CALLBACK( tvcolumn_on_xoffset_changed ), twgroup );

		twcol = g_new0( sTwinColumn, 1 );
		twcol->col_id = column_id;
		twcol->column = column;
		twcol->x = gtk_tree_view_column_get_x_offset( column );
		twgroup->columns = g_list_prepend( twgroup->columns, twcol );

		width = gtk_tree_view_column_get_width( column );
		if( twgroup->width < width ){
			twgroup->width = width;
		}
	}

	g_hash_table_insert( sdata->twins, g_strdup( name ), twgroup );

	return( TRUE );
}

static void
tvcolumn_on_width_changed( GtkTreeViewColumn *column, GParamSpec *spec, sTwinGroup *twgroup )
{
	static const gchar *thisfn = "ofa_itvcolumnable_tvolumn_on_width_changed";
	GList *it;
	gint new_width, col_id;
	sTwinColumn *twcol;
	GtkWidget *widget;

	col_id = -1;
	new_width = gtk_tree_view_column_get_width( column );
	if( new_width != twgroup->width ){
		for( it=twgroup->columns ; it ; it=it->next ){
			twcol = ( sTwinColumn * ) it->data;
			if( twcol->column != column ){
				gtk_tree_view_column_set_fixed_width( twcol->column, new_width );
			} else {
				col_id = twcol->col_id;
			}
		}
		g_debug( "%s: col_id=%d, name=%s, width=%d", thisfn, col_id, twgroup->name, new_width );
		for( it=twgroup->widgets ; it ; it=it->next ){
			widget = ( GtkWidget * ) it->data;
			if( GTK_IS_WIDGET( widget )){
				g_debug( "gtk_widget_set_size_request for widget=%p, width=%d", widget, new_width );
				gtk_widget_set_size_request( widget, new_width, -1 );
			}
		}
		if( twgroup->instance ){
			g_signal_emit_by_name( twgroup->instance, "ofa-twinwidth", col_id, twgroup->name, new_width );
		}
		twgroup->width = new_width;
	}
}

static void
tvcolumn_on_xoffset_changed( GtkTreeViewColumn *column, GParamSpec *spec, sTwinGroup *group )
{
	//g_debug( "tvcolumn_on_xoffset_changed: column=%p", column );
}

/**
 * ofa_itvcolumnable_twins_group_add_widget:
 * @instance: the #ofaITVColumnable instance.
 * @name: the name of this twins group.
 * @widget: a #GtkWidget to be added to the previously defined @name group.
 *
 * Returns: %TRUE if the @widget has been successfully added to the @name
 * group.
 */
gboolean
ofa_itvcolumnable_twins_group_add_widget( ofaITVColumnable *instance, const gchar *name, GtkWidget *widget )
{
	static const gchar *thisfn = "ofa_itvcolumnable_twins_group_add_widget";
	sITVColumnable *sdata;
	sTwinGroup *twgroup;

	g_debug( "%s: instance=%p (%s), name=%s, widget=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), name,
			( void * ) widget, G_OBJECT_TYPE_NAME( widget ));

	g_return_val_if_fail( instance && OFA_IS_ITVCOLUMNABLE( instance ), FALSE );
	g_return_val_if_fail( my_strlen( name ), FALSE );
	g_return_val_if_fail( widget && GTK_IS_WIDGET( widget ), FALSE );

	sdata = get_instance_data( instance );
	twgroup = g_hash_table_lookup( sdata->twins, name );

	if( !twgroup ){
		g_warning( "%s: trying to add a widget to undefined '%s' twins group", thisfn, name );
		return( FALSE );
	}

	twgroup->widgets = g_list_prepend( twgroup->widgets, widget );

	return( TRUE );
}

static gchar *
get_actions_group_name( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	gchar *group;

	group = g_strdup_printf(
				"%s-ITVColumnable",
				my_strlen( sdata->name ) ? sdata->name : G_OBJECT_TYPE_NAME( instance ));

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

static guint
check_min_width( guint candidate_width )
{
	guint width;

	width = ( candidate_width < ITVCOLUMNABLE_MIN_WIDTH ? ITVCOLUMNABLE_MIN_WIDTH : candidate_width );

	return( width );
}

/*
 * settings: pairs of <column_id;column_width;> in order of appearance
 *
 * Returns: count of found columns.
 */
static gint
read_settings( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	myISettings *settings;
	gchar *key;
	GList *strlist, *it;
	const gchar *cstr;
	gint count, col_id, col_width;
	sColumn *scol;
	GtkTreeViewColumn *prev;
	GActionGroup *action_group;

	settings = ofa_igetter_get_user_settings( sdata->getter );
	key = g_strdup_printf( "%s-columns", sdata->name );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	count = 0;
	prev = NULL;
	it = strlist;

	while( it ){
		cstr = it ? ( const gchar * ) it->data : NULL;
		if( my_strlen( cstr )){
			col_id = atoi( cstr );
			it = it->next;
			cstr = it ? it->data : NULL;
			if( my_strlen( cstr )){
				col_width = check_min_width( atoi( cstr ));
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

	my_isettings_free_string_list( settings, strlist );
	g_free( key );

	return( count );
}

static void
write_settings( const ofaITVColumnable *instance, sITVColumnable *sdata )
{
	myISettings *settings;
	gchar *key;
	guint i, count;
	GString *str;
	gint col_id, col_width;
	GtkTreeViewColumn *column;

	if( sdata->treeview ){

		settings = ofa_igetter_get_user_settings( sdata->getter );
		key = g_strdup_printf( "%s-columns", sdata->name );
		count = gtk_tree_view_get_n_columns( sdata->treeview );
		str = g_string_new( "" );

		for( i=0 ; i<count ; ++i ){
			column = gtk_tree_view_get_column( sdata->treeview, i );
			if( gtk_tree_view_column_get_visible( column )){
				col_id = get_column_id( instance, sdata, column );
				col_width = gtk_tree_view_column_get_width( column );
				g_string_append_printf( str, "%d;%d;", col_id, col_width );
			}
		}

		my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str->str );

		g_free( key );
		g_string_free( str, TRUE );
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

		sdata->name = NULL;
		sdata->columns_list = NULL;
		sdata->treeview = NULL;
		sdata->visible_count = 0;
		sdata->twins = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, ( GDestroyNotify ) free_twin_group );

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
	g_hash_table_remove_all( sdata->twins );
	g_hash_table_unref( sdata->twins );
	g_clear_object( &sdata->treeview );
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

static void
free_twin_group( sTwinGroup *twgroup )
{
	g_free( twgroup->name );
	g_list_free_full( twgroup->columns, ( GDestroyNotify ) free_twin_column );
	g_list_free( twgroup->widgets );
	g_free( twgroup );
}

static void
free_twin_column( sTwinColumn *twcolumn )
{
	g_free( twcolumn );
}
