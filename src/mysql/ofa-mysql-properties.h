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

#ifndef __OFA_MYSQL_PROPERTIES_H__
#define __OFA_MYSQL_PROPERTIES_H__

/**
 * SECTION: ofa_mysql_properties
 * @short_description: #ofaMysqlProperties class definition.
 *
 * A convenience object which implements ofaIProperties, and
 * let us allocate new ofaMysqlPrefsBin as needed.
 *
 * The class is instanciated once when loading the module.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_PROPERTIES                ( ofa_mysql_properties_get_type())
#define OFA_MYSQL_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_PROPERTIES, ofaMysqlProperties ))
#define OFA_MYSQL_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_PROPERTIES, ofaMysqlPropertiesClass ))
#define OFA_IS_MYSQL_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_PROPERTIES ))
#define OFA_IS_MYSQL_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_PROPERTIES ))
#define OFA_MYSQL_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_PROPERTIES, ofaMysqlPropertiesClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlProperties;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlPropertiesClass;

GType ofa_mysql_properties_get_type( void );

G_END_DECLS

#endif /* __OFA_MYSQL_PROPERTIES_H__ */
