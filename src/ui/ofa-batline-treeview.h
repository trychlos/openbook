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

#ifndef __OFA_BATLINE_TREEVIEW_H__
#define __OFA_BATLINE_TREEVIEW_H__

/**
 * SECTION: ofa_batline_treeview
 * @short_description: #ofaBatlineTreeview class definition.
 * @include: ui/ofa-batline-treeview.h
 *
 * Manage a treeview with the list of the lines of a BAT file.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+--------------+
 *    | Signal           |   BAT line   |
 *    |                  | may be %NULL |
 *    +------------------+--------------+
 *    | ofa-balchanged   |      Yes     |
 *    | ofa-balactivated |       No     |
 *    | ofa-baldelete    |       No     |
 *    +------------------+--------------+
 */

#include "api/ofa-tvbin.h"
#include "api/ofo-bat-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BATLINE_TREEVIEW                ( ofa_batline_treeview_get_type())
#define OFA_BATLINE_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BATLINE_TREEVIEW, ofaBatlineTreeview ))
#define OFA_BATLINE_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BATLINE_TREEVIEW, ofaBatlineTreeviewClass ))
#define OFA_IS_BATLINE_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BATLINE_TREEVIEW ))
#define OFA_IS_BATLINE_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BATLINE_TREEVIEW ))
#define OFA_BATLINE_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BATLINE_TREEVIEW, ofaBatlineTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaBatlineTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaBatlineTreeviewClass;

/**
 * The columns stored in the embdded #GtkListStore.
 *                                                                      Displayable
 *                                                             Type     in treeview
 *                                                             -------  -----------
 * @BAL_COL_BAT_ID         : BAT identifier                    String       Yes
 * @BAL_COL_LINE_ID        : line identifier                   String       Yes
 * @BAL_COL_DEFFECT        : effect date                       String       Yes
 * @BAL_COL_DOPE           : operation date                    String       Yes
 * @BAL_COL_REF            : line reference                    String       Yes
 * @BAL_COL_LABEL          : label                             String       Yes
 * @BAL_COL_CURRENCY       : currency (from bat)               String       Yes
 * @BAL_COL_AMOUNT         : amount                            String       Yes
 * @BAL_COL_ENTRY          : conciliated entry                 String       Yes
 * @BAL_COL_USER           : conciliation user                 String       Yes
 * @BAL_COL_STAMP          : conciliation timestamp            String       Yes
 * @BAL_COL_OBJECT         : #ofoBatLine object                GObject       No
 */
enum {
	BAL_COL_BAT_ID = 0,
	BAL_COL_LINE_ID,
	BAL_COL_DEFFECT,
	BAL_COL_DOPE,
	BAL_COL_REF,
	BAL_COL_LABEL,
	BAL_COL_CURRENCY,
	BAL_COL_AMOUNT,
	BAL_COL_ENTRY,
	BAL_COL_USER,
	BAL_COL_STAMP,
	BAL_COL_OBJECT,
	BAL_N_COLUMNS
};

GType               ofa_batline_treeview_get_type        ( void ) G_GNUC_CONST;

ofaBatlineTreeview *ofa_batline_treeview_new             ( void );

void                ofa_batline_treeview_set_settings_key( ofaBatlineTreeview *view,
																const gchar *key );

void                ofa_batline_treeview_set_store       ( ofaBatlineTreeview *view );

void                ofa_batline_treeview_set_bat         ( ofaBatlineTreeview *view,
																ofoBat *bat );

G_END_DECLS

#endif /* __OFA_BATLINE_TREEVIEW_H__ */
