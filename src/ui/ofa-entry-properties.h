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

#ifndef __OFA_ENTRY_PROPERTIES_H__
#define __OFA_ENTRY_PROPERTIES_H__

/**
 * SECTION: ofa_entry_properties
 * @short_description: #ofaEntryProperties class definition.
 * @include: ui/ofa-entry-properties.h
 *
 * Update/display the entry properties.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-entry.h"

G_BEGIN_DECLS

#define OFA_TYPE_ENTRY_PROPERTIES                ( ofa_entry_properties_get_type())
#define OFA_ENTRY_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ENTRY_PROPERTIES, ofaEntryProperties ))
#define OFA_ENTRY_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ENTRY_PROPERTIES, ofaEntryPropertiesClass ))
#define OFA_IS_ENTRY_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ENTRY_PROPERTIES ))
#define OFA_IS_ENTRY_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ENTRY_PROPERTIES ))
#define OFA_ENTRY_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ENTRY_PROPERTIES, ofaEntryPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaEntryProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaEntryPropertiesClass;

GType ofa_entry_properties_get_type( void ) G_GNUC_CONST;

void  ofa_entry_properties_run     ( ofaIGetter *getter,
											GtkWindow *parent,
											ofoEntry *entry,
											gboolean editable );

G_END_DECLS

#endif /* __OFA_ENTRY_PROPERTIES_H__ */
