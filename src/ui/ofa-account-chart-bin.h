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
 */

#ifndef __OFA_ACCOUNT_CHART_BIN_H__
#define __OFA_ACCOUNT_CHART_BIN_H__

/**
 * SECTION: ofa_account_chart_bin
 * @short_description: #ofaAccountChartBin class definition.
 * @include: ui/ofa-account-chart-bin.h
 *
 * This is a convenience class which manages the display of the accounts
 * inside of a notebook, with one class per page.
 *
 * At creation time, this convenience class defines a Alt+1..Alt+9
 * mnemonic for each class at the GtkWindow parent level. These
 * mnemonics are removed when disposing.
 *
 * This convenience class also manages the update buttons (new, update,
 * delete and view entries). So that all the AccountPage features are
 * also available in AccountSelect dialog.
 */

#include "api/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_CHART_BIN                ( ofa_account_chart_bin_get_type())
#define OFA_ACCOUNT_CHART_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_CHART_BIN, ofaAccountChartBin ))
#define OFA_ACCOUNT_CHART_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_CHART_BIN, ofaAccountChartBinClass ))
#define OFA_IS_ACCOUNT_CHART_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_CHART_BIN ))
#define OFA_IS_ACCOUNT_CHART_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_CHART_BIN ))
#define OFA_ACCOUNT_CHART_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_CHART_BIN, ofaAccountChartBinClass ))

typedef struct _ofaAccountChartBinPrivate         ofaAccountChartBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                     parent;

	/*< private members >*/
	ofaAccountChartBinPrivate *priv;
}
	ofaAccountChartBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                parent;
}
	ofaAccountChartBinClass;

GType               ofa_account_chart_bin_get_type            ( void ) G_GNUC_CONST;

ofaAccountChartBin *ofa_account_chart_bin_new                 ( const ofaMainWindow *main_window );

void                ofa_account_chart_bin_set_cell_data_func  ( ofaAccountChartBin *book,
																		GtkTreeCellDataFunc fn_cell,
																		void *user_data );

void                ofa_account_chart_bin_expand_all          ( ofaAccountChartBin *book );

gchar              *ofa_account_chart_bin_get_selected        ( ofaAccountChartBin *book );

void                ofa_account_chart_bin_set_selected        ( ofaAccountChartBin *book,
																		const gchar *number );

void                ofa_account_chart_bin_toggle_collapse     ( ofaAccountChartBin *book );

void                ofa_account_chart_bin_button_clicked      ( ofaAccountChartBin *book,
																		gint button_id );

GtkWidget          *ofa_account_chart_bin_get_current_treeview( const ofaAccountChartBin *book );

void                ofa_account_chart_bin_cell_data_renderer  ( ofaAccountChartBin *book,
																		GtkTreeViewColumn *tcolumn,
																		GtkCellRenderer *cell,
																		GtkTreeModel *tmodel,
																		GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_ACCOUNT_CHART_BIN_H__ */
