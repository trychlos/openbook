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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"

#include "core/my-dialog.h"
#include "core/my-window-prot.h"

#include "ui/ofa-ope-template-help.h"

/* private instance data
 */
struct _ofaOpeTemplateHelpPrivate {

	/* input parameters at initialization time
	 */
	const ofaMainWindow *main_window;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-ope-template-help.ui";
static const gchar *st_ui_id            = "OpeTemplateHelpDialog";

static void init_window( ofaOpeTemplateHelp *help );
static void on_response( GtkDialog *dialog, gint response_id, ofaOpeTemplateHelp *help );

G_DEFINE_TYPE( ofaOpeTemplateHelp, ofa_ope_template_help, MY_TYPE_DIALOG )

static void
ope_template_help_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_help_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_HELP( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_help_parent_class )->finalize( instance );
}

static void
ope_template_help_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_HELP( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_help_parent_class )->dispose( instance );
}

static void
ofa_ope_template_help_init( ofaOpeTemplateHelp *self )
{
	static const gchar *thisfn = "ofa_ope_template_help_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_HELP( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelpPrivate );
}

static void
ofa_ope_template_help_class_init( ofaOpeTemplateHelpClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_help_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_help_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_help_finalize;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplateHelpPrivate ));
}

/**
 * ofa_ope_template_help_run:
 */
ofaOpeTemplateHelp *
ofa_ope_template_help_run( const ofaMainWindow *main_window )
{
	ofaOpeTemplateHelp *self;

	self = g_object_new(
					OFA_TYPE_OPE_TEMPLATE_HELP,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	init_window( self );

	return( self );
}

static void
init_window( ofaOpeTemplateHelp *help )
{
	GtkWindow *toplevel;
	GtkWidget *button;

	toplevel = my_window_get_toplevel( MY_WINDOW( help ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-close" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( toplevel, "response", G_CALLBACK( on_response ), help );

	gtk_widget_show_all( GTK_WIDGET( toplevel ));

	/*g_main_loop_new( NULL, TRUE );*/
	/*
	gtk_widget_grab_focus( GTK_WIDGET( toplevel ));
	gboolean grabbed = gtk_widget_has_grab( GTK_WIDGET( toplevel ));
	g_debug( "ofa_ope_template_help_init_window: grabbed=%s", grabbed ? "True":"False" );
	*/
	gtk_main();
}

/*
 * response codes available in /usr/include/gtk-3.0/gtk/gtkdialog.h
 */
static void
on_response( GtkDialog *dialog, gint response_id, ofaOpeTemplateHelp *help )
{
	g_object_unref( help );
}

/**
 * ofa_ope_template_help_run:
 */
void
ofa_ope_template_help_close( ofaOpeTemplateHelp *help )
{
	g_return_if_fail( help && OFA_IS_OPE_TEMPLATE_HELP( help ));

	if( !MY_WINDOW( help )->prot->dispose_has_run ){

		on_response( NULL, 0, help );
	}
}
