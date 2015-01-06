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

#ifndef __OFA_DOSSIER_STORE_H__
#define __OFA_DOSSIER_STORE_H__

/**
 * SECTION: dossier_store
 * @title: ofaDossierStore
 * @short_description: The DossierStore class description
 * @include: ui/ofa-accounts-store.h
 *
 * The #ofaDossierStore derived from #GtkListStore. It is populated
 * with all the known dossiers at time of the creation.
 *
 * The #ofaDossierStore is kept sorted in ascending alphabetical order
 * of dossier name.
 */

#include <gtk/gtk.h>

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
	GtkListStore            parent;

	/*< private members >*/
	ofaDossierStorePrivate *priv;
}
	ofaDossierStore;

/**
 * ofaDossierStoreClass:
 */
typedef struct {
	/*< public members >*/
	GtkListStoreClass       parent;
}
	ofaDossierStoreClass;

/**
 * The columns stored in the subjacent #GtkDossierStore.
 */
enum {
	DOSSIER_COL_DNAME = 0,
	DOSSIER_COL_DBMS,
	DOSSIER_N_COLUMNS
};

/**
 * ofaDossierColumns:
 * The columns displayed in the views.
 */
typedef enum {
	DOSSIER_DISP_DNAME = 1 << 0,
	DOSSIER_DISP_DBMS  = 1 << 1,
}
	ofaDossierColumns;

GType            ofa_dossier_store_get_type  ( void );

ofaDossierStore *ofa_dossier_store_new       ( void );

void             ofa_dossier_store_add_row   ( ofaDossierStore *store,
														const gchar *dname,
														const gchar *dbms );

void             ofa_dossier_store_remove_row( ofaDossierStore *store,
														const gchar *dname );

G_END_DECLS

#endif /* __OFA_DOSSIER_STORE_H__ */
