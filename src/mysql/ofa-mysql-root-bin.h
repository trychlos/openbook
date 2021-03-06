/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_ROOT_BIN_H__
#define __OFA_MYSQL_ROOT_BIN_H__

/**
 * SECTION: ofa_mysql_root_bin
 * @short_description: #ofaMysqlRootBin class definition.
 * @include: core/ofa-dbms-root-bin.h
 *
 * Let the user enter MYSQL administrator/root/superuser account and
 * password.
 *
 * The widget is considered valid here if account and password are both
 * set, and - if the dossier is set - valid to connect to an unnamed
 * database (at the DBMS server level).
 *
 * The widget implements the #ofaIDBSuperuser interface.
 *
 * The widget is used in three cases:
 * - when creating a new dossier/exercice
 * - when restoring an archive file to an exercice
 * - when closing an exercice and opening a new one.
 *
 * Settings are only read/written when this widget is used in association
 * with a #ofaIDBDossierMeta instance. This is in particular not the case
 * when creating a new dossier/exercice.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'my-ibin-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"

#include "mysql/ofa-mysql-dbprovider.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_ROOT_BIN                ( ofa_mysql_root_bin_get_type())
#define OFA_MYSQL_ROOT_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_ROOT_BIN, ofaMysqlRootBin ))
#define OFA_MYSQL_ROOT_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_ROOT_BIN, ofaMysqlRootBinClass ))
#define OFA_IS_MYSQL_ROOT_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_ROOT_BIN ))
#define OFA_IS_MYSQL_ROOT_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_ROOT_BIN ))
#define OFA_MYSQL_ROOT_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_ROOT_BIN, ofaMysqlRootBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaMysqlRootBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaMysqlRootBinClass;

GType            ofa_mysql_root_bin_get_type              ( void ) G_GNUC_CONST;

ofaMysqlRootBin *ofa_mysql_root_bin_new                   ( ofaMysqlDBProvider *provider,
																guint rule );

void             ofa_mysql_root_bin_set_dossier_meta      ( ofaMysqlRootBin *bin,
																ofaIDBDossierMeta *dossier_meta );

void             ofa_mysql_root_bin_set_valid             ( ofaMysqlRootBin *bin,
																gboolean valid );

const gchar     *ofa_mysql_root_bin_get_account           ( ofaMysqlRootBin *bin );

const gchar     *ofa_mysql_root_bin_get_password          ( ofaMysqlRootBin *bin );

const gchar     *ofa_mysql_root_bin_get_remembered_account( ofaMysqlRootBin *bin );

#if 0
void             ofa_mysql_root_bin_get_credentials       ( ofaMysqlRootBin *bin,
																gchar **account,
																gchar **password );

void             ofa_mysql_root_bin_set_credentials       ( ofaMysqlRootBin *bin,
																const gchar *account,
																const gchar *password );
#endif

G_END_DECLS

#endif /* __OFA_MYSQL_ROOT_BIN_H__ */
