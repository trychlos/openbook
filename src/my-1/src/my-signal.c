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

#include "my/my-signal.h"

/**
 * my_signal_accumulator_false_handled:
 * @ihint: standard #GSignalAccumulator parameter
 * @return_accu: standard #GSignalAccumulator parameter
 * @handler_return: standard #GSignalAccumulator parameter
 * @dummy: standard #GSignalAccumulator parameter
 *
 * A signal accumulator to handle the %FALSE return value.
 *
 * It is shamelessly copied from #g_signal_accumulator_true_handled()
 * to handle (in particular) the deletability status of an object.
 *
 * The idea here is that the signal emission will stop as soon as any
 * connected handler returns %FALSE. Else, the signal emission will
 * continue until the default class handler, which itself is expected
 * to return %TRUE.
 *
 * Returns: standard #GSignalAccumulator result
 */
gboolean
my_signal_accumulator_false_handled( GSignalInvocationHint *ihint, GValue *return_accu, const GValue *handler_return, gpointer dummy )
{
	gboolean continue_emission;
	gboolean handler_value;

	handler_value = g_value_get_boolean( handler_return );
	g_value_set_boolean( return_accu, handler_value );
	continue_emission = handler_value;

	return( continue_emission );
}
