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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "ui/ofa-main-page.h"
#include "ui/ofa-account-notebook.h"
#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-chart.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-view-entries.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

/* private instance data
 */
struct _ofaAccountsChartPrivate {
	gboolean            dispose_has_run;

	/* UI
	 */
	ofaAccountNotebook *chart_child;
	GtkButton          *consult_btn;
};

G_DEFINE_TYPE( ofaAccountsChart, ofa_accounts_chart, OFA_TYPE_MAIN_PAGE )

static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       on_row_activated( ofoAccount *account, ofaMainPage *page );
static void       on_view_entries( ofoAccount *account, ofaAccountsChart *self );

static void
accounts_chart_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_finalize";
	ofaAccountsChartPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	priv = OFA_ACCOUNTS_CHART( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_chart_parent_class )->finalize( instance );
}

static void
accounts_chart_dispose( GObject *instance )
{
	ofaAccountsChartPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	priv = ( OFA_ACCOUNTS_CHART( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_chart_parent_class )->dispose( instance );
}

static void
ofa_accounts_chart_init( ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_init";

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaAccountsChartPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_accounts_chart_class_init( ofaAccountsChartClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_chart_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_chart_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	GtkNotebook *chart_book;
	ofaAccountNotebookParms parms;
	ofaAccountsChartPrivate *priv;

	chart_book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( chart_book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( chart_book ), 4 );
	gtk_notebook_popup_enable( chart_book );

	parms.main_window = ofa_main_page_get_main_window( page );
	parms.parent = GTK_CONTAINER( ofa_main_page_get_grid( page ));
	parms.has_import = FALSE;
	parms.has_export = FALSE;
	parms.has_view_entries = TRUE;
	parms.pfnSelected = NULL;
	parms.pfnActivated = ( ofaAccountNotebookCb ) on_row_activated;
	parms.pfnViewEntries = ( ofaAccountNotebookCb ) on_view_entries;
	parms.user_data = page;

	priv = OFA_ACCOUNTS_CHART( page )->private;
	priv->chart_child = ofa_account_notebook_new( &parms );

	return( GTK_WIDGET( chart_book ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaMainPage *page )
{
	ofa_account_notebook_init_view( OFA_ACCOUNTS_CHART( page )->private->chart_child, NULL );
}

/*
 * ofaAccountNotebook callback:
 */
static void
on_row_activated( ofoAccount *account, ofaMainPage *page )
{
	if( account ){
		g_return_if_fail( OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( ofa_main_page_get_main_window( page ), account );
	}

	ofa_account_notebook_grab_focus( OFA_ACCOUNTS_CHART( page )->private->chart_child );
}

static void
on_view_entries( ofoAccount *account, ofaAccountsChart *self )
{
	ofaMainPage *page;

	if( account ){
		page = ofa_main_window_activate_theme(
						ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )),
						THM_VIEW_ENTRIES );
		if( page ){
			ofa_view_entries_display_entries(
							OFA_VIEW_ENTRIES( page ),
							OFO_TYPE_ACCOUNT,
							ofo_account_get_number( account ),
							NULL,
							NULL );
		}
	}
}
