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

#ifndef __MY_SETTINGS_H__
#define __MY_SETTINGS_H__

/* @title: mySettings
 * @short_description: A standard settings class definition
 * @include: openbook/my-settings.h
 *
 * The #mySettings class encapsulates the GKeyFile, providing some
 * (hopefully) useful shortcuts. It implements the myISettings
 * interface, giving thus the application and external plugins the
 * ways to access settings.
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
	/*< public members >*/
	GObject            parent;

	/*< private members >*/
	mySettingsPrivate *priv;
}
	mySettings;

typedef struct {
	/*< public members >*/
	GObjectClass       parent;
}
	mySettingsClass;

GType       my_settings_get_type       ( void ) G_GNUC_CONST;

mySettings *my_settings_new            ( const gchar *filename );

mySettings *my_settings_new_user_config( const gchar *name,
												const gchar *envvar );

G_END_DECLS

#endif /* __MY_SETTINGS_H__ */
