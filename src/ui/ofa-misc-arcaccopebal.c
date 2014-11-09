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

#include "api/ofo-account.h"

#include "ui/ofa-main-window.h"
#include "ui/ofa-misc-arcaccopebal.h"

static gboolean is_confirmed( const ofaMainWindow *main_window );
static void     report_done( const ofaMainWindow *main_window );

/**
 * ofa_misc_arcaccopebal_run:
 * @main: the main window of the application.
 *
 * Ask for a user confirmation before archiving accounts balances
 * when opening the exercice.
 */
void
ofa_misc_arcaccopebal_run( const ofaMainWindow *main_window )
{
	if( is_confirmed( main_window )){
		ofo_account_archive_open_balances( ofa_main_window_get_dossier( main_window ));
		report_done( main_window );
	}
}

static gboolean
is_confirmed( const ofaMainWindow *main_window )
{
	GtkWidget *dialog;
	gint response;
	gchar *msg;

	msg = g_strdup( _(
			"You are about to archive the account current validated balances "
			"to the opening balances.\n"
			"This will have for effect to take these new opening balances "
			"as a start point when editing balances summaries for the exercice.\n"
			"Are you sure you want this ?" ));

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( main_window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", msg );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_OK" ), GTK_RESPONSE_OK,
			NULL );

	g_free( msg );
	response = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
report_done( const ofaMainWindow *main_window )
{
	GtkWidget *dialog;
	gchar *msg;

	msg = g_strdup( _(
			"The account current validated balances have been successfully "
			"archived to the opening balances." ));

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( main_window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", msg );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_OK" ), GTK_RESPONSE_OK,
			NULL );

	g_free( msg );
	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}
