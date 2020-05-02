/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFA_RECONCIL_PAGE_H__
#define __OFA_RECONCIL_PAGE_H__

/**
 * SECTION: ofa_reconciliation
 * @short_description: #ofaReconcilPage class definition.
 * @include: core/ofa-reconcil-page.h
 *
 * Display both entries from an account and a Bank Account Transaction
 * list, letting user reconciliate balanced lines.
 * No matter in which order the user selects the account and the BAT
 * file(s), the code handles things so that entries are always displayed
 * before BAT lines.
 *
 * This is displayed as a tree view per - may be proposed - conciliation
 * groups:
 *
 * - the first row of a conciliation group is at level zero,
 *   other rows of this same conciliation group being children of the
 *   first row - where row here may be either an entry or a BAT line
 *
 *   each time it is possible, the code tries to have an entry as the
 *   first row (because it is more readable and intuitive for the user)
 *   but this cannot be forced because:
 *   . a conciliation group may contain only BAT lines
 *   . only BAT lines may be loaded and no entries
 *
 * - if not member of a conciliation group:
 *   . entries are at level zero
 *   . bat lines may be:
 *     . as the child of an entry if they are proposed to be reconciliated
 *       with this same entry
 *     . at the level zero if they are not proposed to any automatic
 *       conciliation.
 *
 * We so may have actual conciliation group or proposed conciliation
 * group. A proposed conciliation group will be built with an entry and
 * a BAT line.
 *
 * Activating (enter or double-click) a row is only managed when this
 * row is the first one (the parent) of an actual or proposed conciliation
 * group. When true, the state is toggled, i.e. the conciliation group is
 * removed (if was set) or created (if was proposed).
 *
 * Actions:
 * - reconciliate:
 *   . enabled when:
 *     > the selection contains an entry or points to a proposed
 *       conciliation group
 *     > and a conciliation date may be taken either from the manual
 *       reconciliation date entry, or from a selected BAT line or from
 *       a proposed BAT line
 *     > and no selected row is already member of an actual conciliation
 *       group
 *     Note that whether the reconciliation action is enabled does not
 *     depent of whether the selection is rightly balanced: it is always
 *     possible for the user to force entries to be manually
 *     reconciliated; only a confirmation may be required in this case.
 *   . does: create a new conciliation group
 *   . confirmation: if debit <> credit
 * - decline:
 *   . enabled when:
 *     > the selection contains only not conciliated bat lines which
 *       are member of a same proposed conciliation group
 *       pratically this limit to the selection of one BAT line because
 *       this is the limit of the proposal algorythm
 *   . does: cancel the proposal and move the BAT line to level zero
 *   . confirmation: no
 * - unreconciliate:
 *   . enabled when:
 *   . does:
 *   . confirmation:
 *
 * It is possible to import a bat file which concerns already manually
 * reconciliated entries.
 * Imported bat lines will not be proposed against the right entry as
 * this later is already reconciliated. It is so possible to manually
 * select the already reconciliated entry with the to-be reconciliated
 * bat line and to 'Accept' the reconciliation. The batline will be
 * added to the entry reconciliation group.
 *
 * The OFA_T_CONCIL table records the conciliation groups, gathering
 * them by concil identifier.
 */

#include "api/ofa-paned-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCIL_PAGE                ( ofa_reconcil_page_get_type())
#define OFA_RECONCIL_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_PAGE, ofaReconcilPage ))
#define OFA_RECONCIL_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_PAGE, ofaReconcilPageClass ))
#define OFA_IS_RECONCIL_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_PAGE ))
#define OFA_IS_RECONCIL_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_PAGE ))
#define OFA_RECONCIL_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_PAGE, ofaReconcilPageClass ))

typedef struct {
	/*< public members >*/
	ofaPanedPage      parent;
}
	ofaReconcilPage;

typedef struct {
	/*< public members >*/
	ofaPanedPageClass parent;
}
	ofaReconcilPageClass;

GType ofa_reconcil_page_get_type   ( void ) G_GNUC_CONST;

void  ofa_reconcil_page_set_account( ofaReconcilPage *page,
											const gchar *number );

G_END_DECLS

#endif /* __OFA_RECONCIL_PAGE_H__ */
