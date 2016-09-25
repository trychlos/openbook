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

#ifndef __OFA_ENTRY_TREEVIEW_H__
#define __OFA_ENTRY_TREEVIEW_H__

/**
 * SECTION: ofa_entry_treeview
 * @short_description: #ofaEntryTreeview class definition.
 * @include: ui/ofa-entry-treeview.h
 *
 * Manage a treeview with a filtered list of entries.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+------------+
 *    | Signal           | Entry may  |
 *    |                  | be %NULL   |
 *    +------------------+------------+
 *    | ofa-entchanged   |     Yes    |
 *    | ofa-entactivated |      No    |
 *    | ofa-entdelete    |      No    |
 *    +------------------+------------+
 *
 * As the treeview may allow multiple selection, both signals provide
 * a list of selected objects. It is up to the user of this class to
 * decide whether an action may apply or not on a multiple selection.
 */

#include "api/ofa-tvbin.h"
#include "api/ofo-entry.h"

G_BEGIN_DECLS

#define OFA_TYPE_ENTRY_TREEVIEW                ( ofa_entry_treeview_get_type())
#define OFA_ENTRY_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ENTRY_TREEVIEW, ofaEntryTreeview ))
#define OFA_ENTRY_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ENTRY_TREEVIEW, ofaEntryTreeviewClass ))
#define OFA_IS_ENTRY_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ENTRY_TREEVIEW ))
#define OFA_IS_ENTRY_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ENTRY_TREEVIEW ))
#define OFA_ENTRY_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ENTRY_TREEVIEW, ofaEntryTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaEntryTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaEntryTreeviewClass;

/* error levels, in ascending order
 */
enum {
	ENTRY_ERR_NONE = 0,
	ENTRY_ERR_WARNING,
	ENTRY_ERR_ERROR
};

GType             ofa_entry_treeview_get_type          ( void ) G_GNUC_CONST;

ofaEntryTreeview *ofa_entry_treeview_new               ( void );

void              ofa_entry_treeview_set_settings_key  ( ofaEntryTreeview *view,
																const gchar *key );

void              ofa_entry_treeview_setup_columns     ( ofaEntryTreeview *view );

GList            *ofa_entry_treeview_get_selected      ( ofaEntryTreeview *view );

#define           ofa_entry_treeview_free_selected(L)  g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

void              ofa_entry_treeview_set_selected      ( ofaEntryTreeview *view,
																ofxCounter entry );

void              ofa_entry_treeview_cell_data_render  ( ofaEntryTreeview *view,
																GtkTreeViewColumn *column,
																GtkCellRenderer *renderer,
																GtkTreeModel *model,
																GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_ENTRY_TREEVIEW_H__ */
