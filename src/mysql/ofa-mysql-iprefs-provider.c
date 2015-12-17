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

#include "ofa-mysql-iprefs-provider.h"
#include "ofa-mysql-prefs-bin.h"

static guint          iprefs_provider_get_interface_version( const ofaIPrefsProvider *instance );
static ofaIPrefsPage *iprefs_provider_new_page( void );

void
ofa_mysql_iprefs_provider_iface_init( ofaIPrefsProviderInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_iprefs_provider_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iprefs_provider_get_interface_version;
	iface->new_page = iprefs_provider_new_page;
}

static guint
iprefs_provider_get_interface_version( const ofaIPrefsProvider *instance )
{
	return( 1 );
}

static ofaIPrefsPage *
iprefs_provider_new_page( void )
{
	GtkWidget *page;

	page = ofa_mysql_prefs_bin_new();

	return( OFA_IPREFS_PAGE( page ));
}
