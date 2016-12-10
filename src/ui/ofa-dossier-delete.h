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
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_DELETE                ( ofa_dossier_delete_get_type())
#define OFA_DOSSIER_DELETE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_DELETE, ofaDossierDelete ))
#define OFA_DOSSIER_DELETE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_DELETE, ofaDossierDeleteClass ))
#define OFA_IS_DOSSIER_DELETE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_DELETE ))
#define OFA_IS_DOSSIER_DELETE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_DELETE ))
#define OFA_DOSSIER_DELETE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_DELETE, ofaDossierDeleteClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDossierDelete;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDossierDeleteClass;

GType ofa_dossier_delete_get_type( void ) G_GNUC_CONST;

void  ofa_dossier_delete_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										const ofaIDBDossierMeta *dossier_meta,
										const ofaIDBExerciceMeta *period );

G_END_DECLS

#endif /* __OFA_DOSSIER_DELETE_H__ */
