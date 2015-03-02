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

#ifndef __OFA_DBMS_DEF_H__
#define __OFA_DBMS_DEF_H__

/**
 * SECTION: ofa_dbms
 * @short_description: An object which handles the DBMS connexion
 * @include: api/ofa-dbms.h
 *
 * This class provides a convenience object to interact with the
 * #ofaIDbms interface. In particular, it takes in charges all
 * conversions between user-level names (dossier, provider, etc.)
 * and the corresponding code-level objects.
 *
 * Also it holds and maintains the needed informations for keeping
 * the DBMS connection running.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_DBMS                ( ofa_dbms_get_type())
#define OFA_DBMS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBMS, ofaDbms ))
#define OFA_DBMS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBMS, ofaDbmsClass ))
#define OFA_IS_DBMS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBMS ))
#define OFA_IS_DBMS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBMS ))
#define OFA_DBMS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBMS, ofaDbmsClass ))

typedef struct _ofaDbmsPrivate       ofaDbmsPrivate;

typedef struct {
	/*< public members >*/
	GObject         parent;

	/*< private members >*/
	ofaDbmsPrivate *priv;
}
	ofaDbms;

typedef struct {
	/*< public members >*/
	GObjectClass    parent;
}
	ofaDbmsClass;

GType ofa_dbms_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_DBMS_DEF_H__ */
