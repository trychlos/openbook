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

#ifndef __OFA_MYSQL_MAIN_H__
#define __OFA_MYSQL_MAIN_H__

/**
 * SECTION: ofa_mysql_main
 * @short_description: #ofaMysqlMain class definition.
 *
 * This class is the main object needed to implement Openbook extensions.
 * Its particularity is to be defined as derived from a *dynamic* module.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_MAIN                ( ofa_mysql_main_get_type())
#define OFA_MYSQL_MAIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_MAIN, ofaMysqlMain ))
#define OFA_MYSQL_MAIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_MAIN, ofaMysqlMainClass ))
#define OFA_IS_MYSQL_MAIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_MAIN ))
#define OFA_IS_MYSQL_MAIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_MAIN ))
#define OFA_MYSQL_MAIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_MAIN, ofaMysqlMainClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlMain;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlMainClass;

GType ofa_mysql_main_get_type        ( void ) G_GNUC_CONST;

void  ofa_mysql_main_register_type_ex( GTypeModule *module );

G_END_DECLS

#endif /* __OFA_MYSQL_MAIN_H__ */
