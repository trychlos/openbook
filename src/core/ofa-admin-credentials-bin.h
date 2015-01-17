/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifndef __OFA_ADMIN_CREDENTIALS_BIN_H__
#define __OFA_ADMIN_CREDENTIALS_BIN_H__

/**
 * SECTION: ofa_admin_credentials_bin
 * @short_description: #ofaAdminCredentialsBin class definition.
 * @include: core/ofa-admin-credentials-bin.h
 *
 * Let the user enter dossier administrative account and password when
 * defining a new dossier, so we do not check here whether the entered
 * credentials are actually registered into the dossier database.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_ADMIN_CREDENTIALS_BIN                ( ofa_admin_credentials_bin_get_type())
#define OFA_ADMIN_CREDENTIALS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBin ))
#define OFA_ADMIN_CREDENTIALS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBinClass ))
#define OFA_IS_ADMIN_CREDENTIALS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ADMIN_CREDENTIALS_BIN ))
#define OFA_IS_ADMIN_CREDENTIALS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ADMIN_CREDENTIALS_BIN ))
#define OFA_ADMIN_CREDENTIALS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBinClass ))

typedef struct _ofaAdminCredentialsBinPrivate         ofaAdminCredentialsBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                         parent;

	/*< private members >*/
	ofaAdminCredentialsBinPrivate *priv;
}
	ofaAdminCredentialsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                    parent;
}
	ofaAdminCredentialsBinClass;

GType                   ofa_admin_credentials_bin_get_type ( void ) G_GNUC_CONST;

ofaAdminCredentialsBin *ofa_admin_credentials_bin_new      ( void );

gboolean                ofa_admin_credentials_bin_is_valid ( const ofaAdminCredentialsBin *bin );

G_END_DECLS

#endif /* __OFA_ADMIN_CREDENTIALS_BIN_H__ */
