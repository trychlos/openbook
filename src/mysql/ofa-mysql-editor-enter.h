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

#ifndef __OFA_MYSQL_EDITOR_ENTER_H__
#define __OFA_MYSQL_EDITOR_ENTER_H__

/**
 * SECTION: ofa_mysql_editor
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

#include "api/ofa-idbeditor.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbmeta-def.h"
#include "api/ofa-ifile-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_EDITOR_ENTER                ( ofa_mysql_editor_enter_get_type())
#define OFA_MYSQL_EDITOR_ENTER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_EDITOR_ENTER, ofaMySQLEditorEnter ))
#define OFA_MYSQL_EDITOR_ENTER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_EDITOR_ENTER, ofaMySQLEditorEnterClass ))
#define OFA_IS_MYSQL_EDITOR_ENTER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_EDITOR_ENTER ))
#define OFA_IS_MYSQL_EDITOR_ENTER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_EDITOR_ENTER ))
#define OFA_MYSQL_EDITOR_ENTER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_EDITOR_ENTER, ofaMySQLEditorEnterClass ))

typedef struct _ofaMySQLEditorEnterPrivate         ofaMySQLEditorEnterPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaMySQLEditorEnterPrivate *priv;
}
	ofaMySQLEditorEnter;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaMySQLEditorEnterClass;

GType         ofa_mysql_editor_enter_get_type    ( void ) G_GNUC_CONST;

GtkWidget    *ofa_mysql_editor_enter_new         ( void );

const gchar  *ofa_mysql_editor_enter_get_host    ( const ofaMySQLEditorEnter *editor );

const gchar  *ofa_mysql_editor_enter_get_socket  ( const ofaMySQLEditorEnter *editor );

guint         ofa_mysql_editor_enter_get_port    ( const ofaMySQLEditorEnter *editor );

const gchar  *ofa_mysql_editor_enter_get_database( const ofaMySQLEditorEnter *editor );

G_END_DECLS

#endif /* __OFA_MYSQL_EDITOR_ENTER_H__ */
