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

#ifndef __OFA_DOSSIER_OPEN_H__
#define __OFA_DOSSIER_OPEN_H__

/**
 * SECTION: ofa_dossier_open
 * @short_description: #ofaDossierOpen class definition.
 * @include: ui/ofa-dossier-new.h
 *
 * Guide the user through the process of creating a new dossier.
 */

#include "ui/ofa-main-window.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_OPEN                ( ofa_dossier_open_get_type())
#define OFA_DOSSIER_OPEN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_OPEN, ofaDossierOpen ))
#define OFA_DOSSIER_OPEN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_OPEN, ofaDossierOpenClass ))
#define OFA_IS_DOSSIER_OPEN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_OPEN ))
#define OFA_IS_DOSSIER_OPEN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_OPEN ))
#define OFA_DOSSIER_OPEN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_OPEN, ofaDossierOpenClass ))

typedef struct _ofaDossierOpenPrivate        ofaDossierOpenPrivate;

typedef struct {
	/*< private >*/
	GObject                parent;
	ofaDossierOpenPrivate *private;
}
	ofaDossierOpen;

typedef struct _ofaDossierOpenClassPrivate   ofaDossierOpenClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass                parent;
	ofaDossierOpenClassPrivate *private;
}
	ofaDossierOpenClass;

GType ofa_dossier_open_get_type( void );

void  ofa_dossier_open_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_DOSSIER_OPEN_H__ */
