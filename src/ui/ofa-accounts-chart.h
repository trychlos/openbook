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

#ifndef __OFA_ACCOUNTS_CHART_H__
#define __OFA_ACCOUNTS_CHART_H__

/**
 * SECTION: ofa_accounts_chart
 * @short_description: #ofaAccountsChart class definition.
 * @include: ui/ofa-accounts-chart.h
 *
 * Display the chart of accounts, letting the user edit it.
 */

#include "ui/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_CHART                ( ofa_accounts_chart_get_type())
#define OFA_ACCOUNTS_CHART( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_CHART, ofaAccountsChart ))
#define OFA_ACCOUNTS_CHART_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_CHART, ofaAccountsChartClass ))
#define OFA_IS_ACCOUNTS_CHART( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_CHART ))
#define OFA_IS_ACCOUNTS_CHART_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_CHART ))
#define OFA_ACCOUNTS_CHART_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_CHART, ofaAccountsChartClass ))

typedef struct _ofaAccountsChartPrivate        ofaAccountsChartPrivate;

typedef struct {
	/*< private >*/
	ofaPage                  parent;
	ofaAccountsChartPrivate *private;
}
	ofaAccountsChart;

typedef struct {
	/*< private >*/
	ofaPageClass parent;
}
	ofaAccountsChartClass;

GType ofa_accounts_chart_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_ACCOUNTS_CHART_H__ */
