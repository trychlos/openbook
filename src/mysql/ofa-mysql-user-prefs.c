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

#include "api/ofa-backup-header.h"
#include "api/ofa-igetter.h"

#include "ofa-mysql-cmdline.h"
#include "ofa-mysql-user-prefs.h"

#define PREFS_GROUP                     "MySQL"
#define PREFS_BACKUP_CMDLINE            "BackupCommand"
#define PREFS_RESTORE_CMDLINE_GZ        "RestoreCommandGz"
#define PREFS_RESTORE_CMDLINE_ZIP       "RestoreCommandZip"

static const gchar *get_restore_key( guint format );

/**
 * ofa_mysql_user_prefs_get_backup_command:
 *
 * Returns: the backup command from the user settings, as a newly
 * allocated string which should be g_free() by the caller.
 *
 * If unset in the user settings, the method returns the default
 * backup command.
 */
gchar *
ofa_mysql_user_prefs_get_backup_command( ofaIGetter *getter )
{
	myISettings *settings;
	gchar *cmdline;

	settings = ofa_igetter_get_user_settings( getter );
	cmdline = my_isettings_get_string( settings, PREFS_GROUP, PREFS_BACKUP_CMDLINE );
	if( !my_strlen( cmdline )){
		cmdline = g_strdup( ofa_mysql_cmdline_backup_get_default_command());
	}

	return( cmdline );
}

/**
 * ofa_mysql_user_prefs_set_backup_command:
 * @command: the backup command to be set.
 *
 * Records the backup command @command in the user settings.
 */
void
ofa_mysql_user_prefs_set_backup_command( ofaIGetter *getter, const gchar *command )
{
	myISettings *settings;

	settings = ofa_igetter_get_user_settings( getter );
	my_isettings_set_string( settings, PREFS_GROUP, PREFS_BACKUP_CMDLINE, command );
}

/**
 * ofa_mysql_user_prefs_get_restore_command:
 * @getter: a #ofaIGetter instance.
 * @format: the archive format (from #ofaBackupHeader header).
 *
 * Returns: the restore command from the user settings, as a newly
 * allocated string which should be g_free() by the caller.
 */
gchar *
ofa_mysql_user_prefs_get_restore_command( ofaIGetter *getter, guint format )
{
	myISettings *settings;
	const gchar *key;
	gchar *cmdline;

	cmdline = NULL;

	key = get_restore_key( format );
	if( my_strlen( key )){
		settings = ofa_igetter_get_user_settings( getter );
		cmdline = my_isettings_get_string( settings, PREFS_GROUP, key );
		if( !my_strlen( cmdline )){
			cmdline = g_strdup( ofa_mysql_cmdline_restore_get_default_command());
		}
	}

	return( cmdline );
}

/**
 * ofa_mysql_user_prefs_set_restore_command:
 * @getter: a #ofaIGetter instance.
 * @format:
 * @command: the restore command to be set.
 *
 * Records the restore command @command in the user settings.
 */
void
ofa_mysql_user_prefs_set_restore_command( ofaIGetter *getter, guint format, const gchar *command )
{
	myISettings *settings;
	const gchar *key;

	key = get_restore_key( format );
	if( my_strlen( key )){
		settings = ofa_igetter_get_user_settings( getter );
		my_isettings_set_string( settings, PREFS_GROUP, key, command );
	}
}

static const gchar *
get_restore_key( guint format )
{
	static const gchar *thisfn = "ofa_mysql_user_prefs_get_restore_key";
	const gchar *key;

	key = NULL;

	switch( format ){
		case OFA_BACKUP_HEADER_GZ:
			key = PREFS_RESTORE_CMDLINE_GZ;
			break;
		case OFA_BACKUP_HEADER_ZIP:
			key = PREFS_RESTORE_CMDLINE_ZIP;
			break;
		default:
			g_warning( "%s: unknown or invalid archive format=%u", thisfn, format );
			break;
	}

	return( key );
}
