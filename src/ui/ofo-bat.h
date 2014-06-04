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

#ifndef __OFO_BAT_H__
#define __OFO_BAT_H__

/**
 * SECTION: ofo_bat
 * @short_description: #ofoBat class definition.
 * @include: ui/ofo-bat.h
 *
 * This class implements the Bat behavior: these are the tables
 * which contain the imported bank account transaction lines.
 */

#include "api/ofa-iimporter.h"
#include "ui/ofo-bat-def.h"

G_BEGIN_DECLS

GType           ofo_bat_get_type     ( void ) G_GNUC_CONST;

ofoBat         *ofo_bat_new          ( void );

GList          *ofo_bat_get_dataset  ( const ofoDossier *dossier );

gint            ofo_bat_get_id       ( const ofoBat *bat );
const gchar    *ofo_bat_get_uri      ( const ofoBat *bat );
const gchar    *ofo_bat_get_format   ( const ofoBat *bat );
gint            ofo_bat_get_count    ( const ofoBat *bat );
const GDate    *ofo_bat_get_begin    ( const ofoBat *bat );
const GDate    *ofo_bat_get_end      ( const ofoBat *bat );
const gchar    *ofo_bat_get_rib      ( const ofoBat *bat );
const gchar    *ofo_bat_get_currency ( const ofoBat *bat );
gdouble         ofo_bat_get_solde    ( const ofoBat *bat );
gboolean        ofo_bat_get_solde_set( const ofoBat *bat );
const gchar    *ofo_bat_get_notes    ( const ofoBat *bat );
const gchar    *ofo_bat_get_maj_user ( const ofoBat *bat );
const GTimeVal *ofo_bat_get_maj_stamp( const ofoBat *bat );

gboolean        ofo_bat_is_deletable ( const ofoBat *bat );

void            ofo_bat_set_id       ( ofoBat *bat, gint id );
void            ofo_bat_set_uri      ( ofoBat *bat, const gchar *uri );
void            ofo_bat_set_format   ( ofoBat *bat, const gchar *format );
void            ofo_bat_set_count    ( ofoBat *bat, gint count );
void            ofo_bat_set_begin    ( ofoBat *bat, const GDate *date );
void            ofo_bat_set_end      ( ofoBat *bat, const GDate *date );
void            ofo_bat_set_rib      ( ofoBat *bat, const gchar *rib );
void            ofo_bat_set_currency ( ofoBat *bat, const gchar *currency );
void            ofo_bat_set_solde    ( ofoBat *bat, gdouble solde );
void            ofo_bat_set_solde_set( ofoBat *bat, gboolean set );
void            ofo_bat_set_notes    ( ofoBat *bat, const gchar *notes );
void            ofo_bat_set_maj_user ( ofoBat *bat, const gchar *user );
void            ofo_bat_set_maj_stamp( ofoBat *bat, const GTimeVal *stamp );

gboolean        ofo_bat_insert       ( ofoBat *bat, const ofoDossier *dossier );
gboolean        ofo_bat_update       ( ofoBat *bat, const ofoDossier *dossier );
gboolean        ofo_bat_delete       ( ofoBat *bat, const ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_BAT_H__ */
