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

#ifndef __OFA_DOSSIER_DELETE_H__
#define __OFA_DOSSIER_DELETE_H__

/**
 * SECTION: ofa_dossier_delete
 * @short_description: #ofaDossierDelete class definition.
 * @include: ui/ofa-dossier-delete.h
 *
 * Delete a dossier.
 *
 * Development rules:
 * - type:       dialog
 * - settings:   no
 * - current:    no
 */

#include "api/my-dialog.h"
#include "api/ofa-idbmeta-def.h"
#include "api/ofa-idbperiod.h"

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_DELETE                ( ofa_dossier_delete_get_type())
#define OFA_DOSSIER_DELETE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_DELETE, ofaDossierDelete ))
#define OFA_DOSSIER_DELETE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_DELETE, ofaDossierDeleteClass ))
#define OFA_IS_DOSSIER_DELETE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_DELETE ))
#define OFA_IS_DOSSIER_DELETE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_DELETE ))
#define OFA_DOSSIER_DELETE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_DELETE, ofaDossierDeleteClass ))

typedef struct _ofaDossierDeletePrivate        ofaDossierDeletePrivate;

typedef struct {
	/*< public members >*/
	myDialog                 parent;

	/*< private members >*/
	ofaDossierDeletePrivate *priv;
}
	ofaDossierDelete;

typedef struct {
	/*< public members >*/
	myDialogClass            parent;
}
	ofaDossierDeleteClass;

GType    ofa_dossier_delete_get_type( void ) G_GNUC_CONST;

gboolean ofa_dossier_delete_run     ( ofaMainWindow *parent,
											const ofaIDBMeta *meta,
											const ofaIDBPeriod *period );

G_END_DECLS

#endif /* __OFA_DOSSIER_DELETE_H__ */
