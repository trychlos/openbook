/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFO_DATA_H__
#define __OPENBOOK_API_OFO_DATA_H__

/**
 * SECTION: ofodata
 * @title: ofoData
 * @short_description: #ofoData class definition.
 * @include: openbook/ofo-data.h
 *
 * This file defines the #ofoData class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-data-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_DATA                ( ofo_data_get_type())
#define OFO_DATA( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_DATA, ofoData ))
#define OFO_DATA_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_DATA, ofoDataClass ))
#define OFO_IS_DATA( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_DATA ))
#define OFO_IS_DATA_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_DATA ))
#define OFO_DATA_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_DATA, ofoDataClass ))

#if 0
typedef struct _ofoData              ofoData;
#endif

struct _ofoData {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoDataClass;

GType             ofo_data_get_type          ( void ) G_GNUC_CONST;

GList            *ofo_data_get_dataset       ( ofaIGetter *getter );
#define           ofo_data_free_dataset( L ) ( g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref ))

ofoData          *ofo_data_get_by_key        ( ofaIGetter *getter, const gchar *mnemo );

ofoData          *ofo_data_new               ( ofaIGetter *getter );

const gchar      *ofo_data_get_key           ( const ofoData *data );
const gchar      *ofo_data_get_cre_user      ( const ofoData *data );
const myStampVal *ofo_data_get_cre_stamp     ( const ofoData *data );
const gchar      *ofo_data_get_content       ( const ofoData *data );
const gchar      *ofo_data_get_notes         ( const ofoData *data );
const gchar      *ofo_data_get_upd_user      ( const ofoData *data );
const myStampVal *ofo_data_get_upd_stamp     ( const ofoData *data );

void              ofo_data_set_key           ( ofoData *data, const gchar *key );
void              ofo_data_set_content       ( ofoData *data, const gchar *content );
void              ofo_data_set_notes         ( ofoData *data, const gchar *notes );

gboolean          ofo_data_insert            ( ofoData *data );
gboolean          ofo_data_update            ( ofoData *data, const gchar *prev_key );
gboolean          ofo_data_delete            ( ofoData *data );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_DATA_H__ */
