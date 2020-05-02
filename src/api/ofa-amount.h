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

#ifndef __OPENBOOK_API_OFA_AMOUNT_H__
#define __OPENBOOK_API_OFA_AMOUNT_H__

/**
 * SECTION: ofa_amount
 * @short_description: Amount functions
 * @include: openbook/ofa-amount.h
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-currency-def.h"

G_BEGIN_DECLS

ofxAmount ofa_amount_from_str( const gchar *str, ofaIGetter *getter );

gchar    *ofa_amount_to_csv  ( ofxAmount amount, ofoCurrency *currency, ofaStreamFormat *format );

gchar    *ofa_amount_to_sql  ( ofxAmount amount, ofoCurrency *currency );

gchar    *ofa_amount_to_str  ( ofxAmount amount, ofoCurrency *currency, ofaIGetter *getter );

gboolean  ofa_amount_is_zero ( ofxAmount amount, ofoCurrency *currency );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_AMOUNT_H__ */
