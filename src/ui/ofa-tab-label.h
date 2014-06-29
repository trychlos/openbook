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

#ifndef __OFA_TAB_LABEL_H__
#define __OFA_TAB_LABEL_H__

/**
 * SECTION: ofa_tab_label
 * @short_description: #ofaTabLabel class definition.
 * @include: ui/ofa-tab-label.h
 *
 * A custom label for GtkNotebook main pages, which embeds an icon on
 * the left side, and a small close button on the right.
 *
 * From http://www.micahcarrick.com/gtk-notebook-tabs-with-close-button.html
 *
 * Note two side effects:
 * - the tab is a bit more height
 * - the popup menu comes back to default labels "Page 1",....
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_TAB_LABEL                ( ofa_tab_label_get_type())
#define OFA_TAB_LABEL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TAB_LABEL, ofaTabLabel ))
#define OFA_TAB_LABEL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TAB_LABEL, ofaTabLabelClass ))
#define OFA_IS_TAB_LABEL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TAB_LABEL ))
#define OFA_IS_TAB_LABEL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TAB_LABEL ))
#define OFA_TAB_LABEL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TAB_LABEL, ofaTabLabelClass ))

typedef struct _ofaTabLabelPrivate        ofaTabLabelPrivate;

typedef struct {
	/*< private >*/
	GtkGrid             parent;
	ofaTabLabelPrivate *private;
}
	ofaTabLabel;

typedef struct {
	/*< private >*/
	GtkGridClass parent;
}
	ofaTabLabelClass;

/**
 * OFA_SIGNAL_TAB_CLOSE_CLICKED: emitted when the 'close' button of a
 *                               tab is clicked
 */
#define OFA_SIGNAL_TAB_CLOSE_CLICKED    "ofa-signal-tab-close-clicked"

GType        ofa_tab_label_get_type( void ) G_GNUC_CONST;

ofaTabLabel *ofa_tab_label_new     ( GtkImage *image, const gchar *text );

G_END_DECLS

#endif /* __OFA_TAB_LABEL_H__ */
