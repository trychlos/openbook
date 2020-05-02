/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-timeout.h"

static gboolean on_timeout_event_timeout( myTimeout *timeout );

/**
 * my_timeout_new:
 *
 * Returns: a new #myTimeout structure, which should be my_timeout_free() by the caller.
 */
myTimeout *
my_timeout_new()
{
	myTimeout *timeout;

	timeout = g_new0( myTimeout, 1 );
	timeout->last_time = my_stamp_new();

	return( timeout );
}

/**
 * my_timeout_event:
 * @timeout: the #myTimeout structure which will handle this event.
 */
void
my_timeout_event( myTimeout *event )
{
	g_return_if_fail( event != NULL );

	my_stamp_set_now( event->last_time );

	if( !event->source_id ){
		event->source_id = g_timeout_add(
				event->timeout, ( GSourceFunc ) on_timeout_event_timeout, event );
	}
}

/*
 * this timer is set when we receive the first event of a serie
 * we continue to loop until last event is older that our burst timeout
 */
static gboolean
on_timeout_event_timeout( myTimeout *timeout )
{
	myStampVal *now;
	gulong diff;
	gulong timeout_usec;

	now = my_stamp_new_now();
	diff = ( gulong ) my_stamp_diff_us( now, timeout->last_time );
	timeout_usec = 1000*timeout->timeout;

	if( diff < timeout_usec ){
		/* do not stop */
		return( TRUE );
	}

	/* last individual notification is older that the 'timeout' parameter
	 * we may so suppose that the burst is terminated
	 * and feel authorized to trigger the defined callback
	 */
	( *timeout->handler )( timeout->user_data );

	/* at the end of the callback execution, reset the event source id.
	 * and stop the execution of this one
	 */
	timeout->source_id = 0;
	return( FALSE );
}

/**
 * my_timeout_free:
 * @timeout: [allow-none]: a #myTimeout structure to be released.
 *
 * Release the @timeout structure.
 */
void
my_timeout_free( myTimeout *timeout )
{
	if( timeout ){
		g_free( timeout->last_time );
		g_free( timeout );
	}
}
