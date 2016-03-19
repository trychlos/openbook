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

#include <gio/gio.h>
#include <glib-object.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-isettings.h"
#include "my/my-settings.h"
#include "my/my-utils.h"

/* private instance data
 */
struct _mySettingsPrivate {
	gboolean      dispose_has_run;
	gchar        *fname;				/* configuration filename */
	GKeyFile     *keyfile;				/* GKeyFile object */
};

static void       isettings_iface_init( myISettingsInterface *iface );
static guint      isettings_get_interface_version( const myISettings *instance );
static GKeyFile  *isettings_get_keyfile( const myISettings *instance );
static gchar     *isettings_get_filename( const myISettings *instance );
static GList     *isettings_get_groups( const myISettings *instance );
static void       isettings_remove_group( myISettings *instance, const gchar *group );
static GList     *isettings_get_keys( const myISettings *instance, const gchar *group );
static void       isettings_remove_key( myISettings *instance, const gchar *group, const gchar *key );
static gboolean   isettings_get_boolean( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_boolean( myISettings *instance, const gchar *group, const gchar *key, gboolean value );
static guint      isettings_get_uint( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_uint( myISettings *instance, const gchar *group, const gchar *key, guint value );
static GList     *isettings_get_uint_list( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_uint_list( myISettings *instance, const gchar *group, const gchar *key, const GList *value );
static gchar     *isettings_get_string( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_string( myISettings *instance, const gchar *group, const gchar *key, const gchar *value );
static GList     *isettings_get_string_list( const myISettings *instance, const gchar *group, const gchar *key );
static void       isettings_set_string_list( myISettings *instance, const gchar *group, const gchar *key, const GList *value );
static void       load_key_file( mySettings *settings, const gchar *filename );
static gchar     *get_default_config_dir( void );
static gchar     *get_conf_filename( const gchar *name, const gchar *envvar );
static gchar    **str_to_array( const gchar *str );
static gboolean   write_key_file( mySettings *settings );

G_DEFINE_TYPE_EXTENDED( mySettings, my_settings, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( mySettings )
		G_IMPLEMENT_INTERFACE( MY_TYPE_ISETTINGS, isettings_iface_init ))

static void
settings_finalize( GObject *object )
{
	static const gchar *thisfn = "my_settings_finalize";
	mySettingsPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_SETTINGS( object ));

	/* free data members here */
	priv = my_settings_get_instance_private( MY_SETTINGS( object ));

	g_free( priv->fname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_settings_parent_class )->finalize( object );
}

static void
settings_dispose( GObject *object )
{
	mySettingsPrivate *priv;

	g_return_if_fail( object && MY_IS_SETTINGS( object ));

	priv = my_settings_get_instance_private( MY_SETTINGS( object ));

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
	mySettingsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_SETTINGS( self ));

	priv = my_settings_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_settings_class_init( mySettingsClass *klass )
{
	static const gchar *thisfn = "my_settings_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settings_dispose;
	G_OBJECT_CLASS( klass )->finalize = settings_finalize;
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
	iface->get_keyfile = isettings_get_keyfile;
	iface->get_filename = isettings_get_filename;
	iface->get_groups = isettings_get_groups;
	iface->remove_group = isettings_remove_group;
	iface->get_keys = isettings_get_keys;
	iface->remove_key = isettings_remove_key;
	iface->get_boolean = isettings_get_boolean;
	iface->set_boolean = isettings_set_boolean;
	iface->get_uint = isettings_get_uint;
	iface->set_uint = isettings_set_uint;
	iface->get_uint_list = isettings_get_uint_list;
	iface->set_uint_list = isettings_set_uint_list;
	iface->get_string = isettings_get_string;
	iface->set_string = isettings_set_string;
	iface->get_string_list = isettings_get_string_list;
	iface->set_string_list = isettings_set_string_list;
}

static guint
isettings_get_interface_version( const myISettings *instance )
{
	return( 1 );
}

static GKeyFile *
isettings_get_keyfile( const myISettings *instance )
{
	mySettingsPrivate *priv;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->keyfile );
}

static gchar *
isettings_get_filename( const myISettings *instance )
{
	mySettingsPrivate *priv;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( g_strdup( priv->fname ));
}

/*
 * isettings_get_groups:
 * @settings: this #mySettings instance.
 *
 * Returns: the list of all defined groups as a #GList which should be
 * #my_settings_free_groups() by the caller.
 */
static GList *
isettings_get_groups( const myISettings *settings )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar **array, **iter;

	g_return_val_if_fail( settings && MY_IS_SETTINGS( settings ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( settings ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->keyfile, NULL );

	list = NULL;
	array = g_key_file_get_groups( priv->keyfile, NULL );
	list = NULL;
	iter = array;
	while( *iter ){
		list = g_list_prepend( list, g_strdup( *iter ));
		iter++;
	}
	g_strfreev( array );

	return( g_list_reverse( list ));
}

static void
isettings_remove_group( myISettings *instance, const gchar *group )
{
	mySettingsPrivate *priv;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	g_key_file_remove_group( priv->keyfile, group, NULL );
	write_key_file( MY_SETTINGS( instance ));

	return;
}

static GList *
isettings_get_keys( const myISettings *instance, const gchar *group )
{
	mySettingsPrivate *priv;
	gchar **array, **iter;
	GList *list;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->keyfile, NULL );

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

static void
isettings_remove_key( myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	g_key_file_remove_key( priv->keyfile, group, key, NULL );
	write_key_file( MY_SETTINGS( instance ));
}

static gboolean
isettings_get_boolean( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	gboolean value;
	gchar *str;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), FALSE );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );
	g_return_val_if_fail( priv->keyfile, FALSE );

	str = g_key_file_get_string( priv->keyfile, group, key, NULL );
	value = my_utils_boolean_from_str( str );
	g_free( str );

	return( value );
}

static void
isettings_set_boolean( myISettings *instance, const gchar *group, const gchar *key, gboolean value )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	str = g_strdup_printf( "%s", value ? "True":"False" );
	g_key_file_set_string( priv->keyfile, group, key, str );
	g_free( str );
	write_key_file( MY_SETTINGS( instance ));
}

static guint
isettings_get_uint( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	guint value;
	gchar *str;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), 0 );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, 0 );
	g_return_val_if_fail( priv->keyfile, 0 );

	value = 0;
	str = g_key_file_get_string( priv->keyfile, group, key, NULL );
	if( my_strlen( str )){
		value = abs( atoi( str ));
	}
	g_free( str );

	return( value );
}

static void
isettings_set_uint( myISettings *instance, const gchar *group, const gchar *key, guint value )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	str = g_strdup_printf( "%u", value );
	g_key_file_set_string( priv->keyfile, group, key, str );
	g_free( str );
	write_key_file( MY_SETTINGS( instance ));
}

/*
 * Integers must be accessed through GPOINTER_TO_UINT() macro.
 */
static GList *
isettings_get_uint_list( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar *str;
	gchar **array, **iter;
	gint i;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->keyfile, NULL );

	list = NULL;
	str = g_key_file_get_string( priv->keyfile, group, key, NULL );
	if( my_strlen( str )){
		array = str_to_array( str );
		if( array ){
			iter = ( gchar ** ) array;
			while( *iter ){
				i = atoi( *iter );
				list = g_list_prepend( list, GUINT_TO_POINTER( i < 0 ? 0 : i ));
				iter++;
			}
		}
		g_strfreev( array );
	}
	g_free( str );

	return( g_list_reverse( list ));
}

/**
 * The integers are expected to be accessible in the @list through
 * GPOINTER_TO_UINT() macro.
 */
static void
isettings_set_uint_list( myISettings *instance, const gchar *group, const gchar *key, const GList *value )
{
	mySettingsPrivate *priv;
	GString *str;
	const GList *it;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	if( value && g_list_length(( GList * ) value )){
		str = g_string_new( "" );
		for( it=value ; it ; it=it->next ){
			g_string_append_printf( str, "%u;", GPOINTER_TO_UINT( it->data ));
		}
		g_key_file_set_string( priv->keyfile, group, key, str->str );
		g_string_free( str, TRUE );

	} else {
		g_key_file_remove_key( priv->keyfile, group, key, NULL );
	}
	write_key_file( MY_SETTINGS( instance ));
}

static gchar *
isettings_get_string( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	gchar *str;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->keyfile, NULL );

	str = NULL;
	str = g_key_file_get_string( priv->keyfile, group, key, NULL );

	return( str );
}

static void
isettings_set_string( myISettings *instance, const gchar *group, const gchar *key, const gchar *value )
{
	mySettingsPrivate *priv;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	g_key_file_set_string( priv->keyfile, group, key, value );
	write_key_file( MY_SETTINGS( instance ));
}

static GList *
isettings_get_string_list( const myISettings *instance, const gchar *group, const gchar *key )
{
	mySettingsPrivate *priv;
	GList *list;
	gchar *str;
	gchar **array, **iter;

	g_return_val_if_fail( instance && MY_IS_SETTINGS( instance ), NULL );

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->keyfile, NULL );

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

/**
 * Store the @list string list into @settings file.
 * Remove the @key if the @list is %NULL or empty.
 */
static void
isettings_set_string_list( myISettings *instance, const gchar *group, const gchar *key, const GList *value )
{
	mySettingsPrivate *priv;
	GString *str;
	const GList *it;

	g_return_if_fail( instance && MY_IS_SETTINGS( instance ));

	priv = my_settings_get_instance_private( MY_SETTINGS( instance ));

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->keyfile );

	if( value && g_list_length(( GList * ) value )){
		str = g_string_new( "" );
		for( it=value ; it ; it=it->next ){
			g_string_append_printf( str, "%s;", ( const gchar * ) it->data );
		}
		g_key_file_set_string( priv->keyfile, group, key, str->str );
		g_string_free( str, TRUE );

	} else {
		g_key_file_remove_key( priv->keyfile, group, key, NULL );
	}
	write_key_file( MY_SETTINGS( instance ));
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

	priv = my_settings_get_instance_private( settings );

	priv->keyfile = g_key_file_new();
	priv->fname = g_strdup( filename );

	error = NULL;
	if( !g_key_file_load_from_file(
			priv->keyfile, priv->fname, G_KEY_FILE_KEEP_COMMENTS, &error )){

		if( error->code != G_FILE_ERROR_NOENT ){
			g_warning( "%s: %s (%d) %s",
					thisfn, priv->fname, error->code, error->message );
		} else {
			g_debug( "%s: %s: file doesn't exist", thisfn, priv->fname );
		}

		g_error_free( error );
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
	priv = my_settings_get_instance_private( settings );
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
