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

#ifndef __OPENBOOK_API_OFO_CONCIL_H__
#define __OPENBOOK_API_OFO_CONCIL_H__

#include "api/ofa-box.h"
#include "api/ofo-concil-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

/**
 * ofoConcilType:
 */
#define CONCIL_TYPE_BAT                 "B"
#define CONCIL_TYPE_ENTRY               "E"

ofoConcil      *ofo_concil_get_by_id      ( const ofoDossier *dossier,
													ofxCounter rec_id );

ofoConcil      *ofo_concil_get_by_other_id( const ofoDossier *dossier,
													const gchar *type,
													ofxCounter other_id );

ofoConcil      *ofo_concil_new            ( void );

ofxCounter      ofo_concil_get_id         ( const ofoConcil *concil );

const GDate    *ofo_concil_get_dval       ( const ofoConcil *concil );

const gchar    *ofo_concil_get_user       ( const ofoConcil *concil );

const GTimeVal *ofo_concil_get_stamp      ( const ofoConcil *concil );

GList          *ofo_concil_get_ids        ( const ofoConcil *concil );

gboolean        ofo_concil_has_member     ( const ofoConcil *concil,
													const gchar *type,
													ofxCounter id );

void            ofo_concil_set_dval       ( ofoConcil *concil,
													const GDate *dval );

void            ofo_concil_set_user       ( ofoConcil *concil,
													const gchar *user );

void            ofo_concil_set_stamp       ( ofoConcil *concil,
													const GTimeVal *stamp );

gboolean        ofo_concil_insert         ( ofoConcil *concil,
													ofoDossier *dossier );

gboolean        ofo_concil_add_id         ( ofoConcil *concil,
													const gchar *type,
													ofxCounter id,
													ofoDossier *dossier );

gboolean        ofo_concil_delete         ( ofoConcil *concil,
													ofoDossier *dossier );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_CONCIL_H__ */
