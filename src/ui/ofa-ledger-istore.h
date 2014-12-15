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

#ifndef __OFA_LEDGER_ISTORE_H__
#define __OFA_LEDGER_ISTORE_H__

/**
 * SECTION: ledger_istore
 * @title: ofaLedgerIStore
 * @short_description: The LedgerIStore Interface
 * @include: ui/ofa-ledger-istore.h
 *
 * The #ofaLedgerIStore interface manages the subjacent GtkListStore
 * of ledger views.
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_ISTORE                      ( ofa_ledger_istore_get_type())
#define OFA_LEDGER_ISTORE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_LEDGER_ISTORE, ofaLedgerIStore ))
#define OFA_IS_LEDGER_ISTORE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_LEDGER_ISTORE ))
#define OFA_LEDGER_ISTORE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_LEDGER_ISTORE, ofaLedgerIStoreInterface ))

typedef struct _ofaLedgerIStore                     ofaLedgerIStore;

/**
 * ofaLedgerColumns:
 * The columns stored in the subjacent #GtkListStore.
 */
typedef enum {
	LEDGER_COL_MNEMO      = 1 << 0,
	LEDGER_COL_LABEL      = 1 << 1,
	LEDGER_COL_LAST_ENTRY = 1 << 2,
	LEDGER_COL_LAST_CLOSE = 1 << 3,
	LEDGER_COL_NOTES      = 1 << 4,
	LEDGER_COL_UPD_USER   = 1 << 5,
	LEDGER_COL_UPD_STAMP  = 1 << 6,
}
	ofaLedgerColumns;

/**
 * ofaLedgerIStoreInterface:
 *
 * This defines the interface that an #ofaLedgerIStore should
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaLedgerIStore provider.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaLedgerIStore interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaLedgerIStore *instance );

	/**
	 * attach_to:
	 * @instance: the #ofaLedgerIStore instance.
	 * @parent: the #GtkContainer parent.
	 *
	 * The application must implement this method in order to attach
	 * its widget to the specified @parent.
	 */
	void  ( *attach_to )            ( ofaLedgerIStore *instance,
												GtkContainer *parent );

	/**
	 * set_columns:
	 * @instance: the #ofaLedgerIStore instance.
	 * @store: the underlying #GtkListStore store.
	 * @columns: the #ofaLedgerColumns list of displayed columns.
	 *
	 * The interface code calls this method in order for the object to
	 * create the required columns to be able to display them.
	 */
	void  ( *set_columns )          ( ofaLedgerIStore *instance,
												GtkListStore *store,
												ofaLedgerColumns columns );
}
	ofaLedgerIStoreInterface;

GType ofa_ledger_istore_get_type         ( void );

guint ofa_ledger_istore_get_interface_last_version
                                           ( const ofaLedgerIStore *instance );

void  ofa_ledger_istore_attach_to        ( ofaLedgerIStore *instance,
													GtkContainer *parent );

void  ofa_ledger_istore_set_columns      ( ofaLedgerIStore *instance,
													ofaLedgerColumns columns );

void  ofa_ledger_istore_set_dossier      ( ofaLedgerIStore *instance,
													ofoDossier *dossier );

gint  ofa_ledger_istore_get_column_number( const ofaLedgerIStore *instance,
													ofaLedgerColumns column );

G_END_DECLS

#endif /* __OFA_LEDGER_ISTORE_H__ */
