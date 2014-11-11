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

#ifndef __OFA_MYSQL_DOSSIER_NEW_H__
#define __OFA_MYSQL_DOSSIER_NEW_H__

/**
 * SECTION: ofa_mysql
 * @short_description: #ofaMysql class definition.
 */

#include <gtk/gtk.h>

#include "api/ofa-idbms.h"

#include "ofa-mysql.h"

G_BEGIN_DECLS

/**
 * mysqlNew:
 * @sConnect: connection informations
 * @account: root account
 * @password: root password
 *
 * There are the needed informations for initializing a new connection
 * at administrative level of the DBMS server.
 */
typedef struct {
	mysqlConnect sConnect;
	ofnDBMode    dbmode;
}
	mysqlNew;

void     ofa_mysql_properties_new_init ( const ofaIDbms *instance, GtkContainer *parent,
											GtkSizeGroup *group );

gboolean ofa_mysql_properties_new_check( const ofaIDbms *instance, GtkContainer *parent );

gboolean ofa_mysql_properties_new_apply( const ofaIDbms *instance, GtkContainer *parent,
											const gchar *label, const gchar *account, const gchar *password );

G_END_DECLS

#endif /* __OFA_MYSQL_DOSSIER_NEW_H__ */
