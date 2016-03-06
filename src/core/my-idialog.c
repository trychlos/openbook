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

#define IDIALOG_LAST_VERSION            1

static guint    st_initializations      = 0;		/* interface initialization count */

static GType     register_type( void );
static void      interface_base_init( myIDialogInterface *klass );
static void      interface_base_finalize( myIDialogInterface *klass );
static void      idialog_init_dialog( myIDialog *instance );
static void      on_cancel_clicked( GtkButton *button, myIDialog *instance );
static void      on_close_clicked( GtkButton *button, myIDialog *instance );
static void      on_ok_clicked( GtkButton *button, myIDialog *instance );
static void      do_close( myIDialog *instance );

/**
 * my_idialog_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_idialog_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_idialog_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_idialog_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIDialogInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "myIDialog", &info, 0 );

	g_type_interface_add_prerequisite( type, MY_TYPE_IWINDOW );
	g_type_interface_add_prerequisite( type, GTK_TYPE_DIALOG );

	return( type );
}

static void
interface_base_init( myIDialogInterface *klass )
{
	static const gchar *thisfn = "my_idialog_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIDialogInterface *klass )
{
	static const gchar *thisfn = "my_idialog_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_idialog_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_idialog_get_interface_last_version( void )
{
	return( IDIALOG_LAST_VERSION );
}

/**
 * my_idialog_get_interface_version:
 * @instance: this #myIDialog instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
my_idialog_get_interface_version( const myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && MY_IS_IDIALOG( instance ), 0 );

	if( MY_IDIALOG_GET_INTERFACE( instance )->get_interface_version ){
		return( MY_IDIALOG_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: myIDialog instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * my_idialog_set_close_button:
 * @instance: this #myIDialog instance.
 *
 * Replace the [OK] / [Cancel] buttons with a [Close] one which has a
 * GTK_RESPONSE_CLOSE response identifier.
 *
 * This method should only be called for GtkDialog classes.
 *
 * Returns: the newly added 'Close' button.
 */
GtkWidget *
my_idialog_set_close_button( myIDialog *instance )
{
	GtkWidget *button;

	g_return_val_if_fail( instance && MY_IS_IDIALOG( instance ), NULL );
	g_return_val_if_fail( GTK_IS_DIALOG( instance ), NULL );

	if( 0 ){
		idialog_init_dialog( instance );
	}

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_OK );
	if( button ){
		gtk_widget_destroy( button );
	}

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_CANCEL );
	if( button ){
		gtk_widget_destroy( button );
	}

	button = gtk_dialog_add_button( GTK_DIALOG( instance ), _( "Close" ), GTK_RESPONSE_CLOSE );

	gtk_widget_show_all( GTK_WIDGET( instance ));

	return( button );
}

/*
 * idialog_init_dialog:

 * Specific GtkDialog-derived one-time initialization.
 *
 * Response codes are defined in /usr/include/gtk-3.0/gtk/gtkdialog.h.
 */
static void
idialog_init_dialog( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_init_dialog";
	GtkWidget *cancel_btn, *close_btn, *ok_btn;

	g_return_if_fail( instance && GTK_IS_DIALOG( instance ));

	cancel_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_CANCEL );
	if( cancel_btn ){
		g_return_if_fail( GTK_IS_BUTTON( cancel_btn ));
		g_signal_connect( cancel_btn, "clicked", G_CALLBACK( on_cancel_clicked ), instance );
	} else {
		g_debug( "%s: unable to identify the [Cancel] button", thisfn );
	}

	close_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_CLOSE );
	if( close_btn ){
		g_return_if_fail( GTK_IS_BUTTON( close_btn ));
		g_signal_connect( close_btn, "clicked", G_CALLBACK( on_close_clicked ), instance );
	} else {
		g_debug( "%s: unable to identify the [Close] button", thisfn );
	}

	ok_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_OK );
	if( ok_btn ){
		g_return_if_fail( GTK_IS_BUTTON( ok_btn ));
		g_signal_connect( ok_btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	} else {
		g_debug( "%s: unable to identify the [OK] button", thisfn );
	}
}

/*
 * GtkDialog-specific.
 *
 * click on [Cancel] button: close without confirmation
 */
static void
on_cancel_clicked( GtkButton *button, myIDialog *instance )
{
	do_close( instance );
}

/*
 * GtkDialog-specific.
 *
 * click on [Close] button: close without confirmation
 */
static void
on_close_clicked( GtkButton *button, myIDialog *instance )
{
	do_close( instance );
}

/*
 * GtkDialog-specific.
 *
 * click on [OK] button: does nothing ?
 */
static void
on_ok_clicked( GtkButton *button, myIDialog *instance )
{

}

/*
 * this closes the GtkWindow without any user confirmation
 */
static void
do_close( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_do_close";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	my_iwindow_close( MY_IWINDOW( instance ));
}
