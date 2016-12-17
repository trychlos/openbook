/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RECURRENT_RUN_TREEVIEW_H__
#define __OFA_RECURRENT_RUN_TREEVIEW_H__

/**
 * SECTION: ofa_recurrent_run_treeview
 * @short_description: #ofaRecurrentRunTreeview class definition.
 * @include: recurrent/ofa-recurrent-run-treeview.h
 *
 * Manage a treeview with the list of the generated run operations.
 * This list may be provided by the caller, or got ftom the dbms.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+--------------+
 *    | Signal           | Run list     |
 *    |                  | may be %NULL |
 *    +------------------+--------------+
 *    | ofa-recchanged   |      Yes     |
 *    | ofa-recactivated |       No     |
 *    | ofa-recdelete    |       No     |
 *    +------------------+--------------+
 *
 * As the treeview may allow multiple selection, both signals provide
 * a list of path of selected rows. It is up to the user of this class
 * to decide whether an action may apply or not on a multiple selection.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-tvbin.h"

#include "recurrent/ofo-recurrent-run.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_RUN_TREEVIEW                ( ofa_recurrent_run_treeview_get_type())
#define OFA_RECURRENT_RUN_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_RUN_TREEVIEW, ofaRecurrentRunTreeview ))
#define OFA_RECURRENT_RUN_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_RUN_TREEVIEW, ofaRecurrentRunTreeviewClass ))
#define OFA_IS_RECURRENT_RUN_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_RUN_TREEVIEW ))
#define OFA_IS_RECURRENT_RUN_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_RUN_TREEVIEW ))
#define OFA_RECURRENT_RUN_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_RUN_TREEVIEW, ofaRecurrentRunTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaRecurrentRunTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaRecurrentRunTreeviewClass;

/**
 * Filtering the list
 */
enum {
	REC_VISIBLE_NONE      = 0,
	REC_VISIBLE_CANCELLED = 1 << 0,
	REC_VISIBLE_WAITING   = 1 << 1,
	REC_VISIBLE_VALIDATED = 1 << 2,
};

GType                    ofa_recurrent_run_treeview_get_type          ( void ) G_GNUC_CONST;

ofaRecurrentRunTreeview *ofa_recurrent_run_treeview_new               ( ofaHub *hub );

void                     ofa_recurrent_run_treeview_set_settings_key  ( ofaRecurrentRunTreeview *view,
																			const gchar *settings_key );

void                     ofa_recurrent_run_treeview_setup_columns     ( ofaRecurrentRunTreeview *view );

gint                     ofa_recurrent_run_treeview_get_visible       ( ofaRecurrentRunTreeview *view );

void                     ofa_recurrent_run_treeview_set_visible       ( ofaRecurrentRunTreeview *view,
																			gint visible );

GList                   *ofa_recurrent_run_treeview_get_selected      ( ofaRecurrentRunTreeview *view );

#define                  ofa_recurrent_run_treeview_free_selected(L)  g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref )

void                     ofa_recurrent_run_treeview_unselect          ( ofaRecurrentRunTreeview *view,
																			ofoRecurrentRun *run );

G_END_DECLS

#endif /* __OFA_RECURRENT_RUN_TREEVIEW_H__ */
