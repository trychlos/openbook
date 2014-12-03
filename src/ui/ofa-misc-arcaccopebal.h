/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __OFA_MISC_ARCACCOPEBAL_H__
#define __OFA_MISC_ARCACCOPEBAL_H__

/**
 * SECTION: ofa_misc
 * @short_description: Miscellaneous functions.
 * @include: ui/ofa-misc-arcaccopebal.h
 *
 * Archive accounts balances when opening the exercice.
 * Ask for a confirmation when the function is called from the main
 * menu.
 *
 * This function should very rarely be used, and should rather be seen
 * as a maintenance function.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

void ofa_misc_arcaccopebal_run( const ofaMainWindow *main_window );

G_END_DECLS

#endif /* __OFA_MISC_ARCACCOPEBAL_H__ */
