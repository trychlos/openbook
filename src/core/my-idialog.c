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

#include "api/my-idialog.h"
#include "api/my-utils.h"

#define IDIALOG_LAST_VERSION            1
#define IDIALOG_DATA                    "my-idialog-data"

static guint  st_initializations        = 0;	/* interface initialization count */

static GList *st_list                   = NULL;	/* list of IDialog instances */

/* a data structure attached to each instance
 */
typedef struct {
	GtkApplicationWindow *main_window;
	GtkWindow            *parent;
	gchar                *xml_fname;
	gchar                *toplevel_name;
}
	sIDialog;

static GType     register_type( void );
static void      interface_base_init( myIDialogInterface *klass );
static void      interface_base_finalize( myIDialogInterface *klass );
static gchar    *idialog_get_identifier( const myIDialog *instance );
static void      idialog_init( myIDialog *instance );
static sIDialog *get_idialog_data( const myIDialog *instance );
static void      on_idialog_finalized( sIDialog *sdata, GObject *finalized_idialog );

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

	g_type_interface_add_prerequisite( type, GTK_TYPE_WINDOW );

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
 * my_idialog_get_main_window:
 * @instance: this #myIDialog instance.
 *
 * Returns: the main window.
 *
 * The returned reference is owned by the implementation, and should
 * not be released by the caller.
 */
GtkApplicationWindow *
my_idialog_get_main_window( const myIDialog *instance )
{
	sIDialog *sdata;

	g_return_val_if_fail( instance && MY_IS_IDIALOG( instance ), NULL );

	sdata = get_idialog_data( instance );

	return( sdata->main_window );
}

/**
 * my_idialog_set_main_window:
 * @instance: this #myIDialog instance.
 * @main_window: the #GtkApplicationWindow main window of the application.
 *
 * Sets the main window, which happens to be the default parent.
 */
void
my_idialog_set_main_window( myIDialog *instance, GtkApplicationWindow *main_window )
{
	sIDialog *sdata;

	g_return_if_fail( instance && MY_IS_IDIALOG( instance ));
	g_return_if_fail( main_window && GTK_IS_APPLICATION_WINDOW( main_window ));

	sdata = get_idialog_data( instance );
	sdata->main_window = main_window;
}

/**
 * my_idialog_set_ui_from_file:
 * @instance: this #myIDialog instance.
 * @xml_fname: the pathname of the XML file which contains the UI
 *  definition.
 * @toplevel_name: the name of the toplevel to be picked up from the
 *  @xml_fname file.
 *
 * Builds the user interface from the @xml_fname.
 */
void
my_idialog_set_ui_from_file( myIDialog *instance, const gchar *xml_fname, const gchar *toplevel_name )
{
	sIDialog *sdata;
	GtkWidget *toplevel;
	GtkWidget *old_vbox, *new_vbox;

	g_return_if_fail( instance && MY_IS_IDIALOG( instance ));
	g_return_if_fail( my_strlen( xml_fname ));
	g_return_if_fail( my_strlen( toplevel_name ));

	sdata = get_idialog_data( instance );
	sdata->xml_fname = g_strdup( xml_fname );
	sdata->toplevel_name = g_strdup( toplevel_name );

	toplevel = my_utils_builder_load_from_path( xml_fname, toplevel_name );
	if( toplevel ){
		if( GTK_IS_DIALOG( instance ) && GTK_IS_DIALOG( toplevel )){
			old_vbox = my_utils_container_get_child_by_type( GTK_CONTAINER( instance ), GTK_TYPE_BOX );
			if( old_vbox ){
				gtk_container_remove( GTK_CONTAINER( instance ), old_vbox );
			}
			new_vbox = my_utils_container_get_child_by_type( GTK_CONTAINER( toplevel ), GTK_TYPE_BOX );
			if( new_vbox ){
				g_object_ref( new_vbox );
				gtk_container_remove( GTK_CONTAINER( toplevel ), new_vbox );
				gtk_container_add( GTK_CONTAINER( instance ), new_vbox );
				g_object_unref( new_vbox );
			}
		}
		gtk_widget_destroy( toplevel );
	}
}

/**
 * my_idialog_present:
 * @instance: this #myIDialog instance.
 *
 * Present this window, or a previous window with the same identifier,
 * for a non-modal user interaction.
 *
 * If a previous window with the same identifier is eventually found,
 * then this current @instance is #g_object_unref(), and the previous
 * window is returned instead.
 *
 * After the call, the @instance may so be invalid.
 */
void
my_idialog_present( myIDialog *instance )
{
	GList *it;
	myIDialog *other, *found;
	gchar *instance_id, *other_id;
	gint cmp;
	sIDialog *sdata;

	g_return_if_fail( instance && MY_IS_IDIALOG( instance ));

	found = NULL;
	instance_id = idialog_get_identifier( instance );

	for( it=st_list ; it ; it=it->next ){
		other = MY_IDIALOG( it->data );
		other_id = idialog_get_identifier( other );
		cmp = g_utf8_collate( instance_id, other_id );
		g_free( other_id );
		if( cmp == 0 ){
			found = other;
			break;
		}
	}

	g_free( instance_id );

	if( found ){
		gtk_widget_destroy( GTK_WIDGET( instance ));

	} else {
		sdata = get_idialog_data( instance );
		if( !sdata->parent ){
			sdata->parent = GTK_WINDOW( sdata->main_window );
		}
		if( sdata->parent ){
			gtk_window_set_transient_for( GTK_WINDOW( instance ), sdata->parent );
		}
		my_utils_window_restore_position( GTK_WINDOW( instance ), sdata->toplevel_name );
		idialog_init( instance );
		st_list = g_list_prepend( st_list, instance );
		found = instance;
	}

	gtk_window_present( GTK_WINDOW( found ));
}

/*
 * idialog_get_identifier:
 * @instance: this #myIDialog instance.
 *
 * Returns: the instance identifier.
 */
static gchar *
idialog_get_identifier( const myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_get_identifier";

	if( MY_IDIALOG_GET_INTERFACE( instance )->get_identifier ){
		return( MY_IDIALOG_GET_INTERFACE( instance )->get_identifier( instance ));
	}

	g_info( "%s: myIDialog instance %p does not provide 'get_identifier()' method",
			thisfn, ( void * ) instance );
	return( NULL );
}

/*
 * idialog_init:
 * @instance: this #myIDialog instance.
 *
 * Initialize once the dialog.
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "my_idialog_init";

	if( MY_IDIALOG_GET_INTERFACE( instance )->init ){
		MY_IDIALOG_GET_INTERFACE( instance )->init( instance );
		return;
	}

	g_info( "%s: myIDialog instance %p does not provide 'init()' method",
			thisfn, ( void * ) instance );
}

static sIDialog *
get_idialog_data( const myIDialog *instance )
{
	sIDialog *sdata;

	sdata = ( sIDialog * ) g_object_get_data( G_OBJECT( instance ), IDIALOG_DATA );

	if( !sdata ){
		sdata = g_new0( sIDialog, 1 );
		g_object_set_data( G_OBJECT( instance ), IDIALOG_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_idialog_finalized, sdata );
	}

	return( sdata );
}

static void
on_idialog_finalized( sIDialog *sdata, GObject *finalized_idialog )
{
	static const gchar *thisfn = "my_idialog_on_idialog_finalized";

	g_debug( "%s: sdata=%p, finalized_idialog=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_idialog );

	g_free( sdata->xml_fname );
	g_free( sdata->toplevel_name );
	g_free( sdata );

	st_list = g_list_remove( st_list, finalized_idialog );
}
