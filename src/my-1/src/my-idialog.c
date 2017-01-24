/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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

#define IDIALOG_LAST_VERSION            1
#define IDIALOG_DATA                    "my-idialog-data"

/* a data structure attached to each instance
 */
typedef struct {
	gboolean   initialized;
	GtkWidget *cancel_btn;
	GtkWidget *close_btn;
}
	sIDialog;

static guint    st_initializations      = 0;		/* interface initialization count */

static GType      register_type( void );
static void       interface_base_init( myIDialogInterface *klass );
static void       interface_base_finalize( myIDialogInterface *klass );
static void       idialog_init_application( myIDialog *instance );
static GtkWidget *button_connect( myIDialog *instance, const gchar *label, gint response_code, GCallback cb );
static void       on_cancel_clicked( GtkButton *button, myIDialog *instance );
static void       on_close_clicked( GtkButton *button, myIDialog *instance );
static void       do_close( myIDialog *instance );
static gboolean   ok_to_terminate( myIDialog *instance, gint response_code );
static gboolean   do_quit_on_ok( myIDialog *instance );
static gboolean   do_quit_on_code( myIDialog *instance, gint code );
static sIDialog  *get_instance_data( const myIDialog *instance );
static void       on_instance_finalized( sIDialog *sdata, GObject *finalized_idialog );

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

	g_type_interface_add_prerequisite( type, GTK_TYPE_DIALOG );
	/*
	An interface can have at most one instantiatable prerequisite type
	g_type_interface_add_prerequisite( type, MY_TYPE_IWINDOW );
	*/

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
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
my_idialog_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IDIALOG );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIDialogInterface * ) iface )->get_interface_version ){
		version = (( myIDialogInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIDialog::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_idialog_init:

 * Specific GtkDialog-derived one-time initialization.
 *
 * Response codes are defined in /usr/include/gtk-3.0/gtk/gtkdialog.h.
 *
 * Note that this method is mainly thought to be called by #myIWindow
 * interface. It should never be directly called by the application.
 * Rather call the #my_iwindow_init() method.
 */
void
my_idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_init";
	sIDialog *sdata;

	g_return_if_fail( instance && MY_IS_IDIALOG( instance ));
	g_return_if_fail( GTK_IS_DIALOG( instance ));
	g_return_if_fail( MY_IS_IWINDOW( instance ));

	sdata = get_instance_data( instance );

	if( !sdata->initialized ){
		g_debug( "%s: instance=%p", thisfn, ( void * ) instance );
		sdata->initialized = TRUE;

		/* must be called before having connected the cancel/close buttons
		 * so that the application has a chance to do something before
		 * default behavior */
		idialog_init_application( instance );

		sdata->cancel_btn = button_connect(
				instance, "Cancel", GTK_RESPONSE_CANCEL, G_CALLBACK( on_cancel_clicked ));

		sdata->close_btn = button_connect(
				instance, "Close", GTK_RESPONSE_CLOSE, G_CALLBACK( on_close_clicked ));

		gtk_widget_show_all( GTK_WIDGET( instance ));
	}
}

static void
idialog_init_application( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_init";

	if( MY_IDIALOG_GET_INTERFACE( instance )->init ){
		MY_IDIALOG_GET_INTERFACE( instance )->init( instance );
		return;
	}

	g_info( "%s: myIDialog's %s implementation does not provide 'init()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

static GtkWidget *
button_connect( myIDialog *instance, const gchar *label, gint response_code, GCallback cb )
{
	static const gchar *thisfn = "my_idialog_button_connect";
	GtkWidget *btn;

	btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), response_code );

	if( btn ){
		g_return_val_if_fail( GTK_IS_BUTTON( btn ), NULL );
		g_signal_connect( btn, "clicked", cb, instance );

	} else {
		g_debug( "%s: unable to identify the [%s] button", thisfn, label );
	}

	return( btn );
}

/**
 * my_idialog_set_close_button:
 * @instance: this #myIDialog instance.
 *
 * Replace the [OK] / [Cancel] buttons with a [Close] one which has a
 * GTK_RESPONSE_CLOSE response identifier.
 *
 * Returns: the newly added 'Close' button.
 */
GtkWidget *
my_idialog_set_close_button( myIDialog *instance )
{
	GtkWidget *button;

	g_return_val_if_fail( instance && MY_IS_IDIALOG( instance ), NULL );
	g_return_val_if_fail( GTK_IS_DIALOG( instance ), NULL );
	g_return_val_if_fail( MY_IS_IWINDOW( instance ), NULL );

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_OK );
	if( button ){
		gtk_widget_destroy( button );
	}

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_CANCEL );
	if( button ){
		gtk_widget_destroy( button );
	}

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( instance ), GTK_RESPONSE_CLOSE );
	if( !button ){
		button = gtk_dialog_add_button( GTK_DIALOG( instance ), _( "Close" ), GTK_RESPONSE_CLOSE );
		gtk_widget_show_all( button );
	}

	return( button );
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
 * this closes the GtkWindow without any user confirmation
 */
static void
do_close( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_do_close";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	my_iwindow_close( MY_IWINDOW( instance ));
}

/**
 * my_idialog_run:
 * @instance: this #myIDialog instance.
 *
 * Run as a modal dialog.
 *
 * Returns: the #GtkResponseCode of the dialog.
 */
gint
my_idialog_run( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_run";
	gint response_code;

	g_return_val_if_fail( instance && MY_IS_IDIALOG( instance ), 0 );
	g_return_val_if_fail( GTK_IS_DIALOG( instance ), 0 );
	g_return_val_if_fail( MY_IS_IWINDOW( instance ), 0 );

	my_iwindow_init( MY_IWINDOW( instance ));
	gtk_window_set_modal( GTK_WINDOW( instance ), TRUE );

	g_debug( "%s: calling gtk_dialog_run", thisfn );
	do {
		response_code = gtk_dialog_run( GTK_DIALOG( instance ));
		g_debug( "%s: gtk_dialog_run returns code=%d", thisfn, response_code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( instance, response_code ));

	return( response_code );
}

/**
 * my_idialog_run_maybe_modal:
 * @instance: this #myIDialog instance.
 *
 * Run as a modal or non-modal dialog depending of the parent:
 * - if parent has not been defined, it is expected to be the main window
 * - if parent is modal, run modal
 * - else (not modal or not defined), run non modal.
 *
 * Running a non modal dialog requires that the #my_idialog_click_to_update()
 * method be called at initialization time.
 */
void
my_idialog_run_maybe_modal( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_run_maybe_modal";
	GtkWindow *parent;

	g_return_if_fail( instance && MY_IS_IDIALOG( instance ));
	g_return_if_fail( GTK_IS_DIALOG( instance ));
	g_return_if_fail( MY_IS_IWINDOW( instance ));

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	parent = my_iwindow_get_parent( MY_IWINDOW( instance ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	if( parent && gtk_window_get_modal( GTK_WINDOW( parent ))){
		my_idialog_run( instance );

	} else {
		/* after this call, @instance may be invalid */
		my_iwindow_present( MY_IWINDOW( instance ));
	}
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( myIDialog *instance, gint response_code )
{
	gboolean quit = FALSE;

	/* if the user has set #my_idialog_click_to_update() method,
	 * then this dialog maybe already destroyed and finalized */
	if( !MY_IS_IDIALOG( instance )){
		return( TRUE );
	}

	switch( response_code ){
		case GTK_RESPONSE_DELETE_EVENT:
			quit = TRUE;
			break;
		case GTK_RESPONSE_CLOSE:
			quit = TRUE;
			break;
		case GTK_RESPONSE_CANCEL:
			quit = TRUE;
			break;
		case GTK_RESPONSE_OK:
			quit = do_quit_on_ok( instance );
			break;
		default:
			quit = do_quit_on_code( instance, response_code );
			break;
	}

	return( quit );
}

static gboolean
do_quit_on_ok( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_do_quit_on_ok";

	if( MY_IDIALOG_GET_INTERFACE( instance )->quit_on_ok ){
		return( MY_IDIALOG_GET_INTERFACE( instance )->quit_on_ok( instance ));
	}

	g_info( "%s: myIDialog's %s implementation does not provide 'quit_on_ok()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( TRUE );
}

static gboolean
do_quit_on_code( myIDialog *instance, gint code )
{
	static const gchar *thisfn = "my_idialog_do_quit_on_code";

	if( MY_IDIALOG_GET_INTERFACE( instance )->quit_on_code ){
		return( MY_IDIALOG_GET_INTERFACE( instance )->quit_on_code( instance, code ));
	}

	g_info( "%s: myIDialog's %s implementation does not provide 'quit_on_code()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

static sIDialog *
get_instance_data( const myIDialog *instance )
{
	sIDialog *sdata;

	sdata = ( sIDialog * ) g_object_get_data( G_OBJECT( instance ), IDIALOG_DATA );

	if( !sdata ){
		sdata = g_new0( sIDialog, 1 );
		g_object_set_data( G_OBJECT( instance ), IDIALOG_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );

		sdata->initialized = FALSE;
	}

	return( sdata );
}

static void
on_instance_finalized( sIDialog *sdata, GObject *finalized_idialog )
{
	static const gchar *thisfn = "my_idialog_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_idialog=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_idialog );

	g_free( sdata );
}
