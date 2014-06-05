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

#ifndef __OF2_IMPORTER_H__
#define __OF2_IMPORTER_H__

/**
 * SECTION: of2_importer
 * @short_description: #of2Importer class definition.
 * @include: of2-importer.h
 *
 * Import reference tables in CSV format.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OF2_TYPE_IMPORTER                ( of2_importer_get_type())
#define OF2_IMPORTER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OF2_TYPE_IMPORTER, of2Importer ))
#define OF2_IMPORTER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OF2_TYPE_IMPORTER, of2ImporterClass ))
#define OF2_IS_IMPORTER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OF2_TYPE_IMPORTER ))
#define OF2_IS_IMPORTER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OF2_TYPE_IMPORTER ))
#define OF2_IMPORTER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OF2_TYPE_IMPORTER, of2ImporterClass ))

typedef struct _of2ImporterPrivate       of2ImporterPrivate;

typedef struct {
	/*< private >*/
	GObject             parent;
	of2ImporterPrivate *private;
}
	of2Importer;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	of2ImporterClass;

GType of2_importer_get_type     ( void );
void  of2_importer_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OF2_IMPORTER_H__ */
