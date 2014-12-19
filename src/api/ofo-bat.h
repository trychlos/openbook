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
 * @include: api/ofo-bat.h
 *
 * This file defines the #ofoBat class public API: these are the tables
 * which contain the imported bank account transaction lines.
 */

#include "api/ofa-box.h"
#include "api/ofa-iimporter.h"
#include "api/ofo-bat-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

ofoBat         *ofo_bat_new          ( void );

GList          *ofo_bat_get_dataset  ( ofoDossier *dossier );

ofxCounter      ofo_bat_get_id       ( const ofoBat *bat );
const gchar    *ofo_bat_get_uri      ( const ofoBat *bat );
const gchar    *ofo_bat_get_format   ( const ofoBat *bat );
gint            ofo_bat_get_count    ( const ofoBat *bat );
const GDate    *ofo_bat_get_begin    ( const ofoBat *bat );
const GDate    *ofo_bat_get_end      ( const ofoBat *bat );
const gchar    *ofo_bat_get_rib      ( const ofoBat *bat );
const gchar    *ofo_bat_get_currency ( const ofoBat *bat );
ofxAmount       ofo_bat_get_solde    ( const ofoBat *bat );
gboolean        ofo_bat_get_solde_set( const ofoBat *bat );
const gchar    *ofo_bat_get_notes    ( const ofoBat *bat );
const gchar    *ofo_bat_get_upd_user ( const ofoBat *bat );
const GTimeVal *ofo_bat_get_upd_stamp( const ofoBat *bat );

gboolean        ofo_bat_exists       ( const ofoDossier *dossier,
										const gchar *rib, const GDate *begin, const GDate *end );
gboolean        ofo_bat_is_deletable ( const ofoBat *bat );

void            ofo_bat_set_id       ( ofoBat *bat, ofxCounter id );
void            ofo_bat_set_uri      ( ofoBat *bat, const gchar *uri );
void            ofo_bat_set_format   ( ofoBat *bat, const gchar *format );
void            ofo_bat_set_count    ( ofoBat *bat, gint count );
void            ofo_bat_set_begin    ( ofoBat *bat, const GDate *date );
void            ofo_bat_set_end      ( ofoBat *bat, const GDate *date );
void            ofo_bat_set_rib      ( ofoBat *bat, const gchar *rib );
void            ofo_bat_set_currency ( ofoBat *bat, const gchar *currency );
void            ofo_bat_set_solde    ( ofoBat *bat, ofxAmount solde );
void            ofo_bat_set_solde_set( ofoBat *bat, gboolean set );
void            ofo_bat_set_notes    ( ofoBat *bat, const gchar *notes );

gboolean        ofo_bat_insert       ( ofoBat *bat, ofoDossier *dossier );
gboolean        ofo_bat_update       ( ofoBat *bat, ofoDossier *dossier );
gboolean        ofo_bat_delete       ( ofoBat *bat, ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_BAT_H__ */
