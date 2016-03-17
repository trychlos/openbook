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

#ifndef __OPENBOOK_API_OFA_IDBMODEL_H__
#define __OPENBOOK_API_OFA_IDBMODEL_H__

/**
 * SECTION: idbmodel
 * @title: ofaIDBModel
 * @short_description: The DMBS Customer Interface
 * @include: openbook/ofa-idbmodel.h
 *
 * This #ofaIDBModel lets a plugin announces that it makes use of
 * the DBMS. More precisely, it lets the plugin update the DDL model.
 * In other words, this interface must be implemented by any code
 * which may want update the DB model through a suitable UI.
 */

#include "my-iwindow.h"
#include "ofa-hub-def.h"
#include "ofa-idbconnect.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBMODEL                      ( ofa_idbmodel_get_type())
#define OFA_IDBMODEL( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBMODEL, ofaIDBModel ))
#define OFA_IS_IDBMODEL( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBMODEL ))
#define OFA_IDBMODEL_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBMODEL, ofaIDBModelInterface ))

typedef struct _ofaIDBModel                    ofaIDBModel;

/**
 * ofaIDBModelInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @get_name: [should]: return an identification name.
 * @get_current_version: [should]: return the current version of the DB model.
 * @get_last_version: [should]: return the last version of the DB model.
 * @connect_handlers: [may]: let connect to the hub signaling system.
 * @needs_update: [should]: returns whether the DB model needs an update.
 * @ddl_update: [should]: returns whether the DB model has been successfully updated.
 *
 * This defines the interface that an #ofaIDBModel may/should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBModel provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDBModel interface.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint         ( *get_interface_version )( const ofaIDBModel *instance );

	/**
	 * get_name:
	 * @instance: the #ofaIDBModel provider.
	 *
	 * Returns: the identification name of the @instance.
	 *
	 * Since: version 1
	 */
	const gchar * ( *get_name )             ( const ofaIDBModel *instance );

	/**
	 * get_current_version:
	 * @instance: the #ofaIDBModel provider.
	 * @connect: the #ofaIDBConnect connection object.
	 *
	 * Returns: the current version of the DB model.
	 *
	 * If not implemented, the DB model is expected not require any
	 * DDL update.
	 *
	 * Since: version 1
	 */
	guint         ( *get_current_version )  ( const ofaIDBModel *instance,
													const ofaIDBConnect *connect );

	/**
	 * get_last_version:
	 * @instance: the #ofaIDBModel provider.
	 * @connect: the #ofaIDBConnect connection object.
	 *
	 * Returns: the last available version of the DB model.
	 *
	 * If not implemented, the DB model is expected not require any
	 * DDL update.
	 *
	 * Since: version 1
	 */
	guint         ( *get_last_version )     ( const ofaIDBModel *instance,
													const ofaIDBConnect *connect );

	/**
	 * connect_handlers:
	 * @instance: the #ofaIDBModel provider.
	 * @hub: the #ofaHub object.
	 *
	 * Let the implementation connect to the hub signaling system.
	 *
	 * Since: version 1
	 */
	void          ( *connect_handlers )     ( const ofaIDBModel *instance,
													ofaHub *hub );

	/**
	 * needs_update:
	 * @instance: the #ofaIDBModel provider.
	 * @connect: the #ofaIDBConnect connection object.
	 *
	 * Returns: %TRUE if the DB model needs an update, %FALSE else.
	 *
	 * Defaults to %FALSE.
	 *
	 * Since: version 1
	 */
	gboolean      ( *needs_update )         ( const ofaIDBModel *instance,
													const ofaIDBConnect *connect );

	/**
	 * ddl_update:
	 * @instance: the #ofaIDBModel provider.
	 * @hub: the #ofaHub instance which manages the connection
	 *  (required to be able to import files to collections).
	 * @window: the #myIWindow which displays the update.
	 *
	 * Returns: %TRUE if the DB model has been successfully updated,
	 * %FALSE else.
	 *
	 * Defaults to %TRUE.
	 *
	 * Since: version 1
	 */
	gboolean      ( *ddl_update )           ( const ofaIDBModel *instance,
													ofaHub *hub,
													myIWindow *window );
}
	ofaIDBModelInterface;

GType        ofa_idbmodel_get_type                  ( void );

guint        ofa_idbmodel_get_interface_last_version( void );

guint        ofa_idbmodel_get_interface_version     ( const ofaIDBModel *instance );

gboolean     ofa_idbmodel_update                    ( ofaHub *hub );

void         ofa_idbmodel_init_hub_signaling_system ( ofaHub *hub );

ofaIDBModel *ofa_idbmodel_get_by_name               ( const gchar *name );

guint        ofa_idbmodel_get_current_version       ( const ofaIDBModel *instance,
															const ofaIDBConnect *connect );

guint        ofa_idbmodel_get_last_version          ( const ofaIDBModel *instance,
															const ofaIDBConnect *connect );

void         ofa_idbmodel_add_row_widget            ( const ofaIDBModel *instance,
															myIWindow *window,
															GtkWidget *widget );

void         ofa_idbmodel_add_text                  ( const ofaIDBModel *instance,
															myIWindow *window,
															const gchar *text );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBMODEL_H__ */
