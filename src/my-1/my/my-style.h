/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __MY_API_MY_STYLE_H__
#define __MY_API_MY_STYLE_H__

/* @title: myStyle
 * @short_description: A Style-like class definition
 * @include: my/my-style.h
 *
 * The #myStyle class internally implements a singleton, which happens
 * to just be a thin encapsulation of the GtkCssProvider.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_STYLE              ( my_style_get_type())
#define MY_STYLE( object )         ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_STYLE, myStyle ))
#define MY_STYLE_CLASS( klass )    ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_STYLE, myStyleClass ))
#define MY_IS_STYLE( object )      ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_STYLE ))
#define MY_IS_STYLE_CLASS( klass ) ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_STYLE ))
#define MY_STYLE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_STYLE, myStyleClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	myStyle;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	myStyleClass;

GType    my_style_get_type         ( void ) G_GNUC_CONST;

/* use the internal singleton
 */
void     my_style_set_css_resource ( const gchar *path );

void     my_style_add              ( GtkWidget *widget, const gchar *class );

void     my_style_remove           ( GtkWidget *widget, const gchar *class );

void     my_style_free             ( void );

G_END_DECLS

#endif /* __MY_API_MY_STYLE_H__ */
