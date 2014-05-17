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

#include "ui/ofa-sgbd.h"
#include "ui/ofo-base.h"

G_BEGIN_DECLS

#define OFO_TYPE_MODEL                ( ofo_model_get_type())
#define OFO_MODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_MODEL, ofoModel ))
#define OFO_MODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_MODEL, ofoModelClass ))
#define OFO_IS_MODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_MODEL ))
#define OFO_IS_MODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_MODEL ))
#define OFO_MODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_MODEL, ofoModelClass ))

typedef struct _ofoModelPrivate       ofoModelPrivate;

typedef struct {
	/*< private >*/
	ofoBase          parent;
	ofoModelPrivate *private;
}
	ofoModel;

typedef struct _ofoModelClassPrivate  ofoModelClassPrivate;

typedef struct {
	/*< private >*/
	ofoBaseClass          parent;
	ofoModelClassPrivate *private;
}
	ofoModelClass;

GType        ofo_model_get_type     ( void );

ofoModel    *ofo_model_new          ( void );

GList       *ofo_model_load_set     ( ofaSgbd *sgbd );
void         ofo_model_dump_set     ( GList *chart );

gint         ofo_model_get_id       ( const ofoModel *model );
const gchar *ofo_model_get_mnemo    ( const ofoModel *model );
const gchar *ofo_model_get_label    ( const ofoModel *model );
gint         ofo_model_get_family   ( const ofoModel *model );
const gchar *ofo_model_get_journal  ( const ofoModel *model );
const gchar *ofo_model_get_notes    ( const ofoModel *model );

gint         ofo_model_get_count    ( const ofoModel *model );

void         ofo_model_set_id       ( ofoModel *model, gint id );
void         ofo_model_set_mnemo    ( ofoModel *model, const gchar *mnemo );
void         ofo_model_set_label    ( ofoModel *model, const gchar *label );
void         ofo_model_set_family   ( ofoModel *model, gint family );
void         ofo_model_set_journal  ( ofoModel *model, const gchar *journal );
void         ofo_model_set_notes    ( ofoModel *model, const gchar *notes );
void         ofo_model_set_maj_user ( ofoModel *model, const gchar *user );
void         ofo_model_set_maj_stamp( ofoModel *model, const GTimeVal *stamp );

gboolean     ofo_model_insert       ( ofoModel *model, ofaSgbd *sgbd, const gchar *user );
gboolean     ofo_model_update       ( ofoModel *model, ofaSgbd *sgbd, const gchar *user, const gchar *prev_mnemo );
gboolean     ofo_model_delete       ( ofoModel *model, ofaSgbd *sgbd, const gchar *user );

G_END_DECLS

#endif /* __OFO_MODEL_H__ */
