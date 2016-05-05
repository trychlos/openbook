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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-account-select.h"
#include "core/ofa-account-store.h"
#include "core/ofa-account-frame-bin.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* input data
	 */
	ofaIGetter          *getter;
	ofeAccountAllowed    allowed;

	/* UI
	 */
	ofaAccountFrameBin  *account_bin;
	ofaAccountStore     *account_store;
	GtkWidget           *ok_btn;

	/* returned value
	 */
	gchar               *account_number;
}
	ofaAccountSelectPrivate;

static const gchar      *st_resource_ui = "/org/trychlos/openbook/core/ofa-account-select.ui";
static ofaAccountSelect *st_this        = NULL;

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_book_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountSelect *self );
static void      on_account_activated( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self );
static void      check_for_enable_dlg( ofaAccountSelect *self );
static gboolean  is_selection_valid( ofaAccountSelect *self, const gchar *number );
static gboolean  idialog_quit_on_ok( myIDialog *instance );
static gboolean  do_select( ofaAccountSelect *self );
static void      on_hub_finalized( gpointer is_null, gpointer finalized_hub );

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
	ofaAccountSelectPrivate *priv;
	gchar *selected_id;
	ofaHub *hub;

	g_debug( "%s: getter=%p, parent=%p, asked_number=%s, allowed=%u",
			thisfn, ( void * ) getter, ( void * ) parent, asked_number, allowed );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	if( !st_this ){
		st_this = g_object_new( OFA_TYPE_ACCOUNT_SELECT, NULL );
		my_iwindow_set_parent( MY_IWINDOW( st_this ), parent );
		my_iwindow_set_settings( MY_IWINDOW( st_this ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

		priv = ofa_account_select_get_instance_private( st_this );
		priv->getter = ofa_igetter_get_permanent_getter( getter );

		my_iwindow_init( MY_IWINDOW( st_this ));
		my_iwindow_set_hide_on_close( MY_IWINDOW( st_this ), TRUE );

		/* setup a weak reference on the hub to auto-unref */
		hub = ofa_igetter_get_hub( getter );
		g_object_weak_ref( G_OBJECT( hub ), ( GWeakNotify ) on_hub_finalized, NULL );
	}

	priv = ofa_account_select_get_instance_private( st_this );

	ofa_account_frame_bin_set_selected( priv->account_bin, asked_number );
	check_for_enable_dlg( st_this );

	g_free( priv->account_number );
	priv->account_number = NULL;
	priv->allowed = allowed;

	selected_id = NULL;

	if( my_idialog_run( MY_IDIALOG( st_this )) == GTK_RESPONSE_OK ){
		selected_id = g_strdup( priv->account_number );
		my_iwindow_close( MY_IWINDOW( st_this ));
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

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "piece-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->account_bin = ofa_account_frame_bin_new( priv->getter );
	my_utils_widget_set_margins( GTK_WIDGET( priv->account_bin ), 4, 4, 4, 0 );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->account_bin ));
	ofa_account_frame_bin_set_cell_data_func(
			priv->account_bin, ( GtkTreeCellDataFunc ) on_book_cell_data_func, instance );

	/* get our own reference on the underlying store */
	priv->account_store = ofa_account_frame_bin_get_account_store( priv->account_bin );

	g_signal_connect( priv->account_bin, "ofa-activated", G_CALLBACK( on_account_activated ), instance );

	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_NEW, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_PROPERTIES, TRUE );
	ofa_account_frame_bin_add_button( priv->account_bin, ACCOUNT_BTN_DELETE, TRUE );

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

/*
 * display in grey italic the non-selectable accounts
 */
static void
on_book_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	ofoAccount *account;
	GdkRGBA color;

	priv = ofa_account_select_get_instance_private( self );

	ofa_account_frame_bin_cell_data_render( priv->account_bin, tcolumn, cell, tmodel, iter );

	gtk_tree_model_get( tmodel, iter, ACCOUNT_COL_OBJECT, &account, -1 );
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));
	g_object_unref( account );

	if( GTK_IS_CELL_RENDERER_TEXT( cell ) && !ofo_account_is_allowed( account, priv->allowed )){
		gdk_rgba_parse( &color, "#b0b0b0" );
		g_object_set( G_OBJECT( cell ), "foreground-rgba", &color, NULL );
		g_object_set( G_OBJECT( cell ), "style", PANGO_STYLE_ITALIC, NULL );
	}
}

static void
on_account_activated( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self )
{
	gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	gchar *account;
	gboolean ok;

	priv = ofa_account_select_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( priv->account_bin );
	ok = is_selection_valid( self, account );
	g_free( account );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
is_selection_valid( ofaAccountSelect *self, const gchar *number )
{
	ofaAccountSelectPrivate *priv;
	gboolean ok;
	ofoAccount *account;
	ofaHub *hub;

	priv = ofa_account_select_get_instance_private( self );

	ok = FALSE;

	if( my_strlen( number )){
		hub = ofa_igetter_get_hub( priv->getter );
		account = ofo_account_get_by_number( hub, number );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

		ok = ofo_account_is_allowed( account, priv->allowed );
	}

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
	gchar *account;
	gboolean ok;

	priv = ofa_account_select_get_instance_private( self );

	account = ofa_account_frame_bin_get_selected( priv->account_bin );
	ok = is_selection_valid( self, account );
	if( ok ){
		priv->account_number = g_strdup( account );
	}
	g_free( account );

	return( ok );
}

static void
on_hub_finalized( gpointer is_null, gpointer finalized_hub )
{
	static const gchar *thisfn = "ofa_account_select_on_hub_finalized";

	g_debug( "%s: empty=%p, finalized_hub=%p",
			thisfn, ( void * ) is_null, ( void * ) finalized_hub );

	g_return_if_fail( st_this && OFA_IS_ACCOUNT_SELECT( st_this ));

	//g_clear_object( &st_this );
	gtk_widget_destroy( GTK_WIDGET( st_this ));
	st_this = NULL;
}
