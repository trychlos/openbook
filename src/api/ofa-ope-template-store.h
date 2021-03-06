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

#ifndef __OPENBOOK_API_OFA_OPE_TEMPLATE_STORE_H__
#define __OPENBOOK_API_OFA_OPE_TEMPLATE_STORE_H__

/**
 * SECTION: ope_template_store
 * @title: ofaOpeTemplateStore
 * @short_description: The OpeTemplateStore class description
 * @include: openbook/ofa-ope-template-store.h
 *
 * The #ofaOpeTemplateStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the operation
 * templates of the dossier on first call, and stay then alive until
 * the dossier is closed.
 *
 * Once more time: there is only one #ofaOpeTemplateStore while the
 * dossier is opened. All the views are built on this store, using
 * ad-hoc filter models when needed.
 *
 * The #ofaOpeTemplateStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 *
 * See ofo-ope-template.h for a full description of the model language.
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_STORE                ( ofa_ope_template_store_get_type())
#define OFA_OPE_TEMPLATE_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStore ))
#define OFA_OPE_TEMPLATE_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStoreClass ))
#define OFA_IS_OPE_TEMPLATE_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_STORE ))
#define OFA_IS_OPE_TEMPLATE_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_STORE ))
#define OFA_OPE_TEMPLATE_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStoreClass ))

typedef struct {
	/*< public members >*/
	ofaListStore      parent;
}
	ofaOpeTemplateStore;

/**
 * ofaOpeTemplateStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass parent;
}
	ofaOpeTemplateStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                                              Displayable
 *                                                                     Type     in treeview
 *                                                                     -------  -----------
 * @OPE_TEMPLATE_COL_MNEMO         : mnemonic                          String       Yes
 * @OPE_TEMPLATE_COL_CRE_USER      : creation user                     String       Yes
 * @OPE_TEMPLATE_COL_CRE_STAMP     : creation timestamp                String       Yes
 * @OPE_TEMPLATE_COL_LABEL         : label                             String       Yes
 * @OPE_TEMPLATE_COL_LEDGER        : ledger identifier                 String       Yes
 * @OPE_TEMPLATE_COL_LEDGER_LOCKED : is ledger locked                  String       Yes
 * @OPE_TEMPLATE_COL_REF           : piece reference                   String       Yes
 * @OPE_TEMPLATE_COL_REF_LOCKED    : is reference locked               String       Yes
 * @OPE_TEMPLATE_COL_REF_MANDATORY : is reference mandatory            String       Yes
 * @OPE_TEMPLATE_COL_PAM_ROW       : row index of pma target           String       Yes
 * @OPE_TEMPLATE_COL_HAVE_TIERS    : whether has a tiers               String       Yes
 * @OPE_TEMPLATE_COL_TIERS         : tiers                             String       Yes
 * @OPE_TEMPLATE_COL_TIERS_LOCKED  : whether tiers is locked           String       Yes
 * @OPE_TEMPLATE_COL_HAVE_QPPRO    : whether has a qppro               String       Yes
 * @OPE_TEMPLATE_COL_QPPRO         : qppro                             String       Yes
 * @OPE_TEMPLATE_COL_QPPRO_LOCKED  : whether qppro is locked           String       Yes
 * @OPE_TEMPLATE_COL_HAVE_RULE     : whether has a rule                String       Yes
 * @OPE_TEMPLATE_COL_RULE          : rule                              String       Yes
 * @OPE_TEMPLATE_COL_RULE_LOCKED   : whether rule is locked            String       Yes
 * @OPE_TEMPLATE_COL_NOTES         : notes                             String       Yes
 * @OPE_TEMPLATE_COL_NOTES_PNG     : notes indicator                   Pixbuf       Yes
 * @OPE_TEMPLATE_COL_UPD_USER      : last update user                  String       Yes
 * @OPE_TEMPLATE_COL_UPD_STAMP     : last update timestamp             String       Yes
 * @OPE_TEMPLATE_COL_OBJECT        : #ofoOpeTemplate object            GObject       No
 */
enum {
	OPE_TEMPLATE_COL_MNEMO = 0,
	OPE_TEMPLATE_COL_CRE_USER,
	OPE_TEMPLATE_COL_CRE_STAMP,
	OPE_TEMPLATE_COL_LABEL,
	OPE_TEMPLATE_COL_LEDGER,
	OPE_TEMPLATE_COL_LEDGER_LOCKED,
	OPE_TEMPLATE_COL_REF,
	OPE_TEMPLATE_COL_REF_LOCKED,
	OPE_TEMPLATE_COL_REF_MANDATORY,
	OPE_TEMPLATE_COL_PAM_ROW,
	OPE_TEMPLATE_COL_HAVE_TIERS,
	OPE_TEMPLATE_COL_TIERS,
	OPE_TEMPLATE_COL_TIERS_LOCKED,
	OPE_TEMPLATE_COL_HAVE_QPPRO,
	OPE_TEMPLATE_COL_QPPRO,
	OPE_TEMPLATE_COL_QPPRO_LOCKED,
	OPE_TEMPLATE_COL_HAVE_RULE,
	OPE_TEMPLATE_COL_RULE,
	OPE_TEMPLATE_COL_RULE_LOCKED,
	OPE_TEMPLATE_COL_NOTES,
	OPE_TEMPLATE_COL_NOTES_PNG,
	OPE_TEMPLATE_COL_UPD_USER,
	OPE_TEMPLATE_COL_UPD_STAMP,
	OPE_TEMPLATE_COL_OBJECT,
	OPE_TEMPLATE_N_COLUMNS
};

GType                ofa_ope_template_store_get_type    ( void );

ofaOpeTemplateStore *ofa_ope_template_store_new         ( ofaIGetter *getter );

gboolean             ofa_ope_template_store_get_by_mnemo( ofaOpeTemplateStore *store,
																	const gchar *mnemo,
																	GtkTreeIter *iter );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_OPE_TEMPLATE_STORE_H__ */
