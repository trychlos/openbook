/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_ENTRY_PAGE_DELCONF_H__
#define __OFA_ENTRY_PAGE_DELCONF_H__

/**
 * SECTION: ofa_entry_page_delconf
 * @short_description: #ofaEntryPageDelconf class definition.
 * @include: core/ofa-entry-page-delconf.h
 *
 * Require the user for a confirmation when deleting an entry.
 *
 * Development rules:
 * - type:       modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-entry.h"

G_BEGIN_DECLS

#define OFA_TYPE_ENTRY_PAGE_DELCONF                ( ofa_entry_page_delconf_get_type())
#define OFA_ENTRY_PAGE_DELCONF( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ENTRY_PAGE_DELCONF, ofaEntryPageDelconf ))
#define OFA_ENTRY_PAGE_DELCONF_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ENTRY_PAGE_DELCONF, ofaEntryPageDelconfClass ))
#define OFA_IS_ENTRY_PAGE_DELCONF( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ENTRY_PAGE_DELCONF ))
#define OFA_IS_ENTRY_PAGE_DELCONF_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ENTRY_PAGE_DELCONF ))
#define OFA_ENTRY_PAGE_DELCONF_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ENTRY_PAGE_DELCONF, ofaEntryPageDelconfClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaEntryPageDelconf;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaEntryPageDelconfClass;

GType    ofa_entry_page_delconf_get_type( void ) G_GNUC_CONST;

gboolean ofa_entry_page_delconf_run     ( ofaIGetter *getter,
												ofoEntry *entry,
												GList **entries );

G_END_DECLS

#endif /* __OFA_ENTRY_PAGE_DELCONF_H__ */
