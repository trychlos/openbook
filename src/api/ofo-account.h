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

#ifndef __OFO_ACCOUNT_H__
#define __OFO_ACCOUNT_H__

/**
 * SECTION: ofo_account
 * @short_description: #ofoAccount class definition.
 * @include: api/ofo-account.h
 *
 * This class implements the Account behavior, including the general
 * DB definition.
 *
 * Initialization with:
 * load data local infile '/home/pierre/data/pierre@wieser.fr@cloud.trychlos.org/GTD-TR/OLA01 - Specifications/Plan comptable notarial 1988 simplié.csv' into table OFA_T_COMPTES fields terminated by ';' (@dummy,CPT_NUMBER,CPT_LABEL,CPT_NOTES);
 */

#include "api/my-date.h"
#include "api/ofo-base-def.h"
#include "api/ofo-account-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

void            ofo_account_connect_handlers     ( const ofoDossier *dossier );

GList          *ofo_account_get_dataset          ( const ofoDossier *dossier );
ofoAccount     *ofo_account_get_by_number        ( const ofoDossier *dossier, const gchar *number );
gboolean        ofo_account_use_currency         ( const ofoDossier *dossier, const gchar *devise );

ofoAccount     *ofo_account_new                  ( void );

void            ofo_account_dump_chart           ( GList *chart );

gint            ofo_account_get_class            ( const ofoAccount *account );
const gchar    *ofo_account_get_number           ( const ofoAccount *account );
const gchar    *ofo_account_get_label            ( const ofoAccount *account );
const gchar    *ofo_account_get_currency         ( const ofoAccount *account );
const gchar    *ofo_account_get_notes            ( const ofoAccount *account );
const gchar    *ofo_account_get_type_account     ( const ofoAccount *account );
const gchar    *ofo_account_get_upd_user         ( const ofoAccount *account );
const GTimeVal *ofo_account_get_upd_stamp        ( const ofoAccount *account );
gint            ofo_account_get_deb_entry        ( const ofoAccount *account );
const myDate   *ofo_account_get_deb_date         ( const ofoAccount *account );
gdouble         ofo_account_get_deb_amount       ( const ofoAccount *account );
gint            ofo_account_get_cre_entry        ( const ofoAccount *account );
const myDate   *ofo_account_get_cre_date         ( const ofoAccount *account );
gdouble         ofo_account_get_cre_amount       ( const ofoAccount *account );
gint            ofo_account_get_day_deb_entry    ( const ofoAccount *account );
const myDate   *ofo_account_get_day_deb_date     ( const ofoAccount *account );
gdouble         ofo_account_get_day_deb_amount   ( const ofoAccount *account );
gint            ofo_account_get_day_cre_entry    ( const ofoAccount *account );
const myDate   *ofo_account_get_day_cre_date     ( const ofoAccount *account );
gdouble         ofo_account_get_day_cre_amount   ( const ofoAccount *account );

gboolean        ofo_account_is_deletable         ( const ofoAccount *account );
gboolean        ofo_account_is_root              ( const ofoAccount *account );
gboolean        ofo_account_is_valid_data        ( const gchar *number, const gchar *label, const gchar *devise, const gchar *type );
gint            ofo_account_get_class_from_number( const gchar *number );
gint            ofo_account_get_level_from_number( const gchar *number );
const myDate   *ofo_account_get_global_deffect   ( const ofoAccount *account );
gdouble         ofo_account_get_global_solde     ( const ofoAccount *account );

void            ofo_account_set_number           ( ofoAccount *account, const gchar *number );
void            ofo_account_set_label            ( ofoAccount *account, const gchar *label );
void            ofo_account_set_currency         ( ofoAccount *account, const gchar *devise );
void            ofo_account_set_notes            ( ofoAccount *account, const gchar *notes );
void            ofo_account_set_type             ( ofoAccount *account, const gchar *type );
void            ofo_account_set_deb_entry        ( ofoAccount *account, gint num );
void            ofo_account_set_deb_date         ( ofoAccount *account, const myDate *date );
void            ofo_account_set_deb_amount       ( ofoAccount *account, gdouble mnt );
void            ofo_account_set_cre_entry        ( ofoAccount *account, gint num );
void            ofo_account_set_cre_date         ( ofoAccount *account, const myDate *date );
void            ofo_account_set_cre_amount       ( ofoAccount *account, gdouble mnt );
void            ofo_account_set_day_deb_entry    ( ofoAccount *account, gint num );
void            ofo_account_set_day_deb_date     ( ofoAccount *account, const myDate *date );
void            ofo_account_set_day_deb_amount   ( ofoAccount *account, gdouble mnt );
void            ofo_account_set_day_cre_entry    ( ofoAccount *account, gint num );
void            ofo_account_set_day_cre_date     ( ofoAccount *account, const myDate *date );
void            ofo_account_set_day_cre_amount   ( ofoAccount *account, gdouble mnt );

gboolean        ofo_account_insert               ( ofoAccount *account );
gboolean        ofo_account_update               ( ofoAccount *account, const gchar *prev_number );
gboolean        ofo_account_delete               ( ofoAccount *account );

GSList         *ofo_account_get_csv              ( const ofoDossier *dossier );
void            ofo_account_import_csv           ( const ofoDossier *dossier, GSList *lines, gboolean with_header );

G_END_DECLS

#endif /* __OFO_ACCOUNT_H__ */
