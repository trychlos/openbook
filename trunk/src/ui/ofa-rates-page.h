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

#ifndef __OFA_RATES_PAGE_H__
#define __OFA_RATES_PAGE_H__

/**
 * SECTION: ofa_rates_page
 * @short_description: #ofaRatesPage class definition.
 * @include: ui/ofa-taux-set.h
 *
 * Display the chart of accounts, letting the user edit it.
 */

#include "ui/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RATES_PAGE                ( ofa_rates_page_get_type())
#define OFA_RATES_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RATES_PAGE, ofaRatesPage ))
#define OFA_RATES_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RATES_PAGE, ofaRatesPageClass ))
#define OFA_IS_RATES_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RATES_PAGE ))
#define OFA_IS_RATES_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RATES_PAGE ))
#define OFA_RATES_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RATES_PAGE, ofaRatesPageClass ))

typedef struct _ofaRatesPagePrivate        ofaRatesPagePrivate;

typedef struct {
	/*< public members >*/
	ofaPage              parent;

	/*< private members >*/
	ofaRatesPagePrivate *priv;
}
	ofaRatesPage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaRatesPageClass;

GType ofa_rates_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_RATES_PAGE_H__ */
