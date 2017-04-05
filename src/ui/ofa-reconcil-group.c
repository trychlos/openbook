/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include "api/ofa-prefs.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"

#include "ui/ofa-bat-line-properties.h"
#include "ui/ofa-entry-properties.h"
#include "ui/ofa-operation-group.h"
#include "ui/ofa-reconcil-group.h"
#include "ui/ofa-reconcil-store.h"
#include "ui/ofa-reconcil-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;
	ofxCounter           concil_id;

	/* runtime
	 */
	gchar               *settings_prefix;
	ofoConcil           *concil;
	gboolean             is_new;				/* required by my_utils_container_updstamp_init() */

	/* UI
	 */
	ofaReconcilTreeview *tview;

	/* actions
	 */
	GSimpleAction       *ventry_action;
	GSimpleAction       *vbat_action;
	GSimpleAction       *vope_action;

	/* selection
	 */
	ofoEntry            *sel_entry;
	ofoBatLine          *sel_batline;
	ofxCounter           sel_ope_number;
}
	ofaReconcilGroupPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-reconcil-group.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     setup_ui( ofaReconcilGroup *self );
static void     setup_actions( ofaReconcilGroup *self );
static void     setup_store( ofaReconcilGroup *self );
static void     tview_on_selection_changed( ofaTVBin *treeview, GtkTreeSelection *selection, ofaReconcilGroup *self );
static gboolean tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilGroup *self );
static void     action_on_ventry_activated( GSimpleAction *action, GVariant *empty, ofaReconcilGroup *self );
static void     action_on_vbat_activated( GSimpleAction *action, GVariant *empty, ofaReconcilGroup *self );
static void     action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaReconcilGroup *self );
static void     iactionable_iface_init( ofaIActionableInterface *iface );
static guint    iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaReconcilGroup, ofa_reconcil_group, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaReconcilGroup )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ))

static void
reconcil_group_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_group_finalize";
	ofaReconcilGroupPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECONCIL_GROUP( instance ));

	/* free data members here */
	priv = ofa_reconcil_group_get_instance_private( OFA_RECONCIL_GROUP( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_group_parent_class )->finalize( instance );
}

static void
reconcil_group_dispose( GObject *instance )
{
	ofaReconcilGroupPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECONCIL_GROUP( instance ));

	priv = ofa_reconcil_group_get_instance_private( OFA_RECONCIL_GROUP( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		g_clear_object( &priv->ventry_action );
		g_clear_object( &priv->vbat_action );
		g_clear_object( &priv->vope_action );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_group_parent_class )->dispose( instance );
}

static void
ofa_reconcil_group_init( ofaReconcilGroup *self )
{
	static const gchar *thisfn = "ofa_reconcil_group_init";
	ofaReconcilGroupPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECONCIL_GROUP( self ));

	priv = ofa_reconcil_group_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_reconcil_group_class_init( ofaReconcilGroupClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_group_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_group_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_group_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_reconcil_group_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @concil_id: the conciliation group identifier.
 *
 * Display the lines which belongs to the @concil_id group.
 */
void
ofa_reconcil_group_run( ofaIGetter *getter, GtkWindow *parent , ofxCounter concil_id )
{
	static const gchar *thisfn = "ofa_reconcil_group_run";
	ofaReconcilGroup *self;
	ofaReconcilGroupPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, concil_id=%lu",
			thisfn, ( void * ) getter, ( void * ) parent, concil_id );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_RECONCIL_GROUP, NULL );

	priv = ofa_reconcil_group_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->concil_id = concil_id;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_reconcil_group_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_reconcil_group_iwindow_init";
	ofaReconcilGroupPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_reconcil_group_get_instance_private( OFA_RECONCIL_GROUP( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%lu",
				G_OBJECT_TYPE_NAME( instance ), priv->concil_id );
	my_iwindow_set_identifier( instance, id );
	g_free( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_reconcil_group_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_reconcil_group_idialog_init";
	ofaReconcilGroupPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_reconcil_group_get_instance_private( OFA_RECONCIL_GROUP( instance ));

	priv->concil = ofo_concil_get_by_id( priv->getter, priv->concil_id );

	setup_ui( OFA_RECONCIL_GROUP( instance ));
	setup_actions( OFA_RECONCIL_GROUP( instance ));
	setup_store( OFA_RECONCIL_GROUP( instance ));
}

static void
setup_ui( ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;
	GtkWidget *parent, *btn, *label;
	gchar *str;

	priv = ofa_reconcil_group_get_instance_private( self );

	/* terminates on Close */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "close-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( my_iwindow_close ), self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "group-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->tview = ofa_reconcil_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_reconcil_treeview_setup_columns( priv->tview );
	ofa_reconcil_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, self );
	ofa_tvbin_set_selection_mode( OFA_TVBIN( priv->tview ), GTK_SELECTION_BROWSE );
	g_signal_connect( priv->tview, "ofa-selchanged", G_CALLBACK( tview_on_selection_changed ), self );

	my_utils_container_updstamp_init( self, concil );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "id-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%lu", priv->concil_id );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "value-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_date_to_str( ofo_concil_get_dval( priv->concil ), ofa_prefs_date_get_display_format( priv->getter ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );
}

static void
setup_actions( ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;
	GMenu *menu;

	priv = ofa_reconcil_group_get_instance_private( self );

	/* view entry action */
	priv->ventry_action = g_simple_action_new( "viewentry", NULL );
	g_simple_action_set_enabled( priv->ventry_action, FALSE );
	g_signal_connect( priv->ventry_action, "activate", G_CALLBACK( action_on_ventry_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->ventry_action ),
			_( "View the entry..." ));

	/* view batline action */
	priv->vbat_action = g_simple_action_new( "viewbat", NULL );
	g_simple_action_set_enabled( priv->vbat_action, FALSE );
	g_signal_connect( priv->vbat_action, "activate", G_CALLBACK( action_on_vbat_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vbat_action ),
			_( "View the BAT line..." ));

	/* view operation action */
	priv->vope_action = g_simple_action_new( "vope", NULL );
	g_signal_connect( priv->vope_action, "activate", G_CALLBACK( action_on_vope_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->vope_action ),
			_( "View the operation..." ));

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
setup_store( ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;
	ofaReconcilStore *store;
	GtkWidget *label;
	gchar *str;
	ofxCounter count;

	priv = ofa_reconcil_group_get_instance_private( self );

	store = ofa_reconcil_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( store ));
	count = ofa_reconcil_store_load_by_concil( store, priv->concil_id );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "count-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%lu", count );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	ofa_reconcil_treeview_expand_all( priv->tview );
}

/*
 * Selection has been set in browse mode
 */
static void
tview_on_selection_changed( ofaTVBin *treeview, GtkTreeSelection *selection, ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;
	gboolean ventry_enabled, vbat_enabled, vope_enabled;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *base_obj;

	priv = ofa_reconcil_group_get_instance_private( self );

	ventry_enabled = FALSE;
	vbat_enabled = FALSE;
	vope_enabled = FALSE;
	priv->sel_entry = NULL;
	priv->sel_batline = NULL;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, RECONCIL_COL_OBJECT, &base_obj, -1 );
		g_return_if_fail( base_obj && ( OFO_IS_ENTRY( base_obj ) || OFO_IS_BAT_LINE( base_obj )));
		g_object_unref( base_obj );

		if( OFO_IS_ENTRY( base_obj )){
			priv->sel_entry = OFO_ENTRY( base_obj );
			ventry_enabled = TRUE;
			priv->sel_ope_number = ofo_entry_get_ope_number( priv->sel_entry );
			vope_enabled = ( priv->sel_ope_number > 0 );

		} else {
			priv->sel_batline = OFO_BAT_LINE( base_obj );
			vbat_enabled = TRUE;
		}
	}

	g_simple_action_set_enabled( priv->ventry_action, ventry_enabled );
	g_simple_action_set_enabled( priv->vbat_action, vbat_enabled );
	g_simple_action_set_enabled( priv->vope_action, vope_enabled );
}

/*
 * Filter the view to be sure to only display the requested conciliation
 * group.
 */
static gboolean
tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;
	ofxCounter id;

	priv = ofa_reconcil_group_get_instance_private( self );

	gtk_tree_model_get( tmodel, iter, RECONCIL_COL_CONCIL_NUMBER_I, &id, -1 );

	return( id == priv->concil_id );
}

static void
action_on_ventry_activated( GSimpleAction *action, GVariant *empty, ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;

	priv = ofa_reconcil_group_get_instance_private( self );

	ofa_entry_properties_run( priv->getter, priv->parent, priv->sel_entry, FALSE );
}

static void
action_on_vbat_activated( GSimpleAction *action, GVariant *empty, ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;

	priv = ofa_reconcil_group_get_instance_private( self );

	ofa_bat_line_properties_run( priv->getter, priv->parent, priv->sel_batline );
}

static void
action_on_vope_activated( GSimpleAction *action, GVariant *empty, ofaReconcilGroup *self )
{
	ofaReconcilGroupPrivate *priv;

	priv = ofa_reconcil_group_get_instance_private( self );

	ofa_operation_group_run( priv->getter, priv->parent, priv->sel_ope_number );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_reconcil_group_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
