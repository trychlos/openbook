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

#ifndef __OFA_DOSSIER_NEW_H__
#define __OFA_DOSSIER_NEW_H__

/**
 * SECTION: ofa_dossier_new
 * @short_description: #ofaDossierNew class definition.
 * @include: ui/ofa-dossier-new.h
 *
 * Create a new dossier.
 *
 * Creating a new dossier involves following operations:
 * - name the dossier
 * - choose a DBMS provider
 * - enter DBMS connection informations, including database name
 * - enter DBMS root credentials
 * - choose and enter dossier administrative credentials
 *
 * Then (and after asking for user confirmation):
 * - record the dossier connection informations in user settings
 * - (re)create the database, first dropping it if already exist
 * - create and grant the dossier administrative account
 * - creating minimal tables as root in order the administrative
 *   account is allowed to connect to the dossier
 * - create the data model, updating it to last known version
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW                ( ofa_dossier_new_get_type())
#define OFA_DOSSIER_NEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW, ofaDossierNew ))
#define OFA_DOSSIER_NEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW, ofaDossierNewClass ))
#define OFA_IS_DOSSIER_NEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW ))
#define OFA_IS_DOSSIER_NEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW ))
#define OFA_DOSSIER_NEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW, ofaDossierNewClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDossierNew;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDossierNewClass;

GType ofa_dossier_new_get_type( void ) G_GNUC_CONST;

void  ofa_dossier_new_run     ( ofaIGetter *getter,
									GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_H__ */
