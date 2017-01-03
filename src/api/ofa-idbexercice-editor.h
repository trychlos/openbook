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

#ifndef __OPENBOOK_API_OFA_IDBEXERCICE_EDITOR_H__
#define __OPENBOOK_API_OFA_IDBEXERCICE_EDITOR_H__

/**
 * SECTION: idbexercice_editor
 * @title: ofaIDBExerciceEditor
 * @short_description: The DMBS New Interface
 * @include: openbook/ofa-idbexercice-editor.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * The #ofaIDBExerciceEditor is the interface a #GtkWidget instanciated
 * by a DBMS provider should implement to let the application define a
 * new exercice.
 *
 * Basically, this interface is intended to manage the informations
 * needed by the #ofaIDBProvider to address the exercice, informations
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

#include "api/ofa-idbdossier-editor-def.h"
#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-editor-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-idbprovider-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBEXERCICE_EDITOR                      ( ofa_idbexercice_editor_get_type())
#define OFA_IDBEXERCICE_EDITOR( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBEXERCICE_EDITOR, ofaIDBExerciceEditor ))
#define OFA_IS_IDBEXERCICE_EDITOR( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBEXERCICE_EDITOR ))
#define OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBEXERCICE_EDITOR, ofaIDBExerciceEditorInterface ))

#if 0
typedef struct _ofaIDBExerciceEditor                     ofaIDBExerciceEditor;
typedef struct _ofaIDBExerciceEditorInterface            ofaIDBExerciceEditorInterface;
#endif

/**
 * ofaIDBExerciceEditorInterface:
 * @get_interface_version: [should]: returns the interface version number.
 * @get_size_group: [may]: returns the #GtkSizeGroup of the column.
 * @is_valid: [may]: returns %TRUE if the entered informations are valid.
 * @apply: [may]: register informations in the settings.
 *
 * This defines the interface that an #ofaIDBExerciceEditor should implement.
 */
struct _ofaIDBExerciceEditorInterface {
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
	guint          ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_size_group:
	 * @instance: the #ofaIDBExerciceEditor instance.
	 * @column: the desired column.
	 *
	 * Returns: the #GtkSizeGroup for the desired @column.
	 *
	 * Since: version 1
	 */
	GtkSizeGroup * ( *get_size_group )       ( const ofaIDBExerciceEditor *instance,
													guint column );

	/**
	 * is_valid:
	 * @instance: the #ofaIDBExerciceEditor instance.
	 * @message: [allow-none][out]: a message to be set.
	 *
	 * Returns: %TRUE if the entered connection informations are valid.
	 *
	 * Note that we only do here an intrinsic check as we do not have
	 * any credentials to test for a real server connection.
	 *
	 * Since: version 1
	 */
	gboolean       ( *is_valid )             ( const ofaIDBExerciceEditor *instance,
													gchar **message );

	/**
	 * apply:
	 * @instance: the #ofaIDBExerciceEditor instance.
	 *
	 * Returns: %TRUE if the informations have been successfully registered.
	 *
	 * Since: version 1
	 */
	gboolean       ( *apply )                ( const ofaIDBExerciceEditor *instance );
};

/*
 * Interface-wide
 */
GType                ofa_idbexercice_editor_get_type                  ( void );

guint                ofa_idbexercice_editor_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint                ofa_idbexercice_editor_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void                 ofa_idbexercice_editor_set_provider              ( ofaIDBExerciceEditor *instance,
																			ofaIDBProvider *provider );

ofaIDBDossierEditor *ofa_idbexercice_editor_get_dossier_editor        ( ofaIDBExerciceEditor *instance );

void                 ofa_idbexercice_editor_set_dossier_editor        ( ofaIDBExerciceEditor *instance,
																			ofaIDBDossierEditor *editor );

GtkSizeGroup        *ofa_idbexercice_editor_get_size_group            ( const ofaIDBExerciceEditor *instance,
																			guint column );

gboolean             ofa_idbexercice_editor_is_valid                  ( const ofaIDBExerciceEditor *instance,
																			gchar **message );

gboolean             ofa_idbexercice_editor_apply                     ( const ofaIDBExerciceEditor *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBEXERCICE_EDITOR_H__ */
