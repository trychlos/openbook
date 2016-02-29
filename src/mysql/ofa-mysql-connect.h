/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_CONNECT_H__
#define __OFA_MYSQL_CONNECT_H__

/**
 * SECTION: ofa_mysql_connect
 * @short_description: #ofaMySQLConnect class definition.
 * @include: mysql/ofa-mysql-connect.h
 *
 * The #ofaMySQLConnect class manages and handles connection to a
 * specific dossier and exercice (here, a database).
 * It implements the #ofaIDBConnect interface.
 */

#include "ofa-mysql-meta.h"
#include "ofa-mysql-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_CONNECT                ( ofa_mysql_connect_get_type())
#define OFA_MYSQL_CONNECT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_CONNECT, ofaMySQLConnect ))
#define OFA_MYSQL_CONNECT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_CONNECT, ofaMySQLConnectClass ))
#define OFA_IS_MYSQL_CONNECT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_CONNECT ))
#define OFA_IS_MYSQL_CONNECT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_CONNECT ))
#define OFA_MYSQL_CONNECT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_CONNECT, ofaMySQLConnectClass ))

typedef struct _ofaMySQLConnectPrivate        ofaMySQLConnectPrivate;

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMySQLConnect;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMySQLConnectClass;

GType            ofa_mysql_connect_get_type        ( void ) G_GNUC_CONST;

ofaMySQLConnect *ofa_mysql_connect_new             ( void );

gboolean         ofa_mysql_connect_open_with_meta  ( ofaMySQLConnect *connect,
															const gchar *account,
															const gchar *password,
															const ofaMySQLMeta *meta,
															const ofaMySQLPeriod *period );

gboolean         ofa_mysql_connect_query           ( const ofaMySQLConnect *connect,
															const gchar *query );

gchar           *ofa_mysql_connect_get_new_database( ofaMySQLConnect *connect,
															const gchar *prev_database );

G_END_DECLS

#endif /* __OFA_MYSQL_CONNECT_H__ */
