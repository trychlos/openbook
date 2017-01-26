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

#include "my/my-isettings.h"
#include "my/my-utils.h"

#define ISETTINGS_LAST_VERSION         1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( myISettingsInterface *klass );
static void  interface_base_finalize( myISettingsInterface *klass );

/**
 * my_isettings_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_isettings_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_isettings_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_isettings_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myISettingsInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "myISettings", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( myISettingsInterface *klass )
{
	static const gchar *thisfn = "my_isettings_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myISettingsInterface *klass )
{
	static const gchar *thisfn = "my_isettings_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_isettings_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_isettings_get_interface_last_version( void )
{
	return( ISETTINGS_LAST_VERSION );
}

/**
 * my_isettings_get_interface_version:
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
my_isettings_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_ISETTINGS );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myISettingsInterface * ) iface )->get_interface_version ){
		version = (( myISettingsInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myISettings::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_isettings_get_keyfile:
 * @instance: this #myISettings instance.
 *
 * Returns: the keyfile of the underlying settings file.
 *
 * The returned reference is owned by the implementation, and should
 * not be released by the caller.
 */
GKeyFile *
my_isettings_get_keyfile( const myISettings *instance )
{
	static const gchar *thisfn = "my_isettings_get_keyfile";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_keyfile ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_keyfile( instance ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_keyfile()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_get_filename:
 * @instance: this #myISettings instance.
 *
 * Returns: the filename of the underlying settings file, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
my_isettings_get_filename( const myISettings *instance )
{
	static const gchar *thisfn = "my_isettings_get_filename";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_filename ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_filename( instance ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_filename()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_get_groups:
 * @instance: this #myISettings instance.
 *
 * Returns: the list of groups defined in the @group, as a #GList which
 * should be #my_isettings_free_groups() by the caller.
 */
GList *
my_isettings_get_groups( const myISettings *instance )
{
	static const gchar *thisfn = "my_isettings_get_groups";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_groups ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_groups( instance ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_groups()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_free_groups:
 * @instance: this #myISettings instance.
 * @groups_list: the list of groups to be freed, as returned by
 *  #my_isettings_get_groups().
 *
 * Frees the provided @groups_list.
 */
void
my_isettings_free_groups( myISettings *instance, GList *groups_list )
{
	static const gchar *thisfn = "my_isettings_free_groups";

	g_debug( "%s: instance=%p, groups_list=%p", thisfn, ( void * ) instance, ( void * ) groups_list );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( groups_list );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->free_groups ){
		MY_ISETTINGS_GET_INTERFACE( instance )->free_groups( instance, groups_list );

	} else {
		g_list_free_full( groups_list, ( GDestroyNotify ) g_free );
	}
}

/**
 * my_isettings_remove_group:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 *
 * Removes the @group from the settings file.
 */
void
my_isettings_remove_group( myISettings *instance, const gchar *group )
{
	static const gchar *thisfn = "my_isettings_remove_group";

	g_debug( "%s: instance=%p, group=%s", thisfn, ( void * ) instance, group );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->remove_group ){
		MY_ISETTINGS_GET_INTERFACE( instance )->remove_group( instance, group );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'remove_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_get_keys:
 * @instance: this #myISettings instance.
 * @group: the group name.
 *
 * Returns: the list of keys defined in the @group, as a #GList which
 * should be #my_isettings_free_keys() by the caller.
 */
GList *
my_isettings_get_keys( const myISettings *instance, const gchar *group )
{
	static const gchar *thisfn = "my_isettings_get_keys";

	g_debug( "%s: instance=%p, group=%s", thisfn, ( void * ) instance, group );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_keys ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_keys( instance, group ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_keys()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_free_keys:
 * @instance: this #myISettings instance.
 * @keys_list: the list of keys to be freed, as returned by
 *  #my_isettings_get_keys().
 *
 * Frees the provided @keys_list.
 */
void
my_isettings_free_keys( myISettings *instance, GList *keys_list )
{
	static const gchar *thisfn = "my_isettings_free_keys";

	g_debug( "%s: instance=%p, keys_list=%p", thisfn, ( void * ) instance, ( void * ) keys_list );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( keys_list );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->free_keys ){
		MY_ISETTINGS_GET_INTERFACE( instance )->free_keys( instance, keys_list );

	} else {
		g_list_free_full( keys_list, ( GDestroyNotify ) g_free );
	}
}

/**
 * my_isettings_has_key:
 * @instance: this #myISettings instance.
 * @group: the group name.
 * @key: the searched key.
 *
 * Returns: %TRUE if the provided @key exists in the specified @group.
 */
gboolean
my_isettings_has_key( myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_has_key";
	GList *keys, *it;
	gboolean found;

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), FALSE );
	g_return_val_if_fail( my_strlen( group ), FALSE );

	found = FALSE;
	keys = my_isettings_get_keys( instance, group );

	for( it=keys ; it ; it=it->next ){
		if( !my_collate( it->data, key )){
			found = TRUE;
			break;
		}
	}

	my_isettings_free_keys( instance, keys );

	return( found );
}

/**
 * my_isettings_remove_key:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 *
 * Removes the @key of the @group from the settings file.
 */
void
my_isettings_remove_key( myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_remove_key";

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->remove_key ){
		MY_ISETTINGS_GET_INTERFACE( instance )->remove_key( instance, group, key );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'remove_key()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_get_boolean:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 *
 * Returns: the value of @key, or %FALSE.
 */
gboolean
my_isettings_get_boolean( const myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_get_boolean";

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), FALSE );
	g_return_val_if_fail( my_strlen( group ), FALSE );
	g_return_val_if_fail( my_strlen( key ), FALSE );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_boolean ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_boolean( instance, group, key ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_boolean()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * my_isettings_set_boolean:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 * @value: the boolean to be set.
 *
 * Sets the @value boolean as the value of the @key in the @group.
 */
void
my_isettings_set_boolean( myISettings *instance, const gchar *group, const gchar *key, gboolean value )
{
	static const gchar *thisfn = "my_isettings_set_boolean";

	g_debug( "%s: instance=%p, group=%s, key=%s, value=%s",
			thisfn, ( void * ) instance, group, key, value ? "True":"False" );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->set_boolean ){
		MY_ISETTINGS_GET_INTERFACE( instance )->set_boolean( instance, group, key, value );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'set_boolean()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_get_uint:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 *
 * Returns: the value of @key, or 0.
 */
guint
my_isettings_get_uint( const myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_get_uint";

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), 0 );
	g_return_val_if_fail( my_strlen( group ), 0 );
	g_return_val_if_fail( my_strlen( key ), 0 );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_uint ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_uint( instance, group, key ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_uint()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 0 );
}

/**
 * my_isettings_set_uint:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 * @value: the unsigned integer to be set.
 *
 * Sets the @value unsigned integer as the value of the @key in the @group.
 */
void
my_isettings_set_uint( myISettings *instance, const gchar *group, const gchar *key, guint value )
{
	static const gchar *thisfn = "my_isettings_set_uint";

	g_debug( "%s: instance=%p, group=%s, key=%s, value=%u",
			thisfn, ( void * ) instance, group, key, value );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->set_uint ){
		MY_ISETTINGS_GET_INTERFACE( instance )->set_uint( instance, group, key, value );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'set_uint()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_get_uint_list:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 *
 * Returns: the value of @key as a list of unsigned integers which
 * should be #my_isettings_free_uint_list() by the caller.
 */
GList *
my_isettings_get_uint_list( const myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_get_uint_list";

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_uint_list ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_uint_list( instance, group, key ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_uint_list()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_set_uint_list:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 * @value: [allow-none]: the list of unsigned integers to be set.
 *
 * Sets the @value list of unsigned integers as the value of the @key
 * in the @group.
 * Removes the key if @value is %NULL.
 */
void
my_isettings_set_uint_list( myISettings *instance, const gchar *group, const gchar *key, const GList *value )
{
	static const gchar *thisfn = "my_isettings_set_uint_list";

	g_debug( "%s: instance=%p, group=%s, key=%s, value=%p",
			thisfn, ( void * ) instance, group, key, ( void * ) value );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->set_uint_list ){
		MY_ISETTINGS_GET_INTERFACE( instance )->set_uint_list( instance, group, key, value );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'set_uint_list()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_free_uint_list:
 * @instance: this #myISettings instance.
 * @value: the list of unsigned integers to be freed, as returned by
 *  #my_isettings_get_uint_list().
 *
 * Frees the provided @key_list.
 */
void
my_isettings_free_uint_list( myISettings *instance, GList *value )
{
	static const gchar *thisfn = "my_isettings_free_uint_list";

	g_debug( "%s: instance=%p, value=%p", thisfn, ( void * ) instance, ( void * ) value );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( value );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->free_uint_list ){
		MY_ISETTINGS_GET_INTERFACE( instance )->free_uint_list( instance, value );

	} else {
		g_list_free( value );
	}
}

/**
 * my_isettings_get_string:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 *
 * Returns: the value of @key as a newly allocated string which should
 * be g_free() by the caller, or %NULL.
 */
gchar *
my_isettings_get_string( const myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_get_string";

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_string ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_string( instance, group, key ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_string()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_set_string:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 * @value: the string to be set.
 *
 * Sets the @value string as the value of the @key in the @group.
 */
void
my_isettings_set_string( myISettings *instance, const gchar *group, const gchar *key, const gchar *value )
{
	static const gchar *thisfn = "my_isettings_set_string";

	g_debug( "%s: instance=%p, group=%s, key=%s, value=%s",
			thisfn, ( void * ) instance, group, key, value );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->set_string ){
		MY_ISETTINGS_GET_INTERFACE( instance )->set_string( instance, group, key, value );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'set_string()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_get_string_list:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 *
 * Returns: the value of @key as a list of strings which should be
 * #my_isettings_free_string_list() by the caller.
 */
GList *
my_isettings_get_string_list( const myISettings *instance, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_isettings_get_string_list";

	g_debug( "%s: instance=%p, group=%s, key=%s", thisfn, ( void * ) instance, group, key );

	g_return_val_if_fail( instance && MY_IS_ISETTINGS( instance ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	if( MY_ISETTINGS_GET_INTERFACE( instance )->get_string_list ){
		return( MY_ISETTINGS_GET_INTERFACE( instance )->get_string_list( instance, group, key ));
	}

	g_info( "%s: myISettings's %s implementation does not provide 'get_string_list()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * my_isettings_set_string_list:
 * @instance: this #myISettings instance.
 * @group: the name of the group.
 * @key: the name of the key.
 * @value: the list of strings to be set.
 *
 * Sets the @value list of strings as the value of the @key
 * in the @group.
 */
void
my_isettings_set_string_list( myISettings *instance, const gchar *group, const gchar *key, const GList *value )
{
	static const gchar *thisfn = "my_isettings_set_string_list";

	g_debug( "%s: instance=%p, group=%s, key=%s, value=%p",
			thisfn, ( void * ) instance, group, key, ( void * ) value );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->set_string_list ){
		MY_ISETTINGS_GET_INTERFACE( instance )->set_string_list( instance, group, key, value );
		return;
	}

	g_info( "%s: myISettings's %s implementation does not provide 'set_string_list()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * my_isettings_free_string_list:
 * @instance: this #myISettings instance.
 * @string_list: the list of keys to be freed, as returned by
 *  #my_isettings_get_keys().
 *
 * Frees the provided @key_list.
 */
void
my_isettings_free_string_list( myISettings *instance, GList *string_list )
{
	static const gchar *thisfn = "my_isettings_free_string_list";

	g_debug( "%s: instance=%p, string_list=%p", thisfn, ( void * ) instance, ( void * ) string_list );

	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));

	if( MY_ISETTINGS_GET_INTERFACE( instance )->free_string_list ){
		MY_ISETTINGS_GET_INTERFACE( instance )->free_string_list( instance, string_list );

	} else if( string_list ){
		g_list_free_full( string_list, ( GDestroyNotify ) g_free );
	}
}
