/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_USER_PREFS_H__
#define __OFA_MYSQL_USER_PREFS_H__

/**
 * SECTION: ofa_mysql_user_prefs
 * @short_description: #ofaMysql class definition.
 *
 * User preferences management.
 */

#include <glib.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

gchar *ofa_mysql_user_prefs_get_backup_command ( ofaIGetter *getter );

void   ofa_mysql_user_prefs_set_backup_command ( ofaIGetter *getter,
													const gchar *command );

gchar *ofa_mysql_user_prefs_get_restore_command( ofaIGetter *getter,
													guint format );

void   ofa_mysql_user_prefs_set_restore_command( ofaIGetter *getter,
													guint format,
													const gchar *command );

G_END_DECLS

#endif /* __OFA_MYSQL_USER_PREFS_H__ */
