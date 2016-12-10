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
 * the #ofaDossierCollection 'changed' signal.
 *
 * The #ofaDossierStore is managed as a singleton: the first
 * instanciation actually builds the store, while next only returns a
 * new reference on this same instance.
 * The #ofaApplication take ownership on this singleton so that it is
 * always available during the run.
 *
 * The class provides the following signals:
 *    +-----------+-------------------------+
 *    | Signal    | when                    |
 *    +-----------+-------------------------+
 *    | changed   | the content has changed |
 *    +-----------+-------------------------+
 */

#include "api/ofa-dossier-collection.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_STORE                ( ofa_dossier_store_get_type())
#define OFA_DOSSIER_STORE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_STORE, ofaDossierStore ))
#define OFA_DOSSIER_STORE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_STORE, ofaDossierStoreClass ))
#define OFA_IS_DOSSIER_STORE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_STORE ))
#define OFA_IS_DOSSIER_STORE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_STORE ))
#define OFA_DOSSIER_STORE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_STORE, ofaDossierStoreClass ))

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
 * The columns stored in the subjacent #GtkListStore.
 *                                                                      Displayable
 *                                                             Type     in Treeviews
 *                                                             -------  ------------
 * @DOSSIER_COL_DOSNAME : dossier name                         String        Yes
 * @DOSSIER_COL_PROVNAME: dbms provider name                   String        Yes
 * @DOSSIER_COL_PERNAME : localized period label               String        Yes
 * @DOSSIER_COL_END     : end of exercice                      String        Yes
 * @DOSSIER_COL_BEGIN   : begin of exercice                    String        Yes
 * @DOSSIER_COL_STATUS  : localized status of the exercice     String        Yes
 * @DOSSIER_COL_CURRENT : whether the period is current        Bool           No
 * @DOSSIER_COL_META    : the #ofaIDBMeta object               GObject        No
 * @DOSSIER_COL_PERIOD  : the #ofaIDBPeriod object             GObject        No
 */
enum {
	DOSSIER_COL_DOSNAME = 0,
	DOSSIER_COL_PROVNAME,
	DOSSIER_COL_PERNAME,
	DOSSIER_COL_END,
	DOSSIER_COL_BEGIN,
	DOSSIER_COL_STATUS,
	DOSSIER_COL_CURRENT,
	DOSSIER_COL_META,
	DOSSIER_COL_PERIOD,
	DOSSIER_N_COLUMNS
};

GType            ofa_dossier_store_get_type( void );

ofaDossierStore *ofa_dossier_store_new     ( ofaDossierCollection *collection );

G_END_DECLS

#endif /* __OFA_DOSSIER_STORE_H__ */
