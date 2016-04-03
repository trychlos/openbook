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

#ifndef __MY_API_MY_ISETTINGS_H__
#define __MY_API_MY_ISETTINGS_H__

/**
 * SECTION: isettings
 * @title: myISettings
 * @short_description: An interface to let the plugins manage settings
 *  files.
 * @include: my/my-isettings.h
 *
 * This is an interface to manage group/key/value triplets.
 *
 * This interface is the settings API opened to the plugins in order
 * they are able to manage the settings file(s). It is implemented
 * (at the moment) by the #mySettings object class.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_ISETTINGS                      ( my_isettings_get_type())
#define MY_ISETTINGS( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, MY_TYPE_ISETTINGS, myISettings ))
#define MY_IS_ISETTINGS( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, MY_TYPE_ISETTINGS ))
#define MY_ISETTINGS_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), MY_TYPE_ISETTINGS, myISettingsInterface ))

typedef struct _myISettings                    myISettings;

/**
 * myISettingsInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_keyfile: [should]: returns the underlying keyfile.
 * @get_filename: [should]: returns the underlying filename.
 * @get_groups: [should]: returns the list of groups.
 * @remove_group: [should]: removes a group.
 * @get_keys: [should]: returns the list of keys.
 * @free_keys: [may]: frees a list of keys.
 * @remove_key: [should]: removes a key.
 * @get_boolean: [should]: returns a boolean.
 * @set_boolean: [should]: sets a boolean.
 * @get_uint: [should]: returns an unsigned integer.
 * @set_uint: [should]: sets an unsigned integer.
 * @get_uint_list: [should]: returns a list of unsigned integers.
 * @set_uint_list: [should]: sets a list of unsigned integers.
 * @free_uint_list: [may]: frees a list of unsigned integers.
 * @get_string: [should]: returns a string.
 * @set_string: [should]: sets a string.
 * @get_string_list: [should]: returns a list of strings.
 * @set_string_list: [should]: sets a list of strings.
 * @free_string_list: [may]: frees a list of strings.
 *
 * This defines the interface that an #myISettings may/should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint      ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_keyfile:
	 * @instance: the #myISettings instance.
	 *
	 * Returns: the keyfile of the underlying settings file.
	 *
	 * Since: version 1
	 */
	GKeyFile * ( *get_keyfile )          ( const myISettings *instance );

	/**
	 * get_filename:
	 * @instance: the #myISettings instance.
	 *
	 * Returns: the filename of the underlying settings file, as a newly
	 * allocated string which should be g_free() by the caller.
	 *
	 * Since: version 1
	 */
	gchar *    ( *get_filename )         ( const myISettings *instance );

	/**
	 * get_groups:
	 * @instance: the #myISettings instance.
	 *
	 * Returns: the list of groups as a #GList of strings which should
	 * be #my_isettings_free_groups() by the caller.
	 *
	 * Since: version 1
	 */
	GList *    ( *get_groups )           ( const myISettings *instance );

	/**
	 * remove_group:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 *
	 * Removes the @group from the settings file.
	 *
	 * Since: version 1
	 */
	void       ( *remove_group )         ( myISettings *instance,
												const gchar *group );

	/**
	 * get_keys:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 *
	 * Returns: the list of keys as a #GList of strings which should be
	 * #my_isettings_free_keys() by the caller.
	 *
	 * Since: version 1
	 */
	GList *    ( *get_keys )             ( const myISettings *instance,
												const gchar *group );

	/**
	 * free_keys:
	 * @key_list: a list of keys as returned by #get_keys() method.
	 *
	 * Frees the provided @key_list.
	 *
	 * Since: version 1
	 */
	void       ( *free_keys )            ( GList *key_list );

	/**
	 * remove_key:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 *
	 * Removes the @key in the @group from the settings file.
	 *
	 * Since: version 1
	 */
	void       ( *remove_key )           ( myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * get_boolean:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 *
	 * Returns: the value of the @key in the @group,
	 * or FALSE if not found.
	 *
	 * Since: version 1
	 */
	gboolean   ( *get_boolean )          ( const myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * set_boolean:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 * @value: the boolean to be set.
	 *
	 * Sets the @value to the @key of the @group.
	 *
	 * Since: version 1
	 */
	void       ( *set_boolean )          ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												gboolean value );

	/**
	 * get_uint:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 *
	 * Returns: the value of the @key in the @group,
	 * or 0 if not found.
	 *
	 * Since: version 1
	 */
	guint      ( *get_uint )             ( const myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * set_uint:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 * @value: the unsigned integer to be set.
	 *
	 * Sets the @value to the @key of the @group.
	 *
	 * Since: version 1
	 */
	void       ( *set_uint )             ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												guint value );

	/**
	 * get_uint_list:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 *
	 * Returns: the value of the @key in the @group as a #GList of
	 * unsigned integers, or %NULL if not found.
	 *
	 * Since: version 1
	 */
	GList *    ( *get_uint_list )        ( const myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * set_uint_list:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 * @value: [allow-none]: the list of unsigned integers to be set.
	 *
	 * Sets the @value to the @key of the @group.
	 * Removes the key if @value is %NULL.
	 *
	 * Since: version 1
	 */
	void       ( *set_uint_list )        ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												const GList *value );

	/**
	 * free_uint_list:
	 * @value: a list of unsigned integers as returned by #get_uint_list()
	 *  method.
	 *
	 * Frees the provided @value.
	 *
	 * Since: version 1
	 */
	void       ( *free_uint_list )       ( GList *value );

	/**
	 * get_string:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 *
	 * Returns: the value of the @key in the @group, as a newly
	 * allocated string which should be g_free() by the caller,
	 * or %NULL if not found.
	 *
	 * Since: version 1
	 */
	gchar *    ( *get_string )           ( const myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * set_string:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 * @value: the string to be set.
	 *
	 * Sets the @value to the @key of the @group.
	 *
	 * Since: version 1
	 */
	void       ( *set_string )           ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												const gchar *value );

	/**
	 * get_string_list:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 *
	 * Returns: the value of the @key in the @group, as a #GList of
	 * strings which should be #my_isettings_free_string_list() by the
	 * caller.
	 *
	 * Since: version 1
	 */
	GList *    ( *get_string_list )      ( const myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * set_string_list:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 * @key: the name of the key.
	 * @value: the list of strings to be set.
	 *
	 * Sets the @value to the @key of the @group.
	 *
	 * Since: version 1
	 */
	void       ( *set_string_list )      ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												const GList *value );

	/**
	 * free_string_list:
	 * @string_list: a list of strings as returned by #get_string_list()
	 *  method.
	 *
	 * Frees the provided @string_list.
	 *
	 * Since: version 1
	 */
	void       ( *free_string_list )     ( GList *string_list );
}
	myISettingsInterface;

/*
 * Interface-wide
 */
GType     my_isettings_get_type                  ( void );

guint     my_isettings_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint     my_isettings_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GKeyFile *my_isettings_get_keyfile               ( const myISettings *instance );

gchar    *my_isettings_get_filename              ( const myISettings *instance );

/* group management
 */
GList    *my_isettings_get_groups                ( const myISettings *settings );

#define   my_isettings_free_groups( L )		     g_list_free_full(( L ), ( GDestroyNotify ) g_free )

void      my_isettings_remove_group              ( myISettings *settings,
														const gchar *group );

/* key management
 */
GList    *my_isettings_get_keys                  ( const myISettings *instance,
														const gchar *group );

void      my_isettings_free_keys                 ( const myISettings *instance,
														GList *key_list );

gboolean  my_isettings_has_key                   ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void      my_isettings_remove_key                ( myISettings *settings,
														const gchar *group,
														const gchar *key );

/* data management
 */
gboolean  my_isettings_get_boolean               ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void      my_isettings_set_boolean               ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														gboolean value );

guint     my_isettings_get_uint                  ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void      my_isettings_set_uint                  ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														guint value );

GList    *my_isettings_get_uint_list             ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void      my_isettings_set_uint_list             ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														const GList *value );

void      my_isettings_free_uint_list            ( const myISettings *instance,
														GList *value );

gchar    *my_isettings_get_string                ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void      my_isettings_set_string                ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														const gchar *value );

GList    *my_isettings_get_string_list           ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void      my_isettings_set_string_list           ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														const GList *value );

void      my_isettings_free_string_list          ( const myISettings *instance,
														GList *string_list );

G_END_DECLS

#endif /* __MY_API_MY_ISETTINGS_H__ */
