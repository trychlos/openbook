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

#ifndef __OPENBOOK_API_OFA_IRECOVER_H__
#define __OPENBOOK_API_OFA_IRECOVER_H__

/**
 * SECTION: irecover
 * @title: ofaIRecover
 * @short_description: The IRecover Interface
 * @include: openbook/ofa-irecover.h
 *
 * This #ofaIRecover lets a plugin announces that it is able to recover
 * data from another software.
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect-def.h"
#include "api/ofa-stream-format.h"

G_BEGIN_DECLS

#define OFA_TYPE_IRECOVER                      ( ofa_irecover_get_type())
#define OFA_IRECOVER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IRECOVER, ofaIRecover ))
#define OFA_IS_IRECOVER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IRECOVER ))
#define OFA_IRECOVER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IRECOVER, ofaIRecoverInterface ))

typedef struct _ofaIRecover                    ofaIRecover;

/**
 * ofaIRecoverInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 *
 * This defines the interface that an #ofaIRecover may/should implement.
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
	guint         ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * import_uris:
	 * @instance: the #ofaIRecover provider.
	 * @hub: the #ofaHub object of the application.
	 * @uris: a #GList of #ofsRecoverFile structures to be imported.
	 * @format: the input stream format.
	 * @connect: the target connection.
	 * @msg_cb: a message callback.
	 * @msg_data: user data to be passed to @msg_cb.
	 *
	 * Import the specified @uris into the @connect target.
	 *
	 * Returns: %TRUE if the recovery was successfull, %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean      ( *import_uris )          ( ofaIRecover *instance,
													ofaHub *hub,
													GList *uris,
													ofaStreamFormat *format,
													ofaIDBConnect *connect,
													ofaMsgCb msg_cb,
													void *msg_data );
}
	ofaIRecoverInterface;

/**
 * Identifying the nature of the data
 */
enum {
	OFA_RECOVER_ENTRY = 1,
	OFA_RECOVER_ACCOUNT
};

/**
 * Identiying a source file URI
 */
typedef struct {
	guint  nature;
	gchar *uri;
}
	ofsRecoverFile;

/*
 * Interface-wide
 */
GType        ofa_irecover_get_type                  ( void );

guint        ofa_irecover_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint        ofa_irecover_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gboolean     ofa_irecover_import_uris               ( ofaIRecover *instance,
															ofaHub *hub,
															GList *uris,
															ofaStreamFormat *format,
															ofaIDBConnect *connect,
															ofaMsgCb msg_cb,
															void *msg_data );

GList       *ofa_irecover_add_file                  ( GList *uris,
															guint nature,
															const gchar *uri );

void         ofa_irecover_free_files                ( GList **uris );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IRECOVER_H__ */
