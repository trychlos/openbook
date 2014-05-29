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

#ifndef __OFO_DEVISE_H__
#define __OFO_DEVISE_H__

/**
 * SECTION: ofo_devise
 * @short_description: #ofoDevise class definition.
 * @include: ui/ofo-devise.h
 *
 * This class implements the ofoDevise behavior, including the general
 * DB definition.
 */

#include "ui/ofo-devise-def.h"
#include "ui/ofo-dossier-def.h"

G_BEGIN_DECLS

GType           ofo_devise_get_type     ( void ) G_GNUC_CONST;

GList          *ofo_devise_get_dataset  ( const ofoDossier *dossier );
ofoDevise      *ofo_devise_get_by_code  ( const ofoDossier *dossier, const gchar *code );
ofoDevise      *ofo_devise_get_by_id    ( const ofoDossier *dossier, gint id );

ofoDevise      *ofo_devise_new          ( void );

gint            ofo_devise_get_id       ( const ofoDevise *devise );
const gchar    *ofo_devise_get_code     ( const ofoDevise *devise );
const gchar    *ofo_devise_get_label    ( const ofoDevise *devise );
const gchar    *ofo_devise_get_symbol   ( const ofoDevise *devise );
const gchar    *ofo_devise_get_notes    ( const ofoDevise *devise );
const gchar    *ofo_devise_get_maj_user ( const ofoDevise *devise );
const GTimeVal *ofo_devise_get_maj_stamp( const ofoDevise *devise );

gboolean        ofo_devise_is_deletable ( const ofoDevise *devise );
gboolean        ofo_devise_is_valid     ( const gchar *code, const gchar *label, const gchar *symbol );

void            ofo_devise_set_id       ( ofoDevise *devise, gint id );
void            ofo_devise_set_code     ( ofoDevise *devise, const gchar *code );
void            ofo_devise_set_label    ( ofoDevise *devise, const gchar *label );
void            ofo_devise_set_symbol   ( ofoDevise *devise, const gchar *symbol );
void            ofo_devise_set_notes    ( ofoDevise *devise, const gchar *notes );
void            ofo_devise_set_maj_user ( ofoDevise *devise, const gchar *user );
void            ofo_devise_set_maj_stamp( ofoDevise *devise, const GTimeVal *stamp );

gboolean        ofo_devise_insert    ( ofoDevise *devise, ofoDossier *dossier );
gboolean        ofo_devise_update    ( ofoDevise *devise, ofoDossier *dossier );
gboolean        ofo_devise_delete    ( ofoDevise *devise, ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_DEVISE_H__ */
