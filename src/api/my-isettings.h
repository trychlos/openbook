/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_MY_ISETTINGS_H__
#define __OPENBOOK_API_MY_ISETTINGS_H__

/**
 * SECTION: isettings
 * @title: myISettings
 * @short_description: An interface to let the plugins manage settings
 *  files.
 * @include: openbook/my-isettings.h
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

typedef struct _ofaISettings                   myISettings;

/**
 * myISettingsInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @remove_group: [should]: removes a group.
 * @get_keys: [should]: returns the list of keys.
 * @free_keys: [may]: frees a list of keys.
 * @remove_key: [should]: removes a key.
 * @get_string_list: [should]: returns a list of strings.
 * @free_string_list: [may]: frees a list of strings.
 * @get_string: [should]: returns a string.
 * @set_string: [should]: sets a string.
 * @get_uint: [should]: returns an unsigned integer.
 * @set_uint: [should]: sets an unsigned integer.
 *
 * This defines the interface that an #myISettings should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #myISettings instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implemented by the instance.
	 *
	 * Returns: the version number of this interface that the @instance
	 *  is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint    ( *get_interface_version )( const myISettings *instance );

	/**
	 * remove_group:
	 * @instance: the #myISettings instance.
	 * @group: the name of the group in the settings file.
	 *
	 * Removes the @group from the settings file.
	 *
	 * Since: version 1
	 */
	void     ( *remove_group )         ( myISettings *instance,
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
	GList *  ( *get_keys )             ( const myISettings *instance,
												const gchar *group );

	/**
	 * free_keys:
	 * @key_list: a list of keys as returned by #get_keys() method.
	 *
	 * Frees the provided @key_list.
	 *
	 * Since: version 1
	 */
	void     ( *free_keys )            ( GList *key_list );

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
	void     ( *remove_key )           ( myISettings *instance,
												const gchar *group,
												const gchar *key );

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
	GList *  ( *get_string_list )      ( const myISettings *instance,
												const gchar *group,
												const gchar *key );

	/**
	 * free_string_list:
	 * @string_list: a list of strings as returned by #get_string_list()
	 *  method.
	 *
	 * Frees the provided @string_list.
	 *
	 * Since: version 1
	 */
	void     ( *free_string_list )     ( GList *string_list );

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
	gchar *  ( *get_string )           ( const myISettings *instance,
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
	void     ( *set_string )           ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												const gchar *value );

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
	guint    ( *get_uint )             ( const myISettings *instance,
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
	void     ( *set_uint )             ( myISettings *instance,
												const gchar *group,
												const gchar *key,
												guint value );
}
	myISettingsInterface;

GType   my_isettings_get_type                  ( void );

guint   my_isettings_get_interface_last_version( void );

guint   my_isettings_get_interface_version     ( const myISettings *instance );

/* group management
 */
void    my_isettings_remove_group              ( myISettings *settings,
														const gchar *group );

/* key management
 */
GList  *my_isettings_get_keys                  ( const myISettings *instance,
														const gchar *group );

void    my_isettings_free_keys                 ( const myISettings *instance,
														GList *key_list );

void    my_isettings_remove_key                ( myISettings *settings,
														const gchar *group,
														const gchar *key );

/* data management
 */
GList  *my_isettings_get_string_list           ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void    my_isettings_free_string_list          ( const myISettings *instance,
														GList *string_list );

gchar  *my_isettings_get_string                ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void    my_isettings_set_string                ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														const gchar *value );

guint   my_isettings_get_uint                  ( const myISettings *instance,
														const gchar *group,
														const gchar *key );

void    my_isettings_set_uint                  ( myISettings *instance,
														const gchar *group,
														const gchar *key,
														guint value );

G_END_DECLS

#endif /* __OPENBOOK_API_MY_ISETTINGS_H__ */
