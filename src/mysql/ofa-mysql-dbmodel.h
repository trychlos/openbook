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

#ifndef __OFA_MYSQL_DBMODEL_H__
#define __OFA_MYSQL_DBMODEL_H__

/**
 * SECTION: ofa_mysql_dbmodel
 * @short_description: #ofaMysqlDBModel class definition.
 *
 * The main class which manages the DB model.
 *
 * The class is instanciated once when loading the module.
 * It implements the #ofaIDBModel and the #myIIdent interfaces.
 *
 * Notes about data types definitions
 * ==================================
 * TIMESTAMP:
 * cf. https://mariadb.com/kb/en/library/timestamp/
 * In order to keep an auditable audit trace, the timestamp MUST NOT
 * be auto-updated by the DBMS server, but must instead be totally
 * controlled by Openbook software.
 * To obtain that, a DEFAULT 0 is be specified
 * (and because we have chosen to not accept NULL timestamps).
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_DBMODEL                ( ofa_mysql_dbmodel_get_type())
#define OFA_MYSQL_DBMODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModel ))
#define OFA_MYSQL_DBMODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModelClass ))
#define OFA_IS_MYSQL_DBMODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_DBMODEL ))
#define OFA_IS_MYSQL_DBMODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_DBMODEL ))
#define OFA_MYSQL_DBMODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModelClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlDBModel;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlDBModelClass;

GType ofa_mysql_dbmodel_get_type( void );

G_END_DECLS

#endif /* __OFA_MYSQL_DBMODEL_H__ */
