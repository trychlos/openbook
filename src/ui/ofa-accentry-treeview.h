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

#ifndef __OFA_ACCENTRY_TREEVIEW_H__
#define __OFA_ACCENTRY_TREEVIEW_H__

/**
 * SECTION: ofa_accentry_treeview
 * @short_description: #ofaAccentryTreeview class definition.
 * @include: ui/ofa-accentry-treeview.h
 *
 * Manage a treeview with a filtered list of accounts and entries.
 *
 * This class is dedicated to visualizations. It does not allow any
 * edition.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+------------+
 *    | Signal           | List may   |
 *    |                  | be empty   |
 *    +------------------+------------+
 *    | ofa-accchanged   |     Yes    |
 *    | ofa-accactivated |      No    |
 *    +------------------+------------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-base-def.h"
#include "api/ofo-entry.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCENTRY_TREEVIEW                ( ofa_accentry_treeview_get_type())
#define OFA_ACCENTRY_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCENTRY_TREEVIEW, ofaAccentryTreeview ))
#define OFA_ACCENTRY_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCENTRY_TREEVIEW, ofaAccentryTreeviewClass ))
#define OFA_IS_ACCENTRY_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCENTRY_TREEVIEW ))
#define OFA_IS_ACCENTRY_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCENTRY_TREEVIEW ))
#define OFA_ACCENTRY_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCENTRY_TREEVIEW, ofaAccentryTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaAccentryTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaAccentryTreeviewClass;

GType                ofa_accentry_treeview_get_type       ( void ) G_GNUC_CONST;

ofaAccentryTreeview *ofa_accentry_treeview_new            ( ofaIGetter *getter,
																	const gchar *settings_prefix );

ofoBase             *ofa_accentry_treeview_get_selected   ( ofaAccentryTreeview *view );

void                 ofa_accentry_treeview_set_filter_func( ofaAccentryTreeview *view,
																	GtkTreeModelFilterVisibleFunc filter_fn,
																	void *filter_data );

void                 ofa_accentry_treeview_collapse_all   ( ofaAccentryTreeview *view );

void                 ofa_accentry_treeview_expand_all     ( ofaAccentryTreeview *view );

G_END_DECLS

#endif /* __OFA_ACCENTRY_TREEVIEW_H__ */
