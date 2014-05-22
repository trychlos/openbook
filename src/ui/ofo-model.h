/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OFO_MODEL_H__
#define __OFO_MODEL_H__

/**
 * SECTION: ofo_model
 * @short_description: #ofoModel class definition.
 * @include: ui/ofo-model.h
 *
 * This class implements the Model behavior
 */

#include "ui/ofo-base.h"
#include "ui/ofo-sgbd.h"

G_BEGIN_DECLS

#define OFO_TYPE_MODEL                ( ofo_model_get_type())
#define OFO_MODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_MODEL, ofoModel ))
#define OFO_MODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_MODEL, ofoModelClass ))
#define OFO_IS_MODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_MODEL ))
#define OFO_IS_MODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_MODEL ))
#define OFO_MODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_MODEL, ofoModelClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoModelClass;

typedef struct _ofoModelPrivate       ofoModelPrivate;

typedef struct {
	/*< private >*/
	ofoBase          parent;
	ofoModelPrivate *priv;
}
	ofoModel;

GType        ofo_model_get_type     ( void ) G_GNUC_CONST;

ofoModel    *ofo_model_new          ( void );

GList       *ofo_model_load_set     ( ofoSgbd *sgbd );
void         ofo_model_dump_set     ( GList *chart );

gint         ofo_model_get_id            ( const ofoModel *model );
const gchar *ofo_model_get_mnemo         ( const ofoModel *model );
const gchar *ofo_model_get_label         ( const ofoModel *model );
gint         ofo_model_get_journal       ( const ofoModel *model );
gboolean     ofo_model_get_journal_locked( const ofoModel *model );
const gchar *ofo_model_get_notes         ( const ofoModel *model );

void         ofo_model_set_id            ( ofoModel *model, gint id );
void         ofo_model_set_mnemo         ( ofoModel *model, const gchar *mnemo );
void         ofo_model_set_label         ( ofoModel *model, const gchar *label );
void         ofo_model_set_journal       ( ofoModel *model, gint journal );
void         ofo_model_set_journal_locked( ofoModel *model, gboolean journal_locked );
void         ofo_model_set_notes         ( ofoModel *model, const gchar *notes );
void         ofo_model_set_maj_user      ( ofoModel *model, const gchar *user );
void         ofo_model_set_maj_stamp     ( ofoModel *model, const GTimeVal *stamp );

gint         ofo_model_get_detail_count         ( const ofoModel *model );
const gchar *ofo_model_get_detail_comment       ( const ofoModel *model, gint idx );
const gchar *ofo_model_get_detail_account       ( const ofoModel *model, gint idx );
gboolean     ofo_model_get_detail_account_locked( const ofoModel *model, gint idx );
const gchar *ofo_model_get_detail_label         ( const ofoModel *model, gint idx );
gboolean     ofo_model_get_detail_label_locked  ( const ofoModel *model, gint idx );
const gchar *ofo_model_get_detail_debit         ( const ofoModel *model, gint idx );
gboolean     ofo_model_get_detail_debit_locked  ( const ofoModel *model, gint idx );
const gchar *ofo_model_get_detail_credit        ( const ofoModel *model, gint idx );
gboolean     ofo_model_get_detail_credit_locked ( const ofoModel *model, gint idx );

gboolean     ofo_model_detail_is_formula        ( const gchar *str );

void         ofo_model_set_detail   ( const ofoModel *model, gint idx,
										const gchar *comment,
										const gchar *account, gboolean account_locked,
										const gchar *label, gboolean label_locked,
										const gchar *debit, gboolean debit_locked,
										const gchar *credit, gboolean credit_locked );

gboolean     ofo_model_insert       ( ofoModel *model, ofoSgbd *sgbd, const gchar *user );
gboolean     ofo_model_update       ( ofoModel *model, ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo );
gboolean     ofo_model_delete       ( ofoModel *model, ofoSgbd *sgbd, const gchar *user );

G_END_DECLS

#endif /* __OFO_MODEL_H__ */
