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

#include "api/my-utils.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "ui/my-buttons-box.h"
#include "ui/ofa-ledger-properties.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-ledgers-page.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-view-entries.h"

/* private instance data
 */
struct _ofaLedgersPagePrivate {

	/* internals
	 */
	gint                exe_id;			/* internal identifier of the current exercice */

	/* UI
	 */
	ofaLedgerTreeview  *tview;
	GtkWidget          *update_btn;
	GtkWidget          *delete_btn;
	GtkWidget          *entries_btn;
};

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_CLOSING,
	COL_OBJECT,
	N_COLUMNS
};

G_DEFINE_TYPE( ofaLedgersPage, ofa_ledgers_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static void       v_init_view( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       insert_dataset( ofaLedgersPage *self );
static void       on_row_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgersPage *self );
static void       on_row_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgersPage *self );
static void       v_on_button_clicked( ofaPage *page, guint button_id );
static void       on_new_clicked( ofaLedgersPage *page );
static void       on_update_clicked( ofaLedgersPage *page );
static void       do_update( ofaLedgersPage *self, ofoLedger *ledger );
static void       on_delete_clicked( ofaLedgersPage *page );
static gboolean   delete_confirmed( ofaLedgersPage *self, ofoLedger *ledger );
static void       on_view_entries( GtkButton *button, ofaLedgersPage *self );

static void
ledgers_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledgers_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGERS_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledgers_page_parent_class )->finalize( instance );
}

static void
ledgers_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_LEDGERS_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledgers_page_parent_class )->dispose( instance );
}

static void
ofa_ledgers_page_init( ofaLedgersPage *self )
{
	static const gchar *thisfn = "ofa_ledgers_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGERS_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LEDGERS_PAGE, ofaLedgersPagePrivate );
}

static void
ofa_ledgers_page_class_init( ofaLedgersPageClass *klass )
{
	static const gchar *thisfn = "ofa_ledgers_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
	OFA_PAGE_CLASS( klass )->on_button_clicked = v_on_button_clicked;

	g_type_class_add_private( klass, sizeof( ofaLedgersPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	GtkWidget *frame;

	frame = setup_tree_view( page );

	return( frame );
}

static GtkWidget *
setup_tree_view( ofaPage *page )
{
	ofaLedgersPagePrivate *priv;
	GtkFrame *frame;

	priv = OFA_LEDGERS_PAGE( page )->priv;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_margin_left( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_top( GTK_WIDGET( frame ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( frame ), 4 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	priv->tview = ofa_ledger_treeview_new();
	ofa_ledger_treeview_attach_to( priv->tview,
			GTK_CONTAINER( frame ),
			LEDGER_MNEMO | LEDGER_LABEL | LEDGER_ENTRY | LEDGER_CLOSING,
			GTK_SELECTION_BROWSE );

	g_signal_connect( G_OBJECT( priv->tview ), "changed", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( G_OBJECT( priv->tview ), "activated", G_CALLBACK( on_row_activated ), page );

	return( GTK_WIDGET( frame ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaLedgersPagePrivate *priv;
	GtkWidget *buttons_box;
	GtkWidget *button;

	g_return_val_if_fail( OFA_IS_LEDGERS_PAGE( page ), NULL );

	priv = OFA_LEDGERS_PAGE( page )->priv;

	buttons_box = OFA_PAGE_CLASS( ofa_ledgers_page_parent_class )->setup_buttons( page );
	g_return_val_if_fail( buttons_box && MY_IS_BUTTONS_BOX( buttons_box ), NULL );
	priv->update_btn = ofa_page_get_button_by_id( page, BUTTONS_BOX_PROPERTIES );
	priv->delete_btn = ofa_page_get_button_by_id( page, BUTTONS_BOX_DELETE );

	my_buttons_box_add_spacer( MY_BUTTONS_BOX( buttons_box ));

	button = gtk_button_new_with_mnemonic( _( "View _entries..." ));
	my_buttons_box_pack_button(
			MY_BUTTONS_BOX( buttons_box ), button, FALSE, G_CALLBACK( on_view_entries ), page );
	priv->entries_btn = button;

	return( buttons_box );
}

static void
v_init_view( ofaPage *page )
{
	insert_dataset( OFA_LEDGERS_PAGE( page ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_LEDGERS_PAGE( page ), NULL );

	return( ofa_ledger_treeview_get_top_focusable_widget( OFA_LEDGERS_PAGE( page )->priv->tview ));
}

static void
insert_dataset( ofaLedgersPage *self )
{
	ofoDossier *dossier;

	dossier = ofa_page_get_dossier( OFA_PAGE( self ));
	ofa_ledger_treeview_init_view( self->priv->tview, dossier, NULL );
}

/*
 * LedgerTreeview callback
 */
static void
on_row_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgersPage *self )
{
	ofoDossier *dossier;
	ofoLedger *ledger;

	dossier = ofa_page_get_dossier( OFA_PAGE( self ));
	ledger = ofo_ledger_get_by_mnemo( dossier, ( const gchar * ) selected->data );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	do_update( self, ledger );
}

/*
 * LedgerTreeview callback
 */
static void
on_row_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgersPage *self )
{
	ofaLedgersPagePrivate *priv;
	ofoDossier *dossier;
	ofoLedger *ledger;

	priv = self->priv;
	dossier = ofa_page_get_dossier( OFA_PAGE( self ));
	ledger = ofo_ledger_get_by_mnemo( dossier, ( const gchar * ) selected->data );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	gtk_widget_set_sensitive(
			priv->update_btn,
			ledger && OFO_IS_LEDGER( ledger ));

	gtk_widget_set_sensitive(
			priv->delete_btn,
			ledger &&
				OFO_IS_LEDGER( ledger ) &&
				ofo_ledger_is_deletable( ledger, ofa_page_get_dossier( OFA_PAGE( self ))));

	gtk_widget_set_sensitive(
			priv->entries_btn,
			ledger && OFO_IS_LEDGER( ledger ) &&
				ofo_ledger_has_entries( ledger, ofa_page_get_dossier( OFA_PAGE( self ))));
}

static void
v_on_button_clicked( ofaPage *page, guint button_id )
{
	g_return_if_fail( page && OFA_IS_LEDGERS_PAGE( page ));

	switch( button_id ){
		case BUTTONS_BOX_NEW:
			on_new_clicked( OFA_LEDGERS_PAGE( page ));
			break;
		case BUTTONS_BOX_PROPERTIES:
			on_update_clicked( OFA_LEDGERS_PAGE( page ));
			break;
		case BUTTONS_BOX_DELETE:
			on_delete_clicked( OFA_LEDGERS_PAGE( page ));
			break;
	}
}

static void
on_new_clicked( ofaLedgersPage *page )
{
	ofoLedger *ledger;

	ledger = ofo_ledger_new();

	if( ofa_ledger_properties_run(
			ofa_page_get_main_window( OFA_PAGE( page )), ledger )){

		/* this is managed by ofaLedgerTreeview convenience class
		 * graceful to dossier signaling system  */
		/*insert_new_row( OFA_LEDGERS_PAGE( page ), ledger, TRUE );*/

	} else {
		g_object_unref( ledger );
	}
}

static void
on_update_clicked( ofaLedgersPage *page )
{
	ofaLedgersPagePrivate *priv;
	GList *selected;

	priv = page->priv;

	selected = ofa_ledger_treeview_get_selected( priv->tview );

	do_update( page, OFO_LEDGER( selected->data ));

	ofa_ledger_treeview_free_selected( selected );
}

static void
do_update( ofaLedgersPage *self, ofoLedger *ledger )
{
	ofaLedgersPagePrivate *priv;
	GtkWidget *view;

	priv = self->priv;

	if( ledger &&
			ofa_ledger_properties_run(
					ofa_page_get_main_window( OFA_PAGE( self )), ledger )){

			/* this is managed by the ofaLedgerTreeview convenience
			 * class, graceful to the dossier signaling system */
	}

	view = ofa_ledger_treeview_get_top_focusable_widget( priv->tview );
	if( view ){
		gtk_widget_grab_focus( view );
	}
}

/*
 * un ledger peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
on_delete_clicked( ofaLedgersPage *page )
{
	ofaLedgersPagePrivate *priv;
	ofoDossier *dossier;
	ofoLedger *ledger;
	GList *selected;
	const gchar *mnemo;
	GtkWidget *view;

	priv = page->priv;
	dossier = ofa_page_get_dossier( OFA_PAGE( page ));

	selected = ofa_ledger_treeview_get_selected( priv->tview );
	mnemo = selected->data;
	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );
	ofa_ledger_treeview_free_selected( selected );

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( ofo_ledger_is_deletable( ledger, dossier ));

	if( delete_confirmed( page, ledger ) &&
			ofo_ledger_delete( ledger, ofa_page_get_dossier( OFA_PAGE( page )))){

		/* this is managed by the ofaLedgerTreeview convenience
		 * class, graceful to the dossier signaling system */
	}

	view = ofa_ledger_treeview_get_top_focusable_widget( priv->tview );
	if( view ){
		gtk_widget_grab_focus( view );
	}
}

static gboolean
delete_confirmed( ofaLedgersPage *self, ofoLedger *ledger )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' ledger ?" ),
			ofo_ledger_get_mnemo( ledger ),
			ofo_ledger_get_label( ledger ));

	delete_ok = ofa_main_window_confirm_deletion(
						ofa_page_get_main_window( OFA_PAGE( self )), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaLedgersPage *self )
{
	ofaLedgersPagePrivate *priv;
	GList *list;
	ofaPage *page;
	const gchar *mnemo;
	ofoLedger *ledger;

	g_return_if_fail( OFA_IS_LEDGERS_PAGE( self ));

	priv = self->priv;

	list = ofa_ledger_treeview_get_selected( priv->tview );
	mnemo = ( const gchar * ) list->data;
	ledger = ofo_ledger_get_by_mnemo( ofa_page_get_dossier( OFA_PAGE( self )), mnemo );
	ofa_ledger_treeview_free_selected( list );

	if( ledger ){
		page = ofa_main_window_activate_theme(
						ofa_page_get_main_window( OFA_PAGE( self )),
						THM_VIEW_ENTRIES );
		if( page ){
			ofa_view_entries_display_entries(
							OFA_VIEW_ENTRIES( page ),
							OFO_TYPE_LEDGER,
							ofo_ledger_get_mnemo( ledger ),
							NULL,
							NULL );
		}
	}
}
