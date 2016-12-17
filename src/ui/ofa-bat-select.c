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
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-properties-bin.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-bat-store.h"
#include "ui/ofa-bat-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;

	/* runtime
	 */
	gchar               *settings_prefix;
	ofaHub              *hub;

	/* UI
	 */
	GtkPaned            *paned;
	ofaBatTreeview      *tview;
	ofaBatPropertiesBin *bat_bin;
	guint                pane_pos;

	/* preselected value/returned value
	 */
	ofxCounter           bat_id;
}
	ofaBatSelectPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-bat-select.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     setup_pane( ofaBatSelect *self );
static void     setup_treeview( ofaBatSelect *self );
static void     setup_properties( ofaBatSelect *self );
static void     on_selection_changed( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self );
static void     on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self );
static void     check_for_enable_dlg( ofaBatSelect *self );
static gboolean idialog_quit_on_ok( myIDialog *instance );
static void     read_settings( ofaBatSelect *self );
static void     write_settings( ofaBatSelect *self );

G_DEFINE_TYPE_EXTENDED( ofaBatSelect, ofa_bat_select, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaBatSelect )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
bat_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_select_finalize";
	ofaBatSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	/* free data members here */
	priv = ofa_bat_select_get_instance_private( OFA_BAT_SELECT( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_select_parent_class )->finalize( instance );
}

static void
bat_select_dispose( GObject *instance )
{
	ofaBatSelectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	priv = ofa_bat_select_get_instance_private( OFA_BAT_SELECT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		priv->pane_pos = gtk_paned_get_position( priv->paned );
		write_settings( OFA_BAT_SELECT( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_select_parent_class )->dispose( instance );
}

static void
ofa_bat_select_init( ofaBatSelect *self )
{
	static const gchar *thisfn = "ofa_bat_select_init";
	ofaBatSelectPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_SELECT( self ));

	priv = ofa_bat_select_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_bat_select_class_init( ofaBatSelectClass *klass )
{
	static const gchar *thisfn = "ofa_bat_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_select_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_bat_select_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @id: [allow-none]: the initially selected BAT identifier.
 *
 * Returns the selected Bank Account Transaction list (BAT) identifier,
 * or -1.
 */
ofxCounter
ofa_bat_select_run( ofaIGetter *getter, GtkWindow *parent, ofxCounter id )
{
	static const gchar *thisfn = "ofa_bat_select_run";
	ofaBatSelect *self;
	ofaBatSelectPrivate *priv;
	ofxCounter bat_id;

	g_debug( "%s: getter=%p, parent=%p, id=%ld", thisfn, ( void * ) getter, ( void * ) parent, id );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), 0 );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), 0 );

	self = g_object_new( OFA_TYPE_BAT_SELECT, NULL );

	priv = ofa_bat_select_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
	priv->parent = parent;
	priv->bat_id = id;
	bat_id = -1;

	if( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK ){
		bat_id = priv->bat_id;
		my_iwindow_close( MY_IWINDOW( self ));
	}

	return( bat_id );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_select_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_bat_select_iwindow_init";
	ofaBatSelectPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_bat_select_get_instance_private( OFA_BAT_SELECT( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_settings( instance, ofa_hub_get_user_settings( priv->hub ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_select_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_bat_select_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	read_settings( OFA_BAT_SELECT( instance ));

	setup_pane( OFA_BAT_SELECT( instance ));
	setup_properties( OFA_BAT_SELECT( instance ));
	setup_treeview( OFA_BAT_SELECT( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_BAT_SELECT( instance ));
}

static void
setup_pane( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *pane;

	priv = ofa_bat_select_get_instance_private( self );

	pane = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p-paned" );
	g_return_if_fail( pane && GTK_IS_PANED( pane ));
	priv->paned = GTK_PANED( pane );

	gtk_paned_set_position( priv->paned, priv->pane_pos );
}

static void
setup_treeview( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *widget;
	GMenu *menu;

	priv = ofa_bat_select_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "treeview-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->tview = ofa_bat_treeview_new( priv->hub );
	my_utils_widget_set_margins( GTK_WIDGET( priv->tview ), 0, 0, 0, 2 );
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->tview ));
	ofa_bat_treeview_set_settings_key( priv->tview, priv->settings_prefix );
	ofa_bat_treeview_setup_columns( priv->tview );

	g_signal_connect( priv->tview, "ofa-batchanged", G_CALLBACK( on_selection_changed ), self );
	g_signal_connect( priv->tview, "ofa-batactivated", G_CALLBACK( on_row_activated ), self );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_set_menu( OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ), menu );

	/* install the store at the very end of the initialization
	 * (i.e. after treeview creation, signals connection, actions and
	 *  menus definition) */
	ofa_bat_treeview_setup_store( priv->tview );
	ofa_bat_treeview_set_selected( priv->tview, priv->bat_id );
}

static void
setup_properties( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *container;
	gchar *key;
	ofaBatlineTreeview *line_tview;

	priv = ofa_bat_select_get_instance_private( self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "properties-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->bat_bin = ofa_bat_properties_bin_new( priv->hub );
	my_utils_widget_set_margins( GTK_WIDGET( priv->bat_bin ), 0, 0, 2, 0 );
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->bat_bin ));

	key = g_strdup_printf( "%s-BatLine", priv->settings_prefix );
	ofa_bat_properties_bin_set_settings_key( priv->bat_bin, key );
	g_free( key );

	line_tview = ofa_bat_properties_bin_get_batline_treeview( priv->bat_bin );
	ofa_batline_treeview_setup_columns( line_tview );
}

static void
on_selection_changed( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;

	priv = ofa_bat_select_get_instance_private( self );

	priv->bat_id = -1;

	if( bat ){
		priv->bat_id = ofo_bat_get_id( bat );
		ofa_bat_properties_bin_set_bat( priv->bat_bin, bat );
	}
}

static void
on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self )
{
	gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *btn;

	priv = ofa_bat_select_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "btn-ok" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));

	gtk_widget_set_sensitive( btn, priv->bat_id > 0 );
}

static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	ofaBatSelectPrivate *priv;

	priv = ofa_bat_select_get_instance_private( OFA_BAT_SELECT( instance ));

	return( priv->bat_id > 0 );
}

/*
 * pane_position;
 */
static void
read_settings( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *settings_key;

	priv = ofa_bat_select_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, settings_key );

	it = strlist ? strlist : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->pane_pos = cstr ? atoi( cstr ) : 200;

	my_isettings_free_string_list( settings, strlist );
	g_free( settings_key );
}

static void
write_settings( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	myISettings *settings;
	gchar *str, *settings_key;

	priv = ofa_bat_select_get_instance_private( self );

	str = g_strdup_printf( "%u;", priv->pane_pos );

	settings = ofa_hub_get_user_settings( priv->hub );
	settings_key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, settings_key, str );

	g_free( str );
	g_free( settings_key );
}
