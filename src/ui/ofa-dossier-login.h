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

#ifndef __OFA_DOSSIER_LOGIN_H__
#define __OFA_DOSSIER_LOGIN_H__

/**
 * SECTION: ofa_dossier_login
 * @short_description: #ofaDossierLogin class definition.
 * @include: ui/ofa-dossier-login.h
 *
 * Let the user enter DBMS administrator account and password.
 */

#include "core/my-dialog.h"
#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_LOGIN                ( ofa_dossier_login_get_type())
#define OFA_DOSSIER_LOGIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_LOGIN, ofaDossierLogin ))
#define OFA_DOSSIER_LOGIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_LOGIN, ofaDossierLoginClass ))
#define OFA_IS_DOSSIER_LOGIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_LOGIN ))
#define OFA_IS_DOSSIER_LOGIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_LOGIN ))
#define OFA_DOSSIER_LOGIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_LOGIN, ofaDossierLoginClass ))

typedef struct _ofaDossierLoginPrivate        ofaDossierLoginPrivate;

typedef struct {
	/*< private >*/
	myDialog                parent;
	ofaDossierLoginPrivate *private;
}
	ofaDossierLogin;

typedef struct {
	/*< private >*/
	myDialogClass parent;
}
	ofaDossierLoginClass;

GType    ofa_dossier_login_get_type( void ) G_GNUC_CONST;

void     ofa_dossier_login_run     ( const ofaMainWindow *main_window, const gchar *label, gchar **account, gchar **password );

G_END_DECLS

#endif /* __OFA_DOSSIER_LOGIN_H__ */
