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

#ifndef __OFA_DBLOGIN_H__
#define __OFA_DBLOGIN_H__

/**
 * SECTION: ofa_dblogin
 * @short_description: #ofaDBLogin class definition.
 * @include: core/ofa-dblogin.h
 *
 * Let the user enter DBMS administrator account and password.
 */

#include "core/my-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_DBLOGIN                ( ofa_dblogin_get_type())
#define OFA_DBLOGIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBLOGIN, ofaDBLogin ))
#define OFA_DBLOGIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBLOGIN, ofaDBLoginClass ))
#define OFA_IS_DBLOGIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBLOGIN ))
#define OFA_IS_DBLOGIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBLOGIN ))
#define OFA_DBLOGIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBLOGIN, ofaDBLoginClass ))

typedef struct _ofaDBLoginPrivate       ofaDBLoginPrivate;

typedef struct {
	/*< private >*/
	myDialog           parent;
	ofaDBLoginPrivate *private;
}
	ofaDBLogin;

typedef struct {
	/*< private >*/
	myDialogClass parent;
}
	ofaDBLoginClass;

GType    ofa_dblogin_get_type( void ) G_GNUC_CONST;

gboolean ofa_dblogin_run     ( const gchar *label, gchar **account, gchar **password );

G_END_DECLS

#endif /* __OFA_DBLOGIN_H__ */
