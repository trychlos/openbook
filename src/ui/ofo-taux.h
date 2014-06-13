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

#ifndef __OFO_TAUX_H__
#define __OFO_TAUX_H__

/**
 * SECTION: ofo_taux
 * @short_description: #ofoTaux class definition.
 * @include: ui/ofo-taux.h
 *
 * This class implements the Taux behavior.
 */

#include "ui/ofo-dossier-def.h"
#include "ui/ofo-taux-def.h"

G_BEGIN_DECLS

/**
 * sTauxVData:
 *
 * The structure used to validate all the validities of a rate.
 */
typedef struct {
	GDate   begin;
	GDate   end;
	gdouble rate;
}
	sTauxVData;

GType           ofo_taux_get_type     ( void ) G_GNUC_CONST;

GList          *ofo_taux_get_dataset  ( const ofoDossier *dossier );
ofoTaux        *ofo_taux_get_by_mnemo ( const ofoDossier *dossier, const gchar *mnemo );

ofoTaux        *ofo_taux_new          ( void );

const gchar    *ofo_taux_get_mnemo    ( const ofoTaux *taux );
const gchar    *ofo_taux_get_label    ( const ofoTaux *taux );
const gchar    *ofo_taux_get_notes    ( const ofoTaux *taux );
const gchar    *ofo_taux_get_maj_user ( const ofoTaux *taux );
const GTimeVal *ofo_taux_get_maj_stamp( const ofoTaux *taux );

const GDate    *ofo_taux_get_min_valid( const ofoTaux *taux );
const GDate    *ofo_taux_get_max_valid( const ofoTaux *taux );

void            ofo_taux_add_val      ( ofoTaux *taux, const gchar *begin, const gchar *end, const char *rate );
void            ofo_taux_free_val_all ( ofoTaux *taux );

gint            ofo_taux_get_val_count( const ofoTaux *taux );
const GDate    *ofo_taux_get_val_begin( const ofoTaux *taux, gint idx );
const GDate    *ofo_taux_get_val_end  ( const ofoTaux *taux, gint idx );
gdouble         ofo_taux_get_val_rate ( const ofoTaux *taux, gint idx );

gdouble         ofo_taux_get_val_rate_by_date( const ofoTaux *taux, const GDate *date );

gboolean        ofo_taux_is_deletable ( const ofoTaux *taux );
gboolean        ofo_taux_is_valid     ( const gchar *mnemo, const gchar *label, GList *validities );

void            ofo_taux_set_mnemo    ( ofoTaux *taux, const gchar *number );
void            ofo_taux_set_label    ( ofoTaux *taux, const gchar *label );
void            ofo_taux_set_notes    ( ofoTaux *taux, const gchar *notes );
void            ofo_taux_set_maj_user ( ofoTaux *taux, const gchar *user );
void            ofo_taux_set_maj_stamp( ofoTaux *taux, const GTimeVal *stamp );

gboolean        ofo_taux_insert       ( ofoTaux *taux );
gboolean        ofo_taux_update       ( ofoTaux *taux, const gchar *prev_mnemo );
gboolean        ofo_taux_delete       ( ofoTaux *taux );

GSList         *ofo_taux_get_csv      ( const ofoDossier *dossier );
void            ofo_taux_import_csv   ( const ofoDossier *dossier, GSList *lines, gboolean with_header );

G_END_DECLS

#endif /* __OFO_TAUX_H__ */
