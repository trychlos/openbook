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

#ifndef __OPENBOOK_API_OFA_IDBEDITOR_H__
#define __OPENBOOK_API_OFA_IDBEDITOR_H__

/**
 * SECTION: idbeditor
 * @title: ofaIDBEditor
 * @short_description: The DMBS New Interface
 * @include: openbook/ofa-idbeditor.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * The #ofaIDBEditor is the interface a #GtkWidget instanciated by a
 * DBMS provider should implement to let the application define a new
 * dossier. Basically, this interface is intended to only manage the
 * informations to be written in the settings, and is not expected to
 * actually define a new database storage space.
 *
 * The implementation should provide a 'ofa-changed' signal in order
 * the application is able to detect the modifications brought up by
 * the user.
 */

#include <gtk/gtk.h>

#include "api/ofa-idbprovider-def.h"
#include "api/ofa-idbmeta-def.h"
#include "api/ofa-idbperiod.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBEDITOR                      ( ofa_idbeditor_get_type())
#define OFA_IDBEDITOR( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBEDITOR, ofaIDBEditor ))
#define OFA_IS_IDBEDITOR( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBEDITOR ))
#define OFA_IDBEDITOR_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBEDITOR, ofaIDBEditorInterface ))

typedef struct _ofaIDBEditor                    ofaIDBEditor;

/**
 * ofaIDBEditorInterface:
 * @get_interface_version: [should]: returns the interface version number.
 * @set_meta: [may]: initialize the widget with provided data.
 * @get_size_group: [may]: returns the #GtkSizeGroup of the column.
 * @get_valid: [may]: returns %TRUE if the entered informations are valid.
 *
 * This defines the interface that an #ofaIDBEditor should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBEditor instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIDBEditor interface.
	 *
	 * Returns: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint           ( *get_interface_version )( const ofaIDBEditor *instance );

	/**
	 * set_meta:
	 * @instance: the #ofaIDBEditor instance.
	 * @meta: the #ofaIDBMeta object which holds dossier meta
	 *  informations.
	 * @period: the #ofaIDBPeriod object which holds exercice
	 *  informations.
	 *
	 * Initialize the composite widget with provided informations.
	 *
	 * Since: version 1
	 */
	void            ( *set_meta )             ( ofaIDBEditor *instance,
													const ofaIDBMeta *meta,
													const ofaIDBPeriod *period );

	/**
	 * get_size_group:
	 * @instance: the #ofaIDBEditor instance.
	 * @column: the desired column.
	 *
	 * Returns: the #GtkSizeGroup for the desired @column.
	 *
	 * Since: version 1
	 */
	GtkSizeGroup *  ( *get_size_group )       ( const ofaIDBEditor *instance,
													guint column );

	/**
	 * get_valid:
	 * @instance: the #ofaIDBEditor instance.
	 * @message: [allow-none][out]: a message to be set.
	 *
	 * Returns: %TRUE if the entered connection informations are valid.
	 *
	 * Note that we only do here an intrinsic check as we do not have
	 * any credentials to test for a real server connection.
	 *
	 * Since: version 1
	 */
	gboolean        ( *get_valid )            ( const ofaIDBEditor *instance,
													gchar **message );
}
	ofaIDBEditorInterface;

GType           ofa_idbeditor_get_type                  ( void );

guint           ofa_idbeditor_get_interface_last_version( void );

guint           ofa_idbeditor_get_interface_version     ( const ofaIDBEditor *instance );

ofaIDBProvider *ofa_idbeditor_get_provider              ( const ofaIDBEditor *instance );

void            ofa_idbeditor_set_provider              ( ofaIDBEditor *instance,
															const ofaIDBProvider *provider );

void            ofa_idbeditor_set_meta                  ( ofaIDBEditor *instance,
															const ofaIDBMeta *meta,
															const ofaIDBPeriod *period );

GtkSizeGroup   *ofa_idbeditor_get_size_group            ( const ofaIDBEditor *instance,
															guint column );

gboolean        ofa_idbeditor_get_valid                 ( const ofaIDBEditor *instance,
															gchar **message );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBEDITOR_H__ */
