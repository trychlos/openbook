/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OFO_CURRENCY_H__
#define __OFO_CURRENCY_H__

/**
 * SECTION: ofo_currency
 * @short_description: #ofoCurrency class definition.
 * @include: api/ofo-currency.h
 *
 * This file defines the #ofoCurrency public API.
 */

#include "api/ofo-currency-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

GList          *ofo_currency_get_dataset  ( ofoDossier *dossier );
ofoCurrency    *ofo_currency_get_by_code  ( ofoDossier *dossier, const gchar *code );

ofoCurrency    *ofo_currency_new          ( void );

const gchar    *ofo_currency_get_code     ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_label    ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_symbol   ( const ofoCurrency *currency );
gint            ofo_currency_get_digits   ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_notes    ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_upd_user ( const ofoCurrency *currency );
const GTimeVal *ofo_currency_get_upd_stamp( const ofoCurrency *currency );

gboolean        ofo_currency_is_deletable ( const ofoCurrency *currency, ofoDossier *dossier );
gboolean        ofo_currency_is_valid     ( const gchar *code, const gchar *label, const gchar *symbol, gint digits );

void            ofo_currency_set_code     ( ofoCurrency *currency, const gchar *code );
void            ofo_currency_set_label    ( ofoCurrency *currency, const gchar *label );
void            ofo_currency_set_symbol   ( ofoCurrency *currency, const gchar *symbol );
void            ofo_currency_set_digits   ( ofoCurrency *currency, gint digits );
void            ofo_currency_set_notes    ( ofoCurrency *currency, const gchar *notes );

gboolean        ofo_currency_insert       ( ofoCurrency *currency, ofoDossier *dossier );
gboolean        ofo_currency_update       ( ofoCurrency *currency, ofoDossier *dossier, const gchar *prev_code );
gboolean        ofo_currency_delete       ( ofoCurrency *currency, ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_CURRENCY_H__ */
