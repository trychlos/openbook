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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/ofa-buttons-box.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-account-frame-bin.h"
#include "ui/ofa-account-page.h"

/* private instance data
 */
struct _ofaAccountPagePrivate {

	/* UI
	 */
	ofaAccountFrameBin *accounts_frame;
};

G_DEFINE_TYPE( ofaAccountPage, ofa_account_page, OFA_TYPE_PAGE )

static void       v_setup_page( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_account_activated( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self );

static void
accounts_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_page_parent_class )->finalize( instance );
}

static void
accounts_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_ACCOUNT_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_page_parent_class )->dispose( instance );
}

static void
ofa_account_page_init( ofaAccountPage *self )
{
	static const gchar *thisfn = "ofa_account_page_init";

	g_return_if_fail( self && OFA_IS_ACCOUNT_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNT_PAGE, ofaAccountPagePrivate );
}

static void
ofa_account_page_class_init( ofaAccountPageClass *klass )
{
	static const gchar *thisfn = "ofa_account_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_page = v_setup_page;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaAccountPagePrivate ));
}

static void
v_setup_page( ofaPage *page )
{
	static const gchar *thisfn = "ofa_account_page_v_setup_page";
	ofaAccountPagePrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_ACCOUNT_PAGE( page )->priv;

	priv->accounts_frame = ofa_account_frame_bin_new( ofa_page_get_main_window( page ));
	gtk_widget_set_margin_top( GTK_WIDGET( priv->accounts_frame ), 4 );
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( priv->accounts_frame ), 0, 0, 1, 1 );
	ofa_account_frame_bin_set_buttons( priv->accounts_frame, TRUE, TRUE, TRUE );

	g_signal_connect(
			priv->accounts_frame, "ofa-activated", G_CALLBACK( on_account_activated ), page );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaAccountPagePrivate *priv;
	ofaAccountChartBin *book;
	GtkWidget *top_widget;

	g_return_val_if_fail( page && OFA_IS_ACCOUNT_PAGE( page ), NULL );

	priv = OFA_ACCOUNT_PAGE( page )->priv;
	book = ofa_account_frame_bin_get_chart( priv->accounts_frame );
	top_widget = ofa_account_chart_bin_get_current_treeview( book );

	return( top_widget );
}

static void
on_account_activated( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self )
{
	ofoAccount *account;
	const ofaMainWindow *main_window;
	GtkApplication *application;
	ofaHub *hub;

	if( number ){
		main_window = ofa_page_get_main_window( OFA_PAGE( self ));

		application = gtk_window_get_application( GTK_WINDOW( main_window ));
		g_return_if_fail( application && OFA_IS_IHUBBER( application ));

		hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
		g_return_if_fail( hub && OFA_IS_HUB( hub ));

		account = ofo_account_get_by_number( hub, number );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( main_window, account );
	}
}
