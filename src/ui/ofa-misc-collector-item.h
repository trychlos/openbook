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

#ifndef __OFA_MISC_COLLECTOR_ITEM_H__
#define __OFA_MISC_COLLECTOR_ITEM_H__

/**
 * SECTION: ofa-misccollector_item
 * @include: ui/ofa-misc-collector-item.h
 *
 * Add option to Misc menu.
 */

#include <glib.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

void  ofa_misc_collector_item_signal_connect( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_MISC_COLLECTOR_ITEM_H__ */