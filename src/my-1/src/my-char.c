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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-char.h"

typedef struct {
	gint         ch;
	const gchar *label;
}
	sChar;

static const sChar st_chars[] = {
		{ MY_CHAR_COMMA,  N_( ",	(comma)" )},
		{ MY_CHAR_DOT,    N_( ".	(dot)" )},
		{ MY_CHAR_DQUOTE, N_( "\"	(double quote)" )},
		{ MY_CHAR_PIPE,   N_( "|	(pipe)" )},
		{ MY_CHAR_SCOLON, N_( ";	(semi-colon)" )},
		{ MY_CHAR_SPACE,  N_( " 	(space)" )},
		{ MY_CHAR_TAB,    N_( "\\t	(tab)" )},
		{ MY_CHAR_ZERO,   N_( "(none)" )},
		{ -1 }
};

static const sChar *find_by_char( gunichar ch );

/**
 * my_char_get_label:
 * @ch: the character.
 *
 * Returns: the label to be displayed.
 */
const gchar *
my_char_get_label( gunichar ch )
{
	const sChar *sdata;

	sdata = find_by_char( ch );

	return( sdata ? gettext( sdata->label ) : NULL );
}

static const sChar *
find_by_char( gunichar ch )
{
	gint i;

	for( i=0 ; st_chars[i].ch >= 0 ; ++i ){
		if( st_chars[i].ch == ch ){
			return( &st_chars[i] );
		}
	}

	return( NULL );
}
