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

#ifndef __OFA_REC_PERIOD_TREEVIEW_H__
#define __OFA_REC_PERIOD_TREEVIEW_H__

/**
 * SECTION: ofa_rec_period_treeview
 * @short_description: #ofaRecPeriodTreeview class definition.
 * @include: recurrent/ofa-rec-period-treeview.h
 *
 * Manage a treeview with the list of the periodicities.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+--------------+
 *    | Signal           | Periodicity  |
 *    |                  | may be %NULL |
 *    +------------------+--------------+
 *    | ofa-perchanged   |      Yes     |
 *    | ofa-peractivated |       No     |
 *    | ofa-perdelete    |       No     |
 *    +------------------+--------------+
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-tvbin.h"

#include "recurrent/ofo-rec-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_REC_PERIOD_TREEVIEW                ( ofa_rec_period_treeview_get_type())
#define OFA_REC_PERIOD_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_REC_PERIOD_TREEVIEW, ofaRecPeriodTreeview ))
#define OFA_REC_PERIOD_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_REC_PERIOD_TREEVIEW, ofaRecPeriodTreeviewClass ))
#define OFA_IS_REC_PERIOD_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_REC_PERIOD_TREEVIEW ))
#define OFA_IS_REC_PERIOD_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_REC_PERIOD_TREEVIEW ))
#define OFA_REC_PERIOD_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_REC_PERIOD_TREEVIEW, ofaRecPeriodTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaRecPeriodTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaRecPeriodTreeviewClass;

GType                 ofa_rec_period_treeview_get_type        ( void ) G_GNUC_CONST;

ofaRecPeriodTreeview *ofa_rec_period_treeview_new             ( ofaHub *hub );

void                  ofa_rec_period_treeview_set_settings_key( ofaRecPeriodTreeview *view,
																			const gchar *settings_key );

void                  ofa_rec_period_treeview_setup_columns   ( ofaRecPeriodTreeview *view );

ofoRecPeriod         *ofa_rec_period_treeview_get_selected    ( ofaRecPeriodTreeview *view );

G_END_DECLS

#endif /* __OFA_REC_PERIOD_TREEVIEW_H__ */
