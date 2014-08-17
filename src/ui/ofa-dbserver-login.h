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

#ifndef __OFA_DBSERVER_LOGIN_H__
#define __OFA_DBSERVER_LOGIN_H__

/**
 * SECTION: ofa_dbserver_login
 * @short_description: #ofaDBserverLogin class definition.
 * @include: ui/ofa-dbserver-login.h
 *
 * Provides informations required to connect to the DB server.
 */

#include "ui/my-dialog.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DBSERVER_LOGIN                ( ofa_dbserver_login_get_type())
#define OFA_DBSERVER_LOGIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBSERVER_LOGIN, ofaDBserverLogin ))
#define OFA_DBSERVER_LOGIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBSERVER_LOGIN, ofaDBserverLoginClass ))
#define OFA_IS_DBSERVER_LOGIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBSERVER_LOGIN ))
#define OFA_IS_DBSERVER_LOGIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBSERVER_LOGIN ))
#define OFA_DBSERVER_LOGIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBSERVER_LOGIN, ofaDBserverLoginClass ))

typedef struct _ofaDBserverLoginPrivate        ofaDBserverLoginPrivate;

typedef struct {
	/*< private >*/
	myDialog                 parent;
	ofaDBserverLoginPrivate *private;
}
	ofaDBserverLogin;

typedef struct {
	/*< private >*/
	myDialogClass parent;
}
	ofaDBserverLoginClass;

GType    ofa_dbserver_login_get_type( void ) G_GNUC_CONST;

gboolean ofa_dbserver_login_run     ( ofaMainWindow *parent,
										const gchar *name,
										const gchar *provider, const gchar *host, const gchar *dbname,
										gchar **account, gchar **password,
										gboolean *remove_account );

G_END_DECLS

#endif /* __OFA_DBSERVER_LOGIN_H__ */
