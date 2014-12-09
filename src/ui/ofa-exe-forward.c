/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-dossier-cur.h"
#include "ui/ofa-exe-forward.h"
#include "ui/ofa-ledger-combo.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaExeForwardPrivate {
	gboolean        dispose_has_run;

	/* runtime data
	 */
	ofaMainWindow  *main_window;
	ofoDossier     *dossier;

	GtkContainer   *parent;
	ofaLedgerCombo *sld_ledger_combo;
	GtkWidget      *sld_ope_entry;
	GtkWidget      *sld_label_entry;
	ofaLedgerCombo *for_ledger_combo;
	GtkWidget      *for_ope_entry;
	GtkWidget      *for_label_close_entry;
	GtkWidget      *for_label_open_entry;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]        = { 0 };

static const gchar *st_ui_xml               = PKGUIDIR "/ofa-exe-forward.piece.ui";
static const gchar *st_ui_id                = "ExeForwardWindow";

static const gchar *st_def_sld_ope          = N_( "CLOSLD" );
static const gchar *st_def_sld_label        = N_( "Account solde for the exercice" );
static const gchar *st_def_for_ope          = N_( "CLORAN" );
static const gchar *st_def_for_label_close  = N_( "Account solde for the exercice" );
static const gchar *st_def_for_label_open   = N_( "Carried forward from previous exercice" );

G_DEFINE_TYPE( ofaExeForward, ofa_exe_forward, G_TYPE_OBJECT )

static void     on_parent_finalized( ofaExeForward *self, gpointer finalized_parent );
static void     setup_solde( ofaExeForward *piece );
static void     setup_forward( ofaExeForward *piece );
static void     on_sld_ledger_new( GtkButton *button, ofaExeForward *piece );
static void     on_sld_ope_select( GtkButton *button, ofaExeForward *piece );
static void     on_for_ledger_new( GtkButton *button, ofaExeForward *piece );
static void     on_for_ope_select( GtkButton *button, ofaExeForward *piece );
static void     on_entry_changed( GtkEditable *editable, ofaExeForward *self );
static void     on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, const gchar *label, ofaExeForward *self );
static void     on_balance_accounts( GtkButton *button, ofaExeForward *self );
static void     check_piece( ofaExeForward *piece );
static gboolean check_for_ledger( ofaExeForward *self, ofaLedgerCombo *combo );
static gboolean check_for_ope( ofaExeForward *self, GtkWidget *entry );
static gboolean check_for_label( ofaExeForward *self, GtkWidget *entry );

static void
exe_forward_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exe_forward_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXE_FORWARD( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_forward_parent_class )->finalize( instance );
}

static void
exe_forward_dispose( GObject *instance )
{
	ofaExeForwardPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXE_FORWARD( instance ));

	priv = OFA_EXE_FORWARD( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_forward_parent_class )->dispose( instance );
}

static void
ofa_exe_forward_init( ofaExeForward *self )
{
	static const gchar *thisfn = "ofa_exe_forward_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXE_FORWARD( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXE_FORWARD, ofaExeForwardPrivate );
}

static void
ofa_exe_forward_class_init( ofaExeForwardClass *klass )
{
	static const gchar *thisfn = "ofa_exe_forward_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exe_forward_dispose;
	G_OBJECT_CLASS( klass )->finalize = exe_forward_finalize;

	g_type_class_add_private( klass, sizeof( ofaExeForwardPrivate ));

	/**
	 * ofaExeForward::changed:
	 *
	 * This signal is sent when one of the field is changed.
	 *
	 * Arguments is the selected ledger.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaExeForward *piece,
	 * 						gpointer     user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_EXE_FORWARD,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );
}

static void
on_parent_finalized( ofaExeForward *self, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_exe_forward_on_parent_finalized";

	g_debug( "%s: self=%p, finalized_parent=%p", thisfn, ( void * ) self, ( void * ) finalized_parent );

	g_return_if_fail( self && OFA_IS_EXE_FORWARD( self ));

	g_object_unref( self );
}

/**
 * ofa_exe_forward_new:
 */
ofaExeForward *
ofa_exe_forward_new( void )
{
	ofaExeForward *self;

	self = g_object_new( OFA_TYPE_EXE_FORWARD, NULL );

	return( self );
}

/**
 * ofa_exe_forward_attach_to:
 */
void
ofa_exe_forward_attach_to( ofaExeForward *piece, GtkContainer *new_parent, ofaMainWindow *main_window )
{
	ofaExeForwardPrivate *priv;
	GtkWidget *window, *forward;

	g_return_if_fail( piece && OFA_IS_EXE_FORWARD( piece ));
	g_return_if_fail( new_parent && GTK_IS_CONTAINER( new_parent ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		g_object_weak_ref( G_OBJECT( new_parent ), ( GWeakNotify ) on_parent_finalized, piece );

		window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
		g_return_if_fail( window && GTK_IS_WINDOW( window ));

		forward = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "p-exe-forward" );
		g_return_if_fail( forward && GTK_IS_CONTAINER( forward ));

		gtk_widget_reparent( forward, GTK_WIDGET( new_parent ));

		priv->main_window = main_window;
		priv->dossier = ofa_main_window_get_dossier( main_window );
		priv->parent = new_parent;

		setup_solde( piece );
		setup_forward( piece );
	}
}

static void
setup_solde( ofaExeForward *piece )
{
	ofaExeForwardPrivate *priv;
	const gchar *cstr;
	GtkWidget *parent, *widget, *image;

	priv = piece->priv;

	/* balancing ledger for closing entries (must exist) */
	parent = my_utils_container_get_child_by_name( priv->parent, "p2-bledger-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->sld_ledger_combo = ofa_ledger_combo_new();
	ofa_ledger_combo_attach_to(
			priv->sld_ledger_combo, FALSE, TRUE, GTK_CONTAINER( parent ));
	ofa_ledger_combo_init_view(
			priv->sld_ledger_combo, priv->dossier, ofo_dossier_get_sld_ledger( priv->dossier ));
	g_signal_connect(
			G_OBJECT( priv->sld_ledger_combo ), "changed", G_CALLBACK( on_ledger_changed ), piece );

	widget = my_utils_container_get_child_by_name( priv->parent, "p2-bledger-new" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	image = gtk_image_new_from_icon_name( "gtk-new", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( widget ), image );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_sld_ledger_new ), piece );
	gtk_widget_set_sensitive( widget, FALSE );

	/* operation mnemo for closing entries
	 * - have a default value
	 * - must be associated with the above ledger */
	priv->sld_ope_entry = my_utils_container_get_child_by_name( priv->parent, "p2-bope-entry" );
	g_return_if_fail( priv->sld_ope_entry && GTK_IS_ENTRY( priv->sld_ope_entry ));
	g_signal_connect( G_OBJECT( priv->sld_ope_entry ), "changed", G_CALLBACK( on_entry_changed ), piece );
	cstr = ofo_dossier_get_sld_ope( priv->dossier );
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		cstr = gettext( st_def_sld_ope );
	}
	gtk_entry_set_text( GTK_ENTRY( priv->sld_ope_entry ), cstr );

	widget = my_utils_container_get_child_by_name( priv->parent, "p2-bope-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( widget ), image );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_sld_ope_select ), piece );
	gtk_widget_set_sensitive( widget, FALSE );

	/* closing entries label */
	priv->sld_label_entry = my_utils_container_get_child_by_name( priv->parent, "p2-label" );
	g_return_if_fail( priv->sld_label_entry && GTK_IS_ENTRY( priv->sld_label_entry ));
	g_signal_connect( G_OBJECT( priv->sld_label_entry ), "changed", G_CALLBACK( on_entry_changed ), piece );
	cstr = ofo_dossier_get_sld_label( priv->dossier );
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		cstr = st_def_sld_label;
	}
	gtk_entry_set_text( GTK_ENTRY( priv->sld_label_entry ), cstr );

	/* balancing accounts for currencies */
	widget = my_utils_container_get_child_by_name( priv->parent, "p2-balance-accounts" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	g_signal_connect( widget, "clicked", G_CALLBACK( on_balance_accounts ), piece );
}

static void
setup_forward( ofaExeForward *piece )
{
	ofaExeForwardPrivate *priv;
	const gchar *cstr;
	GtkWidget *parent, *widget, *image;

	priv = piece->priv;

	/* forward ledger */
	parent = my_utils_container_get_child_by_name( priv->parent, "p2-fledger-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->for_ledger_combo = ofa_ledger_combo_new();
	ofa_ledger_combo_attach_to(
			priv->for_ledger_combo, FALSE, TRUE, GTK_CONTAINER( parent ));
	ofa_ledger_combo_init_view(
			priv->for_ledger_combo, priv->dossier, ofo_dossier_get_forward_ledger( priv->dossier ));
	g_signal_connect(
			G_OBJECT( priv->for_ledger_combo ), "changed", G_CALLBACK( on_ledger_changed ), piece );

	widget = my_utils_container_get_child_by_name( priv->parent, "p2-fledger-new" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	image = gtk_image_new_from_icon_name( "gtk-new", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( widget ), image );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_for_ledger_new ), piece );
	gtk_widget_set_sensitive( widget, FALSE );

	/* forward ope template */
	priv->for_ope_entry = my_utils_container_get_child_by_name( priv->parent, "p2-fope-entry" );
	g_return_if_fail( priv->for_ope_entry && GTK_IS_ENTRY( priv->for_ope_entry ));
	g_signal_connect( G_OBJECT( priv->for_ope_entry ), "changed", G_CALLBACK( on_entry_changed ), piece );
	cstr = ofo_dossier_get_forward_ope( priv->dossier );
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		cstr = gettext( st_def_for_ope );
	}
	gtk_entry_set_text( GTK_ENTRY( priv->for_ope_entry ), cstr );

	widget = my_utils_container_get_child_by_name( priv->parent, "p2-fope-select" );
	g_return_if_fail( widget && GTK_IS_BUTTON( widget ));
	image = gtk_image_new_from_icon_name( "gtk-index", GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( GTK_BUTTON( widget ), image );
	g_signal_connect( G_OBJECT( widget ), "clicked", G_CALLBACK( on_for_ope_select ), piece );
	gtk_widget_set_sensitive( widget, FALSE );

	/* forward entries close label */
	priv->for_label_close_entry = my_utils_container_get_child_by_name( priv->parent, "p2-label-close" );
	g_return_if_fail( priv->for_label_close_entry && GTK_IS_ENTRY( priv->for_label_close_entry ));
	g_signal_connect( G_OBJECT( priv->for_label_close_entry ), "changed", G_CALLBACK( on_entry_changed ), piece );
	cstr = ofo_dossier_get_forward_label_close( priv->dossier );
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		cstr = gettext( st_def_for_label_close );
	}
	gtk_entry_set_text( GTK_ENTRY( priv->for_label_close_entry ), cstr );

	/* forward entries open label */
	priv->for_label_open_entry = my_utils_container_get_child_by_name( priv->parent, "p2-label-open" );
	g_return_if_fail( priv->for_label_open_entry && GTK_IS_ENTRY( priv->for_label_open_entry ));
	g_signal_connect( G_OBJECT( priv->for_label_open_entry ), "changed", G_CALLBACK( on_entry_changed ), piece );
	cstr = ofo_dossier_get_forward_label_open( priv->dossier );
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		cstr = gettext( st_def_for_label_open );
	}
	gtk_entry_set_text( GTK_ENTRY( priv->for_label_open_entry ), cstr );
}

#if 0
static void
on_account_select( GtkButton *button, ofaExeForward *piece )
{
	ofaExeForwardPrivate *priv;
	gchar *acc_number;

	priv = piece->priv;

	acc_number = ofa_account_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), acc_number );
		g_free( acc_number );
	}
}
#endif

static void
on_sld_ledger_new( GtkButton *button, ofaExeForward *piece )
{
#if 0
	ofaExeForwardPrivate *priv;
	gchar *acc_number;

	priv = piece->priv;

	acc_number = ofa_account_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), acc_number );
		g_free( acc_number );
	}
#endif
}

static void
on_sld_ope_select( GtkButton *button, ofaExeForward *piece )
{
#if 0
	ofaExeForwardPrivate *priv;
	gchar *acc_number;

	priv = piece->priv;

	acc_number = ofa_account_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), acc_number );
		g_free( acc_number );
	}
#endif
}

static void
on_for_ledger_new( GtkButton *button, ofaExeForward *piece )
{
#if 0
	ofaExeForwardPrivate *priv;
	gchar *acc_number;

	priv = piece->priv;

	acc_number = ofa_account_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), acc_number );
		g_free( acc_number );
	}
#endif
}

static void
on_for_ope_select( GtkButton *button, ofaExeForward *piece )
{
#if 0
	ofaExeForwardPrivate *priv;
	gchar *acc_number;

	priv = piece->priv;

	acc_number = ofa_account_select_run(
							priv->main_window,
							gtk_entry_get_text( GTK_ENTRY( priv->account_entry )));
	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), acc_number );
		g_free( acc_number );
	}
#endif
}

static void
on_entry_changed( GtkEditable *editable, ofaExeForward *self )
{
	check_piece( self );
}

static void
on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, const gchar *label, ofaExeForward *self )
{
	check_piece( self );
}

static void
on_balance_accounts( GtkButton *button, ofaExeForward *self )
{
	ofaExeForwardPrivate *priv;
	GtkWindow *parent;

	priv = self->priv;

	parent = my_utils_widget_get_toplevel_window( GTK_WIDGET( button ));
	ofa_dossier_cur_run( priv->main_window, parent );
}

static void
check_piece( ofaExeForward *self )
{
	g_signal_emit_by_name( self, "changed" );
}

/**
 * ofa_exe_forward_check:
 */
gboolean
ofa_exe_forward_check( ofaExeForward *piece )
{
	ofaExeForwardPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( piece && OFA_IS_EXE_FORWARD( piece ), FALSE );

	priv = piece->priv;
	ok = FALSE;

	if( !priv->dispose_has_run ){

		ok = TRUE;
		/*if( ok ) ok &= check_for_account( piece, priv->account_entry );*/
		if( ok ) ok &= check_for_ledger( piece, priv->sld_ledger_combo );
		if( ok ) ok &= check_for_ope( piece, priv->sld_ope_entry );
		if( ok ) ok &= check_for_label( piece, priv->sld_label_entry );

		if( ok ) ok &= check_for_ledger( piece, priv->for_ledger_combo );
		if( ok ) ok &= check_for_ope( piece, priv->for_ope_entry );
		if( ok ) ok &= check_for_label( piece, priv->for_label_close_entry );
		if( ok ) ok &= check_for_label( piece, priv->for_label_open_entry );
	}

	return( ok );
}

static gboolean
check_for_ledger( ofaExeForward *self, ofaLedgerCombo *combo )
{
	static const gchar *thisfn = "ofa_exe_forward_check_for_ledger";
	ofaExeForwardPrivate *priv;
	gchar *mnemo;
	ofoLedger *ledger;

	priv = self->priv;

	mnemo = ofa_ledger_combo_get_selected( priv->for_ledger_combo );
	if( !mnemo || !g_utf8_strlen( mnemo, -1 )){
		g_debug( "%s: empty ledger mnemo", thisfn );
		g_free( mnemo );
		return( FALSE );
	}
	ledger = ofo_ledger_get_by_mnemo( priv->dossier, mnemo );
	if( !ledger ){
		g_debug( "%s: ledger not found: %s", thisfn, mnemo );
		g_free( mnemo );
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_LEDGER( ledger ), FALSE );
	g_free( mnemo );

	return( TRUE );
}

static gboolean
check_for_ope( ofaExeForward *self, GtkWidget *entry )
{
	static const gchar *thisfn = "ofa_exe_forward_check_for_ope";
	ofaExeForwardPrivate *priv;
	const gchar *cstr;
	ofoOpeTemplate *ope;
	gchar *mnemo;
	gint cmp;

	priv = self->priv;

	cstr = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		g_debug( "%s: empty operation template mnemo", thisfn );
		return( FALSE );
	}
	ope = ofo_ope_template_get_by_mnemo( priv->dossier, cstr );
	if( !ope ){
		g_debug( "%s: operation template not found: %s", thisfn, cstr );
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( ope ), FALSE );

	mnemo = ofa_ledger_combo_get_selected( priv->for_ledger_combo );
	cstr = ofo_ope_template_get_ledger( ope );
	cmp = g_utf8_collate( mnemo, cstr );
	g_free( mnemo );
	if( cmp != 0 ){
		g_debug( "%s: operation template is attached to %s ledger", thisfn, cstr );
		return( FALSE );
	}

	return( TRUE );
}

static gboolean
check_for_label( ofaExeForward *self, GtkWidget *entry )
{
	static const gchar *thisfn = "ofa_exe_forward_check_for_label";
	const gchar *cstr;

	cstr = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		g_debug( "%s: empty label", thisfn );
		return( FALSE );
	}

	return( TRUE );
}

#if 0
static gboolean
check_for_account( ofaExeForward *self, GtkWidget *entry )
{
	static const gchar *thisfn = "ofa_exe_forward_check_for_account";
	ofaExeForwardPrivate *priv;
	const gchar *cstr;
	ofoAccount *account;

	priv = self->priv;

	cstr = gtk_entry_get_text( GTK_ENTRY( entry ));
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		g_debug( "%s: empty account number", thisfn );
		return( FALSE );
	}
	account = ofo_account_get_by_number( priv->dossier, cstr );
	if( !account ){
		g_debug( "%s: account not found: %s", thisfn, cstr );
		return( FALSE );
	}
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	return( TRUE );
}
#endif

/**
 * ofa_exe_forward_apply:
 */
void
ofa_exe_forward_apply( ofaExeForward *piece )
{
	ofaExeForwardPrivate *priv;
	gchar *mnemo;

	g_return_if_fail( piece && OFA_IS_EXE_FORWARD( piece ));

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		mnemo = ofa_ledger_combo_get_selected( priv->for_ledger_combo );
		ofo_dossier_set_forward_ledger( priv->dossier, mnemo );
		g_free( mnemo );

		ofo_dossier_set_forward_ope( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->for_ope_entry )));

		ofo_dossier_set_forward_label_close( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->for_label_close_entry )));

		ofo_dossier_set_forward_label_open( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->for_label_open_entry )));

		mnemo = ofa_ledger_combo_get_selected( priv->sld_ledger_combo );
		ofo_dossier_set_sld_ledger( priv->dossier, mnemo );
		g_free( mnemo );

		ofo_dossier_set_sld_ope( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->sld_ope_entry )));

		ofo_dossier_set_sld_label( priv->dossier,
					gtk_entry_get_text( GTK_ENTRY( priv->sld_label_entry )));
	}
}
