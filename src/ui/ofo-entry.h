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

#ifndef __OFO_ENTRY_H__
#define __OFO_ENTRY_H__

/**
 * SECTION: ofo_entry
 * @short_description: #ofoEntry class definition.
 * @include: ui/ofo-entry.h
 *
 * This class implements the Entry behavior.
 */

#include "ui/ofo-dossier.h"

G_BEGIN_DECLS

#define OFO_TYPE_ENTRY                ( ofo_entry_get_type())
#define OFO_ENTRY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ENTRY, ofoEntry ))
#define OFO_ENTRY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ENTRY, ofoEntryClass ))
#define OFO_IS_ENTRY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ENTRY ))
#define OFO_IS_ENTRY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ENTRY ))
#define OFO_ENTRY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ENTRY, ofoEntryClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoEntryClass;

typedef struct _ofoEntryPrivate       ofoEntryPrivate;

typedef struct {
	/*< private >*/
	ofoBase          parent;
	ofoEntryPrivate *priv;
}
	ofoEntry;

/**
 * ofaEntrySens:
 */
typedef enum {
	ENT_SENS_DEBIT = 1,
	ENT_SENS_CREDIT,
}
	ofaEntrySens;

/**
 * ofaEntryStatus:
 */
typedef enum {
	ENT_STATUS_ROUGH = 1,
	ENT_STATUS_VALIDATED,
	ENT_STATUS_DELETED
}
	ofaEntryStatus;

GType        ofo_entry_get_type( void );

ofoEntry    *ofo_entry_new     ( const GDate *effet, const GDate *ope,
									const gchar *label, const gchar *ref,
									gint dev_id, gint jou_id,
									gdouble amount, ofaEntrySens sens );

gboolean     ofo_entry_insert  ( ofoEntry *entry, ofoDossier *dossier );
gboolean     ofo_entry_validate( ofoEntry *entry, ofoDossier *dossier );
gboolean     ofo_entry_delete  ( ofoEntry *entry, ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_ENTRY_H__ */
