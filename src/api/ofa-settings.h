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
 * @include: api/ofa-settings.h
 *
 * The #ofaSettings class manages both user preferences and dossiers
 * configuration in two distinct text files.
 */

#include <glib.h>

G_BEGIN_DECLS

/**
 * ofaSettingsType:
 *
 * We need to publish them in order they are available to plugins.
 */
typedef enum {
	SETTINGS_TYPE_STRING = 0,
	SETTINGS_TYPE_INT
}
	ofaSettingsType;

/**
 * ofaSettingsTarget:
 */
typedef enum {
	SETTINGS_TARGET_USER = 1,
	SETTINGS_TARGET_DOSSIER
}
	ofaSettingsTarget;

/* some used keys */
#define  SETTINGS_DBMS_PROVIDER         "DBMSProvider"
#define  SETTINGS_EXPORT_FOLDER         "DefaultExportFolder"

/* this is the group name for user preferences
 * should not be used by the code, but needed to define following
 * user preferences macros */
#define SETTINGS_GROUP_GENERAL          "General"

/* this is the group name for dossiers configuration */
#define SETTINGS_GROUP_DOSSIER          "Dossier"

/* called on application dispose */
void     ofa_settings_free                   ( void );

/* user preferences management */
#define  ofa_settings_get_boolean(K)         ofa_settings_get_boolean_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define  ofa_settings_set_boolean(K,V)       ofa_settings_set_boolean_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define  ofa_settings_get_int(K)             ofa_settings_get_int_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define  ofa_settings_set_int(K,V)           ofa_settings_set_int_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define  ofa_settings_get_int_list(K)        ofa_settings_get_int_list_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define  ofa_settings_set_int_list(K,V)      ofa_settings_set_int_list_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define  ofa_settings_get_string(K)          ofa_settings_get_string_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define  ofa_settings_set_string(K,V)        ofa_settings_set_string_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define  ofa_settings_get_string_list(K)     ofa_settings_get_string_list_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define  ofa_settings_set_string_list(K,V)   ofa_settings_set_string_list_ex(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

/* multi-usage functions */
gboolean ofa_settings_get_boolean_ex         ( ofaSettingsTarget target,
														const gchar *group, const gchar *key );
void     ofa_settings_set_boolean_ex         ( ofaSettingsTarget target,
														const gchar *group, const gchar *key,
														gboolean bvalue );

gint     ofa_settings_get_int_ex             ( ofaSettingsTarget target,
														const gchar *group, const gchar *key );
void     ofa_settings_set_int_ex             ( ofaSettingsTarget target,
														const gchar *group, const gchar *key,
														gint ivalue );

GList   *ofa_settings_get_int_list_ex        ( ofaSettingsTarget target,
														const gchar *group, const gchar *key );
void     ofa_settings_set_int_list_ex        ( ofaSettingsTarget target,
														const gchar *group, const gchar *key,
														const GList *int_list );
#define  ofa_settings_free_int_list(L)       g_list_free( L )

gchar   *ofa_settings_get_string_ex          ( ofaSettingsTarget target,
														const gchar *group, const gchar *key );
void     ofa_settings_set_string_ex          ( ofaSettingsTarget target,
														const gchar *group, const gchar *key,
														const gchar *svalue );

GList   *ofa_settings_get_string_list_ex     ( ofaSettingsTarget target,
														const gchar *group, const gchar *key );
void     ofa_settings_set_string_list_ex     ( ofaSettingsTarget target,
														const gchar *group, const gchar *key,
														const GList *str_list );
#define  ofa_settings_free_string_list(L)    g_list_free_full(( L ), ( GDestroyNotify ) g_free )

GSList  *ofa_settings_get_groups             ( ofaSettingsTarget target );
#define  ofa_settings_free_groups(L)         g_slist_free_full(( L ), ( GDestroyNotify ) g_free )

/* dossiers configuration management */

GSList  *ofa_settings_dossier_get_keys       ( const gchar *dname );
#define  ofa_settings_dossier_free_keys(L)   g_slist_free_full(( L ), ( GDestroyNotify ) g_free )

gboolean ofa_settings_create_dossier         ( const gchar *dname, ... );

void     ofa_settings_remove_dossier         ( const gchar *dname );

gboolean ofa_settings_has_dossier            ( const gchar *dname );

gchar   *ofa_settings_get_dossier_provider   ( const gchar *dname );

gint     ofa_settings_dossier_get_int        ( const gchar *dname, const gchar *key );

gchar   *ofa_settings_dossier_get_string     ( const gchar *dname, const gchar *key );
void     ofa_settings_dossier_set_string     ( const gchar *dname, const gchar *key, const gchar *svalue );

GList   *ofa_settings_dossier_get_string_list( const gchar *dname, const gchar *key );
void     ofa_settings_dossier_set_string_list( const gchar *dname, const gchar *key, const GList *list );

G_END_DECLS

#endif /* __OFA_SETTINGS_H__ */
