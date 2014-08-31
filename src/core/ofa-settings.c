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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib-object.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"

#define OFA_TYPE_SETTINGS                ( ofa_settings_get_type())
#define OFA_SETTINGS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_SETTINGS, ofaSettings ))
#define OFA_SETTINGS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_SETTINGS, ofaSettingsClass ))
#define OFA_IS_SETTINGS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_SETTINGS ))
#define OFA_IS_SETTINGS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_SETTINGS ))
#define OFA_SETTINGS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_SETTINGS, ofaSettingsClass ))

typedef struct _ofaSettingsPrivate       ofaSettingsPrivate;

typedef struct {
	/*< private >*/
	GObject             parent;
	ofaSettingsPrivate *private;
}
	ofaSettings;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaSettingsClass;

/* private instance data
 */
struct _ofaSettingsPrivate {
	gboolean   dispose_has_run;

	GKeyFile  *keyfile;
	gchar     *kf_name;
};

#define GROUP_GENERAL             "General"
#define GROUP_DOSSIER             "Dossier"

GType ofa_settings_get_type( void ) G_GNUC_CONST;

static ofaSettings *st_settings = NULL;

G_DEFINE_TYPE( ofaSettings, ofa_settings, G_TYPE_OBJECT )

static void     settings_new( void );
static gint     settings_get_uint( const gchar *group, const gchar *key );
static void     load_key_file( ofaSettings *settings );
static gboolean write_key_file( ofaSettings *settings );
static gchar  **string_to_array( const gchar *string );

static void
settings_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_settings_finalize";
	ofaSettingsPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->private;

	/* free data members here */
	g_key_file_free( priv->keyfile );
	g_free( priv->kf_name );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settings_parent_class )->finalize( object );
}

static void
settings_dispose( GObject *object )
{
	ofaSettingsPrivate *priv;

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settings_parent_class )->dispose( object );
}

static void
settings_constructed( GObject *object )
{
	static const gchar *thisfn = "ofa_settings_constructed";
	ofaSettingsPrivate *priv;

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: object=%p (%s)",
				thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

		/* first chain up to the parent class */
		if( G_OBJECT_CLASS( ofa_settings_parent_class )->constructed ){
			G_OBJECT_CLASS( ofa_settings_parent_class )->constructed( object );
		}

		/* then do our stuff */
		load_key_file( OFA_SETTINGS( object ));
	}
}

static void
ofa_settings_init( ofaSettings *self )
{
	static const gchar *thisfn = "ofa_settings_init";

	g_return_if_fail( self && OFA_IS_SETTINGS( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaSettingsPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_settings_class_init( ofaSettingsClass *klass )
{
	static const gchar *thisfn = "ofa_settings_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = settings_constructed;
	G_OBJECT_CLASS( klass )->dispose = settings_dispose;
	G_OBJECT_CLASS( klass )->finalize = settings_finalize;
}

/**
 * ofa_settings_new:
 *
 * Allocates a new #ofaSettings object which should be ofa_settings_free()
 * by the caller at the end of the program.
 */
static void
settings_new( void )
{
	if( !st_settings ){
		st_settings = g_object_new( OFA_TYPE_SETTINGS, NULL );
	}
}

static void
load_key_file( ofaSettings *settings )
{
	static const gchar *thisfn = "ofa_settings_load_key_file";
	gchar *dir;
	GError *error;

	g_debug( "%s: settings=%p", thisfn, ( void * ) settings );

	settings->private->keyfile = g_key_file_new();

	dir = g_build_filename( g_get_home_dir(), ".config", PACKAGE, NULL );
	g_mkdir_with_parents( dir, 0750 );
	settings->private->kf_name = g_strdup_printf( "%s/%s.conf", dir, PACKAGE );
	g_free( dir );

	error = NULL;
	if( !g_key_file_load_from_file(
			settings->private->keyfile,
			settings->private->kf_name, G_KEY_FILE_KEEP_COMMENTS, &error )){
		if( error->code != G_FILE_ERROR_NOENT ){
			g_warning( "%s: %s (%d) %s",
					thisfn, settings->private->kf_name, error->code, error->message );
		} else {
			g_debug( "%s: %s: file doesn't exist", thisfn, settings->private->kf_name );
		}
		g_error_free( error );
	}
}

static gboolean
write_key_file( ofaSettings *settings )
{
	static const gchar *thisfn = "ofa_settings_write_key_file";
	gchar *data;
	GFile *file;
	GFileOutputStream *stream;
	GError *error;
	gsize length;

	error = NULL;
	data = g_key_file_to_data( settings->private->keyfile, &length, NULL );
	file = g_file_new_for_path( settings->private->kf_name );

	stream = g_file_replace( file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error );
	if( error ){
		g_warning( "%s: g_file_replace: %s", thisfn, error->message );
		g_error_free( error );
		if( stream ){
			g_object_unref( stream );
		}
		g_object_unref( file );
		g_free( data );
		return( FALSE );
	}

	g_output_stream_write( G_OUTPUT_STREAM( stream ), data, length, NULL, &error );
	if( error ){
		g_warning( "%s: g_output_stream_write: %s", thisfn, error->message );
		g_error_free( error );
		g_object_unref( stream );
		g_object_unref( file );
		g_free( data );
		return( FALSE );
	}

	g_output_stream_close( G_OUTPUT_STREAM( stream ), NULL, &error );
	if( error ){
		g_warning( "%s: g_output_stream_close: %s", thisfn, error->message );
		g_error_free( error );
		g_object_unref( stream );
		g_object_unref( file );
		g_free( data );
		return( FALSE );
	}

	g_object_unref( stream );
	g_object_unref( file );
	g_free( data );

	return( TRUE );
}

/**
 * ofa_settings_free:
 */
void
ofa_settings_free( void )
{
	if( st_settings ){
		g_object_unref( st_settings );
		st_settings = NULL;
	}
}

/**
 * ofa_settings_get_dossiers:
 *
 * Returns the list of all defined dossiers as a newly allocated #GSList
 * list of newly allocated strings. The returned list should be
 * #g_slist_free_full() by the caller.
 */
GSList *
ofa_settings_get_dossiers( void )
{
	static const gchar *thisfn = "ofa_settings_get_dossiers";
	GSList *slist;
	gchar *prefix;
	gint spfx;
	gchar **array, **idx;

	g_debug( "%s", thisfn );

	settings_new();

	prefix = g_strdup_printf( "%s ", GROUP_DOSSIER );
	spfx = g_utf8_strlen( prefix, -1 );
	array = g_key_file_get_groups( st_settings->private->keyfile, NULL );
	slist = NULL;
	idx = array;

	while( *idx ){
		if( g_str_has_prefix( *idx, prefix )){
			slist = g_slist_prepend( slist, g_strstrip( g_strdup( *idx+spfx )));
		}
		idx++;
	}

	g_strfreev( array );
	g_free( prefix );

	return( slist );
}

/**
 * ofa_settings_remove_dossier:
 */
void
ofa_settings_remove_dossier( const gchar *name )
{
	static const gchar *thisfn = "ofa_settings_remove_dossier";
	gchar *group;

	g_debug( "%s: name=%s", thisfn, name );

	settings_new();

	group = g_strdup_printf( "%s %s", GROUP_DOSSIER, name );
	g_key_file_remove_group( st_settings->private->keyfile, group, NULL );
	g_free( group );
}

/**
 * ofa_settings_get_dossier_provider:
 * @name: the name of the dossier
 *
 * Returns the ISgbd provider name as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
ofa_settings_get_dossier_provider( const gchar *name )
{
	return( ofa_settings_get_dossier_key_string( name, "Provider" ));
}

/**
 * ofa_settings_get_dossier_key_string:
 * @name: the name of the dossier
 * @key: the searched key
 *
 * Returns the key string for the dossier, as a newly allocated string
 * which should be g_free() by the caller.
 *
 * But for the "Provider" key that is directly implemented by the
 * ofaIDbms interface, all other keys are supposed to be used only by
 * the DBMS providers.
 */
gchar *
ofa_settings_get_dossier_key_string( const gchar *name, const gchar *key )
{
	static const gchar *thisfn = "ofa_settings_get_dossier_key_string";
	gchar *group, *value;

	g_debug( "%s: name=%s, key=%s", thisfn, name, key );

	settings_new();

	group = g_strdup_printf( "%s %s", GROUP_DOSSIER, name );
	value = g_key_file_get_string( st_settings->private->keyfile, group, key, NULL );
	g_free( group );

	return( value );
}

/**
 * ofa_settings_get_dossier_key_uint:
 * @name: the name of the dossier
 * @key: the searched key
 *
 * Returns the key integer for the dossier.
 *
 * But for the "Provider" key that is directly implemented by the
 * ofaIDbms interface, all other keys are supposed to be used only by
 * the DBMS providers.
 */
gint
ofa_settings_get_dossier_key_uint( const gchar *name, const gchar *key )
{
	gchar *group;
	gint result;

	settings_new();

	group = g_strdup_printf( "%s %s", GROUP_DOSSIER, name );

	result = settings_get_uint( group, key );

	g_free( group );

	return( result );
}

/**
 * ofa_settings_set_dossier_key_string:
 * @name: the name of the dossier
 * @key: the searched key
 * @value: the value to be set
 *
 * Set the value for the key in the dossier group.
 */
void
ofa_settings_set_dossier_key_string( const gchar *name, const gchar *key, const gchar *value )
{
	static const gchar *thisfn = "ofa_settings_set_dossier_key_string";
	gchar *group;

	g_debug( "%s: name=%s, key=%s", thisfn, name, key );

	settings_new();

	group = g_strdup_printf( "%s %s", GROUP_DOSSIER, name );
	ofa_settings_set_string_ex( group, key, value );
	g_free( group );
}

/**
 * ofa_settings_set_dossier:
 * @name:
 *
 * Define a new user dossier.
 */
gboolean
ofa_settings_set_dossier( const gchar *name, ... )
{
	static const gchar *thisfn = "ofa_settings_set_dossier";
	gchar *group;
	va_list ap;
	const gchar *key;
	gint type;
	const gchar *scontent;
	gint icontent;

	g_debug( "%s: name=%s", thisfn, name );

	settings_new();

	group = g_strdup_printf( "%s %s", GROUP_DOSSIER, name );

	va_start( ap, name );
	while( TRUE ){
		key = va_arg( ap, const gchar * );
		if( !key ){
			break;
		}
		type = va_arg( ap, gint );
		switch( type ){

			case SETTINGS_TYPE_STRING:
				scontent = va_arg( ap, const gchar * );
				if( scontent && g_utf8_strlen( scontent, -1 )){
					g_debug( "%s: setting key group=%s, key=%s, content=%s", thisfn, group, key, scontent );
					g_key_file_set_string( st_settings->private->keyfile, group, key, scontent );
				} else {
					g_debug( "%s: removing key group=%s, key=%s", thisfn, group, key );
					g_key_file_remove_key( st_settings->private->keyfile, group, key, NULL );
				}
				break;

			case SETTINGS_TYPE_INT:
				icontent = va_arg( ap, gint );
				if( icontent > 0 ){
					g_debug( "%s: setting key group=%s, key=%s, content=%d", thisfn, group, key, icontent );
					g_key_file_set_integer( st_settings->private->keyfile, group, key, icontent );
				} else {
					g_debug( "%s: removing key group=%s, key=%s", thisfn, group, key );
					g_key_file_remove_key( st_settings->private->keyfile, group, key, NULL );
				}
				break;
		}
	}
	va_end( ap );
	g_free( group );

	return( write_key_file( st_settings ));
}

/**
 * ofa_settings_get_string_list:
 *
 * Returns a newly allocated GSList of string, which should be
 * g_slist_free_full( GSList *, ( GDestroyNotify ) g_free ) by the
 * caller.
 */
GSList *
ofa_settings_get_string_list( const gchar *key )
{
	GSList *list;
	gchar **array, **i;

	settings_new();

	list = NULL;

	array = g_key_file_get_string_list( st_settings->private->keyfile, GROUP_GENERAL, key, NULL, NULL );
	if( array ){
		i = ( gchar ** ) array;
		while( *i ){
			list = g_slist_prepend( list, g_strdup( *i ));
			i++;
		}
	}
	g_strfreev( array );

	return( g_slist_reverse( list ));
}

/**
 * ofa_settings_set_string_list:
 */
void
ofa_settings_set_string_list( const gchar *key, const GSList *str_list )
{
	GString *string;
	const GSList *it;

	settings_new();

	string = g_string_new( "" );
	for( it = str_list ; it ; it = it->next ){
		g_string_append_printf( string, "%s;", ( const gchar * ) it->data );
	}
	g_key_file_set_string( st_settings->private->keyfile, GROUP_GENERAL, key, string->str );
	g_string_free( string, TRUE );

	write_key_file( st_settings );
}

/**
 * ofa_settings_get_uint_list:
 *
 * Returns a newly allocated GList of int, which should be
 * g_list_free() by the caller.
 */
GList *
ofa_settings_get_uint_list( const gchar *key )
{
	GList *list;
	gchar *str;
	gchar **array, **i;

	settings_new();

	list = NULL;

	str = g_key_file_get_string( st_settings->private->keyfile, GROUP_GENERAL, key, NULL );

	if( str && g_utf8_strlen( str, -1 )){
		array = string_to_array( str );
		if( array ){
			i = ( gchar ** ) array;
			while( *i ){
				list = g_list_prepend( list, GINT_TO_POINTER( atoi( *i )));
				i++;
			}
		}
		g_strfreev( array );
	}

	g_free( str );

	return( g_list_reverse( list ));
}

/**
 * ofa_settings_set_uint_list:
 */
void
ofa_settings_set_uint_list( const gchar *key, const GList *uint_list )
{
	GString *string;
	const GList *it;

	settings_new();

	string = g_string_new( "" );
	for( it = uint_list ; it ; it = it->next ){
		g_string_append_printf( string, "%u;", GPOINTER_TO_UINT( it->data ));
	}
	g_key_file_set_string( st_settings->private->keyfile, GROUP_GENERAL, key, string->str );
	g_string_free( string, TRUE );

	write_key_file( st_settings );
}

/*
 * converts a string to an array of strings
 * accepts both:
 * - a semi-comma-separated list of strings (the last separator, if any, is not counted)
 * - a comma-separated list of strings between square brackets (à la GConf)
 *
 */
static gchar **
string_to_array( const gchar *string )
{
	gchar *sdup;
	gchar **array;

	array = NULL;

	if( string && strlen( string )){
		sdup = g_strstrip( g_strdup( string ));

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
			if( g_str_has_suffix( string, ";" )){
				sdup[strlen(sdup)-1] = ' ';
				sdup = g_strstrip( sdup );
			}
			array = g_strsplit( sdup, ";", -1 );
		}
		g_free( sdup );
	}

	return( array );
}

/**
 * ofa_settings_get_uint:
 *
 * Returns the specified integer value.
 */
gint
ofa_settings_get_uint( const gchar *key )
{
	settings_new();
	return( settings_get_uint( GROUP_GENERAL, key ));
}

/**
 * ofa_settings_get_uint:
 *
 * Returns the specified integer value.
 */
static gint
settings_get_uint( const gchar *group, const gchar *key )
{
	gint result;
	gchar *str;

	result = -1;

	str = g_key_file_get_string( st_settings->private->keyfile, group, key, NULL );
	if( str && g_utf8_strlen( str, -1 )){
		result = atoi( str );
	}

	g_free( str );

	return( result );
}

/**
 * ofa_settings_set_uint:
 */
void
ofa_settings_set_uint( const gchar *key, guint value )
{
	gchar *string;

	settings_new();

	string = g_strdup_printf( "%u", value );
	g_key_file_set_string( st_settings->private->keyfile, GROUP_GENERAL, key, string );
	g_free( string );

	write_key_file( st_settings );
}

/**
 * ofa_settings_get_string:
 *
 * Returns the specified string value as a newly allocated string which
 * must be g_free() by the caller.
 */
gchar *
ofa_settings_get_string( const gchar *key )
{
	return( ofa_settings_get_string_ex( GROUP_GENERAL, key ));
}

/**
 * ofa_settings_set_string:
 */
void
ofa_settings_set_string( const gchar *key, const gchar *value )
{
	ofa_settings_set_string_ex( GROUP_GENERAL, key, value );
}

/**
 * ofa_settings_get_boolean:
 *
 * Returns the specified boolean value.
 */
gboolean
ofa_settings_get_boolean( const gchar *key )
{
	gboolean result;
	gchar *str;

	settings_new();

	result = FALSE;
	str = g_key_file_get_string( st_settings->private->keyfile, GROUP_GENERAL, key, NULL );

	if( str && g_utf8_strlen( str, -1 )){
		my_utils_boolean_set_from_str( str, &result );
	}

	g_free( str );

	return( result );
}

/**
 * ofa_settings_set_boolean:
 */
void
ofa_settings_set_boolean( const gchar *key, gboolean value )
{
	gchar *string;

	settings_new();

	string = g_strdup_printf( "%s", value ? "True":"False" );
	g_key_file_set_string( st_settings->private->keyfile, GROUP_GENERAL, key, string );
	g_free( string );

	write_key_file( st_settings );
}

/**
 * ofa_settings_get_string_ex:
 *
 * Returns the specified string value as a newly allocated string which
 * must be g_free() by the caller.
 */
gchar *
ofa_settings_get_string_ex( const gchar *group, const gchar *key )
{
	gchar *str;

	settings_new();

	str = g_key_file_get_string( st_settings->private->keyfile, group, key, NULL );

	return( str );
}

/**
 * ofa_settings_set_string_ex:
 */
void
ofa_settings_set_string_ex( const gchar *group, const gchar *key, const gchar *value )
{
	settings_new();

	g_key_file_set_string( st_settings->private->keyfile, group, key, value );

	write_key_file( st_settings );
}
