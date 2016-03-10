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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"

#include "ui/ofa-itreeview-display.h"

#define ITREEVIEW_DISPLAY_LAST_VERSION  1

/* a data structure attached to the instance
 */
#define ITREEVIEW_DISPLAY_DATA          "ofa-itreeview-display-data"

typedef struct {
	gboolean            initialization;
	GSimpleActionGroup *action_group;
	GList              *columns_list;
	gchar              *settings_key;
}
	sITreeviewDisplay;

/* the data stored for each displayable column
 */
typedef struct {
	guint              id;
	gchar             *name;			/* internal name of the action */
	gchar             *label;
	gboolean           visible;
	GtkTreeViewColumn *column;
}
	sColumn;

/* signals defined here
 */
enum {
	TOGGLED = 0,
	N_SIGNALS
};

static guint        st_initializations      = 0;	/* interface initialization count */
static guint        st_signals[ N_SIGNALS ] = { 0 };
static const gchar *st_prefix               = "itreeview-display";
static const gchar *st_resource_arrow_down  = "/org/trychlos/openbook/ui/ofa-itreeview-display-arrow-down.png";

static GType              register_type( void );
static void               interface_base_init( ofaITreeviewDisplayInterface *klass );
static void               interface_base_finalize( ofaITreeviewDisplayInterface *klass );
static guint              itreeview_display_get_interface_version( const ofaITreeviewDisplay *instance );
static gboolean           has_column_id( GList *list, gint column_id );
static GtkWidget         *setup_button( const ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata );
static void               on_action_change_state( GSimpleAction *action, GVariant *value, ofaITreeviewDisplay *instance );
static void               update_settings( ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata );
static sColumn           *get_column_by_id( const ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata, guint id );
static sColumn           *get_column_by_name( ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata, const gchar *name );
static gchar             *id_to_name( guint id );
static guint              name_to_id( const gchar *name );
static sITreeviewDisplay *get_instance_data( const ofaITreeviewDisplay *instance );
static void               on_instance_finalized( sITreeviewDisplay *sdata, void *instance );
static void               free_column( sColumn *scol );

/**
 * ofa_itreeview_display_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itreeview_display_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itreeview_display_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itreeview_display_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaITreeviewDisplayInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITreeviewDisplay", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaITreeviewDisplayInterface *klass )
{
	static const gchar *thisfn = "ofa_itreeview_display_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->get_interface_version = itreeview_display_get_interface_version;

		/**
		 * ofaITreeviewDisplay::itreeview-display-toggled:
		 *
		 * This signal is sent when the visibility state of a column
		 * has been toggled.
		 * @column_id: the column identifier.
		 * @visible: the new visibility status.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaITreeviewDisplay *instance,
		 * 						gint               column_id,
		 * 						gboolean           visible,
		 * 						gpointer           user_data );
		 */
		st_signals[ TOGGLED ] = g_signal_new_class_handler(
					"ofa-toggled",
					OFA_TYPE_ITREEVIEW_DISPLAY,
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
interface_base_finalize( ofaITreeviewDisplayInterface *klass )
{
	static const gchar *thisfn = "ofa_itreeview_display_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

static guint
itreeview_display_get_interface_version( const ofaITreeviewDisplay *instance )
{
	return( 1 );
}

/**
 * ofa_itreeview_display_get_interface_last_version:
 * @instance: this #ofaITreeviewDisplay instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itreeview_display_get_interface_last_version( const ofaITreeviewDisplay *instance )
{
	return( ITREEVIEW_DISPLAY_LAST_VERSION );
}

/**
 * ofa_itreeview_display_add_column:
 * @instance: the #ofaITreeviewDisplay instance.
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
ofa_itreeview_display_add_column( ofaITreeviewDisplay *instance, GtkTreeViewColumn *column, guint id )
{
	sITreeviewDisplay *sdata;
	sColumn *scol;
	GSimpleAction *action;

	g_return_if_fail( instance && OFA_IS_ITREEVIEW_DISPLAY( instance ));

	sdata = get_instance_data( instance );
	scol = get_column_by_id( instance, sdata, id );
	g_return_if_fail( scol == NULL );

	/* define new column properties */
	scol = g_new0( sColumn, 1 );
	sdata->columns_list = g_list_append( sdata->columns_list, scol );

	scol->id = id;
	scol->name = id_to_name( id );
	scol->column = column;

	if( OFA_ITREEVIEW_DISPLAY_GET_INTERFACE( instance )->get_label ){
		scol->label = OFA_ITREEVIEW_DISPLAY_GET_INTERFACE( instance )->get_label( instance, id );
	}

	scol->visible = TRUE;
	if( my_strlen( scol->label )){
		if( OFA_ITREEVIEW_DISPLAY_GET_INTERFACE( instance )->get_def_visible ){
			scol->visible = OFA_ITREEVIEW_DISPLAY_GET_INTERFACE( instance )->get_def_visible( instance, id );
		}
	}

	/* define a new action and attach it to the action group
	 * default visibility state is set */
	action = g_simple_action_new_stateful(
					scol->name, NULL, g_variant_new_boolean( scol->visible ));
	g_signal_connect( action, "change-state", G_CALLBACK( on_action_change_state ), instance );
	g_action_map_add_action( G_ACTION_MAP( sdata->action_group ), G_ACTION( action ));
	g_object_unref( action );
}

/**
 * ofa_itreeview_display_init_visible:
 * @instance: this #ofaITreeviewDisplay instance.
 * @key: the settings key where displayed columns are stored.
 *
 * Initialize the visible columns on treeview initialization.
 *
 * This make sure the #GtkTreeViewColumn columns previously defined
 * are visible, depending of their default visibility state, and the
 * value read from the settings.
 */
void
ofa_itreeview_display_init_visible( const ofaITreeviewDisplay *instance, const gchar *key )
{
	static const gchar *thisfn = "ofa_itreeview_display_init_visible";
	sITreeviewDisplay *sdata;
	GList *list, *it;
	sColumn *scol;
	gboolean visible;

	g_return_if_fail( instance && OFA_IS_ITREEVIEW_DISPLAY( instance ));

	sdata = get_instance_data( instance );
	sdata->initialization = TRUE;
	g_free( sdata->settings_key );
	sdata->settings_key = g_strdup( key );

	list = ofa_settings_user_get_uint_list( sdata->settings_key );

	for( it=sdata->columns_list ; it ; it=it->next ){
		scol = ( sColumn * ) it->data;
		visible = list ? has_column_id( list, scol->id ) : scol->visible;
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
 * ofa_itreeview_display_get_visible:
 * @instance: this #ofaITreeviewDisplay instance.
 * @id: a column identifier.
 *
 * Returns: whether the column is visible.
 * Returns: %TRUE if the @id column has not been previously defined
 *  (the visible status of the column cannot be toggled by the user).
 */
gboolean
ofa_itreeview_display_get_visible( const ofaITreeviewDisplay *instance, guint id )
{
	sITreeviewDisplay *sdata;
	sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITREEVIEW_DISPLAY( instance ), FALSE );

	sdata = get_instance_data( instance );
	scol = get_column_by_id( instance, sdata, id );
	if( scol && scol->column ){
		return( gtk_tree_view_column_get_visible( scol->column ));
	}

	return( TRUE );
}

/**
 * ofa_itreeview_display_set_visible:
 * @instance: this #ofaITreeviewDisplay instance.
 * @id: a column identifier.
 * @visible: whether the column must be set visible or not.
 *
 * Toggle the column visibility.
 */
void
ofa_itreeview_display_set_visible( const ofaITreeviewDisplay *instance, guint id, gboolean visible )
{
	sITreeviewDisplay *sdata;
	gchar *name;

	g_return_if_fail( instance && OFA_IS_ITREEVIEW_DISPLAY( instance ));

	sdata = get_instance_data( instance );
	name = id_to_name( id );
	g_action_group_change_action_state(
			G_ACTION_GROUP( sdata->action_group ), name, g_variant_new_boolean( visible ));
	g_free( name );
}

/**
 * ofa_itreeview_display_attach_menu_button:
 * @instance: this #ofaITreeviewDisplay instance.
 * @parent: a #GtkContainer to which attach the menu button.
 *
 * Attach a menu button to the @parent container.
 * The menu contains one toggle action for each previously defined
 * column.
 */
void
ofa_itreeview_display_attach_menu_button( const ofaITreeviewDisplay *instance, GtkContainer *parent )
{
	sITreeviewDisplay *sdata;
	GtkWidget *button;

	g_return_if_fail( instance && OFA_IS_ITREEVIEW_DISPLAY( instance ));

	sdata = get_instance_data( instance );
	button = setup_button( instance, sdata );
	gtk_container_add( parent, button );
}

/*
 * Defines a new menu button with the attached menu, and returns it
 */
static GtkWidget *
setup_button( const ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata )
{
	GList *it;
	sColumn *scol;
	GtkWidget *button, *box, *label, *image;
	GMenu *menu;
	gchar *action_name;
	GMenuItem *item;

	button = gtk_menu_button_new();

	/* setup menu button */
	gtk_widget_set_halign( button, GTK_ALIGN_START );
	gtk_menu_button_set_direction( GTK_MENU_BUTTON( button ), GTK_ARROW_DOWN );
	gtk_menu_button_set_use_popover( GTK_MENU_BUTTON( button ), FALSE );
	gtk_menu_button_set_align_widget( GTK_MENU_BUTTON( button ), NULL );

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 6 );
	gtk_container_add( GTK_CONTAINER( button ), box );

	label = gtk_label_new_with_mnemonic( _( "_Columns selection" ));
	gtk_box_pack_start( GTK_BOX( box ), label, FALSE, TRUE, 0 );

	image = gtk_image_new_from_resource( st_resource_arrow_down );
	gtk_box_pack_start( GTK_BOX( box ), image, FALSE, TRUE, 0 );

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

/*
 * a request has been made to chage the state of the action
 * this request may have been sent from our code (see init_visible())
 * or after the action has been activated from the UI
 *
 * If we are not during init_visible(), and the key has been set, then
 * settings are updated with displayed columns
 */
static void
on_action_change_state( GSimpleAction *action, GVariant *value, ofaITreeviewDisplay *instance )
{
	gboolean visible;
	sITreeviewDisplay *sdata;
	sColumn *scol;

	/* set the action state as requested */
	g_simple_action_set_state( action, value);

	/* display the column or not */
	visible = g_variant_get_boolean( value );
	sdata = get_instance_data( instance );
	scol = get_column_by_name( instance, sdata, g_action_get_name( G_ACTION( action )));
	if( scol->column ){
		gtk_tree_view_column_set_visible( scol->column, visible );
		/*g_debug( "ofa_itreeview_display_on_action_change_state: %s %s", scol->label, visible ? "True":"False" );*/
		g_signal_emit_by_name( instance, "ofa-toggled", scol->id, visible );
	}

	if( !sdata->initialization ){
		update_settings( instance, sdata );
	}
}

static void
update_settings( ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata )
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

/*
 * returns the sColumn data structure defined for the column
 * or %NULL if the column has not yet been defined
 */
static sColumn *
get_column_by_id( const ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata, guint id )
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
get_column_by_name( ofaITreeviewDisplay *instance, sITreeviewDisplay *sdata, const gchar *name )
{
	return( get_column_by_id( instance, sdata, name_to_id( name )));
}

static gchar *
id_to_name( guint id )
{
	return( g_strdup_printf( "%s%u", st_prefix, id ));
}

static guint
name_to_id( const gchar *name )
{
	static guint len_prefix = 0;

	if( !len_prefix ){
		len_prefix = my_strlen( st_prefix );
	}
	return( atoi( name+len_prefix ));
}

/*
 * returns the sITreeviewDisplay data structure attached to the instance
 */
static sITreeviewDisplay *
get_instance_data( const ofaITreeviewDisplay *instance )
{
	sITreeviewDisplay *sdata;

	sdata = ( sITreeviewDisplay * ) g_object_get_data( G_OBJECT( instance ), ITREEVIEW_DISPLAY_DATA );

	if( !sdata ){
		sdata = g_new0( sITreeviewDisplay, 1 );
		sdata->initialization = FALSE;
		sdata->action_group = g_simple_action_group_new();
		g_object_set_data( G_OBJECT( instance ), ITREEVIEW_DISPLAY_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sITreeviewDisplay *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_itreeview_display_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_object_unref( sdata->action_group );
	g_list_free_full( sdata->columns_list, ( GDestroyNotify ) free_column );
	g_free( sdata->settings_key );

	g_free( sdata );
}

static void
free_column( sColumn *scol )
{
	g_free( scol->name );
	g_free( scol->label );
	g_free( scol );
}
