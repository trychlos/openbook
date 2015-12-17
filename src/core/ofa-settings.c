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

#include "api/my-utils.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-settings.h"

#include "my-settings.h"

static mySettings *st_user_settings     = NULL;
static mySettings *st_dossier_settings  = NULL;

static void        load_key_file( mySettings **settings, ofaSettingsTarget target );
static mySettings *get_settings_from_target( ofaSettingsTarget target );

/**
 * ofa_settings_new:
 *
 * Allocates a new #ofaSettings object which should be ofa_settings_free()
 * by the caller at the end of the program.
 * This is to be called at early program startup.
 */
void
ofa_settings_new( void )
{
	gboolean called = FALSE;

	if( !called ){
		load_key_file( &st_user_settings, SETTINGS_TARGET_USER );
		load_key_file( &st_dossier_settings, SETTINGS_TARGET_DOSSIER );
		called = TRUE;
	}
}

static void
load_key_file( mySettings **settings, ofaSettingsTarget target )
{
	gchar *name;

	switch( target ){
		case SETTINGS_TARGET_USER:
			name = g_strdup_printf( "%s.conf", PACKAGE );
			*settings = my_settings_new_user_config( name, "OFA_USER_CONF" );
			g_free( name );
			break;

		case SETTINGS_TARGET_DOSSIER:
			*settings = my_settings_new_user_config( "dossier.conf", "OFA_DOSSIER_CONF" );
			break;
	}
}

/**
 * ofa_settings_free:
 *
 * Called on application dispose.
 */
void
ofa_settings_free( void )
{
	g_clear_object( &st_user_settings );
	g_clear_object( &st_dossier_settings );
}

/**
 * ofa_settings_get_settings:
 * @target: the target settings file.
 *
 * Returns: the #myISettings interface implemented by the specified
 * @target settings file.
 *
 * The returned reference is owned by the #ofaSettings code, and
 * should not be released by the caller.
 */
myISettings *
ofa_settings_get_settings( ofaSettingsTarget target )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	return( settings ? MY_ISETTINGS( settings ) : NULL );
}

/**
 * ofa_settings_get_boolean:
 *
 * Returns the specified boolean value, or %FALSE.
 */
gboolean
ofa_settings_get_boolean( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), FALSE );
		return( my_isettings_get_boolean( MY_ISETTINGS( settings ), group, key ));
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofa_settings_set_boolean:
 */
void
ofa_settings_set_boolean( ofaSettingsTarget target, const gchar *group, const gchar *key, gboolean value )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		my_isettings_set_boolean( MY_ISETTINGS( settings ), group, key, value );
	}

	g_return_if_reached();
}

/**
 * ofa_settings_get_uint:
 *
 * Returns the specified integer value, or 0.
 */
gint
ofa_settings_get_uint( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), 0 );
		return( my_isettings_get_uint( MY_ISETTINGS( settings ), group, key ));
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofa_settings_set_uint:
 */
void
ofa_settings_set_uint( ofaSettingsTarget target, const gchar *group, const gchar *key, guint value )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		my_isettings_set_uint( MY_ISETTINGS( settings ), group, key, value );
	}

	g_return_if_reached();
}

/**
 * ofa_settings_get_uint_list:
 *
 * Returns: a newly allocated GList of int
 *
 * The returned list should be #ofa_settings_free_uint_list() by the
 * caller.
 */
GList *
ofa_settings_get_uint_list( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), NULL );
		return( my_isettings_get_uint_list( MY_ISETTINGS( settings ), group, key ));
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_settings_set_uint_list:
 */
void
ofa_settings_set_uint_list( ofaSettingsTarget target, const gchar *group, const gchar *key, const GList *value )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		my_isettings_set_uint_list( MY_ISETTINGS( settings ), group, key, value );
	}

	g_return_if_reached();
}

/**
 * ofa_settings_get_string:
 *
 * Returns the specified string value as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
ofa_settings_get_string( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), NULL );
		return( my_isettings_get_string( MY_ISETTINGS( settings ), group, key ));
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_settings_set_string:
 */
void
ofa_settings_set_string( ofaSettingsTarget target, const gchar *group, const gchar *key, const gchar *value )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		my_isettings_set_string( MY_ISETTINGS( settings ), group, key, value );
	}

	g_return_if_reached();
}

/**
 * ofa_settings_get_string_list:
 *
 * Returns: a newly allocated GList of strings.
 *
 * The returned list should be #ofa_settings_free_string_list() by the
 * caller.
 * caller.
 */
GList *
ofa_settings_get_string_list( ofaSettingsTarget target, const gchar *group, const gchar *key )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), NULL );
		return( my_isettings_get_string_list( MY_ISETTINGS( settings ), group, key ));
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_settings_set_string_list:
 */
void
ofa_settings_set_string_list( ofaSettingsTarget target, const gchar *group, const gchar *key, const GList *value )
{
	mySettings *settings;

	settings = get_settings_from_target( target );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		my_isettings_set_string_list( MY_ISETTINGS( settings ), group, key, value );
	}

	g_return_if_reached();
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
ofa_settings_dossier_get_string( const ofaIDBMeta *meta, const gchar *key )
{
	mySettings *settings;
	gchar *group, *value;

	settings = get_settings_from_target( SETTINGS_TARGET_DOSSIER );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), NULL );
		group = ofa_idbmeta_get_group_name( meta );
		value = my_isettings_get_string( MY_ISETTINGS( settings ), group, key );
		g_free( group );
		return( value );
	}

	g_return_val_if_reached( NULL );
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
ofa_settings_dossier_set_string( const ofaIDBMeta *meta, const gchar *key, const gchar *value )
{
	mySettings *settings;
	gchar *group;

	settings = get_settings_from_target( SETTINGS_TARGET_DOSSIER );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		group = ofa_idbmeta_get_group_name( meta );
		my_isettings_set_string( MY_ISETTINGS( settings ), group, key, value );
		g_free( group );
		return;
	}

	g_return_if_reached();
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
ofa_settings_dossier_get_string_list( const ofaIDBMeta *meta, const gchar *key )
{
	mySettings *settings;
	gchar *group;
	GList *value;

	settings = get_settings_from_target( SETTINGS_TARGET_DOSSIER );
	if( settings ){
		g_return_val_if_fail( MY_IS_SETTINGS( settings ), NULL );
		group = ofa_idbmeta_get_group_name( meta );
		value = my_isettings_get_string_list( MY_ISETTINGS( settings ), group, key );
		g_free( group );
		return( value );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_settings_dossier_set_string_list:
 * @dname: the name of the dossier
 * @key: the searched key
 * @value: the value to be set
 *
 * Set the value for the key in the dossier group.
 */
void
ofa_settings_dossier_set_string_list( const ofaIDBMeta *meta, const gchar *key, const GList *value )
{
	mySettings *settings;
	gchar *group;

	settings = get_settings_from_target( SETTINGS_TARGET_DOSSIER );
	if( settings ){
		g_return_if_fail( MY_IS_SETTINGS( settings ));
		group = ofa_idbmeta_get_group_name( meta );
		my_isettings_set_string_list( MY_ISETTINGS( settings ), group, key, value );
		g_free( group );
		return;
	}

	g_return_if_reached();
}

static mySettings *
get_settings_from_target( ofaSettingsTarget target )
{
	static const gchar *thisfn = "ofa_settings_get_settings_from_target";

	switch( target ){
		case SETTINGS_TARGET_USER:
			return( st_user_settings );

		case SETTINGS_TARGET_DOSSIER:
			return( st_dossier_settings );
	}

	g_warning( "%s: unknown target: %d", thisfn, target );
	return( NULL );
}
