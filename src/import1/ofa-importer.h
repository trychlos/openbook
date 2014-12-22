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

#ifndef __OF1_IMPORTER_H__
#define __OF1_IMPORTER_H__

/**
 * SECTION: of1_importer
 * @short_description: #of1Importer class definition.
 * @include: of1-importer.h
 *
 * Import Bank Account Transaction (BAT) files in tabulated text format.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OF1_TYPE_IMPORTER                ( of1_importer_get_type())
#define OF1_IMPORTER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OF1_TYPE_IMPORTER, of1Importer ))
#define OF1_IMPORTER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OF1_TYPE_IMPORTER, of1ImporterClass ))
#define OF1_IS_IMPORTER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OF1_TYPE_IMPORTER ))
#define OF1_IS_IMPORTER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OF1_TYPE_IMPORTER ))
#define OF1_IMPORTER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OF1_TYPE_IMPORTER, of1ImporterClass ))

typedef struct _of1ImporterPrivate       of1ImporterPrivate;

typedef struct {
	/*< public members >*/
	GObject             parent;

	/*< private members >*/
	of1ImporterPrivate *priv;
}
	of1Importer;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	of1ImporterClass;

GType of1_importer_get_type     ( void );

void  of1_importer_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OF1_IMPORTER_H__ */
