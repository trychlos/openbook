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

#ifndef __OPENBOOK_API_OFO_COUNTER_H__
#define __OPENBOOK_API_OFO_COUNTER_H__

/**
 * SECTION: ofocounter
 * @title: ofoCounter
 * @short_description: #ofoCounter class definition.
 * @include: openbook/ofo-dossier-ids.h
 *
 * This file defines the #ofoCounter public API.
 *
 * The #ofoCounter class handles the last identifiers used by all
 * other tables of the DBMS.
 *
 * The class is implemented through a singleton which is maintained at
 * application level, and available through the #ofoIGetter of the
 * application.
 *
 * Note that despite its nomenclature, this class is NOT derived from
 * #ofoBase.
 */

#include <glib-object.h>

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-counter-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_COUNTER                ( ofo_counter_get_type())
#define OFO_COUNTER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_COUNTER, ofoCounter ))
#define OFO_COUNTER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_COUNTER, ofoCounterClass ))
#define OFO_IS_COUNTER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_COUNTER ))
#define OFO_IS_COUNTER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_COUNTER ))
#define OFO_COUNTER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_COUNTER, ofoCounterClass ))

#if 0
typedef struct _ofoCounter               ofoCounter;
#endif

struct _ofoCounter {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofoCounterClass;

GType       ofo_counter_get_type              ( void ) G_GNUC_CONST;

ofoCounter *ofo_counter_new                   ( ofaIGetter *getter );

ofxCounter  ofo_counter_get_last_bat_id       ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_bat_id       ( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_batline_id   ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_batline_id   ( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_concil_id    ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_concil_id    ( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_doc_id       ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_doc_id       ( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_entry_id     ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_entry_id     ( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_ope_id       ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_ope_id       ( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_settlement_id( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_settlement_id( ofoCounter *counters );

ofxCounter  ofo_counter_get_last_tiers_id     ( ofoCounter *counters );
ofxCounter  ofo_counter_get_next_tiers_id     ( ofoCounter *counters );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_COUNTER_H__ */
