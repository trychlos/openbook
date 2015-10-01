/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OFA_RECONCILIATION_H__
#define __OFA_RECONCILIATION_H__

/**
 * SECTION: ofa_reconciliation
 * @short_description: #ofaReconciliation class definition.
 * @include: ui/ofa-reconciliation.h
 *
 * Display both entries from an account and a Bank Account Transaction
 * list, letting user reconciliate balanced lines.
 *
 * This is displayed as a tree view where:
 * - entries (if any) are always at the level zero
 * - bat lines may be:
 *   . as the child of an entry if they are reconciliated with this same
 *     entry, or proposed to
 *   . at the level zero if they are not reconciliated nor may be
 *     automatically proposed to be reconciliated against any entry.
 *
 * An entry may have zero or one bat line child.
 *
 * Bat lines are inserted:
 * - preferentially as the child of an entry:
 *   . either (one of) the entry against which the bat line has been
 *     previously reconciliated
 *   . or a proposed compatible entry:
 *     > without yet any child
 *     > not yet reconciliated
 *     > with compatible amounts
 * - or at level zero if no automatic proposition can be made.
 *
 * Activating (enter or double-click) an entry row toggles its
 * reconciliation state:
 * - if the entry was reconciliated, then all the reconciliation group
 *   is undone as well
 * - if the entry was not reconciliated, then the reconciliation date
 *   is taken from:
 *   . the child bat line effect date if any, and these two lines (the
 *     entry and the bat line) are set as members of a new reconciliation
 *     group
 *   . the manual reconciliation date if set (the entry is the only
 *     member of a new reconciliation group)
 *   . else (no reconciliation date) nothing happens.
 *
 * Activating a bat line row has no effect
 *
 * Selection can be multiple in order to allow 'b' bat lines be
 * reconciliated against 'e' entries. Both b and e may be equal to
 * zero, as soon as sum(b+e)=0.
 *
 * It is possible to import a bat file which concerns already manually
 * reconciliated entries.
 * Imported bat lines will not be proposed against the right entry as
 * this later is already reconciliated. It is so possible to manually
 * select the already reconciliated entry with the to-be reconciliated
 * bat line and to 'Accept' the reconciliation. The batline will be
 * added to the entry reconciliation group.
 *
 * The automatic proposal may be wrong, and may thus be declined. The
 * corresponding bat line is moved at tree view level zero.
 *
 * The OFA_T_CONCIL table records the conciliation groups, gathering
 * them by concil identifier.
 *
 * Development rules:
 * - type:       dialog
 * - settings:   yes
 * - current:    no
 */

#include "ui/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCILIATION                ( ofa_reconciliation_get_type())
#define OFA_RECONCILIATION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCILIATION, ofaReconciliation ))
#define OFA_RECONCILIATION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCILIATION, ofaReconciliationClass ))
#define OFA_IS_RECONCILIATION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCILIATION ))
#define OFA_IS_RECONCILIATION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCILIATION ))
#define OFA_RECONCILIATION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCILIATION, ofaReconciliationClass ))

typedef struct _ofaReconciliationPrivate       ofaReconciliationPrivate;

typedef struct {
	/*< public members >*/
	ofaPage                   parent;

	/*< private members >*/
	ofaReconciliationPrivate *priv;
}
	ofaReconciliation;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaReconciliationClass;

GType ofa_reconciliation_get_type   ( void ) G_GNUC_CONST;

void  ofa_reconciliation_set_account( ofaReconciliation *page,
											const gchar *number );

G_END_DECLS

#endif /* __OFA_RECONCILIATION_H__ */
