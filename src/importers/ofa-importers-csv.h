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

#ifndef __OFA_IMPORTERS_CSV_H__
#define __OFA_IMPORTERS_CSV_H__

/**
 * SECTION: ofa_importers_csv
 * @short_description: #ofaImportersCSV class definition.
 *
 * A #ofaIImmporter implementation which manages text/csv mimetypes.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTERS_CSV                ( ofa_importers_csv_get_type())
#define OFA_IMPORTERS_CSV( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTERS_CSV, ofaImportersCSV ))
#define OFA_IMPORTERS_CSV_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTERS_CSV, ofaImportersCSVClass ))
#define OFA_IS_IMPORTERS_CSV( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTERS_CSV ))
#define OFA_IS_IMPORTERS_CSV_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTERS_CSV ))
#define OFA_IMPORTERS_CSV_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTERS_CSV, ofaImportersCSVClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaImportersCSV;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaImportersCSVClass;

GType ofa_importers_csv_get_type( void );

G_END_DECLS

#endif /* __OFA_IMPORTERS_CSV_H__ */
