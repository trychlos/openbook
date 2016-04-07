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

#ifndef __OFA_TVA_EXECLOSE_H__
#define __OFA_TVA_EXECLOSE_H__

/**
 * SECTION: ofa_recurrent_execlose
 * @short_description: #ofaRecurrent definition.
 *
 * #ofaIExeClose interface management.
 */

#include "api/ofa-iexe-close.h"

G_BEGIN_DECLS

void ofa_recurrent_execlose_iface_init( ofaIExeCloseInterface *iface );

G_END_DECLS

#endif /* __OFA_TVA_EXECLOSE_H__ */
