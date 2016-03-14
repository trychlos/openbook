/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
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

typedef struct _ofoRecurrentRunPrivate        ofoRecurrentRunPrivate;

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

GType              ofo_recurrent_run_get_type     ( void ) G_GNUC_CONST;

GList             *ofo_recurrent_run_get_dataset  ( ofaHub *hub );

ofoRecurrentRun *ofo_recurrent_run_new            ( void );

const gchar       *ofo_recurrent_run_get_mnemo    ( const ofoRecurrentRun *model );
const GDate       *ofo_recurrent_run_get_date     ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_status   ( const ofoRecurrentRun *model );
const gchar       *ofo_recurrent_run_get_upd_user ( const ofoRecurrentRun *model );
const GTimeVal    *ofo_recurrent_run_get_upd_stamp( const ofoRecurrentRun *model );

void               ofo_recurrent_run_set_mnemo    ( ofoRecurrentRun *model, const gchar *mnemo );
void               ofo_recurrent_run_set_date     ( ofoRecurrentRun *model, const GDate *date );
void               ofo_recurrent_run_set_status   ( ofoRecurrentRun *model, const gchar *status );

gboolean           ofo_recurrent_run_insert       ( ofoRecurrentRun *model, ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_RECURRENT_RUN_H__ */
