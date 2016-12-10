/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-utils.h"

#include "api/ofa-dossier-prefs.h"
#include "api/ofa-hub.h"
#include "api/ofa-settings.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaHub     *hub;

	/* runtime
	 */
	gboolean    notes;
	gboolean    nonempty;
	gboolean    properties;
	gboolean    balances;
	gboolean    integrity;

	gchar      *background_uri;
}
	ofaDossierPrefsPrivate;

static const gchar *st_background_img   = "ofa-BackgroundImage";
static const gchar *st_prefs_settings   = "ofa-UserPreferences-settings";

static void  get_dossier_settings( ofaDossierPrefs *self );
static void  set_dossier_settings( ofaDossierPrefs *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierPrefs, ofa_dossier_prefs, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaDossierPrefs ))

static void
dossier_prefs_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_prefs_finalize";
	ofaDossierPrefsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_PREFS( instance ));

	/* free data members here */
	priv = ofa_dossier_prefs_get_instance_private( OFA_DOSSIER_PREFS( instance ));

	g_free( priv->background_uri );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_prefs_parent_class )->finalize( instance );
}

static void
dossier_prefs_dispose( GObject *instance )
{
	ofaDossierPrefsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_PREFS( instance ));

	priv = ofa_dossier_prefs_get_instance_private( OFA_DOSSIER_PREFS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_prefs_parent_class )->dispose( instance );
}

static void
ofa_dossier_prefs_init( ofaDossierPrefs *self )
{
	static const gchar *thisfn = "ofa_dossier_prefs_init";
	ofaDossierPrefsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_PREFS( self ));

	priv = ofa_dossier_prefs_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->background_uri = NULL;
}

static void
ofa_dossier_prefs_class_init( ofaDossierPrefsClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_prefs_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_prefs_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_prefs_finalize;
}

/**
 * ofa_dossier_prefs_new:
 * @hub:
 *
 * Allocate a new #ofaDossierPrefs object.
 */
ofaDossierPrefs *
ofa_dossier_prefs_new( ofaHub *hub )
{
	ofaDossierPrefs *self;
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	self = g_object_new( OFA_TYPE_DOSSIER_PREFS, NULL );
	priv = ofa_dossier_prefs_get_instance_private( self );

	priv->hub = hub;

	get_dossier_settings( self );

	return( self );
}

/**
 * ofa_dossier_prefs_get_open_notes:
 * @prefs:
 *
 * Returns: whether the notes should be displayed on dossier opening.
 */
gboolean
ofa_dossier_prefs_get_open_notes( ofaDossierPrefs *prefs )
{
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ), FALSE );

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->notes );
}

/**
 * ofa_dossier_prefs_set_open_notes:
 * @prefs:
 * @notes:
 */
void
ofa_dossier_prefs_set_open_notes( ofaDossierPrefs *prefs, gboolean open )
{
	ofaDossierPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ));

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->notes = open;
	set_dossier_settings( prefs );
}

/**
 * ofa_dossier_prefs_get_nonempty:
 * @prefs:
 *
 * Returns: whether the notes should be displayed only when non empty.
 */
gboolean
ofa_dossier_prefs_get_nonempty( ofaDossierPrefs *prefs )
{
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ), FALSE );

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->nonempty );
}

/**
 * ofa_dossier_prefs_set_nonempty:
 * @prefs:
 * @nonempty:
 */
void
ofa_dossier_prefs_set_nonempty( ofaDossierPrefs *prefs, gboolean nonempty )
{
	ofaDossierPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ));

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->nonempty = nonempty;
	set_dossier_settings( prefs );
}

/**
 * ofa_dossier_prefs_get_properties:
 * @prefs:
 *
 * Returns: whether the properties should be displayed on dossier opening.
 */
gboolean
ofa_dossier_prefs_get_properties( ofaDossierPrefs *prefs )
{
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ), FALSE );

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->properties );
}

/**
 * ofa_dossier_prefs_set_properties:
 * @prefs:
 * @properties:
 */
void
ofa_dossier_prefs_set_properties( ofaDossierPrefs *prefs, gboolean properties )
{
	ofaDossierPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ));

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->properties = properties;
	set_dossier_settings( prefs );
}

/**
 * ofa_dossier_prefs_get_balances:
 * @prefs:
 *
 * Returns: whether the balances should be checked on dossier opening.
 */
gboolean
ofa_dossier_prefs_get_balances( ofaDossierPrefs *prefs )
{
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ), FALSE );

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->balances );
}

/**
 * ofa_dossier_prefs_set_balances:
 * @prefs:
 * @balances:
 */
void
ofa_dossier_prefs_set_balances( ofaDossierPrefs *prefs, gboolean balances )
{
	ofaDossierPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ));

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->balances = balances;
	set_dossier_settings( prefs );
}

/**
 * ofa_dossier_prefs_get_integrity:
 * @prefs:
 *
 * Returns: whether the integrity should be checked on dossier opening.
 */
gboolean
ofa_dossier_prefs_get_integrity( ofaDossierPrefs *prefs )
{
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ), FALSE );

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->integrity );
}

/**
 * ofa_dossier_prefs_set_integrity:
 * @prefs:
 * @integrity:
 */
void
ofa_dossier_prefs_set_integrity( ofaDossierPrefs *prefs, gboolean integrity )
{
	ofaDossierPrefsPrivate *priv;

	g_return_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ));

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	priv->integrity = integrity;
	set_dossier_settings( prefs );
}

/**
 * ofa_dossier_prefs_get_background_img:
 * @prefs:
 *
 * Returns: the background image URI, as a newly allocated string which
 * should be g_free() by the caller.
 */
gchar *
ofa_dossier_prefs_get_background_img( ofaDossierPrefs *prefs )
{
	ofaDossierPrefsPrivate *priv;

	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ), FALSE );

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( g_strdup( priv->background_uri ));
}

/**
 * ofa_dossier_prefs_set_background_img:
 * @prefs:
 * @uri:
 */
void
ofa_dossier_prefs_set_background_img( ofaDossierPrefs *prefs, const gchar *uri )
{
	ofaDossierPrefsPrivate *priv;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *meta;

	g_return_if_fail( prefs && OFA_IS_DOSSIER_PREFS( prefs ));

	priv = ofa_dossier_prefs_get_instance_private( prefs );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->background_uri );
	priv->background_uri = g_strdup( uri );

	connect = ofa_hub_get_connect( priv->hub );
	meta = ofa_idbconnect_get_dossier_meta( connect );

	ofa_settings_dossier_set_string( meta, st_background_img, priv->background_uri );

	g_object_unref( meta );
}

/*
 * dossier settings: open_notes;only_when_non_empty;properties;balances;integrity;
 */
static void
get_dossier_settings( ofaDossierPrefs *self )
{
	ofaDossierPrefsPrivate *priv;
	GList *list, *it;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *meta;
	const gchar *cstr;
	gchar *str;

	priv = ofa_dossier_prefs_get_instance_private( self );

	connect = ofa_hub_get_connect( priv->hub );
	meta = ofa_idbconnect_get_dossier_meta( connect );
	list = ofa_settings_dossier_get_string_list( meta, st_prefs_settings );

	it = list ? list : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->notes = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->nonempty = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->properties = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->balances = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->integrity = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;

	ofa_settings_free_string_list( list );

	str = ofa_settings_dossier_get_string( meta, st_background_img );
	g_free( priv->background_uri );
	priv->background_uri = str;

	g_object_unref( meta );
}

static void
set_dossier_settings( ofaDossierPrefs *self )
{
	ofaDossierPrefsPrivate *priv;
	const ofaIDBConnect *connect;
	ofaIDBDossierMeta *meta;
	gchar *str;

	priv = ofa_dossier_prefs_get_instance_private( self );

	connect = ofa_hub_get_connect( priv->hub );
	meta = ofa_idbconnect_get_dossier_meta( connect );

	str = g_strdup_printf( "%s;%s;%s;%s;%s;",
			priv->notes ? "True":"False",
			priv->nonempty ? "True":"False",
			priv->properties ? "True":"False",
			priv->balances ? "True":"False",
			priv->integrity ? "True":"False" );

	ofa_settings_dossier_set_string( meta, st_prefs_settings, str );

	g_free( str );
	g_object_unref( meta );
}
