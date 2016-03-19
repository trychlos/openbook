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

#ifndef __MY_API_MY_TIMEOUT_H__
#define __MY_API_MY_TIMEOUT_H__

/**
 * SECTION: timeout
 * @title: myTimeout
 * @short_description: The myTimeout Structure
 * @include: my/my-timeout.h
 *
 * The myTimeout structure is a convenience structure to manage timeout
 * functions.
 */

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * myTimeoutFunc:
 * @user_data: data to be passed to the callback function.
 *
 * Prototype of the callback function.
 */
typedef void ( *myTimeoutFunc )( void *user_data );

/**
 * myTimeout:
 * @timeout:   timeout configurable parameter (ms)
 * @handler:   handler function
 * @user_data: user data
 *
 * This structure let the user (i.e. the code which uses it) manage
 * functions which should only be called after some time of inactivity,
 * which is, for example, typically the case of change handlers.
 *
 * The structure is supposed to be initialized at construction time
 * with @timeout in milliseconds, @handler and @user_data input
 * parameters. The private data should be set to %NULL.
 *
 * Such a structure must be allocated for each managed event.
 *
 * When an event is detected, the #my_timeout_event() function must be
 * called with this structure. The function makes sure that the @handler
 * callback will be triggered as soon as no event will be recorded after
 * @timeout milliseconds of inactivity.
 */
typedef struct {
	/*< public >*/
	guint          timeout;
	myTimeoutFunc  handler;
	gpointer       user_data;
	/*< private >*/
	GTimeVal       last_time;
	guint          source_id;
}
	myTimeout;

void my_timeout_event( myTimeout *timeout );

G_END_DECLS

#endif /* __MY_API_MY_TIMEOUT_H__ */
