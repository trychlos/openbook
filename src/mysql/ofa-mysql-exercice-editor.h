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

#ifndef __OFA_MYSQL_EXERCICE_EDITOR_H__
#define __OFA_MYSQL_EXERCICE_EDITOR_H__

/**
 * SECTION: ofa_mysql_exercice_editor
 * @short_description: #ofaMysql class definition.
 *
 * Let the user enter connection informations.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-editor-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_EXERCICE_EDITOR                ( ofa_mysql_exercice_editor_get_type())
#define OFA_MYSQL_EXERCICE_EDITOR( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_EXERCICE_EDITOR, ofaMysqlExerciceEditor ))
#define OFA_MYSQL_EXERCICE_EDITOR_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_EXERCICE_EDITOR, ofaMysqlExerciceEditorClass ))
#define OFA_IS_MYSQL_EXERCICE_EDITOR( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_EXERCICE_EDITOR ))
#define OFA_IS_MYSQL_EXERCICE_EDITOR_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_EXERCICE_EDITOR ))
#define OFA_MYSQL_EXERCICE_EDITOR_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_EXERCICE_EDITOR, ofaMysqlExerciceEditorClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaMysqlExerciceEditor;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaMysqlExerciceEditorClass;

GType                   ofa_mysql_exercice_editor_get_type    ( void ) G_GNUC_CONST;

ofaMysqlExerciceEditor *ofa_mysql_exercice_editor_new         ( ofaIDBDossierEditor *editor,
																	const gchar *settings_prefix,
																	guint rule );

const gchar            *ofa_mysql_exercice_editor_get_database( ofaMysqlExerciceEditor *editor );

G_END_DECLS

#endif /* __OFA_MYSQL_EXERCICE_EDITOR_H__ */
