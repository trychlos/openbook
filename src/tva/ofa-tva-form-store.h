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

#ifndef __OFA_TVA_FORM_STORE_H__
#define __OFA_TVA_FORM_STORE_H__

/**
 * SECTION: tva_form_store
 * @title: ofaTVAFormStore
 * @short_description: The TVAFormStore class definition
 * @include: tva/ofa-tva-form-store.h
 *
 * The #ofaTVAFormStore derived from #ofaListStore, which itself
 * derives from #GtkListStore. It is populated with all the TVA forms
 * defined on the dossier on first call, and stay then alive until the
 * dossier is closed.
 *
 * Once more time: there is only one #ofaTVAFormStore while the dossier
 * is opened. All the views are built on this store, using ad-hoc filter
 * models when needed.
 *
 * The #ofaTVAFormStore takes advantage of the dossier signaling
 * system to maintain itself up to date.
 */

#include "api/ofa-list-store.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_FORM_STORE                ( ofa_tva_form_store_get_type())
#define OFA_TVA_FORM_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_FORM_STORE, ofaTVAFormStore ))
#define OFA_TVA_FORM_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_FORM_STORE, ofaTVAFormStoreClass ))
#define OFA_IS_TVA_FORM_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_FORM_STORE ))
#define OFA_IS_TVA_FORM_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_FORM_STORE ))
#define OFA_TVA_FORM_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_FORM_STORE, ofaTVAFormStoreClass ))

typedef struct _ofaTVAFormStorePrivate         ofaTVAFormStorePrivate;

typedef struct {
	/*< public members >*/
	ofaListStore            parent;

	/*< private members >*/
	ofaTVAFormStorePrivate *priv;
}
	ofaTVAFormStore;

/**
 * ofaTVAFormStoreClass:
 */
typedef struct {
	/*< public members >*/
	ofaListStoreClass       parent;
}
	ofaTVAFormStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 */
enum {
	TVA_COL_MNEMO = 0,
	TVA_COL_LABEL,
	TVA_COL_NOTES,
	TVA_COL_UPD_USER,
	TVA_COL_UPD_STAMP,
	TVA_COL_OBJECT,
	TVA_N_COLUMNS
};

/**
 * ofaCurrencyColumns:
 * The columns displayed in the views.
 */
typedef enum {
	TVA_DISP_MNEMO     = 1 << 0,
	TVA_DISP_LABEL     = 1 << 1,
	TVA_DISP_NOTES     = 1 << 2,
	TVA_DISP_UPD_USER  = 1 << 3,
	TVA_DISP_UPD_STAMP = 1 << 4,
}
	ofaTVAFormColumns;

GType            ofa_tva_form_store_get_type( void );

ofaTVAFormStore *ofa_tva_form_store_new     ( ofoDossier *dossier );

G_END_DECLS

#endif /* __OFA_TVA_FORM_STORE_H__ */