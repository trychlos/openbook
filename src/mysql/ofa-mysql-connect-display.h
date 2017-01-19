/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_CONNECT_DISPLAY_H__
#define __OFA_MYSQL_CONNECT_DISPLAY_H__

/**
 * SECTION: ofa_mysql_idbms
 * @short_description: #ofaMysql class definition.
 *
 * Display the connection informations.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "mysql/ofa-mysql-connect.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_CONNECT_DISPLAY                ( ofa_mysql_connect_display_get_type())
#define OFA_MYSQL_CONNECT_DISPLAY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_CONNECT_DISPLAY, ofaMysqlConnectDisplay ))
#define OFA_MYSQL_CONNECT_DISPLAY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_CONNECT_DISPLAY, ofaMysqlConnectDisplayClass ))
#define OFA_IS_MYSQL_CONNECT_DISPLAY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_CONNECT_DISPLAY ))
#define OFA_IS_MYSQL_CONNECT_DISPLAY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_CONNECT_DISPLAY ))
#define OFA_MYSQL_CONNECT_DISPLAY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_CONNECT_DISPLAY, ofaMysqlConnectDisplayClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaMysqlConnectDisplay;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaMysqlConnectDisplayClass;

GType                   ofa_mysql_connect_display_get_type( void ) G_GNUC_CONST;

ofaMysqlConnectDisplay *ofa_mysql_connect_display_new     ( ofaMysqlConnect *connect,
																const gchar *style );

G_END_DECLS

#endif /* __OFA_MYSQL_CONNECT_DISPLAY_H__ */
