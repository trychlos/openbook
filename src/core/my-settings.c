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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib-object.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-isettings.h"
#include "api/my-settings.h"
#include "api/my-utils.h"

/* private instance data
 */
struct _mySettingsPrivate {
	gboolean      dispose_has_run;
	gchar        *fname;				/* configuration filename */
	GKeyFile     *keyfile;				/* GKeyFile object */
};

static void       isettings_iface_init( myISettingsInterface *iface );
static guint      isettings_get_interface_version( const myISettings *instance );
static void       isettings_remove_group( myISettings *instance, const gchar *group );
static GList     *isettings_get_keys( const myISettings *instance, const gchar *group );
static void       isettings_remove_key( myISettings *instance, const gchar *group, const gchar *key );
static GList     *isettings_get_string_list( const myISettings *instance, const gchar *group, const gchar *key );
static gchar     *isettings_get_string( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_string( myISettings *instance, const gchar *group, const gchar *key, const gchar *value );
static guint      isettings_get_uint( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_uint( myISettings *instance, const gchar *group, const gchar *key, guint value );
static void       load_key_file( mySettings *settings, const gchar *filename );
static gchar     *get_default_config_dir( void );
static gchar     *get_conf_filename( const gchar *name, const gchar *envvar );
static gchar    **str_to_array( const gchar *str );
static gboolean   write_key_file( mySettings *settings );

G_DEFINE_TYPE_EXTENDED( mySettings, my_settings, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( MY_TYPE_ISETTINGS, isettings_iface_init ));

static void
settings_finalize( GObject *object )
{
	static const gchar *thisfn = "my_settings_finalize";
	mySettingsPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_SETTINGS( object ));

	/* free data members here */
	priv = MY_SETTINGS( object )->priv;

	g_free( priv->fname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_settings_parent_class )->finalize( object );
}

static void
settings_dispose( GObject *object )
{
	mySettingsPrivate *priv;

	g_return_if_fail( object && MY_IS_SETTINGS( object ));

	priv = MY_SETTINGS( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_key_file_free( priv->keyfile );
		priv->keyfile = NULL;
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_settings_parent_class )->dispose( object );
}

static void
my_settings_init( mySettings *self )
{
	static const gchar *thisfn = "my_settings_init";

	g_return_if_fail( self && MY_IS_SETTINGS( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_SETTINGS, mySettingsPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
my_settings_class_init( mySettingsClass *klass )
{
	static const gchar *thisfn = "my_settings_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settings_dispose;
	G_OBJECT_CLASS( klass )->finalize = settings_finalize;

	g_type_class_add_private( klass, sizeof( mySettingsPrivate ));
}

/*
 * myISettings interface management
 */
static void
isettings_iface_init( myISettingsInterface *iface )
{
	static const gchar *thisfn = "my_settings_isettings_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = isettings_get_interface_version;
	iface->remove_group = isettings_remove_group;
	iface->get_keys = isettings_get_keys;
	iface->remove_key = isettings_remove_key;
	iface->get_string_list = isettings_get_string_list;
	iface->get_string = isettings_get_string;
	iface->set_string = isettings_set_string;
	iface->get_uint = isettings_get_uint;
	iface->set_uint = isettings_set_uint;
}

static guint
isettings_get_interface_version( const myISettings *instance )
{
	return( 1 );
}

static void
isettings_remove_group( myISettings *instance, const gchar *group )
{
	mySettingsPrivate *priv;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));
	priv = MY_SETTINGS( instance )->priv;
	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		g_key_file_remove_group( priv->keyfile, group, NULL );
		write_key_file( MY_SETTINGS( instance ));
		return;
	}

	g_return_if_reached();
}

static GList *
isettings_get_keys( const myISettings *instance, const gchar *group )
{
	mySettingsPrivate *priv;
	gchar **array, **iter;
	GList *list;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );
	priv = MY_SETTINGS( instance )->priv;
	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		list = NULL;
		array = g_key_file_get_keys( priv->keyfile, group, NULL, NULL );
		if( array ){
			iter = array;
			while( *iter ){
				list = g_list_prepend( list, g_strdup( *iter ));
				iter++;
			}
			g_strfreev( array );
		}
		return( g_list_reverse( list ));
	}

	g_return_val_if_reached( NULL );
}

static void
isettings_remove_key( myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));
	priv = MY_SETTINGS( instance )->priv;
	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		g_key_file_remove_key( priv->keyfile, group, key, NULL );
		write_key_file( MY_SETTINGS( instance ));
		return;
	}

	g_return_if_reached();
}

static GList *
isettings_get_string_list( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar *str;
	gchar **array, **iter;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );
	priv = MY_SETTINGS( instance )->priv;
	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		list = NULL;
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		if( my_strlen( str )){
			array = str_to_array( str );
			if( array ){
				iter = ( gchar ** ) array;
				while( *iter ){
					list = g_list_prepend( list, g_strdup( *iter ));
					iter++;
				}
			}
			g_strfreev( array );
		}
		g_free( str );
		return( g_list_reverse( list ));
	}

	g_return_val_if_reached( NULL );
}

static gchar *
isettings_get_string( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );
	priv = MY_SETTINGS( instance )->priv;
	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		str = NULL;
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		return( str );
	}

	g_return_val_if_reached( NULL );
}

static void
isettings_set_string( myISettings *instance, const gchar *group, const gchar *key, const gchar *value )
{
	mySettingsPrivate *priv;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));
	priv = MY_SETTINGS( instance )->priv;
	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		g_key_file_set_string( priv->keyfile, group, key, value );
		write_key_file( MY_SETTINGS( instance ));
		return;
	}

	g_return_if_reached();
}

static guint
isettings_get_uint( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	guint value;
	gchar *str;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), 0 );
	priv = MY_SETTINGS( instance )->priv;
	g_return_val_if_fail( priv->keyfile, 0 );

	if( !priv->dispose_has_run ){
		value = 0;
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		if( my_strlen( str )){
			value = abs( atoi( str ));
		}
		g_free( str );
		return( value );
	}

	g_return_val_if_reached( 0 );
}

static void
isettings_set_uint( myISettings *instance, const gchar *group, const gchar *key, guint value )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));
	priv = MY_SETTINGS( instance )->priv;
	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		str = g_strdup_printf( "%u", value );
		g_key_file_set_string( priv->keyfile, group, key, str );
		g_free( str );
		write_key_file( MY_SETTINGS( instance ));
		return;
	}

	g_return_if_reached();
}

/**
 * my_settings_new:
 * @filename: the full path to the settings file.
 *
 * Returns: a new reference to a #mySettings object.
 *
 * The returned reference should be #g_object_unref() by the caller.
 */
mySettings *
my_settings_new( const gchar *filename )
{
	mySettings *settings;

	settings = g_object_new( MY_TYPE_SETTINGS, NULL );
	load_key_file( settings, filename );

	return( settings );
}

/**
 * my_settings_new_user_config:
 * @name: the name of the settings file in the configuration directory
 *  of the current user.
 * @envvar: [allow-none]: the name of an environment variable whose
 *  value may override the full pathname of the settings file.
 *
 * Returns: a new reference to a #mySettings object.
 *
 * The returned reference should be #g_object_unref() by the caller.
 */
mySettings *
my_settings_new_user_config( const gchar *name, const gchar *envvar )
{
	mySettings *settings;
	gchar *filename;

	filename = get_conf_filename( name, envvar );
	settings = my_settings_new( filename );
	g_free( filename );

	return( settings );
}

static void
load_key_file( mySettings *settings, const gchar *filename )
{
	static const gchar *thisfn = "my_settings_load_key_file";
	mySettingsPrivate *priv;
	GError *error;

	g_debug( "%s: settings=%p, fname=%s", thisfn, ( void * ) settings, filename );

	priv = settings->priv;
	priv->keyfile = g_key_file_new();
	priv->fname = g_strdup( filename );

	error = NULL;
	if( !g_key_file_load_from_file(
			priv->keyfile, priv->fname, G_KEY_FILE_KEEP_COMMENTS, &error )){

		if( error->code != G_FILE_ERROR_NOENT ){
			g_warning( "%s: %s (%d) %s",
					thisfn, settings->priv->fname, error->code, error->message );
		} else {
			g_debug( "%s: %s: file doesn't exist", thisfn, settings->priv->fname );
		}

		g_error_free( error );
	}
}

/**
 * my_settings_get_filename:
 * @settings: this #mySettings instance.
 *
 * Returns: the filename of the @settings file, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
my_settings_get_filename( const mySettings *settings )
{
	mySettingsPrivate *priv;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings) , NULL );

	priv = settings->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->fname ));
	}

	return( NULL );
}

/**
 * my_settings_get_keyfile:
 * @settings: this #mySettings instance.
 *
 * Returns: the #GKeyFile associated to the @settings file, as a new
 * reference which should be g_key_file_unref() by the caller.
 */
GKeyFile *
my_settings_get_keyfile( const mySettings *settings )
{
	mySettingsPrivate *priv;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings) , NULL );

	priv = settings->priv;

	if( !priv->dispose_has_run ){
		return( g_key_file_ref( priv->keyfile ));
	}

	return( NULL );
}

/**
 * my_settings_get_boolean:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 *
 * Returns: the specified boolean value, or %FALSE if the key is not
 * found.
 */
gboolean
my_settings_get_boolean( const mySettings *settings, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	gboolean value;
	gchar *str;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), FALSE );
	g_return_val_if_fail( my_strlen( group ), FALSE );
	g_return_val_if_fail( my_strlen( key ), FALSE );

	priv = settings->priv;
	value = FALSE;

	g_return_val_if_fail( priv->keyfile, FALSE );

	if( !priv->dispose_has_run ){
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		value = my_utils_boolean_from_str( str );
		g_free( str );
	}

	return( value );
}

/**
 * my_settings_set_boolean:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 * @value: the boolean value to be set.
 *
 * Store the boolean @value, as a "True" or "False" string, in
 * @settings file.
 */
void
my_settings_set_boolean( mySettings *settings, const gchar *group, const gchar *key, gboolean value )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_if_fail( settings && MY_IS_SETTINGS( settings ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	priv = settings->priv;

	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		str = g_strdup_printf( "%s", value ? "True":"False" );
		g_key_file_set_string( priv->keyfile, group, key, str );
		g_free( str );
		write_key_file( settings );
	}
}

/**
 * my_settings_get_uint:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 *
 * Returns: the specified unsigned integer value, or -1 if the key is
 * not found.
 */
gint
my_settings_get_uint( const mySettings*settings, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	gint value;
	gchar *str;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), -1 );
	g_return_val_if_fail( my_strlen( group ), -1 );
	g_return_val_if_fail( my_strlen( key ), -1 );

	priv = settings->priv;
	value = -1;

	g_return_val_if_fail( priv->keyfile, -1 );

	if( !priv->dispose_has_run ){
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		if( my_strlen( str )){
			value = abs( atoi( str ));
		}
		g_free( str );
	}

	return( value );
}

/**
 * my_settings_set_uint:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 * @value: the unsigned integer value to be set.
 *
 * Store the @value in @settings file.
 */
void
my_settings_set_uint( mySettings *settings, const gchar *group, const gchar *key, guint value )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_if_fail( settings && MY_IS_SETTINGS( settings ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	priv = settings->priv;

	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		str = g_strdup_printf( "%u", value );
		g_key_file_set_string( settings->priv->keyfile, group, key, str );
		g_free( str );
		write_key_file( settings );
	}
}

/**
 * my_settings_get_int_list:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 *
 * Returns: a newly allocated #GList of integers, which should be
 * #my_settings_free_int_list() by the caller, or %NULL if the key is
 * not found.
 *
 * Integers must be accessed through GPOINTER_TO_INT() macro.
 */
GList *
my_settings_get_int_list( const mySettings *settings, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar *str;
	gchar **array, **iter;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	priv = settings->priv;
	list = NULL;

	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		if( my_strlen( str )){
			array = str_to_array( str );
			if( array ){
				iter = ( gchar ** ) array;
				while( *iter ){
					list = g_list_prepend( list, GINT_TO_POINTER( atoi( *iter )));
					iter++;
				}
			}
			g_strfreev( array );
		}
		g_free( str );
	}

	return( g_list_reverse( list ));
}

/**
 * my_settings_set_int_list:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 * @list: the list of integers to be written.
 *
 * Store the @list into @settings file.
 * Remove the @key if the @list is %NULL or empty.
 *
 * The integers are expected to be accessible in the @list through
 * GPOINTER_TO_INT() macro.
 */
void
my_settings_set_int_list( mySettings *settings, const gchar *group, const gchar *key, GList *list )
{
	mySettingsPrivate *priv;
	GString *str;
	const GList *it;

	g_return_if_fail( settings && MY_IS_SETTINGS( settings ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	priv = settings->priv;

	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){

		if( list && g_list_length( list )){
			str = g_string_new( "" );
			for( it=list ; it ; it=it->next ){
				g_string_append_printf( str, "%d;", GPOINTER_TO_INT( it->data ));
			}
			g_key_file_set_string( settings->priv->keyfile, group, key, str->str );
			g_string_free( str, TRUE );

		} else {
			g_key_file_remove_key( settings->priv->keyfile, group, key, NULL );
		}

		write_key_file( settings );
	}
}

/**
 * my_settings_get_string:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 *
 * Returns: the specified string value as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
my_settings_get_string( const mySettings *settings, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	priv = settings->priv;
	str = NULL;

	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
	}

	return( str );
}

/**
 * my_settings_set_string:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 * @value: the string to be written.
 *
 * Store the @value string into @settings file.
 */
void
my_settings_set_string( mySettings *settings, const gchar *group, const gchar *key, const gchar *value )
{
	mySettingsPrivate *priv;

	g_return_if_fail( settings && MY_IS_SETTINGS( settings ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	priv = settings->priv;

	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		g_key_file_set_string( settings->priv->keyfile, group, key, value );
		write_key_file( settings );
	}
}

/**
 * my_settings_get_string_list:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 *
 * Returns: a newly allocated #GList of strings, or %NULL if the key is
 * not found.
 *
 * The returned list should be #my_settings_free_string_list() by the
 * caller.
 */
GList *
my_settings_get_string_list( const mySettings *settings, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar *str;
	gchar **array, **iter;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	priv = settings->priv;
	list = NULL;

	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		str = g_key_file_get_string( priv->keyfile, group, key, NULL );
		if( my_strlen( str )){
			array = str_to_array( str );
			if( array ){
				iter = ( gchar ** ) array;
				while( *iter ){
					list = g_list_prepend( list, g_strdup( *iter ));
					iter++;
				}
			}
			g_strfreev( array );
		}
		g_free( str );
	}

	return( g_list_reverse( list ));
}

/**
 * my_settings_set_string_list:
 * @settings: this #mySettings instance.
 * @group: the group name in the @settings file.
 * @key: the key name in the @settings file.
 * @list: the list of strings to be written.
 *
 * Store the @list string list into @settings file.
 * Remove the @key if the @list is %NULL or empty.
 */
void
my_settings_set_string_list( mySettings *settings, const gchar *group, const gchar *key, GList *list )
{
	mySettingsPrivate *priv;
	GString *str;
	const GList *it;

	g_return_if_fail( settings && MY_IS_SETTINGS( settings ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	priv = settings->priv;

	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){

		if( list && g_list_length( list )){
			str = g_string_new( "" );
			for( it=list ; it ; it=it->next ){
				g_string_append_printf( str, "%s;", ( const gchar * ) it->data );
			}
			g_key_file_set_string( settings->priv->keyfile, group, key, str->str );
			g_string_free( str, TRUE );

		} else {
			g_key_file_remove_key( settings->priv->keyfile, group, key, NULL );
		}

		write_key_file( settings );
	}
}

/**
 * my_settings_get_groups:
 * @settings: this #mySettings instance.
 *
 * Returns: the list of all defined groups as a #GList which should be
 * #my_settings_free_groups() by the caller.
 */
GList *
my_settings_get_groups( const mySettings *settings )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar **array, **iter;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), NULL );

	priv = settings->priv;
	list = NULL;

	g_return_val_if_fail( priv->keyfile, NULL );

	if( !priv->dispose_has_run ){
		array = g_key_file_get_groups( priv->keyfile, NULL );
		list = NULL;
		iter = array;
		while( *iter ){
			list = g_list_prepend( list, g_strdup( *iter ));
			iter++;
		}
		g_strfreev( array );
	}

	return( g_list_reverse( list ));
}

/**
 * my_settings_reload:
 * @settings: this #mySettings instance.
 *
 * Reload the content of the @settings file.
 */
void
my_settings_reload( mySettings *settings )
{
	mySettingsPrivate *priv;
	gchar *filename;

	g_return_if_fail( settings && MY_IS_SETTINGS( settings ));

	priv = settings->priv;

	g_return_if_fail( priv->keyfile );

	if( !priv->dispose_has_run ){
		filename = g_strdup( priv->fname );
		g_key_file_unref( priv->keyfile );
		load_key_file( settings, filename );
		g_free( filename );
	}
}

/*
 * Returns: the default configuration directory of the current user
 */
static gchar *
get_default_config_dir( void )
{
	gchar *dir;

	dir = g_build_filename( g_get_home_dir(), ".config", PACKAGE, NULL );
	g_mkdir_with_parents( dir, 0750 );

	return( dir );
}

/*
 * Returns: the full pathname of a settings file from user configuration
 * directory, taking into account a possible override from environment
 * variable
 */
static gchar *
get_conf_filename( const gchar *name, const gchar *envvar )
{
	const gchar *cstr;
	gchar *dir, *fname;

	cstr = NULL;
	if( my_strlen( envvar )){
		cstr = g_getenv( envvar );
	}
	if( my_strlen( cstr )){
		fname = g_strdup( cstr );
	} else {
		dir = get_default_config_dir();
		fname = g_build_filename( dir, name, NULL );
		g_free( dir );
	}

	return( fname );
}

/*
 * converts a string to an array of strings
 * accepts both:
 * - a semi-comma-separated list of strings (the last separator, if any, is not counted)
 * - a comma-separated list of strings between square brackets (à la GConf)
 */
static gchar **
str_to_array( const gchar *str )
{
	gchar *sdup;
	gchar **array;

	array = NULL;

	if( my_strlen( str )){
		sdup = g_strstrip( g_strdup( str ));

		/* GConf-style string list [value,value]
		 */
		if( sdup[0] == '[' && sdup[strlen(sdup)-1] == ']' ){
			sdup[0] = ' ';
			sdup[strlen(sdup)-1] = ' ';
			sdup = g_strstrip( sdup );
			array = g_strsplit( sdup, ",", -1 );

		/* semi-comma-separated list of strings
		 */
		} else {
			if( sdup[strlen(sdup)-1] == ';' ){
				sdup[strlen(sdup)-1] = '\0';
			}
			array = g_strsplit( sdup, ";", -1 );
		}
		g_free( sdup );
	}

	return( array );
}

static gboolean
write_key_file( mySettings *settings )
{
	static const gchar *thisfn = "my_settings_write_key_file";
	mySettingsPrivate *priv;
	gchar *sysfname, *data;
	GFile *file;
	GFileOutputStream *stream;
	GError *error;
	gssize written;
	gsize length;
	gboolean ok;

	error = NULL;
	ok = FALSE;
	priv = settings->priv;
	data = g_key_file_to_data( priv->keyfile, &length, NULL );
	sysfname = my_utils_filename_from_utf8( priv->fname );
	if( !sysfname ){
		g_free( data );
		return( FALSE );
	}

	file = g_file_new_for_path( sysfname );
	g_free( sysfname );

	stream = g_file_replace( file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error );
	if( !stream ){
		g_return_val_if_fail( error, FALSE );
		g_warning( "%s: bname=%s, g_file_replace: %s", thisfn, priv->fname, error->message );
		g_error_free( error );
	} else {
		written = g_output_stream_write( G_OUTPUT_STREAM( stream ), data, length, NULL, &error );
		if( written == -1 ){
			g_return_val_if_fail( error, FALSE );
			g_warning( "%s: bname=%s, g_output_stream_write: %s", thisfn, priv->fname, error->message );
			g_error_free( error );

		} else if( !g_output_stream_close( G_OUTPUT_STREAM( stream ), NULL, &error )){
			g_return_val_if_fail( error, FALSE );
			g_warning( "%s: bname=%s, g_output_stream_close: %s", thisfn, priv->fname, error->message );
			g_error_free( error );

		} else {
			ok = TRUE;
		}
		g_object_unref( stream );
	}

	g_object_unref( file );
	g_free( data );

	return( ok );
}
