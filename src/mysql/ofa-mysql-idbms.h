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

#ifndef __OFA_MYSQL_IDBMS_H__
#define __OFA_MYSQL_IDBMS_H__

/**
 * SECTION: ofa_mysql_idbms
 * @short_description: #ofaMysql class definition.
 *
 * #ofaIDbms interface management.
 */

#include "api/ofa-idbms.h"

G_BEGIN_DECLS

void         ofa_mysql_idbms_iface_init       ( ofaIDbmsInterface *iface );

const gchar *ofa_mysql_idbms_get_provider_name( const ofaIDbms *instance );

/* MySQL private functions
 */
mysqlInfos  *ofa_mysql_get_connect_infos      ( const gchar *dname );

mysqlInfos  *ofa_mysql_get_connect_newdb_infos( const gchar *dname,
														const gchar *root_account,
														const gchar *root_password,
														gchar **prev_dbname );

gboolean     ofa_mysql_connect_with_infos     ( mysqlInfos *infos );

void         ofa_mysql_free_connect_infos     ( mysqlInfos *infos );

gboolean     ofa_mysql_query                  ( const ofaIDbms *instance,
														mysqlInfos *infos,
														const gchar *query );

gboolean     ofa_mysql_duplicate_grants       ( const ofaIDbms *instance,
														mysqlInfos *infos,
														const gchar *user_account,
														const gchar *prev_dbname );

G_END_DECLS

#endif /* __OFA_MYSQL_IDBMS_H__ */
