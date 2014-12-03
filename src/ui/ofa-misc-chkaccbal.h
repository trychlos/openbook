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

#ifndef __OFA_MISC_CHKACCBAL_H__
#define __OFA_MISC_CHKACCBAL_H__

/**
 * SECTION: ofa_misc
 * @short_description: Miscellaneous functions.
 * @include: ui/ofa-misc-chkentbal.h
 *
 * Check that the entries on the current exercice are balanced.
 *
 * This is done in particular before closing the exercice.
 * The beginning and ending date must be set, or all entries will be
 * checked.
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

#include "ui/my-progress-bar.h"
#include "ui/ofa-balances-grid.h"

G_BEGIN_DECLS

gboolean ofa_misc_chkaccbal_run( ofoDossier *dossier, myProgressBar *bar, ofaBalancesGrid *grid );

G_END_DECLS

#endif /* __OFA_MISC_CHKACCBAL_H__ */
