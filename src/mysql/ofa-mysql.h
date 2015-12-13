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

#ifndef __OFA_MYSQL_H__
#define __OFA_MYSQL_H__

/**
 * SECTION: ofa_mysql
 * @short_description: #ofaMysql class definition.
 *
 * The main class which manages the MySQL database server.
 *
 * The class is instanciated once when loading the module.
 * It implements the #ofaIDBProvider and #ofaIPreferences interfaces.
 *
 * The companion classes #ofaMySQLMeta, #ofaMySQLPeriod,
 * #ofaMySQLConnect, #ofaMySQLEditorEnter or #ofaMySQLEditorDisplay
 * respectively implement the #ofaIDBMeta, #ofaIDBPeriod, #ofaIDBConnect
 * and #ofaIDBEditor interfaces.
 *
 * As the Openbook software suite has chosen to store its meta datas
 * in a dossier settings file, server and database(s) keys are stored
 * in #ofaMySQLMeta (server keys) and #ofaMySQLPeriod (database key).
 *
 * The MySQL plugin let the user configure the command-line utilities
 * with following placeholders:
 * - %B: current database name
 * - %F: filename
 * - %N: new database name
 * - %O: connection options (host, port, socket)
 * - %P: password
 * - %U: account
 */

#include <glib-object.h>
#include <mysql/mysql.h>

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
	GObjectClass     parent;
}
	ofaMysqlClass;

/**
 * mysqlInfos:
 * @dname: the name of the dossier
 * @host: [allow-none]:
 * @port: [allow-none]:
 * @socket: [allow-none]:
 * @dbname: [allow-none]:
 * @account: [allow-none]:
 * @password: [allow-none]:
 * @mysql: the handle allocated by MySQL for the connection
 *
 * Connection information for MySQL.
 *
 * MySQL provides a default value for all optional fields.
 */
typedef struct {
	gchar *dname;
	gchar *host;
	gint   port;
	gchar *socket;
	gchar *dbname;
	gchar *account;
	gchar *password;
	MYSQL *mysql;
}
	mysqlInfos;

GType ofa_mysql_get_type     ( void );

void  ofa_mysql_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OFA_MYSQL_H__ */
