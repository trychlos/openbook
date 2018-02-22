/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RECURRENT_DBMODEL_H__
#define __OFA_RECURRENT_DBMODEL_H__

/**
 * SECTION: ofa_recurrent_dbmodel
 * @short_description: #ofaRecurrentDBModel definition.
 *
 * #ofaIDBModel interface management.
 *
 * The #ofaIDBModel may be easily directly implemeted by the
 * #ofaRecurrentId class, which is naturally instanciated by
 * Openbook mechanisms.
 *
 * Having a dedicated class for the #ofaIDBModel implementation
 * let us display a dedicated version number in the Openbook
 * plugin's management interface.
 */

#include "api/ofa-idbmodel.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_DBMODEL                ( ofa_recurrent_dbmodel_get_type())
#define OFA_RECURRENT_DBMODEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_DBMODEL, ofaRecurrentDBModel ))
#define OFA_RECURRENT_DBMODEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_DBMODEL, ofaRecurrentDBModelClass ))
#define OFA_IS_RECURRENT_DBMODEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_DBMODEL ))
#define OFA_IS_RECURRENT_DBMODEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_DBMODEL ))
#define OFA_RECURRENT_DBMODEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_DBMODEL, ofaRecurrentDBModelClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaRecurrentDBModel;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaRecurrentDBModelClass;

GType ofa_recurrent_dbmodel_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_RECURRENT_DBMODEL_H__ */
