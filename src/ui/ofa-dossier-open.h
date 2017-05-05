/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_DOSSIER_OPEN_H__
#define __OFA_DOSSIER_OPEN_H__

/**
 * SECTION: ofa_dossier_open
 * @short_description: #ofaDossierOpen class definition.
 * @include: ui/ofa-dossier-open.h
 *
 * Open an existing dossier.
 *
 * #ofa_dossier_open_run_modal() function should be the canonical way of
 * opening a dossier from the user interface.
 *
 * It takes care:
 * - of validating the provided parameters, displaying the dialog and
 *   requesting the user if unset or invalid;
 * - closing the currently opened dossier (if any);
 * - opening the dossier and setting up the application
 *   (cf. #ofa_hub_open_dossier() function).
 *
 * Development rules:
 * - type:       modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_OPEN                ( ofa_dossier_open_get_type())
#define OFA_DOSSIER_OPEN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_OPEN, ofaDossierOpen ))
#define OFA_DOSSIER_OPEN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_OPEN, ofaDossierOpenClass ))
#define OFA_IS_DOSSIER_OPEN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_OPEN ))
#define OFA_IS_DOSSIER_OPEN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_OPEN ))
#define OFA_DOSSIER_OPEN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_OPEN, ofaDossierOpenClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDossierOpen;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDossierOpenClass;

GType    ofa_dossier_open_get_type ( void ) G_GNUC_CONST;

gboolean ofa_dossier_open_run_modal( ofaIGetter *getter,
										GtkWindow *parent,
										ofaIDBExerciceMeta *exercice_meta,
										const gchar *account,
										const gchar *password,
										gboolean read_only );

G_END_DECLS

#endif /* __OFA_DOSSIER_OPEN_H__ */
