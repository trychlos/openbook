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

#ifndef __OFA_DOSSIER_ISTORE_H__
#define __OFA_DOSSIER_ISTORE_H__

/**
 * SECTION: dossier_istore
 * @title: ofaDossierIStore
 * @short_description: The DossierIStore Interface
 * @include: ui/ofa-dossier-istore.h
 *
 * The #ofaDossierIStore interface manages the subjacent GtkListStore
 * of dossier views.
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_ISTORE                      ( ofa_dossier_istore_get_type())
#define OFA_DOSSIER_ISTORE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_DOSSIER_ISTORE, ofaDossierIStore ))
#define OFA_IS_DOSSIER_ISTORE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_DOSSIER_ISTORE ))
#define OFA_DOSSIER_ISTORE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_DOSSIER_ISTORE, ofaDossierIStoreInterface ))

typedef struct _ofaDossierIStore                     ofaDossierIStore;

/**
 * ofaDossierColumns:
 * The columns stored in the subjacent #GtkListStore.
 */
typedef enum {
	DOSSIER_COL_DNAME     = 1 << 0,
}
	ofaDossierColumns;

/**
 * ofaDossierIStoreInterface:
 *
 * This defines the interface that an #ofaDossierIStore should
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaDossierIStore provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaDossierIStore interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaDossierIStore *instance );

	/**
	 * attach_to:
	 * @instance: the #ofaDossierIStore instance.
	 * @parent: the #GtkContainer parent.
	 *
	 * The application must implement this method in order to attach
	 * its widget to the specified @parent.
	 */
	void  ( *attach_to)             ( ofaDossierIStore *instance,
												GtkContainer *parent );

	/**
	 * set_columns:
	 * @instance: the #ofaDossierIStore instance.
	 * @store: the underlying #GtkListStore store.
	 * @columns: the #ofaDossierColumns list of displayed columns.
	 *
	 * The interface code calls this method in order for the object to
	 * create the required columns to be able to display them.
	 */
	void  ( *set_columns)           ( ofaDossierIStore *instance,
												GtkListStore *store,
												ofaDossierColumns columns );
}
	ofaDossierIStoreInterface;

GType ofa_dossier_istore_get_type         ( void );

guint ofa_dossier_istore_get_interface_last_version
                                          ( const ofaDossierIStore *instance );

void  ofa_dossier_istore_attach_to        ( ofaDossierIStore *instance,
													GtkContainer *parent );

void  ofa_dossier_istore_set_columns      ( ofaDossierIStore *instance,
													ofaDossierColumns columns );

gint  ofa_dossier_istore_get_column_number( const ofaDossierIStore *instance,
													ofaDossierColumns column );

G_END_DECLS

#endif /* __OFA_DOSSIER_ISTORE_H__ */
