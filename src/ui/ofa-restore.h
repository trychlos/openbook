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

#ifndef __OFA_RESTORE_H__
#define __OFA_RESTORE_H__

/**
 * SECTION: ofa_restore
 * @short_description: #ofaRestore class definition.
 * @include: ui/ofa-restore.h
 *
 * Restore a backuped database into a dossier.
 *
 * The destination dossier may be an existing one, or a new dossier to
 * be defined.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RESTORE                ( ofa_restore_get_type())
#define OFA_RESTORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RESTORE, ofaRestore ))
#define OFA_RESTORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RESTORE, ofaRestoreClass ))
#define OFA_IS_RESTORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RESTORE ))
#define OFA_IS_RESTORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RESTORE ))
#define OFA_RESTORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RESTORE, ofaRestoreClass ))

typedef struct _ofaRestorePrivate        ofaRestorePrivate;

typedef struct {
	/*< public members >*/
	GObject            parent;

	/*< private members >*/
	ofaRestorePrivate *priv;
}
	ofaRestore;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaRestoreClass;

GType ofa_restore_get_type( void ) G_GNUC_CONST;

void  ofa_restore_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_RESTORE_H__ */
