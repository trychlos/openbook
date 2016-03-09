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
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-dossier-manager.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-dossier-store.h"
#include "ui/ofa-dossier-treeview.h"

/* private instance data
 */
struct _ofaDossierManagerPrivate {
	gboolean            dispose_has_run;

	/* UI
	 */
	ofaDossierTreeview *tview;
	GtkWidget          *open_btn;
	GtkWidget          *delete_btn;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-manager.ui";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      setup_treeview( ofaDossierManager *self );
static void      on_tview_changed( ofaDossierTreeview *tview, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaDossierManager *self );
static void      on_tview_activated( ofaDossierTreeview *tview, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaDossierManager *self );
static void      on_new_clicked( GtkButton *button, ofaDossierManager *self );
static void      on_open_clicked( GtkButton *button, ofaDossierManager *self );
static void      do_open( ofaDossierManager *self, ofaIDBMeta *meta, ofaIDBPeriod *period );
static void      on_delete_clicked( GtkButton *button, ofaDossierManager *self );
static gboolean  confirm_delete( ofaDossierManager *self, const ofaIDBMeta *meta, const ofaIDBPeriod *period );

G_DEFINE_TYPE_EXTENDED( ofaDossierManager, ofa_dossier_manager, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierManager )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
dossier_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_manager_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_manager_parent_class )->finalize( instance );
}

static void
dossier_manager_dispose( GObject *instance )
{
	ofaDossierManagerPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	priv = ofa_dossier_manager_get_instance_private( OFA_DOSSIER_MANAGER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_manager_parent_class )->dispose( instance );
}

static void
ofa_dossier_manager_init( ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_init";
	ofaDossierManagerPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_MANAGER( self ));

	priv = ofa_dossier_manager_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_manager_class_init( ofaDossierManagerClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_manager_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_manager_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_manager_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_dossier_manager_run:
 * @main: the main window of the application.
 *
 * Run the dialog to manage the dossiers
 */
void
ofa_dossier_manager_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_manager_run";
	ofaDossierManager *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_MANAGER, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_manager_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_manager_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_manager_idialog_init";
	ofaDossierManagerPrivate *priv;
	GtkWidget *button;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_manager_get_instance_private( OFA_DOSSIER_MANAGER( instance ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "new-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_new_clicked ), instance );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "open-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_open_clicked ), instance );
	priv->open_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "delete-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_delete_clicked ), instance );
	priv->delete_btn = button;

	setup_treeview( OFA_DOSSIER_MANAGER( instance ));
}

static void
setup_treeview( ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	GtkWidget *parent;
	static ofaDossierDispColumn st_columns[] = {
			DOSSIER_DISP_DOSNAME,
			DOSSIER_DISP_BEGIN,
			DOSSIER_DISP_END,
			DOSSIER_DISP_STATUS,
			DOSSIER_DISP_PERNAME,
			DOSSIER_DISP_PROVNAME,
			0 };

	priv = ofa_dossier_manager_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->tview = ofa_dossier_treeview_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->tview ));
	ofa_dossier_treeview_set_columns( priv->tview, st_columns );
	ofa_dossier_treeview_set_headers( priv->tview, TRUE );
	ofa_dossier_treeview_set_show( priv->tview, DOSSIER_SHOW_ALL );

	g_signal_connect( priv->tview, "changed", G_CALLBACK( on_tview_changed ), self );
	g_signal_connect( priv->tview, "activated", G_CALLBACK( on_tview_activated ), self );
}

static void
on_tview_changed( ofaDossierTreeview *tview, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	gboolean ok;

	priv = ofa_dossier_manager_get_instance_private( self );

	ok = ( meta && period );

	gtk_widget_set_sensitive( priv->open_btn, ok );
	gtk_widget_set_sensitive( priv->delete_btn, ok );
}

static void
on_tview_activated( ofaDossierTreeview *tview, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaDossierManager *self )
{
	if( meta && period ){
		do_open( self, meta, period );
	}
}

static void
on_new_clicked( GtkButton *button, ofaDossierManager *self )
{
	GtkApplicationWindow *main_window;
	gboolean dossier_opened;

	main_window = my_iwindow_get_main_window( MY_IWINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	dossier_opened = ofa_dossier_new_run_with_parent(
							OFA_MAIN_WINDOW( main_window ),
							my_window_get_toplevel( MY_WINDOW( self )));

	if( dossier_opened ){
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_CANCEL );
	}
}

static void
on_open_clicked( GtkButton *button, ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;

	priv = ofa_dossier_manager_get_instance_private( self );

	meta = NULL;
	period = NULL;

	if( ofa_dossier_treeview_get_selected( priv->tview, &meta, &period )){
		//ofa_idbmeta_dump( meta );
		//ofa_idbperiod_dump( period );
		do_open( self, meta, period );
	}

	g_clear_object( &meta );
	g_clear_object( &period );
}

static void
do_open( ofaDossierManager *self, ofaIDBMeta *meta, ofaIDBPeriod *period )
{
	GtkApplicationWindow *main_window;

	main_window = my_iwindow_get_main_window( MY_IWINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	if( ofa_dossier_open_run_with_parent(
			OFA_MAIN_WINDOW( main_window ), GTK_WINDOW( self ), meta, period, NULL, NULL )){

		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_CLOSE );
	}
}

static void
on_delete_clicked( GtkButton *button, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_on_delete_clicked";
	ofaDossierManagerPrivate *priv;
	GtkApplicationWindow *main_window;
	ofaHub *hub;
	const ofaIDBConnect *dossier_connect;
	ofaIDBMeta *meta, *dossier_meta;
	ofaIDBPeriod *period, *dossier_period;
	ofoDossier *dossier;
	gint cmp;

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	priv = ofa_dossier_manager_get_instance_private( self );

	if( ofa_dossier_treeview_get_selected( priv->tview, &meta, &period ) &&
		confirm_delete( self, meta, period )){

		/* close the currently opened dossier/exercice if we are about
		 * to delete it */
		main_window = my_iwindow_get_main_window( MY_IWINDOW( self ));
		g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

		hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
		if( hub ){
			g_return_if_fail( OFA_IS_HUB( hub ));

			dossier = ofa_hub_get_dossier( hub );
			g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

			dossier_connect = ofa_hub_get_connect( hub );
			g_return_if_fail( dossier_connect && OFA_IS_IDBCONNECT( dossier_connect ));
			dossier_meta = ofa_idbconnect_get_meta( dossier_connect );
			g_return_if_fail( dossier_meta && OFA_IS_IDBMETA( dossier_meta ));
			cmp = 1;
			if( ofa_idbmeta_are_equal( meta, dossier_meta )){
				dossier_period = ofa_idbconnect_get_period( dossier_connect );
				g_return_if_fail( dossier_period && OFA_IS_IDBPERIOD( dossier_period ));
				cmp = ofa_idbperiod_compare( period, dossier_period );
				g_object_unref( dossier_period );
			}
			g_object_unref( dossier_meta );
			if( cmp == 0 ){
				ofa_main_window_close_dossier( OFA_MAIN_WINDOW( main_window ));
			}
		}
		ofa_idbmeta_remove_period( meta, period );
	}

	g_clear_object( &meta );
	g_clear_object( &period );
}

static gboolean
confirm_delete( ofaDossierManager *self, const ofaIDBMeta *meta, const ofaIDBPeriod *period )
{
	gboolean ok;
	gchar *period_name, *dossier_name, *str;

	dossier_name = ofa_idbmeta_get_dossier_name( meta );
	period_name = ofa_idbperiod_get_name( period );
	str = g_strdup_printf(
			_( "You are about to remove the '%s' period from the '%s' dossier.\n"
				"This operation will remove the referenced exercice from the settings, "
				"while letting the database itself unchanged.\n"
				"Are your sure ?" ),
					period_name, dossier_name );

	ok = my_utils_dialog_question( str, _( "_Delete" ));

	g_free( str );
	g_free( period_name );
	g_free( dossier_name );

	return( ok );
}
