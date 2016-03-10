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

#ifndef __OFA_IACCOUNT_ENTRY_H__
#define __OFA_IACCOUNT_ENTRY_H__

/**
 * SECTION: iaccount_entry
 * @title: ofaIAccountEntry
 * @short_description: The IAccountEntry interface
 * @include: ui/ofa-iaccount-entry.h
 *
 * The #ofaIAccountEntry interface lets the user enter and select
 * accounts in the provided GtkEntry.
 *
 * Just call #ofa_iaccount_entry_init() with each GtkEntry you want
 * set, and the function will take care of setting an icon, triggering
 * #ofaAccountSelect dialog for account selection.
 */

#include "api/ofa-main-window-def.h"
#include "api/ofo-account.h"

G_BEGIN_DECLS

#define OFA_TYPE_IACCOUNT_ENTRY                      ( ofa_iaccount_entry_get_type())
#define OFA_IACCOUNT_ENTRY( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IACCOUNT_ENTRY, ofaIAccountEntry ))
#define OFA_IS_IACCOUNT_ENTRY( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IACCOUNT_ENTRY ))
#define OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IACCOUNT_ENTRY, ofaIAccountEntryInterface ))

typedef struct _ofaIAccountEntry                     ofaIAccountEntry;

/**
 * ofaIAccountEntryInterface:
 * @get_interface_version: [should]: returns the version of the
 *  implemented interface.
 * @on_pre_select: [may]: modify the initial selection.
 * @on_icon_pressed: [may]: do something after the selection.
 *
 * This defines the interface that an #ofaIAccountEntry should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIAccountEntry instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIAccountEntry interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint   ( *get_interface_version )( const ofaIAccountEntry *instance );

	/**
	 * on_pre_select:
	 * @instance: the #ofaIAccountEntry instance.
	 * @entry: the #GtkEntry which embeds the pressed icon.
	 * @position: the #GtkEntryIconPosition.
	 * @allowed: the allowed selection.
	 *
	 * Called before opening the account selection dialog with the content
	 * of the @entry.
	 * The implementation may force the initial selection by returning
	 * a to-be-selected identifier as a newly allocated string which will
	 * be g_free() by the interface.
	 *
	 * If this method is not implemented, the interface will set the
	 * initial selection with the content of the @entry.
	 *
	 * When this method is implemented, the interface will set the initial
	 * selection with the returning string, whatever this string be.
	 *
	 * Since: version 1
	 */
	gchar * ( *on_pre_select )        ( ofaIAccountEntry *instance,
											GtkEntry *entry,
											GtkEntryIconPosition position,
											ofeAccountAllowed allowed );

	/**
	 * on_post_select:
	 * @instance: the #ofaIAccountEntry instance.
	 * @entry: the #GtkEntry which embeds the pressed icon.
	 * @position: the #GtkEntryIconPosition.
	 * @allowed: the allowed selection.
	 * @account_id: the selected account identifier.
	 *
	 * Let the implementation modify the selection, or do something after
	 * the selection.
	 *
	 * If this method is not implemented, or returns %NULL, then the
	 * selected @account_id will be used as the account identifier.
	 *
	 * When this method is implemented and returns something, then it
	 * must be a newly allocated string which whill be used a the
	 * selected account identifier, before being g_free() by the interface.
	 *
	 * Since: version 1
	 */
	gchar * ( *on_post_select )       ( ofaIAccountEntry *instance,
											GtkEntry *entry,
											GtkEntryIconPosition position,
											ofeAccountAllowed allowed,
											const gchar *account_id );
}
	ofaIAccountEntryInterface;

GType ofa_iaccount_entry_get_type                  ( void );

guint ofa_iaccount_entry_get_interface_last_version( void );

guint ofa_iaccount_entry_get_interface_version     ( const ofaIAccountEntry *instance );

void  ofa_iaccount_entry_init                      ( ofaIAccountEntry *instance,
															GtkEntry *entry,
															ofaMainWindow *main_window,
															ofeAccountAllowed allowed );

G_END_DECLS

#endif /* __OFA_IACCOUNT_ENTRY_H__ */
