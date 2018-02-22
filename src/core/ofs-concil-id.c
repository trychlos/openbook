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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofs-concil-id.h"

/**
 * ofs_concil_id_new:
 */
ofsConcilId *
ofs_concil_id_new( void )
{
	return( g_new0( ofsConcilId, 1 ));
}

/**
 * ofs_concil_id_free:
 */
void
ofs_concil_id_free( ofsConcilId *sid )
{
	g_free( sid->type );
	g_free( sid );
}

/**
 * ofs_concil_id_is_equal:
 * @sid:
 * @type: the requested type.
 * @id:
 *
 * Returns: %TRUE if the #ofsConcilId data is equal to @type+@id.
 */
gboolean
ofs_concil_id_is_equal( const ofsConcilId *sid, const gchar *type, ofxCounter id )
{
	return( !g_utf8_collate( sid->type, type ) && sid->other_id == id );
}

/**
 * ofs_concil_id_get_first:
 * @ids: a list of #ofsConcilId structs.
 * @type: the requested type.
 *
 * Returns the identifier 'other_id' of the first line which corresponds
 * to the requested @type, or 0 if not found.
 */
ofxCounter
ofs_concil_id_get_first( GList *ids, const gchar *type )
{
	GList *it;
	ofsConcilId *sid;

	for( it=ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( !g_utf8_collate( sid->type, type )){
			return( sid->other_id );
		}
	}

	return( 0 );
}

/**
 * ofs_concil_id_get_count_by_type:
 * @ids:
 * @ent_count:
 * @bat_count:
 *
 * Compute the count of rows of every type.
 */
void
ofs_concil_id_get_count_by_type( GList *ids, guint *ent_count, guint *bat_count )
{
	GList *it;
	ofsConcilId *sid;

	*ent_count = 0;
	*bat_count = 0;

	for( it=ids ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( !g_utf8_collate( sid->type, CONCIL_TYPE_ENTRY )){
			*ent_count += 1;
		} else if( !g_utf8_collate( sid->type, CONCIL_TYPE_BAT )){
			*bat_count += 1;
		}
	}
}
