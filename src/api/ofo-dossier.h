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
 */

#include "api/ofo-dossier-def.h"
#include "api/ofo-sgbd-def.h"

G_BEGIN_DECLS

/**
 * ofaDossierStatus:
 *
 * Status of the exercice.
 *
 * The system makes sure there is at most one opened exercice at any
 * time.
 */
typedef enum {
	DOS_STATUS_OPENED = 1,
	DOS_STATUS_CLOSED
}
	ofaDossierStatus;

ofoDossier      *ofo_dossier_new                       ( const gchar *name );

gboolean         ofo_dossier_open                      ( ofoDossier *dossier,
														const gchar *account, const gchar *password );

const gchar     *ofo_dossier_get_name                  ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_user                  ( const ofoDossier *dossier );
const ofoSgbd   *ofo_dossier_get_sgbd                  ( const ofoDossier *dossier );

gchar           *ofo_dossier_get_dbname                ( const ofoDossier *dossier );

gboolean         ofo_dossier_use_currency              ( const ofoDossier *dossier, const gchar *currency );

const gchar     *ofo_dossier_get_label                 ( const ofoDossier *dossier );
gint             ofo_dossier_get_exercice_length       ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_default_currency      ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_notes                 ( const ofoDossier *dossier );
const gchar     *ofo_dossier_get_upd_user              ( const ofoDossier *dossier );
const GTimeVal  *ofo_dossier_get_upd_stamp             ( const ofoDossier *dossier );

GList           *ofo_dossier_get_exercices_list        ( const ofoDossier *dossier );
GDate           *ofo_dossier_get_last_closed_exercice  ( const ofoDossier *dossier );
gint             ofo_dossier_get_current_exe_id        ( const ofoDossier *dossier );

gint             ofo_dossier_get_exe_by_date           ( const ofoDossier *dossier, const GDate *date );
const GDate     *ofo_dossier_get_exe_begin             ( const ofoDossier *dossier, gint exe_id );
const GDate     *ofo_dossier_get_exe_end               ( const ofoDossier *dossier, gint exe_id );
ofaDossierStatus ofo_dossier_get_exe_status            ( const ofoDossier *dossier, gint exe_id );
gint             ofo_dossier_get_exe_last_entry        ( const ofoDossier *dossier, gint exe_id );
gint             ofo_dossier_get_exe_last_settlement   ( const ofoDossier *dossier, gint exe_id );
gint             ofo_dossier_get_exe_last_bat          ( const ofoDossier *dossier, gint exe_id );
gint             ofo_dossier_get_exe_last_bat_line     ( const ofoDossier *dossier, gint exe_id );
const gchar     *ofo_dossier_get_exe_notes             ( const ofoDossier *dossier, gint exe_id );

gint             ofo_dossier_get_next_entry_number     ( const ofoDossier *dossier );
gint             ofo_dossier_get_next_bat_number       ( const ofoDossier *dossier );
gint             ofo_dossier_get_next_batline_number   ( const ofoDossier *dossier );
gint             ofo_dossier_get_next_settlement_number( const ofoDossier *dossier );

const gchar     *ofo_dossier_get_status_label          ( ofaDossierStatus status );

gboolean         ofo_dossier_is_valid                  ( const gchar *label,
															gint nb_months, const gchar *currency,
															const GDate *begin, const GDate *end );

void             ofo_dossier_set_label                 ( ofoDossier *dossier, const gchar *label );
void             ofo_dossier_set_exercice_length       ( ofoDossier *dossier, gint nb_months );
void             ofo_dossier_set_default_currency      ( ofoDossier *dossier, const gchar *currency );
void             ofo_dossier_set_notes                 ( ofoDossier *dossier, const gchar *notes );

void             ofo_dossier_set_current_exe_id        ( const ofoDossier *dossier, gint exe_id );
void             ofo_dossier_set_current_exe_begin     ( const ofoDossier *dossier, const GDate *date );
void             ofo_dossier_set_current_exe_end       ( const ofoDossier *dossier, const GDate *date );
void             ofo_dossier_set_current_exe_notes     ( const ofoDossier *dossier, const gchar *notes );
void             ofo_dossier_set_current_exe_last_entry( const ofoDossier *dossier, gint number );

gboolean         ofo_dossier_dbmodel_update            ( ofoSgbd *sgbd, const gchar *name, const gchar *account );

gboolean         ofo_dossier_update                    ( ofoDossier *dossier );

GSList          *ofo_dossier_get_csv                   ( const ofoDossier *dossier );

gboolean         ofo_dossier_backup                    ( const ofoDossier *dossier, const gchar *fname );

G_END_DECLS

#endif /* __OFO_DOSSIER_H__ */
