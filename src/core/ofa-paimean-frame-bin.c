/*
 * Open Firm Paimeaning
 * A double-entry paimeaning application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Paimeaning is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Paimeaning is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Paimeaning; see the file COPYING. If not,
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
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-istore.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"
#include "api/ofo-paimean.h"

#include "core/ofa-paimean-frame-bin.h"
#include "core/ofa-paimean-properties.h"
#include "core/ofa-paimean-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;

	/* runtime
	 */
	gboolean            is_writable;		/* whether the dossier is writable */
	gchar              *settings_prefix;

	/* UI
	 */
	ofaPaimeanTreeview *tview;
	ofaButtonsBox      *buttonsbox;

	/* actions
	 */
	GSimpleAction      *new_action;
	GSimpleAction      *update_action;
	GSimpleAction      *delete_action;
}
	ofaPaimeanFrameBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void       setup_getter( ofaPaimeanFrameBin *self, ofaIGetter *getter );
static void       setup_bin( ofaPaimeanFrameBin *self );
static void       setup_actions( ofaPaimeanFrameBin *self );
static void       init_view( ofaPaimeanFrameBin *self );
static void       on_row_selected( ofaPaimeanTreeview *tview, ofoPaimean *paimean, ofaPaimeanFrameBin *self );
static void       on_row_activated( ofaPaimeanTreeview *tview, ofoPaimean *paimean, ofaPaimeanFrameBin *self );
static void       on_delete_key( ofaPaimeanTreeview *tview, ofoPaimean *paimean, ofaPaimeanFrameBin *self );
static void       on_insert_key( ofaPaimeanTreeview *tview, ofaPaimeanFrameBin *self );
static void       action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaPaimeanFrameBin *self );
static void       action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaPaimeanFrameBin *self );
static void       action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaPaimeanFrameBin *self );
static gboolean   check_for_deletability( ofaPaimeanFrameBin *self, ofoPaimean *paimean );
static void       delete_with_confirm( ofaPaimeanFrameBin *self, ofoPaimean *paimean );
static void       iactionable_iface_init( ofaIActionableInterface *iface );
static guint      iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaPaimeanFrameBin, ofa_paimean_frame_bin, GTK_TYPE_BIN, 0, \
		G_ADD_PRIVATE( ofaPaimeanFrameBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ));

static void
paimean_frame_bin_finalize( GObject *instance )
{
	ofaPaimeanFrameBinPrivate *priv;
	static const gchar *thisfn = "ofa_paimean_frame_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAIMEAN_FRAME_BIN( instance ));

	priv = ofa_paimean_frame_bin_get_instance_private( OFA_PAIMEAN_FRAME_BIN( instance ));

	/* free data members here */
	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_frame_bin_parent_class )->finalize( instance );
}

static void
paimean_frame_bin_dispose( GObject *instance )
{
	ofaPaimeanFrameBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAIMEAN_FRAME_BIN( instance ));

	priv = ofa_paimean_frame_bin_get_instance_private( OFA_PAIMEAN_FRAME_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->new_action );
		g_clear_object( &priv->update_action );
		g_clear_object( &priv->delete_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_frame_bin_parent_class )->dispose( instance );
}

static void
ofa_paimean_frame_bin_init( ofaPaimeanFrameBin *self )
{
	static const gchar *thisfn = "ofa_paimean_frame_bin_init";
	ofaPaimeanFrameBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PAIMEAN_FRAME_BIN( self ));

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_paimean_frame_bin_class_init( ofaPaimeanFrameBinClass *klass )
{
	static const gchar *thisfn = "ofa_paimean_frame_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_frame_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_frame_bin_finalize;

	/**
	 * ofaPaimeanFrameBin::ofa-changed:
	 *
	 * This signal is sent when the selection is changed.
	 *
	 * Argument is the selected paimean. It may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPaimeanFrameBin *frame,
	 * 						ofoPaimean       *paimean,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_PAIMEAN_FRAME_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaPaimeanFrameBin::ofa-activated:
	 *
	 * This signal is sent when the selection is activated.
	 *
	 * Argument is the selected paimean.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPaimeanFrameBin *frame,
	 * 						ofoPaimean       *paimean,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-activated",
				OFA_TYPE_PAIMEAN_FRAME_BIN,
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
 * ofa_paimean_frame_bin_new:
 * @getter: a #ofaIGetter instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Creates the structured content, i.e. the paimeans treeview on the
 * left column, the buttons box on the right one.
 *
 *   +-------------------------------------------------------------------+
 *   | creates a grid which will contain the frame and the buttons       |
 *   | +---------------------------------------------+-----------------+ +
 *   | | creates a treeview                          | creates         | |
 *   | |                                             |   a buttons box | |
 *   | |                                             |                 | |
 *   | +---------------------------------------------+-----------------+ |
 *   +-------------------------------------------------------------------+
 * +-----------------------------------------------------------------------+
 */
ofaPaimeanFrameBin *
ofa_paimean_frame_bin_new( ofaIGetter *getter, const gchar *key  )
{
	static const gchar *thisfn = "ofa_paimean_frame_bin_new";
	ofaPaimeanFrameBin *self;
	ofaPaimeanFrameBinPrivate *priv;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	self = g_object_new( OFA_TYPE_PAIMEAN_FRAME_BIN, NULL );

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( my_strlen( key ) ? key : G_OBJECT_TYPE_NAME( self ));

	setup_getter( self, getter );
	setup_bin( self );
	setup_actions( self );
	init_view( self );

	return( self );
}

/*
 * Record the getter
 * Initialize private datas which depend only of getter
 */
static void
setup_getter( ofaPaimeanFrameBin *self, ofaIGetter *getter )
{
	ofaPaimeanFrameBinPrivate *priv;
	ofaHub *hub;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	priv->getter = getter;

	hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv->is_writable = ofa_hub_is_writable_dossier( hub );
}

/*
 * Create the top grid which contains the paimeans treeview and the
 * buttons box, and attach it to our #GtkBin
 *
 * Note that each self of the notebook is created on the fly, when an
 * paimean for this self is inserted in the store.
 *
 * Each self of the notebook presents the paimeans of a given class.
 */
static void
setup_bin( ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;
	GtkWidget *grid;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	/* UI grid */
	grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( self ), grid );

	/* treeview */
	priv->tview = ofa_paimean_treeview_new( priv->getter );
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( priv->tview ), 0, 0, 1, 1 );
	ofa_paimean_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_paimean_treeview_setup_columns( priv->tview );

	/* ofaTVBin signals */
	g_signal_connect( priv->tview, "ofa-insert", G_CALLBACK( on_insert_key ), self );

	/* ofaPaimeanTreeview signals */
	g_signal_connect( priv->tview, "ofa-pamchanged", G_CALLBACK( on_row_selected ), self );
	g_signal_connect( priv->tview, "ofa-pamactivated", G_CALLBACK( on_row_activated ), self );
	g_signal_connect( priv->tview, "ofa-pamdelete", G_CALLBACK( on_delete_key ), self );

	/* UI buttons box */
	priv->buttonsbox = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( priv->buttonsbox ), 0, 0, 2, 2 );
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( priv->buttonsbox ), 1, 0, 1, 1 );
}

static void
setup_actions( ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			priv->buttonsbox,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->new_action ),
					OFA_IACTIONABLE_NEW_BTN ));
	g_simple_action_set_enabled( priv->new_action, priv->is_writable );

	/* update action */
	priv->update_action = g_simple_action_new( "update", NULL );
	g_signal_connect( priv->update_action, "activate", G_CALLBACK( action_on_update_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->update_action ),
			priv->is_writable ? OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT : OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY );
	ofa_buttons_box_append_button(
			priv->buttonsbox,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->update_action ),
					OFA_IACTIONABLE_PROPERTIES_BTN ));

	/* delete action */
	priv->delete_action = g_simple_action_new( "delete", NULL );
	g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->delete_action ),
			OFA_IACTIONABLE_DELETE_ITEM );
	ofa_buttons_box_append_button(
			priv->buttonsbox,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->delete_action ),
					OFA_IACTIONABLE_DELETE_BTN ));
}

static void
init_view( ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;
	GMenu *menu;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );

	/* install the store at the very end of the initialization
	 * (i.e. after treeview creation, signals connection, actions and
	 *  menus definition) */
	ofa_paimean_treeview_setup_store( priv->tview );
}

/**
 * ofa_paimean_frame_bin_get_tree_view:
 * @bin: this #ofaPaimeanFrameBin instance.
 *
 * Returns: the #GtkTreeView.
 */
GtkWidget *
ofa_paimean_frame_bin_get_tree_view( ofaPaimeanFrameBin *bin )
{
	ofaPaimeanFrameBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_PAIMEAN_FRAME_BIN( bin ), NULL );

	priv = ofa_paimean_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->tview ? ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )) : NULL );
}

/**
 * ofa_paimean_frame_bin_get_selected:
 * @bin:
 *
 * Returns: the currently selected paimean.
 *
 * The returned reference is owned by the underlying #ofaPaimeanStore,
 * and should not be unreffed by the caller.
 */
ofoPaimean *
ofa_paimean_frame_bin_get_selected( ofaPaimeanFrameBin *bin )
{
	static const gchar *thisfn = "ofa_paimean_frame_bin_get_selected";
	ofaPaimeanFrameBinPrivate *priv;
	ofoPaimean *paimean;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_PAIMEAN_FRAME_BIN( bin ), NULL );

	priv = ofa_paimean_frame_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	paimean = ofa_paimean_treeview_get_selected( priv->tview );

	return( paimean );
}

/**
 * ofa_paimean_frame_bin_set_selected:
 * @bin: this #ofaPaimeanFrameBin instance
 * @code: [allow-none]: the identifier to be selected.
 *
 * Let the user set the selection.
 */
void
ofa_paimean_frame_bin_set_selected( ofaPaimeanFrameBin *bin, const gchar *code )
{
	static const gchar *thisfn = "ofa_paimean_frame_bin_set_selected";
	ofaPaimeanFrameBinPrivate *priv;

	g_debug( "%s: bin=%p, code=%s", thisfn, ( void * ) bin, code );

	g_return_if_fail( bin && OFA_IS_PAIMEAN_FRAME_BIN( bin ));

	priv = ofa_paimean_frame_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_strlen( code )){
		ofa_paimean_treeview_set_selected( priv->tview, code );
	}
}

/*
 * Signal sent by ofaPaimeanTreeview on selection change
 *
 * Other actions do not depend of the selection:
 * - new: enabled when dossier is writable.
 */
static void
on_row_selected( ofaPaimeanTreeview *tview, ofoPaimean *paimean, ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;
	gboolean is_paimean;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	is_paimean = paimean && OFO_IS_PAIMEAN( paimean );

	g_simple_action_set_enabled( priv->update_action, is_paimean );
	g_simple_action_set_enabled( priv->delete_action, check_for_deletability( self, paimean ));

	g_signal_emit_by_name( self, "ofa-changed", paimean );
}

/*
 * signal sent by ofaPaimeanTreeview on selection activation
 */
static void
on_row_activated( ofaPaimeanTreeview *tview, ofoPaimean *paimean, ofaPaimeanFrameBin *self )
{
	g_signal_emit_by_name( self, "ofa-activated", paimean );
}

/*
 * signal sent by ofaTVBin on Insert key
 *
 * Note that the key may be pressend even if dossier is not writable.
 * If this is the case, just silently ignore the key.
 */
static void
on_insert_key( ofaPaimeanTreeview *tview, ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	if( priv->is_writable ){
		g_action_activate( G_ACTION( priv->new_action ), NULL );
	}
}

/*
 * signal sent by ofaPaimeanTreeview on Delete key
 *
 * Note that the key may be pressed, even if the button
 * is disabled. So have to check all prerequisite conditions.
 * If the current row is not deletable, just silently ignore the key.
 */
static void
on_delete_key( ofaPaimeanTreeview *tview, ofoPaimean *paimean, ofaPaimeanFrameBin *self )
{
	if( check_for_deletability( self, paimean )){
		delete_with_confirm( self, paimean );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;
	ofoPaimean *paimean;
	GtkWindow *toplevel;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	paimean = ofo_paimean_new( priv->getter );
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_paimean_properties_run( priv->getter, toplevel, paimean );
}

static void
action_on_update_activated( GSimpleAction *action, GVariant *empty, ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;
	ofoPaimean *paimean;
	GtkWindow *toplevel;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	paimean = ofa_paimean_treeview_get_selected( priv->tview );
	g_return_if_fail( paimean && OFO_IS_PAIMEAN( paimean ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ofa_paimean_properties_run( priv->getter, toplevel, paimean );
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaPaimeanFrameBin *self )
{
	ofaPaimeanFrameBinPrivate *priv;
	ofoPaimean *paimean;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	paimean = ofa_paimean_treeview_get_selected( priv->tview );
	g_return_if_fail( paimean && OFO_IS_PAIMEAN( paimean ));

	delete_with_confirm( self, paimean );
}

static gboolean
check_for_deletability( ofaPaimeanFrameBin *self, ofoPaimean *paimean )
{
	ofaPaimeanFrameBinPrivate *priv;
	gboolean is_paimean;

	priv = ofa_paimean_frame_bin_get_instance_private( self );

	is_paimean = paimean && OFO_IS_PAIMEAN( paimean );

	return( is_paimean && priv->is_writable && ofo_paimean_is_deletable( paimean ));
}

static void
delete_with_confirm( ofaPaimeanFrameBin *self, ofoPaimean *paimean )
{
	gchar *msg;
	gboolean delete_ok;
	GtkWindow *toplevel;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' mean of paiement ?" ),
			ofo_paimean_get_code( paimean ),
			ofo_paimean_get_label( paimean ));

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	delete_ok = my_utils_dialog_question( toplevel, msg, _( "_Delete" ));

	g_free( msg );

	if( delete_ok ){
		ofo_paimean_delete( paimean );
	}
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_paimean_frame_bin_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
