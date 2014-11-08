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

#ifndef __OFA_TREEVIEW_H__
#define __OFA_TREEVIEW_H__

/**
 * SECTION: ofa_treeview
 * @short_description: #ofaTreeview class definition.
 * @include: ui/ofa-treeview.h
 *
 * A base class for application treeviews.
 * It is supposed to let us factorize and homogeneïze the treeviews
 * behavior through the application.
 *
 * In the provided parent container, this class defines a GtkTreeView
 * embedded in a GtkScrolledWindow.
 *
 * In other words, #ofaTreeview object is a particulaer
 * GtkScrolledWindow which embeds a GtkTreeview.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_TREEVIEW                ( ofa_treeview_get_type())
#define OFA_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TREEVIEW, ofaTreeview ))
#define OFA_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TREEVIEW, ofaTreeviewClass ))
#define OFA_IS_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TREEVIEW ))
#define OFA_IS_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TREEVIEW ))
#define OFA_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TREEVIEW, ofaTreeviewClass ))

typedef struct _ofaTreeviewProtected     ofaTreeviewProtected;
typedef struct _ofaTreeviewPrivate       ofaTreeviewPrivate;

typedef struct {
	/*< public members >*/
	GtkScrolledWindow      parent;

	/*< protected members >*/
	ofaTreeviewProtected  *prot;

	/*< private members >*/
	ofaTreeviewPrivate    *priv;
}
	ofaTreeview;

typedef struct {
	/*< public members >*/
	GtkScrolledWindowClass parent;

	/* protected virtual functions */

	/**
	 * treemodel_new:
	 * @instance: this #ofaTreeview instance.
	 *
	 * Returns: a GtkTreeModel object suitable for display in our
	 *  GtkTreeView.
	 *
	 * This is a pure virtual function that the derived class should
	 * implement.
	 */
	GtkTreeModel * ( *tree_model_new )( ofaTreeview *instance );
}
	ofaTreeviewClass;

/**
 * Properties set against the #ofaTreeview class:
 * @TREEVIEW_PROP_USE_BOXES: whether the derived class makes use of the
 *  #ofaBoxes class for defining its field datas.
 *  Defaults to %FALSE.
 */
#define TREEVIEW_PROP_USE_BOXES         "ofa-treeview-prop-use-boxes"

/**
 * Signals emitted by the #ofaTreeview:
 * @TREEVIEW_SIGNAL_ROW_SELECTED: the selection has changed.
 * @TREEVIEW_SIGNAL_ROW_ACTIVATED: a row has been activated.
 * @TREEVIEW_SIGNAL_KEY_PRESSED: a key shortcut has been pressed.
 */
#define TREEVIEW_SIGNAL_ROW_SELECTED    "ofa-treeview-signal-row-selected"
#define TREEVIEW_SIGNAL_ROW_ACTIVATED   "ofa-treeview-signal-row-activated"
#define TREEVIEW_SIGNAL_KEY_PRESSED     "ofa-treeview-signal-key-pressed"

GType  ofa_treeview_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_TREEVIEW_H__ */
