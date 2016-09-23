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

#ifndef __OFO_RECURRENT_RUN_H__
#define __OFO_RECURRENT_RUN_H__

/**
 * SECTION: ofo_recurrent_run
 * @short_description: #ofoRecurrentRun class definition.
 * @include: recurrent/ofo-recurrent-form.h
 *
 * This file defines the #ofoRecurrentRun class behavior.
 *
 * An #ofoRecurrentRun describes an operation generated from a recurrent
 * operation template.
 */

#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_RECURRENT_RUN                ( ofo_recurrent_run_get_type())
#define OFO_RECURRENT_RUN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_RECURRENT_RUN, ofoRecurrentRun ))
#define OFO_RECURRENT_RUN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_RECURRENT_RUN, ofoRecurrentRunClass ))
#define OFO_IS_RECURRENT_RUN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_RECURRENT_RUN ))
#define OFO_IS_RECURRENT_RUN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_RECURRENT_RUN ))
#define OFO_RECURRENT_RUN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_RECURRENT_RUN, ofoRecurrentRunClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoRecurrentRun;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoRecurrentRunClass;

/**
 * Status:
 */
#define REC_STATUS_CANCELLED            "C"
#define REC_STATUS_WAITING              "W"
#define REC_STATUS_VALIDATED            "V"

GType            ofo_recurrent_run_get_type        ( void ) G_GNUC_CONST;

GList           *ofo_recurrent_run_get_dataset     ( ofaHub *hub );

ofoRecurrentRun *ofo_recurrent_run_get_by_id       ( ofaHub *hub, const gchar *mnemo, const GDate *date );

ofoRecurrentRun *ofo_recurrent_run_new             ( void );

const gchar     *ofo_recurrent_run_get_mnemo       ( const ofoRecurrentRun *model );
const GDate     *ofo_recurrent_run_get_date        ( const ofoRecurrentRun *model );
const gchar     *ofo_recurrent_run_get_status      ( const ofoRecurrentRun *model );
gchar           *ofo_recurrent_run_get_status_label( const gchar *status );
const gchar     *ofo_recurrent_run_get_upd_user    ( const ofoRecurrentRun *model );
const GTimeVal  *ofo_recurrent_run_get_upd_stamp   ( const ofoRecurrentRun *model );
gdouble          ofo_recurrent_run_get_amount1     ( const ofoRecurrentRun *model );
gdouble          ofo_recurrent_run_get_amount2     ( const ofoRecurrentRun *model );
gdouble          ofo_recurrent_run_get_amount3     ( const ofoRecurrentRun *model );
ofxCounter       ofo_recurrent_run_get_numseq      ( const ofoRecurrentRun *model );

gint             ofo_recurrent_run_compare         ( const ofoRecurrentRun *a, const ofoRecurrentRun *b );

void             ofo_recurrent_run_set_mnemo       ( ofoRecurrentRun *model, const gchar *mnemo );
void             ofo_recurrent_run_set_date        ( ofoRecurrentRun *model, const GDate *date );
void             ofo_recurrent_run_set_status      ( ofoRecurrentRun *model, const gchar *status );
void             ofo_recurrent_run_set_amount1     ( ofoRecurrentRun *model, gdouble amount );
void             ofo_recurrent_run_set_amount2     ( ofoRecurrentRun *model, gdouble amount );
void             ofo_recurrent_run_set_amount3     ( ofoRecurrentRun *model, gdouble amount );

gboolean         ofo_recurrent_run_insert          ( ofoRecurrentRun *model, ofaHub *hub );
gboolean         ofo_recurrent_run_update          ( ofoRecurrentRun *model );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_RECURRENT_RUN_H__ */
