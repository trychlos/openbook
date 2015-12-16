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

#include "core/ofa-settings.h"

#include "ofa-mysql-cmdline.h"
#include "ofa-mysql-iprefs-provider.h"
#include "ofa-mysql-prefs-bin.h"

#define PREFS_GROUP                     "MySQL"
#define PREFS_BACKUP_CMDLINE            "BackupCommand"
#define PREFS_RESTORE_CMDLINE           "RestoreCommand"

static guint ipreferences_get_interface_version( const ofaIPrefsProvider *instance );

void
ofa_mysql_iprefs_provider_iface_init( ofaIPrefsProviderInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_iprefs_provider_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ipreferences_get_interface_version;
	iface->do_init = ofa_mysql_prefs_bin_new;
	iface->do_check = ofa_mysql_prefs_bin_is_valid;
	iface->do_apply = ofa_mysql_prefs_bin_apply;
}

static guint
ipreferences_get_interface_version( const ofaIPrefsProvider *instance )
{
	return( 1 );
}

/**
 * ofa_mysql_iprefs_provider_get_backup_command:
 *
 * Returns: the backup command from the user settings, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * If unset in the user settings, the method returns the default
 * backup command.
 */
gchar *
ofa_mysql_iprefs_provider_get_backup_command ( void )
{
	gchar *cmdline;

	cmdline = ofa_settings_get_string_ex(
					SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE );
	if( !my_strlen( cmdline )){
		cmdline = g_strdup( ofa_mysql_cmdline_backup_get_default_command());
	}

	return( cmdline );
}

/**
 * ofa_mysql_iprefs_provider_set_backup_command:
 * @command: the backup command to be set.
 *
 * Records the backup command @command in the user settings.
 */
void
ofa_mysql_iprefs_provider_set_backup_command ( const gchar *command )
{
	ofa_settings_set_string_ex(
			SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_BACKUP_CMDLINE, command );
}

/**
 * ofa_mysql_iprefs_provider_get_restore_command:
 *
 * Returns: the restore command from the user settings, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_mysql_iprefs_provider_get_restore_command( void )
{
	gchar *cmdline;

	cmdline = ofa_settings_get_string_ex(
					SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE );
	if( !my_strlen( cmdline )){
		cmdline = g_strdup( ofa_mysql_cmdline_restore_get_default_command());
	}

	return( cmdline );
}

/**
 * ofa_mysql_iprefs_provider_set_restore_command:
 * @command: the restore command to be set.
 *
 * Records the restore command @command in the user settings.
 */
void
ofa_mysql_iprefs_provider_set_restore_command( const gchar *command )
{
	ofa_settings_set_string_ex(
			SETTINGS_TARGET_USER, PREFS_GROUP, PREFS_RESTORE_CMDLINE, command );
}
