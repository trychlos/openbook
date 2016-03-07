/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_USER_CREDENTIALS_BIN_H__
#define __OFA_USER_CREDENTIALS_BIN_H__

/**
 * SECTION: ofa_user_credentials_bin
 * @short_description: #ofaUserCredentialsBin class definition.
 * @include: core/ofa-admin-credentials-bin.h
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

G_BEGIN_DECLS

#define OFA_TYPE_USER_CREDENTIALS_BIN                ( ofa_user_credentials_bin_get_type())
#define OFA_USER_CREDENTIALS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_USER_CREDENTIALS_BIN, ofaUserCredentialsBin ))
#define OFA_USER_CREDENTIALS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_USER_CREDENTIALS_BIN, ofaUserCredentialsBinClass ))
#define OFA_IS_USER_CREDENTIALS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_USER_CREDENTIALS_BIN ))
#define OFA_IS_USER_CREDENTIALS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_USER_CREDENTIALS_BIN ))
#define OFA_USER_CREDENTIALS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_USER_CREDENTIALS_BIN, ofaUserCredentialsBinClass ))

typedef struct _ofaUserCredentialsBinPrivate         ofaUserCredentialsBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaUserCredentialsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaUserCredentialsBinClass;

GType                  ofa_user_credentials_bin_get_type      ( void ) G_GNUC_CONST;

ofaUserCredentialsBin *ofa_user_credentials_bin_new           ( void );

GtkSizeGroup          *ofa_user_credentials_bin_get_size_group( const ofaUserCredentialsBin *bin,
																			guint column );

void                   ofa_user_credentials_bin_grab_focus    ( const ofaUserCredentialsBin *bin );

void                   ofa_user_credentials_bin_set_account   ( ofaUserCredentialsBin *bin,
																			const gchar *account );

void                   ofa_user_credentials_bin_set_password  ( ofaUserCredentialsBin *bin,
																			const gchar *account );

gboolean               ofa_user_credentials_bin_is_valid      ( const ofaUserCredentialsBin *bin,
																			gchar **msgerr );

G_END_DECLS

#endif /* __OFA_USER_CREDENTIALS_BIN_H__ */
