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

#ifndef __OFA_IMPORT_ASSISTANT_H__
#define __OFA_IMPORT_ASSISTANT_H__

/**
 * SECTION: ofa_import
 * @short_description: #ofaImportAssistant class definition.
 * @include: ui/ofa-import-assistant.h
 *
 * Guide the user through the process of importing data.
 *
 * ofaImportAssistant implements the ofaIImporter interface.
 * It so takes benefit of the signals defined by the interface to
 * visually render the progress and the possible error messages.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IMPORT_ASSISTANT                ( ofa_import_assistant_get_type())
#define OFA_IMPORT_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORT_ASSISTANT, ofaImportAssistant ))
#define OFA_IMPORT_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORT_ASSISTANT, ofaImportAssistantClass ))
#define OFA_IS_IMPORT_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORT_ASSISTANT ))
#define OFA_IS_IMPORT_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORT_ASSISTANT ))
#define OFA_IMPORT_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORT_ASSISTANT, ofaImportAssistantClass ))

typedef struct {
	/*< public members >*/
	GtkAssistant      parent;
}
	ofaImportAssistant;

typedef struct {
	/*< public members >*/
	GtkAssistantClass parent;
}
	ofaImportAssistantClass;

GType   ofa_import_assistant_get_type( void ) G_GNUC_CONST;

void    ofa_import_assistant_run     ( ofaIGetter *getter,
											GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_IMPORT_ASSISTANT_H__ */
