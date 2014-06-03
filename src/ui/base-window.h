/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifndef __BASE_WINDOW_H__
#define __BASE_WINDOW_H__

/**
 * SECTION: base_window
 * @short_description: #BaseWindow class definition.
 * @include: ui/ofa-base-window.h
 *
 * This base class let us factorize all what is common to GtkDialog
 * and GtkAssistant.
 */

#include "ui/ofa-main-window-def.h"
#include "ui/ofo-dossier-def.h"

G_BEGIN_DECLS

#define BASE_TYPE_WINDOW                ( base_window_get_type())
#define BASE_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, BASE_TYPE_WINDOW, BaseWindow ))
#define BASE_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, BASE_TYPE_WINDOW, BaseWindowClass ))
#define IS_BASE_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, BASE_TYPE_WINDOW ))
#define IS_BASE_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), BASE_TYPE_WINDOW ))
#define BASE_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), BASE_TYPE_WINDOW, BaseWindowClass ))

typedef struct _BaseWindowPrivate       BaseWindowPrivate;
typedef struct _BaseWindowProtected     BaseWindowProtected;

typedef struct {
	/*< private >*/
	GObject              parent;
	BaseWindowPrivate   *private;
	BaseWindowProtected *protected;
}
	BaseWindow;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	BaseWindowClass;

/**
 * Properties defined by the BaseWindow class.
 *
 * @BASE_PROP_MAIN_WINDOW:      main window of the application
 * @BASE_PROP_WINDOW_XML:       path to the xml file which contains the UI description
 * @BASE_PROP_WINDOW_NAME:      window toplevel name
 */
#define BASE_PROP_MAIN_WINDOW             "base-window-prop-main-window"
#define BASE_PROP_WINDOW_XML              "base-window-prop-xml"
#define BASE_PROP_WINDOW_NAME             "base-window-prop-name"

GType          base_window_get_type       ( void ) G_GNUC_CONST;

ofoDossier    *base_window_get_dossier    ( const BaseWindow *window );

ofaMainWindow *base_window_get_main_window( const BaseWindow *window );

G_END_DECLS

#endif /* __BASE_WINDOW_H__ */
