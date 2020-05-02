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

#ifndef __OPENBOOK_API_OFO_TIERS_H__
#define __OPENBOOK_API_OFO_TIERS_H__

/**
 * SECTION: ofotiers
 * @title: ofoTiers
 * @short_description: #ofoTiers class definition.
 * @include: openbook/ofo-tiers.h
 *
 * This file defines the #ofoTiers class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_TIERS                ( ofo_tiers_get_type())
#define OFO_TIERS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TIERS, ofoTiers ))
#define OFO_TIERS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TIERS, ofoTiersClass ))
#define OFO_IS_TIERS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TIERS ))
#define OFO_IS_TIERS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TIERS ))
#define OFO_TIERS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TIERS, ofoTiersClass ))

typedef struct _ofoTiers              ofoTiers;

struct _ofoTiers {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoTiersClass;

GType             ofo_tiers_get_type              ( void ) G_GNUC_CONST;

GList            *ofo_tiers_get_dataset           ( ofaIGetter *getter );
#define           ofo_tiers_free_dataset( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref ))

ofoTiers         *ofo_tiers_get_by_id             ( ofaIGetter *getter, ofxCounter id );

ofoTiers         *ofo_tiers_new                   ( ofaIGetter *getter );

ofxCounter        ofo_tiers_get_id                ( const ofoTiers *tiers );
const gchar      *ofo_tiers_get_cre_user          ( const ofoTiers *tiers );
const myStampVal *ofo_tiers_get_cre_stamp         ( const ofoTiers *tiers );
const gchar      *ofo_tiers_get_label             ( const ofoTiers *tiers );
const gchar      *ofo_tiers_get_notes             ( const ofoTiers *tiers );
const gchar      *ofo_tiers_get_upd_user          ( const ofoTiers *tiers );
const myStampVal *ofo_tiers_get_upd_stamp         ( const ofoTiers *tiers );

gboolean          ofo_tiers_is_deletable          ( const ofoTiers *tiers );
gboolean          ofo_tiers_is_valid_data         ( const gchar *label, gchar **msgerr );

void              ofo_tiers_set_label             ( ofoTiers *tiers, const gchar *label );
void              ofo_tiers_set_notes             ( ofoTiers *tiers, const gchar *notes );

guint             ofo_tiers_doc_get_count         ( ofoTiers *tiers );

GList            *ofo_tiers_doc_get_orphans       ( ofaIGetter *getter );
#define           ofo_tiers_free_orphans( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean          ofo_tiers_insert                ( ofoTiers *tiers );
gboolean          ofo_tiers_update                ( ofoTiers *tiers );
gboolean          ofo_tiers_delete                ( ofoTiers *tiers );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_TIERS_H__ */
