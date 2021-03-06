/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-icontext.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-operation-group.h"
#include "api/ofo-concil.h"
#include "api/ofo-entry.h"

#include "core/ofa-entry-properties.h"
#include "core/ofa-entry-store.h"
#include "core/ofa-entry-treeview.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-reconcil-group.h"
#include "core/ofa-settlement-group.h"

/* private instance data
 */
typedef struct {
	gboolean          dispose_has_run;

	/* initialization
	 */
	ofaIGetter       *getter;
	GtkWindow        *parent;
	GList            *opes_list;

	/* runtime
	 */
	gchar            *settings_prefix;
	GtkWindow        *actual_parent;

	/* UI
	 */
	ofaEntryTreeview *tview;

	/* actions
	 */
	GSimpleAction    *ventry_action;
	GSimpleAction    *vconcil_action;
	GSimpleAction    *vsettle_action;

	/* selection
	 */
	ofoEntry         *sel_entry;
	ofxCounter        sel_concil_id;
	ofxCounter        sel_settle_id;
}
	ofaOperationGroupPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-operation-group.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      iwindow_init( myIWindow *instance );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      setup_ui( ofaOperationGroup *self );
static void      setup_actions( ofaOperationGroup *self );
static void      setup_store( ofaOperationGroup *self );
static void      tview_on_selection_changed( ofaEntryTreeview *treeview, GList *selection, ofaOperationGroup *self );
static void      tview_on_selection_activated( ofaEntryTreeview *treeview, GList *selection, ofaOperationGroup *self );
static gboolean  tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOperationGroup *self );
static void      action_on_ventry_activated( GSimpleAction *action, GVariant *empty, ofaOperationGroup *self );
static void      action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaOperationGroup *self );
static void      action_on_vsettle_activated( GSimpleAction *action, GVariant *empty, ofaOperationGroup *self );
static void      iactionable_iface_init( ofaIActionableInterface *iface );
static guint     iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaOperationGroup, ofa_operation_group, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaOperationGroup )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ))

static void
operation_group_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_operation_group_finalize";
	ofaOperationGroupPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPERATION_GROUP( instance ));

	/* free data members here */
	priv = ofa_operation_group_get_instance_private( OFA_OPERATION_GROUP( instance ));

	g_free( priv->settings_prefix );
	g_list_free( priv->opes_list );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_operation_group_parent_class )->finalize( instance );
}

static void
operation_group_dispose( GObject *instance )
{
	ofaOperationGroupPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPERATION_GROUP( instance ));

	priv = ofa_operation_group_get_instance_private( OFA_OPERATION_GROUP( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->ventry_action );
		g_clear_object( &priv->vconcil_action );
		g_clear_object( &priv->vsettle_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_operation_group_parent_class )->dispose( instance );
}

static void
ofa_operation_group_init( ofaOperationGroup *self )
{
	static const gchar *thisfn = "ofa_operation_group_init";
	ofaOperationGroupPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPERATION_GROUP( self ));

	priv = ofa_operation_group_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->opes_list = NULL;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_operation_group_class_init( ofaOperationGroupClass *klass )
{
	static const gchar *thisfn = "ofa_operation_group_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = operation_group_dispose;
	G_OBJECT_CLASS( klass )->finalize = operation_group_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_operation_group_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @opes: a #GList of operations number to be displayed.
 *
 * Display the lines which belongs to the @opes list.
 */
void
ofa_operation_group_run( ofaIGetter *getter, GtkWindow *parent , GList *opes )
{
	static const gchar *thisfn = "ofa_operation_group_run";
	ofaOperationGroup *self;
	ofaOperationGroupPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, opes=%p",
			thisfn, ( void * ) getter, ( void * ) parent, opes );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_OPERATION_GROUP, NULL );

	priv = ofa_operation_group_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->opes_list = g_list_copy( opes );

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_operation_group_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_operation_group_iwindow_init";
	ofaOperationGroupPrivate *priv;
	GString *gstr;
	GList *it;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_operation_group_get_instance_private( OFA_OPERATION_GROUP( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	gstr = g_string_new( G_OBJECT_TYPE_NAME( instance ));
	for( it=priv->opes_list ; it ; it=it->next ){
		g_string_append_printf( gstr, "-%lu", ( ofxCounter ) GPOINTER_TO_UINT( it->data ));
	}
	my_iwindow_set_identifier( instance, gstr->str );
	g_string_free( gstr, TRUE );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_operation_group_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_operation_group_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	setup_ui( OFA_OPERATION_GROUP( instance ));
	setup_actions( OFA_OPERATION_GROUP( instance ));
	setup_store( OFA_OPERATION_GROUP( instance ));
}

static void
setup_ui( ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;
	GtkWidget *parent, *btn, *label;
	GString *gstr;
	GList *it;

	priv = ofa_operation_group_get_instance_private( self );

	/* terminates on Close */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "close-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( my_iwindow_close ), self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "group-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->tview = ofa_entry_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_entry_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, self );
	ofa_tvbin_set_selection_mode( OFA_TVBIN( priv->tview ), GTK_SELECTION_BROWSE );
	g_signal_connect( priv->tview, "ofa-entchanged", G_CALLBACK( tview_on_selection_changed ), self );
	g_signal_connect( priv->tview, "ofa-entactivated", G_CALLBACK( tview_on_selection_activated ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "id-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gstr = g_string_new( "" );
	for( it=priv->opes_list ; it ; it=it->next ){
		if( gstr->len > 0 ){
			gstr = g_string_append( gstr, ", " );
		}
		g_string_append_printf( gstr, "%lu", ( ofxCounter ) GPOINTER_TO_UINT( it->data ));
	}
	gtk_label_set_text( GTK_LABEL( label ), gstr->str );
	g_string_free( gstr, TRUE );
}

static void
setup_actions( ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;
	GMenu *menu;

	priv = ofa_operation_group_get_instance_private( self );

	/* view entry action */
	priv->ventry_action = g_simple_action_new( "viewentry", NULL );
	g_simple_action_set_enabled( priv->ventry_action, FALSE );
	g_signal_connect( priv->ventry_action, "activate", G_CALLBACK( action_on_ventry_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->ventry_action ),
			_( "View the entry..." ));

	/* view conciliation action */
	priv->vconcil_action = g_simple_action_new( "vconcil", NULL );
	g_signal_connect( priv->vconcil_action, "activate", G_CALLBACK( action_on_vconcil_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vconcil_action ),
			_( "View the conciliation group..." ));

	/* view settlement action */
	priv->vsettle_action = g_simple_action_new( "vsettle", NULL );
	g_signal_connect( priv->vsettle_action, "activate", G_CALLBACK( action_on_vsettle_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vsettle_action ),
			_( "View the settlement group..." ));

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static void
setup_store( ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;
	ofaEntryStore *store;

	priv = ofa_operation_group_get_instance_private( self );

	store = ofa_entry_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( store ));
}

/*
 * Selection has been set in browse mode
 */
static void
tview_on_selection_changed( ofaEntryTreeview *treeview, GList *selection, ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;
	gboolean ventry_enabled, vconcil_enabled, vsettle_enabled;
	ofoEntry *entry;
	ofoConcil *concil;

	priv = ofa_operation_group_get_instance_private( self );

	ventry_enabled = FALSE;
	vconcil_enabled = FALSE;
	vsettle_enabled = FALSE;
	priv->sel_entry = NULL;

	if( selection && g_list_length( selection ) > 0 ){
		g_return_if_fail( g_list_length( selection ) == 1 );
		entry = OFO_ENTRY( selection->data );
		g_return_if_fail( entry && OFO_IS_ENTRY( entry ));

		priv->sel_entry = OFO_ENTRY( entry );
		ventry_enabled = TRUE;
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( priv->sel_entry ));
		priv->sel_concil_id = concil ? ofo_concil_get_id( concil ) : 0;
		vconcil_enabled = ( priv->sel_concil_id > 0 );
		priv->sel_settle_id = ofo_entry_get_settlement_number( priv->sel_entry );
		vsettle_enabled = ( priv->sel_settle_id > 0 );
	}

	g_simple_action_set_enabled( priv->ventry_action, ventry_enabled );
	g_simple_action_set_enabled( priv->vconcil_action, vconcil_enabled );
	g_simple_action_set_enabled( priv->vsettle_action, vsettle_enabled );
}

/*
 * Selection has been set in browse mode
 */
static void
tview_on_selection_activated( ofaEntryTreeview *treeview, GList *selection, ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;

	priv = ofa_operation_group_get_instance_private( self );

	g_return_if_fail( priv->sel_entry && OFO_IS_ENTRY( priv->sel_entry ));

	ofa_entry_properties_run( priv->getter, priv->parent, priv->sel_entry, FALSE );
}

/*
 * Filter the view to be sure to only display the requested conciliation
 * group.
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;
	ofxCounter id;
	gboolean visible;

	priv = ofa_operation_group_get_instance_private( self );

	gtk_tree_model_get( tmodel, iter, ENTRY_COL_OPE_NUMBER_I, &id, -1 );

	visible = ( g_list_find( priv->opes_list, ( gpointer ) id ) != NULL );

	return( visible );
}

static void
action_on_ventry_activated( GSimpleAction *action, GVariant *empty, ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;

	priv = ofa_operation_group_get_instance_private( self );

	ofa_entry_properties_run( priv->getter, priv->parent, priv->sel_entry, FALSE );
}

static void
action_on_vconcil_activated( GSimpleAction *action, GVariant *empty, ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;

	priv = ofa_operation_group_get_instance_private( self );

	ofa_reconcil_group_run( priv->getter, priv->parent, priv->sel_concil_id );
}

static void
action_on_vsettle_activated( GSimpleAction *action, GVariant *empty, ofaOperationGroup *self )
{
	ofaOperationGroupPrivate *priv;

	priv = ofa_operation_group_get_instance_private( self );

	ofa_settlement_group_run( priv->getter, priv->parent, priv->sel_settle_id );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_operation_group_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
