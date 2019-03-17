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

#ifndef __OPENBOOK_API_OFA_HUB_REMEDIATE_H__
#define __OPENBOOK_API_OFA_HUB_REMEDIATE_H__

/**
 * SECTION: ofa_hub
 * @title: ofaHub
 * @short_description: The #ofaHub remediation actions
 * @include: openbook/ofa-hub-remediate.h
 */

#include "api/ofa-igetter-def.h"

/* The callback function for ofa_hub_remediate_recompute_balances():
 */
typedef gboolean ( *fnRemediateRecompute )( ofaHub *, void * );

gboolean ofa_hub_remediate_logicals          ( ofaHub *hub );

gboolean ofa_hub_remediate_recompute_balances( ofaHub *hub, fnRemediateRecompute pfn, void *data );

gboolean ofa_hub_remediate_settings          ( ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_HUB_REMEDIATE_H__ */
