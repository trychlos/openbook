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

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-account-store.h"
#include "ui/ofa-account-frame-bin.h"

/* private instance data
 */
struct _ofaAccountSelectPrivate {
	gboolean             dispose_has_run;

	/* input data
	 */
	ofaHub              *hub;
	gboolean             is_current;
	gint                 allowed;

	/* UI
	 */
	ofaAccountFrameBin  *account_bin;
	GtkWidget           *properties_btn;
	GtkWidget           *delete_btn;
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;

	/* returned value
	 */
	gchar               *account_number;
};

static const gchar      *st_resource_ui = "/org/trychlos/openbook/ui/ofa-account-select.ui";
static ofaAccountSelect *st_this        = NULL;

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_book_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountSelect *self );
static void      on_new_clicked( GtkButton *button, ofaAccountSelect *self );
static void      on_properties_clicked( GtkButton *button, ofaAccountSelect *self );
static void      on_delete_clicked( GtkButton *button, ofaAccountSelect *self );
static void      on_account_changed( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self );
static void      on_account_activated( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self );
static void      check_for_enable_dlg( ofaAccountSelect *self );
static gboolean  is_selection_valid( ofaAccountSelect *self, const gchar *number );
static void      do_update_sensitivity( ofaAccountSelect *self, ofoAccount *account );
static gboolean  idialog_quit_on_ok( myIDialog *instance );
static gboolean  do_select( ofaAccountSelect *self );
static void      set_message( ofaAccountSelect *self, const gchar *str );
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
 * @main_window: the #ofaMainWindow main window of the application.
 * @asked_number: [allow-none]: the initially selected account identifier.
 * @allowed: flags which qualifies the allowed selection (see ofoAccount.h).
 *
 * Returns: the selected account identifier, as a newly allocated string
 * that must be g_free() by the caller.
 */
gchar *
ofa_account_select_run( const ofaMainWindow *main_window, const gchar *asked_number, gint allowed )
{
	static const gchar *thisfn = "ofa_account_select_run";
	ofaAccountSelectPrivate *priv;
	gchar *selected_id;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, asked_number=%s",
			thisfn, ( void * ) main_window, asked_number );

	if( !st_this ){
		st_this = g_object_new( OFA_TYPE_ACCOUNT_SELECT, NULL );
		my_iwindow_set_main_window( MY_IWINDOW( st_this ), GTK_APPLICATION_WINDOW( main_window ));

		priv = ofa_account_select_get_instance_private( st_this );

		priv->hub = ofa_main_window_get_hub( main_window );
		g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

		my_iwindow_init( MY_IWINDOW( st_this ));
		my_iwindow_set_hide_on_close( MY_IWINDOW( st_this ), TRUE );

		/* setup a weak reference on the hub to auto-unref */
		g_object_weak_ref( G_OBJECT( priv->hub ), ( GWeakNotify ) on_hub_finalized, NULL );
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
	GtkApplicationWindow *main_window;
	GtkWidget *parent, *btn;
	ofaButtonsBox *box;
	ofoDossier *dossier;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_account_select_get_instance_private( OFA_ACCOUNT_SELECT( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p-message" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	my_utils_widget_set_style( priv->msg_label, "labelerror" );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "piece-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->account_bin = ofa_account_frame_bin_new( OFA_MAIN_WINDOW( main_window ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->account_bin ));
	ofa_account_frame_bin_set_cell_data_func(
			priv->account_bin, ( GtkTreeCellDataFunc ) on_book_cell_data_func, instance );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->is_current = ofo_dossier_is_current( dossier );

	box = ofa_account_frame_bin_get_buttons_box( priv->account_bin );

	btn = ofa_buttons_box_add_button_with_mnemonic( box, BUTTON_NEW, G_CALLBACK( on_new_clicked ), instance );
	gtk_widget_set_sensitive( btn, priv->is_current );

	btn = ofa_buttons_box_add_button_with_mnemonic( box, BUTTON_PROPERTIES, G_CALLBACK( on_properties_clicked ), instance );
	priv->properties_btn = btn;

	ofa_buttons_box_add_button_with_mnemonic( box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), instance );
	priv->delete_btn = btn;

	g_signal_connect( priv->account_bin, "ofa-changed", G_CALLBACK( on_account_changed ), instance );
	g_signal_connect( priv->account_bin, "ofa-activated", G_CALLBACK( on_account_activated ), instance );

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
on_new_clicked( GtkButton *button, ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;

	priv = ofa_account_select_get_instance_private( self );

	ofa_account_frame_bin_do_new( priv->account_bin );
}

static void
on_properties_clicked( GtkButton *button, ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;

	priv = ofa_account_select_get_instance_private( self );

	ofa_account_frame_bin_do_properties( priv->account_bin );
}

static void
on_delete_clicked( GtkButton *button, ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;

	priv = ofa_account_select_get_instance_private( self );

	ofa_account_frame_bin_do_delete( priv->account_bin );
}

static void
on_account_changed( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self )
{
	check_for_enable_dlg( self );
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

	priv = ofa_account_select_get_instance_private( self );

	ok = FALSE;
	set_message( self, "" );

	if( my_strlen( number )){
		account = ofo_account_get_by_number( priv->hub, number );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

		do_update_sensitivity( self, account );
		ok = ofo_account_is_allowed( account, priv->allowed );
	}

	return( ok );
}

static void
do_update_sensitivity( ofaAccountSelect *self, ofoAccount *account )
{
	ofaAccountSelectPrivate *priv;
	gboolean has_account;

	priv = ofa_account_select_get_instance_private( self );

	has_account = ( account && OFO_IS_ACCOUNT( account ));

	gtk_widget_set_sensitive( priv->properties_btn, has_account );
	gtk_widget_set_sensitive( priv->delete_btn, has_account && priv->is_current && ofo_account_is_deletable( account ));
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
set_message( ofaAccountSelect *self, const gchar *str )
{
	ofaAccountSelectPrivate *priv;

	priv = ofa_account_select_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), str );
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
