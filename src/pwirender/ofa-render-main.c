/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "pwirender/ofa-render-main.h"

#if 0
/**
 * ofa_recurrent_main_signal_connect:
 * @getter: the main #ofaIGetter of the application.
 *
 * Connect to the ofaIGetter signals.
 * This will in particular let us update the application menubar.
 */
void
ofa_recurrent_main_signal_connect( ofaIGetter *getter )
{
	static const gchar *thisfn = "recurrent/ofa_recurrent_main_signal_connect";
	ofaISignaler *signaler;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	signaler = ofa_igetter_get_signaler( getter );

	g_signal_connect( signaler,
			SIGNALER_PAGE_MANAGER_AVAILABLE, G_CALLBACK( on_page_manager_available ), NULL );

	g_signal_connect( signaler,
			SIGNALER_MENU_AVAILABLE, G_CALLBACK( on_menu_available ), getter );
}
#endif
