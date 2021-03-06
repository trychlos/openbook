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

#ifndef __OFA_MYSQL_CMDLINE_H__
#define __OFA_MYSQL_CMDLINE_H__

/**
 * SECTION: ofa_mysql
 * @short_description: #ofaMysql class definition.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect-def.h"
#include "api/ofa-idbsuperuser-def.h"

#include "mysql/ofa-mysql-connect.h"
#include "mysql/ofa-mysql-exercice-meta.h"

G_BEGIN_DECLS

const gchar *ofa_mysql_cmdline_backup_get_default_command ( void );

gboolean     ofa_mysql_cmdline_backup_db_run              ( ofaMysqlConnect *connect,
																const gchar *uri,
																ofaMsgCb msg_cb,
																ofaDataCb data_cb,
																void *user_data );

const gchar *ofa_mysql_cmdline_restore_get_default_command( void );

gboolean     ofa_mysql_cmdline_restore_db_run             ( ofaMysqlConnect *connect,
																ofaMysqlExerciceMeta *period,
																const gchar *uri,
																guint format,
																ofaMsgCb msg_cb,
																ofaDataCb data_cb,
																void *user_data );

gboolean     ofa_mysql_cmdline_archive_and_new            ( ofaMysqlConnect *connect,
																ofaIDBSuperuser *su,
																const GDate *begin_next,
																const GDate *end_next );

G_END_DECLS

#endif /* __OFA_MYSQL_CMDLINE_H__ */
