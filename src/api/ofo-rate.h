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

#ifndef __OPENBOOK_API_OFO_RATE_H__
#define __OPENBOOK_API_OFO_RATE_H__

/**
 * SECTION: oforate
 * @title: ofoRate
 * @short_description: #ofoRate class definition.
 * @include: openbook/ofo-rate.h
 *
 * This file defines the #ofoRate class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-rate-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_RATE                ( ofo_rate_get_type())
#define OFO_RATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_RATE, ofoRate ))
#define OFO_RATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_RATE, ofoRateClass ))
#define OFO_IS_RATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_RATE ))
#define OFO_IS_RATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_RATE ))
#define OFO_RATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_RATE, ofoRateClass ))

#if 0
typedef struct _ofoRate              ofoRate;
#endif

struct _ofoRate {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoRateClass;

/**
 * ofsRateValidity:
 *
 * The structure used to validate all the validities of a rate.
 */
typedef struct {
	GDate     begin;					/* invalid date is infinite in the past */
	GDate     end;						/* invalid date is infinite in the future */
	ofxAmount rate;
}
	ofsRateValidity;

GType           ofo_rate_get_type          ( void ) G_GNUC_CONST;

GList          *ofo_rate_get_dataset       ( ofaIGetter *getter );
#define         ofo_rate_free_dataset( L ) g_list_free_full(( L ),( GDestroyNotify ) g_object_unref )

ofoRate        *ofo_rate_get_by_mnemo      ( ofaIGetter *getter, const gchar *mnemo );

ofoRate        *ofo_rate_new               ( ofaIGetter *getter );

const gchar    *ofo_rate_get_mnemo         ( const ofoRate *rate );
const gchar    *ofo_rate_get_label         ( const ofoRate *rate );
const gchar    *ofo_rate_get_notes         ( const ofoRate *rate );
const gchar    *ofo_rate_get_upd_user      ( const ofoRate *rate );
const GTimeVal *ofo_rate_get_upd_stamp     ( const ofoRate *rate );

const GDate    *ofo_rate_get_min_valid     ( ofoRate *rate );
const GDate    *ofo_rate_get_max_valid     ( ofoRate *rate );
gint            ofo_rate_get_val_count     ( ofoRate *rate );
const GDate    *ofo_rate_get_val_begin     ( ofoRate *rate, gint idx );
const GDate    *ofo_rate_get_val_end       ( ofoRate *rate, gint idx );
ofxAmount       ofo_rate_get_val_rate      ( ofoRate *rate, gint idx );
ofxAmount       ofo_rate_get_rate_at_date  ( ofoRate *rate, const GDate *date );

gboolean        ofo_rate_is_deletable      ( const ofoRate *rate );
gboolean        ofo_rate_is_valid_data     ( const gchar *mnemo, const gchar *label, GList *validities, gchar **msgerr );

void            ofo_rate_set_mnemo         ( ofoRate *rate, const gchar *number );
void            ofo_rate_set_label         ( ofoRate *rate, const gchar *label );
void            ofo_rate_set_notes         ( ofoRate *rate, const gchar *notes );
void            ofo_rate_free_all_val      ( ofoRate *rate );
void            ofo_rate_add_val           ( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value );

gboolean        ofo_rate_insert            ( ofoRate *rate );
gboolean        ofo_rate_update            ( ofoRate *rate, const gchar *prev_mnemo );
gboolean        ofo_rate_delete            ( ofoRate *rate );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_RATE_H__ */
