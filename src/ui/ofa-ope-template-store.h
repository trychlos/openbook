/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_OPE_TEMPLATE_STORE_H__
#define __OFA_OPE_TEMPLATE_STORE_H__

/**
 * SECTION: ope_template_store
 * @title: ofaOpeTemplateStore
 * @short_description: The OpeTemplateStore class description
 * @include: ui/ofa-ope-template-store.h
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
 * See api/ofo-ope-template.h for a full description of the model language.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-list-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_STORE                ( ofa_ope_template_store_get_type())
#define OFA_OPE_TEMPLATE_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStore ))
#define OFA_OPE_TEMPLATE_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStoreClass ))
#define OFA_IS_OPE_TEMPLATE_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_STORE ))
#define OFA_IS_OPE_TEMPLATE_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_STORE ))
#define OFA_OPE_TEMPLATE_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStoreClass ))

typedef struct _ofaOpeTemplateStorePrivate         ofaOpeTemplateStorePrivate;

typedef struct {
	/*< public members >*/
	ofaListStore                parent;

	/*< private members >*/
	ofaOpeTemplateStorePrivate *priv;
}
	ofaOpeTemplateStore;

/**
 * ofaOpeTemplateStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass           parent;
}
	ofaOpeTemplateStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 */
enum {
	OPE_TEMPLATE_COL_MNEMO = 0,
	OPE_TEMPLATE_COL_LABEL,
	OPE_TEMPLATE_COL_LEDGER,
	OPE_TEMPLATE_COL_NOTES,
	OPE_TEMPLATE_COL_UPD_USER,
	OPE_TEMPLATE_COL_UPD_STAMP,
	OPE_TEMPLATE_COL_OBJECT,
	OPE_TEMPLATE_N_COLUMNS
};

GType                ofa_ope_template_store_get_type    ( void );

ofaOpeTemplateStore *ofa_ope_template_store_new         ( ofaHub *hub );

gboolean             ofa_ope_template_store_get_by_mnemo( ofaOpeTemplateStore *store,
																	const gchar *mnemo,
																	GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_STORE_H__ */
