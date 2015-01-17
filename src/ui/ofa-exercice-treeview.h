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

#ifndef __OFA_EXERCICE_TREEVIEW_H__
#define __OFA_EXERCICE_TREEVIEW_H__

/**
 * SECTION: ofa_exercice_treeview
 * @short_description: #ofaExerciceTreeview class definition.
 * @include: ui/ofa-exercice-treeview.h
 *
 * A convenience class to display exercices in a treeview.
 *
 * In the provided parent container, this class will create a GtkTreeView
 * embedded in a GtkScrolledWindow.
 */

#include <gtk/gtk.h>

#include "ui/ofa-exercice-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_TREEVIEW                ( ofa_exercice_treeview_get_type())
#define OFA_EXERCICE_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeview ))
#define OFA_EXERCICE_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeviewClass ))
#define OFA_IS_EXERCICE_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_TREEVIEW ))
#define OFA_IS_EXERCICE_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_TREEVIEW ))
#define OFA_EXERCICE_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_TREEVIEW, ofaExerciceTreeviewClass ))

typedef struct _ofaExerciceTreeviewPrivate        ofaExerciceTreeviewPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaExerciceTreeviewPrivate *priv;
}
	ofaExerciceTreeview;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaExerciceTreeviewClass;

GType                ofa_exercice_treeview_get_type   ( void ) G_GNUC_CONST;

ofaExerciceTreeview *ofa_exercice_treeview_new        ( void );

void                 ofa_exercice_treeview_set_columns( ofaExerciceTreeview *view,
																ofaExerciceColumns columns );

void                 ofa_exercice_treeview_set_dossier( ofaExerciceTreeview *view,
																const gchar *dname );

#if 0
GList               *ofa_exercice_treeview_get_selected            ( ofaExerciceTreeview *view );
#define              ofa_exercice_treeview_free_selected(L)        g_list_free_full(( L ), ( GDestroyNotify ) g_free )
#endif

G_END_DECLS

#endif /* __OFA_EXERCICE_TREEVIEW_H__ */
