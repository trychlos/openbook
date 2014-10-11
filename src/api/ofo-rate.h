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

#ifndef __OFO_RATE_H__
#define __OFO_RATE_H__

/**
 * SECTION: ofo_rate
 * @short_description: #ofoRate class definition.
 * @include: api/ofo-rate.h
 *
 * This file defines the #ofoRate class public API.
 */

#include "api/ofo-dossier-def.h"
#include "api/ofo-rate-def.h"

G_BEGIN_DECLS

/**
 * ofsRateValidity:
 *
 * The structure used to validate all the validities of a rate.
 */
typedef struct {
	GDate   begin;						/* invalid date is infinite in the past */
	GDate   end;						/* invalid date is infinite in the future */
	gdouble rate;
}
	ofsRateValidity;

GList          *ofo_rate_get_dataset     ( const ofoDossier *dossier );
ofoRate        *ofo_rate_get_by_mnemo    ( const ofoDossier *dossier, const gchar *mnemo );

ofoRate        *ofo_rate_new             ( void );

const gchar    *ofo_rate_get_mnemo       ( const ofoRate *rate );
const gchar    *ofo_rate_get_label       ( const ofoRate *rate );
const gchar    *ofo_rate_get_notes       ( const ofoRate *rate );
const gchar    *ofo_rate_get_upd_user    ( const ofoRate *rate );
const GTimeVal *ofo_rate_get_upd_stamp   ( const ofoRate *rate );
const GDate    *ofo_rate_get_min_valid   ( const ofoRate *rate );
const GDate    *ofo_rate_get_max_valid   ( const ofoRate *rate );
gint            ofo_rate_get_val_count   ( const ofoRate *rate );
const GDate    *ofo_rate_get_val_begin   ( const ofoRate *rate, gint idx );
const GDate    *ofo_rate_get_val_end     ( const ofoRate *rate, gint idx );
gdouble         ofo_rate_get_val_rate    ( const ofoRate *rate, gint idx );
gdouble         ofo_rate_get_rate_at_date( const ofoRate *rate, const GDate *date );

gboolean        ofo_rate_is_deletable    ( const ofoRate *rate );
gboolean        ofo_rate_is_valid        ( const gchar *mnemo, const gchar *label, GList *validities );

void            ofo_rate_set_mnemo       ( ofoRate *rate, const gchar *number );
void            ofo_rate_set_label       ( ofoRate *rate, const gchar *label );
void            ofo_rate_set_notes       ( ofoRate *rate, const gchar *notes );
void            ofo_rate_free_all_val    ( ofoRate *rate );
void            ofo_rate_add_val         ( ofoRate *rate, const GDate *begin, const GDate *end, gdouble value );

gboolean        ofo_rate_insert          ( ofoRate *rate );
gboolean        ofo_rate_update          ( ofoRate *rate, const gchar *prev_mnemo );
gboolean        ofo_rate_delete          ( ofoRate *rate );

GSList         *ofo_rate_get_csv         ( const ofoDossier *dossier );
void            ofo_rate_import_csv      ( const ofoDossier *dossier, GSList *lines, gboolean with_header );

G_END_DECLS

#endif /* __OFO_RATE_H__ */
