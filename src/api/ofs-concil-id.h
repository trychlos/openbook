/*
 * ConcilIdn Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
 *
 * ConcilIdn Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * ConcilIdn Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ConcilIdn Freelance Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFS_CONCIL_ID_H__
#define __OFS_CONCIL_ID_H__

/**
 * SECTION: ofs_concil_id
 * @short_description: #ofsConcilId structure definition.
 * @include: openbook/ofs-concil-id.h
 *
 * One line of a reconciliation group.
 */

#include "api/ofa-box.h"

G_BEGIN_DECLS

/**
 * ofsConcilId:
 */
typedef struct {
	gchar     *type;
	ofxCounter other_id;
}
	ofsConcilId;

/**
 * ofaConcilType:
 */
#define CONCIL_TYPE_BAT                 "B"
#define CONCIL_TYPE_ENTRY               "E"

ofsConcilId *ofs_concil_id_new              ( void );

void         ofs_concil_id_free             ( ofsConcilId *sid );

gboolean     ofs_concil_id_is_equal         ( const ofsConcilId *sid,
														const gchar *type, ofxCounter id );

ofxCounter   ofs_concil_id_get_first        ( GList *ids,
														const gchar *type );

void         ofs_concil_id_get_count_by_type( GList *ids,
														guint *ent_count,
														guint *bat_count );

G_END_DECLS

#endif /* __OFS_CONCIL_ID_H__ */
