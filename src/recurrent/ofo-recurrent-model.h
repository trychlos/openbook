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

#ifndef __OFO_RECURRENT_MODEL_H__
#define __OFO_RECURRENT_MODEL_H__

/**
 * SECTION: ofo_recurrent_model
 * @short_description: #ofoRecurrentModel class definition.
 * @include: recurrent/ofo-recurrent-model.h
 *
 * This file defines the #ofoRecurrentModel class behavior.
 *
 * An #ofoRecurrentModel describes a recurrent operation template.
 */

#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"

#include "recurrent/ofo-rec-period.h"

G_BEGIN_DECLS

#define OFO_TYPE_RECURRENT_MODEL                ( ofo_recurrent_model_get_type())
#define OFO_RECURRENT_MODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_RECURRENT_MODEL, ofoRecurrentModel ))
#define OFO_RECURRENT_MODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_RECURRENT_MODEL, ofoRecurrentModelClass ))
#define OFO_IS_RECURRENT_MODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_RECURRENT_MODEL ))
#define OFO_IS_RECURRENT_MODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_RECURRENT_MODEL ))
#define OFO_RECURRENT_MODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_RECURRENT_MODEL, ofoRecurrentModelClass ))

typedef struct {
	/*< public members >*/
	ofoBase      parent;
}
	ofoRecurrentModel;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoRecurrentModelClass;

/* data max length */
#define RECM_MNEMO_MAX_LENGTH           64

GType              ofo_recurrent_model_get_type               ( void ) G_GNUC_CONST;

GList             *ofo_recurrent_model_get_dataset            ( ofaHub *hub );

ofoRecurrentModel *ofo_recurrent_model_get_by_mnemo           ( ofaHub *hub, const gchar *mnemo );

gboolean           ofo_recurrent_model_use_ope_template       ( ofaHub *hub, const gchar *ope_template );

ofoRecurrentModel *ofo_recurrent_model_new                    ( void );
ofoRecurrentModel *ofo_recurrent_model_new_from_model         ( const ofoRecurrentModel *model );

const gchar       *ofo_recurrent_model_get_mnemo              ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_label              ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_ope_template       ( const ofoRecurrentModel *model );
ofxCounter         ofo_recurrent_model_get_periodicity        ( const ofoRecurrentModel *model );
ofxCounter         ofo_recurrent_model_get_periodicity_detail ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_notes              ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_upd_user           ( const ofoRecurrentModel *model );
const GTimeVal    *ofo_recurrent_model_get_upd_stamp          ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_def_amount1        ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_def_amount2        ( const ofoRecurrentModel *model );
const gchar       *ofo_recurrent_model_get_def_amount3        ( const ofoRecurrentModel *model );
gboolean           ofo_recurrent_model_get_is_enabled         ( const ofoRecurrentModel *model );

gboolean           ofo_recurrent_model_is_deletable           ( const ofoRecurrentModel *model );
gboolean           ofo_recurrent_model_is_valid_data          ( const gchar *mnemo, const gchar *label, const gchar *ope_template, ofoRecPeriod *period, ofxCounter detail, gchar **msgerr );

void               ofo_recurrent_model_set_mnemo              ( ofoRecurrentModel *model, const gchar *mnemo );
void               ofo_recurrent_model_set_label              ( ofoRecurrentModel *model, const gchar *label );
void               ofo_recurrent_model_set_ope_template       ( ofoRecurrentModel *model, const gchar *ope_template );
void               ofo_recurrent_model_set_periodicity        ( ofoRecurrentModel *model, ofxCounter periodicity );
void               ofo_recurrent_model_set_periodicity_detail ( ofoRecurrentModel *model, ofxCounter detail );
void               ofo_recurrent_model_set_notes              ( ofoRecurrentModel *model, const gchar *notes );
void               ofo_recurrent_model_set_def_amount1        ( ofoRecurrentModel *model, const gchar *def_amount );
void               ofo_recurrent_model_set_def_amount2        ( ofoRecurrentModel *model, const gchar *def_amount );
void               ofo_recurrent_model_set_def_amount3        ( ofoRecurrentModel *model, const gchar *def_amount );
void               ofo_recurrent_model_set_is_enabled         ( ofoRecurrentModel *model, gboolean is_enabled );

gboolean           ofo_recurrent_model_insert                 ( ofoRecurrentModel *model, ofaHub *hub );
gboolean           ofo_recurrent_model_update                 ( ofoRecurrentModel *model, const gchar *prev_mnemo );
gboolean           ofo_recurrent_model_delete                 ( ofoRecurrentModel *model );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_RECURRENT_MODEL_H__ */
