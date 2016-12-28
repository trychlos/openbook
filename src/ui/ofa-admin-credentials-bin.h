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

#ifndef __OFA_ADMIN_CREDENTIALS_BIN_H__
#define __OFA_ADMIN_CREDENTIALS_BIN_H__

/**
 * SECTION: ofa_admin_credentials_bin
 * @short_description: #ofaAdminCredentialsBin class definition.
 * @include: ui/ofa-admin-credentials-bin.h
 *
 * Let the user enter dossier administrative account and password when
 * defining a new (or restoring a) dossier. We do not check here whether
 * the entered credentials are actually registered and valid into the
 * dossier database, but only if they are set.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ADMIN_CREDENTIALS_BIN                ( ofa_admin_credentials_bin_get_type())
#define OFA_ADMIN_CREDENTIALS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBin ))
#define OFA_ADMIN_CREDENTIALS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBinClass ))
#define OFA_IS_ADMIN_CREDENTIALS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ADMIN_CREDENTIALS_BIN ))
#define OFA_IS_ADMIN_CREDENTIALS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ADMIN_CREDENTIALS_BIN ))
#define OFA_ADMIN_CREDENTIALS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ADMIN_CREDENTIALS_BIN, ofaAdminCredentialsBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaAdminCredentialsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaAdminCredentialsBinClass;

GType                   ofa_admin_credentials_bin_get_type      ( void ) G_GNUC_CONST;

ofaAdminCredentialsBin *ofa_admin_credentials_bin_new           ( ofaHub *hub,
																		const gchar *settings_prefix );

GtkSizeGroup           *ofa_admin_credentials_bin_get_size_group( ofaAdminCredentialsBin *bin,
																			guint column );

gboolean                ofa_admin_credentials_bin_is_valid      ( ofaAdminCredentialsBin *bin,
																			gchar **error_message );

G_END_DECLS

#endif /* __OFA_ADMIN_CREDENTIALS_BIN_H__ */
