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

#ifndef __OFA_IACCOUNT_FILTER_H__
#define __OFA_IACCOUNT_FILTER_H__

/**
 * SECTION: iaccount_filter
 * @title: ofaIAccountFilter
 * @short_description: The IAccountFilter Interface
 * @include: ui/ofa-iaccount-filter.h
 *
 * The #ofaIAccountFilter interface is implemented by #ofaAccountFilterBin
 * class. It implements all needed methods to manage the composite widget.
 */

#include <glib-object.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IACCOUNT_FILTER                      ( ofa_iaccount_filter_get_type())
#define OFA_IACCOUNT_FILTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IACCOUNT_FILTER, ofaIAccountFilter ))
#define OFA_IS_IACCOUNT_FILTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IACCOUNT_FILTER ))
#define OFA_IACCOUNT_FILTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IACCOUNT_FILTER, ofaIAccountFilterInterface ))

typedef struct _ofaIAccountFilter                     ofaIAccountFilter;

/**
 * ofaIAccountFilterInterface:
 *
 * This defines the interface that an #ofaIAccountFilter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIAccountFilter instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIAccountFilter interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIAccountFilter *instance );
}
	ofaIAccountFilterInterface;

/**
 * Whether we are addressing the "From:" date or the "To:" one
 */
enum {
	IACCOUNT_FILTER_FROM = 1,
	IACCOUNT_FILTER_TO
};

GType        ofa_iaccount_filter_get_type                  ( void );

guint        ofa_iaccount_filter_get_interface_last_version( void );

guint        ofa_iaccount_filter_get_interface_version     ( const ofaIAccountFilter *filter );

void         ofa_iaccount_filter_setup_bin                 ( ofaIAccountFilter *filter,
																ofaIGetter *getter,
																const gchar *resource_name );

const gchar *ofa_iaccount_filter_get_account               ( const ofaIAccountFilter *filter,
																gint who );

void         ofa_iaccount_filter_set_account               ( ofaIAccountFilter *filter,
																gint who,
																const gchar *account );

gboolean     ofa_iaccount_filter_get_all_accounts          ( const ofaIAccountFilter *filter );

void         ofa_iaccount_filter_set_all_accounts          ( ofaIAccountFilter *filter,
																gboolean all_accounts );

gboolean     ofa_iaccount_filter_is_valid                  ( const ofaIAccountFilter *filter,
																gint who,
																gchar **message );

GtkWidget   *ofa_iaccount_filter_get_frame_label           ( const ofaIAccountFilter *filter );

GtkWidget   *ofa_iaccount_filter_get_from_prompt           ( const ofaIAccountFilter *filter );

G_END_DECLS

#endif /* __OFA_IACCOUNT_FILTER_H__ */
