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
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-dossier-collection.h"
#include "api/ofa-dossier-store.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dbsu.h"
#include "ui/ofa-dossier-manager.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;
	GtkWindow          *parent;

	/* runtime
	 */
	gchar              *settings_prefix;
	GtkWindow          *actual_parent;
	gboolean            apply_actions;

	/* UI
	 */
	ofaDossierTreeview *dossier_tview;

	/* actions
	 */
	GSimpleAction      *new_action;
	GSimpleAction      *open_action;
	GSimpleAction      *close_action;
	GSimpleAction      *delete_action;
}
	ofaDossierManagerPrivate;

static const gchar *st_resource_ui       = "/org/trychlos/openbook/ui/ofa-dossier-manager.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     setup_treeview( ofaDossierManager *self );
static void     idialog_init_actions( ofaDossierManager *self );
static void     idialog_init_menu( ofaDossierManager *self );
static void     on_tview_changed( ofaDossierTreeview *tview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaDossierManager *self );
static void     do_enable_actions( ofaDossierManager *self, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period );
static void     on_tview_activated( ofaDossierTreeview *tview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaDossierManager *self );
static void     action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self );
static void     action_on_open_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self );
static void     do_open( ofaDossierManager *self, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period );
static void     action_on_close_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self );
static void     action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self );
static gboolean confirm_delete( ofaDossierManager *self, const ofaIDBDossierMeta *meta, const ofaIDBExerciceMeta *period );
static void     iactionable_iface_init( ofaIActionableInterface *iface );
static guint    iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaDossierManager, ofa_dossier_manager, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierManager )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ))

static void
dossier_manager_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_manager_finalize";
	ofaDossierManagerPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	/* free data members here */
	priv = ofa_dossier_manager_get_instance_private( OFA_DOSSIER_MANAGER( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_manager_parent_class )->finalize( instance );
}

static void
dossier_manager_dispose( GObject *instance )
{
	ofaDossierManagerPrivate *priv;
	GtkApplicationWindow *main_window;

	g_return_if_fail( instance && OFA_IS_DOSSIER_MANAGER( instance ));

	priv = ofa_dossier_manager_get_instance_private( OFA_DOSSIER_MANAGER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->new_action );
		g_clear_object( &priv->open_action );
		g_clear_object( &priv->close_action );
		g_clear_object( &priv->delete_action );

		if( priv->apply_actions && priv->getter ){
			main_window = ofa_igetter_get_main_window( priv->getter );
			g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
			ofa_main_window_dossier_apply_actions( OFA_MAIN_WINDOW( main_window ));
		}
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
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->apply_actions = FALSE;

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
 * @getter: a #ofaIGetter instance.
 * @parent: the parent #GtkWindow.
 *
 * Run the dialog to manage the dossiers
 */
void
ofa_dossier_manager_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_dossier_manager_run";
	ofaDossierManager *self;
	ofaDossierManagerPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_DOSSIER_MANAGER, NULL );

	priv = ofa_dossier_manager_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_manager_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_manager_iwindow_init";
	ofaDossierManagerPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_manager_get_instance_private( OFA_DOSSIER_MANAGER( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
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

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	setup_treeview( OFA_DOSSIER_MANAGER( instance ));
	idialog_init_actions( OFA_DOSSIER_MANAGER( instance ));
	idialog_init_menu( OFA_DOSSIER_MANAGER( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

static void
setup_treeview( ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	GtkWidget *parent;

	priv = ofa_dossier_manager_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "tview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->dossier_tview = ofa_dossier_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dossier_tview ));
	ofa_dossier_treeview_setup_columns( priv->dossier_tview );
	ofa_dossier_treeview_set_show_all( priv->dossier_tview, TRUE );

	g_signal_connect( priv->dossier_tview, "ofa-doschanged", G_CALLBACK( on_tview_changed ), self );
	g_signal_connect( priv->dossier_tview, "ofa-dosactivated", G_CALLBACK( on_tview_activated ), self );

	ofa_dossier_treeview_setup_store( priv->dossier_tview );
}

static void
idialog_init_actions( ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	GtkWidget *container;
	ofaButtonsBox *buttons_box;

	priv = ofa_dossier_manager_get_instance_private( self );

	container = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "buttons-box" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));
	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 0, 0, 2, 2 );
	gtk_container_add( GTK_CONTAINER( container ), GTK_WIDGET( buttons_box ));

	/* new action */
	priv->new_action = g_simple_action_new( "new", NULL );
	g_signal_connect( priv->new_action, "activate", G_CALLBACK( action_on_new_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->new_action ),
					_( "_New dossier..." )));
	g_simple_action_set_enabled( priv->new_action, TRUE );

	/* open action */
	priv->open_action = g_simple_action_new( "open", NULL );
	g_signal_connect( priv->open_action, "activate", G_CALLBACK( action_on_open_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->open_action ),
			_( "Open" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->open_action ),
					_( "_Open the dossier..." )));
	g_simple_action_set_enabled( priv->open_action, FALSE );

	/* close action */
	priv->close_action = g_simple_action_new( "close", NULL );
	g_signal_connect( priv->close_action, "activate", G_CALLBACK( action_on_close_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->close_action ),
			_( "Close" ));
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->close_action ),
					_( "C_lose the dossier" )));
	g_simple_action_set_enabled( priv->close_action, FALSE );

	/* delete action */
	priv->delete_action = g_simple_action_new( "delete", NULL );
	g_signal_connect( priv->delete_action, "activate", G_CALLBACK( action_on_delete_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->delete_action ),
			OFA_IACTIONABLE_DELETE_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->delete_action ),
					_( "_Delete the dossier..." )));
	g_simple_action_set_enabled( priv->delete_action, FALSE );
}

static void
idialog_init_menu( ofaDossierManager *self )
{
	ofaDossierManagerPrivate *priv;
	GMenu *menu;

	priv = ofa_dossier_manager_get_instance_private( self );

	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->dossier_tview ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->dossier_tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->dossier_tview ), OFA_IACTIONABLE( priv->dossier_tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static void
on_tview_changed( ofaDossierTreeview *tview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaDossierManager *self )
{
	do_enable_actions( self, meta, period );
}

static void
do_enable_actions( ofaDossierManager *self, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period )
{
	ofaDossierManagerPrivate *priv;
	ofaHub *hub;
	gboolean have_data, is_opened;

	priv = ofa_dossier_manager_get_instance_private( self );

	have_data = meta && period;
	hub = ofa_igetter_get_hub( priv->getter );
	is_opened = have_data && ofa_hub_is_opened_dossier( hub, period );

	if( priv->open_action ){
		g_simple_action_set_enabled( priv->open_action, have_data && !is_opened );
	}
	if( priv->close_action ){
		g_simple_action_set_enabled( priv->close_action, have_data && is_opened );
	}
	if( priv->delete_action ){
		g_simple_action_set_enabled( priv->delete_action, have_data && !is_opened );
	}
}

static void
on_tview_activated( ofaDossierTreeview *tview, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaDossierManager *self )
{
	if( meta && period ){
		do_open( self, meta, period );
	}
}

static void
action_on_new_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_action_on_new_activated";
	ofaDossierManagerPrivate *priv;
	GtkWindow *toplevel;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_dossier_manager_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ofa_dossier_new_run_modal( priv->getter, toplevel, NULL, HUB_RULE_DOSSIER_NEW, TRUE, TRUE, TRUE, TRUE, NULL );
}

static void
action_on_open_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_action_on_open_activated";
	ofaDossierManagerPrivate *priv;
	ofaIDBDossierMeta *meta;
	ofaIDBExerciceMeta *period;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_dossier_manager_get_instance_private( self );

	meta = NULL;
	period = NULL;

	if( ofa_dossier_treeview_get_selected( priv->dossier_tview, &meta, &period )){
		if( 0 ){
			ofa_idbdossier_meta_dump( meta );
			ofa_idbexercice_meta_dump( period );
		}
		do_open( self, meta, period );
	}
}

/*
 * Only close the ofaDossierManager if open is successful
 * (and after the #ofa_dossier_open_run_modal() has returned, else the
 *  ofaMainWindow will close all windows, releasing our connection)
 */
static void
do_open( ofaDossierManager *self, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period )
{
	ofaDossierManagerPrivate *priv;
	gboolean ok;

	priv = ofa_dossier_manager_get_instance_private( self );

	my_iwindow_set_allow_close( MY_IWINDOW( self ), FALSE );

	ok = ofa_dossier_open_run_modal( priv->getter, GTK_WINDOW( self ), period, NULL, NULL, FALSE );

	my_iwindow_set_allow_close( MY_IWINDOW( self ), TRUE );

	if( ok ){
		priv->apply_actions = TRUE;
		my_iwindow_close( MY_IWINDOW( self ));
	}
}

static void
action_on_close_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_action_on_close_activated";
	ofaDossierManagerPrivate *priv;
	ofaHub *hub;
	ofaIDBDossierMeta *meta;
	ofaIDBExerciceMeta *period;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_dossier_manager_get_instance_private( self );

	meta = NULL;
	period = NULL;
	hub = ofa_igetter_get_hub( priv->getter );

	if( ofa_dossier_treeview_get_selected( priv->dossier_tview, &meta, &period ) &&
			ofa_hub_is_opened_dossier( hub, period )){

		/* does not close this dossier manager on dossier close */
		my_iwindow_set_allow_close( MY_IWINDOW( self ), FALSE );
		ofa_hub_close_dossier( hub );
		my_iwindow_set_allow_close( MY_IWINDOW( self ), TRUE );

		/* re-enable the actions for the same selection */
		do_enable_actions( self, meta, period );
	}
}

static void
action_on_delete_activated( GSimpleAction *action, GVariant *empty, ofaDossierManager *self )
{
	static const gchar *thisfn = "ofa_dossier_manager_action_on_delete_activated";
	ofaDossierManagerPrivate *priv;
	ofaHub *hub;
	ofaIDBProvider *provider;
	ofaIDBDossierMeta *meta;
	ofaIDBExerciceMeta *period;
	ofaIDBSuperuser *su_bin;
	gchar *msgerr;
	ofaIDBConnect *connect;
	ofaDossierCollection *collection;
	GtkWindow *toplevel;
	gboolean ok_to_delete;

	g_debug( "%s: action=%p, empty=%p, self=%p",
			thisfn, ( void * ) action, ( void * ) empty, ( void * ) self );

	priv = ofa_dossier_manager_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );

	if( ofa_dossier_treeview_get_selected( priv->dossier_tview, &meta, &period )){

		provider = ofa_idbdossier_meta_get_provider( meta );

		g_debug( "%s: selected dossier_meta=%p, exercice_meta=%p, provider=%p",
				thisfn, ( void * ) meta, ( void * ) period, ( void * ) provider );

		/* delete the exercice
		 * delete the dossier when deleting the last exercice */

		ok_to_delete = TRUE;
		su_bin  = ofa_idbprovider_new_superuser_bin( provider, HUB_RULE_EXERCICE_DELETE );

		if( su_bin ){
			ofa_idbsuperuser_set_dossier_meta( su_bin, meta );
			ofa_idbsuperuser_set_with_remember( su_bin, FALSE );
			g_object_ref( su_bin );
			ok_to_delete = ofa_dbsu_run_modal( priv->getter, GTK_WINDOW( self ), su_bin );
		}

		if( ok_to_delete && confirm_delete( self, meta, period )){

			/* close the currently opened dossier/exercice if we are about to delete it */
			if( ofa_hub_is_opened_dossier( hub, period )){
				ofa_hub_close_dossier( hub );
			}

			connect = ofa_idbdossier_meta_new_connect( meta, NULL );
			if( ofa_idbconnect_open_with_superuser( connect, su_bin )){

				msgerr = NULL;
				collection = ofa_igetter_get_dossier_collection( priv->getter );

				if( !ofa_dossier_collection_delete_period( collection, connect, period, TRUE, &msgerr )){
					toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
					my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING, msgerr );
					g_free( msgerr );
				}
			}

			g_object_unref( connect );
		}

		if( su_bin ){
			g_object_unref( su_bin );
			//gtk_widget_destroy( GTK_WIDGET( su_bin ));
		}
	}
}

static gboolean
confirm_delete( ofaDossierManager *self, const ofaIDBDossierMeta *meta, const ofaIDBExerciceMeta *period )
{
	gboolean ok;
	gchar *period_name, *str;
	const gchar *dossier_name;
	GtkWindow *toplevel;

	dossier_name = ofa_idbdossier_meta_get_dossier_name( meta );
	period_name = ofa_idbexercice_meta_get_name( period );
	str = g_strdup_printf(
			_( "You are about to remove the '%s' period from the '%s' dossier.\n"
				"This operation will entirely delete the referenced exercice from the settings "
				"and from the DBMS provider.\n"
				"Are your sure ?" ),
					period_name, dossier_name );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "_Delete" ));

	g_free( str );
	g_free( period_name );

	return( ok );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_manager_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
