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
#include <stdlib.h>

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-bat-properties-bin.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-bat-treeview.h"

/* private instance data
 */
struct _ofaBatSelectPrivate {
	gboolean             dispose_has_run;

	/* UI
	 */
	GtkPaned            *paned;
	ofaBatTreeview      *tview;
	ofaBatPropertiesBin *bat_bin;
	guint                pane_pos;

	/* preselected value/returned value
	 */
	ofxCounter           bat_id;
};

static const gchar *st_settings         = "BatSelect-settings";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-bat-select.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      setup_pane( ofaBatSelect *self );
static void      setup_treeview( ofaBatSelect *self );
static void      setup_properties( ofaBatSelect *self );
static void      on_selection_changed( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self );
static void      on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self );
static void      check_for_enable_dlg( ofaBatSelect *self );
static gboolean  idialog_quit_on_ok( myIDialog *instance );
static void      get_settings( ofaBatSelect *self );
static void      set_settings( ofaBatSelect *self );

G_DEFINE_TYPE_EXTENDED( ofaBatSelect, ofa_bat_select, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaBatSelect )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
bat_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_select_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	/* free data members here */

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
		set_settings( OFA_BAT_SELECT( instance ));
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
 * @main_window: the #ofaMainWindow main window of the application.
 * @id: [allow-none]: the initially selected BAT identifier.
 *
 * Returns the selected Bank Account Transaction list (BAT) identifier,
 * or -1.
 */
ofxCounter
ofa_bat_select_run( const ofaMainWindow *main_window, ofxCounter id )
{
	static const gchar *thisfn = "ofa_bat_select_run";
	ofaBatSelect *self;
	ofaBatSelectPrivate *priv;
	ofxCounter bat_id;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), 0 );

	g_debug( "%s: main_window=%p, id=%ld", thisfn, ( void * ) main_window, id );

	self = g_object_new( OFA_TYPE_BAT_SELECT, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_bat_select_get_instance_private( self );
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
	get_settings( OFA_BAT_SELECT( instance ));

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

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
	static ofaBatColumns st_columns[] = { BAT_DISP_URI, BAT_DISP_UNUSED, 0 };
	GtkApplicationWindow *main_window;
	ofaHub *hub;
	GtkWidget *widget;

	priv = ofa_bat_select_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "treeview-parent" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

	priv->tview = ofa_bat_treeview_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->tview ));

	ofa_bat_treeview_set_columns( priv->tview, st_columns );
	g_signal_connect( priv->tview, "changed", G_CALLBACK( on_selection_changed ), self );
	g_signal_connect( priv->tview, "activated", G_CALLBACK( on_row_activated ), self );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	ofa_bat_treeview_set_hub( priv->tview, hub );
	ofa_bat_treeview_set_selected( priv->tview, priv->bat_id );
}

static void
setup_properties( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *container;

	priv = ofa_bat_select_get_instance_private( self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "properties-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->bat_bin = ofa_bat_properties_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->bat_bin ));
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
get_settings( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_bat_select_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_settings );

	it = slist ? slist : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	priv->pane_pos = cstr ? atoi( cstr ) : 200;

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	gchar *str;

	priv = ofa_bat_select_get_instance_private( self );

	str = g_strdup_printf( "%u;", priv->pane_pos );
	ofa_settings_user_set_string( st_settings, str );
	g_free( str );
}
