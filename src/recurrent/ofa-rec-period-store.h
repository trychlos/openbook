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

#ifndef __OFA_REC_PERIOD_STORE_H__
#define __OFA_REC_PERIOD_STORE_H__

/**
 * SECTION: rec_period_store
 * @title: ofaRecPeriodStore
 * @short_description: The RecPeriodStore class definition
 * @include: recurrent/ofa-rec-period-store.h
 *
 * A #ofaListStore -derived class which handles the periodicities.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_REC_PERIOD_STORE                ( ofa_rec_period_store_get_type())
#define OFA_REC_PERIOD_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_REC_PERIOD_STORE, ofaRecPeriodStore ))
#define OFA_REC_PERIOD_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_REC_PERIOD_STORE, ofaRecPeriodStoreClass ))
#define OFA_IS_REC_PERIOD_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_REC_PERIOD_STORE ))
#define OFA_IS_REC_PERIOD_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_REC_PERIOD_STORE ))
#define OFA_REC_PERIOD_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_REC_PERIOD_STORE, ofaRecPeriodStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaRecPeriodStore;

/**
 * ofaRecPeriodStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaRecPeriodStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                             Type     Displayable
 *                                                             -------  -----------
 * @PER_COL_ID              : identifier                       String       Yes
 * @PER_COL_ORDER           : order                            String       Yes
 * @PER_COL_ORDER_I         : order                            Int           No
 * @PER_COL_LABEL           : label                            String       Yes
 * @PER_COL_DETAILS_COUNT   : count of detail types            String       Yes
 * @PER_COL_DETAILS_COUNT_I : count of detail types            Int           No
 * @PER_COL_NOTES           : notes                            String       Yes
 * @PER_COL_NOTES_PNG       : notes indicator                  Pixbuf       Yes
 * @PER_COL_UPD_USER        : last update user                 String       Yes
 * @PER_COL_UPD_STAMP       : last update timestamp            String       Yes
 * @REC_RUN_COL_OBJECT      : the #ofoRecPeriod object         GObject       No
 */
enum {
	PER_COL_ID= 0,
	PER_COL_ORDER,
	PER_COL_ORDER_I,
	PER_COL_LABEL,
	PER_COL_DETAILS_COUNT,
	PER_COL_DETAILS_COUNT_I,
	PER_COL_NOTES,
	PER_COL_NOTES_PNG,
	PER_COL_UPD_USER,
	PER_COL_UPD_STAMP,
	PER_COL_OBJECT,
	PER_N_COLUMNS
};

GType              ofa_rec_period_store_get_type( void );

ofaRecPeriodStore *ofa_rec_period_store_new     ( ofaHub *hub );

G_END_DECLS

#endif /* __OFA_REC_PERIOD_STORE_H__ */
