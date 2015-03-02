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
 *
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * How to have two simultaneously active dialogs ??
 *
 * All happens as if they was not possible to have a modal dialog
 * coexist with a non-modal dialog ?
 * Two non-modal dialogs work fine however...
 */
#include <gtk/gtk.h>

static GtkWidget *
create_dialog2( GtkWidget *parent )
{
	GtkWidget *dialog, *content, *label, *entry;

	dialog = gtk_dialog_new_with_buttons(
			"Openbook [Test] help dialog",
			GTK_WINDOW( parent ),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			"Close", GTK_RESPONSE_CLOSE,
			NULL );

	content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));

	label = gtk_label_new( "Label : " );
	gtk_container_add( GTK_CONTAINER( content ), label );

	entry = gtk_entry_new();
	gtk_container_add( GTK_CONTAINER( content ), entry );

	gtk_widget_show_all( dialog );

	/* with or without gtk_main() call, the non-modal dialog doesn't
	 * receive user interaction */
	gtk_main();

	return( dialog );
}

static void
on_help_clicked( GtkButton *button, GtkWidget *parent )
{
	GtkWidget *dialog;

	dialog = create_dialog2( parent );
	g_debug( "dialog=%p created", ( void * ) dialog );
}

static GtkWidget *
create_dialog1( GtkWindow *window )
{
	GtkWidget *dialog, *content, *label, *entry, *button;

	dialog = gtk_dialog_new_with_buttons(
			"Openbook [Test] parent dialog",
			window,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			"Cancel", GTK_RESPONSE_CANCEL,
			"OK", GTK_RESPONSE_OK,
			"Help", 1,
			NULL );

	content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));

	label = gtk_label_new( "Label : " );
	gtk_container_add( GTK_CONTAINER( content ), label );

	entry = gtk_entry_new();
	gtk_container_add( GTK_CONTAINER( content ), entry );

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( dialog ), 1 );
	g_signal_connect( button, "clicked", G_CALLBACK( on_help_clicked ), window );

	gtk_widget_show_all( dialog );

	return( dialog );
}

int
main( int argc, char *argv[] )
{
	GtkWindow *window;
	GtkWidget *dialog;

	gtk_init( &argc, &argv );

	window = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ));
	gtk_window_set_title( window, "Openbook [Test] non-modal dialogs" );
	gtk_window_set_default_size( window, 600, 400 );
	g_signal_connect_swapped( G_OBJECT( window ), "destroy", G_CALLBACK( gtk_main_quit ), NULL );
	gtk_widget_show_all( GTK_WIDGET( window ));

	/* create a first dialog which is modal for the application
	 * this first dialog has a button which will open a second dialog
	 * which itself should be non modal */

	dialog = create_dialog1( window );
	g_debug( "dialog1=%p created", ( void * ) dialog );
	/*
	while( gtk_dialog_run( GTK_DIALOG( dialog )) != GTK_RESPONSE_OK ){
		;
	}*/
	gtk_main();

	return 0;
}
