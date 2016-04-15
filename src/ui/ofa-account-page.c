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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-account.h"

#include "core/ofa-account-properties.h"
#include "core/ofa-account-frame-bin.h"

#include "ui/ofa-account-page.h"

/* private instance data
 */
typedef struct {

	/* UI
	 */
	ofaAccountFrameBin *account_bin;
}
	ofaAccountPagePrivate;

static void       v_setup_page( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_activated( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountPage, ofa_account_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaAccountPage ))

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
}

static void
v_setup_page( ofaPage *page )
{
	static const gchar *thisfn = "ofa_account_page_v_setup_page";
	ofaAccountPagePrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_account_page_get_instance_private( OFA_ACCOUNT_PAGE( page ));

	priv->account_bin = ofa_account_frame_bin_new( OFA_IGETTER( page ));
	my_utils_widget_set_margins( GTK_WIDGET( priv->account_bin ), 4, 4, 4, 0 );
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( priv->account_bin ), 0, 0, 1, 1 );

	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_NEW, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_PROPERTIES, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_DELETE, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_SPACER, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_VIEW_ENTRIES, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_SETTLEMENT, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_RECONCILIATION, TRUE );

	g_signal_connect( priv->account_bin, "ofa-activated", G_CALLBACK( on_row_activated ), page );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaAccountPagePrivate *priv;
	GtkWidget *widget;

	g_return_val_if_fail( page && OFA_IS_ACCOUNT_PAGE( page ), NULL );

	priv = ofa_account_page_get_instance_private( OFA_ACCOUNT_PAGE( page ));

	widget = ofa_account_frame_bin_get_current_treeview( priv->account_bin );

	return( widget );
}

static void
on_row_activated( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self )
{
	ofoAccount *account;
	ofaHub *hub;
	GtkWindow *toplevel;

	if( my_strlen( number )){
		hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
		account = ofo_account_get_by_number( hub, number );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_account_properties_run( OFA_IGETTER( self ), toplevel, account );
	}
}
