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

#ifndef __OFA_DOSSIER_NEW_H__
#define __OFA_DOSSIER_NEW_H__

/**
 * SECTION: ofa_dossier_new
 * @short_description: #ofaDossierNew class definition.
 * @include: ui/ofa-dossier-new.h
 *
 * Guide the user through the process of creating a new dossier.
 */

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW                ( ofa_dossier_new_get_type())
#define OFA_DOSSIER_NEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW, ofaDossierNew ))
#define OFA_DOSSIER_NEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW, ofaDossierNewClass ))
#define OFA_IS_DOSSIER_NEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW ))
#define OFA_IS_DOSSIER_NEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW ))
#define OFA_DOSSIER_NEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW, ofaDossierNewClass ))

typedef struct _ofaDossierNewPrivate        ofaDossierNewPrivate;

typedef struct {
	/*< private >*/
	GObject               parent;
	ofaDossierNewPrivate *private;
}
	ofaDossierNew;

typedef struct _ofaDossierNewClassPrivate   ofaDossierNewClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass               parent;
	ofaDossierNewClassPrivate *private;
}
	ofaDossierNewClass;

GType           ofa_dossier_new_get_type( void );

ofaOpenDossier *ofa_dossier_new_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_H__ */
