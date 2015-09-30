/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"

#include "ui/ofa-bat-properties-bin.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-bat-treeview.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaBatSelectPrivate {

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

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-bat-select.ui";
static const gchar *st_ui_id            = "BatSelectDlg";

G_DEFINE_TYPE( ofaBatSelect, ofa_bat_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_pane( ofaBatSelect *self, GtkContainer *parent );
static void      setup_treeview( ofaBatSelect *self, GtkContainer *parent );
static void      setup_properties( ofaBatSelect *self, GtkContainer *parent );
static void      on_selection_changed( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self );
static void      on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self );
static void      check_for_enable_dlg( ofaBatSelect *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static void      get_settings( ofaBatSelect *self );
static void      set_settings( ofaBatSelect *self );

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

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_BAT_SELECT( instance )->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_SELECT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_SELECT, ofaBatSelectPrivate );
}

static void
ofa_bat_select_class_init( ofaBatSelectClass *klass )
{
	static const gchar *thisfn = "ofa_bat_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_select_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaBatSelectPrivate ));
}

/**
 * ofa_bat_select_run:
 *
 * Returns the selected Bank Account Transaction list (BAT) identifier,
 * or -1.
 */
ofxCounter
ofa_bat_select_run( ofaMainWindow *main_window, ofxCounter id )
{
	static const gchar *thisfn = "ofa_bat_select_run";
	ofaBatSelect *self;
	ofaBatSelectPrivate *priv;
	ofxCounter bat_id;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, id=%ld", thisfn, ( void * ) main_window, id );

	self = g_object_new(
			OFA_TYPE_BAT_SELECT,
			MY_PROP_MAIN_WINDOW, main_window,
			MY_PROP_WINDOW_XML,  st_ui_xml,
			MY_PROP_WINDOW_NAME, st_ui_id,
			NULL );

	priv = self->priv;
	priv->bat_id = id;
	bat_id = -1;

	if( my_dialog_run_dialog( MY_DIALOG( self )) == GTK_RESPONSE_OK ){
		bat_id = priv->bat_id;
	}

	g_object_unref( self );

	return( bat_id );
}

static void
v_init_dialog( myDialog *dialog )
{
	GtkWindow *toplevel;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	get_settings( OFA_BAT_SELECT( dialog ));

	setup_pane( OFA_BAT_SELECT( dialog ), GTK_CONTAINER( toplevel ));
	setup_properties( OFA_BAT_SELECT( dialog ), GTK_CONTAINER( toplevel ));
	setup_treeview( OFA_BAT_SELECT( dialog ), GTK_CONTAINER( toplevel ));

	gtk_widget_show_all( GTK_WIDGET( toplevel ));

	check_for_enable_dlg( OFA_BAT_SELECT( dialog ));
}

static void
setup_pane( ofaBatSelect *self, GtkContainer *parent )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *pane;

	priv = self->priv;

	pane = my_utils_container_get_child_by_name( parent, "p-paned" );
	g_return_if_fail( pane && GTK_IS_PANED( pane ));
	priv->paned = GTK_PANED( pane );

	gtk_paned_set_position( priv->paned, priv->pane_pos );
}

static void
setup_treeview( ofaBatSelect *self, GtkContainer *parent )
{
	ofaBatSelectPrivate *priv;
	GtkApplicationWindow *main_window;
	static ofaBatColumns st_columns[] = { BAT_DISP_URI, 0 };
	GtkWidget *widget;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	widget = my_utils_container_get_child_by_name( parent, "treeview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->tview = ofa_bat_treeview_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->tview ));

	ofa_bat_treeview_set_columns( priv->tview, st_columns );
	g_signal_connect( priv->tview, "changed", G_CALLBACK( on_selection_changed ), self );
	g_signal_connect( priv->tview, "activated", G_CALLBACK( on_row_activated ), self );

	ofa_bat_treeview_set_main_window( priv->tview, OFA_MAIN_WINDOW( main_window ));
	ofa_bat_treeview_set_selected( priv->tview, priv->bat_id );
}

static void
setup_properties( ofaBatSelect *self, GtkContainer *parent )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *container;

	priv = self->priv;

	container = my_utils_container_get_child_by_name( parent, "properties-parent" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	priv->bat_bin = ofa_bat_properties_bin_new();
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( priv->bat_bin ));
}

static void
on_selection_changed( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	priv->bat_id = -1;

	if( bat ){
		priv->bat_id = ofo_bat_get_id( bat );
		ofa_bat_properties_bin_set_bat( priv->bat_bin, bat, dossier );
	}
}

static void
on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaBatSelect *self )
{
	ofaBatSelectPrivate *priv;
	GtkWidget *btn;

	priv = self->priv;

	btn = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( btn, priv->bat_id > 0 );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaBatSelectPrivate *priv;

	priv = OFA_BAT_SELECT( dialog )->priv;

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

	priv = self->priv;
	slist = ofa_settings_get_string_list( st_settings );

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

	priv = self->priv;

	str = g_strdup_printf( "%u;", priv->pane_pos );
	ofa_settings_set_string( st_settings, str );
	g_free( str );
}
