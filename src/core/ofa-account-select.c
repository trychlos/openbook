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

#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-account-frame-bin.h"
#include "core/ofa-account-select.h"
#include "core/ofa-account-store.h"
#include "core/ofa-account-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* input data
	 */
	ofaIGetter          *getter;
	ofeAccountAllowed    allowed;
	gchar               *settings_prefix;

	/* UI
	 */
	ofaAccountFrameBin  *account_bin;
	GtkWidget           *ok_btn;

	/* returned value
	 */
	gchar               *account_number;
}
	ofaAccountSelectPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-account-select.ui";

static ofaAccountSelect *account_select_new( ofaIGetter *getter, GtkWindow *parent );
static void              iwindow_iface_init( myIWindowInterface *iface );
static void              idialog_iface_init( myIDialogInterface *iface );
static void              idialog_init( myIDialog *instance );
static void              on_treeview_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountSelect *self );
static void              on_selection_changed( ofaAccountFrameBin *bin, ofoAccount *account, ofaAccountSelect *self );
static void              on_selection_activated( ofaAccountFrameBin *bin, ofoAccount *account, ofaAccountSelect *self );
static void              check_for_enable_dlg( ofaAccountSelect *self );
static void              check_for_enable_dlg_with_account( ofaAccountSelect *self, ofoAccount *account );
static gboolean          is_selection_valid( ofaAccountSelect *self, ofoAccount *account );
static gboolean          idialog_quit_on_ok( myIDialog *instance );
static gboolean          do_select( ofaAccountSelect *self );
static void              write_settings( ofaAccountSelect *self );

G_DEFINE_TYPE_EXTENDED( ofaAccountSelect, ofa_account_select, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaAccountSelect )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
account_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_select_finalize";
	ofaAccountSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	/* free data members here */
	priv = ofa_account_select_get_instance_private( OFA_ACCOUNT_SELECT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->finalize( instance );
}

static void
account_select_dispose( GObject *instance )
{
	ofaAccountSelectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	priv = ofa_account_select_get_instance_private( OFA_ACCOUNT_SELECT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->dispose( instance );
}

static void
ofa_account_select_init( ofaAccountSelect *self )
{
	static const gchar *thisfn = "ofa_account_select_init";
	ofaAccountSelectPrivate *priv;

	g_return_if_fail( self && OFA_IS_ACCOUNT_SELECT( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_account_select_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_account_select_class_init( ofaAccountSelectClass *klass )
{
	static const gchar *thisfn = "ofa_account_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_select_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/*
 * Returns: the unique #ofaAccountSelect instance.
 */
static ofaAccountSelect *
account_select_new( ofaIGetter *getter, GtkWindow *parent )
{
	ofaAccountSelect *dialog;
	ofaAccountSelectPrivate *priv;
	ofaHub *hub;
	myICollector *collector;

	hub = ofa_igetter_get_hub( getter );
	collector = ofa_hub_get_collector( hub );

	dialog = ( ofaAccountSelect * ) my_icollector_single_get_object( collector, OFA_TYPE_ACCOUNT_SELECT );

	if( !dialog ){
		dialog = g_object_new( OFA_TYPE_ACCOUNT_SELECT, NULL );
		my_iwindow_set_parent( MY_IWINDOW( dialog ), parent );
		my_iwindow_set_settings( MY_IWINDOW( dialog ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

		/* setup a permanent getter before initialization */
		priv = ofa_account_select_get_instance_private( dialog );
		priv->getter = ofa_igetter_get_permanent_getter( getter );
		my_iwindow_init( MY_IWINDOW( dialog ));

		/* and record this unique object */
		my_icollector_single_set_object( collector, dialog );
	}

	return( dialog );
}

/**
 * ofa_account_select_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @asked_number: [allow-none]: the initially selected account identifier.
 * @allowed: flags which qualifies the allowed selection (see ofoAccount.h).
 *
 * Returns: the selected account identifier, as a newly allocated string
 * that must be g_free() by the caller.
 */
gchar *
ofa_account_select_run( ofaIGetter *getter, GtkWindow *parent, const gchar *asked_number, ofeAccountAllowed allowed )
{
	static const gchar *thisfn = "ofa_account_select_run";
	ofaAccountSelect *dialog;
	ofaAccountSelectPrivate *priv;
	gchar *selected_id;

	g_debug( "%s: getter=%p, parent=%p, asked_number=%s, allowed=%u",
			thisfn, ( void * ) getter, ( void * ) parent, asked_number, allowed );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), NULL );

	dialog = account_select_new( getter, parent );
	priv = ofa_account_select_get_instance_private( dialog );

	ofa_account_frame_bin_set_selected( priv->account_bin, asked_number );
	check_for_enable_dlg( dialog );

	g_free( priv->account_number );
	priv->account_number = NULL;
	priv->allowed = allowed;

	selected_id = NULL;

	if( my_idialog_run( MY_IDIALOG( dialog )) == GTK_RESPONSE_OK ){
		selected_id = g_strdup( priv->account_number );
		write_settings( dialog );
		gtk_widget_hide( GTK_WIDGET( dialog ));
	}

	return( selected_id );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_account_select_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_account_select_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_account_select_idialog_init";
	ofaAccountSelectPrivate *priv;
	GtkWidget *parent;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_account_select_get_instance_private( OFA_ACCOUNT_SELECT( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "bin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->account_bin = ofa_account_frame_bin_new( priv->getter );
	my_utils_widget_set_margins( GTK_WIDGET( priv->account_bin ), 0, 4, 0, 0 );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->account_bin ));
	ofa_account_frame_bin_set_settings_key( priv->account_bin, priv->settings_prefix );
	ofa_account_frame_bin_set_cell_data_func( priv->account_bin, ( GtkTreeCellDataFunc ) on_treeview_cell_data_func, instance );

	g_signal_connect( priv->account_bin, "ofa-changed", G_CALLBACK( on_selection_changed ), instance );
	g_signal_connect( priv->account_bin, "ofa-activated", G_CALLBACK( on_selection_activated ), instance );

	ofa_account_frame_bin_add_action( priv->account_bin, ACCOUNT_ACTION_NEW );
	ofa_account_frame_bin_add_action( priv->account_bin, ACCOUNT_ACTION_UPDATE );
	ofa_account_frame_bin_add_action( priv->account_bin, ACCOUNT_ACTION_DELETE );

	ofa_account_frame_bin_load_dataset( priv->account_bin );

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

/*
 * display in grey italic the non-selectable accounts
 */
static void
on_treeview_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	ofoAccount *account;
	GdkRGBA color;
	GtkWidget *treeview;

	priv = ofa_account_select_get_instance_private( self );

	treeview = ofa_account_frame_bin_get_current_page( priv->account_bin );
	g_return_if_fail( treeview && OFA_IS_ACCOUNT_TREEVIEW( treeview ));

	ofa_account_treeview_cell_data_render( OFA_ACCOUNT_TREEVIEW( treeview ), tcolumn, cell, tmodel, iter );

	gtk_tree_model_get( tmodel, iter, ACCOUNT_COL_OBJECT, &account, -1 );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
	g_object_unref( account );

	/*
	g_debug( "account=%s, allowed=%d, ok=%s",
			ofo_account_get_number( account),
			priv->allowed,
			ofo_account_is_allowed( account, priv->allowed ) ? "True":"False" );
			*/

	if( GTK_IS_CELL_RENDERER_TEXT( cell ) && !ofo_account_is_allowed( account, priv->allowed )){
		gdk_rgba_parse( &color, "#b0b0b0" );
		g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
		g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
	}
}

static void
on_selection_changed( ofaAccountFrameBin *bin, ofoAccount *account, ofaAccountSelect *self )
{
	check_for_enable_dlg_with_account( self, account );
}

static void
on_selection_activated( ofaAccountFrameBin *bin, ofoAccount *account, ofaAccountSelect *self )
{
	gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	ofoAccount *account;

	priv = ofa_account_select_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( priv->account_bin );
	check_for_enable_dlg_with_account( self, account );
}

static void
check_for_enable_dlg_with_account( ofaAccountSelect *self, ofoAccount *account )
{
	ofaAccountSelectPrivate *priv;
	gboolean ok;

	priv = ofa_account_select_get_instance_private( self );

	ok = is_selection_valid( self, account );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
is_selection_valid( ofaAccountSelect *self, ofoAccount *account )
{
	ofaAccountSelectPrivate *priv;
	gboolean ok;

	priv = ofa_account_select_get_instance_private( self );

	/* account may be %NULL */
	ok = account ? ofo_account_is_allowed( account, priv->allowed ) : FALSE;

	return( ok );
}

static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	return( do_select( OFA_ACCOUNT_SELECT( instance )));
}

static gboolean
do_select( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	ofoAccount *account;
	gboolean ok;

	priv = ofa_account_select_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( priv->account_bin );
	ok = is_selection_valid( self, account );
	if( ok ){
		priv->account_number = g_strdup( ofo_account_get_number( account ));
	}

	return( ok );
}

static void
write_settings( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	GtkWidget *current_page;
	GList *pages_list;

	priv = ofa_account_select_get_instance_private( self );

	/* save the settings before hiding */
	current_page = ofa_account_frame_bin_get_current_page( priv->account_bin );
	ofa_itvcolumnable_write_columns_settings( OFA_ITVCOLUMNABLE( current_page ));

	/* propagate the visible columns to other pages of the book */
	pages_list = ofa_account_frame_bin_get_pages_list( priv->account_bin );
	ofa_itvcolumnable_propagate_visible_columns( OFA_ITVCOLUMNABLE( current_page ), pages_list );
	g_list_free( pages_list );
}
