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

#include <math.h>

#include "api/ofo-account.h"
#include "api/ofs-account-balance.h"

static void account_balance_free( ofsAccountBalance *balance );

/**
 * ofs_account_balance_list_free:
 */
void
ofs_account_balance_list_free( GList **list )
{
	if( *list ){
		g_list_free_full( *list, ( GDestroyNotify ) account_balance_free );
		*list = NULL;
	}
}

static void
account_balance_free( ofsAccountBalance *balance )
{
	g_free( balance->account );
	g_free( balance->currency );
	g_free( balance );
}
