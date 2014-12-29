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

#ifndef __OFA_DOSSIER_NEW_MINI_H__
#define __OFA_DOSSIER_NEW_MINI_H__

/**
 * SECTION: ofa_dossier_new_mini
 * @short_description: #ofaDossierNewMini class definition.
 * @include: ui/ofa-dossier-new-mini.h
 *
 * Define a new dossier.
 * Use case: when restoring a backup...
 *
 * #ofaDossierNewMini dialog only implements the dossier definition in
 * user settings: the name of the dossier, the connection informations
 * and the DBMS root credentials.
 * The dialog returns with a newly defined dossier in user settings if
 * %TRUE.
 *
 * #ofaDossierNewMini dialog works by implementing the
 * #ofaDossierNewPiece piece of dialog, which itself encapsulates both
 * the #ofaIDbmsConnectEnterPiece and the #ofaDbmsRootPiece pieces of
 * dialog.
 */

#include "core/my-dialog.h"
#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW_MINI                ( ofa_dossier_new_mini_get_type())
#define OFA_DOSSIER_NEW_MINI( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMini ))
#define OFA_DOSSIER_NEW_MINI_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMiniClass ))
#define OFA_IS_DOSSIER_NEW_MINI( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW_MINI ))
#define OFA_IS_DOSSIER_NEW_MINI_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW_MINI ))
#define OFA_DOSSIER_NEW_MINI_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW_MINI, ofaDossierNewMiniClass ))

typedef struct _ofaDossierNewMiniPrivate         ofaDossierNewMiniPrivate;

typedef struct {
	/*< public members >*/
	myDialog                  parent;

	/*< private members >*/
	ofaDossierNewMiniPrivate *priv;
}
	ofaDossierNewMini;

typedef struct {
	/*< public members >*/
	myDialogClass             parent;
}
	ofaDossierNewMiniClass;

GType    ofa_dossier_new_mini_get_type( void ) G_GNUC_CONST;

gboolean ofa_dossier_new_mini_run     ( ofaMainWindow *parent, gchar **dname, gchar **account, gchar **password );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_MINI_H__ */
