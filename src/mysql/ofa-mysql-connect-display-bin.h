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

#ifndef __OFA_MYSQL_CONNECT_DISPLAY_BIN_H__
#define __OFA_MYSQL_CONNECT_DISPLAY_BIN_H__

/**
 * SECTION: ofa_mysql_idbms
 * @short_description: #ofaMysql class definition.
 *
 * Display the connection informations read for the named dossier
 * from the settings.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include <api/ofa-idbms.h>

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN                ( ofa_mysql_connect_display_bin_get_type())
#define OFA_MYSQL_CONNECT_DISPLAY_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN, ofaMySQLConnectDisplayBin ))
#define OFA_MYSQL_CONNECT_DISPLAY_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN, ofaMySQLConnectDisplayBinClass ))
#define OFA_IS_MYSQL_CONNECT_DISPLAY_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN ))
#define OFA_IS_MYSQL_CONNECT_DISPLAY_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN ))
#define OFA_MYSQL_CONNECT_DISPLAY_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN, ofaMySQLConnectDisplayBinClass ))

typedef struct _ofaMySQLConnectDisplayBinPrivate          ofaMySQLConnectDisplayBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                            parent;

	/*< private members >*/
	ofaMySQLConnectDisplayBinPrivate *priv;
}
	ofaMySQLConnectDisplayBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                       parent;
}
	ofaMySQLConnectDisplayBinClass;

GType         ofa_mysql_connect_display_bin_get_type      ( void ) G_GNUC_CONST;

GtkWidget    *ofa_mysql_connect_display_bin_new           ( const ofaIDbms *instance,
																	const gchar *dname );

GtkSizeGroup *ofa_mysql_connect_display_bin_get_size_group( const ofaIDbms *instance,
																	GtkWidget *bin,
																	guint column );

G_END_DECLS

#endif /* __OFA_MYSQL_CONNECT_DISPLAY_BIN_H__ */
