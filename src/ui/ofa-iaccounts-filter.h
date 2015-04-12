/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_IACCOUNTS_FILTER_H__
#define __OFA_IACCOUNTS_FILTER_H__

/**
 * SECTION: iaccounts_filter
 * @title: ofaIAccountsFilter
 * @short_description: The IAccountsFilter Interface
 * @include: ui/ofa-iaccounts-filter.h
 *
 * The #ofaIAccountsFilter interface is implemented by #ofaAccountsFilterBin
 * class. It implements all needed methods to manage the composite widget.
 */

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IACCOUNTS_FILTER                      ( ofa_iaccounts_filter_get_type())
#define OFA_IACCOUNTS_FILTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IACCOUNTS_FILTER, ofaIAccountsFilter ))
#define OFA_IS_IACCOUNTS_FILTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IACCOUNTS_FILTER ))
#define OFA_IACCOUNTS_FILTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IACCOUNTS_FILTER, ofaIAccountsFilterInterface ))

typedef struct _ofaIAccountsFilter                     ofaIAccountsFilter;

/**
 * ofaIAccountsFilterInterface:
 *
 * This defines the interface that an #ofaIAccountsFilter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIAccountsFilter instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIAccountsFilter interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIAccountsFilter *instance );
}
	ofaIAccountsFilterInterface;

/**
 * Whether we are addressing the "From:" date or the "To:" one
 */
enum {
	IACCOUNTS_FILTER_FROM = 1,
	IACCOUNTS_FILTER_TO
};

GType        ofa_iaccounts_filter_get_type        ( void );

guint        ofa_iaccounts_filter_get_interface_last_version
                                                  ( const ofaIAccountsFilter *filter );

void         ofa_iaccounts_filter_setup_bin       ( ofaIAccountsFilter *filter,
															const gchar *xml_name,
															ofaMainWindow *main_window );

void         ofa_iaccounts_filter_set_prefs       ( ofaIAccountsFilter *filter,
															const gchar *prefs_key );

const gchar *ofa_iaccounts_filter_get_account     ( ofaIAccountsFilter *filter,
															gint who );

void         ofa_iaccounts_filter_set_account     ( ofaIAccountsFilter *filter,
															gint who,
															const gchar *account );

gboolean     ofa_iaccounts_filter_get_all_accounts( ofaIAccountsFilter *filter );

void         ofa_iaccounts_filter_set_all_accounts( ofaIAccountsFilter *filter,
															gboolean all_accounts );

gboolean     ofa_iaccounts_filter_is_valid        ( ofaIAccountsFilter *filter,
															gint who,
															gchar **message );

GtkWidget   *ofa_iaccounts_filter_get_frame_label ( ofaIAccountsFilter *filter );

GtkWidget   *ofa_iaccounts_filter_get_from_prompt ( ofaIAccountsFilter *filter );

G_END_DECLS

#endif /* __OFA_IACCOUNTS_FILTER_H__ */
