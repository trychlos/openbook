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

#ifndef __OPENBOOK_API_OFO_PAIMEANS_H__
#define __OPENBOOK_API_OFO_PAIMEANS_H__

/**
 * SECTION: ofopaimeans
 * @title: ofoPaimeans
 * @short_description: #ofoPaimeans class definition.
 * @include: openbook/ofo-paimeans.h
 *
 * This file defines the #ofoPaimeans class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_PAIMEANS                ( ofo_paimeans_get_type())
#define OFO_PAIMEANS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_PAIMEANS, ofoPaimeans ))
#define OFO_PAIMEANS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_PAIMEANS, ofoPaimeansClass ))
#define OFO_IS_PAIMEANS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_PAIMEANS ))
#define OFO_IS_PAIMEANS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_PAIMEANS ))
#define OFO_PAIMEANS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_PAIMEANS, ofoPaimeansClass ))

typedef struct _ofoPaimeans              ofoPaimeans;

struct _ofoPaimeans {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoPaimeansClass;

GType           ofo_paimeans_get_type          ( void ) G_GNUC_CONST;

GList          *ofo_paimeans_get_dataset       ( ofaHub *hub );
#define         ofo_paimeans_free_dataset( L ) g_list_free_full(( L ),( GDestroyNotify ) g_object_unref )

ofoPaimeans    *ofo_paimeans_get_by_code       ( ofaHub *hub, const gchar *code );

ofoPaimeans    *ofo_paimeans_new               ( void );

const gchar    *ofo_paimeans_get_code          ( const ofoPaimeans *paimeans );
const gchar    *ofo_paimeans_get_label         ( const ofoPaimeans *paimeans );
gboolean        ofo_paimeans_get_must_alone    ( const ofoPaimeans *paimeans );
const gchar    *ofo_paimeans_get_account       ( const ofoPaimeans *paimeans );
const gchar    *ofo_paimeans_get_notes         ( const ofoPaimeans *paimeans );
const gchar    *ofo_paimeans_get_upd_user      ( const ofoPaimeans *paimeans );
const GTimeVal *ofo_paimeans_get_upd_stamp     ( const ofoPaimeans *paimeans );

gboolean        ofo_paimeans_is_deletable      ( const ofoPaimeans *paimeans );
gboolean        ofo_paimeans_is_valid_data     ( const gchar *code, gchar **msgerr );

void            ofo_paimeans_set_code          ( ofoPaimeans *paimeans, const gchar *code );
void            ofo_paimeans_set_label         ( ofoPaimeans *paimeans, const gchar *label );
void            ofo_paimeans_set_must_alone    ( ofoPaimeans *paimeans, gboolean alone );
void            ofo_paimeans_set_account       ( ofoPaimeans *paimeans, const gchar *account );
void            ofo_paimeans_set_notes         ( ofoPaimeans *paimeans, const gchar *notes );

gboolean        ofo_paimeans_insert            ( ofoPaimeans *paimeans, ofaHub *hub );
gboolean        ofo_paimeans_update            ( ofoPaimeans *paimeans, const gchar *prev_code );
gboolean        ofo_paimeans_delete            ( ofoPaimeans *paimeans );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_PAIMEANS_H__ */
