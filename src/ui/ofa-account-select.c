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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofo-account.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-account-store.h"
#include "ui/ofa-account-chart-bin.h"
#include "ui/ofa-account-frame-bin.h"

/* private instance data
 */
struct _ofaAccountSelectPrivate {

	/* input data
	 */
	const ofaMainWindow *main_window;
	ofoDossier          *dossier;
	gint                 allowed;

	/* UI
	 */
	GtkWindow           *toplevel;
	ofaAccountFrameBin  *account_frame;
	ofaAccountChartBin  *account_chart;
	GtkWidget           *ok_btn;
	GtkWidget           *msg_label;

	/* returned value
	 */
	gchar               *account_number;
};

static const gchar      *st_ui_xml      = PKGUIDIR "/ofa-account-select.ui";
static const gchar      *st_ui_id       = "AccountSelectDlg";
static ofaAccountSelect *st_this        = NULL;

G_DEFINE_TYPE( ofaAccountSelect, ofa_account_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_book_cell_data_func( GtkTreeViewColumn *tcolumn, GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter, ofaAccountSelect *self );
static void      on_account_changed( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self );
static void      on_account_activated( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self );
static void      check_for_enable_dlg( ofaAccountSelect *self );
static gboolean  is_selection_valid( ofaAccountSelect *self, const gchar *number );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_select( ofaAccountSelect *self );
static void      set_message( ofaAccountSelect *self, const gchar *str );

static void
account_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_select_finalize";
	ofaAccountSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	/* free data members here */
	priv = OFA_ACCOUNT_SELECT( instance )->priv;

	g_free( priv->account_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->finalize( instance );
}

static void
account_select_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_select_parent_class )->dispose( instance );
}

static void
ofa_account_select_init( ofaAccountSelect *self )
{
	static const gchar *thisfn = "ofa_account_select_init";

	g_return_if_fail( self && OFA_IS_ACCOUNT_SELECT( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_ACCOUNT_SELECT, ofaAccountSelectPrivate );
}

static void
ofa_account_select_class_init( ofaAccountSelectClass *klass )
{
	static const gchar *thisfn = "ofa_account_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_select_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaAccountSelectPrivate ));
}

static void
on_dossier_finalized( gpointer is_null, gpointer finalized_dossier )
{
	static const gchar *thisfn = "ofa_account_select_on_dossier_finalized";

	g_debug( "%s: empty=%p, finalized_dossier=%p",
			thisfn, ( void * ) is_null, ( void * ) finalized_dossier );

	g_return_if_fail( st_this && OFA_IS_ACCOUNT_SELECT( st_this ));

	g_clear_object( &st_this );
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
	ofaAccountChartBin *book;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, asked_number=%s",
			thisfn, ( void * ) main_window, asked_number );

	if( !st_this ){

		st_this = g_object_new(
				OFA_TYPE_ACCOUNT_SELECT,
				MY_PROP_MAIN_WINDOW,   main_window,
				MY_PROP_WINDOW_XML,    st_ui_xml,
				MY_PROP_WINDOW_NAME,   st_ui_id,
				MY_PROP_SIZE_POSITION, FALSE,
				NULL );

		st_this->priv->main_window = main_window;
		st_this->priv->dossier = ofa_main_window_get_dossier( main_window );
		st_this->priv->toplevel = my_window_get_toplevel( MY_WINDOW( st_this ));
		my_utils_window_restore_position( st_this->priv->toplevel, st_ui_id );
		my_dialog_init_dialog( MY_DIALOG( st_this ));

		/* setup a weak reference on the dossier to auto-unref */
		g_object_weak_ref(
				G_OBJECT( st_this->priv->dossier ), ( GWeakNotify ) on_dossier_finalized, NULL );
	}

	priv = st_this->priv;

	book = ofa_account_frame_bin_get_chart( priv->account_frame );
	ofa_account_chart_bin_set_selected( book, asked_number );
	check_for_enable_dlg( st_this );

	g_free( priv->account_number );
	priv->account_number = NULL;
	priv->allowed = allowed;

	my_dialog_run_dialog( MY_DIALOG( st_this ));

	my_utils_window_save_position( st_this->priv->toplevel, st_ui_id );
	gtk_widget_hide( GTK_WIDGET( st_this->priv->toplevel ));

	return( g_strdup( priv->account_number ));
}

static void
v_init_dialog( myDialog *dialog )
{
	static const gchar *thisfn = "ofa_account_select_v_init_dialog";
	ofaAccountSelectPrivate *priv;
	GtkWidget *parent;

	g_debug( "%s: dialog=%p", thisfn, ( void * ) dialog );

	priv = OFA_ACCOUNT_SELECT( dialog )->priv;

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->toplevel ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->toplevel ), "p-message" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	my_utils_widget_set_style( priv->msg_label, "labelerror" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->toplevel ), "piece-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->account_frame = ofa_account_frame_bin_new( priv->main_window );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->account_frame ));
	priv->account_chart = ofa_account_frame_bin_get_chart( priv->account_frame );
	ofa_account_chart_bin_set_cell_data_func(
			priv->account_chart, ( GtkTreeCellDataFunc ) on_book_cell_data_func, dialog );
	ofa_account_frame_bin_set_buttons( priv->account_frame, FALSE, FALSE, FALSE );

	g_signal_connect(
			G_OBJECT( priv->account_frame ), "ofa-changed", G_CALLBACK( on_account_changed ), dialog );
	g_signal_connect(
			G_OBJECT( priv->account_frame ), "ofa-activated", G_CALLBACK( on_account_activated ), dialog );
}

/*
 * level 1: not displayed (should not appear)
 * level 2 and root: bold, colored background
 * level 3 and root: colored background
 * other root: italic
 *
 * detail accounts who have no currency are red written.
 */
static void
on_book_cell_data_func( GtkTreeViewColumn *tcolumn,
							GtkCellRenderer *cell, GtkTreeModel *tmodel, GtkTreeIter *iter,
							ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	ofoAccount *account;
	GdkRGBA color;

	priv = self->priv;

	ofa_account_chart_bin_cell_data_renderer( priv->account_chart, tcolumn, cell, tmodel, iter );

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
on_account_changed( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self )
{
	check_for_enable_dlg( self );
}

static void
on_account_activated( ofaAccountFrameBin *piece, const gchar *number, ofaAccountSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	gchar *account;
	gboolean ok;

	priv = self->priv;

	account = ofa_account_chart_bin_get_selected( priv->account_chart );
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

	priv = self->priv;
	ok = FALSE;
	set_message( self, "" );

	if( my_strlen( number )){
		account = ofo_account_get_by_number( priv->dossier, number );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

		ok = ofo_account_is_allowed( account, priv->allowed );
	}

	return( ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_select( OFA_ACCOUNT_SELECT( dialog )));
}

static gboolean
do_select( ofaAccountSelect *self )
{
	ofaAccountSelectPrivate *priv;
	ofaAccountChartBin *book;
	gchar *account;
	gboolean ok;

	priv = self->priv;

	book = ofa_account_frame_bin_get_chart( priv->account_frame );
	account = ofa_account_chart_bin_get_selected( book );
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

	priv = self->priv;

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), str );
}
