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

#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-properties.h"
#include "ui/ofa-account-frame-bin.h"
#include "ui/ofa-account-page.h"
#include "ui/ofa-entry-page.h"
#include "ui/ofa-settlement.h"
#include "ui/ofa-reconcil-page.h"

/* private instance data
 */
struct _ofaAccountPagePrivate {

	/* runtime
	 */
	const ofaMainWindow *main_window;
	ofaHub              *hub;
	gboolean             is_current;

	/* UI
	 */
	ofaAccountFrameBin  *account_bin;
	GtkWidget           *properties_btn;
	GtkWidget           *delete_btn;
	GtkWidget           *entries_btn;
	GtkWidget           *settlement_btn;
	GtkWidget           *reconcil_btn;
};

G_DEFINE_TYPE( ofaAccountPage, ofa_account_page, OFA_TYPE_PAGE )

static void       v_setup_page( ofaPage *page );
static void       on_new_clicked( GtkButton *button, ofaAccountPage *page );
static void       on_properties_clicked( GtkButton *button, ofaAccountPage *page );
static void       on_delete_clicked( GtkButton *button, ofaAccountPage *page );
static void       on_view_entries( GtkButton *button, ofaAccountPage *page );
static void       on_settlement( GtkButton *button, ofaAccountPage *page );
static void       on_reconciliation( GtkButton *button, ofaAccountPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_selection_changed( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self );
static void       do_update_sensitivity( ofaAccountPage *self, ofoAccount *account );
static void       on_row_activated( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self );

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
	ofaButtonsBox *box;
	GtkWidget *btn;
	ofoDossier *dossier;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_ACCOUNT_PAGE( page )->priv;

	priv->main_window = ofa_page_get_main_window( page );

	priv->hub = ofa_main_window_get_hub( priv->main_window );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->is_current = ofo_dossier_is_current( dossier );

	priv->account_bin = ofa_account_frame_bin_new( priv->main_window );
	gtk_widget_set_margin_top( GTK_WIDGET( priv->account_bin ), 4 );
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( priv->account_bin ), 0, 0, 1, 1 );

	g_signal_connect( priv->account_bin, "ofa-changed", G_CALLBACK( on_selection_changed ), page );
	g_signal_connect( priv->account_bin, "ofa-activated", G_CALLBACK( on_row_activated ), page );

	box = ofa_account_frame_bin_get_buttons_box( priv->account_bin );

	btn = ofa_buttons_box_add_button_with_mnemonic( box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, priv->is_current );

	btn = ofa_buttons_box_add_button_with_mnemonic( box, BUTTON_PROPERTIES, G_CALLBACK( on_properties_clicked ), page );
	priv->properties_btn = btn;

	btn = ofa_buttons_box_add_button_with_mnemonic( box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );
	priv->delete_btn = btn;

	ofa_buttons_box_add_spacer( box );

	btn = ofa_buttons_box_add_button_with_mnemonic( box, _( "_View entries..."), G_CALLBACK( on_view_entries ), page );
	priv->entries_btn = btn;

	btn = ofa_buttons_box_add_button_with_mnemonic( box, _( "_Settlement..." ), G_CALLBACK( on_settlement ), page );
	priv->settlement_btn = btn;

	btn = ofa_buttons_box_add_button_with_mnemonic( box, _( "_Reconcililation..." ), G_CALLBACK( on_reconciliation ), page );
	priv->reconcil_btn = btn;
}

static void
on_new_clicked( GtkButton *button, ofaAccountPage *page )
{
	ofaAccountPagePrivate *priv;

	priv = page->priv;

	ofa_account_frame_bin_do_new( priv->account_bin );
}

static void
on_properties_clicked( GtkButton *button, ofaAccountPage *page )
{
	ofaAccountPagePrivate *priv;

	priv = page->priv;

	ofa_account_frame_bin_do_properties( priv->account_bin );
}

static void
on_delete_clicked( GtkButton *button, ofaAccountPage *page )
{
	ofaAccountPagePrivate *priv;

	priv = page->priv;

	ofa_account_frame_bin_do_delete( priv->account_bin );
}

static void
on_view_entries( GtkButton *button, ofaAccountPage *self )
{
	ofaAccountPagePrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = self->priv;

	number = ofa_account_frame_bin_get_selected( priv->account_bin );
	page = ofa_main_window_activate_theme( priv->main_window, THM_ENTRIES );
	ofa_entry_page_display_entries( OFA_ENTRY_PAGE( page ), OFO_TYPE_ACCOUNT, number, NULL, NULL );
	g_free( number );
}

static void
on_settlement( GtkButton *button, ofaAccountPage *self )
{
	ofaAccountPagePrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = self->priv;

	number = ofa_account_frame_bin_get_selected( priv->account_bin );
	page = ofa_main_window_activate_theme( priv->main_window, THM_SETTLEMENT );
	ofa_settlement_set_account( OFA_SETTLEMENT( page ), number );
	g_free( number );
}

static void
on_reconciliation( GtkButton *button, ofaAccountPage *self )
{
	ofaAccountPagePrivate *priv;
	gchar *number;
	ofaPage *page;

	priv = self->priv;

	number = ofa_account_frame_bin_get_selected( priv->account_bin );
	page = ofa_main_window_activate_theme( priv->main_window, THM_RECONCIL );
	ofa_reconcil_page_set_account( OFA_RECONCIL_PAGE( page ), number );
	g_free( number );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaAccountPagePrivate *priv;
	GtkWidget *widget;

	g_return_val_if_fail( page && OFA_IS_ACCOUNT_PAGE( page ), NULL );

	priv = OFA_ACCOUNT_PAGE( page )->priv;
	widget = ofa_account_frame_bin_get_current_treeview( priv->account_bin );

	return( widget );
}

static void
on_selection_changed( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self )
{
	ofaAccountPagePrivate *priv;
	ofoAccount *account;

	priv = self->priv;
	account = my_strlen( number ) ? ofo_account_get_by_number( priv->hub, number ) : NULL;
	do_update_sensitivity( self, account );
}

static void
do_update_sensitivity( ofaAccountPage *self, ofoAccount *account )
{
	ofaAccountPagePrivate *priv;
	gboolean has_account;

	priv = self->priv;
	has_account = ( account && OFO_IS_ACCOUNT( account ));

	gtk_widget_set_sensitive( priv->properties_btn, has_account );
	gtk_widget_set_sensitive( priv->delete_btn, has_account && priv->is_current && ofo_account_is_deletable( account ));
	gtk_widget_set_sensitive( priv->entries_btn, has_account  && !ofo_account_is_root( account ));
	gtk_widget_set_sensitive( priv->settlement_btn, has_account && priv->is_current && ofo_account_is_settleable( account ));
	gtk_widget_set_sensitive( priv->reconcil_btn, has_account && priv->is_current && ofo_account_is_reconciliable( account ));
}

static void
on_row_activated( ofaAccountFrameBin *frame, const gchar *number, ofaAccountPage *self )
{
	ofaAccountPagePrivate *priv;
	ofoAccount *account;

	priv = self->priv;

	if( number ){
		account = ofo_account_get_by_number( priv->hub, number );
		g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

		ofa_account_properties_run( priv->main_window, account );
	}
}
