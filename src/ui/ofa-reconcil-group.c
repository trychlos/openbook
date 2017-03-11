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
#include "api/ofa-preferences.h"
#include "api/ofo-bat.h"
#include "api/ofo-concil.h"

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
	gboolean             is_new;
	ofoConcil           *concil;

	/* UI
	 */
	ofaReconcilTreeview *tview;
}
	ofaReconcilGroupPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-reconcil-group.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static gboolean tview_is_visible_row( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaReconcilGroup *self );
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

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * BAT record
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_reconcil_group_idialog_init";
	ofaReconcilGroupPrivate *priv;
	GtkWidget *parent, *btn, *label;
	ofaReconcilStore *store;
	gchar *str;
	ofxCounter count;
	GMenu *menu;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_reconcil_group_get_instance_private( OFA_RECONCIL_GROUP( instance ));

	priv->concil = ofo_concil_get_by_id( priv->getter, priv->concil_id );

	/* terminates on Close */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "close-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( my_iwindow_close ), instance );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "group-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->tview = ofa_reconcil_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_reconcil_treeview_setup_columns( priv->tview );
	ofa_reconcil_treeview_set_filter_func( priv->tview, ( GtkTreeModelFilterVisibleFunc ) tview_is_visible_row, instance );
	ofa_tvbin_set_selection_mode( OFA_TVBIN( priv->tview ), GTK_SELECTION_BROWSE );

	my_utils_container_updstamp_init( instance, concil );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "id-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%lu", priv->concil_id );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "value-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_date_to_str( ofo_concil_get_dval( priv->concil ), ofa_prefs_date_display( priv->getter ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	store = ofa_reconcil_store_new( priv->getter );
	ofa_tvbin_set_store( OFA_TVBIN( priv->tview ), GTK_TREE_MODEL( store ));
	count = ofa_reconcil_store_load_by_concil( store, priv->concil_id );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "count-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%lu", count );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	ofa_reconcil_treeview_expand_all( priv->tview );

	/* setup actions */
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( instance ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( instance ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
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