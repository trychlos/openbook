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

#ifndef __OPENBOOK_API_OFO_CONCIL_H__
#define __OPENBOOK_API_OFO_CONCIL_H__

/**
 * SECTION: ofoconcil
 * @title: ofoConcil
 * @short_description: #ofoConcil class definition.
 * @include: openbook/ofo-concil.h
 *
 * This file defines the #ofoConcil public API.
 *
 * A #ofoConcil object is actually a conciliation group. Under a
 * dossier-wide unique identifier, it maintains a list of all lines,
 * entries or BAT, which belong and share this same conciliation group.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-concil-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_CONCIL                ( ofo_concil_get_type())
#define OFO_CONCIL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_CONCIL, ofoConcil ))
#define OFO_CONCIL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_CONCIL, ofoConcilClass ))
#define OFO_IS_CONCIL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_CONCIL ))
#define OFO_IS_CONCIL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_CONCIL ))
#define OFO_CONCIL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_CONCIL, ofoConcilClass ))

#if 0
typedef struct _ofoConcil              ofoConcil;
#endif

struct _ofoConcil {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoConcilClass;

/**
 * ofoConcilType:
 */
#define CONCIL_TYPE_BAT                 "B"
#define CONCIL_TYPE_ENTRY               "E"

typedef void (*ofoConcilEnumerate )( ofoConcil *concil, const gchar *type, ofxCounter id, void *user_data );

GType             ofo_concil_get_type               ( void ) G_GNUC_CONST;

GList            *ofo_concil_get_dataset            ( ofaIGetter *getter );

ofoConcil        *ofo_concil_get_by_id              ( ofaIGetter *getter, ofxCounter rec_id );
ofoConcil        *ofo_concil_get_by_other_id        ( ofaIGetter *getter, const gchar *type, ofxCounter other_id );

ofoConcil        *ofo_concil_new                    ( ofaIGetter *getter );

ofxCounter        ofo_concil_get_id                 ( ofoConcil *concil );
const GDate      *ofo_concil_get_dval               ( ofoConcil *concil );
const gchar      *ofo_concil_get_upd_user           ( ofoConcil *concil );
const myStampVal *ofo_concil_get_upd_stamp          ( ofoConcil *concil );
GList            *ofo_concil_get_ids                ( ofoConcil *concil );
gboolean          ofo_concil_has_member             ( ofoConcil *concil, const gchar *type, ofxCounter id );

void              ofo_concil_for_each_member        ( ofoConcil *concil, ofoConcilEnumerate fn, void *user_data );

void              ofo_concil_set_dval               ( ofoConcil *concil, const GDate *dval );
void              ofo_concil_set_upd_user           ( ofoConcil *concil, const gchar *user );
void              ofo_concil_set_upd_stamp          ( ofoConcil *concil, const myStampVal *stamp );

GList            *ofo_concil_get_concil_orphans     ( ofaIGetter *getter );
#define           ofo_concil_free_concil_orphans(L) g_list_free(( L ))

GList            *ofo_concil_get_bat_orphans        ( ofaIGetter *getter );
#define           ofo_concil_free_bat_orphans(L)    g_list_free(( L ))

GList            *ofo_concil_get_entry_orphans      ( ofaIGetter *getter );
#define           ofo_concil_free_entry_orphans(L)  g_list_free(( L ))

gboolean          ofo_concil_insert                 ( ofoConcil *concil );
gboolean          ofo_concil_add_id                 ( ofoConcil *concil, const gchar *type, ofxCounter id );
gboolean          ofo_concil_delete                 ( ofoConcil *concil );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_CONCIL_H__ */
