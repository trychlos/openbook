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
#include "api/ofa-iactionable.h"
#include "api/ofa-iactioner.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-istore.h"
#include "api/ofa-itree-adder.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-settings.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-guided-input.h"
#include "core/ofa-ope-template-frame-bin.h"
#include "core/ofa-ope-template-properties.h"
#include "core/ofa-ope-template-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;

	/* runtime
	 */
	ofaHub              *hub;
	GList               *hub_handlers;
	gboolean             is_writable;
	ofaOpeTemplateStore *store;
	GList               *store_handlers;
	gchar               *settings_key;
	GtkWidget           *current_page;

	/* UI
	 */
	GtkWidget           *grid;
	GtkWidget           *notebook;
	ofaButtonsBox       *buttonsbox;

	/* actions
	 */
	GSimpleAction       *new_action;
	GSimpleAction       *update_action;
	GSimpleAction       *delete_action;
	GSimpleAction       *duplicate_action;
	GSimpleAction       *guided_input_action;
}
	ofaOpeTemplateFrameBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static void       setup_getter( ofaOpeTemplateFrameBin *self, ofaIGetter *getter );
static void       setup_bin( ofaOpeTemplateFrameBin *self );
static GtkWidget *book_get_page_by_ledger( ofaOpeTemplateFrameBin *self, const gchar *ledger, gboolean bcreate );
static GtkWidget *book_create_page( ofaOpeTemplateFrameBin *self, const gchar *ledger );
static void       book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplateFrameBin *self );
static void       tview_on_selection_changed( ofaOpeTemplateTreeview *view, ofoOpeTemplate *template, ofaOpeTemplateFrameBin *self );
static void       tview_on_selection_activated( ofaOpeTemplateTreeview *view, ofoOpeTemplate *template, ofaOpeTemplateFrameBin *self );
static void       tview_on_key_delete( ofaOpeTemplateTreeview *view, ofoOpeTemplate *template, ofaOpeTemplateFrameBin *self );
static void       tview_on_key_insert( ofaOpeTemplateTreeview *view, ofaOpeTemplateFrameBin *self );
static void       action_update_enabled( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self );
static void       action_on_duplicate_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self );
static void       action_on_guided_input_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self );
static gboolean   is_new_allowed( ofaOpeTemplateFrameBin *self );
static gboolean   is_delete_allowed( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *selected );
static void       do_insert_ope_template( ofaOpeTemplateFrameBin *self );
static void       do_update_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static void       do_delete_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static gboolean   delete_confirmed( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *ope );
static void       do_duplicate_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static void       do_guided_input( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static void       store_on_row_inserted( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOpeTemplateFrameBin *self );
static void       hub_connect_to_signaling_system( ofaOpeTemplateFrameBin *self );
static void       hub_on_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self );
static void       hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateFrameBin *self );
static void       hub_on_updated_ledger( ofaOpeTemplateFrameBin *self, const gchar *prev_id, ofoLedger *ledger );
static void       hub_on_updated_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template );
static void       hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self );
static void       hub_on_deleted_ledger_object( ofaOpeTemplateFrameBin *self, ofoLedger *ledger );
static void       hub_on_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateFrameBin *self );
static void       do_write_settings( ofaOpeTemplateFrameBin *self );
static void       iactionable_iface_init( ofaIActionableInterface *iface );
static guint      iactionable_get_interface_version( void );
static void       iactioner_iface_init( ofaIActionerInterface *iface );
static guint      iactioner_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateFrameBin, ofa_ope_template_frame_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaOpeTemplateFrameBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONER, iactioner_iface_init ))

static void
ope_template_frame_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_finalize";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_FRAME_BIN( instance ));

	priv = ofa_ope_template_frame_bin_get_instance_private( OFA_OPE_TEMPLATE_FRAME_BIN( instance ));

	/* free data members here */
	g_free( priv->settings_key );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_frame_bin_parent_class )->finalize( instance );
}

static void
ope_template_frame_bin_dispose( GObject *instance )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_FRAME_BIN( instance ));

	priv = ofa_ope_template_frame_bin_get_instance_private( OFA_OPE_TEMPLATE_FRAME_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* disconnect from ofaHub signaling system */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );

		/* disconnect from ofaOpeTemplateStore */
		if( priv->store && OFA_IS_OPE_TEMPLATE_STORE( priv->store )){
			for( it=priv->store_handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( priv->store, ( gulong ) it->data );
			}
		}
		if( priv->store_handlers ){
			g_list_free( priv->store_handlers );
			priv->store_handlers = NULL;
		}
		g_clear_object( &priv->store );

		g_clear_object( &priv->new_action );
		g_clear_object( &priv->update_action );
		g_clear_object( &priv->delete_action );
		g_clear_object( &priv->duplicate_action );
		g_clear_object( &priv->guided_input_action );

		/* we expect that the last page seen by the user is those which
		 * has the better sizes and positions for the columns */
		ofa_itvcolumnable_write_columns_settings( OFA_ITVCOLUMNABLE( priv->current_page ));
		do_write_settings( OFA_OPE_TEMPLATE_FRAME_BIN( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_frame_bin_parent_class )->dispose( instance );
}

static void
ofa_ope_template_frame_bin_init( ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_init";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_FRAME_BIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_key = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->current_page = NULL;
}

static void
ofa_ope_template_frame_bin_class_init( ofaOpeTemplateFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_frame_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_frame_bin_finalize;

	/**
	 * ofaOpeTemplateFrameBin::ofa-changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected operation template object, or %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *frame,
	 * 							ofoOpeTemplate   *template,
	 * 							gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaOpeTemplateFrameBin::ofa-activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected operation template object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateFrameBin *frame,
	 * 							ofoOpeTemplate   *template,
	 * 							gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_OPE_TEMPLATE_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofa_ope_template_frame_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Creates the structured content, i.e. the operation templates notebook
 * on the left column, the buttons box on the right one.
 *
 * +-----------------------------------------------------------------------+
 * | parent container:                                                     |
 * |   this is the grid of the main page,                                  |
 * |   or any another container (i.e. a frame)                             |
 * | +-------------------------------------------------------------------+ |
 * | | creates a grid which will contain the frame and the buttons       | |
 * | | +---------------------------------------------+-----------------+ + |
 * | | | creates a notebook where each page contains | creates         | | |
 * | | |   the account of the corresponding class    |   a buttons box | | |
 * | | |   (cf. ofaOpeTemplateFrameBin class)        |                 | | |
 * | | |                                             |                 | | |
 * | | +---------------------------------------------+-----------------+ | |
 * | +-------------------------------------------------------------------+ |
 * +-----------------------------------------------------------------------+
 */
ofaOpeTemplateFrameBin *
ofa_ope_template_frame_bin_new( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_new";
	ofaOpeTemplateFrameBin *self;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	self = g_object_new( OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, NULL );

	setup_getter( self, getter );
	setup_bin( self );

	return( self );
}

/*
 * Record the getter
 * Initialize private datas which depend of only getter
 */
static void
setup_getter( ofaOpeTemplateFrameBin *self, ofaIGetter *getter )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gulong handler;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	priv->getter = getter;

	/* hub-related initialization */
	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));
	priv->is_writable = ofa_hub_dossier_is_writable( priv->hub );
	hub_connect_to_signaling_system( self );

	/* then initialize the store */
	priv->store = ofa_ope_template_store_new( priv->hub );
	handler = g_signal_connect( priv->store, "ofa-row-inserted", G_CALLBACK( store_on_row_inserted ), self );
	priv->store_handlers = g_list_prepend( priv->store_handlers, ( gpointer ) handler );
}

/*
 * Create the top grid which contains the ope_templates notebook and
 * the buttons box, and attach it to our #GtkBin
 *
 * Note that each page of the notebook is created on the fly, when an
 * ope template for this page is inserted in the store.
 *
 * Each page of the notebook presents the operation templates attached
 * to a given ledger.
 */
static void
setup_bin( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	/* UI grid */
	priv->grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( self ), priv->grid );

	/* UI notebook */
	priv->notebook = gtk_notebook_new();
	gtk_notebook_popup_enable( GTK_NOTEBOOK( priv->notebook ));
	gtk_notebook_set_scrollable( GTK_NOTEBOOK( priv->notebook ), TRUE );
	gtk_notebook_set_show_tabs( GTK_NOTEBOOK( priv->notebook ), TRUE );
	gtk_grid_attach( GTK_GRID( priv->grid ), priv->notebook, 0, 0, 1, 1 );

	g_signal_connect( priv->notebook, "switch-page", G_CALLBACK( book_on_page_switched ), self );

	/* UI buttons box */
	priv->buttonsbox = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( priv->buttonsbox ), 0, 0, 2, 2 );
	gtk_grid_attach( GTK_GRID( priv->grid ), GTK_WIDGET( priv->buttonsbox ), 1, 0, 1, 1 );
}

/*
 * Returns the notebook's page container which is dedicated to the
 * given ledger.
 *
 * If the page doesn't exist, and @bcreate is %TRUE, then it is created.
 */
static GtkWidget *
book_get_page_by_ledger( ofaOpeTemplateFrameBin *self, const gchar *ledger, gboolean bcreate )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_get_page_by_ledger";
	ofaOpeTemplateFrameBinPrivate *priv;
	gint count, i;
	GtkWidget *found, *page_widget;
	const gchar *page_ledger;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	found = NULL;
	count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ));

	/* search for an existing page */
	for( i=0 ; i<count ; ++i ){
		page_widget = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i );
		g_return_val_if_fail( page_widget && OFA_IS_OPE_TEMPLATE_TREEVIEW( page_widget ), NULL );
		page_ledger = ofa_ope_template_treeview_get_ledger( OFA_OPE_TEMPLATE_TREEVIEW( page_widget ));
		if( !my_collate( page_ledger, ledger )){
			found = page_widget;
			break;
		}
	}

	/* if not exists, create it (if allowed) */
	if( !found && bcreate ){
		found = book_create_page( self, ledger );
		if( !found ){
			g_warning( "%s: unable to create the page for ledger=%s", thisfn, ledger );
			return( NULL );
		} else {
			gtk_widget_show_all( found );
		}
	}

	return( found );
}

/*
 * creates the page widget for the given ledger
 */
static GtkWidget *
book_create_page( ofaOpeTemplateFrameBin *self, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_book_create_page";
	ofaOpeTemplateFrameBinPrivate *priv;
	ofaOpeTemplateTreeview *view;
	GtkWidget *label;
	ofoLedger *ledger_obj;
	const gchar *ledger_label;
	gint page_num;
	GMenu *menu;

	g_debug( "%s: self=%p, ledger=%s", thisfn, ( void * ) self, ledger );

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	/* get ledger label */
	if( !my_collate( ledger, UNKNOWN_LEDGER_MNEMO )){
		ledger_label = UNKNOWN_LEDGER_LABEL;
		ledger_obj = NULL;

	} else {
		ledger_obj = ofo_ledger_get_by_mnemo( priv->hub, ledger );
		if( ledger_obj ){
			g_return_val_if_fail( OFO_IS_LEDGER( ledger_obj ), NULL );
			ledger_label = ofo_ledger_get_label( ledger_obj );
		} else {
			g_warning( "%s: ledger not found: %s", thisfn, ledger );
			return( NULL );
		}
	}

	view = ofa_ope_template_treeview_new( ledger );
	ofa_ope_template_treeview_set_settings_key( view, priv->settings_key );
	ofa_ope_template_treeview_setup_columns( view );
	ofa_istore_add_columns( OFA_ISTORE( priv->store ), OFA_TVBIN( view ));
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));

	g_signal_connect( view, "ofa-opechanged", G_CALLBACK( tview_on_selection_changed ), self );
	g_signal_connect( view, "ofa-opeactivated", G_CALLBACK( tview_on_selection_activated ), self );
	g_signal_connect( view, "ofa-opedelete", G_CALLBACK( tview_on_key_delete ), self );
	g_signal_connect( view, "ofa-insert", G_CALLBACK( tview_on_key_insert ), self );

	/* last add the page to the notebook */
	label = gtk_label_new( ledger_label );

	page_num = gtk_notebook_append_page( GTK_NOTEBOOK( priv->notebook ), GTK_WIDGET( view ), label );
	if( page_num == -1 ){
		g_warning( "%s: unable to add a page to the notebook for ledger=%s", thisfn, ledger );
		g_object_unref( view );
		return( NULL );
	}
	gtk_notebook_set_tab_reorderable( GTK_NOTEBOOK( priv->notebook ), GTK_WIDGET( view ), TRUE );

	/* create a new context menu for each page of the notebook */
	menu = g_menu_new();
	g_menu_append_section( menu, NULL,
			G_MENU_MODEL( ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_key )));
	ofa_icontext_set_menu(
			OFA_ICONTEXT( view ), OFA_IACTIONABLE( self ),
			menu );
	g_object_unref( menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( view ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( view ), OFA_IACTIONABLE( view ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* proxy and sync of action messages */
	ofa_iactioner_register_actionable( OFA_IACTIONER( self ), OFA_IACTIONABLE( view ));

	return( GTK_WIDGET( view ));
}

/*
 * we have switch to this given page (wpage, npage)
 * just setup the selection
 *
 * At this time, GtkNotebook current page is not yet set. We so cannot
 * rely on it.
 */
static void
book_on_page_switched( GtkNotebook *book, GtkWidget *wpage, guint npage, ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *template;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	priv->current_page = wpage;
	template = ofa_ope_template_treeview_get_selected( OFA_OPE_TEMPLATE_TREEVIEW( wpage ));
	tview_on_selection_changed( OFA_OPE_TEMPLATE_TREEVIEW( wpage ), template, self );
}

/**
 * ofa_ope_template_frame_bin_get_current_page:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 *
 * Returns the current page of the notebook, which happens to be an
 * #ofaOpeTemplateTreeview.
 */
GtkWidget *
ofa_ope_template_frame_bin_get_current_page( ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->current_page );
}

/**
 * ofa_ope_template_frame_bin_get_pages_list:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 *
 * Returns the list of #ofaOpeTemplateTreeview pages.
 *
 * The returned list should be g_list_free() by the caller.
 */
GList *
ofa_ope_template_frame_bin_get_pages_list( ofaOpeTemplateFrameBin *bin )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GList *list;
	gint i, count;

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	list = NULL;
	count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ));
	for( i=0 ; i<count ; ++i ){
		list = g_list_prepend( list, gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i ));
	}

	return( list );
}

/**
 * ofa_ope_template_frame_bin_get_selected:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 *
 * Returns: the currently selected operation template, as a newly
 * allocated string which should be g_free() by the caller.
 */
ofoOpeTemplate *
ofa_ope_template_frame_bin_get_selected( ofaOpeTemplateFrameBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_get_selected";
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *template;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ), NULL );

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->current_page, NULL );

	template = ofa_ope_template_treeview_get_selected( OFA_OPE_TEMPLATE_TREEVIEW( priv->current_page ));

	return( template );
}

/**
 * ofa_ope_template_frame_bin_set_selected:
 * @bin: this #ofaOpeTemplateFrameBin instance
 * @mnemo: the operation template mnemonic to be selected.
 *
 * Let the user reset the selection after the end of setup and
 * initialization phases
 */
void
ofa_ope_template_frame_bin_set_selected( ofaOpeTemplateFrameBin *bin, const gchar *mnemo )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_set_selected";
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *page_w;
	gint page_n;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_strlen( mnemo )){
		page_w = book_get_page_by_ledger( bin, mnemo, FALSE );
		if( !page_w ){
			return;
		}
		g_return_if_fail( OFA_IS_OPE_TEMPLATE_TREEVIEW( page_w ));

		page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
		gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), page_n );

		ofa_ope_template_treeview_set_selected( OFA_OPE_TEMPLATE_TREEVIEW( page_w ), mnemo );
	}
}

static void
tview_on_selection_changed( ofaOpeTemplateTreeview *view, ofoOpeTemplate *template, ofaOpeTemplateFrameBin *self )
{
	g_return_if_fail( !template || OFO_IS_OPE_TEMPLATE( template ));

	action_update_enabled( self, template );

	g_signal_emit_by_name( self, "ofa-changed", template );
}

static void
tview_on_selection_activated( ofaOpeTemplateTreeview *view, ofoOpeTemplate *template, ofaOpeTemplateFrameBin *self )
{
	g_return_if_fail( template && OFO_IS_OPE_TEMPLATE( template ));

	g_signal_emit_by_name( self, "ofa-activated", template );
}

static void
tview_on_key_delete( ofaOpeTemplateTreeview *view, ofoOpeTemplate *template, ofaOpeTemplateFrameBin *self )
{
	g_return_if_fail( !template || OFO_IS_OPE_TEMPLATE( template ));

	if( template && is_delete_allowed( self, template )){
		do_delete_ope_template( self, template );
	}
}

static void
tview_on_key_insert( ofaOpeTemplateTreeview *view, ofaOpeTemplateFrameBin *self )
{
	if( is_new_allowed( self )){
		do_insert_ope_template( self );
	}
}

/**
 * ofa_ope_template_frame_bin_add_action:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 * @id: the action identifier.
 *
 * Create a new button in the #ofaButtonsBox, and define a menu item
 * for the contextual menu.
 */
void
ofa_ope_template_frame_bin_add_action( ofaOpeTemplateFrameBin *bin, ofeOpeTemplateAction id )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_add_action";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, id=%u", thisfn, ( void * ) bin, id );

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	switch( id ){
		case TEMPLATE_ACTION_SPACER:
			ofa_buttons_box_add_spacer( priv->buttonsbox );
			break;

		case TEMPLATE_ACTION_NEW:
			priv->new_action = g_simple_action_new( "new", NULL );
			g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), bin );
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->new_action ),
					OFA_IACTIONABLE_NEW_ITEM );
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_new_button(
							OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->new_action ),
							OFA_IACTIONABLE_NEW_BTN ));
			g_simple_action_set_enabled( priv->new_action, priv->is_writable );
			break;

		case TEMPLATE_ACTION_PROPERTIES:
			priv->update_action = g_simple_action_new( "update", NULL );
			g_signal_connect( priv->update_action, "activate", G_CALLBACK( action_on_update_activated ), bin );
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->update_action ),
					priv->is_writable ? OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT : OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY );
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_new_button(
							OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->update_action ),
							OFA_IACTIONABLE_PROPERTIES_BTN ));
			g_simple_action_set_enabled( priv->update_action, FALSE );
			break;

		case TEMPLATE_ACTION_DELETE:
			priv->delete_action = g_simple_action_new( "delete", NULL );
			g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), bin );
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_ITEM );
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_new_button(
							OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->delete_action ),
							OFA_IACTIONABLE_DELETE_BTN ));
			g_simple_action_set_enabled( priv->delete_action, FALSE );
			break;

		case TEMPLATE_ACTION_DUPLICATE:
			priv->duplicate_action = g_simple_action_new( "duplicate", NULL );
			g_signal_connect( priv->duplicate_action, "activate", G_CALLBACK( action_on_duplicate_activated ), bin );
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->duplicate_action ),
					_( "Duplicate this" ));
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_new_button(
							OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->duplicate_action ),
							_( "_Duplicate" )));
			g_simple_action_set_enabled( priv->duplicate_action, FALSE );
			break;

		case TEMPLATE_ACTION_GUIDED_INPUT:
			priv->guided_input_action = g_simple_action_new( "guided-input", NULL );
			g_signal_connect( priv->guided_input_action, "activate", G_CALLBACK( action_on_guided_input_activated ), bin );
			ofa_iactionable_set_menu_item(
					OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->guided_input_action ),
					_( "Guided input" ));
			ofa_buttons_box_append_button(
					priv->buttonsbox,
					ofa_iactionable_new_button(
							OFA_IACTIONABLE( bin ), priv->settings_key, G_ACTION( priv->guided_input_action ),
							_( "_Guided input" )));
			g_simple_action_set_enabled( priv->guided_input_action, FALSE );
			break;

		default:
			break;
	}
}

static void
action_update_enabled( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gboolean has_template;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	has_template = template && OFO_IS_OPE_TEMPLATE( template );

	if( priv->new_action ){
		g_simple_action_set_enabled( priv->new_action, is_new_allowed( self ));
	}
	if( priv->update_action ){
		g_simple_action_set_enabled( priv->update_action, has_template );
	}
	if( priv->delete_action ){
		g_simple_action_set_enabled( priv->delete_action, is_delete_allowed( self, template ));
	}
	if( priv->duplicate_action ){
		g_simple_action_set_enabled( priv->duplicate_action, has_template && is_new_allowed( self ));
	}
	if( priv->guided_input_action ){
		g_simple_action_set_enabled( priv->guided_input_action, has_template && is_new_allowed( self ));
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self )
{
	do_insert_ope_template( self );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self )
{
	ofoOpeTemplate *template;

	template = ofa_ope_template_frame_bin_get_selected( self );
	if( template ){
		do_update_ope_template( self, template );
	}
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self )
{
	ofoOpeTemplate *template;

	template = ofa_ope_template_frame_bin_get_selected( self );
	if( template ){
		g_return_if_fail( is_delete_allowed( self, template ));
		do_delete_ope_template( self, template );
	}
}

static void
action_on_duplicate_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self )
{
	ofoOpeTemplate *template;

	template = ofa_ope_template_frame_bin_get_selected( self );
	if( template ){
		do_duplicate_ope_template( self, template );
	}
}

static void
action_on_guided_input_activated( GSimpleAction *action, GVariant *empty, ofaOpeTemplateFrameBin *self )
{
	ofoOpeTemplate *template;

	template = ofa_ope_template_frame_bin_get_selected( self );
	if( template ){
		do_guided_input( self, template );
	}
}

static gboolean
is_new_allowed( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ok = priv->is_writable;

	return( ok );
}

static gboolean
is_delete_allowed( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gboolean ok;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	ok = priv->is_writable && template && ofo_ope_template_is_deletable( template );

	if( template ){
		g_debug( "template=%s, is_deletable=%s",
				ofo_ope_template_get_mnemo( template ),
				ofo_ope_template_is_deletable( template ) ? "True":"False" );
	}

	return( ok );
}

static void
do_insert_ope_template( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *ope;
	gint page_n;
	GtkWidget *page_w;
	GtkWindow *toplevel;
	const gchar *page_ledger;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	page_ledger = NULL;

	page_n = gtk_notebook_get_current_page( GTK_NOTEBOOK( priv->notebook ));
	if( page_n >= 0 ){
		page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), page_n );
		g_return_if_fail( page_w && OFA_IS_OPE_TEMPLATE_TREEVIEW( page_w ));
		page_ledger = ofa_ope_template_treeview_get_ledger( OFA_OPE_TEMPLATE_TREEVIEW( page_w ));
	}

	ope = ofo_ope_template_new();
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ope_template_properties_run( priv->getter, toplevel, ope, page_ledger );
}

static void
do_update_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_ope_template_properties_run( priv->getter, toplevel, template, NULL );
}

static void
do_delete_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	gchar *mnemo;

	g_return_if_fail( is_delete_allowed( self, template ));

	mnemo = g_strdup( ofo_ope_template_get_mnemo( template ));

	if( delete_confirmed( self, template ) &&
			ofo_ope_template_delete( template )){

		/* nothing to do here, all being managed by signal hub_handlers
		 * just reset the selection as this is not managed by the
		 * ope notebook (and doesn't have to)
		 * asking for selection of the just deleted ope makes
		 * almost sure that we are going to select the most close
		 * row */
		ofa_ope_template_frame_bin_set_selected( self, mnemo );
	}

	g_free( mnemo );
}

/*
 * - this is a root account with children and the preference is set so
 *   that all accounts will be deleted
 * - this is a root account and the preference is not set
 * - this is a detail account
 */
static gboolean
delete_confirmed( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *ope )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want to delete the '%s - %s' entry model ?" ),
			ofo_ope_template_get_mnemo( ope ),
			ofo_ope_template_get_label( ope ));

	delete_ok = my_utils_dialog_question( msg, _( "_Delete" ));

	g_free( msg );

	return( delete_ok );
}

static void
do_duplicate_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	ofoOpeTemplate *duplicate;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	duplicate = ofo_ope_template_new_from_template( template );

	if( !ofo_ope_template_insert( duplicate, priv->hub )){
		g_object_unref( duplicate );
	}
}

static void
do_guided_input( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWindow *toplevel;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_guided_input_run( priv->getter, toplevel, template );
}

/*
 * is triggered by the store when a row is inserted
 */
static void
store_on_row_inserted( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_store_on_row_inserted";
	ofoOpeTemplate *ope;
	const gchar *ledger;

	gtk_tree_model_get( tmodel, iter, OPE_TEMPLATE_COL_OBJECT, &ope, -1 );
	g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));
	g_object_unref( ope );

	if( 0 ){
		g_debug( "%s: tmodel=%p, iter=%p, self=%p, ope_template=%s",
				thisfn, ( void * ) tmodel, ( void * ) iter, ( void * ) self,
				ofo_ope_template_get_mnemo( ope ));
	}

	ledger = ofo_ope_template_get_ledger( ope );
	if( !book_get_page_by_ledger( self, ledger, TRUE )){
		book_get_page_by_ledger( self, UNKNOWN_LEDGER_MNEMO, TRUE );
	}
}

/**
 * ofa_ope_template_frame_bin_set_settings_key:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_ope_template_frame_bin_set_settings_key( ofaOpeTemplateFrameBin *bin, const gchar *key )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_set_settings_key";
	ofaOpeTemplateFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, key=%s", thisfn, ( void * ) bin, key );

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->settings_key );
	priv->settings_key = g_strdup( my_strlen( key ) ? key : G_OBJECT_TYPE_NAME( bin ));
}

/**
 * ofa_ope_template_frame_bin_load_dataset:
 * @bin: this #ofaOpeTemplateFrameBin instance.
 *
 * Load the dataset.
 */
void
ofa_ope_template_frame_bin_load_dataset( ofaOpeTemplateFrameBin *bin )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_load_dataset";
	ofaOpeTemplateFrameBinPrivate *priv;
	GList *strlist, *it;
	gchar *key;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_if_fail( bin && OFA_IS_OPE_TEMPLATE_FRAME_BIN( bin ));

	priv = ofa_ope_template_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	/* create one page per ledger
	 * if strlist is set, then create one page per ledger
	 * other needed pages will be created on fly
	 * nb: if the ledger no more exists, no page is created */
	key = g_strdup_printf( "%s-pages", priv->settings_key );
	strlist = ofa_settings_user_get_string_list( key );
	for( it=strlist ; it ; it=it->next ){
		book_get_page_by_ledger( bin, ( const gchar * ) it->data, FALSE );
	}
	ofa_settings_free_string_list( strlist );
	g_free( key );

	ofa_istore_load_dataset( OFA_ISTORE( priv->store ));

	gtk_notebook_set_current_page( GTK_NOTEBOOK( priv->notebook ), 0 );
}

static void
hub_connect_to_signaling_system( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	gulong handler;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), prev_id, ( void * ) self );

	if( OFO_IS_LEDGER( object )){
		hub_on_updated_ledger( self, prev_id, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		hub_on_updated_ope_template( self, OFO_OPE_TEMPLATE( object ));
	}
}

/*
 * a ledger identifier and/or label has changed : update the
 * corresponding tab
 */
static void
hub_on_updated_ledger( ofaOpeTemplateFrameBin *self, const gchar *prev_id, ofoLedger *ledger )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *page_w;
	const gchar *mnemo, *new_id;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	new_id = ofo_ledger_get_mnemo( ledger );
	mnemo = prev_id ? prev_id : new_id;
	page_w = book_get_page_by_ledger( self, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		gtk_notebook_set_tab_label_text( GTK_NOTEBOOK( priv->notebook ), page_w, ofo_ledger_get_label( ledger ));
	}
}

/*
 * we do not have any way to know if the ledger attached to the operation
 *  template has changed or not - So just make sure the correct page is
 *  shown
 */
static void
hub_on_updated_ope_template( ofaOpeTemplateFrameBin *self, ofoOpeTemplate *template )
{
	ofa_ope_template_frame_bin_set_selected( self, ofo_ope_template_get_mnemo( template ));
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_deleted_object";

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn, ( void * ) hub,
					( void * ) object, G_OBJECT_TYPE_NAME( object ), ( void * ) self );

	if( OFO_IS_LEDGER( object )){
		hub_on_deleted_ledger_object( self, OFO_LEDGER( object ));
	}
}

static void
hub_on_deleted_ledger_object( ofaOpeTemplateFrameBin *self, ofoLedger *ledger )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GtkWidget *page_w;
	gint page_n;
	const gchar *mnemo;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	mnemo = ofo_ledger_get_mnemo( ledger );
	page_w = book_get_page_by_ledger( self, mnemo, FALSE );
	if( page_w ){
		g_return_if_fail( GTK_IS_WIDGET( page_w ));
		page_n = gtk_notebook_page_num( GTK_NOTEBOOK( priv->notebook ), page_w );
		gtk_notebook_remove_page( GTK_NOTEBOOK( priv->notebook ), page_n );
		book_get_page_by_ledger( self, UNKNOWN_LEDGER_MNEMO, TRUE );
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaOpeTemplateFrameBin *self )
{
	static const gchar *thisfn = "ofa_ope_template_frame_bin_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );
}

static void
do_write_settings( ofaOpeTemplateFrameBin *self )
{
	ofaOpeTemplateFrameBinPrivate *priv;
	GList *strlist;
	gint i, count;
	GtkWidget *page_w;
	gchar *key;
	const gchar *page_ledger;

	priv = ofa_ope_template_frame_bin_get_instance_private( self );

	key = g_strdup_printf( "%s-pages", priv->settings_key );
	strlist = NULL;

	/* record in settings the pages position */
	count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( priv->notebook ) );
	for( i=0 ; i<count ; ++i ){
		page_w = gtk_notebook_get_nth_page( GTK_NOTEBOOK( priv->notebook ), i );
		g_return_if_fail( page_w && OFA_IS_OPE_TEMPLATE_TREEVIEW( page_w ));
		page_ledger = ofa_ope_template_treeview_get_ledger( OFA_OPE_TEMPLATE_TREEVIEW( page_w ));
		if( my_collate( page_ledger, UNKNOWN_LEDGER_MNEMO )){
			strlist = g_list_append( strlist, ( gpointer ) page_ledger );
		}
	}

	ofa_settings_user_set_string_list( key, strlist );

	g_list_free( strlist );
	g_free( key );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_account_frame_bin_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaIActioner interface management
 */
static void
iactioner_iface_init( ofaIActionerInterface *iface )
{
	static const gchar *thisfn = "ofa_account_frame_bin_iactioner_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactioner_get_interface_version;
}

static guint
iactioner_get_interface_version( void )
{
	return( 1 );
}
