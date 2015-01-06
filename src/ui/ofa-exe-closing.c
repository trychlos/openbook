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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "core/my-window-prot.h"
#include "core/ofa-preferences.h"

#include "ui/my-assistant.h"
#include "ui/my-editable-date.h"
#include "ui/my-progress-bar.h"
#include "ui/ofa-balances-grid.h"
#include "ui/ofa-exe-closing.h"
#include "ui/ofa-exe-forward-piece.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-misc-chkbal.h"

/* private instance data
 */
struct _ofaExeClosingPrivate {

	/* p2 - parms
	 */
	GtkWidget     *p2_begin_cur;
	GtkWidget     *p2_end_cur;
	GtkWidget     *p2_begin_next;
	GtkWidget     *p2_end_next;
	ofaExeForwardPiece *p2_forward;

	/* p3 - while checks are run, store GtkAssistant datas
	 */
	GtkAssistant  *assistant;
	GtkWidget     *page_widget;
	gboolean       p3_entries_ok;
	GList         *p3_entries_list;
	gboolean       p3_ledgers_ok;
	GList         *p3_ledgers_list;
	gboolean       p3_accounts_ok;
	GList         *p3_accounts_list;
	gboolean       p3_done;

	/* p5 - close the exercice
	 */
	GList         *p5_forwards;
	GList         *p5_unsettled;
	GList         *p5_unreconciliated;
};

/* the pages of this assistant
 * note that pages are numbered from zero by GtkAssistant,
 * but from 1 by GtkBuilder !
 * So our page_names are numbered from 1 to stay sync with widgets
 */
enum {
	PAGE_INTRO = 0,						/* Intro */
	PAGE_PARMS,							/* p2 - Content */
	PAGE_CHECKS,						/* p3 - Content */
	PAGE_CONFIRM,						/* Confirm */
	PAGE_SUMMARY						/* Summary */
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-exe-closing.ui";
static const gchar *st_ui_id            = "ExeClosingAssistant";

G_DEFINE_TYPE( ofaExeClosing, ofa_exe_closing, MY_TYPE_ASSISTANT )

static void             on_prepare( GtkAssistant *assistant, GtkWidget *page_widget, ofaExeClosing *self );
static void             on_page_forward( ofaExeClosing *self, GtkWidget *page_widget, gint   page_num, void *empty );
static void             p2_do_init( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static void             p2_display( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static void             p2_on_date_changed( GtkEditable *editable, ofaExeClosing *self );
static void             p2_on_forward_changed( ofaExeForwardPiece *piece, ofaExeClosing *self );
static void             p2_check_for_complete( ofaExeClosing *self );
static void             p2_do_forward( ofaExeClosing *self, GtkWidget *page_widget );
static void             p3_do_init( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static void             p3_checks( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static gboolean         p3_check_entries_balance( ofaExeClosing *self );
static gboolean         p3_check_ledgers_balance( ofaExeClosing *self );
static gboolean         p3_check_accounts_balance( ofaExeClosing *self );
static myProgressBar   *get_new_bar( ofaExeClosing *self, const gchar *w_name );
static ofaBalancesGrid *p3_get_new_balances( ofaExeClosing *self, const gchar *w_name );
static void             p3_check_status( ofaExeClosing *self, gboolean ok, const gchar *w_name );
static gboolean         p3_info_checks( ofaExeClosing *self );
static void             on_apply( GtkAssistant *assistant, ofaExeClosing *self );
static void             p5_do_close( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget );
static gboolean         p5_validate_entries( ofaExeClosing *self );
static gboolean         p5_solde_accounts( ofaExeClosing *self );
static gboolean         p5_close_ledgers( ofaExeClosing *self );
static gboolean         p5_get_unsettled( ofaExeClosing *self );

static void
exe_closing_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exe_closing_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXE_CLOSING( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_closing_parent_class )->finalize( instance );
}

static void
exe_closing_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_EXE_CLOSING( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exe_closing_parent_class )->dispose( instance );
}

static void
ofa_exe_closing_init( ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXE_CLOSING, ofaExeClosingPrivate );
}

static void
ofa_exe_closing_class_init( ofaExeClosingClass *klass )
{
	static const gchar *thisfn = "ofa_exe_closing_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exe_closing_dispose;
	G_OBJECT_CLASS( klass )->finalize = exe_closing_finalize;

	g_type_class_add_private( klass, sizeof( ofaExeClosingPrivate ));
}

/**
 * ofa_exe_closing_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected ledgers
 */
void
ofa_exe_closing_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_exe_closing_run";
	ofaExeClosing *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_EXE_CLOSING,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	g_signal_connect(
			G_OBJECT( self ), MY_SIGNAL_PAGE_FORWARD, G_CALLBACK( on_page_forward ), NULL );

	my_assistant_signal_connect( MY_ASSISTANT( self ), "prepare", G_CALLBACK( on_prepare ));
	my_assistant_signal_connect( MY_ASSISTANT( self ), "apply", G_CALLBACK( on_apply ));

	my_assistant_run( MY_ASSISTANT( self ));
}

static void
on_prepare( GtkAssistant *assistant, GtkWidget *page_widget, ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_on_prepare";
	gint page_num;

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( page_widget && GTK_IS_WIDGET( page_widget ));
	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));

	page_num = gtk_assistant_get_current_page( assistant );

	g_debug( "%s: assistant=%p, page_widget=%p, page_num=%d, self=%p",
			thisfn, ( void * ) assistant, ( void * ) page_widget, page_num, ( void * ) self );

	switch( page_num ){
		/* page_num=0 / p1 [Intro] Introduction */
		case PAGE_INTRO:
			break;

		/* page_num=1 / p2  [Content] Enter parms */
		case PAGE_PARMS:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page_widget )){
				p2_do_init( self, assistant, page_widget );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page_widget, TRUE );
			}
			p2_display( self, assistant, page_widget );
			break;

		/* page_num=2 / p3  [Content] Check books */
		case PAGE_CHECKS:
			if( !my_assistant_is_page_initialized( MY_ASSISTANT( self ), page_widget )){
				p3_do_init( self, assistant, page_widget );
				my_assistant_set_page_initialized( MY_ASSISTANT( self ), page_widget, TRUE );
			}
			p3_checks( self, assistant, page_widget );
			break;

		/* page_num=3 / p4  [Confirm] confirm closing ope */
		case PAGE_CONFIRM:
			break;

		/* page_num=4 / p5  [Summary] Close the exercice and print the result */
		case PAGE_SUMMARY:
			p5_do_close( self, assistant, page_widget );
			break;
	}
}

static void
on_page_forward( ofaExeClosing *self, GtkWidget *page_widget, gint page_num, void *empty )
{
	static const gchar *thisfn = "ofa_exe_closing_on_page_forward";

	g_return_if_fail( self && OFA_IS_EXE_CLOSING( self ));
	g_return_if_fail( page_widget && GTK_IS_WIDGET( page_widget ));

	g_debug( "%s: self=%p, page_widget=%p, page_num=%d, empty=%p",
			thisfn, ( void * ) self, ( void * ) page_widget, page_num, ( void * ) empty );

	switch( page_num ){
		/* 1 [Content] Enter parms */
		case PAGE_PARMS:
			p2_do_forward( self, page_widget );
			break;
	}
}

static void
p2_do_init( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{
	ofaExeClosingPrivate *priv;
	GtkWidget *parent;
	ofoDossier *dossier;
	const GDate *begin_cur, *end_cur;
	GDate begin, end;
	gint exe_length;

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;
	exe_length = ofo_dossier_get_exe_length( dossier );

	priv->p2_begin_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-closing-begin" );
	g_return_if_fail( priv->p2_begin_cur && GTK_IS_ENTRY( priv->p2_begin_cur ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_begin_cur ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_begin_cur ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_begin_cur ), TRUE );
	begin_cur = ofo_dossier_get_exe_begin( dossier );
	my_editable_date_set_date( GTK_EDITABLE( priv->p2_begin_cur ), begin_cur );
	g_signal_connect( G_OBJECT( priv->p2_begin_cur ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	priv->p2_end_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-closing-end" );
	g_return_if_fail( priv->p2_end_cur && GTK_IS_ENTRY( priv->p2_end_cur ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_end_cur ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_end_cur ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_end_cur ), TRUE );
	end_cur = ofo_dossier_get_exe_end( dossier );
	my_editable_date_set_date( GTK_EDITABLE( priv->p2_end_cur ), end_cur );
	g_signal_connect( G_OBJECT( priv->p2_end_cur ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	/* set a date if the other is valid */
	if( !my_date_is_valid( begin_cur ) && my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &begin, end_cur );
		g_date_subtract_months( &begin, exe_length );
		g_date_add_days( &begin, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_begin_cur ), &begin );

	} else if( my_date_is_valid( begin_cur ) && !my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &end, begin_cur );
		g_date_add_months( &end, exe_length );
		g_date_subtract_days( &end, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_end_cur ), &end );
	}

	priv->p2_begin_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-next-begin" );
	g_return_if_fail( priv->p2_begin_next && GTK_IS_ENTRY( priv->p2_begin_next ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_begin_next ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_begin_next ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_begin_next ), TRUE );
	g_signal_connect( G_OBJECT( priv->p2_begin_next ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	if( my_date_is_valid( end_cur )){
		my_date_set_from_date( &begin, end_cur );
		g_date_add_days( &begin, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_begin_next ), &begin );
	}

	priv->p2_end_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-next-end" );
	g_return_if_fail( priv->p2_end_next && GTK_IS_ENTRY( priv->p2_end_next ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_end_next ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_end_next ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_end_next ), TRUE );
	g_signal_connect( G_OBJECT( priv->p2_end_next ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	if( my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &end, end_cur );
		g_date_add_months( &end, exe_length );
		g_date_subtract_days( &end, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_end_next ), &end );
	}

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p2_forward = ofa_exe_forward_piece_new();
	ofa_exe_forward_piece_attach_to(
			priv->p2_forward, GTK_CONTAINER( parent ));
	ofa_exe_forward_piece_set_main_window(
			priv->p2_forward, MY_WINDOW( self )->prot->main_window );
	g_signal_connect( G_OBJECT( priv->p2_forward ), "changed", G_CALLBACK( p2_on_forward_changed ), self );

	gtk_assistant_set_page_complete( assistant, page_widget, FALSE );
}

/*
 * check if the page is validable
 */
static void
p2_display( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{
	p2_check_for_complete( self );
}

/*
 * try to set some default values
 */
static void
p2_on_date_changed( GtkEditable *editable, ofaExeClosing *self )
{
	p2_check_for_complete( self );
}

static void
p2_on_forward_changed( ofaExeForwardPiece *piece, ofaExeClosing *self )
{
	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaExeClosing *self )
{
	ofaExeClosingPrivate *priv;
	GtkAssistant *assistant;
	gint page_num;
	GtkWidget *page_widget;
	gboolean complete;
	const GDate *begin_cur, *end_cur, *begin_next, *end_next;
	GDate date;
	gchar *msg;

	priv = self->priv;

	assistant = my_assistant_get_assistant( MY_ASSISTANT( self ));
	page_num = gtk_assistant_get_current_page( assistant );
	page_widget = gtk_assistant_get_nth_page( assistant, page_num );

	complete = FALSE;

	if( priv->p2_end_next ){
		begin_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_cur ), NULL );
		end_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_cur ), NULL );
		begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_next ), NULL );
		end_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_next ), NULL );

		/* check that all dates are valid
		 * and next exercice begins the next day after the end of the
		 * current */
		if( my_date_is_valid( begin_cur ) &&
				my_date_is_valid( end_cur ) &&
				my_date_is_valid( begin_next ) &&
				my_date_is_valid( end_next ) &&
				my_date_compare( begin_cur, end_cur ) < 0 &&
				my_date_compare( begin_next, end_next ) < 0 ){

			my_date_set_from_date( &date, end_cur );
			g_date_add_days( &date, 1 );
			if( my_date_compare( &date, begin_next ) == 0 ){
				complete = TRUE;
			}
		}
	}

	if( priv->p2_forward ){
		complete &= ofa_exe_forward_piece_check( priv->p2_forward, &msg );
		g_free( msg );
	}

	gtk_assistant_set_page_complete( assistant, page_widget, complete );
}

/*
 * as all parameters have been checked ok, save in dossier
 */
static void
p2_do_forward( ofaExeClosing *self, GtkWidget *page_widget )
{
	ofaExeClosingPrivate *priv;
	const GDate *begin_cur, *end_cur;
	ofoDossier *dossier;

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;

	begin_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_cur ), NULL );
	end_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_cur ), NULL );

	ofo_dossier_set_exe_begin( dossier, begin_cur );
	ofo_dossier_set_exe_end( dossier, end_cur );

	ofa_exe_forward_piece_apply( priv->p2_forward );

	ofo_dossier_update( dossier );
}

static void
p3_do_init( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{
	ofaExeClosingPrivate *priv;

	priv = self->priv;

	priv->p3_done = FALSE;
}

/*
 * begins the checks before exercice closing
 */
static void
p3_checks( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{
	ofaExeClosingPrivate *priv;
	gint page_num;

	priv = self->priv;

	gtk_assistant_set_page_complete( assistant, page_widget, priv->p3_done );

	if( !priv->p3_done ){

		page_num = gtk_assistant_get_current_page( assistant );

		priv->assistant = assistant;
		priv->page_widget = gtk_assistant_get_nth_page( assistant, page_num );
		priv->p3_entries_ok = FALSE;
		priv->p3_ledgers_ok = FALSE;
		priv->p3_accounts_ok = FALSE;

		g_idle_add(( GSourceFunc ) p3_check_entries_balance, self );
	}
}

/*
 * 1/ check that entries are balanced per currency
 */
static gboolean
p3_check_entries_balance( ofaExeClosing *self )
{
	ofaExeClosingPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;

	priv = self->priv;

	bar = get_new_bar( self, "p3-entry-parent" );
	grid = p3_get_new_balances( self, "p3-entry-bals" );
	priv->p3_entries_ok = ofa_misc_chkbalent_run(
			MY_WINDOW( self )->prot->dossier, &priv->p3_entries_list, bar, grid );
	p3_check_status( self, priv->p3_entries_ok, "p3-entry-ok" );

	/* next: check for ledgers balances */
	g_idle_add(( GSourceFunc ) p3_check_ledgers_balance, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * 2/ check that ledgers are balanced per currency
 */
static gboolean
p3_check_ledgers_balance( ofaExeClosing *self )
{
	ofaExeClosingPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;

	priv = self->priv;

	bar = get_new_bar( self, "p3-ledger-parent" );
	grid = p3_get_new_balances( self, "p3-ledger-bals" );
	priv->p3_ledgers_ok = ofa_misc_chkballed_run(
			MY_WINDOW( self )->prot->dossier, &priv->p3_ledgers_list, bar, grid );
	p3_check_status( self, priv->p3_ledgers_ok, "p3-ledger-ok" );

	/* next: check for accounts balances */
	g_idle_add(( GSourceFunc ) p3_check_accounts_balance, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * 3/ check that accounts are balanced per currency
 */
static gboolean
p3_check_accounts_balance( ofaExeClosing *self )
{
	ofaExeClosingPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;
	gboolean complete;

	priv = self->priv;

	bar = get_new_bar( self, "p3-account-parent" );
	grid = p3_get_new_balances( self, "p3-account-bals" );
	priv->p3_accounts_ok = ofa_misc_chkbalacc_run(
			MY_WINDOW( self )->prot->dossier, &priv->p3_accounts_list, bar, grid );
	p3_check_status( self, priv->p3_accounts_ok, "p3-account-ok" );

	/* next: if all checks complete and ok ?
	 * set priv->p3_done */
	complete = p3_info_checks( self );
	gtk_assistant_set_page_complete( priv->assistant, priv->page_widget, complete );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

static myProgressBar *
get_new_bar( ofaExeClosing *self, const gchar *w_name )
{
	ofaExeClosingPrivate *priv;
	GtkWidget *parent;
	myProgressBar *bar;

	priv = self->priv;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->assistant ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );
	bar = my_progress_bar_new();
	my_progress_bar_attach_to( bar, GTK_CONTAINER( parent ));

	return( bar );
}

static ofaBalancesGrid *
p3_get_new_balances( ofaExeClosing *self, const gchar *w_name )
{
	ofaExeClosingPrivate *priv;
	GtkWidget *parent;
	ofaBalancesGrid *grid;

	priv = self->priv;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->assistant ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );
	grid = ofa_balances_grid_new();
	ofa_balances_grid_attach_to( grid, GTK_CONTAINER( parent ));

	return( grid );
}

/*
 * display OK/NOT OK for a single balance check
 */
static void
p3_check_status( ofaExeClosing *self, gboolean ok, const gchar *w_name )
{
	ofaExeClosingPrivate *priv;
	GtkWidget *label;
	GdkRGBA color;

	priv = self->priv;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->assistant ), w_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	gdk_rgba_parse( &color, ok ? "#000000" : "#ff0000" );

	if( ok ){
		gtk_label_set_text( GTK_LABEL( label ), _( "OK" ));

	} else {
		gtk_label_set_text( GTK_LABEL( label ), _( "NOT OK" ));
	}

	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
}

/*
 * after the end of individual checks (entries, ledgers, accounts)
 * check that the balances are the sames
 */
static gboolean
p3_info_checks( ofaExeClosing *self )
{
	ofaExeClosingPrivate *priv;
	gboolean ok;
	GtkWidget *dialog, *label;

	priv = self->priv;

	ok = priv->p3_entries_ok && priv->p3_ledgers_ok && priv->p3_accounts_ok;
	priv->p3_done = TRUE;

	if( !ok ){
		dialog = gtk_message_dialog_new(
				my_window_get_toplevel( MY_WINDOW( self )),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_CLOSE,
				"%s", _( "We have detected losses of balance in your books.\n\n"
						"In this current state, we are unable to close this exercice\n"
						"until you fix your balances." ));

		gtk_dialog_run( GTK_DIALOG( dialog ));
		gtk_widget_destroy( dialog );

	} else {
		ok = ofa_misc_chkbalsame_run(
				priv->p3_entries_list, priv->p3_ledgers_list, priv->p3_accounts_list );

		label = my_utils_container_get_child_by_name(
						GTK_CONTAINER( priv->page_widget ), "p3-label-end" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );

		if( ok ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your books are rightly balanced. Good !" ));

		} else {
			gtk_label_set_text( GTK_LABEL( label ),
					_( "\nThough each book is individually balanced, it appears "
						"that some distorsion has happended among them.\n"
						"In this current state, we are unable to close this exercice "
						"until you fix your balances." ));
		}
	}

	ofa_misc_chkbal_free( priv->p3_entries_list );
	priv->p3_entries_list = NULL;

	ofa_misc_chkbal_free( priv->p3_ledgers_list );
	priv->p3_ledgers_list = NULL;

	ofa_misc_chkbal_free( priv->p3_accounts_list );
	priv->p3_accounts_list = NULL;

	ok = TRUE;
	return( ok );
}

static void
on_apply( GtkAssistant *assistant, ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_on_apply";

	g_debug( "%s: assistant=%p, self=%p", thisfn, ( void * ) assistant, ( void * ) self );
}

static void
p5_do_close( ofaExeClosing *self, GtkAssistant *assistant, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exe_closing_p5_do_close";

	g_debug( "%s: self=%p, assistant=%p, page_widget=%p",
			thisfn, ( void * ) self, ( void * ) assistant, ( void * ) page_widget );

	gtk_assistant_set_page_complete( assistant, page_widget, FALSE );

	g_idle_add(( GSourceFunc ) p5_validate_entries, self );
}

/*
 * validate rough entries remaining in the exercice
 */
static gboolean
p5_validate_entries( ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_p5_validate_entries";
	ofoDossier *dossier;
	GList *entries, *it;
	myProgressBar *bar;
	gint count, i;
	gdouble progress;
	gchar *text;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	dossier = MY_WINDOW( self )->prot->dossier;
	entries = ofo_entry_get_dataset_remaining_for_val( dossier );
	count = g_list_length( entries );
	bar = get_new_bar( self, "p5-validating" );

	for( i=1, it=entries ; it ; ++i, it=it->next ){
		ofo_entry_validate( OFO_ENTRY( it->data ), dossier );

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "text", text );
		g_free( text );
	}
	if( !entries ){
		g_signal_emit_by_name( bar, "double", 1.0 );
		g_signal_emit_by_name( bar, "text", "0/0" );
	}

	ofo_entry_free_dataset( entries );

	/* do not continue and remove from idle callbacks list */
	g_idle_add(( GSourceFunc ) p5_solde_accounts, self );
	return( G_SOURCE_REMOVE );
}

/*
 * balance the detail accounts -for validated soldes only
 *
 * It shouldn't remain any amount on day soldes, but we do not take
 * care of that here.
 */
static gboolean
p5_solde_accounts( ofaExeClosing *self )
{
#if 0
	static const gchar *thisfn = "ofa_exe_closing_p5_solde_accounts";
	ofaExeClosingPrivate *priv;
	ofoDossier *dossier;
	GList *accounts, *it;
	myProgressBar *bar;
	gint count, i;
	gdouble progress;
	gchar *text;
	ofoAccount *account;
	const gchar *currency;
	/*const gchar *sld_ope, *sld_account;
	const gchar *for_ope;*/
	ofxAmount debit, credit;
	ofoEntry *entry;
	const GDate *end_cur, *begin_next;
	gboolean is_ran;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;
	accounts = ofo_account_get_dataset( dossier );
	count = g_list_length( accounts );
	bar = get_new_bar( self, "p5-balancing" );

	priv->p5_forwards = NULL;

	/*end_cur = ofo_dossier_get_exe_end( dossier );
	begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_next ), NULL );
	sld_ope = ofo_dossier_get_sld_ope( dossier );
	for_ope = ofo_dossier_get_forward_ope( dossier );*/

	for( i=1, it=accounts ; it ; ++i, it=it->next ){
		account = OFO_ACCOUNT( it->data );

		if( !ofo_account_is_root( account )){
			currency = ofo_account_get_currency( account );
			/*sld_account = ofo_dossier_get_sld_account( dossier, currency );*/
			debit = ofo_account_get_val_debit( account );
			credit = ofo_account_get_val_credit( account );
			is_ran = ofo_account_is_forward( account );
			if(( debit || credit ) && debit != credit ){

				/* balance the account */
				/*
				entry = ofo_entry_new();
				ofo_entry_set_deffect( entry, end_cur );
				ofo_entry_set_dope( entry, end_cur );
				ofo_entry_set_label( entry, is_ran ? for_label_c : sld_label );
				ofo_entry_set_ref( entry, sld_account );
				ofo_entry_set_account( entry, ofo_account_get_number( account ));
				ofo_entry_set_currency( entry, currency );
				ofo_entry_set_ledger( entry, is_ran ? for_ledger : sld_ledger );
				ofo_entry_set_ope_template( entry, is_ran ? for_ope : sld_ope );
				if( debit > credit ){
					ofo_entry_set_credit( entry, debit-credit );
				} else {
					ofo_entry_set_debit( entry, credit-debit );
				}
				ofo_entry_set_status( entry, ENT_STATUS_ROUGH );
				ofo_entry_insert( entry, dossier );
				g_object_unref( entry );*/

				/* create the entry on the balancing account */
				/*
				entry = ofo_entry_new();
				ofo_entry_set_deffect( entry, end_cur );
				ofo_entry_set_dope( entry, end_cur );
				ofo_entry_set_label( entry, is_ran ? for_label_c : sld_label );
				ofo_entry_set_ref( entry, ofo_account_get_number( account ));
				ofo_entry_set_account( entry, sld_account );
				ofo_entry_set_currency( entry, currency );
				ofo_entry_set_ledger( entry, is_ran ? for_ledger : sld_ledger );
				ofo_entry_set_ope_template( entry, is_ran ? for_ope : sld_ope );
				if( debit > credit ){
					ofo_entry_set_debit( entry, debit-credit );
				} else {
					ofo_entry_set_credit( entry, credit-debit );
				}
				ofo_entry_set_status( entry, ENT_STATUS_ROUGH );
				ofo_entry_insert( entry, dossier );
				g_object_unref( entry );*/

				if( is_ran ){
					/* carried forward entry */
					/*
					entry = ofo_entry_new();
					ofo_entry_set_deffect( entry, begin_next );
					ofo_entry_set_dope( entry, begin_next );
					ofo_entry_set_label( entry, for_label_o );
					ofo_entry_set_ref( entry, sld_account );
					ofo_entry_set_account( entry, ofo_account_get_number( account ));
					ofo_entry_set_currency( entry, currency );
					ofo_entry_set_ledger( entry, for_ledger );
					ofo_entry_set_ope_template( entry, for_ope );
					if( debit > credit ){
						ofo_entry_set_debit( entry, debit-credit );
					} else {
						ofo_entry_set_credit( entry, credit-debit );
					}
					ofo_entry_set_status( entry, ENT_STATUS_ROUGH );
					priv->p5_forwards = g_list_prepend( priv->p5_forwards, entry );
					*/

					/* create the entry on the balancing account */
					/*
					entry = ofo_entry_new();
					ofo_entry_set_deffect( entry, begin_next );
					ofo_entry_set_dope( entry, begin_next );
					ofo_entry_set_label( entry, for_label_o );
					ofo_entry_set_ref( entry, ofo_account_get_number( account ));
					ofo_entry_set_account( entry, sld_account );
					ofo_entry_set_currency( entry, currency );
					ofo_entry_set_ledger( entry, for_ledger );
					ofo_entry_set_ope_template( entry, for_ope );
					if( debit > credit ){
						ofo_entry_set_credit( entry, debit-credit );
					} else {
						ofo_entry_set_debit( entry, credit-debit );
					}
					ofo_entry_set_status( entry, ENT_STATUS_ROUGH );
					priv->p5_forwards = g_list_prepend( priv->p5_forwards, entry );
					*/
				}
			}
		}

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "text", text );
		g_free( text );
	}
#endif

	/* do not continue and remove from idle callbacks list */
	g_idle_add(( GSourceFunc ) p5_close_ledgers, self );
	return( FALSE );
}

/*
 * close all the ledgers
 */
static gboolean
p5_close_ledgers( ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_p5_close_ledgers";
	ofoDossier *dossier;
	GList *ledgers, *it;
	myProgressBar *bar;
	gint count, i;
	gdouble progress;
	gchar *text;
	const GDate *end_cur;
	ofoLedger *ledger;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	dossier = MY_WINDOW( self )->prot->dossier;
	ledgers = ofo_ledger_get_dataset( dossier );
	count = g_list_length( ledgers );
	bar = get_new_bar( self, "p5-ledgers" );

	end_cur = ofo_dossier_get_exe_end( dossier );

	for( i=1, it=ledgers ; it ; ++i, it=it->next ){
		ledger = OFO_LEDGER( it->data );

		ofo_ledger_close( ledger, dossier, end_cur );

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "text", text );
		g_free( text );
	}

	/* do not continue and remove from idle callbacks list */
	g_idle_add(( GSourceFunc ) p5_get_unsettled, self );
	return( FALSE );
}

/*
 * get unsettled and reconciliated entries
 */
static gboolean
p5_get_unsettled( ofaExeClosing *self )
{
	static const gchar *thisfn = "ofa_exe_closing_p5_get_unsettled";
	ofaExeClosingPrivate *priv;
	ofoDossier *dossier;
	myProgressBar *bar;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;
	bar = get_new_bar( self, "p5-unsettled" );

	priv->p5_unsettled = ofo_entry_get_unsettled( dossier );
	g_debug( "%s: unsettled_count=%d", thisfn, g_list_length( priv->p5_unsettled ));

	g_signal_emit_by_name( bar, "double", 0.5 );
	g_signal_emit_by_name( bar, "text", "1/2" );

	priv->p5_unreconciliated = ofo_entry_get_unreconciliated( dossier );
	g_debug( "%s: unreconciliated_count=%d", thisfn, g_list_length( priv->p5_unreconciliated ));

	g_signal_emit_by_name( bar, "double", 1.0 );
	g_signal_emit_by_name( bar, "text", "2/2" );

	/* do not continue and remove from idle callbacks list */
	/*g_idle_add(( GSourceFunc ) p5_unsettled, self );*/
	return( FALSE );
}
