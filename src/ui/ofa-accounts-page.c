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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-frame.h"
#include "ui/ofa-accounts-page.h"
#include "ui/ofa-buttons-box.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-view-entries.h"

/* private instance data
 */
struct _ofaAccountsPagePrivate {

	/* UI
	 */
	ofaAccountsFrame *accounts_frame;
};

G_DEFINE_TYPE( ofaAccountsPage, ofa_accounts_page, OFA_TYPE_PAGE )

static void       v_setup_page( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_account_activated( ofaAccountsFrame *frame, const gchar *number, ofaAccountsPage *self );

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
	ofaAccountsPagePrivate *priv;
	GtkGrid *grid;

	priv = OFA_ACCOUNTS_PAGE( page )->priv;

	grid = ofa_page_get_top_grid( page );

	priv->accounts_frame = ofa_accounts_frame_new();
	gtk_grid_attach( grid, GTK_WIDGET( priv->accounts_frame ), 0, 0, 1, 1 );

	ofa_accounts_frame_set_main_window( priv->accounts_frame, ofa_page_get_main_window( page ));
	ofa_accounts_frame_set_buttons( priv->accounts_frame, TRUE );

	g_signal_connect(
			G_OBJECT( priv->accounts_frame ),
			"activated", G_CALLBACK( on_account_activated ), page );
}

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_accounts_page_v_init_view";

	g_debug( "%s: page=%p", thisfn, ( void * ) page );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaAccountsPagePrivate *priv;
	ofaAccountsBook *book;
	GtkWidget *top_widget;

	g_return_val_if_fail( page && OFA_IS_ACCOUNTS_PAGE( page ), NULL );

	priv = OFA_ACCOUNTS_PAGE( page )->priv;
	book = ofa_accounts_frame_get_book( priv->accounts_frame );
	top_widget = ofa_accounts_book_get_current_treeview( book );

	return( top_widget );
}

static void
on_account_activated( ofaAccountsFrame *frame, const gchar *number, ofaAccountsPage *self )
{
	ofoAccount *account;

	if( number ){
		account = ofo_account_get_by_number( ofa_page_get_dossier( OFA_PAGE( self )), number );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( ofa_page_get_main_window( OFA_PAGE( self )), account );
	}
}
