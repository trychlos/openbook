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

#ifndef __OPENBOOK_API_OFO_ACCOUNT_V34_H__
#define __OPENBOOK_API_OFO_ACCOUNT_V34_H__

/**
 * SECTION: ofoaccount
 * @title: ofoAccountv34
 * @short_description: #ofoAccountv34 class definition.
 * @include: openbook/ofo-account-v34.h
 *
 * This file defines the #ofoAccountv34 class public API.
 *
 * This class does not participate to the normal run of the application.
 *
 * Instead, it is specially built to be used during DB model migrations,
 * by providing to the plugin a v34 version of the #ofoAccount class.
 *
 * ofoAccountv34 is made a base class of ofoAccount in order to be able
 * to share some functions.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-account-v34-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_ACCOUNT_V34                ( ofo_account_v34_get_type())
#define OFO_ACCOUNT_V34( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ACCOUNT_V34, ofoAccountv34 ))
#define OFO_ACCOUNT_V34_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ACCOUNT_V34, ofoAccountv34Class ))
#define OFO_IS_ACCOUNT_V34( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ACCOUNT_V34 ))
#define OFO_IS_ACCOUNT_V34_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ACCOUNT_V34 ))
#define OFO_ACCOUNT_V34_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ACCOUNT_V34, ofoAccountv34Class ))

#if 0
typedef struct _ofoAccountv34              ofoAccountv34;
typedef struct _ofoAccountv34Class         ofoAccountv34Class;

struct _ofoAccountv34 {
	/*< public members >*/
	ofoBase      parent;
};

struct _ofoAccountv34Class {
	/*< public members >*/
	ofoBaseClass parent;
};
#endif

GType          ofo_account_v34_get_type           ( void ) G_GNUC_CONST;

GList         *ofo_account_v34_get_dataset        ( ofaIGetter *getter );
#define        ofo_account_v34_free_dataset( L )  ( g_list_free_full(( L ),( GDestroyNotify ) g_object_unref ))

ofoAccountv34 *ofo_account_v34_get_by_number      ( GList *dataset, const gchar *number );

ofoAccountv34 *ofo_account_v34_new                ( ofaIGetter *getter );

const gchar   *ofo_account_v34_get_number         ( const ofoAccountv34 *account );
const gchar   *ofo_account_v34_get_currency       ( const ofoAccountv34 *account );
gboolean       ofo_account_v34_is_root            ( const ofoAccountv34 *account );

gboolean       ofo_account_v34_archive_balances_ex( ofoAccountv34 *account, const GDate *exe_begin, const GDate *date );

guint          ofo_account_v34_archive_get_count  ( ofoAccountv34 *account );
const GDate   *ofo_account_v34_archive_get_date   ( ofoAccountv34 *account, guint idx );
ofxAmount      ofo_account_v34_archive_get_debit  ( ofoAccountv34 *account, guint idx );
ofxAmount      ofo_account_v34_archive_get_credit ( ofoAccountv34 *account, guint idx );


G_END_DECLS

#endif /* __OPENBOOK_API_OFO_ACCOUNT_V34_H__ */
