/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "core/ofa-open-prefs.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	myISettings *settings;
	gchar       *group;
	gchar       *key;

	/* the core data
	 */
	gboolean     display_notes;
	gboolean     non_empty_notes;
	gboolean     display_properties;
	gboolean     check_balances;
	gboolean     check_integrity;
}
	ofaOpenPrefsPrivate;

static void read_settings( ofaOpenPrefs *self );
static void write_settings( ofaOpenPrefs *self );

G_DEFINE_TYPE_EXTENDED( ofaOpenPrefs, ofa_open_prefs, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaOpenPrefs ))

static void
open_prefs_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_open_prefs_finalize";
	ofaOpenPrefsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPEN_PREFS( instance ));

	/* free data members here */
	priv = ofa_open_prefs_get_instance_private( OFA_OPEN_PREFS( instance ));

	g_free( priv->group );
	g_free( priv->key );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_open_prefs_parent_class )->finalize( instance );
}

static void
open_prefs_dispose( GObject *instance )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPEN_PREFS( instance ));

	priv = ofa_open_prefs_get_instance_private( OFA_OPEN_PREFS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_open_prefs_parent_class )->dispose( instance );
}

static void
ofa_open_prefs_init( ofaOpenPrefs *self )
{
	static const gchar *thisfn = "ofa_open_prefs_instance_init";
	ofaOpenPrefsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPEN_PREFS( self ));

	priv = ofa_open_prefs_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings = NULL;
	priv->group = NULL;
	priv->key = NULL;
	priv->display_notes = FALSE;
	priv->non_empty_notes = FALSE;
	priv->display_properties = FALSE;
	priv->check_balances = FALSE;
	priv->check_integrity = FALSE;
}

static void
ofa_open_prefs_class_init( ofaOpenPrefsClass *klass )
{
	static const gchar *thisfn = "ofa_open_prefs_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = open_prefs_dispose;
	G_OBJECT_CLASS( klass )->finalize = open_prefs_finalize;
}

/**
 * ofa_open_prefs_new:
 * @settings: the settings interface to be used;
 *  may be either the user or the dossier settings interface.
 * @group: the group name.
 * @key: the key.
 *
 * Returns: a new #ofaOpenPrefs object.
 */
ofaOpenPrefs *
ofa_open_prefs_new( myISettings *settings, const gchar *group, const gchar *key )
{
	ofaOpenPrefs *prefs;
	ofaOpenPrefsPrivate *priv;

	g_return_val_if_fail( settings && MY_IS_ISETTINGS( settings ), NULL );
	g_return_val_if_fail( my_strlen( group ), NULL );
	g_return_val_if_fail( my_strlen( key ), NULL );

	prefs = g_object_new( OFA_TYPE_OPEN_PREFS, NULL );

	priv = ofa_open_prefs_get_instance_private( prefs );

	priv->settings = settings;
	priv->group = g_strdup( group );
	priv->key = g_strdup( key );

	read_settings( prefs );

	return( prefs );
}

/**
 * ofa_open_prefs_get_display_notes:
 * @prefs: this #ofaOpenPrefs object.
 *
 * Returns: %TRUE if notes should be displayed when opening the dossier.
 */
gboolean
ofa_open_prefs_get_display_notes( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ), FALSE );

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->display_notes );
}

/**
 * ofa_open_prefs_set_display_notes:
 * @prefs: this #ofaOpenPrefs object.
 * @display_notes: whether notes should be displayed when opening the dossier.
 *
 * Set @display_notes.
 */
void
ofa_open_prefs_set_display_notes( ofaOpenPrefs *prefs, gboolean display_notes )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->display_notes = display_notes;
}

/**
 * ofa_open_prefs_get_non_empty_notes:
 * @prefs: this #ofaOpenPrefs object.
 *
 * Returns: %TRUE if only non-empty notes should be displayed when
 * opening the dossier,
 */
gboolean
ofa_open_prefs_get_non_empty_notes( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ), FALSE );

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->non_empty_notes );
}

/**
 * ofa_open_prefs_set_non_empty_notes:
 * @prefs: this #ofaOpenPrefs object.
 * @non_empty_notes: whether only non-empty notes should be displayed
 *  when opening the dossier.
 *
 * Set @non_empty_notes.
 */
void
ofa_open_prefs_set_non_empty_notes( ofaOpenPrefs *prefs, gboolean non_empty_notes )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->non_empty_notes = non_empty_notes;
}

/**
 * ofa_open_prefs_get_display_properties:
 * @prefs: this #ofaOpenPrefs object.
 *
 * Returns: %TRUE if properties should be displayed when opening the dossier.
 */
gboolean
ofa_open_prefs_get_display_properties( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ), FALSE );

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->display_properties );
}

/**
 * ofa_open_prefs_set_display_properties:
 * @prefs: this #ofaOpenPrefs object.
 * @display_properties: whether properties should be displayed when opening the dossier.
 *
 * Set @display_properties.
 */
void
ofa_open_prefs_set_display_properties( ofaOpenPrefs *prefs, gboolean display_properties )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->display_properties = display_properties;
}

/**
 * ofa_open_prefs_get_check_balances:
 * @prefs: this #ofaOpenPrefs object.
 *
 * Returns: %TRUE if balances should be checked when opening the dossier.
 */
gboolean
ofa_open_prefs_get_check_balances( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ), FALSE );

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->check_balances );
}

/**
 * ofa_open_prefs_set_check_balances:
 * @prefs: this #ofaOpenPrefs object.
 * @check_balances: whether balances should be checked when opening the dossier.
 *
 * Set @check_balances.
 */
void
ofa_open_prefs_set_check_balances( ofaOpenPrefs *prefs, gboolean check_balances )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->check_balances = check_balances;
}

/**
 * ofa_open_prefs_get_check_integrity:
 * @prefs: this #ofaOpenPrefs object.
 *
 * Returns: %TRUE if integrity should be checked when opening the dossier.
 */
gboolean
ofa_open_prefs_get_check_integrity( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ), FALSE );

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->check_integrity );
}

/**
 * ofa_open_prefs_set_check_integrity:
 * @prefs: this #ofaOpenPrefs object.
 * @check_integrity: whether integrity should be checked when opening the dossier.
 *
 * Set @check_integrity.
 */
void
ofa_open_prefs_set_check_integrity( ofaOpenPrefs *prefs, gboolean check_integrity )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->check_integrity = check_integrity;
}

/**
 * ofa_open_prefs_apply_settings:
 * @prefs: this #ofaOpenPrefs object.
 *
 * Write settings.
 */
void
ofa_open_prefs_apply_settings( ofaOpenPrefs *prefs )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	write_settings( prefs );
}

/**
 * ofa_open_prefs_change_settings:
 * @prefs: this #ofaOpenPrefs object.
 * @settings: the settings interface to be used;
 *  may be either the user or the dossier settings interface.
 * @group: the group name.
 * @key: the key.
 *
 * Change the settings interface.
 * This let us copy the preferences from a settings interface to another.
 */
void
ofa_open_prefs_change_settings( ofaOpenPrefs *prefs, myISettings *settings, const gchar *group, const gchar *key )
{
	ofaOpenPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_OPEN_PREFS( prefs ));
	g_return_if_fail( settings && MY_IS_ISETTINGS( settings ));
	g_return_if_fail( my_strlen( group ));
	g_return_if_fail( my_strlen( key ));

	priv = ofa_open_prefs_get_instance_private( prefs );

	priv->settings = settings;

	g_free( priv->group );
	priv->group = g_strdup( group );

	g_free( priv->key );
	priv->key = g_strdup( key );
}

/*
 * Settings are:
 *     open_notes(b); non_empty(b); open_properties(b); check_balances(b); check_integrity(b);
 */
static void
read_settings( ofaOpenPrefs *self )
{
	ofaOpenPrefsPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_open_prefs_get_instance_private( self );

	key = g_strdup_printf( "%s-settings", priv->key );
	strlist = my_isettings_get_string_list( priv->settings, priv->group, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->display_notes = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->non_empty_notes = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->display_properties = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->check_balances = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->check_integrity = my_utils_boolean_from_str( cstr );
	}

	my_isettings_free_string_list( priv->settings, strlist );
	g_free( key );
}

static void
write_settings( ofaOpenPrefs *self )
{
	ofaOpenPrefsPrivate *priv;
	gchar *key, *str;

	priv = ofa_open_prefs_get_instance_private( self );

	str = g_strdup_printf( "%s;%s;%s;%s;%s;",
			priv->display_notes ? "True":"False",
			priv->non_empty_notes ? "True":"False",
			priv->display_properties ? "True":"False",
			priv->check_balances ? "True":"False",
			priv->check_integrity ? "True":"False" );

	key = g_strdup_printf( "%s-settings", priv->key );
	my_isettings_set_string( priv->settings, priv->group, key, str );

	g_free( key );
	g_free( str );
}
