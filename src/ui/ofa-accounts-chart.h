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

#ifndef __OFA_ACCOUNTS_CHART_H__
#define __OFA_ACCOUNTS_CHART_H__

/**
 * SECTION: ofa_accounts_chart
 * @short_description: #ofaAccountsChart class definition.
 * @include: ui/ofa-accounts-chart.h
 *
 * This is a convenience class which manages the display of the accounts
 * inside of a notebook, with one class per page.
 *
 * At creation time, this convenience class defines a Alt+1..Alt+9
 * mnemonic for each class at the GtkWindow parent level. These
 * mnemonics are removed when disposing.
 *
 * This convenience class also manages the update buttons (new, update,
 * delete and view entries). So that all the AccountsPage features are
 * also available in AccountSelect dialog.
 */

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_CHART                ( ofa_accounts_chart_get_type())
#define OFA_ACCOUNTS_CHART( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_CHART, ofaAccountsChart ))
#define OFA_ACCOUNTS_CHART_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_CHART, ofaAccountsChartClass ))
#define OFA_IS_ACCOUNTS_CHART( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_CHART ))
#define OFA_IS_ACCOUNTS_CHART_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_CHART ))
#define OFA_ACCOUNTS_CHART_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_CHART, ofaAccountsChartClass ))

typedef struct _ofaAccountsChartPrivate        ofaAccountsChartPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                   parent;

	/*< private members >*/
	ofaAccountsChartPrivate *priv;
}
	ofaAccountsChart;

typedef struct {
	/*< public members >*/
	GtkBinClass              parent;
}
	ofaAccountsChartClass;

GType             ofa_accounts_chart_get_type            ( void ) G_GNUC_CONST;

ofaAccountsChart *ofa_accounts_chart_new                 ( void );

void              ofa_accounts_chart_set_cell_data_func  ( ofaAccountsChart *book,
																GtkTreeCellDataFunc fn_cell,
																void *user_data );

void              ofa_accounts_chart_set_main_window     ( ofaAccountsChart *book,
																ofaMainWindow *main_window );

void              ofa_accounts_chart_expand_all          ( ofaAccountsChart *book );

gchar            *ofa_accounts_chart_get_selected        ( ofaAccountsChart *book );

void              ofa_accounts_chart_set_selected        ( ofaAccountsChart *book,
																const gchar *number );

void              ofa_accounts_chart_toggle_collapse     ( ofaAccountsChart *book );

void              ofa_accounts_chart_button_clicked      ( ofaAccountsChart *book,
																gint button_id );

GtkWidget        *ofa_accounts_chart_get_current_treeview( const ofaAccountsChart *book );

void              ofa_accounts_chart_cell_data_renderer  ( ofaAccountsChart *book,
																GtkTreeViewColumn *tcolumn,
																GtkCellRenderer *cell,
																GtkTreeModel *tmodel,
																GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_ACCOUNTS_CHART_H__ */
