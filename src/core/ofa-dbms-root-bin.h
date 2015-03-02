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
 *
 */

#ifndef __OFA_DBMS_ROOT_BIN_H__
#define __OFA_DBMS_ROOT_BIN_H__

/**
 * SECTION: ofa_dbms_root_bin
 * @short_description: #ofaDBMSRootBin class definition.
 * @include: core/ofa-dbms-root-bin.h
 *
 * Let the user enter DBMS administrator account and password.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DBMS_ROOT_BIN                ( ofa_dbms_root_bin_get_type())
#define OFA_DBMS_ROOT_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBMS_ROOT_BIN, ofaDBMSRootBin ))
#define OFA_DBMS_ROOT_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBMS_ROOT_BIN, ofaDBMSRootBinClass ))
#define OFA_IS_DBMS_ROOT_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBMS_ROOT_BIN ))
#define OFA_IS_DBMS_ROOT_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBMS_ROOT_BIN ))
#define OFA_DBMS_ROOT_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBMS_ROOT_BIN, ofaDBMSRootBinClass ))

typedef struct _ofaDBMSRootBinPrivate         ofaDBMSRootBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                 parent;

	/*< private members >*/
	ofaDBMSRootBinPrivate *priv;
}
	ofaDBMSRootBin;

typedef struct {
	/*< public members >*/
	GtkBinClass            parent;
}
	ofaDBMSRootBinClass;

GType           ofa_dbms_root_bin_get_type       ( void ) G_GNUC_CONST;

ofaDBMSRootBin *ofa_dbms_root_bin_new            ( void );

void            ofa_dbms_root_bin_attach_to      ( ofaDBMSRootBin *bin,
																GtkContainer *parent,
																GtkSizeGroup *group );

void            ofa_dbms_root_bin_set_dossier    ( ofaDBMSRootBin *bin,
																const gchar *dname );

gboolean        ofa_dbms_root_bin_is_valid       ( const ofaDBMSRootBin *bin );

void            ofa_dbms_root_bin_set_credentials( ofaDBMSRootBin *bin,
																const gchar *account,
																const gchar *password );

void            ofa_dbms_root_bin_set_valid      ( ofaDBMSRootBin *bin,
																gboolean valid );

G_END_DECLS

#endif /* __OFA_DBMS_ROOT_BIN_H__ */
