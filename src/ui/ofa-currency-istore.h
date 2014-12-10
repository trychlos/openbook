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

#ifndef __OFA_CURRENCY_ISTORE_H__
#define __OFA_CURRENCY_ISTORE_H__

/**
 * SECTION: currency_istore
 * @title: ofaCurrencyIStore
 * @short_description: The CurrencyIStore Interface
 * @include: ui/ofa-currency-istore.h
 *
 * The #ofaCurrencyIStore interface manages the subjacent GtkListStore
 * of currency views.
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CURRENCY_ISTORE                      ( ofa_currency_istore_get_type())
#define OFA_CURRENCY_ISTORE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_CURRENCY_ISTORE, ofaCurrencyIStore ))
#define OFA_IS_CURRENCY_ISTORE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_CURRENCY_ISTORE ))
#define OFA_CURRENCY_ISTORE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_CURRENCY_ISTORE, ofaCurrencyIStoreInterface ))

typedef struct _ofaCurrencyIStore                    ofaCurrencyIStore;

/**
 * ofaCurrencyColumns:
 * The columns stored in the subjacent #GtkListStore.
 */
typedef enum {
	CURRENCY_COL_CODE      = 1 << 0,
	CURRENCY_COL_LABEL     = 1 << 1,
	CURRENCY_COL_SYMBOL    = 1 << 2,
	CURRENCY_COL_DIGITS    = 1 << 3,
	CURRENCY_COL_NOTES     = 1 << 4,
	CURRENCY_COL_UPD_USER  = 1 << 5,
	CURRENCY_COL_UPD_STAMP = 1 << 6,
}
	ofaCurrencyColumns;

/**
 * ofaCurrencyIStoreInterface:
 *
 * This defines the interface that an #ofaCurrencyIStore should
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaCurrencyIStore provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaCurrencyIStore interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaCurrencyIStore *instance );

	/**
	 * attach_to:
	 * @instance: the #ofaCurrencyIStore instance.
	 * @parent: the #GtkContainer parent.
	 *
	 * The application must implement this method in order to attach
	 * its widget to the specified @parent.
	 */
	void  ( *attach_to)             ( ofaCurrencyIStore *instance,
												GtkContainer *parent );

	/**
	 * set_columns:
	 * @instance: the #ofaCurrencyIStore instance.
	 * @store: the underlying #GtkListStore store.
	 * @columns: the #ofaCurrencyColumns list of displayed columns.
	 *
	 * The interface code calls this method in order for the object to
	 * create the required columns to be able to display them.
	 */
	void  ( *set_columns)           ( ofaCurrencyIStore *instance,
												GtkListStore *store,
												ofaCurrencyColumns columns );
}
	ofaCurrencyIStoreInterface;

GType ofa_currency_istore_get_type         ( void );

guint ofa_currency_istore_get_interface_last_version
                                           ( const ofaCurrencyIStore *instance );

void  ofa_currency_istore_attach_to        ( ofaCurrencyIStore *instance,
													GtkContainer *parent );

void  ofa_currency_istore_set_columns      ( ofaCurrencyIStore *instance,
													ofaCurrencyColumns columns );

void  ofa_currency_istore_set_dossier      ( ofaCurrencyIStore *instance,
													ofoDossier *dossier );

gint  ofa_currency_istore_get_column_number( const ofaCurrencyIStore *instance,
													ofaCurrencyColumns column );

G_END_DECLS

#endif /* __OFA_CURRENCY_ISTORE_H__ */
