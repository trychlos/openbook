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

#ifndef __OFA_DOSSIER_STORE_H__
#define __OFA_DOSSIER_STORE_H__

/**
 * SECTION: dossier_store
 * @title: ofaDossierStore
 * @short_description: The ofaDossierStore class description
 * @include: ui/ofa-dossier-store.h
 *
 * The #ofaDossierStore derived from #GtkListStore. It is populated
 * with all the known dossiers and exercices at instanciation time,
 * with one row per dossier/exercice.
 *
 * The #ofaDossierStore is kept sorted in ascending alphabetical order
 * of dossier name, and descending exercice order (the most recent
 * first).
 *
 * The #ofaDossierStore maintains itself up-to-date by connecting to
 * the #ofaFileDir 'changed' signal.
 *
 * The #ofaDossierStore is managed as a singleton: the first
 * instanciation actually builds the store, while next only returns a
 * new reference on this same instance.
 * The #ofaApplication take ownership on this singleton so that it is
 * always available during the run.
 */

#include "core/ofa-file-dir.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_STORE                ( ofa_dossier_store_get_type())
#define OFA_DOSSIER_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_STORE, ofaDossierStore ))
#define OFA_DOSSIER_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_STORE, ofaDossierStoreClass ))
#define OFA_IS_DOSSIER_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_STORE ))
#define OFA_IS_DOSSIER_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_STORE ))
#define OFA_DOSSIER_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_STORE, ofaDossierStoreClass ))

typedef struct _ofaDossierStorePrivate        ofaDossierStorePrivate;

typedef struct {
	/*< public members >*/
	GtkListStore      parent;
}
	ofaDossierStore;

/**
 * ofaDossierStoreClass:
 */
typedef struct {
	/*< public members >*/
	GtkListStoreClass parent;
}
	ofaDossierStoreClass;

/**
 * ofaDossierStoreColumn:
 * @DOSSIER_COL_DOSNAME:  dossier name
 * @DOSSIER_COL_PROVNAME: dbms provider name
 * @DOSSIER_COL_DBNAME: database name
 * @DOSSIER_COL_END:      end of exercice
 * @DOSSIER_COL_BEGIN:    begin of exercice
 * @DOSSIER_COL_STATUS:   localized status of the exercice
 * @DOSSIER_COL_CODE:   status code (from api/ofo-dossier.h) of the exercice
 * @DOSSIER_COL_META:     ofaIDBMeta object
 * @DOSSIER_COL_PERIOD:   ofaIDBPeriod object
 *
 * The columns stored in the subjacent #GtkListStore.
 */
typedef enum {
	DOSSIER_COL_DOSNAME = 0,
	DOSSIER_COL_PROVNAME,
	DOSSIER_COL_PERNAME,
	DOSSIER_COL_END,
	DOSSIER_COL_BEGIN,
	DOSSIER_COL_STATUS,					/* the displayable status */
	DOSSIER_COL_META,
	DOSSIER_COL_PERIOD,
	DOSSIER_N_COLUMNS
}
	ofaDossierStoreColumn;

/**
 * ofaDossierDispColumn:
 * @DOSSIER_DISP_DOSNAME:  dossier name
 * @DOSSIER_DISP_PROVNAME: dbms provider name
 * @DOSSIER_DISP_PERNAME:  period name
 * @DOSSIER_DISP_END:      end of exercice
 * @DOSSIER_DISP_BEGIN:    begin of exercice
 * @DOSSIER_DISP_STATUS:   localized status of the exercice
 *
 * The columns displayed in the views.
 */
typedef enum {
	DOSSIER_DISP_DOSNAME = 1,
	DOSSIER_DISP_PROVNAME,
	DOSSIER_DISP_PERNAME,
	DOSSIER_DISP_END,
	DOSSIER_DISP_BEGIN,
	DOSSIER_DISP_STATUS
}
	ofaDossierDispColumn;

GType            ofa_dossier_store_get_type       ( void );

ofaDossierStore *ofa_dossier_store_new            ( ofaFileDir *dir );

G_END_DECLS

#endif /* __OFA_DOSSIER_STORE_H__ */
