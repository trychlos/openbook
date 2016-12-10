/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_EXERCICE_STORE_H__
#define __OFA_EXERCICE_STORE_H__

/**
 * SECTION: ofa_exercice_store
 * @short_description: #ofaExerciceStore class definition.
 * @include: ui/ofa-exercice-store.h
 *
 * This class manages a GtkListStore -derived class which stores the
 * exercices available on a dossier.
 */

#include "api/ofa-idbdossier-meta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_STORE                ( ofa_exercice_store_get_type())
#define OFA_EXERCICE_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_STORE, ofaExerciceStore ))
#define OFA_EXERCICE_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_STORE, ofaExerciceStoreClass ))
#define OFA_IS_EXERCICE_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_STORE ))
#define OFA_IS_EXERCICE_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_STORE ))
#define OFA_EXERCICE_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_STORE, ofaExerciceStoreClass ))

typedef struct {
	/*< public members >*/
	GtkListStore      parent;
}
	ofaExerciceStore;

typedef struct {
	/*< public members >*/
	GtkListStoreClass parent;
}
	ofaExerciceStoreClass;

/**
 * The columns stored in the subjacent #GtkListStore.
 *                                                              Type     Displayable
 *                                                              -------  -----------
 * @EXERCICE_COL_STATUS: localized status string                String       Yes
 * @EXERCICE_COL_BEGIN : begin of exercice                      String       Yes
 * @EXERCICE_COL_END   : end of exercice                        String       Yes
 * @EXERCICE_COL_LABEL : localized exercice description string  String       Yes
 * @EXERCICE_COL_PERIOD: ofaIDBPeriod object                    GObject       No
 */
enum {
	EXERCICE_COL_STATUS = 0,
	EXERCICE_COL_BEGIN,
	EXERCICE_COL_END,
	EXERCICE_COL_LABEL,
	EXERCICE_COL_PERIOD,
	EXERCICE_N_COLUMNS
};

GType             ofa_exercice_store_get_type    ( void ) G_GNUC_CONST;

ofaExerciceStore *ofa_exercice_store_new         ( void );

void              ofa_exercice_store_set_dossier ( ofaExerciceStore *combo,
															ofaIDBDossierMeta *meta );

G_END_DECLS

#endif /* __OFA_EXERCICE_STORE_H__ */
