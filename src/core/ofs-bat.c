/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/ofa-preferences.h"
#include "api/ofs-bat.h"

static void bat_dump_detail( ofsBatDetail *detail, const gchar *thisfn );

/**
 * ofs_bat_dump:
 * @bat:
 */
void
ofs_bat_dump( const ofsBat *bat )
{
	static const gchar *thisfn = "ofs_bat_dump";
	gchar *sbegin, *send;

	g_debug( "%s:     version=%u", thisfn, bat->version );
	g_debug( "%s:         uri=%s", thisfn, bat->uri );
	g_debug( "%s:      format=%s", thisfn, bat->format );
	sbegin = my_date_to_str( &bat->begin, ofa_prefs_date_display());
	g_debug( "%s:       begin=%s", thisfn, sbegin );
	g_free( sbegin );
	send = my_date_to_str( &bat->end, ofa_prefs_date_display());
	g_debug( "%s:         end=%s", thisfn, send );
	g_free( send );
	g_debug( "%s:    currency=%s", thisfn, bat->currency );
	sbegin = my_double_to_str( bat->begin_solde );
	g_debug( "%s: begin_solde=%s, set=%s", thisfn, sbegin, bat->begin_solde_set ? "True":"False" );
	g_free( sbegin );
	send = my_double_to_str( bat->end_solde );
	g_debug( "%s:   end_solde=%s, set=%s", thisfn, send, bat->end_solde_set ? "True":"False" );
	g_free( send );

	g_list_foreach( bat->details, ( GFunc ) bat_dump_detail, ( gpointer ) thisfn );
}

static void
bat_dump_detail( ofsBatDetail *detail, const gchar *thisfn )
{
	gchar *sdope, *sdeffect, *samount;

	sdope = my_date_to_str( &detail->dope, ofa_prefs_date_display());
	sdeffect = my_date_to_str( &detail->deffect, ofa_prefs_date_display());
	samount = my_double_to_str( detail->amount );

	g_debug( "%s: version=%u, dope=%s, deffect=%s, ref=%s, label=%s, amount=%s, currency=%s",
			thisfn, detail->version,
			sdope, sdeffect, detail->ref, detail->label, samount, detail->currency );

	g_free( sdeffect );
	g_free( sdope );
	g_free( samount );
}

/**
 * ofo_bat_free:
 * @bat:
 *
 * Free the provided #ofsBat structure.
 */
void
ofs_bat_free( ofsBat *bat )
{
	g_list_free_full( bat->details, ( GDestroyNotify ) ofs_bat_detail_free );

	g_free( bat->uri );
	g_free( bat->format );
	g_free( bat->rib );
	g_free( bat->currency );
	g_free( bat );
}

/**
 * ofs_bat_detail_free:
 * @detail:
 */
void
ofs_bat_detail_free( ofsBatDetail *detail )
{
	g_free( detail->ref );
	g_free( detail->label );
	g_free( detail->currency );
	g_free( detail );
}
