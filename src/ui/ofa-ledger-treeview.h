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

#ifndef __OFA_LEDGER_TREEVIEW_H__
#define __OFA_LEDGER_TREEVIEW_H__

/**
 * SECTION: ofa_ledger_treeview
 * @short_description: #ofaLedgerTreeview class definition.
 * @include: ui/ofa-ledger-treeview.h
 *
 * A convenience class to display ledgers treeview based on a liststore
 * (use cases: ofaLedgersSet main page, ofaIntClosing dialog)
 *
 * In the provided parent container, this class will create a GtkTreeView
 * embedded in a GtkScrolledWindow.
 */

#include "api/ofo-dossier-def.h"
#include "api/ofo-ledger-def.h"

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_LEDGER_TREEVIEW                ( ofa_ledger_treeview_get_type())
#define OFA_LEDGER_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeview ))
#define OFA_LEDGER_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewClass ))
#define OFA_IS_LEDGER_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_LEDGER_TREEVIEW ))
#define OFA_IS_LEDGER_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_LEDGER_TREEVIEW ))
#define OFA_LEDGER_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_LEDGER_TREEVIEW, ofaLedgerTreeviewClass ))

typedef struct _ofaLedgerTreeviewPrivate        ofaLedgerTreeviewPrivate;

typedef struct {
	/*< public members >*/
	GObject                   parent;

	/*< private members >*/
	ofaLedgerTreeviewPrivate *priv;
}
	ofaLedgerTreeview;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaLedgerTreeviewClass;

/**
 * ofaLedgerTreeviewCb:
 *
 * A callback to be triggered when a row is selected or activated,
 *
 * Passed parameters are:
 * - a GList * of selected #ofoLedger objects
 * - user_data provided at initialization time
 */
typedef void    ( *ofaLedgerTreeviewCb )           ( GList *, gpointer );

/**
 * ofsLedgerTreeviewParms:
 *
 * The structure used to initialize this convenience class.
 */
typedef struct {
	ofaMainWindow      *main_window;
	GtkContainer       *parent;
	gboolean            allow_multiple_selection;
	ofaLedgerTreeviewCb pfnSelected;
	ofaLedgerTreeviewCb pfnActivated;
	void               *user_data;
}
	ofsLedgerTreeviewParms;

GType              ofa_ledger_treeview_get_type                ( void ) G_GNUC_CONST;

ofaLedgerTreeview *ofa_ledger_treeview_new                     ( const ofsLedgerTreeviewParms *parms );

void               ofa_ledger_treeview_init_view               ( ofaLedgerTreeview *view, const gchar *initial_selection );

GList             *ofa_ledger_treeview_get_selected            ( ofaLedgerTreeview *view );

GtkWidget         *ofa_ledger_treeview_get_top_focusable_widget( ofaLedgerTreeview *view );

G_END_DECLS

#endif /* __OFA_LEDGER_TREEVIEW_H__ */
