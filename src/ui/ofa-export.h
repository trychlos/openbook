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

#ifndef __OFA_EXPORT_H__
#define __OFA_EXPORT_H__

/**
 * SECTION: ofa_export
 * @short_description: #ofaExport class definition.
 * @include: ui/ofa-export.h
 *
 * Guide the user through the process of exporting data.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXPORT                ( ofa_export_get_type())
#define OFA_EXPORT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXPORT, ofaExport ))
#define OFA_EXPORT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXPORT, ofaExportClass ))
#define OFA_IS_EXPORT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXPORT ))
#define OFA_IS_EXPORT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXPORT ))
#define OFA_EXPORT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXPORT, ofaExportClass ))

typedef struct _ofaExportPrivate        ofaExportPrivate;

typedef struct {
	/*< public memnbers >*/
	GObject           parent;

	/*< private memnbers >*/
	ofaExportPrivate *priv;
}
	ofaExport;

typedef struct {
	/*< public memnbers >*/
	GObjectClass parent;
}
	ofaExportClass;

GType   ofa_export_get_type( void ) G_GNUC_CONST;

void    ofa_export_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_EXPORT_H__ */
