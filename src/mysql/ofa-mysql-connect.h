/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_CONNECT_H__
#define __OFA_MYSQL_CONNECT_H__

/**
 * SECTION: ofa_mysql_connect
 * @short_description: #ofaMysqlConnect class definition.
 * @include: mysql/ofa-mysql-connect.h
 *
 * The #ofaMysqlConnect class manages and handles connection to a
 * specific dossier and exercice (here, a database).
 * It implements the #ofaIDBConnect interface.
 */

#include "ofa-mysql-dossier-meta.h"
#include "ofa-mysql-exercice-meta.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_CONNECT                ( ofa_mysql_connect_get_type())
#define OFA_MYSQL_CONNECT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_CONNECT, ofaMysqlConnect ))
#define OFA_MYSQL_CONNECT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_CONNECT, ofaMysqlConnectClass ))
#define OFA_IS_MYSQL_CONNECT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_CONNECT ))
#define OFA_IS_MYSQL_CONNECT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_CONNECT ))
#define OFA_MYSQL_CONNECT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_CONNECT, ofaMysqlConnectClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlConnect;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlConnectClass;

GType            ofa_mysql_connect_get_type           ( void ) G_GNUC_CONST;

ofaMysqlConnect *ofa_mysql_connect_new                ( void );

gboolean         ofa_mysql_connect_open_with_details  ( ofaMysqlConnect *connect,
															const gchar *host,
															const gchar *soket,
															guint port,
															const gchar *database,
															const gchar *account,
															const gchar *password );

gboolean         ofa_mysql_connect_open_with_meta     ( ofaMysqlConnect *connect,
															const gchar *account,
															const gchar *password,
															const ofaMysqlDossierMeta *dossier_meta,
															const ofaMysqlExerciceMeta *period );

gboolean         ofa_mysql_connect_is_opened          ( ofaMysqlConnect *connect );

void             ofa_mysql_connect_close              ( ofaMysqlConnect *connect );

gboolean         ofa_mysql_connect_query              ( ofaMysqlConnect *connect,
															const gchar *query );

gboolean         ofa_mysql_connect_does_database_exist( ofaMysqlConnect *connect,
															const gchar *database );

gchar           *ofa_mysql_connect_get_new_database   ( ofaMysqlConnect *connect,
															const gchar *prev_database );

G_END_DECLS

#endif /* __OFA_MYSQL_CONNECT_H__ */
