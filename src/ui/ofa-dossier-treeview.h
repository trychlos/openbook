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

#ifndef __OFA_DOSSIER_TREEVIEW_H__
#define __OFA_DOSSIER_TREEVIEW_H__

/**
 * SECTION: ofa_dossier_treeview
 * @short_description: #ofaDossierTreeview class definition.
 * @include: ui/ofa-dossier-treeview.h
 *
 * Manage a treeview with the list of the dossiers which are defined in
 * the settings.
 */

#include <gtk/gtk.h>

#include "core/ofa-main-window-def.h"

#include "ui/ofa-dossier-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_TREEVIEW                ( ofa_dossier_treeview_get_type())
#define OFA_DOSSIER_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeview ))
#define OFA_DOSSIER_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeviewClass ))
#define OFA_IS_DOSSIER_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_TREEVIEW ))
#define OFA_IS_DOSSIER_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_TREEVIEW ))
#define OFA_DOSSIER_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeviewClass ))

typedef struct _ofaDossierTreeviewPrivate        ofaDossierTreeviewPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                     parent;

	/*< private members >*/
	ofaDossierTreeviewPrivate *priv;
}
	ofaDossierTreeview;

typedef struct {
	/*< public members >*/
	GtkBinClass                parent;
}
	ofaDossierTreeviewClass;

GType               ofa_dossier_treeview_get_type    ( void ) G_GNUC_CONST;

ofaDossierTreeview *ofa_dossier_treeview_new         ( void );

void                ofa_dossier_treeview_set_columns ( ofaDossierTreeview *view,
																ofaDossierColumns columns );

void                ofa_dossier_treeview_set_headers ( ofaDossierTreeview *view,
																gboolean visible );

void                ofa_dossier_treeview_add_row     ( ofaDossierTreeview *view,
																const gchar *dname );

void                ofa_dossier_treeview_remove_row  ( ofaDossierTreeview *view,
																const gchar *dname );

gchar              *ofa_dossier_treeview_get_selected( ofaDossierTreeview *view );

void                ofa_dossier_treeview_set_selected( ofaDossierTreeview *view,
																const gchar *dname );

G_END_DECLS

#endif /* __OFA_DOSSIER_TREEVIEW_H__ */
