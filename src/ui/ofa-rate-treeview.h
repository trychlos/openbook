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

#ifndef __OFA_RATE_TREEVIEW_H__
#define __OFA_RATE_TREEVIEW_H__

/**
 * SECTION: ofa_rate_treeview
 * @short_description: #ofaRateTreeview class definition.
 * @include: ui/ofa-rate-treeview.h
 *
 * Manage a treeview with the list of the rates.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+----------+
 *    | Signal           | Rate may |
 *    |                  | be %NULL |
 *    +------------------+----------+
 *    | ofa-ratchanged   |   Yes    |
 *    | ofa-ratactivated |    No    |
 *    | ofa-ratdelete    |    No    |
 *    +------------------+----------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-rate-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RATE_TREEVIEW                ( ofa_rate_treeview_get_type())
#define OFA_RATE_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RATE_TREEVIEW, ofaRateTreeview ))
#define OFA_RATE_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_RATE_CAST( klass, OFA_TYPE_RATE_TREEVIEW, ofaRateTreeviewRate ))
#define OFA_IS_RATE_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RATE_TREEVIEW ))
#define OFA_IS_RATE_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_RATE_TYPE(( klass ), OFA_TYPE_RATE_TREEVIEW ))
#define OFA_RATE_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RATE_TREEVIEW, ofaRateTreeviewRate ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaRateTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaRateTreeviewClass;

GType            ofa_rate_treeview_get_type     ( void ) G_GNUC_CONST;

ofaRateTreeview *ofa_rate_treeview_new          ( ofaIGetter *getter,
														const gchar *settings_prefix );

void             ofa_rate_treeview_setup_store  ( ofaRateTreeview *view );

ofoRate         *ofa_rate_treeview_get_selected ( ofaRateTreeview *view );

G_END_DECLS

#endif /* __OFA_RATE_TREEVIEW_H__ */
