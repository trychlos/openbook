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

#ifndef __OFO_DOSSIER_H__
#define __OFO_DOSSIER_H__

/**
 * SECTION: ofo_dossier
 * @short_description: #ofoDossier class definition.
 * @include: api/ofo-dossier.h
 *
 * This file defines the #ofoDossier public API, including the general
 * DB definition.
 *
 * Terminology:
 * - dbname: the name of the subjacent database
 * - dname: the name of the dossier as recorded in the global
 *   configuration, which appears in combo boxes
 * - label: the 'raison sociale' of the dossier, recorded in DBMS
 *   at creation time, this label defaults to the dossier name
 */

#include "api/ofa-boxed.h"
#include "api/ofa-dbms-def.h"
#include "api/ofo-dossier-def.h"
#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

/**
 * ofaDossierStatus:
 */
typedef enum {
	DOS_STATUS_OPENED = 1,
	DOS_STATUS_CLOSED
}
	ofaDossierStatus;

ofoDossier      *ofo_dossier_new                       ( void );

gboolean         ofo_dossier_open                      ( ofoDossier *dossier,
																const gchar *dname,
																const gchar *dbname,
																const gchar *account,
																const gchar *password );

const gchar     *ofo_dossier_get_name                  ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_user                  ( const ofoDossier *dossier );
const ofaDbms   *ofo_dossier_get_dbms                  ( const ofoDossier *dossier );

gboolean         ofo_dossier_use_currency              ( const ofoDossier *dossier,
																const gchar *currency );

const gchar     *ofo_dossier_get_default_currency      ( const ofoDossier *dossier );
const GDate     *ofo_dossier_get_exe_begin             ( const ofoDossier *dossier );
const GDate     *ofo_dossier_get_exe_end               ( const ofoDossier *dossier );
gint             ofo_dossier_get_exe_length            ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_exe_notes             ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_forward_label_close   ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_forward_label_open    ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_forward_ledger        ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_forward_ope           ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_label                 ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_notes                 ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_siren                 ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_sld_account           ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_sld_label             ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_sld_ledger            ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_sld_ope               ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_upd_user              ( const ofoDossier *dossier );
const GTimeVal  *ofo_dossier_get_upd_stamp             ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_status                ( const ofoDossier *dossier );
gint             ofo_dossier_get_this_exe_id           ( const ofoDossier *dossier );
gint             ofo_dossier_get_last_exe_id           ( const ofoDossier *dossier );
ofxCounter       ofo_dossier_get_last_bat              ( const ofoDossier *dossier );
ofxCounter       ofo_dossier_get_last_batline          ( const ofoDossier *dossier );
ofxCounter       ofo_dossier_get_last_entry            ( const ofoDossier *dossier );
ofxCounter       ofo_dossier_get_last_settlement       ( const ofoDossier *dossier );

ofxCounter       ofo_dossier_get_next_bat              ( ofoDossier *dossier );
ofxCounter       ofo_dossier_get_next_batline          ( ofoDossier *dossier );
ofxCounter       ofo_dossier_get_next_entry            ( ofoDossier *dossier );
ofxCounter       ofo_dossier_get_next_settlement       ( ofoDossier *dossier );

GDate           *ofo_dossier_get_min_deffect           ( GDate *date, const ofoDossier *dossier, ofoLedger *ledger );

gboolean         ofo_dossier_is_current                ( const ofoDossier *dossier );
gboolean         ofo_dossier_is_valid                  ( const gchar *label,
																gint nb_months,
																const gchar *currency,
																const GDate *begin,
																const GDate *end );

void             ofo_dossier_set_default_currency      ( ofoDossier *dossier, const gchar *currency );
void             ofo_dossier_set_exe_begin             ( ofoDossier *dossier, const GDate *date );
void             ofo_dossier_set_exe_end               ( ofoDossier *dossier, const GDate *date );
void             ofo_dossier_set_exe_length            ( ofoDossier *dossier, gint nb_months );
void             ofo_dossier_set_exe_notes             ( ofoDossier *dossier, const gchar *notes );
void             ofo_dossier_set_forward_label_close   ( ofoDossier *dossier, const gchar *label );
void             ofo_dossier_set_forward_label_open    ( ofoDossier *dossier, const gchar *label );
void             ofo_dossier_set_forward_ledger        ( ofoDossier *dossier, const gchar *ledger );
void             ofo_dossier_set_forward_ope           ( ofoDossier *dossier, const gchar *ope );
void             ofo_dossier_set_label                 ( ofoDossier *dossier, const gchar *label );
void             ofo_dossier_set_notes                 ( ofoDossier *dossier, const gchar *notes );
void             ofo_dossier_set_siren                 ( ofoDossier *dossier, const gchar *siren );
void             ofo_dossier_set_sld_account           ( ofoDossier *dossier, const gchar *account );
void             ofo_dossier_set_sld_label             ( ofoDossier *dossier, const gchar *label );
void             ofo_dossier_set_sld_ledger            ( ofoDossier *dossier, const gchar *ledger );
void             ofo_dossier_set_sld_ope               ( ofoDossier *dossier, const gchar *ope );

gboolean         ofo_dossier_update                    ( ofoDossier *dossier );

gboolean         ofo_dossier_backup                    ( const ofoDossier *dossier, const gchar *fname );

G_END_DECLS

#endif /* __OFO_DOSSIER_H__ */
