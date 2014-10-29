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

#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-book.h"
#include "ui/ofa-accounts-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-view-entries.h"

/* private instance data
 */
struct _ofaAccountsPagePrivate {

	/* UI
	 */
	ofaAccountsBook *book_child;
	GtkButton       *consult_btn;
};

G_DEFINE_TYPE( ofaAccountsPage, ofa_accounts_page, OFA_TYPE_PAGE )

static void       v_setup_page( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( ofaPage *page );
static void       on_row_activated( ofoAccount *account, ofaPage *page );
static void       on_view_entries( ofoAccount *account, ofaAccountsPage *self );

static void
accounts_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_page_parent_class )->finalize( instance );
}

static void
accounts_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_ACCOUNTS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_page_parent_class )->dispose( instance );
}

static void
ofa_accounts_page_init( ofaAccountsPage *self )
{
	static const gchar *thisfn = "ofa_accounts_page_init";

	g_return_if_fail( self && OFA_IS_ACCOUNTS_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNTS_PAGE, ofaAccountsPagePrivate );
}

static void
ofa_accounts_page_class_init( ofaAccountsPageClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_page = v_setup_page;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaAccountsPagePrivate ));
}

static void
v_setup_page( ofaPage *page )
{
	ofsAccountsBookParms parms;
	ofaAccountsPagePrivate *priv;

	parms.main_window = ofa_page_get_main_window( page );
	parms.parent = GTK_CONTAINER( ofa_page_get_grid( page ));
	parms.has_import = FALSE;
	parms.has_export = FALSE;
	parms.has_view_entries = TRUE;
	parms.pfnSelected = NULL;
	parms.pfnActivated = ( ofaAccountsBookCb ) on_row_activated;
	parms.pfnViewEntries = ( ofaAccountsBookCb ) on_view_entries;
	parms.user_data = page;

	priv = OFA_ACCOUNTS_PAGE( page )->priv;
	priv->book_child = ofa_accounts_book_new( &parms );
}

static void
v_init_view( ofaPage *page )
{
	ofa_accounts_book_init_view(
			OFA_ACCOUNTS_PAGE( page )->priv->book_child, NULL );
}

static GtkWidget *
v_get_top_focusable_widget( ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_ACCOUNTS_PAGE( page ), NULL );

	return(
			ofa_accounts_book_get_top_focusable_widget(
					OFA_ACCOUNTS_PAGE( page )->priv->book_child ));
}

/*
 * ofaAccountsBook callback:
 */
static void
on_row_activated( ofoAccount *account, ofaPage *page )
{
	GtkWidget *tview;

	if( account ){
		g_return_if_fail( OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( ofa_page_get_main_window( page ), account );
	}

	tview = ofa_accounts_book_get_top_focusable_widget(
					OFA_ACCOUNTS_PAGE( page )->priv->book_child );
	if( tview ){
		gtk_widget_grab_focus( tview );
	}
}

static void
on_view_entries( ofoAccount *account, ofaAccountsPage *self )
{
	ofaPage *page;

	if( account ){
		page = ofa_main_window_activate_theme(
						ofa_page_get_main_window( OFA_PAGE( self )),
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
