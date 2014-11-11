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

#ifndef __OFA_MYSQL_H__
#define __OFA_MYSQL_H__

/**
 * SECTION: ofa_mysql
 * @short_description: #ofaMysql class definition.
 *
 * Manage the MySQL database server.
 *
 * The class is instanciated once when loading the module.
 */

#include <glib-object.h>

#include <mysql/mysql.h>

#include <api/ofa-idbms.h>
#include <api/ofa-ipreferences.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL                ( ofa_mysql_get_type())
#define OFA_MYSQL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL, ofaMysql ))
#define OFA_MYSQL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL, ofaMysqlClass ))
#define OFA_IS_MYSQL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL ))
#define OFA_IS_MYSQL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL ))
#define OFA_MYSQL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL, ofaMysqlClass ))

typedef struct _ofaMysqlPrivate       ofaMysqlPrivate;

typedef struct {
	/*< public members >*/
	GObject          parent;

	/*< private members >*/
	ofaMysqlPrivate *priv;
}
	ofaMysql;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlClass;

/**
 * mysqlConnect:
 * @host: [allow-none]:
 * @port: [allow-none]:
 * @socket: [allow-none]:
 * @dbname: [allow-none]:
 * @account: [allow-none]:
 * @password: [allow-none]:
 *
 * Connection information for MySQL.
 *
 * MySQL provides a default value for all these fields.
 */
typedef struct {
	gchar *host;
	gint   port;
	gchar *socket;
	gchar *dbname;
	gchar *account;
	gchar *password;
}
	mysqlConnect;

/**
 * mysqlInfos:
 * @label:
 * @mysql:
 * @connect:
 *
 * A structure which associates the ofoSgbd connection to its properties.
 *
 * The structure is dynamically allocated on a new connection, and its
 * handle is returned to the caller.
 */
typedef struct {
	gchar        *label;
	MYSQL        *mysql;
	mysqlConnect  connect;
}
	mysqlInfos;

#define PREFS_GROUP                     "MySQL"
#define PREFS_BACKUP_CMDLINE            "BackupCommand"
#define PREFS_RESTORE_CMDLINE           "RestoreCommand"

GType         ofa_mysql_get_type         ( void );

void          ofa_mysql_register_type    ( GTypeModule *module );

const gchar  *ofa_mysql_get_provider_name( const ofaIDbms *instance );

MYSQL        *ofa_mysql_connect          ( mysqlConnect *sConnect );

gboolean      ofa_mysql_get_db_exists    ( mysqlConnect *sConnect );

mysqlConnect *ofa_mysql_get_connect_infos( mysqlConnect *sConnect, const gchar *label );

void          ofa_mysql_free_connect     ( mysqlConnect *sConnect );

G_END_DECLS

#endif /* __OFA_MYSQL_H__ */
