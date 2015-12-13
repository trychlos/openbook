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

#ifndef __OFA_MYSQL_CMDLINE_H__
#define __OFA_MYSQL_CMDLINE_H__

/**
 * SECTION: ofa_mysql
 * @short_description: #ofaMysql class definition.
 */

#include "ofa-mysql-connect.h"
#include "ofa-mysql-period.h"

G_BEGIN_DECLS

const gchar *ofa_mysql_cmdline_backup_get_default_command ( void );

gboolean     ofa_mysql_cmdline_backup_run                 ( const ofaMySQLConnect *connect,
																const gchar *uri );

const gchar *ofa_mysql_cmdline_restore_get_default_command( void );

gboolean     ofa_mysql_cmdline_restore_run                ( const ofaMySQLConnect *connect,
																const ofaMySQLPeriod *period,
																const gchar *uri );

gboolean     ofa_mysql_cmdline_archive_and_new            ( const ofaMySQLConnect *connect,
																const gchar *root_account,
																const gchar *root_password,
																const GDate *begin_next,
																const GDate *end_next );

G_END_DECLS

#endif /* __OFA_MYSQL_CMDLINE_H__ */
