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

#ifndef __OFA_EXPORT_ASSISTANT_H__
#define __OFA_EXPORT_ASSISTANT_H__

/**
 * SECTION: ofa_export_assistant
 * @short_description: #ofaExportAssistant class definition.
 * @include: ui/ofa-export.h
 *
 * Guide the user through the process of exporting data.
 */

#include "api/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXPORT_ASSISTANT                ( ofa_export_assistant_get_type())
#define OFA_EXPORT_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXPORT_ASSISTANT, ofaExportAssistant ))
#define OFA_EXPORT_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXPORT_ASSISTANT, ofaExportAssistantClass ))
#define OFA_IS_EXPORT_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXPORT_ASSISTANT ))
#define OFA_IS_EXPORT_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXPORT_ASSISTANT ))
#define OFA_EXPORT_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXPORT_ASSISTANT, ofaExportAssistantClass ))

typedef struct {
	/*< public memnbers >*/
	GtkAssistant      parent;
}
	ofaExportAssistant;

typedef struct {
	/*< public memnbers >*/
	GtkAssistantClass parent;
}
	ofaExportAssistantClass;

GType   ofa_export_assistant_get_type( void ) G_GNUC_CONST;

void    ofa_export_assistant_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_EXPORT_ASSISTANT_H__ */
