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

#ifndef __OPENBOOK_API_OFA_ENTRY_PAGE_H__
#define __OPENBOOK_API_OFA_ENTRY_PAGE_H__

/**
 * SECTION: ofa_entry_page
 * @short_description: #ofaEntryPage class definition.
 * @include: openbook/ofa-entry-page.h
 *
 * Display the entries with various selection criteria.
 */

#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ENTRY_PAGE                ( ofa_entry_page_get_type())
#define OFA_ENTRY_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ENTRY_PAGE, ofaEntryPage ))
#define OFA_ENTRY_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ENTRY_PAGE, ofaEntryPageClass ))
#define OFA_IS_ENTRY_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ENTRY_PAGE ))
#define OFA_IS_ENTRY_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ENTRY_PAGE ))
#define OFA_ENTRY_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ENTRY_PAGE, ofaEntryPageClass ))

typedef struct {
	/*< public members >*/
	ofaPage      parent;
}
	ofaEntryPage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaEntryPageClass;

GType ofa_entry_page_get_type          ( void ) G_GNUC_CONST;

void  ofa_entry_page_display_entries   ( ofaEntryPage *page,
												GType type,
												const gchar *type_id,
												const GDate *begin,
												const GDate *end );

void  ofa_entry_page_display_operations( ofaEntryPage *page,
												GList *opes );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ENTRY_PAGE_H__ */
