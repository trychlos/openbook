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

#ifndef __OFA_RECURRENT_RUN_TREEVIEW_H__
#define __OFA_RECURRENT_RUN_TREEVIEW_H__

/**
 * SECTION: ofa_recurrent_run_treeview
 * @short_description: #ofaRecurrentRunTreeview class definition.
 * @include: recurrent/ofa-recurrent_run-treeview.h
 *
 * A convenience class to display ofoRecurrentRun objects in a
 * liststore-based treeview.
 *
 * Signals defined here:
 * - ofa-changed: when the selection has changed
 * - ofa-activated: when the selection is activated.
 */

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_RUN_TREEVIEW                ( ofa_recurrent_run_treeview_get_type())
#define OFA_RECURRENT_RUN_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_RUN_TREEVIEW, ofaRecurrentRunTreeview ))
#define OFA_RECURRENT_RUN_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_RUN_TREEVIEW, ofaRecurrentRunTreeviewClass ))
#define OFA_IS_RECURRENT_RUN_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_RUN_TREEVIEW ))
#define OFA_IS_RECURRENT_RUN_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_RUN_TREEVIEW ))
#define OFA_RECURRENT_RUN_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_RUN_TREEVIEW, ofaRecurrentRunTreeviewClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaRecurrentRunTreeview;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaRecurrentRunTreeviewClass;

GType                    ofa_recurrent_run_treeview_get_type          ( void ) G_GNUC_CONST;

ofaRecurrentRunTreeview *ofa_recurrent_run_treeview_new               ( ofaHub *hub,
																			gboolean auto_update );

void                     ofa_recurrent_run_treeview_set_visible       ( ofaRecurrentRunTreeview *bin,
																			const gchar *status,
																			gboolean visible );

void                     ofa_recurrent_run_treeview_set_selection_mode( ofaRecurrentRunTreeview *bin,
																			GtkSelectionMode mode );

void                     ofa_recurrent_run_treeview_get_sort_settings ( ofaRecurrentRunTreeview *bin,
																			gint *sort_column_id,
																			gint *sort_sens );

void                     ofa_recurrent_run_treeview_set_sort_settings ( ofaRecurrentRunTreeview *bin,
																			gint sort_column_id,
																			gint sort_sens );

void                     ofa_recurrent_run_treeview_set_from_list     ( ofaRecurrentRunTreeview *bin,
																			GList *dataset );

void                     ofa_recurrent_run_treeview_set_from_db       ( ofaRecurrentRunTreeview *bin );

void                     ofa_recurrent_run_treeview_clear             ( ofaRecurrentRunTreeview *bin );

GtkWidget               *ofa_recurrent_run_treeview_get_treeview      ( ofaRecurrentRunTreeview *bin );

#define                  ofa_recurrent_run_treeview_free_selected(L)  g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

GList                   *ofa_recurrent_run_treeview_get_selected      ( ofaRecurrentRunTreeview *bin );

G_END_DECLS

#endif /* __OFA_RECURRENT_RUN_TREEVIEW_H__ */
