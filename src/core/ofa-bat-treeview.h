/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_BAT_TREEVIEW_H__
#define __OFA_BAT_TREEVIEW_H__

/**
 * SECTION: ofa_bat_treeview
 * @short_description: #ofaBatTreeview class definition.
 * @include: core/ofa-bat-treeview.h
 *
 * Manage a treeview with the list of the BAT files imported in the
 * dossier.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+----------+
 *    | Signal           | BAT may  |
 *    |                  | be %NULL |
 *    +------------------+----------+
 *    | ofa-batchanged   |   Yes    |
 *    | ofa-batactivated |    No    |
 *    | ofa-batdelete    |    No    |
 *    +------------------+----------+
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-bat-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_TREEVIEW                ( ofa_bat_treeview_get_type())
#define OFA_BAT_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_TREEVIEW, ofaBatTreeview ))
#define OFA_BAT_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_TREEVIEW, ofaBatTreeviewClass ))
#define OFA_IS_BAT_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_TREEVIEW ))
#define OFA_IS_BAT_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_TREEVIEW ))
#define OFA_BAT_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_TREEVIEW, ofaBatTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaBatTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaBatTreeviewClass;

GType           ofa_bat_treeview_get_type        ( void ) G_GNUC_CONST;

ofaBatTreeview *ofa_bat_treeview_new             ( ofaIGetter *getter,
														const gchar *settings_prefix );

void            ofa_bat_treeview_setup_store     ( ofaBatTreeview *view );

ofoBat         *ofa_bat_treeview_get_selected    ( ofaBatTreeview *view );

void            ofa_bat_treeview_set_selected    ( ofaBatTreeview *view,
														ofxCounter id );

G_END_DECLS

#endif /* __OFA_BAT_TREEVIEW_H__ */
