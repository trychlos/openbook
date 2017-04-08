/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ISIGNALER_H__
#define __OPENBOOK_API_OFA_ISIGNALER_H__

/**
 * SECTION: isignaler
 * @title: ofaISignaler
 * @short_description: The ISignaler Interface
 * @include: openbook/ofa-isignaler.h
 *
 * The #ofaISignaler class defines a signaling system which emits
 * dedicated messages on new, updated or deleted objects or collections,
 * as well as on application-wide or dossier-level events.
 *
 * The #ofaISignaler interface is the instance everyone may connect to
 * in order to be advertized of some application-wide events.
 *
 * The architecture of the application makes sure that this instance is
 * available right after the #ofaHub has been initialized.
 */

#include <glib-object.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISIGNALER                      ( ofa_isignaler_get_type())
#define OFA_ISIGNALER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISIGNALER, ofaISignaler ))
#define OFA_IS_ISIGNALER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISIGNALER ))
#define OFA_ISIGNALER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISIGNALER, ofaISignalerInterface ))

typedef struct _ofaISignaler                    ofaISignaler;

/**
 * ofaISignalerInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaISignaler should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint        ( *get_interface_version )( void );

	/*** instance-wide ***/
}
	ofaISignalerInterface;

/**
 * Signals defined here:
 */
#define SIGNALER_BASE_NEW                   "ofa-signaler-base-new"
#define SIGNALER_BASE_UPDATED               "ofa-signaler-base-updated"
#define SIGNALER_BASE_IS_DELETABLE          "ofa-signaler-base-is-deletable"
#define SIGNALER_BASE_DELETED               "ofa-signaler-base-deleted"
#define SIGNALER_COLLECTION_RELOAD          "ofa-signaler-collection-reload"
#define SIGNALER_DOSSIER_OPENED             "ofa-signaler-dossier-opened"
#define SIGNALER_DOSSIER_CLOSED             "ofa-signaler-dossier-closed"
#define SIGNALER_DOSSIER_CHANGED            "ofa-signaler-dossier-changed"
#define SIGNALER_DOSSIER_PREVIEW            "ofa-signaler-dossier-preview"
#define SIGNALER_DOSSIER_PERIOD_CLOSED      "ofa-signaler-dossier-period-closed"
#define SIGNALER_EXERCICE_DATES_CHANGED     "ofa-signaler-exercice-dates-changed"
#define SIGNALER_STATUS_COUNT               "ofa-signaler-entry-status-count"
#define SIGNALER_STATUS_CHANGE              "ofa-signaler-entry-status-change"

#define SIGNALER_MENU_AVAILABLE             "ofa-signaler-menu-available"
#define SIGNALER_PAGE_MANAGER_AVAILABLE     "ofa-signaler-page-manager-available"

/*
 * Interface-wide
 */
GType       ofa_isignaler_get_type                  ( void );

guint       ofa_isignaler_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint       ofa_isignaler_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void        ofa_isignaler_init_signaling_system     ( ofaISignaler *signaler,
															ofaIGetter *getter );

ofaIGetter *ofa_isignaler_get_getter                ( ofaISignaler *signaler );

void        ofa_isignaler_disconnect_handlers       ( ofaISignaler *signaler,
															GList **handlers );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISIGNALER_H__ */
