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

#include "ui/my-utils.h"
#include "ui/ofa-settings.h"

#define OFA_TYPE_SETTINGS                ( settings_get_type())
#define OFA_SETTINGS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_SETTINGS, ofaSettings ))
#define OFA_SETTINGS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_SETTINGS, ofaSettingsClass ))
#define OFA_IS_SETTINGS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_SETTINGS ))
#define OFA_IS_SETTINGS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_SETTINGS ))
#define OFA_SETTINGS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_SETTINGS, ofaSettingsClass ))

typedef struct _ofaSettingsPrivate       ofaSettingsPrivate;

typedef struct {
	/*< private >*/
	mySettings          parent;
	ofaSettingsPrivate *private;
}
	ofaSettings;

typedef struct _ofaSettingsClassPrivate  ofaSettingsClassPrivate;

typedef struct {
	/*< private >*/
	mySettingsClass          parent;
	ofaSettingsClassPrivate *private;
}
	ofaSettingsClass;

/* private class data
 */
struct _ofaSettingsClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaSettingsPrivate {
	gboolean   dispose_has_run;
};

#define GROUP_GENERAL                    "General"
#define KEY_DOSSIERS                     "Dossiers"

#define GROUP_DOSSIER                    "Dossier"
#define KEY_DESCRIPTION                  "Description"

static const mySettingsKeyDef st_def_keys[] = {
	{ GROUP_GENERAL, KEY_DOSSIERS,    MY_SETTINGS_STRING_LIST, NULL, TRUE },
	{ NULL,          KEY_DESCRIPTION, MY_SETTINGS_STRING,      NULL, FALSE },
	{ 0 }
};

static mySettingsClass *st_parent_class = NULL;
static ofaSettings     *st_settings     = NULL;

static GType   settings_get_type( void );
static GType   register_type( void );
static void    class_init( ofaSettingsClass *klass );
static void    instance_init( GTypeInstance *instance, gpointer klass );
static void    instance_dispose( GObject *object );
static void    instance_finalize( GObject *object );

static void    settings_new( void );
static GSList *settings_get_dossiers( mySettingsMode mode );

static GType
settings_get_type( void )
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
	static const gchar *thisfn = "ofa_settings_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaSettingsClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaSettings ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( MY_TYPE_SETTINGS, "ofaSettings", &info, 0 );

	return( type );
}

static void
class_init( ofaSettingsClass *klass )
{
	static const gchar *thisfn = "ofa_settings_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaSettingsClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_settings_instance_init";
	ofaSettings *self;

	g_return_if_fail( OFA_IS_SETTINGS( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_SETTINGS( instance );

	self->private = g_new0( ofaSettingsPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	static const gchar *thisfn = "ofa_settings_instance_dispose";
	ofaSettingsPrivate *priv;

	g_return_if_fail( OFA_IS_SETTINGS( object ));

	priv = OFA_SETTINGS( object )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

		priv->dispose_has_run = TRUE;

		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( object );
		}
	}
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_settings_instance_finalize";
	ofaSettingsPrivate *priv;

	g_return_if_fail( OFA_IS_SETTINGS( object ));

	g_debug( "%s: object=%p (%s)", thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	priv = OFA_SETTINGS( object )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( object );
	}
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
		st_settings = g_object_new(
				OFA_TYPE_SETTINGS,
				MY_PROP_KEYDEFS, st_def_keys,
				NULL );
	}
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
 * #my_utils_slist_free() by the caller.
 */
GSList *
ofa_settings_get_dossiers( void )
{
	static const gchar *thisfn = "ofa_settings_get_dossiers";
	GSList *list;

	g_debug( "%s", thisfn );

	settings_new();
	list = settings_get_dossiers( MY_SETTINGS_ALL );

	return( list );
}

/**
 * ofa_settings_add_user_dossier:
 * @name:
 * @description:
 *
 * Define a new user dossier.
 */
void
ofa_settings_add_user_dossier( const gchar *name, const gchar *description )
{
	static const gchar *thisfn = "ofa_settings_add_user_dossier";
	GSList *dossiers;

	g_debug( "%s: name=%s, description=%s", thisfn, name, description );

	settings_new();
	dossiers = settings_get_dossiers( MY_SETTINGS_USER_ONLY );
	dossiers = g_slist_prepend( dossiers, ( gpointer ) name );
	my_settings_set_string_list( MY_SETTINGS( st_settings ), GROUP_GENERAL, KEY_DOSSIERS, dossiers );
	my_utils_slist_free( dossiers );
}

/*
 * settings_get_dossiers:
 *
 * Returns the list of defined dossiers as a newly allocated #GSList
 * list of newly allocated strings. The returned list should be
 * #my_utils_slist_free() by the caller.
 */
static GSList *
settings_get_dossiers( mySettingsMode mode )
{
	g_return_val_if_fail( st_settings && OFA_IS_SETTINGS( st_settings ), NULL );

	return( my_settings_get_string_list( MY_SETTINGS( st_settings ), GROUP_GENERAL, KEY_DOSSIERS, mode, NULL, NULL ));
}
