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
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

/*
 * Both accounts book and operation templates are built the same way:
 * - a page which contains:
 *   - a GtkFrame with or without buttons, which contains:
 *     - a GtkNotebook, which contains:
 *       - one dynamically created page for each member (or group)
 *         of the input dataset
 *
 * Both make the same message be displayed:
 * Gtk-WARNING **: GtkNotebook 0x28af510 is mapped but visible child GtkLabel 0x259c990 is not mapped
 */
#include <gtk/gtk.h>

/* the first characters will be the page title, others the line content */
static const gchar *st_lines[] = {
		"aLes mathématiques sont beauté, esthétique de l'absolu,",
		"bdu zéro et de l'infini. Elles définissent la pensée, posent",
		"cdes lois. Elles participent de la vérité. La physique et la",
		"achimie proposent des lectures de la nature, depuis les",
		"bstructures de l'infiniment petit jusqu'aux espaces",
		"cgalactiques. De l'explosion du Big Bang au magma",
		NULL
};

static GtkWidget *
create_page( void )
{
	GtkWidget *grid;

	grid = gtk_grid_new();

	return( grid );
}

static GtkWidget *
find_page_for_title( GtkNotebook *book, const gchar *line )
{
	GtkWidget *page_w, *label;
	gint i, pages_count;
	const gchar *label_text;
	gchar title[11];

	pages_count = gtk_notebook_get_n_pages( book );
	memset( title, '\0', sizeof( title ));
	g_utf8_strncpy( title, line, 1 );
	for( i=0 ; i<pages_count ; ++i ){
		page_w = gtk_notebook_get_nth_page( book, i );
		label_text = gtk_notebook_get_tab_label_text( book, page_w );
		if( !g_utf8_collate( label_text, title )){
			return( page_w );
		}
	}

	/* not found ? then create it */
	page_w = create_page();
	label = gtk_label_new( title );
	gtk_notebook_append_page( book, page_w, label );

	return( page_w );
}

static void
add_line_to_page( GtkWidget *page, const gchar *line )
{
	GtkWidget *label;

	label = gtk_label_new( line );
	gtk_grid_insert_row( GTK_GRID( page ), 0 );
	gtk_grid_attach( GTK_GRID( page ), label, 0, 0, 1, 1 );
}

int
main( int argc, char *argv[] )
{
	GtkWindow *window;
	GtkWidget *book, *page_w;
	gint i;

	gtk_init( &argc, &argv );

	window = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ));
	gtk_window_set_title( window, "Openbook [Test] Dynamic notebook" );
	gtk_window_set_default_size( window, 600, 400 );
	g_signal_connect_swapped( G_OBJECT( window ), "destroy", G_CALLBACK( gtk_main_quit ), NULL );
	gtk_widget_show_all( GTK_WIDGET( window ));

	/* Creates a notebook */
	book = gtk_notebook_new();
	gtk_container_add( GTK_CONTAINER( window ), book );

	for( i=0 ; st_lines[i] ; ++i ){
		page_w = find_page_for_title( GTK_NOTEBOOK( book ), st_lines[i] );
		add_line_to_page( page_w, st_lines[i] );
	}

	gtk_widget_show_all( GTK_WIDGET( window ));
	gtk_main();

	return 0;
}
