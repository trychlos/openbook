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

#ifndef __OFO_JOURNAL_H__
#define __OFO_JOURNAL_H__

/**
 * SECTION: ofo_journal
 * @short_description: #ofoJournal class definition.
 * @include: ui/ofo-journal.h
 *
 * This class implements the Journal behavior, including the general
 * DB definition.
 *
 * Initialization with:
 * load data local infile '/home/pierre/data/pierre@wieser.fr@cloud.trychlos.org/GTD-TR/OLA01 - Specifications/Plan comptable notarial 1988 simplié.csv' into table OFA_T_COMPTES fields terminated by ';' (@dummy,CPT_NUMBER,CPT_LABEL,CPT_NOTES);
 */

#include "ui/ofo-dossier-def.h"
#include "ui/ofo-journal-def.h"

G_BEGIN_DECLS

GType           ofo_journal_get_type     ( void ) G_GNUC_CONST;

GList          *ofo_journal_get_dataset  ( const ofoDossier *dossier );
ofoJournal     *ofo_journal_get_by_mnemo ( const ofoDossier *dossier, const gchar *mnemo );
gboolean        ofo_journal_use_devise   ( const ofoDossier *dossier, const gchar *devise );

ofoJournal     *ofo_journal_new          ( void );

const gchar    *ofo_journal_get_mnemo    ( const ofoJournal *journal );
const gchar    *ofo_journal_get_label    ( const ofoJournal *journal );
const gchar    *ofo_journal_get_notes    ( const ofoJournal *journal );
const gchar    *ofo_journal_get_maj_user ( const ofoJournal *journal );
const GTimeVal *ofo_journal_get_maj_stamp( const ofoJournal *journal );

const GDate    *ofo_journal_get_last_entry  ( const ofoJournal *journal );
const GDate    *ofo_journal_get_last_closing( const ofoJournal *journal );

gdouble         ofo_journal_get_clo_deb  ( const ofoJournal *journal, gint exe_id, const gchar *devise );
gdouble         ofo_journal_get_clo_cre  ( const ofoJournal *journal, gint exe_id, const gchar *devise );
gdouble         ofo_journal_get_deb      ( const ofoJournal *journal, gint exe_id, const gchar *devise );
gdouble         ofo_journal_get_cre      ( const ofoJournal *journal, gint exe_id, const gchar *devise );

const GDate    *ofo_journal_get_cloture  ( const ofoJournal *journal, gint exe_id );

gboolean        ofo_journal_has_entries  ( const ofoJournal *journal );
gboolean        ofo_journal_is_deletable ( const ofoJournal *journal, const ofoDossier *dossier );
gboolean        ofo_journal_is_valid     ( const gchar *mnemo, const gchar *label );

void            ofo_journal_set_mnemo    ( ofoJournal *journal, const gchar *number );
void            ofo_journal_set_label    ( ofoJournal *journal, const gchar *label );
void            ofo_journal_set_notes    ( ofoJournal *journal, const gchar *notes );
void            ofo_journal_set_maj_user ( ofoJournal *journal, const gchar *user );
void            ofo_journal_set_maj_stamp( ofoJournal *journal, const GTimeVal *stamp );

void            ofo_journal_set_clo_deb  ( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount );
void            ofo_journal_set_clo_cre  ( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount );
void            ofo_journal_set_deb      ( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount );
void            ofo_journal_set_cre      ( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount );

gboolean        ofo_journal_close        ( ofoJournal *journal, const GDate *closing );

gboolean        ofo_journal_insert       ( ofoJournal *journal, const ofoDossier *dossier );
gboolean        ofo_journal_update       ( ofoJournal *journal, const ofoDossier *dossier, const gchar *prev_mnemo );
gboolean        ofo_journal_delete       ( ofoJournal *journal, const ofoDossier *dossier );

GSList         *ofo_journal_get_csv      ( const ofoDossier *dossier );
void            ofo_journal_import_csv   ( const ofoDossier *dossier, GSList *lines, gboolean with_header );

G_END_DECLS

#endif /* __OFO_JOURNAL_H__ */
