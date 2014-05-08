/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __MY_SETTINGS_H__
#define __MY_SETTINGS_H__

/* @title: mySettings
 * @short_description: The Settings Class Definition
 * @include: ui/my-settings.h
 *
 * The #mySettings class manages users preferences.
 *
 * Actual configuration may come from two sources:
 * - a global configuration, which apply to all users, as read-only
 *   parameters;
 * - a per-user configuration.
 *
 * Whether the user configuration supersedes the global one, or if the
 * global configuration is seen as holding mandatory informations is a
 * conception decision.
 *
 * For keys which hold a list, whether it is a string list or an int
 * list, one may also decide if the resulting list is just read from
 * one configuration file, depending of the above configuration, or if
 * the content of the two configuration files are added.
 *
 * The configuration is implemented as keyed files:
 * - global configuration is sysconfdir/xdg/openbook/openbook.conf
 * - per-user configuration is HOME/.config/openbook/openbook.conf
 *
 * mySettings class monitors the whole configuration.
 * A client may be informed of a modification of the value of a key either by
 * pre-registering a callback on this key (see my_settings_register_key_callback()
 * function), or by connecting to and filtering the notification signal.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_SETTINGS                ( my_settings_get_type())
#define MY_SETTINGS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_SETTINGS, mySettings ))
#define MY_SETTINGS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_SETTINGS, mySettingsClass ))
#define MY_IS_SETTINGS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_SETTINGS ))
#define MY_IS_SETTINGS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_SETTINGS ))
#define MY_SETTINGS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_SETTINGS, mySettingsClass ))

typedef struct _mySettingsPrivate       mySettingsPrivate;

typedef struct {
	/*< private >*/
	GObject            parent;
	mySettingsPrivate *private;
}
	mySettings;

typedef struct _mySettingsClassPrivate  mySettingsClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass            parent;
	mySettingsClassPrivate *private;
}
	mySettingsClass;

/**
 * Properties defined by the mySettings class.
 *
 * @MY_PROP_GLOBAL_MANDATORY:  whether the global preferences are mandatory.
 * @MY_PROP_TIMEOUT:           timeout when signaling modifications
 * @MY_PROP_KEYDEF:            sKeyDef definitions to be provided at construction time.
 */
#define MY_PROP_GLOBAL_MANDATORY "my-settings-prop-global-mandatory"
#define MY_PROP_TIMEOUT          "my-settings-prop-timeout"
#define MY_PROP_KEYDEFS          "my-settings-prop-keydefs"

/**
 * mySettingsMode.
 *
 *  When a user key is added to the global one, one may need to get all
 * data, or only those coming from the user preferences
 */
typedef enum {
	MY_SETTINGS_USER_ONLY = 0,
	MY_SETTINGS_GLOBAL_ONLY,
	MY_SETTINGS_ALL
}
	mySettingsMode;

typedef enum {
	MY_SETTINGS_BOOLEAN = 0,
	MY_SETTINGS_STRING,
	MY_SETTINGS_STRING_LIST,
	MY_SETTINGS_INT,
	MY_SETTINGS_INT_LIST,
}
	mySettingsType;

/**
 * mySettingsKeyDef
 *
 * This structure describes the different groups and keys which are
 * handled in our configuration files. It must be provided as a property
 * at construction time, as a statically allocated, NULL-terminated array.
 *
 * @group: the group; may be %NULL if the group name is dynamically
 *  computed.
 * @key: the key.
 * @type: the #mySettingsType of the value to be handled.
 * @default_value: the default value of the key.
 * @user_is_added: whether user config replace the global one or is
 *  added to it; when TRUE, the user configuration is added to the the
 *  content of the global one. This parameter is only relevant when
 *  #type is a list.
 */
typedef struct {
	const gchar   *group;
	const gchar   *key;
	mySettingsType type;
	const gchar   *default_value;
	gboolean       user_is_added;
}
	mySettingsKeyDef;

/* pre-registration of a callback
 */
typedef void ( *mySettingsCallback )( mySettings *settings, const gchar *group, const gchar *key, gconstpointer new_value, gboolean global, void *user_data );

void      my_settings_register_callback( mySettings *settings, const gchar *group, const gchar *key, mySettingsCallback callback, gpointer user_data );

/* signal sent when the value of a key changes
 */
#define SETTINGS_SIGNAL_KEY_CHANGED     "settings-key-changed"

GType     my_settings_get_type       ( void );

GSList   *my_settings_get_groups     ( mySettings *settings, mySettingsMode mode );

gboolean  my_settings_get_boolean    ( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global );
gchar    *my_settings_get_string     ( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global );
GSList   *my_settings_get_string_list( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global );
gint      my_settings_get_int        ( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global );
GList    *my_settings_get_int_list   ( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global );

gboolean  my_settings_set_boolean    ( mySettings *settings, const gchar *group, const gchar *key, gboolean value );
gboolean  my_settings_set_string     ( mySettings *settings, const gchar *group, const gchar *key, const gchar *value );
gboolean  my_settings_set_string_list( mySettings *settings, const gchar *group, const gchar *key, const GSList *value );
gboolean  my_settings_set_int        ( mySettings *settings, const gchar *group, const gchar *key, gint value );
gboolean  my_settings_set_int_list   ( mySettings *settings, const gchar *group, const gchar *key, const GList *value );

G_END_DECLS

#endif /* __MY_SETTINGS_H__ */
