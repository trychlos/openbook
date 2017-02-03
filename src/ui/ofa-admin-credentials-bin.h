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

#ifndef __OFA_ADMIN_CREDENTIALS_BIN_H__
#define __OFA_ADMIN_CREDENTIALS_BIN_H__

/**
 * SECTION: ofa_admin_credentials_bin
 * @short_description: #ofaAdminCredentialsBin class definition.
 * @include: ui/ofa-admin-credentials-bin.h
 *
 * Let the user define the administrative account and password of an
 * exercice.
 *
 * This widget is used:
 * - from restore assistant
 * - in new dossier dialog
 *
 * The widget implements the #myIBin interface, but does not provide
 * any code for the apply() method. Instead, the caller should get the
 * currently set credentials, and act accordingly.
 *
 * Whether the administrative account of a dossier should be remembered
 * is an application-wide user preferences. The administrative account
 * itself is a per-dossier settings (and so requires an #ofaIDBDossierMeta
 * to have been set).
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofa-idbdossier-meta-def.h"

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

GType                   ofa_admin_credentials_bin_get_type              ( void ) G_GNUC_CONST;

ofaAdminCredentialsBin *ofa_admin_credentials_bin_new                   ( ofaIGetter *getter,
																				const gchar *settings_prefix,
																				guint rule );

void                    ofa_admin_credentials_bin_set_dossier_meta      ( ofaAdminCredentialsBin *bin,
																				ofaIDBDossierMeta *dossier_meta );

const gchar            *ofa_admin_credentials_bin_get_remembered_account( ofaAdminCredentialsBin *bin );

void                    ofa_admin_credentials_bin_get_credentials       ( ofaAdminCredentialsBin *bin,
																				gchar **account,
																				gchar **password );

G_END_DECLS

#endif /* __OFA_ADMIN_CREDENTIALS_BIN_H__ */
