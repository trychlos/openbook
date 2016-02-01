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

#ifndef __MY_TAB_LABEL_H__
#define __MY_TAB_LABEL_H__

/**
 * SECTION: my_tab_label
 * @short_description: #myTabLabel class definition.
 * @include: ui/my-tab-label.h
 *
 * A custom label for GtkNotebook main pages, which embeds an icon on
 * the left side, and a small close button on the right.
 *
 * From http://www.micahcarrick.com/gtk-notebook-tabs-with-close-button.html
 *
 * Note two side effects:
 * - the tab is a bit higher
 * - the popup menu comes back to default labels "Page 1",....
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_TYPE_TAB_LABEL                ( my_tab_label_get_type())
#define MY_TAB_LABEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_TAB_LABEL, myTabLabel ))
#define MY_TAB_LABEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_TAB_LABEL, myTabLabelClass ))
#define MY_IS_TAB_LABEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_TAB_LABEL ))
#define MY_IS_TAB_LABEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_TAB_LABEL ))
#define MY_TAB_LABEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_TAB_LABEL, myTabLabelClass ))

typedef struct _myTabLabelPrivate        myTabLabelPrivate;

typedef struct {
	/*< public members >*/
	GtkGrid            parent;

	/*< private members >*/
	myTabLabelPrivate *priv;
}
	myTabLabel;

typedef struct {
	/*< public members >*/
	GtkGridClass parent;
}
	myTabLabelClass;

/**
 * MY_SIGNAL_TAB_CLOSE_CLICKED: emitted when the 'close' button of a
 *                               tab is clicked
 * MY_SIGNAL_TAB_PIN_CLICKED: emitted when the 'pin' button of a
 *                               tab is clicked
 */
#define MY_SIGNAL_TAB_CLOSE_CLICKED     "tab-close-clicked"
#define MY_SIGNAL_TAB_PIN_CLICKED       "tab-pin-clicked"

GType       my_tab_label_get_type ( void ) G_GNUC_CONST;

myTabLabel *my_tab_label_new      ( GtkImage *image, const gchar *text );

gchar      *my_tab_label_get_label( const myTabLabel *tab );

G_END_DECLS

#endif /* __MY_TAB_LABEL_H__ */
