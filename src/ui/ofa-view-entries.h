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

#ifndef __OFA_VIEW_ENTRIES_H__
#define __OFA_VIEW_ENTRIES_H__

/**
 * SECTION: ofa_view_entries
 * @short_description: #ofaViewEntries class definition.
 * @include: ui/ofa-view-entries.h
 */

#include "core/ofa-main-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_VIEW_ENTRIES                ( ofa_view_entries_get_type())
#define OFA_VIEW_ENTRIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_VIEW_ENTRIES, ofaViewEntries ))
#define OFA_VIEW_ENTRIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_VIEW_ENTRIES, ofaViewEntriesClass ))
#define OFA_IS_VIEW_ENTRIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_VIEW_ENTRIES ))
#define OFA_IS_VIEW_ENTRIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_VIEW_ENTRIES ))
#define OFA_VIEW_ENTRIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_VIEW_ENTRIES, ofaViewEntriesClass ))

typedef struct _ofaViewEntriesPrivate        ofaViewEntriesPrivate;

typedef struct {
	/*< private >*/
	ofaMainPage            parent;
	ofaViewEntriesPrivate *private;
}
	ofaViewEntries;

typedef struct {
	/*< private >*/
	ofaMainPageClass parent;
}
	ofaViewEntriesClass;

GType ofa_view_entries_get_type       ( void ) G_GNUC_CONST;

void  ofa_view_entries_display_entries( ofaViewEntries *self, GType type, const gchar *id, const GDate *begin, const GDate *end );

G_END_DECLS

#endif /* __OFA_VIEW_ENTRIES_H__ */
