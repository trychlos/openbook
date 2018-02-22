/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_TVA_EXECLOSE_H__
#define __OFA_TVA_EXECLOSE_H__

/**
 * SECTION: ofa_tva_iexeclose
 * @short_description: #ofaTva class definition.
 *
 * #ofaIExeClose interface management.
 */

#include "api/ofa-iexe-close.h"

G_BEGIN_DECLS

void ofa_tva_execlose_iface_init( ofaIExeCloseInterface *iface );

G_END_DECLS

#endif /* __OFA_TVA_EXECLOSE_H__ */
