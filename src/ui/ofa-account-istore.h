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

#ifndef __OFA_ACCOUNT_ISTORE_H__
#define __OFA_ACCOUNT_ISTORE_H__

/**
 * SECTION: account_istore
 * @title: ofaAccountIStore
 * @short_description: The AccountIStore Interface
 * @include: ui/ofa-account-istore.h
 *
 * The #ofaAccountIStore interface manages the subjacent GtkListStore
 * of account views.
 * As account views are built on a notebook with one page per class,
 * then it would be worth to notice that this #ofaAccountIStore only
 * manage one store (i.e. one class page).
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_ISTORE                      ( ofa_account_istore_get_type())
#define OFA_ACCOUNT_ISTORE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ACCOUNT_ISTORE, ofaAccountIStore ))
#define OFA_IS_ACCOUNT_ISTORE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ACCOUNT_ISTORE ))
#define OFA_ACCOUNT_ISTORE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ACCOUNT_ISTORE, ofaAccountIStoreInterface ))

typedef struct _ofaAccountIStore                     ofaAccountIStore;

/**
 * ofaAccountColumns:
 * The columns stored in the subjacent #GtkListStore.
 */
typedef enum {
	ACCOUNT_COL_NUMBER        = 1 <<  0,
	ACCOUNT_COL_LABEL         = 1 <<  1,
	ACCOUNT_COL_CURRENCY      = 1 <<  2,
	ACCOUNT_COL_TYPE          = 1 <<  3,
	ACCOUNT_COL_NOTES         = 1 <<  4,
	ACCOUNT_COL_UPD_USER      = 1 <<  5,
	ACCOUNT_COL_UPD_STAMP     = 1 <<  6,
	ACCOUNT_COL_VAL_DEBIT     = 1 <<  7,
	ACCOUNT_COL_VAL_CREDIT    = 1 <<  8,
	ACCOUNT_COL_ROUGH_DEBIT   = 1 <<  9,
	ACCOUNT_COL_ROUGH_CREDIT  = 1 << 10,
	ACCOUNT_COL_OPEN_DEBIT    = 1 << 11,
	ACCOUNT_COL_OPEN_CREDIT   = 1 << 12,
	ACCOUNT_COL_FUT_DEBIT     = 1 << 13,
	ACCOUNT_COL_FUT_CREDIT    = 1 << 14,
	ACCOUNT_COL_SETTLEABLE    = 1 << 15,
	ACCOUNT_COL_RECONCILIABLE = 1 << 16,
	ACCOUNT_COL_FORWARD       = 1 << 17,
	ACCOUNT_COL_EXE_DEBIT     = 1 << 18,		/* exercice = val+rough */
	ACCOUNT_COL_EXE_CREDIT    = 1 << 19,
}
	ofaAccountColumns;

/**
 * ofaAccountIStoreInterface:
 *
 * This defines the interface that an #ofaAccountIStore should
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaAccountIStore provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaAccountIStore interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaAccountIStore *instance );

	/**
	 * attach_to:
	 * @instance: the #ofaAccountIStore instance.
	 * @parent: the #GtkContainer parent.
	 *
	 * The application must implement this method in order to attach
	 * its widget to the specified @parent.
	 */
	void  ( *attach_to)             ( ofaAccountIStore *instance,
												GtkContainer *parent );

	/**
	 * set_columns:
	 * @instance: the #ofaAccountIStore instance.
	 * @store: the underlying #GtkTreeStore store.
	 * @columns: the #ofaAccountColumns list of displayed columns.
	 *
	 * The interface code calls this method in order for the object to
	 * create the required columns to be able to display them.
	 */
	void  ( *set_columns)           ( ofaAccountIStore *instance,
												GtkTreeStore *store,
												ofaAccountColumns columns );
}
	ofaAccountIStoreInterface;

GType ofa_account_istore_get_type         ( void );

guint ofa_account_istore_get_interface_last_version
                                           ( const ofaAccountIStore *instance );

void  ofa_account_istore_attach_to        ( ofaAccountIStore *instance,
													GtkContainer *parent );

void  ofa_account_istore_set_columns      ( ofaAccountIStore *instance,
													ofaAccountColumns columns );

void  ofa_account_istore_set_dossier      ( ofaAccountIStore *instance,
													ofoDossier *dossier );

gint  ofa_account_istore_get_column_number( const ofaAccountIStore *instance,
													ofaAccountColumns column );

G_END_DECLS

#endif /* __OFA_ACCOUNT_ISTORE_H__ */
