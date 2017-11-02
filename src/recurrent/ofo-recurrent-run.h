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

#ifndef __OFO_RECURRENT_RUN_H__
#define __OFO_RECURRENT_RUN_H__

/**
 * SECTION: ofo_recurrent_run
 * @short_description: #ofoRecurrentRun class definition.
 * @include: recurrent/ofo-recurrent-run.h
 *
 * This file defines the #ofoRecurrentRun class behavior.
 *
 * An #ofoRecurrentRun describes an operation generated from a recurrent
 * operation template.
 */

#include "my/my-period.h"

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"

#include "recurrent/ofo-recurrent-model.h"

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
 * ofeRecurrentStatus:
 *
 * @REC_STATUS_CANCELLED: the operation has been cancelled;
 *  may only be changed back to waiting.
 *
 * @REC_STATUS_WAITING  : the operation is waiting for a status change,
 * either to be cancelled or to be validated.
 *
 * @REC_STATUS_VALIDATED: the operation has been sent to the accounting.
 */
typedef enum {
	REC_STATUS_CANCELLED = 1 << 1,
	REC_STATUS_WAITING   = 1 << 2,
	REC_STATUS_VALIDATED = 1 << 3
}
	ofeRecurrentStatus;

GType              ofo_recurrent_run_get_type              ( void ) G_GNUC_CONST;

GList             *ofo_recurrent_run_get_dataset           ( ofaIGetter *getter );

ofoRecurrentRun   *ofo_recurrent_run_get_by_id             ( ofaIGetter *getter, const gchar *mnemo, const GDate *date );

ofoRecurrentRun   *ofo_recurrent_run_new                   ( ofoRecurrentModel *model );

ofxCounter         ofo_recurrent_run_get_numseq            ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_mnemo             ( const ofoRecurrentRun *model );
const GDate       *ofo_recurrent_run_get_date              ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_label             ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_ope_template      ( const ofoRecurrentRun *model );
myPeriod          *ofo_recurrent_run_get_period            ( ofoRecurrentRun *model );
const GDate       *ofo_recurrent_run_get_end               ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_cre_user          ( const ofoRecurrentRun *model );
const GTimeVal    *ofo_recurrent_run_get_cre_stamp         ( const ofoRecurrentRun *model );
ofeRecurrentStatus ofo_recurrent_run_get_status            ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_status_get_dbms       ( ofeRecurrentStatus status );
const gchar       *ofo_recurrent_run_status_get_abr        ( ofeRecurrentStatus status );
const gchar       *ofo_recurrent_run_status_get_label      ( ofeRecurrentStatus status );
const gchar       *ofo_recurrent_run_get_sta_user          ( const ofoRecurrentRun *model );
const GTimeVal    *ofo_recurrent_run_get_sta_stamp         ( const ofoRecurrentRun *model );
ofxAmount          ofo_recurrent_run_get_amount1           ( const ofoRecurrentRun *model );
ofxAmount          ofo_recurrent_run_get_amount2           ( const ofoRecurrentRun *model );
ofxAmount          ofo_recurrent_run_get_amount3           ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_edi_user          ( const ofoRecurrentRun *model );
const GTimeVal    *ofo_recurrent_run_get_edi_stamp         ( const ofoRecurrentRun *model );

gint               ofo_recurrent_run_compare               ( const ofoRecurrentRun *a, const ofoRecurrentRun *b );

const GDate       *ofo_recurrent_run_get_last              ( ofaIGetter *getter, GDate *dlast, const gchar *mnemo, guint status );

void               ofo_recurrent_run_set_mnemo             ( ofoRecurrentRun *model, const gchar *mnemo );
void               ofo_recurrent_run_set_date              ( ofoRecurrentRun *model, const GDate *date );
void               ofo_recurrent_run_set_status            ( ofoRecurrentRun *model, ofeRecurrentStatus status );
void               ofo_recurrent_run_set_amount1           ( ofoRecurrentRun *model, ofxAmount amount );
void               ofo_recurrent_run_set_amount2           ( ofoRecurrentRun *model, ofxAmount amount );
void               ofo_recurrent_run_set_amount3           ( ofoRecurrentRun *model, ofxAmount amount );

GList             *ofo_recurrent_run_get_doc_orphans       ( ofaIGetter *getter );
#define            ofo_recurrent_run_free_doc_orphans( L ) ( g_list_free( L ))

gboolean           ofo_recurrent_run_insert                ( ofoRecurrentRun *model );
gboolean           ofo_recurrent_run_update_status         ( ofoRecurrentRun *model );
gboolean           ofo_recurrent_run_update_amounts        ( ofoRecurrentRun *model );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_RECURRENT_RUN_H__ */
