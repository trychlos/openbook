/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_IMPORTER_TXT_H__
#define __OFA_IMPORTER_TXT_H__

/**
 * SECTION: ofa_importer_txt
 * @short_description: #ofaImporterTxt class definition.
 * @include: importers/ofa-importer-txt.h
 *
 * This is a base class to handle import of text and tabulated files
 * (which may also be known as application/vnd.ms-excel).
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTER_TXT                ( ofa_importer_txt_get_type())
#define OFA_IMPORTER_TXT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTER_TXT, ofaImporterTxt ))
#define OFA_IMPORTER_TXT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTER_TXT, ofaImporterTxtClass ))
#define OFA_IS_IMPORTER_TXT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTER_TXT ))
#define OFA_IS_IMPORTER_TXT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTER_TXT ))
#define OFA_IMPORTER_TXT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTER_TXT, ofaImporterTxtClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaImporterTxt;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaImporterTxtClass;

GType    ofa_importer_txt_get_type     ( void );

gboolean ofa_importer_txt_is_willing_to( const ofaImporterTxt *instance,
												ofaHub *hub,
												const gchar *uri,
												const GList *accepted_contents );

G_END_DECLS

#endif /* __OFA_IMPORTER_TXT_H__ */
