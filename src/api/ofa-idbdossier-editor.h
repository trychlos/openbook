/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IDBDOSSIER_EDITOR_H__
#define __OPENBOOK_API_OFA_IDBDOSSIER_EDITOR_H__

/**
 * SECTION: idbdossier_editor
 * @title: ofaIDBDossierEditor
 * @short_description: The DMBS New Interface
 * @include: openbook/ofa-idbdossier-editor.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * The #ofaIDBDossierEditor is the interface a #GtkWidget instanciated
 * by a DBMS provider should implement to let the application define a
 * new dossier.
 *
 * Basically, this interface is intended to manage the informations
 * needed by the #ofaIDBProvider to address the dossier, informations
 * which are to be written in the dossier settings.
 *
 * Particularily, this interface is not expected to actually define a
 * new database storage space.
 *
 * The implementation should provide a 'ofa-changed' signal in order
 * the application is able to detect the modifications brought up by
 * the user.
 */

#include <gtk/gtk.h>

#include "my/my-isettings.h"

#include "api/ofa-idbconnect-def.h"
#include "api/ofa-idbdossier-editor-def.h"
#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbprovider-def.h"
#include "api/ofa-idbsuperuser-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBDOSSIER_EDITOR                      ( ofa_idbdossier_editor_get_type())
#define OFA_IDBDOSSIER_EDITOR( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBDOSSIER_EDITOR, ofaIDBDossierEditor ))
#define OFA_IS_IDBDOSSIER_EDITOR( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBDOSSIER_EDITOR ))
#define OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBDOSSIER_EDITOR, ofaIDBDossierEditorInterface ))

#if 0
typedef struct _ofaIDBDossierEditor                     ofaIDBDossierEditor;
typedef struct _ofaIDBDossierEditorInterface            ofaIDBDossierEditorInterface;
#endif

/**
 * ofaIDBDossierEditorInterface:
 * @get_interface_version: [should]: returns the interface version number.
 * @get_size_group: [may]: returns the #GtkSizeGroup of the column.
 * @is_valid: [may]: returns %TRUE if the entered informations are valid.
 * @get_valid_connect: [may]: returns the valid connection.
 *
 * This defines the interface that an #ofaIDBDossierEditor should implement.
 */
struct _ofaIDBDossierEditorInterface {
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
	guint                  ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_su:
	 * @instance: the #ofaIDBDossierEditor instance.
	 *
	 * Returns: the managed #ofaIDBSuperuser.
	 *
	 * The returned reference is owned by the @instance, and should not
	 * be released by the caller.
	 *
	 * Since: version 1
	 */
	ofaIDBSuperuser *      ( *get_su )               ( const ofaIDBDossierEditor *instance );
};

/*
 * Interface-wide
 */
GType                 ofa_idbdossier_editor_get_type                  ( void );

guint                 ofa_idbdossier_editor_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint                 ofa_idbdossier_editor_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofaIDBProvider       *ofa_idbdossier_editor_get_provider              ( ofaIDBDossierEditor *editor );

void                  ofa_idbdossier_editor_set_provider              ( ofaIDBDossierEditor *editor,
																			ofaIDBProvider *provider );

GtkSizeGroup         *ofa_idbdossier_editor_get_size_group            ( const ofaIDBDossierEditor *editor,
																			guint column );

gboolean              ofa_idbdossier_editor_is_valid                  ( const ofaIDBDossierEditor *editor,
																			gchar **message );

ofaIDBSuperuser      *ofa_idbdossier_editor_get_su                    ( const ofaIDBDossierEditor *editor );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBDOSSIER_EDITOR_H__ */
