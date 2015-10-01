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

#ifndef __OFA_MYSQL_CONNECT_ENTER_BIN_H__
#define __OFA_MYSQL_CONNECT_ENTER_BIN_H__

/**
 * SECTION: ofa_mysql_idbms
 * @short_description: #ofaMysql class definition.
 *
 * Let the user enter connection informations.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <api/ofa-idbms.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_CONNECT_ENTER_BIN                ( ofa_mysql_connect_enter_bin_get_type())
#define OFA_MYSQL_CONNECT_ENTER_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_CONNECT_ENTER_BIN, ofaMySQLConnectEnterBin ))
#define OFA_MYSQL_CONNECT_ENTER_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_CONNECT_ENTER_BIN, ofaMySQLConnectEnterBinClass ))
#define OFA_IS_MYSQL_CONNECT_ENTER_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_CONNECT_ENTER_BIN ))
#define OFA_IS_MYSQL_CONNECT_ENTER_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_CONNECT_ENTER_BIN ))
#define OFA_MYSQL_CONNECT_ENTER_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_CONNECT_ENTER_BIN, ofaMySQLConnectEnterBinClass ))

typedef struct _ofaMySQLConnectEnterBinPrivate          ofaMySQLConnectEnterBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                          parent;

	/*< private members >*/
	ofaMySQLConnectEnterBinPrivate *priv;
}
	ofaMySQLConnectEnterBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                     parent;
}
	ofaMySQLConnectEnterBinClass;

GType         ofa_mysql_connect_enter_bin_get_type      ( void ) G_GNUC_CONST;

GtkWidget    *ofa_mysql_connect_enter_bin_new           ( ofaIDbms *instance );

GtkSizeGroup *ofa_mysql_connect_enter_bin_get_size_group( const ofaIDbms *instance,
																GtkWidget *bin,
																guint column );

gboolean      ofa_mysql_connect_enter_bin_is_valid      ( const ofaIDbms *instance,
																GtkWidget *bin,
																gchar **message );

gchar        *ofa_mysql_connect_enter_bin_get_database  ( const ofaIDbms *instance,
																GtkWidget *bin );

gboolean      ofa_mysql_connect_enter_bin_apply         ( const ofaIDbms *instance,
																const gchar *dname,
																void *infos );

G_END_DECLS

#endif /* __OFA_MYSQL_CONNECT_ENTER_BIN_H__ */
