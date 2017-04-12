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

#ifndef __OPENBOOK_API_OFO_COUNTERS_H__
#define __OPENBOOK_API_OFO_COUNTERS_H__

/**
 * SECTION: ofocounters
 * @title: ofoCounters
 * @short_description: #ofoCounters class definition.
 * @include: openbook/ofo-dossier-ids.h
 *
 * This file defines the #ofoCounters public API.
 *
 * The #ofoCounters class handles the last identifiers used by all
 * other tables of the DBMS.
 *
 * The class is implemented through a singleton which is maintained at
 * application level, and available through the #ofaHub of the
 * application.
 *
 * Note that despite its nomenclature, this class is NOT derived from
 * #ofoBase.
 */

#include <glib-object.h>

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-counters-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_COUNTERS                ( ofo_counters_get_type())
#define OFO_COUNTERS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_COUNTERS, ofoCounters ))
#define OFO_COUNTERS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_COUNTERS, ofoCountersClass ))
#define OFO_IS_COUNTERS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_COUNTERS ))
#define OFO_IS_COUNTERS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_COUNTERS ))
#define OFO_COUNTERS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_COUNTERS, ofoCountersClass ))

#if 0
typedef struct _ofoCounters               ofoCounters;
#endif

struct _ofoCounters {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofoCountersClass;

GType        ofo_counters_get_type              ( void ) G_GNUC_CONST;

ofoCounters *ofo_counters_new                   ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_bat_id       ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_bat_id       ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_batline_id   ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_batline_id   ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_concil_id    ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_concil_id    ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_doc_id       ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_doc_id       ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_entry_id     ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_entry_id     ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_ope_id       ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_ope_id       ( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_settlement_id( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_settlement_id( ofaIGetter *getter );

ofxCounter   ofo_counters_get_last_tiers_id     ( ofaIGetter *getter );
ofxCounter   ofo_counters_get_next_tiers_id     ( ofaIGetter *getter );

guint        ofo_counters_get_count             ( void );
const gchar *ofo_counters_get_key               ( ofaIGetter *getter, guint idx );
ofxCounter   ofo_counters_get_last_value        ( ofaIGetter *getter, guint idx );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_COUNTERS_H__ */
