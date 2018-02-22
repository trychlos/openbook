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

#ifndef __OFA_PAIMEAN_TREEVIEW_H__
#define __OFA_PAIMEAN_TREEVIEW_H__

/**
 * SECTION: ofa_paimean_treeview
 * @short_description: #ofaPaimeanTreeview class definition.
 * @include: core/ofa-paimean-treeview.h
 *
 * Manage a treeview with the list of the means of paiement.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+-------------+
 *    | Signal           | Paimean may |
 *    |                  | be %NULL    |
 *    +------------------+-------------+
 *    | ofa-pamchanged   |     Yes     |
 *    | ofa-pamactivated |      No     |
 *    | ofa-pamdelete    |      No     |
 *    +------------------+-------------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-paimean.h"

G_BEGIN_DECLS

#define OFA_TYPE_PAIMEAN_TREEVIEW                ( ofa_paimean_treeview_get_type())
#define OFA_PAIMEAN_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAIMEAN_TREEVIEW, ofaPaimeanTreeview ))
#define OFA_PAIMEAN_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_PAIMEAN_CAST( klass, OFA_TYPE_PAIMEAN_TREEVIEW, ofaPaimeanTreeviewPaimean ))
#define OFA_IS_PAIMEAN_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAIMEAN_TREEVIEW ))
#define OFA_IS_PAIMEAN_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_PAIMEAN_TYPE(( klass ), OFA_TYPE_PAIMEAN_TREEVIEW ))
#define OFA_PAIMEAN_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAIMEAN_TREEVIEW, ofaPaimeanTreeviewPaimean ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaPaimeanTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaPaimeanTreeviewClass;

GType               ofa_paimean_treeview_get_type        ( void ) G_GNUC_CONST;

ofaPaimeanTreeview *ofa_paimean_treeview_new             ( ofaIGetter *getter,
																const gchar *settings_prefix );

void                ofa_paimean_treeview_setup_store     ( ofaPaimeanTreeview *view );

ofoPaimean         *ofa_paimean_treeview_get_selected    ( ofaPaimeanTreeview *view );

void                ofa_paimean_treeview_set_selected    ( ofaPaimeanTreeview *view,
																const gchar *code );

G_END_DECLS

#endif /* __OFA_PAIMEAN_TREEVIEW_H__ */
