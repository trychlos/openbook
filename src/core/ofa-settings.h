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

#ifndef __OFA_SETTINGS_H__
#define __OFA_SETTINGS_H__

/* @title: ofaSettings
 * @short_description: The Settings Class Definition
 * @include: core/ofa-settings.h
 *
 * The #ofaSettings class manages user preferences.
 *
 * #ofaSettings class defines a singleton object, which allocates itself
 * when needed.
 */

#include <glib.h>

G_BEGIN_DECLS

/**
 *
 */
typedef enum {
	SETTINGS_TYPE_STRING = 0,
	SETTINGS_TYPE_INT
}
	SettingsType;

void     ofa_settings_free         ( void );

GSList  *ofa_settings_get_dossiers ( void );

void     ofa_settings_get_dossier  ( const gchar *name, gchar **host, gint *port, gchar **socket, gchar **dbname );
gboolean ofa_settings_set_dossier  ( const gchar *name, ... );

GList   *ofa_settings_get_uint_list( const gchar *key );
void     ofa_settings_set_uint_list( const gchar *key, const GList *uint_list );

gint     ofa_settings_get_uint     ( const gchar *key );
void     ofa_settings_set_uint     ( const gchar *key, guint value );

gboolean ofa_settings_get_boolean  ( const gchar *key );
void     ofa_settings_set_boolean  ( const gchar *key, gboolean value );

G_END_DECLS

#endif /* __OFA_SETTINGS_H__ */
