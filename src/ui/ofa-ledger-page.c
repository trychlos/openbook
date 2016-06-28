/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-entry-page.h"
#include "ui/ofa-ledger-properties.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-ledger-page.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	gboolean            is_writable;
	gint                exe_id;			/* internal identifier of the current exercice */

	/* UI
	 */
	ofaLedgerTreeview  *tview;
	GtkWidget          *update_btn;
	GtkWidget          *delete_btn;
	GtkWidget          *entries_btn;
}
	ofaLedgerPagePrivate;

/* column ordering in the selection listview
 */
enum {
	COL_MNEMO = 0,
	COL_LABEL,
	COL_CLOSING,
	COL_OBJECT,
	N_COLUMNS
};

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_tree_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgerPage *self );
static void       on_row_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgerPage *self );
static void       on_insert_key( ofaLedgerTreeview *view, ofaLedgerPage *self );
static void       on_delete_key( ofaLedgerTreeview *view, GList *selected, ofaLedgerPage *self );
static void       on_new_clicked( GtkButton *button, ofaLedgerPage *page );
static void       on_update_clicked( GtkButton *button, ofaLedgerPage *page );
static void       do_update( ofaLedgerPage *self, ofoLedger *ledger );
static void       on_delete_clicked( GtkButton *button, ofaLedgerPage *page );
static gboolean   delete_confirmed( ofaLedgerPage *self, ofoLedger *ledger );
static void       on_entry_page( GtkButton *button, ofaLedgerPage *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerPage, ofa_ledger_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaLedgerPage ))

static void
ledgers_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_page_parent_class )->finalize( instance );
}

static void
ledgers_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_LEDGER_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_page_parent_class )->dispose( instance );
}

static void
ofa_ledger_page_init( ofaLedgerPage *self )
{
	static const gchar *thisfn = "ofa_ledger_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_PAGE( self ));
}

static void
ofa_ledger_page_class_init( ofaLedgerPageClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_ledger_page_v_setup_view";
	ofaLedgerPagePrivate *priv;
	GtkWidget *frame;
	ofaHub *hub;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));
	priv->is_writable = ofa_hub_dossier_is_writable( hub );

	frame = setup_tree_view( page );

	return( frame );
}

static GtkWidget *
setup_tree_view( ofaPage *page )
{
	ofaLedgerPagePrivate *priv;
	GtkWidget *parent;
	ofaHub *hub;

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	hub = ofa_igetter_get_hub( OFA_IGETTER( page ));

	parent = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_widget_set_margins( parent, 4, 4, 4, 0 );

	priv->tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_ledger_treeview_set_columns( priv->tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LABEL | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_hub( priv->tview, hub );
	ofa_ledger_treeview_set_selection_mode( priv->tview, GTK_SELECTION_BROWSE );

	g_signal_connect( priv->tview, "ofa-changed", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "ofa-activated", G_CALLBACK( on_row_activated ), page );
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), page );
	g_signal_connect( priv->tview, "ofa-delete", G_CALLBACK( on_delete_key ), page );

	return( parent );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaLedgerPagePrivate *priv;
	ofaButtonsBox *buttons_box;
	GtkWidget *btn;

	g_return_val_if_fail( page && OFA_IS_LEDGER_PAGE( page ), NULL );

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 4, 4, 0, 0 );

	btn = ofa_buttons_box_add_button_with_mnemonic(
				buttons_box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), page );
	gtk_widget_set_sensitive( btn, priv->is_writable );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	priv->entries_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "_View entries..." ), G_CALLBACK( on_entry_page ), page );

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaLedgerPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_LEDGER_PAGE( page ), NULL );

	priv = ofa_ledger_page_get_instance_private( OFA_LEDGER_PAGE( page ));

	return( ofa_ledger_treeview_get_treeview( priv->tview ));
}

/*
 * LedgerTreeview callback
 */
static void
on_row_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgerPage *self )
{
	ofoLedger *ledger;
	ofaHub *hub;

	if( selected ){
		hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
		ledger = ofo_ledger_get_by_mnemo( hub, ( const gchar * ) selected->data );
		g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

		do_update( self, ledger );
	}
}

/*
 * LedgerTreeview callback
 */
static void
on_row_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	ofoLedger *ledger;
	gboolean is_ledger;
	ofaHub *hub;

	priv = ofa_ledger_page_get_instance_private( self );

	ledger = NULL;
	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));

	if( selected ){
		ledger = ofo_ledger_get_by_mnemo( hub, ( const gchar * ) selected->data );
		g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	}

	is_ledger = ledger && OFO_IS_LEDGER( ledger );

	gtk_widget_set_sensitive( priv->update_btn,
			is_ledger );

	gtk_widget_set_sensitive( priv->delete_btn,
			priv->is_writable && is_ledger && ofo_ledger_is_deletable( ledger ));

	gtk_widget_set_sensitive( priv->entries_btn,
			is_ledger && ofo_ledger_has_entries( ledger ));
}

static void
on_insert_key( ofaLedgerTreeview *view, ofaLedgerPage *self )
{
	on_new_clicked( NULL, self );
}

/*
 * only delete if there is only one selected ledger
 */
static void
on_delete_key( ofaLedgerTreeview *view, GList *selected, ofaLedgerPage *self )
{
	if( g_list_length( selected ) == 1 ){
		on_delete_clicked( NULL, self );
	}
}

static void
on_new_clicked( GtkButton *button, ofaLedgerPage *self )
{
	ofoLedger *ledger;
	GtkWindow *toplevel;

	ledger = ofo_ledger_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ledger_properties_run( OFA_IGETTER( self ), toplevel, ledger );
}

static void
on_update_clicked( GtkButton *button, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	GList *selected;
	ofoLedger *ledger;
	ofaHub *hub;

	priv = ofa_ledger_page_get_instance_private( self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	selected = ofa_ledger_treeview_get_selected( priv->tview );
	ledger = ofo_ledger_get_by_mnemo( hub, ( const gchar * ) selected->data );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	ofa_ledger_treeview_free_selected( selected );

	do_update( self, ledger );
}

static void
do_update( ofaLedgerPage *self, ofoLedger *ledger )
{
	GtkWindow *toplevel;

	if( ledger ){
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
		ofa_ledger_properties_run( OFA_IGETTER( self ), toplevel, ledger );
	}
}

/*
 * un ledger peut être supprimé tant qu'aucune écriture n'y a été
 * enregistrée, et après confirmation de l'utilisateur
 */
static void
on_delete_clicked( GtkButton *button, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	ofoLedger *ledger;
	GList *selected;
	const gchar *mnemo;
	ofaHub *hub;

	priv = ofa_ledger_page_get_instance_private( self );

	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	selected = ofa_ledger_treeview_get_selected( priv->tview );
	mnemo = selected->data;
	ledger = ofo_ledger_get_by_mnemo( hub, mnemo );
	ofa_ledger_treeview_free_selected( selected );

	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));
	g_return_if_fail( ofo_ledger_is_deletable( ledger ));

	if( delete_confirmed( self, ledger ) &&
			ofo_ledger_delete( ledger )){

		/* this is managed by the ofaLedgerTreeview convenience
		 * class, graceful to the dossier signaling system */
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( self )));
}

static gboolean
delete_confirmed( ofaLedgerPage *self, ofoLedger *ledger )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' ledger ?" ),
			ofo_ledger_get_mnemo( ledger ),
			ofo_ledger_get_label( ledger ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
on_entry_page( GtkButton *button, ofaLedgerPage *self )
{
	ofaLedgerPagePrivate *priv;
	GList *list;
	ofaPage *page;
	const gchar *mnemo;
	ofoLedger *ledger;
	ofaIThemeManager *manager;
	ofaHub *hub;

	g_return_if_fail( self && OFA_IS_LEDGER_PAGE( self ));

	priv = ofa_ledger_page_get_instance_private( self );

	list = ofa_ledger_treeview_get_selected( priv->tview );
	g_return_if_fail( list && list->data );

	mnemo = ( const gchar * ) list->data;
	hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
	ledger = ofo_ledger_get_by_mnemo( hub, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	ofa_ledger_treeview_free_selected( list );

	manager = ofa_igetter_get_theme_manager( OFA_IGETTER( self ));
	page = ofa_itheme_manager_activate( manager, OFA_TYPE_ENTRY_PAGE );
	g_return_if_fail( page && OFA_IS_ENTRY_PAGE( page ));

	ofa_entry_page_display_entries(
			OFA_ENTRY_PAGE( page ), OFO_TYPE_LEDGER, ofo_ledger_get_mnemo( ledger ), NULL, NULL );
}
