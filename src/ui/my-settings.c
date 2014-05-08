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
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ui/my-int-list.h"
#include "ui/my-marshal.h"
#include "ui/my-settings.h"
#include "ui/my-string-list.h"
#include "ui/my-timeout.h"
#include "ui/my-utils.h"

/* private class data
 */
struct _mySettingsClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* The characteristics of a configuration file.
 * We manage two configuration files:
 * - the global configuration file handles global, readonly, preferences;
 * - the user configuration file handles.. well, user preferences.
 */
typedef struct {
	gchar        *fname;
	gboolean      global;
	GKeyFile     *key_file;
	GFileMonitor *monitor;
	gulong        handler;
}
	sKeyFile;

/* Each consumer may register a callback function which will be
 * triggered when a specific group (i.e. any key in the group), or a
 * specific key is modified.
 */
typedef struct {
	gchar             *monitored_group;
	gchar             *monitored_key;
	mySettingsCallback callback;
	gpointer           user_data;
}
	sConsumer;

/* private instance data
 */
struct _mySettingsPrivate {
	gboolean          dispose_has_run;

	/* properties
	 */
	gboolean          global_is_mandatory;
	mySettingsKeyDef *keydefs;

	/* internals
	 */
	sKeyFile         *global;
	sKeyFile         *user;
	GList            *content;
	GList            *consumers;
	myTimeout         timeout;
};

/* The configuration content is handled as a GList of sKeyValue structs.
 * This list is loaded at initialization time, and then compared each
 * time our file monitors signal us that a change has occured.
 */
typedef struct {
	const mySettingsKeyDef *def;
	gchar                  *group;
	gboolean                global;
	GValue                 *value;
}
	sKeyValue;

/* signals
 */
enum {
	KEY_CHANGED,
	LAST_SIGNAL
};

/* class properties
 */
enum {
	MY_PROP_0,

	MY_PROP_GLOBAL_MANDATORY_ID,
	MY_PROP_TIMEOUT_ID,
	MY_PROP_KEYDEFS_ID,

	MY_PROP_N_PROPERTIES
};

static GObjectClass *st_parent_class           = NULL;
static gint          st_signals[ LAST_SIGNAL ] = { 0 };

static GType             register_type( void );
static void              class_init( mySettingsClass *klass );
static void              instance_init( GTypeInstance *instance, gpointer klass );
static void              instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec );
static void              instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec );
static void              instance_dispose( GObject *object );
static void              instance_finalize( GObject *object );

static sKeyFile         *key_file_new( mySettings *settings, const gchar *dir, gboolean global );
static void              on_keyfile_changed( GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, mySettings *settings );
static void              on_keyfile_changed_timeout( mySettings *settings );
static void              on_key_changed_final_handler( mySettings *settings, gchar *group, gchar *key, GValue *new_value, gboolean global );
static GList            *content_diff( GList *old, GList *new );
static void              release_consumer( sConsumer *consumer );
static void              release_key_file( sKeyFile *keyfile );
static void              release_key_value( sKeyValue *value );
static GList            *content_load_keys( mySettings *settings, GList *content, sKeyFile *keyfile );
static mySettingsKeyDef *get_key_def( mySettings *settings, const gchar *group, const gchar *key );
static sKeyValue        *read_key_value_from_key_file( sKeyFile *keyfile, const gchar *group, const gchar *key, const mySettingsKeyDef *keydef );
static sKeyValue        *read_key_value_from_content( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global );
static sKeyValue        *lookup_key_value( GList *content, const gchar *group, const gchar *key, gboolean global );
static gboolean          write_user_key( mySettings *settings, const gchar *group, const gchar *key, const gchar *string );
static gboolean          write_user_file( mySettings *settings );

GType
my_settings_get_type( void )
{
	static GType st_type = 0;

	if( !st_type ){
		st_type = register_type();
	}

	return( st_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "my_settings_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( mySettingsClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( mySettings ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "mySettings", &info, 0 );

	return( type );
}

static void
class_init( mySettingsClass *klass )
{
	static const gchar *thisfn = "my_settings_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->get_property = instance_get_property;
	object_class->set_property = instance_set_property;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( mySettingsClassPrivate, 1 );

	/*
	 * install class properties
	 */
	g_object_class_install_property( object_class, MY_PROP_GLOBAL_MANDATORY_ID,
			g_param_spec_boolean(
					MY_PROP_GLOBAL_MANDATORY,
					"Global mandatory",
					"Whether global preferences are said to be mandatory",
					FALSE,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, MY_PROP_TIMEOUT_ID,
			g_param_spec_int(
					MY_PROP_TIMEOUT,
					"Timeout",
					"The timeout when signaling external modifications",
					0,
					INT_MAX,
					100,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	/*
	 * mySettings::settings-key-changed:
	 *
	 * This signal is sent by mySettings when the value of a specific
	 * group (i.e. any key in this group), or the value of a specific
	 * key, is modified.
	 *
	 * Arguments passed to the callback are the group, the key, the new
	 * value, and whether the modification comes from the global
	 * preferences or from the user one.
	 *
	 * Handler is of type:
	 * void ( *handler )( mySettings *settings,
	 * 						const gchar *group,
	 * 						const gchar *key,
	 * 						GValue *value,
	 * 						gboolean global,
	 * 						gpointer user_data );
	 *
	 * The default class handler frees these datas.
	 */
	st_signals[ KEY_CHANGED ] = g_signal_new_class_handler(
				SETTINGS_SIGNAL_KEY_CHANGED,
				MY_TYPE_SETTINGS,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_key_changed_final_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				my_cclosure_marshal_VOID__STRING_STRING_POINTER_BOOLEAN,
				G_TYPE_NONE,
				4,
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_BOOLEAN );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "my_settings_instance_init";
	mySettings *self;
	gchar *dir;
	GList *content;

	g_return_if_fail( MY_IS_SETTINGS( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = MY_SETTINGS( instance );

	self->private = g_new0( mySettingsPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->global = NULL;
	self->private->user = NULL;
	self->private->content = NULL;
	self->private->consumers = NULL;

	self->private->timeout.handler = ( myTimeoutFunc ) on_keyfile_changed_timeout;
	self->private->timeout.user_data = self;
	self->private->timeout.source_id = 0;

	g_debug( "%s: reading global configuration", thisfn );
	dir = g_build_filename( SYSCONFDIR, "xdg", PACKAGE, NULL );
	self->private->global = key_file_new( self, dir, TRUE );
	g_free( dir );
	content = content_load_keys( self, NULL, self->private->global );

	g_debug( "%s: reading user configuration", thisfn );
	dir = g_build_filename( g_get_home_dir(), ".config", PACKAGE, NULL );
	g_mkdir_with_parents( dir, 0750 );
	self->private->user = key_file_new( self, dir, FALSE );
	g_free( dir );
	content = content_load_keys( self, content, self->private->user );

	self->private->content = g_list_copy( content );
	g_list_free( content );
}

static void
instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	mySettings *self;

	g_return_if_fail( MY_IS_SETTINGS( object ));
	self = MY_SETTINGS( object );

	if( !self->private->dispose_has_run ){

		switch( property_id ){
			case MY_PROP_GLOBAL_MANDATORY_ID:
				g_value_set_boolean( value, self->private->global_is_mandatory );
				break;

			case MY_PROP_TIMEOUT_ID:
				g_value_set_int( value, self->private->timeout.timeout );
				break;

			case MY_PROP_KEYDEFS_ID:
				g_value_set_pointer( value, self->private->keydefs	 );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	mySettings *self;

	g_return_if_fail( MY_IS_SETTINGS( object ));
	self = MY_SETTINGS( object );

	if( !self->private->dispose_has_run ){

		switch( property_id ){
			case MY_PROP_GLOBAL_MANDATORY_ID:
				self->private->global_is_mandatory = g_value_get_boolean( value );
				break;

			case MY_PROP_TIMEOUT_ID:
				self->private->timeout.timeout = g_value_get_int( value );
				break;

			case MY_PROP_KEYDEFS_ID:
				self->private->keydefs = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_dispose( GObject *object )
{
	static const gchar *thisfn = "my_settings_instance_dispose";
	mySettings *self;

	g_return_if_fail( MY_IS_SETTINGS( object ));

	self = MY_SETTINGS( object );

	if( !self->private->dispose_has_run ){

		g_debug( "%s: object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

		self->private->dispose_has_run = TRUE;

		release_key_file( self->private->global );
		release_key_file( self->private->user );

		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( object );
		}
	}
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "my_settings_instance_finalize";
	mySettings *self;

	g_return_if_fail( MY_IS_SETTINGS( object ));

	g_debug( "%s: object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	self = MY_SETTINGS( object );

	g_list_foreach( self->private->content, ( GFunc ) release_key_value, NULL );
	g_list_free( self->private->content );

	g_list_foreach( self->private->consumers, ( GFunc ) release_consumer, NULL );
	g_list_free( self->private->consumers );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( object );
	}
}

/*
 * called at instance initialisation
 * allocate and load the key files for global and user preferences
 */
static sKeyFile *
key_file_new( mySettings *settings, const gchar *dir, gboolean global )
{
	static const gchar *thisfn = "my_settings_key_file_new";
	sKeyFile *keyfile;
	GError *error;
	GFile *file;

	keyfile = g_new0( sKeyFile, 1 );

	keyfile->key_file = g_key_file_new();
	keyfile->fname = g_strdup_printf( "%s/%s.conf", dir, PACKAGE );
	my_utils_file_list_perms( keyfile->fname, thisfn );

	error = NULL;
	file = g_file_new_for_path( keyfile->fname );
	keyfile->monitor = g_file_monitor_file( file, 0, NULL, &error );
	if( error ){
		g_warning( "%s: %s: %s", thisfn, keyfile->fname, error->message );
		g_error_free( error );
	} else {
		keyfile->handler = g_signal_connect( keyfile->monitor, "changed", ( GCallback ) on_keyfile_changed, settings );
	}
	g_object_unref( file );

	keyfile->global = global;

	return( keyfile );
}

/*
 * one of the two monitored configuration files have changed on the disk
 * we do not try to identify which keys have actually change
 * instead we trigger each registered consumer for the 'global' event
 *
 * consumers which register for the 'global_conf' event are recorded
 * with a NULL key
 */
static void
on_keyfile_changed( GFileMonitor *monitor,
		GFile *file, GFile *other_file, GFileMonitorEvent event_type, mySettings *settings )
{
	my_timeout_event( &settings->private->timeout );
}

static void
on_keyfile_changed_timeout( mySettings *settings )
{
	static const gchar *thisfn = "my_settings_on_keyfile_changed_timeout";
	GList *new_content;
	GList *modifs;
	GList *ic, *im;
	const sKeyValue *changed;
	const sConsumer *consumer;
	gchar *key;
	const gchar *value;

	/* last individual notification is older that the timeout property
	 * we may so suppose that the burst is terminated
	 */
	new_content = content_load_keys( settings, NULL, settings->private->global );
	new_content = content_load_keys( settings, new_content, settings->private->user );
	modifs = content_diff( settings->private->content, new_content );

	g_debug( "%s: %d found update(s)", thisfn, g_list_length( modifs ));
	for( im = modifs ; im ; im = im->next ){
		changed = ( const sKeyValue * ) im->data;
		value = g_value_get_string( changed->value );
		g_debug( "%s: group=%s, key=%s, value=%s", thisfn, changed->group, changed->def->key, value );
	}

	/* for each modification found,
	 * - check if a consumer has registered for this key, and triggers callback if apply
	 * - send a notification message
	 */
	for( im = modifs ; im ; im = im->next ){
		changed = ( const sKeyValue * ) im->data;

		for( ic = settings->private->consumers ; ic ; ic = ic->next ){
			consumer = ( const sConsumer * ) ic->data;
			key = consumer->monitored_key;

			if( !strcmp( changed->def->key, key )){
				( *( mySettingsCallback ) consumer->callback )(
						settings,
						changed->group,
						changed->def->key,
						g_value_get_pointer( changed->value ),
						changed->global,
						consumer->user_data );
			}
		}

		g_debug( "%s: sending signal for group=%s, key=%s", thisfn, changed->group, changed->def->key );
		g_signal_emit_by_name( settings,
				SETTINGS_SIGNAL_KEY_CHANGED,
				changed->group, changed->def->key, changed->value, changed->global );
	}

	g_debug( "%s: releasing content", thisfn );
	g_list_foreach( settings->private->content, ( GFunc ) release_key_value, NULL );
	g_list_free( settings->private->content );
	settings->private->content = new_content;

	g_debug( "%s: releasing modifs", thisfn );
	g_list_foreach( modifs, ( GFunc ) release_key_value, NULL );
	g_list_free( modifs );
}

static void
on_key_changed_final_handler( mySettings *settings, gchar *group, gchar *key, GValue *new_value, gboolean global )
{
	g_debug( "my_settings_on_key_changed_final_handler: group=%s, key=%s", group, key );
	my_utils_g_value_dump( new_value );
}

/*
 * returns a list of modified sKeyValue
 * - order in the lists is not signifiant
 * - the global flag is not signifiant
 * - a key is modified:
 *   > if it appears in new
 *   > if it disappears: the value is so reset to its default
 *   > if the value has been modified
 *
 * we return here a new list, with newly allocated KeyValue structs
 * which hold the new value of each modified key
 */
static GList *
content_diff( GList *old, GList *new )
{
	GList *diffs, *io, *in;
	sKeyValue *kold, *knew, *kdiff;
	gboolean found;

	diffs = NULL;

	for( io = old ; io ; io = io->next ){
		kold = ( sKeyValue * ) io->data;
		found = FALSE;
		for( in = new ; in && !found ; in = in->next ){
			knew = ( sKeyValue * ) in->data;
			if(( gpointer ) kold->def == ( gpointer ) knew->def ){
				found = TRUE;
				if( my_utils_g_value_compare( kold->value, knew->value )){
					/* a key has been modified */
					kdiff = g_new0( sKeyValue, 1 );
					kdiff->def = knew->def;
					kdiff->global = knew->global;
					kdiff->value = my_utils_g_value_dup( knew->value );
					diffs = g_list_prepend( diffs, kdiff );
				}
			}
		}
		if( !found ){
			/* a key has disappeared */
			kdiff = g_new0( sKeyValue, 1 );
			kdiff->def = kold->def;
			kdiff->global = FALSE;
			kdiff->value = my_utils_g_value_new_from_string( kold->def->type, kold->def->default_value );
			diffs = g_list_prepend( diffs, kdiff );
		}
	}

	for( in = new ; in ; in = in->next ){
		knew = ( sKeyValue * ) in->data;
		found = FALSE;
		for( io = old ; io && !found ; io = io->next ){
			kold = ( sKeyValue * ) io->data;
			if(( gpointer ) kold->def == ( gpointer ) knew->def ){
				found = TRUE;
			}
		}
		if( !found ){
			/* a key is new */
			kdiff = g_new0( sKeyValue, 1 );
			kdiff->def = knew->def;
			kdiff->global = knew->global;
			kdiff->value = my_utils_g_value_dup( knew->value );
			diffs = g_list_prepend( diffs, kdiff );
		}
	}

	return( diffs );
}

/*
 * called from instance_finalize
 * release the list of registered consumers
 */
static void
release_consumer( sConsumer *consumer )
{
	g_free( consumer->monitored_group );
	g_free( consumer->monitored_key );
	g_free( consumer );
}

/*
 * called from instance_dispose
 * release the opened and monitored GKeyFiles
 */
static void
release_key_file( sKeyFile *key_file )
{
	g_key_file_free( key_file->key_file );
	if( key_file->monitor ){
		if( key_file->handler ){
			g_signal_handler_disconnect( key_file->monitor, key_file->handler );
		}
		g_file_monitor_cancel( key_file->monitor );
		g_object_unref( key_file->monitor );
	}
	g_free( key_file->fname );
	g_free( key_file );
}

/*
 * called from instance_finalize
 * release a KeyValue struct
 */
static void
release_key_value( sKeyValue *value )
{
	g_free( value->group );
	g_value_unset( value->value );
	g_free( value );
}

/* add the content of a configuration files to those already loaded
 *
 * when the two configuration files have been read, then the content of
 * _the_ configuration has been loaded, while indivdually preserving
 * global and user keys
 */
static GList *
content_load_keys( mySettings *settings, GList *content, sKeyFile *keyfile )
{
	static const gchar *thisfn = "my_settings_content_load_keys";
	GError *error;
	gchar **groups, **ig;
	gchar **keys, **ik;
	sKeyValue *key_value;
	mySettingsKeyDef *key_def;

	error = NULL;
	if( !g_key_file_load_from_file( keyfile->key_file, keyfile->fname, G_KEY_FILE_KEEP_COMMENTS, &error )){
		if( error->code != G_FILE_ERROR_NOENT ){
			g_warning( "%s: %s (%d) %s", thisfn, keyfile->fname, error->code, error->message );
		} else {
			g_debug( "%s: %s: file doesn't exist", thisfn, keyfile->fname );
		}
		g_error_free( error );
		error = NULL;

	} else {
		groups = g_key_file_get_groups( keyfile->key_file, NULL );
		ig = groups;
		while( *ig ){
			keys = g_key_file_get_keys( keyfile->key_file, *ig, NULL, NULL );
			ik = keys;
			while( *ik ){
				key_def = get_key_def( settings, *ig, *ik );
				if( key_def ){
					key_value = read_key_value_from_key_file( keyfile, *ig, *ik, key_def );
					if( key_value ){
						key_value->global = keyfile->global;
						content = g_list_prepend( content, key_value );
					}
				}
				ik++;
			}
			g_strfreev( keys );
			ig++;
		}
		g_strfreev( groups );
	}

	return( content );
}

static mySettingsKeyDef *
get_key_def( mySettings *settings, const gchar *group, const gchar *key )
{
	static const gchar *thisfn = "my_settings_get_key_def";
	mySettingsKeyDef *found = NULL;
	mySettingsKeyDef *idef;

	g_return_val_if_fail( group && strlen( group ), NULL );
	g_return_val_if_fail( key && strlen( key ), NULL );

	idef = settings->private->keydefs;
	while(( idef->group || idef->key ) && !found ){
		if( !idef->group || ( idef->group && !strcmp( group, idef->group ))){
			if( idef->key && !strcmp( idef->key, key )){
				found = idef;
			}
		}
		idef++;
	}
	if( !found ){
		g_warning( "%s: no mySettingsKeyDef definition found for key='%s'", thisfn, key );
	}

	return( found );
}

static sKeyValue *
read_key_value_from_key_file( sKeyFile *keyfile, const gchar *group, const gchar *key, const mySettingsKeyDef *key_def )
{
	static const gchar *thisfn = "my_settings_read_key_value_from_key_file";
	sKeyValue *value;
	gchar *str;
	GError *error;

	value = NULL;
	error = NULL;
	str = NULL;

	switch( key_def->type ){

		case MY_SETTINGS_BOOLEAN:
		case MY_SETTINGS_STRING:
		case MY_SETTINGS_STRING_LIST:
		case MY_SETTINGS_INT:
		case MY_SETTINGS_INT_LIST:
			str = g_key_file_get_string( keyfile->key_file, group, key, &error );
			if( error ){
				if( error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND && error->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND ){
					g_warning( "%s: %s", thisfn, error->message );
				}
				g_error_free( error );

			/* key exists, but may be empty */
			} else {
				value = g_new0( sKeyValue, 1 );
				value->def = key_def;
				value->value = my_utils_g_value_new_from_string( key_def->type, str );
			}
			break;

		default:
			g_warning( "%s: group=%s, key=%s - unmanaged type: %ld", thisfn, group, key, ( long ) key_def->type );
			return( NULL );
	}

	if( value ){
		g_debug( "%s: group=%s, key=%s, value=%s, global=%s",
				thisfn, group, key, str, keyfile->global ? "True":"False" );
	}

	g_free( str );

	return( value );
}

/**
 * my_settings_register_callback:
 * @group: the group to be monitored.
 * @key: the key to be monitored, or #NULL for all the keys of the group.
 * @callback: the function to be called when the value of the key changes.
 * @user_data: data to be passed to the @callback function.
 *
 * Registers a new consumer of the monitoring of the @key.
 */
void
my_settings_register_callback( mySettings *settings,
		const gchar *group, const gchar *key, mySettingsCallback callback, gpointer user_data )
{
	static const gchar *thisfn = "my_settings_register_callback";

	g_debug( "%s: key=%s, callback=%p, user_data=%p",
			thisfn, key, ( void * ) callback, ( void * ) user_data );

	sConsumer *consumer = g_new0( sConsumer, 1 );
	consumer->monitored_group = g_strdup( group );
	consumer->monitored_key = g_strdup( key );
	consumer->callback = callback;
	consumer->user_data = user_data;

	settings->private->consumers =
			g_list_prepend( settings->private->consumers, consumer );
}

/**
 * my_settings_get_groups:
 * @settings: the #mySettings object
 * @mode: whether we want all groups, or only those of the user of
 *  global configuration file
 *
 * Returns: the list of groups in the configuration; this list should be
 * my_utils_slist_free() by the caller.
 */
GSList *
my_settings_get_groups( mySettings *settings, mySettingsMode mode )
{
	GSList *groups;
	gchar **array;

	groups = NULL;

	if( mode == MY_SETTINGS_GLOBAL_ONLY || mode == MY_SETTINGS_ALL ){
		array = g_key_file_get_groups( settings->private->global->key_file, NULL );
		if( array ){
			groups = my_utils_slist_from_array(( const gchar ** ) array );
			g_strfreev( array );
		}
	}

	if( mode == MY_SETTINGS_USER_ONLY || mode == MY_SETTINGS_ALL ){
		array = g_key_file_get_groups( settings->private->user->key_file, NULL );
		if( array ){
			groups = g_slist_concat( groups, my_utils_slist_from_array(( const gchar ** ) array ));
			g_strfreev( array );
		}
	}

	return( groups );
}

/**
 * my_settings_get_boolean:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @mode: a mySettingsMode indicator which says whether the search
 *  must be made in user, global or both preferences.
 * @found: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the searched @key has been found (%TRUE), or if the returned
 *  value comes from default (%FALSE).
 * @global: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the returned value has been read from global preferences
 *  (%TRUE), or from the user preferences (%FALSE). When the @key has not
 *  been found, @global is set to %FALSE.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: the value of the key, of its default value if not found.
 */
gboolean
my_settings_get_boolean( mySettings *settings,
		const gchar *group, const gchar *key, mySettingsMode mode,
		gboolean *found, gboolean *global )
{
	gboolean value;
	sKeyValue *key_value;
	mySettingsKeyDef *key_def;

	value = FALSE;
	key_value = read_key_value_from_content( settings, group, key, mode, found, global );

	if( key_value ){
		value = g_value_get_boolean( key_value->value );
		release_key_value( key_value );

	} else {
		key_def = get_key_def( settings, group, key );
		if( key_def ){
			value = ( key_def->default_value ? ( strcasecmp( key_def->default_value, "true" ) == 0 || atoi( key_def->default_value ) != 0 ) : FALSE );
		}
	}

	return( value );
}

/**
 * my_settings_get_string:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @mode: a mySettingsMode indicator which says whether the search
 *  must be made in user, global or both preferences.
 * @found: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the searched @key has been found (%TRUE), or if the returned
 *  value comes from default (%FALSE).
 * @global: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the returned value has been read from global preferences
 *  (%TRUE), or from the user preferences (%FALSE). When the @key has not
 *  been found, @global is set to %FALSE.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: the value of the key as a newly allocated string, which should
 * be g_free() by the caller.
 */
gchar *
my_settings_get_string( mySettings *settings,
		const gchar *group, const gchar *key, mySettingsMode mode,
		gboolean *found, gboolean *global )
{
	gchar *value;
	sKeyValue *key_value;
	mySettingsKeyDef *key_def;

	value = NULL;
	key_value = read_key_value_from_content( settings, group, key, mode, found, global );

	if( key_value ){
		value = g_strdup( g_value_get_string( key_value->value ));
		release_key_value( key_value );

	} else {
		key_def = get_key_def( settings, group, key );
		if( key_def && key_def->default_value ){
			value = g_strdup( key_def->default_value );
		}
	}

	return( value );
}

/**
 * my_settings_get_string_list:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @mode: a mySettingsMode indicator which says whether the search
 *  must be made in user, global or both preferences.
 * @found: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the searched @key has been found (%TRUE), or if the returned
 *  value comes from default (%FALSE).
 * @global: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the returned value has been read from global preferences
 *  (%TRUE), or from the user preferences (%FALSE). When the @key has not
 *  been found, @global is set to %FALSE.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: the value of the key as a newly allocated list of strings.
 * The returned list should be my_utils_slist_free() by the caller.
 */
GSList *
my_settings_get_string_list( mySettings *settings,
		const gchar *group, const gchar *key, mySettingsMode mode,
		gboolean *found, gboolean *global )
{
	GSList *value;
	sKeyValue *key_value;
	mySettingsKeyDef *key_def;
	myStringList *list;

	value = NULL;
	key_value = read_key_value_from_content( settings, group, key, mode, found, global );

	if( key_value ){
		list = my_string_list_new_from_g_value( key_value->value );
		value = my_string_list_get_gslist( list );
		my_string_list_free( list );
		release_key_value( key_value );

	} else {
		key_def = get_key_def( settings, group, key );
		if( key_def && key_def->default_value && strlen( key_def->default_value )){
			value = g_slist_append( NULL, g_strdup( key_def->default_value ));
		}
	}

	return( value );
}

/**
 * my_settings_get_int:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @mode: a mySettingsMode indicator which says whether the search
 *  must be made in user, global or both preferences.
 * @found: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the searched @key has been found (%TRUE), or if the returned
 *  value comes from default (%FALSE).
 * @global: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the returned value has been read from global preferences
 *  (%TRUE), or from the user preferences (%FALSE). When the @key has not
 *  been found, @global is set to %FALSE.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: the value of the key.
 */
gint
my_settings_get_int( mySettings *settings,
		const gchar *group, const gchar *key, mySettingsMode mode,
		gboolean *found, gboolean *global )
{
	gint value;
	mySettingsKeyDef *key_def;
	sKeyValue *key_value;

	value = 0;
	key_value = read_key_value_from_content( settings, group, key, mode, found, global );

	if( key_value ){
		value = g_value_get_int( key_value->value );
		release_key_value( key_value );

	} else {
		key_def = get_key_def( settings, group, key );
		if( key_def && key_def->default_value ){
			value = atoi( key_def->default_value );
		}
	}

	return( value );
}

/**
 * my_settings_get_int_list:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @mode: a mySettingsMode indicator which says whether the search
 *  must be made in user, global or both preferences.
 * @found: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the searched @key has been found (%TRUE), or if the returned
 *  value comes from default (%FALSE).
 * @global: if not %NULL, a pointer to a gboolean in which we will store
 *  whether the returned value has been read from global preferences
 *  (%TRUE), or from the user preferences (%FALSE). When the @key has not
 *  been found, @global is set to %FALSE.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: the value of the key as a newly allocated list of uints.
 * The returned list should be g_list_free() by the caller.
 */
GList *
my_settings_get_int_list( mySettings *settings,
		const gchar *group, const gchar *key, mySettingsMode mode,
		gboolean *found, gboolean *global )
{
	GList *value;
	mySettingsKeyDef *key_def;
	sKeyValue *key_value;
	myIntList *list;

	value = NULL;
	key_value = read_key_value_from_content( settings, group, key, mode, found, global );

	if( key_value ){
		list = my_int_list_new_from_g_value( key_value->value );
		value = my_int_list_get_glist( list );
		my_int_list_free( list );
		release_key_value( key_value );

	} else {
		key_def = get_key_def( settings, group, key );
		if( key_def && key_def->default_value ){
			value = g_list_append( NULL, GINT_TO_POINTER( atoi( key_def->default_value )));
		}
	}

	return( value );
}

static sKeyValue *
read_key_value_from_content( mySettings *settings, const gchar *group, const gchar *key, mySettingsMode mode, gboolean *found, gboolean *global )
{
	mySettingsKeyDef *key_def;
	sKeyValue *key_value;

	key_value = NULL;
	if( found ){
		*found = FALSE;
	}
	if( global ){
		*global = FALSE;
	}

	key_def = get_key_def( settings, group, key );
	if( !key_def ){
		return( NULL );
	}

	/* if global is mandatory, only search in user preferences if not
	 * found in global
	 * else (user supersedes global), only search in global if not found
	 * in user preferences
	 */
	if( settings->private->global_is_mandatory ){
		if( mode == MY_SETTINGS_GLOBAL_ONLY || mode == MY_SETTINGS_ALL ){
			key_value = lookup_key_value( settings->private->content, group, key, TRUE );
		}
		if( !key_value ){
			if( mode == MY_SETTINGS_USER_ONLY || mode == MY_SETTINGS_ALL ){
				key_value = lookup_key_value( settings->private->content, group, key, FALSE );
			}
		}
	} else {
		if( mode == MY_SETTINGS_USER_ONLY || mode == MY_SETTINGS_ALL ){
			key_value = lookup_key_value( settings->private->content, group, key, FALSE );
		}
		if( !key_value ){
			if( mode == MY_SETTINGS_GLOBAL_ONLY || mode == MY_SETTINGS_ALL ){
				key_value = lookup_key_value( settings->private->content, group, key, TRUE );
			}
		}
	}

	if( key_value ){
		if( found ){
			*found = TRUE;
		}
		if( global ){
			*global = key_value->global;
		}
		key_value->def = key_def;
	}

	return( key_value );
}

static sKeyValue *
lookup_key_value( GList *content, const gchar *group, const gchar *key, gboolean global )
{
	return( NULL );
}

/**
 * my_settings_set_boolean:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @value: the boolean to be written.
 *
 * This function writes @value as a user preference.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: %TRUE is the writing has been successful, %FALSE else.
 */
gboolean
my_settings_set_boolean( mySettings *settings, const gchar *group, const gchar *key, gboolean value )
{
	gchar *string;
	gboolean ok;

	string = g_strdup_printf( "%s", value ? "true" : "false" );
	ok = write_user_key( settings, group, key, string );
	g_free( string );

	return( ok );
}

/**
 * my_settings_set_string:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @value: the string to be written.
 *
 * This function writes @value as a user preference.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: %TRUE is the writing has been successful, %FALSE else.
 */
gboolean
my_settings_set_string( mySettings *settings, const gchar *group, const gchar *key, const gchar *value )
{
	return( write_user_key( settings, group, key, value ));
}

/**
 * my_settings_set_string_list:
 * @settings: this #mySettings object.
 * @group: the group in which search for the key.
 * @key: the key whose value is to be returned.
 * @value: the list of strings to be written.
 *
 * This function writes @value as a user preference.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: %TRUE is the writing has been successful, %FALSE else.
 */
gboolean
my_settings_set_string_list( mySettings *settings, const gchar *group, const gchar *key, const GSList *value )
{
	GString *string;
	const GSList *it;
	gboolean ok;

	string = g_string_new( "" );
	for( it = value ; it ; it = it->next ){
		g_string_append_printf( string, "%s;", ( const gchar * ) it->data );
	}
	ok = write_user_key( settings, group, key, string->str );
	g_string_free( string, TRUE );

	return( ok );
}

/**
 * my_settings_set_int:
 * @settings: this #mySettings object.
 * @group: the group in the keyed file;
 * @key: the key whose value is to be returned.
 * @value: the unsigned integer to be written.
 *
 * This function writes @value as a user preference.
 *
 * Returns: %TRUE is the writing has been successful, %FALSE else.
 */
gboolean
my_settings_set_int( mySettings *settings, const gchar *group, const gchar *key, int value )
{
	gchar *string;
	gboolean ok;

	string = g_strdup_printf( "%d", value );
	ok = write_user_key( settings, group, key, string );
	g_free( string );

	return( ok );
}

/**
 * my_settings_set_int_list:
 * @settings: this #mySettings object.
 * @group: the group in the keyed file;
 * @key: the key whose value is to be returned.
 * @value: the list of unsigned integers to be written.
 *
 * This function writes @value as a user preference.
 *
 * This function should only be called for unambiguous keys; the resultat
 * is otherwise undefined (and rather unpredictable).
 *
 * Returns: %TRUE is the writing has been successful, %FALSE else.
 */
gboolean
my_settings_set_int_list( mySettings *settings, const gchar *group, const gchar *key, const GList *value )
{
	GString *string;
	const GList *it;
	gboolean ok;

	string = g_string_new( "" );
	for( it = value ; it ; it = it->next ){
		g_string_append_printf( string, "%u;", GPOINTER_TO_UINT( it->data ));
	}
	ok = write_user_key( settings, group, key, string->str );
	g_string_free( string, TRUE );

	return( ok );
}

static gboolean
write_user_key( mySettings *settings, const gchar *group, const gchar *key, const gchar *string )
{
	static const gchar *thisfn = "my_settings_write_user_key";
	mySettingsKeyDef *key_def;
	const gchar *wgroup;
	gboolean ok;
	GError *error;

	ok = FALSE;

	wgroup = group;
	if( !group ){
		key_def = get_key_def( settings, group, key );
		if( key_def ){
			wgroup = key_def->group;
		}
	}
	if( wgroup ){
		ok = TRUE;

		if( string ){
			g_key_file_set_string( settings->private->user->key_file, wgroup, key, string );

		} else {
			error = NULL;
			ok = g_key_file_remove_key( settings->private->user->key_file, wgroup, key, &error );
			if( error ){
				g_warning( "%s: g_key_file_remove_key: %s", thisfn, error->message );
				g_error_free( error );
			}
		}

		ok &= write_user_file( settings );
	}

	return( ok );
}

static gboolean
write_user_file( mySettings *settings )
{
	static const gchar *thisfn = "my_settings_write_user_file";
	gchar *data;
	GFile *file;
	GFileOutputStream *stream;
	GError *error;
	gsize length;

	error = NULL;
	data = g_key_file_to_data( settings->private->user->key_file, &length, NULL );
	file = g_file_new_for_path( settings->private->user->fname );

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
