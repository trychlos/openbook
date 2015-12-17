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

#ifndef __OPENBOOK_API_OFA_SETTINGS_H__
#define __OPENBOOK_API_OFA_SETTINGS_H__

/* @title: ofaSettings
 * @short_description: The Settings Class Definition
 * @include: api/ofa-settings.h
 *
 * The #ofaSettings code manages both user preferences and dossiers
 * configuration as two distinct settings files, each of them being
 * managed by its own mySettings singleton.
 *
 * Both configuration filenames may be overriden  by an environment
 * variable:
 * - OFA_USER_CONF addresses the user configuration file
 *   default is HOME/.config/PACKAGE/PACKAGE.conf
 * - OFA_DOSSIER_CONF addresses the dossier configuration file
 *   default is HOME/.config/PACKAGE/dossier.conf
 */

#include "my-isettings.h"
#include "ofa-idbmeta-def.h"

G_BEGIN_DECLS

/**
 * ofaSettingsTarget:
 * @SETTINGS_TARGET_USER: target the user preferences settings file.
 * @SETTINGS_TARGET_DOSSIER: target the dossier settings file.
 */
typedef enum {
	SETTINGS_TARGET_USER = 1,
	SETTINGS_TARGET_DOSSIER
}
	ofaSettingsTarget;

/* some used keys
 */
#define SETTINGS_EXPORT_FOLDER                       "ExportDefaultFolder"
#define SETTINGS_EXPORT_SETTINGS                     "ExportSettings"
#define SETTINGS_IMPORT_SETTINGS                     "ImportSettings"

GType        ofa_settings_get_type                   ( void ) G_GNUC_CONST;

void         ofa_settings_new                        ( void );

void         ofa_settings_free                       ( void );

myISettings *ofa_settings_get_settings               ( ofaSettingsTarget target );

/* multi-usage functions
 */
gboolean     ofa_settings_get_boolean                ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key );

void         ofa_settings_set_boolean                ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key,
															gboolean value );

gint         ofa_settings_get_uint                   ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key );

void         ofa_settings_set_uint                   ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key,
															guint value );

GList       *ofa_settings_get_uint_list              ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key );

void         ofa_settings_set_uint_list              ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key,
															const GList *value );

#define      ofa_settings_free_uint_list(L)          g_list_free( L )

gchar       *ofa_settings_get_string                 ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key );

void         ofa_settings_set_string                 ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key,
															const gchar *value );

GList       *ofa_settings_get_string_list            ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key );

void         ofa_settings_set_string_list            ( ofaSettingsTarget target,
															const gchar *group,
															const gchar *key,
															const GList *value );

#define      ofa_settings_free_string_list(L)        g_list_free_full(( L ), ( GDestroyNotify ) g_free )

/* user preferences management
 */
/* this is the group name for user preferences
 * should not be used by the code, but needed to define following
 * user preferences macros
 */
#define SETTINGS_GROUP_GENERAL                       "General"

#define      ofa_settings_user_get_boolean(K)        ofa_settings_get_boolean(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define      ofa_settings_user_set_boolean(K,V)      ofa_settings_set_boolean(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define      ofa_settings_user_get_uint(K)           ofa_settings_get_uint(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define      ofa_settings_user_set_uint(K,V)         ofa_settings_set_uint(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define      ofa_settings_user_get_uint_list(K)      ofa_settings_get_uint_list(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define      ofa_settings_user_set_uint_list(K,V)    ofa_settings_set_uint_list(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define      ofa_settings_user_get_string(K)         ofa_settings_get_string(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define      ofa_settings_user_set_string(K,V)       ofa_settings_set_string(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

#define      ofa_settings_user_get_string_list(K)    ofa_settings_get_string_list(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K))
#define      ofa_settings_user_set_string_list(K,V)  ofa_settings_set_string_list(SETTINGS_TARGET_USER,SETTINGS_GROUP_GENERAL,(K),(V))

/* dossiers configuration management
 */
gchar       *ofa_settings_dossier_get_string         ( const ofaIDBMeta *meta,
															const gchar *key );

void         ofa_settings_dossier_set_string         ( const ofaIDBMeta *meta,
															const gchar *key,
															const gchar *value );

GList       *ofa_settings_dossier_get_string_list    ( const ofaIDBMeta *meta,
															const gchar *key );

void         ofa_settings_dossier_set_string_list    ( const ofaIDBMeta *meta,
															const gchar *key,
															const GList *list );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_SETTINGS_H__ */
