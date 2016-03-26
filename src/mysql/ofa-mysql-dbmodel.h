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
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_DBMODEL                ( ofa_mysql_dbmodel_get_type())
#define OFA_MYSQL_DBMODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModel ))
#define OFA_MYSQL_DBMODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModelClass ))
#define OFA_IS_MYSQL_DBMODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_DBMODEL ))
#define OFA_IS_MYSQL_DBMODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_DBMODEL ))
#define OFA_MYSQL_DBMODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_DBMODEL, ofaMysqlDBModelClass ))

typedef struct _ofaMysqlDBModelPrivate        ofaMysqlDBModelPrivate;

typedef struct {
	/*< public members >*/
	GObject                 parent;

	/*< private members >*/
	ofaMysqlDBModelPrivate *priv;
}
	ofaMysqlDBModel;

typedef struct {
	/*< public members >*/
	GObjectClass            parent;
}
	ofaMysqlDBModelClass;

GType ofa_mysql_dbmodel_get_type     ( void );

void  ofa_mysql_dbmodel_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OFA_MYSQL_DBMODEL_H__ */