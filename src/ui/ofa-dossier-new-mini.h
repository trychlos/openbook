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

#ifndef __OFA_DOSSIER_NEW_MINI_H__
#define __OFA_DOSSIER_NEW_MINI_H__

/**
 * SECTION: ofa_dossier_new_mini
 * @short_description: #ofaDossierNewMini class definition.
 * @include: ui/ofa-dossier-new-mini.h
 *
 * Define a new dossier in dossier settings, but does not create the
 * corresponding database (thus does not define dossier administrative
 * credentials).
 * Use case: when restoring a backup...
 *
 * #ofaDossierNewMini dialog only implements the dossier definition in
 * user settings: the name of the dossier and the connection informations.
 * The dialog returns with a newly defined dossier in user settings if
 * %TRUE.
 *
 * #ofaDossierNewMini dialog makes use of
 * #ofaDossierNewBin piece of dialog, which itself encapsulates
 * the #ofaIDBEditor piece of dialog.
 *
 * Development rules:
 * - type:       modal dialog
 * - settings:   no
 * - current:    no
 */

#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW_MINI                ( ofa_dossier_new_mini_get_type())
#define OFA_DOSSIER_NEW_MINI( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMini ))
#define OFA_DOSSIER_NEW_MINI_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMiniClass ))
#define OFA_IS_DOSSIER_NEW_MINI( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW_MINI ))
#define OFA_IS_DOSSIER_NEW_MINI_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW_MINI ))
#define OFA_DOSSIER_NEW_MINI_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMiniClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDossierNewMini;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDossierNewMiniClass;

GType    ofa_dossier_new_mini_get_type( void ) G_GNUC_CONST;

gboolean ofa_dossier_new_mini_run     ( ofaIGetter *getter,
												GtkWindow *parent,
												ofaIDBDossierMeta **meta );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_MINI_H__ */
