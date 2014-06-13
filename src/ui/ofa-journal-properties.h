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

#ifndef __OFA_JOURNAL_PROPERTIES_H__
#define __OFA_JOURNAL_PROPERTIES_H__

/**
 * SECTION: ofa_journal_properties
 * @short_description: #ofaJournalProperties class definition.
 * @include: ui/ofa-journal-properties.h
 *
 * Update the journal properties.
 */

#include "ui/my-dialog.h"
#include "ui/ofo-journal-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_JOURNAL_PROPERTIES                ( ofa_journal_properties_get_type())
#define OFA_JOURNAL_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_JOURNAL_PROPERTIES, ofaJournalProperties ))
#define OFA_JOURNAL_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_JOURNAL_PROPERTIES, ofaJournalPropertiesClass ))
#define OFA_IS_JOURNAL_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_JOURNAL_PROPERTIES ))
#define OFA_IS_JOURNAL_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_JOURNAL_PROPERTIES ))
#define OFA_JOURNAL_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_JOURNAL_PROPERTIES, ofaJournalPropertiesClass ))

typedef struct _ofaJournalPropertiesPrivate        ofaJournalPropertiesPrivate;

typedef struct {
	/*< private >*/
	myDialog                     parent;
	ofaJournalPropertiesPrivate *private;
}
	ofaJournalProperties;

typedef struct {
	/*< private >*/
	myDialogClass parent;
}
	ofaJournalPropertiesClass;

GType    ofa_journal_properties_get_type( void ) G_GNUC_CONST;

gboolean ofa_journal_properties_run     ( ofaMainWindow *parent, ofoJournal *journal );

G_END_DECLS

#endif /* __OFA_JOURNAL_PROPERTIES_H__ */
