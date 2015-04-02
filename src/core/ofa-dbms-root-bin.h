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

#ifndef __OFA_DBMS_ROOT_BIN_H__
#define __OFA_DBMS_ROOT_BIN_H__

/**
 * SECTION: ofa_dbms_root_bin
 * @short_description: #ofaDBMSRootBin class definition.
 * @include: core/ofa-dbms-root-bin.h
 *
 * Let the user enter DBMS administrator account and password.
 *
 * The widget is considered valid here if account and password are both
 * set, and - if the dossier is set - valid to connect to an unnamed
 * database.
 *
 * This is a composite widget, so:
 * - have a "top" container
 * - do not manage its own error message display
 * - do not manage its own settings.
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

GtkWidget      *ofa_dbms_root_bin_get_label      ( const ofaDBMSRootBin *bin );

void            ofa_dbms_root_bin_set_dossier    ( ofaDBMSRootBin *bin,
																const gchar *dname );

gboolean        ofa_dbms_root_bin_is_valid       ( const ofaDBMSRootBin *bin,
																gchar **error_message );

void            ofa_dbms_root_bin_set_valid      ( const ofaDBMSRootBin *bin,
																gboolean valid );

void            ofa_dbms_root_bin_set_credentials( ofaDBMSRootBin *bin,
																const gchar *account,
																const gchar *password );

G_END_DECLS

#endif /* __OFA_DBMS_ROOT_BIN_H__ */
