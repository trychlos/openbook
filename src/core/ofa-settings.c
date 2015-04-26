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
	/*< public members >*/
	GObject             parent;

	/*< private members >*/
	ofaSettingsPrivate *priv;
}
	ofaSettings;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaSettingsClass;

/* private instance data
 */
struct _ofaSettingsPrivate {
	gboolean          dispose_has_run;

	ofaSettingsTarget target;
	gchar            *fname;			/* configuration filename */
	GKeyFile         *keyfile;			/* GKeyFile object */
};

/* properties
 * @PROP_TARGET: the settings target, set at instanciation time.
 */
#define PROP_TARGET                     "ofa-settings-prop-target"

enum {
	PROP_TARGET_ID = 1,
};

GType ofa_settings_get_type( void ) G_GNUC_CONST;

static ofaSettings *st_user_settings    = NULL;
static ofaSettings *st_dossier_settings = NULL;

G_DEFINE_TYPE( ofaSettings, ofa_settings, G_TYPE_OBJECT )

static void         settings_new( void );
static void         load_key_file( ofaSettings *settings );
static gchar       *get_user_conf_filename( ofaSettings *settings );
static gchar       *get_dossier_conf_filename( ofaSettings *settings );
static gchar       *get_default_config_dir( ofaSettings *settings );
static gboolean     write_key_file( ofaSettings *settings );
static GKeyFile    *get_keyfile_from_target( ofaSettingsTarget target );
static ofaSettings *get_settings_from_target( ofaSettingsTarget target );
static gchar      **str_to_array( const gchar *str );
static gchar       *get_dossier_group_from_name( const gchar *dname );

static void
settings_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_settings_finalize";
	ofaSettingsPrivate *priv;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	/* free data members here */
	priv = OFA_SETTINGS( object )->priv;

	g_free( priv->fname );
	g_key_file_free( priv->keyfile );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settings_parent_class )->finalize( object );
}

static void
settings_dispose( GObject *object )
{
	ofaSettingsPrivate *priv;

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->priv;

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

	priv = OFA_SETTINGS( object )->priv;

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
settings_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaSettingsPrivate *priv;

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->priv;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_TARGET_ID:
				g_value_set_int( value, priv->target );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
settings_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaSettingsPrivate *priv;

	g_return_if_fail( object && OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->priv;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_TARGET_ID:
				priv->target = g_value_get_int( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
ofa_settings_init( ofaSettings *self )
{
	static const gchar *thisfn = "ofa_settings_init";

	g_return_if_fail( self && OFA_IS_SETTINGS( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_SETTINGS, ofaSettingsPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_settings_class_init( ofaSettingsClass *klass )
{
	static const gchar *thisfn = "ofa_settings_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = settings_constructed;
	G_OBJECT_CLASS( klass )->get_property = settings_get_property;
	G_OBJECT_CLASS( klass )->set_property = settings_set_property;
	G_OBJECT_CLASS( klass )->dispose = settings_dispose;
	G_OBJECT_CLASS( klass )->finalize = settings_finalize;

	g_type_class_add_private( klass, sizeof( ofaSettingsPrivate ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_TARGET_ID,
			g_param_spec_int(
					PROP_TARGET,
					"Target",
					"Settings target identifier",
					0, SETTINGS_TARGET_LAST, 	0,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
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
	if( !st_user_settings ){
		st_user_settings = g_object_new(
				OFA_TYPE_SETTINGS, PROP_TARGET, SETTINGS_TARGET_USER, NULL );
	}
	if( !st_dossier_settings ){
		st_dossier_settings = g_object_new(
				OFA_TYPE_SETTINGS, PROP_TARGET, SETTINGS_TARGET_DOSSIER, NULL );
	}
}

/**
 * ofa_settings_get_filename:
 */
const gchar *
ofa_settings_get_filename( ofaSettingsTarget target )
{
	ofaSettings *settings;
	const gchar *fname;

	settings_new();
	settings = get_settings_from_target( target );
	fname = settings ? settings->priv->fname : NULL;

	return( fname );
}

/**
 * ofa_settings_get_keyfile:
 */
GKeyFile *
ofa_settings_get_keyfile( ofaSettingsTarget target )
{
	ofaSettings *settings;
	GKeyFile *keyfile;

	settings_new();
	settings = get_settings_from_target( target );
	keyfile = settings ? settings->priv->keyfile : NULL;

	return( keyfile );
}

static void
load_key_file( ofaSettings *settings )
{
	static const gchar *thisfn = "ofa_settings_load_key_file";
	ofaSettingsPrivate *priv;
	GError *error;

	priv = settings->priv;
	priv->keyfile = g_key_file_new();
	priv->fname = NULL;

	switch( priv->target ){
		case SETTINGS_TARGET_USER:
			priv->fname = get_user_conf_filename( settings );
			break;
		case SETTINGS_TARGET_DOSSIER:
			priv->fname = get_dossier_conf_filename( settings );
			break;
		default:
			break;
	}

	if( !my_strlen( priv->fname )){
		g_warning( "%s: target=%d, unable to set configuration filename", thisfn, priv->target );
		return;
	}

	g_debug( "%s: settings=%p, target=%d, fname=%s",
			thisfn, ( void * ) settings, priv->target, priv->fname );

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

static gchar *
get_user_conf_filename( ofaSettings *settings )
{
	const gchar *cstr;
	gchar *dir, *bname, *fname;

	cstr = g_getenv( "OFA_USER_CONF" );
	if( my_strlen( cstr )){
		fname = g_strdup( cstr );
	} else {
		dir = get_default_config_dir( settings );
		bname = g_strdup_printf( "%s.conf", PACKAGE );
		fname = g_build_filename( dir, bname, NULL );
		g_free( bname );
		g_free( dir );
	}

	return( fname );
}

static gchar *
get_dossier_conf_filename( ofaSettings *settings )
{
	const gchar *cstr;
	gchar *dir, *fname;

	cstr = g_getenv( "OFA_DOSSIER_CONF" );
	if( my_strlen( cstr )){
		fname = g_strdup( cstr );
	} else {
		dir = get_default_config_dir( settings );
		fname = g_build_filename( dir, "dossier.conf", NULL );
		g_free( dir );
	}

	return( fname );
}

static gchar *
get_default_config_dir( ofaSettings *settings )
{
	gchar *dir;

	dir = g_build_filename( g_get_home_dir(), ".config", PACKAGE, NULL );
	g_mkdir_with_parents( dir, 0750 );

	return( dir );
}

static gboolean
write_key_file( ofaSettings *settings )
{
	static const gchar *thisfn = "ofa_settings_write_key_file";
	ofaSettingsPrivate *priv;
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

/**
 * ofa_settings_free:
 *
 * Called on application dispose.
 */
void
ofa_settings_free( void )
{
	if( st_user_settings ){
		g_clear_object( &st_user_settings );
	}
	if( st_dossier_settings ){
		g_clear_object( &st_dossier_settings );
	}
}

/**
 * ofa_settings_get_boolean_ex:
 *
 * Returns the specified integer value.
 */
gboolean
ofa_settings_get_boolean_ex( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	GKeyFile *kfile;
	gboolean result;
	gchar *str;

	settings_new();

	result = FALSE;

	kfile = get_keyfile_from_target( target );
	if( kfile ){
		str = g_key_file_get_string( kfile, group, key, NULL );
		result = my_utils_boolean_from_str( str );
		g_free( str );
	}

	return( result );
}

/**
 * ofa_settings_set_boolean_ex:
 */
void
ofa_settings_set_boolean_ex( ofaSettingsTarget target, const gchar *group, const gchar *key, gboolean bvalue )
{
	gchar *str;
	ofaSettings *settings;

	settings_new();

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( OFA_IS_SETTINGS( settings ));

		str = g_strdup_printf( "%s", bvalue ? "True":"False" );
		g_key_file_set_string( settings->priv->keyfile, group, key, str );
		g_free( str );

		write_key_file( settings );
	}
}

/**
 * ofa_settings_get_int_ex:
 *
 * Returns the specified integer value.
 */
gint
ofa_settings_get_int_ex( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	GKeyFile *kfile;
	gint result;
	gchar *str;

	settings_new();

	result = -1;

	kfile = get_keyfile_from_target( target );
	if( kfile ){
		str = g_key_file_get_string( kfile, group, key, NULL );
		if( my_strlen( str )){
			result = atoi( str );
		}

		g_free( str );
	}

	return( result );
}

/**
 * ofa_settings_set_int_ex:
 */
void
ofa_settings_set_int_ex( ofaSettingsTarget target, const gchar *group, const gchar *key, gint ivalue )
{
	gchar *str;
	ofaSettings *settings;

	settings_new();

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( OFA_IS_SETTINGS( settings ));

		str = g_strdup_printf( "%d", ivalue );
		g_key_file_set_string( settings->priv->keyfile, group, key, str );
		g_free( str );

		write_key_file( settings );
	}
}

/**
 * ofa_settings_get_int_list_ex:
 *
 * Returns: a newly allocated GList of int
 *
 * The returned list should be #ofa_settings_free_int_list() by the
 * caller.
 */
GList *
ofa_settings_get_int_list_ex( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	GKeyFile *kfile;
	GList *list;
	gchar *str;
	gchar **array, **iter;

	settings_new();

	list = NULL;

	kfile = get_keyfile_from_target( target );
	if( kfile ){
		str = g_key_file_get_string( kfile, group, key, NULL );

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
 * ofa_settings_set_int_list_ex:
 */
void
ofa_settings_set_int_list_ex( ofaSettingsTarget target, const gchar *group, const gchar *key, const GList *int_list )
{
	GString *str;
	const GList *it;
	ofaSettings *settings;

	settings_new();

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( OFA_IS_SETTINGS( settings ));

		str = g_string_new( "" );
		for( it=int_list ; it ; it=it->next ){
			g_string_append_printf( str, "%d;", GPOINTER_TO_INT( it->data ));
		}

		g_key_file_set_string( settings->priv->keyfile, group, key, str->str );
		g_string_free( str, TRUE );

		write_key_file( settings );
	}
}

/**
 * ofa_settings_get_string_ex:
 *
 * Returns the specified string value as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
ofa_settings_get_string_ex( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	GKeyFile *kfile;
	gchar *str;

	settings_new();

	str = NULL;
	kfile = get_keyfile_from_target( target );

	if( kfile ){
		str = g_key_file_get_string( kfile, group, key, NULL );
	}

	return( str );
}

/**
 * ofa_settings_set_string_ex:
 */
void
ofa_settings_set_string_ex( ofaSettingsTarget target, const gchar *group, const gchar *key, const gchar *svalue )
{
	ofaSettings *settings;

	settings_new();

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( OFA_IS_SETTINGS( settings ));

		g_key_file_set_string( settings->priv->keyfile, group, key, svalue );
		write_key_file( settings );
	}
}

/**
 * ofa_settings_get_string_list_ex:
 *
 * Returns: a newly allocated GList of strings.
 *
 * The returned list should be #ofa_settings_free_string_list() by the
 * caller.
 * caller.
 */
GList *
ofa_settings_get_string_list_ex( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	GKeyFile *kfile;
	GList *list;
	gchar *str;
	gchar **array, **iter;

	settings_new();

	list = NULL;

	kfile = get_keyfile_from_target( target );
	if( kfile ){
		str = g_key_file_get_string( kfile, group, key, NULL );
		/*g_debug( "ofa_settings_get_string_list_ex: str='%s'", str );*/

		if( my_strlen( str )){
			array = str_to_array( str );
			if( array ){
				iter = ( gchar ** ) array;
				while( *iter ){
					/*g_debug( "ofa_settings_get_string_list_ex: iter='%s'", *iter );*/
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
 * ofa_settings_set_string_list_ex:
 */
void
ofa_settings_set_string_list_ex( ofaSettingsTarget target, const gchar *group, const gchar *key, const GList *str_list )
{
	GString *str;
	const GList *it;
	ofaSettings *settings;

	settings_new();

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( OFA_IS_SETTINGS( settings ));

		str = g_string_new( "" );
		for( it=str_list ; it ; it=it->next ){
			g_string_append_printf( str, "%s;", ( const gchar * ) it->data );
		}

		g_key_file_set_string( settings->priv->keyfile, group, key, str->str );
		g_string_free( str, TRUE );

		write_key_file( settings );
	}
}

static GKeyFile *
get_keyfile_from_target( ofaSettingsTarget target )
{
	ofaSettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_val_if_fail( OFA_IS_SETTINGS( settings ), NULL );
		return( settings->priv->keyfile );
	}

	return( NULL );
}

static ofaSettings *
get_settings_from_target( ofaSettingsTarget target )
{
	static const gchar *thisfn = "ofa_settings_get_settings_from_target";

	switch( target ){
		case SETTINGS_TARGET_USER:
			return( st_user_settings );
			break;
		case SETTINGS_TARGET_DOSSIER:
			return( st_dossier_settings );
			break;
		default:
			break;
	}

	g_warning( "%s: unknown target: %d", thisfn, target );
	return( NULL );
}

/*
 * converts a string to an array of strings
 * accepts both:
 * - a semi-comma-separated list of strings (the last separator, if any, is not counted)
 * - a comma-separated list of strings between square brackets (à la GConf)
 *
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

/**
 * ofa_settings_get_groups:
 *
 * Returns: the list of all defined groups.
 *
 * The returned list should be #ofa_settings_free_groups() by the
 * caller.
 */
GSList *
ofa_settings_get_groups( ofaSettingsTarget target )
{
	GSList *slist;
	gchar **array, **iter;

	/* make sure the key file loaded in memory is a real-time image
	 * of the corresponding configuration file
	 * (so that ofaSettingsMonitor is able to do its stuff */
	if( st_dossier_settings ){
		g_clear_object( &st_dossier_settings );
	}

	settings_new();

	array = g_key_file_get_groups( get_keyfile_from_target( target ), NULL );
	slist = NULL;
	iter = array;

	while( *iter ){
		slist = g_slist_prepend( slist, g_strdup( *iter ));
		iter++;
	}

	g_strfreev( array );

	return( slist );
}

/**
 * ofa_settings_dossier_get_keys:
 * @dname: the name of the dossier
 *
 * Returns: the list of keys defined in the dossier.
 *
 * The returned list should be #ofa_settings_dossier_free_keys() by the
 * caller.
 */
GSList *
ofa_settings_dossier_get_keys( const gchar *dname )
{
	static const gchar *thisfn = "ofa_settings_dossier_get_keys";
	gchar *group;
	gchar **array, **iter;
	GSList *list;

	g_debug( "%s: dname=%s", thisfn, dname );

	settings_new();

	list = NULL;
	group = get_dossier_group_from_name( dname );
	array = g_key_file_get_keys( get_keyfile_from_target( SETTINGS_TARGET_DOSSIER ), group, NULL, NULL );
	iter = array;
	while( *iter ){
		list = g_slist_prepend( list, g_strdup( *iter ));
		iter++;
	}
	g_strfreev( array );

	return( list );
}

/**
 * ofa_settings_remove_dossier:
 */
void
ofa_settings_remove_dossier( const gchar *dname )
{
	static const gchar *thisfn = "ofa_settings_remove_dossier";
	gchar *group;

	g_debug( "%s: name=%s", thisfn, dname );

	settings_new();

	group = get_dossier_group_from_name( dname );
	g_key_file_remove_group( st_dossier_settings->priv->keyfile, group, NULL );
	g_free( group );

	write_key_file( st_dossier_settings );
}

/**
 * ofa_settings_remove_exercice:
 */
void
ofa_settings_remove_exercice( const gchar *dname, const gchar *dbname )
{
	static const gchar *thisfn = "ofa_settings_remove_exercice";
	gchar *group;
	GSList *keylist, *it;
	const gchar *key, *keydb;
	gchar *strlist;
	gchar **array;
	gboolean found;
	gint count;

	g_debug( "%s: name=%s, dbname=%s", thisfn, dname, dbname );

	settings_new();
	found = FALSE;
	count = 0;
	keylist = ofa_settings_dossier_get_keys( dname );

	for( it=keylist ; it ; it=it->next ){
		key = ( const gchar * ) it->data;
		if( g_str_has_prefix( key, SETTINGS_DBMS_DATABASE )){
			count += 1;
			if( !found ){
				strlist = ofa_settings_dossier_get_string( dname, key );
				array = g_strsplit( strlist, ";", -1 );
				keydb = ( const gchar * ) *array;
				found = g_utf8_collate( dbname, keydb ) == 0;
				g_strfreev( array );
				g_free( strlist );
				if( found ){
					group = get_dossier_group_from_name( dname );
					g_key_file_remove_key( st_dossier_settings->priv->keyfile, group, key, NULL );
					g_free( group );
					write_key_file( st_dossier_settings );
				}
			}
		}
	}

	ofa_settings_dossier_free_keys( keylist );

	/* at the end, if the searched key was found, and was the only
	 * database key, then remove the all dossier group */
	if( found && count == 1 ){
		ofa_settings_remove_dossier( dname );
	}
}

/**
 * ofa_settings_has_dossier:
 * @dname: the name of the dossier
 *
 * Returns %TRUE if the dossier exists.
 */
gboolean
ofa_settings_has_dossier( const gchar *dname )
{
	gboolean exists;
	gchar *group;

	settings_new();

	group = get_dossier_group_from_name( dname );
	exists = g_key_file_has_group( get_keyfile_from_target( SETTINGS_TARGET_DOSSIER ), group );
	g_free( group );

	return( exists );
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
	return( ofa_settings_dossier_get_string( name, SETTINGS_DBMS_PROVIDER ));
}

/**
 * ofa_settings_dossier_get_int:
 * @dname: the name of the dossier
 * @key: the searched key
 *
 * Returns the key integer for the dossier.
 *
 * But for the "Provider" key that is directly implemented by the
 * ofaIDbms interface, all other keys are supposed to be used only by
 * the DBMS providers.
 */
gint
ofa_settings_dossier_get_int( const gchar *dname, const gchar *key )
{
	static const gchar *thisfn = "ofa_settings_dossier_get_int";
	gchar *group;
	gint ivalue;

	g_debug( "%s: dname=%s, key=%s", thisfn, dname, key );

	settings_new();

	group = get_dossier_group_from_name( dname );
	ivalue = ofa_settings_get_int_ex( SETTINGS_TARGET_DOSSIER, group, key );
	g_free( group );

	return( ivalue );
}

/**
 * ofa_settings_dossier_get_string:
 * @dname: the name of the dossier
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
ofa_settings_dossier_get_string( const gchar *dname, const gchar *key )
{
	static const gchar *thisfn = "ofa_settings_dossier_get_string";
	gchar *group, *svalue;

	g_debug( "%s: dname=%s, key=%s", thisfn, dname, key );

	settings_new();

	group = get_dossier_group_from_name( dname );
	svalue = ofa_settings_get_string_ex( SETTINGS_TARGET_DOSSIER, group, key );
	g_free( group );

	return( svalue );
}

/**
 * ofa_settings_dossier_set_string:
 * @dname: the name of the dossier
 * @key: the searched key
 * @value: the value to be set
 *
 * Set the value for the key in the dossier group.
 */
void
ofa_settings_dossier_set_string( const gchar *dname, const gchar *key, const gchar *svalue )
{
	static const gchar *thisfn = "ofa_settings_dossier_set_string";
	gchar *group;

	g_debug( "%s: dname=%s, key=%s", thisfn, dname, key );

	settings_new();

	group = get_dossier_group_from_name( dname );
	ofa_settings_set_string_ex( SETTINGS_TARGET_DOSSIER, group, key, svalue );
	g_free( group );
}

/**
 * ofa_settings_dossier_get_string_list:
 * @dname: the name of the dossier
 * @key: the searched key
 *
 * Returns the key string list for the dossier.
 *
 * The returned list should be #ofa_settings_free_string_list()
 * by the caller.
 */
GList *
ofa_settings_dossier_get_string_list( const gchar *dname, const gchar *key )
{
	static const gchar *thisfn = "ofa_settings_dossier_get_string_list";
	gchar *group;
	GList *list;

	g_debug( "%s: dname=%s, key=%s", thisfn, dname, key );

	settings_new();

	group = get_dossier_group_from_name( dname );
	list = ofa_settings_get_string_list_ex( SETTINGS_TARGET_DOSSIER, group, key );
	g_free( group );

	return( list );
}

/**
 * ofa_settings_dossier_set_string_list:
 * @dname: the name of the dossier
 * @key: the searched key
 * @list: the value to be set
 *
 * Set the value for the key in the dossier group.
 */
void
ofa_settings_dossier_set_string_list( const gchar *dname, const gchar *key, const GList *list )
{
	static const gchar *thisfn = "ofa_settings_dossier_set_string_list";
	gchar *group;

	g_debug( "%s: dname=%s, key=%s", thisfn, dname, key );

	settings_new();

	group = get_dossier_group_from_name( dname );
	ofa_settings_set_string_list_ex( SETTINGS_TARGET_DOSSIER, group, key, list );
	g_free( group );
}

/**
 * ofa_settings_set_dossier:
 * @name:
 *
 * Define a new user dossier.
 */
gboolean
ofa_settings_create_dossier( const gchar *name, ... )
{
	static const gchar *thisfn = "ofa_settings_create_dossier";
	gchar *group;
	va_list ap;
	const gchar *key, *scontent;
	gint type;
	gchar *str;
	gint icontent;
	GKeyFile *kfile;

	g_debug( "%s: name=%s", thisfn, name );

	settings_new();

	group = get_dossier_group_from_name( name );
	kfile = get_keyfile_from_target( SETTINGS_TARGET_DOSSIER );

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
				if( my_strlen( scontent )){
					if( !g_utf8_collate( key, SETTINGS_DBMS_DATABASE )){
						str = g_strdup_printf( "%s;;;", scontent );
					} else {
						str = g_strdup( scontent );
					}
					g_debug( "%s: setting key group=%s, key=%s, content=%s", thisfn, group, key, str );
					g_key_file_set_string( kfile, group, key, str );
					g_free( str );
				} else {
					g_debug( "%s: removing key group=%s, key=%s", thisfn, group, key );
					g_key_file_remove_key( kfile, group, key, NULL );
				}
				break;

			case SETTINGS_TYPE_INT:
				icontent = va_arg( ap, gint );
				if( icontent > 0 ){
					g_debug( "%s: setting key group=%s, key=%s, content=%d", thisfn, group, key, icontent );
					g_key_file_set_integer( kfile, group, key, icontent );
				} else {
					g_debug( "%s: removing key group=%s, key=%s", thisfn, group, key );
					g_key_file_remove_key( kfile, group, key, NULL );
				}
				break;
		}
	}
	va_end( ap );
	g_free( group );

	return( write_key_file( st_dossier_settings ));
}

#if 0
/**
 * Returns: the list of keys which have the specified common prefix.
 *
 * The returned list should be
 * g_slist_free_full( list, ( GDestroyNotify ) g_free ) by the caller.
 */
GSList *
ofa_settings_get_prefixed_keys( const gchar *prefix )
{
	GSList *list;
	gchar **array, **i;

	settings_new();
	list = NULL;

	array = g_key_file_get_keys( st_user_settings->priv->keyfile, SETTINGS_GROUP_GENERAL, NULL, NULL );
	if( array ){
		i = ( gchar ** ) array;
		while( *i ){
			if( g_str_has_prefix(( const gchar * ) *i, prefix )){
				list = g_slist_prepend( list, g_strdup( *i ));
			}
			i++;
		}
	}
	g_strfreev( array );

	return( g_slist_reverse( list ));
}
#endif

static gchar *
get_dossier_group_from_name( const gchar *dname )
{
	return( g_strdup_printf( "%s %s", SETTINGS_GROUP_DOSSIER, dname ));
}
