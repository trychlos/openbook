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

#include "api/ofa-idbmeta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_STORE                ( ofa_exercice_store_get_type())
#define OFA_EXERCICE_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_STORE, ofaExerciceStore ))
#define OFA_EXERCICE_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_STORE, ofaExerciceStoreClass ))
#define OFA_IS_EXERCICE_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_STORE ))
#define OFA_IS_EXERCICE_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_STORE ))
#define OFA_EXERCICE_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_STORE, ofaExerciceStoreClass ))

typedef struct _ofaExerciceStorePrivate        ofaExerciceStorePrivate;

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
 * ofaExerciceStoreColumn:
 * @EXERCICE_COL_STATUS: localized status string
 * @EXERCICE_COL_BEGIN:  begin of exercice
 * @EXERCICE_COL_END:    end of exercice
 * @EXERCICE_COL_LABEL:  localized exercice description string
 * @EXERCICE_COL_PERIOD: ofaIDBPeriod object
 *
 * The identifiers of the columns stored in the subjacent #GtkListStore.
 */
typedef enum {
	EXERCICE_COL_STATUS = 0,
	EXERCICE_COL_BEGIN,
	EXERCICE_COL_END,
	EXERCICE_COL_LABEL,
	EXERCICE_COL_PERIOD,
	EXERCICE_N_COLUMNS
}
	ofaExerciceStoreColumn;

/**
 * ofaExerciceDispColumn:
 * @EXERCICE_DISP_STATUS: localized status string
 * @EXERCICE_DISP_BEGIN:  begin of exercice
 * @EXERCICE_DISP_END:    end of exercice
 * @EXERCICE_DISP_LABEL:  localized exercice description string
 *
 * The columns displayed in the views.
 */
typedef enum {
	EXERCICE_DISP_STATUS    = 1 << 0,
	EXERCICE_DISP_BEGIN     = 1 << 1,
	EXERCICE_DISP_END       = 1 << 2,
	EXERCICE_DISP_LABEL     = 1 << 3,
}
	ofaExerciceDispColumn;

GType             ofa_exercice_store_get_type    ( void ) G_GNUC_CONST;

ofaExerciceStore *ofa_exercice_store_new         ( void );

void              ofa_exercice_store_set_dossier ( ofaExerciceStore *combo,
															ofaIDBMeta *meta );

G_END_DECLS

#endif /* __OFA_EXERCICE_STORE_H__ */
