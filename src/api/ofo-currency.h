/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFO_CURRENCY_H__
#define __OPENBOOK_API_OFO_CURRENCY_H__

/**
 * SECTION: ofocurrency
 * @title: ofoCurrency
 * @short_description: #ofoCurrency class definition.
 * @include: openbook/ofo-currency.h
 *
 * This file defines the #ofoCurrency public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-currency-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_CURRENCY                ( ofo_currency_get_type())
#define OFO_CURRENCY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_CURRENCY, ofoCurrency ))
#define OFO_CURRENCY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_CURRENCY, ofoCurrencyClass ))
#define OFO_IS_CURRENCY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_CURRENCY ))
#define OFO_IS_CURRENCY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_CURRENCY ))
#define OFO_CURRENCY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_CURRENCY, ofoCurrencyClass ))

#if 0
typedef struct _ofoCurrency              ofoCurrency;
#endif

struct _ofoCurrency {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoCurrencyClass;

GType           ofo_currency_get_type              ( void ) G_GNUC_CONST;

GList          *ofo_currency_get_dataset           ( ofaIGetter *getter );
#define         ofo_currency_free_dataset( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref ))

ofoCurrency    *ofo_currency_get_by_code           ( ofaIGetter *getter, const gchar *code );

ofoCurrency    *ofo_currency_new                   ( ofaIGetter *getter );

const gchar    *ofo_currency_get_code              ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_label             ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_symbol            ( const ofoCurrency *currency );
gint            ofo_currency_get_digits            ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_notes             ( const ofoCurrency *currency );
const gchar    *ofo_currency_get_upd_user          ( const ofoCurrency *currency );
const GTimeVal *ofo_currency_get_upd_stamp         ( const ofoCurrency *currency );
const gdouble   ofo_currency_get_precision         ( const ofoCurrency *currency );

gboolean        ofo_currency_is_deletable          ( const ofoCurrency *currency );
gboolean        ofo_currency_is_valid_data         ( const gchar *code, const gchar *label, const gchar *symbol, gint digits, gchar **msgerr );

guint           ofo_currency_doc_get_count         ( ofoCurrency *currency );

void            ofo_currency_set_code              ( ofoCurrency *currency, const gchar *code );
void            ofo_currency_set_label             ( ofoCurrency *currency, const gchar *label );
void            ofo_currency_set_symbol            ( ofoCurrency *currency, const gchar *symbol );
void            ofo_currency_set_digits            ( ofoCurrency *currency, gint digits );
void            ofo_currency_set_notes             ( ofoCurrency *currency, const gchar *notes );

GList          *ofo_currency_get_doc_orphans       ( ofaIGetter *getter );
#define         ofo_currency_free_doc_orphans( L ) ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean        ofo_currency_insert                ( ofoCurrency *currency );
gboolean        ofo_currency_update                ( ofoCurrency *currency, const gchar *prev_code );
gboolean        ofo_currency_delete                ( ofoCurrency *currency );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_CURRENCY_H__ */
