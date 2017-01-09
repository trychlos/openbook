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

#ifndef __OFO_REC_PERIOD_H__
#define __OFO_REC_PERIOD_H__

/**
 * SECTION: ofo_rec_period
 * @short_description: #ofoRecPeriod class definition.
 * @include: recurrent/ofo-rec-period.h
 *
 * This file defines the #ofoRecPeriod class behavior.
 *
 * An #ofoRecPeriod describes a periodicity.
 */

#include "api/ofa-box.h"
#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_REC_PERIOD                ( ofo_rec_period_get_type())
#define OFO_REC_PERIOD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_REC_PERIOD, ofoRecPeriod ))
#define OFO_REC_PERIOD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_REC_PERIOD, ofoRecPeriodClass ))
#define OFO_IS_REC_PERIOD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_REC_PERIOD ))
#define OFO_IS_REC_PERIOD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_REC_PERIOD ))
#define OFO_REC_PERIOD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_REC_PERIOD, ofoRecPeriodClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoRecPeriod;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoRecPeriodClass;

/**
 * Periodicity identifier
 * This is an invariant which identifies the periodicity object.
 * This cannot be fully configurable as the #ofo_rec_period_enum_between()
 * method must know how to deal with each periodicity.
 */
#define REC_PERIOD_MONTHLY             "MONTHLY"
#define REC_PERIOD_NEVER               "NEVER"
#define REC_PERIOD_WEEKLY              "WEEKLY"

/*
 * widgets Tmax size
 */
#define REC_PERIOD_LABEL_MAX            256

/**
 * RecPeriodEnumBetweenCb:
 *
 * The #ofo_rec_period_enum_between() callback.
 */
typedef void (*RecPeriodEnumBetweenCb)( const GDate *date, void *user_data );

GType           ofo_rec_period_get_type          ( void ) G_GNUC_CONST;

GList          *ofo_rec_period_get_dataset       ( ofaHub *hub );
#define         ofo_rec_period_free_dataset( L ) g_list_free_full(( L ),( GDestroyNotify ) g_object_unref )

ofoRecPeriod   *ofo_rec_period_get_by_id         ( ofaHub *hub, const gchar *id );

ofoRecPeriod   *ofo_rec_period_new               ( void );

const gchar    *ofo_rec_period_get_id            ( ofoRecPeriod *period );
guint           ofo_rec_period_get_order         ( ofoRecPeriod *period );
const gchar    *ofo_rec_period_get_label         ( ofoRecPeriod *period );
guint           ofo_rec_period_get_details_count ( ofoRecPeriod *period );
const gchar    *ofo_rec_period_get_notes         ( ofoRecPeriod *period );
const gchar    *ofo_rec_period_get_upd_user      ( ofoRecPeriod *period );
const GTimeVal *ofo_rec_period_get_upd_stamp     ( ofoRecPeriod *period );

guint           ofo_rec_period_detail_get_count  ( ofoRecPeriod *period );
gint            ofo_rec_period_detail_get_by_id  ( ofoRecPeriod *period, ofxCounter det_id );
ofxCounter      ofo_rec_period_detail_get_id     ( ofoRecPeriod *period, guint idx );
guint           ofo_rec_period_detail_get_order  ( ofoRecPeriod *period, guint idx );
guint           ofo_rec_period_detail_get_number ( ofoRecPeriod *period, guint idx );
guint           ofo_rec_period_detail_get_value  ( ofoRecPeriod *period, guint idx );
const gchar    *ofo_rec_period_detail_get_label  ( ofoRecPeriod *period, guint idx );

gboolean        ofo_rec_period_is_valid_data     ( const gchar *label, gchar **msgerr );

gboolean        ofo_rec_period_is_deletable      ( ofoRecPeriod *period );

void            ofo_rec_period_enum_between      ( ofoRecPeriod *period,
														ofxCounter detail_id,
														const GDate *begin, const GDate *end,
														RecPeriodEnumBetweenCb cb, void *user_data );

void            ofo_rec_period_set_id            ( ofoRecPeriod *period, const gchar *id );
void            ofo_rec_period_set_order         ( ofoRecPeriod *period, guint order );
void            ofo_rec_period_set_label         ( ofoRecPeriod *period, const gchar *label );
void            ofo_rec_period_set_details_count ( ofoRecPeriod *period, guint count );
void            ofo_rec_period_set_notes         ( ofoRecPeriod *period, const gchar *notes );

void            ofo_rec_period_free_detail_all   ( ofoRecPeriod *period );
void            ofo_rec_period_add_detail        ( ofoRecPeriod *period, guint order, const gchar *label, guint number, guint value );

gboolean        ofo_rec_period_insert            ( ofoRecPeriod *period, ofaHub *hub );
gboolean        ofo_rec_period_update            ( ofoRecPeriod *period );
gboolean        ofo_rec_period_delete            ( ofoRecPeriod *period );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_REC_PERIOD_H__ */
