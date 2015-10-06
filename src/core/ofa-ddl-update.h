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

#ifndef __OFA_DDL_UPDATE_H__
#define __OFA_DDL_UPDATE_H__

/**
 * SECTION: ofa_ddl_update
 * @short_description: #ofaDDLUpdate class definition.
 * @include: core/ofa-ddl-update.h
 *
 * Make sure the database is updated to the last version,
 * simultaneously displaying the applied SQL sentences (if any).
 *
 * Development rules:
 * - type:       dialog
 * - settings:   yes
 * - current:    yes (should not upgrade archived exercices)
 */

#include "api/my-dialog.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DDL_UPDATE                ( ofa_ddl_update_get_type())
#define OFA_DDL_UPDATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DDL_UPDATE, ofaDDLUpdate ))
#define OFA_DDL_UPDATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DDL_UPDATE, ofaDDLUpdateClass ))
#define OFA_IS_DDL_UPDATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DDL_UPDATE ))
#define OFA_IS_DDL_UPDATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DDL_UPDATE ))
#define OFA_DDL_UPDATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DDL_UPDATE, ofaDDLUpdateClass ))

typedef struct _ofaDDLUpdatePrivate        ofaDDLUpdatePrivate;

typedef struct {
	/*< public members >*/
	myDialog             parent;

	/*< private members >*/
	ofaDDLUpdatePrivate *priv;
}
	ofaDDLUpdate;

typedef struct {
	/*< public members >*/
	myDialogClass        parent;
}
	ofaDDLUpdateClass;

GType    ofa_ddl_update_get_type( void ) G_GNUC_CONST;

gboolean ofa_ddl_update_run     ( ofoDossier *dossier );

G_END_DECLS

#endif /* __OFA_DDL_UPDATE_H__ */
