/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_DOSSIER_MANAGER_H__
#define __OFA_DOSSIER_MANAGER_H__

/**
 * SECTION: ofa_dossier_manager
 * @short_description: #ofaDossierManager class definition.
 * @include: ui/ofa-dossier-manager.h
 *
 * Manage (open/delete) the existing dossiers, or create a new one.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_MANAGER                ( ofa_dossier_manager_get_type())
#define OFA_DOSSIER_MANAGER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_MANAGER, ofaDossierManager ))
#define OFA_DOSSIER_MANAGER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_MANAGER, ofaDossierManagerClass ))
#define OFA_IS_DOSSIER_MANAGER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_MANAGER ))
#define OFA_IS_DOSSIER_MANAGER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_MANAGER ))
#define OFA_DOSSIER_MANAGER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_MANAGER, ofaDossierManagerClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDossierManager;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDossierManagerClass;

GType ofa_dossier_manager_get_type( void ) G_GNUC_CONST;

void  ofa_dossier_manager_run     ( ofaIGetter *getter,
										GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_DOSSIER_MANAGER_H__ */
