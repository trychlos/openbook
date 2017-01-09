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

#ifndef __OFA_MYSQL_DOSSIER_BIN_H__
#define __OFA_MYSQL_DOSSIER_BIN_H__

/**
 * SECTION: ofa_mysql_dossier_bin
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

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"

#include "mysql/ofa-mysql-dbprovider.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_DOSSIER_BIN                ( ofa_mysql_dossier_bin_get_type())
#define OFA_MYSQL_DOSSIER_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_DOSSIER_BIN, ofaMysqlDossierBin ))
#define OFA_MYSQL_DOSSIER_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_DOSSIER_BIN, ofaMysqlDossierBinClass ))
#define OFA_IS_MYSQL_DOSSIER_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_DOSSIER_BIN ))
#define OFA_IS_MYSQL_DOSSIER_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_DOSSIER_BIN ))
#define OFA_MYSQL_DOSSIER_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_DOSSIER_BIN, ofaMysqlDossierBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaMysqlDossierBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaMysqlDossierBinClass;

GType               ofa_mysql_dossier_bin_get_type        ( void ) G_GNUC_CONST;

ofaMysqlDossierBin *ofa_mysql_dossier_bin_new             ( ofaMysqlDBProvider *provider,
																const gchar *settings_prefix,
																guint rule );

GtkSizeGroup       *ofa_mysql_dossier_bin_get_size_group  ( ofaMysqlDossierBin *bin,
																guint column );

gboolean            ofa_mysql_dossier_bin_is_valid        ( ofaMysqlDossierBin *bin,
																gchar **error_message );

const gchar        *ofa_mysql_dossier_bin_get_host        ( ofaMysqlDossierBin *bin );

guint               ofa_mysql_dossier_bin_get_port        ( ofaMysqlDossierBin *bin );

const gchar        *ofa_mysql_dossier_bin_get_socket      ( ofaMysqlDossierBin *bin );

void                ofa_mysql_dossier_bin_set_dossier_meta( ofaMysqlDossierBin *bin,
																ofaIDBDossierMeta *dossier_meta );

G_END_DECLS

#endif /* __OFA_MYSQL_DOSSIER_BIN_H__ */
