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

#include "ui/ofa-sgbd.h"
#include "ui/ofo-base.h"

G_BEGIN_DECLS

#define OFO_TYPE_JOURNAL                ( ofo_journal_get_type())
#define OFO_JOURNAL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_JOURNAL, ofoJournal ))
#define OFO_JOURNAL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_JOURNAL, ofoJournalClass ))
#define OFO_IS_JOURNAL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_JOURNAL ))
#define OFO_IS_JOURNAL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_JOURNAL ))
#define OFO_JOURNAL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_JOURNAL, ofoJournalClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass            parent;
}
	ofoJournalClass;

typedef struct _ofoJournalPrivate       ofoJournalPrivate;

typedef struct {
	/*< private >*/
	ofoBase            parent;
	ofoJournalPrivate *priv;
}
	ofoJournal;

GType        ofo_journal_get_type     ( void );

ofoJournal  *ofo_journal_new          ( void );

GList       *ofo_journal_load_set     ( ofaSgbd *sgbd );
void         ofo_journal_dump_set     ( GList *chart );

gint         ofo_journal_get_id       ( const ofoJournal *journal );
const gchar *ofo_journal_get_mnemo    ( const ofoJournal *journal );
const gchar *ofo_journal_get_label    ( const ofoJournal *journal );
const gchar *ofo_journal_get_notes    ( const ofoJournal *journal );
const GDate *ofo_journal_get_maxdate  ( const ofoJournal *journal );
const GDate *ofo_journal_get_cloture  ( const ofoJournal *journal );

void         ofo_journal_set_id       ( ofoJournal *journal, gint id );
void         ofo_journal_set_mnemo    ( ofoJournal *journal, const gchar *number );
void         ofo_journal_set_label    ( ofoJournal *journal, const gchar *label );
void         ofo_journal_set_notes    ( ofoJournal *journal, const gchar *notes );
void         ofo_journal_set_maj_user ( ofoJournal *journal, const gchar *user );
void         ofo_journal_set_maj_stamp( ofoJournal *journal, const GTimeVal *stamp );
void         ofo_journal_set_maxdate  ( ofoJournal *journal, const GDate *date );
void         ofo_journal_set_cloture  ( ofoJournal *journal, const GDate *date );

gboolean     ofo_journal_insert       ( ofoJournal *journal, ofaSgbd *sgbd, const gchar *user );
gboolean     ofo_journal_update       ( ofoJournal *journal, ofaSgbd *sgbd, const gchar *user );
gboolean     ofo_journal_delete       ( ofoJournal *journal, ofaSgbd *sgbd, const gchar *user );

G_END_DECLS

#endif /* __OFO_JOURNAL_H__ */
