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
 *
 */

#ifndef __OFA_BAT_TREEVIEW_H__
#define __OFA_BAT_TREEVIEW_H__

/**
 * SECTION: ofa_bat_treeview
 * @short_description: #ofaBatTreeview class definition.
 * @include: ui/ofa-bat-treeview.h
 *
 * Manage a treeview with the list of the BAT files imported in the
 * dossier.
 */

#include <gtk/gtk.h>

#include "api/ofo-bat-def.h"

#include "core/ofa-main-window-def.h"

#include "ui/ofa-bat-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_TREEVIEW                ( ofa_bat_treeview_get_type())
#define OFA_BAT_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_TREEVIEW, ofaBatTreeview ))
#define OFA_BAT_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_TREEVIEW, ofaBatTreeviewClass ))
#define OFA_IS_BAT_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_TREEVIEW ))
#define OFA_IS_BAT_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_TREEVIEW ))
#define OFA_BAT_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_TREEVIEW, ofaBatTreeviewClass ))

typedef struct _ofaBatTreeviewPrivate        ofaBatTreeviewPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                 parent;

	/*< private members >*/
	ofaBatTreeviewPrivate *priv;
}
	ofaBatTreeview;

typedef struct {
	/*< public members >*/
	GtkBinClass            parent;
}
	ofaBatTreeviewClass;

/**
 * ofaBatColumns:
 * The columns displayed in the views.
 */
typedef enum {
	BAT_DISP_ID = 1,
	BAT_DISP_URI,
	BAT_DISP_FORMAT,
	BAT_DISP_BEGIN,
	BAT_DISP_END,
	BAT_DISP_RIB,
	BAT_DISP_CURRENCY,
	BAT_DISP_BEGIN_SOLDE,
	BAT_DISP_BEGIN_SOLDE_SET,
	BAT_DISP_END_SOLDE,
	BAT_DISP_END_SOLDE_SET,
	BAT_DISP_NOTES,
	BAT_DISP_COUNT,
	BAT_DISP_UPD_USER,
	BAT_DISP_UPD_STAMP
}
	ofaBatColumns;

GType           ofa_bat_treeview_get_type       ( void ) G_GNUC_CONST;

ofaBatTreeview *ofa_bat_treeview_new            ( void );

void            ofa_bat_treeview_set_columns    ( ofaBatTreeview *view,
														ofaBatColumns *columns );

void            ofa_bat_treeview_set_delete     ( ofaBatTreeview *view,
														gboolean authorize );

void            ofa_bat_treeview_set_main_window( ofaBatTreeview *view,
														ofaMainWindow *main_window );

ofoBat         *ofa_bat_treeview_get_selected   ( const ofaBatTreeview *view );

void            ofa_bat_treeview_set_selected   ( ofaBatTreeview *view,
														ofxCounter id );

GtkWidget      *ofa_bat_treeview_get_treeview   ( const ofaBatTreeview *view );

void            ofa_bat_treeview_delete_bat     ( ofaBatTreeview *view,
														ofoBat *bat );

G_END_DECLS

#endif /* __OFA_BAT_TREEVIEW_H__ */
