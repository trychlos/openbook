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
#include <string.h>
#include <glib/gprintf.h>

#include "ui/ofa-main-window.h"

/* private class data
 */
struct _ofaApplicationClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaApplicationPrivate {
	gboolean      dispose_has_run;

	/* properties
	 */
	int           argc;
	GStrv         argv;
	GOptionEntry *options;
	gchar        *application_name;
	gchar        *description;
	gchar        *icon_name;
	int           code;
};

/* class properties
 */
enum {
	OFA_PROP_0,

	OFA_PROP_ARGC_ID,
	OFA_PROP_ARGV_ID,
	OFA_PROP_OPTIONS_ID,
	OFA_PROP_APPLICATION_NAME_ID,
	OFA_PROP_DESCRIPTION_ID,
	OFA_PROP_ICON_NAME_ID,
	OFA_PROP_CODE_ID,

	OFA_PROP_N_PROPERTIES
};

static       GtkApplicationClass *st_parent_class = NULL;

static const gchar               *st_application_id    = "org.trychlos.openbook.ui";
static const GApplicationFlags    st_application_flags = G_APPLICATION_NON_UNIQUE;

static const gchar               *st_application_name  = N_( "Open Freelance Accounting" );
static const gchar               *st_description       = N_( "La comptabilité pour les professionnels libéraux" );
static const gchar               *st_icon_name         = N_( "openbook" );

static       gboolean             st_version_opt       = FALSE;

static       GOptionEntry         st_option_entries[]  = {
	{ "version"   , 'v', 0, G_OPTION_ARG_NONE, &st_version_opt,
			N_( "Affiche la version de l'application, et termine [non]" ), NULL },
	{ NULL }
};

static GType    register_type( void );
static void     class_init( ofaApplicationClass *klass );
static void     instance_init( GTypeInstance *instance, gpointer klass );
static void     instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec );
static void     instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec );
static void     instance_dispose( GObject *application );
static void     instance_finalize( GObject *application );

static void     application_startup( GApplication *application );
static void     application_activate( GApplication *application );
static void     application_open( GApplication *application, GFile **files, gint n_files, const gchar *hint );

static void     init_i18n( ofaApplication *application );
static gboolean init_gtk_args( ofaApplication *application );
static gboolean manage_options( ofaApplication *application );

static void     on_new( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_open( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_quit( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_about( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_version( ofaApplication *application );

static const GActionEntry st_app_entries[] = {
		{ "new",   on_new,   NULL, NULL, NULL },
		{ "open",  on_open,  NULL, NULL, NULL },
		{ "quit",  on_quit,  NULL, NULL, NULL },
		{ "about", on_about, NULL, NULL, NULL },
};

static const gchar  *st_appmenu_xml = PKGUIDIR "/ofa-app-menubar.ui";
static const gchar  *st_appmenu_id = "app-menu";


GType
ofa_application_get_type( void )
{
	static GType application_type = 0;

	if( !application_type ){
		application_type = register_type();
	}

	return( application_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_application_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaApplicationClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaApplication ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( GTK_TYPE_APPLICATION, "ofaApplication", &info, 0 );

	return( type );
}

static void
class_init( ofaApplicationClass *klass )
{
	static const gchar *thisfn = "ofa_application_class_init";
	GObjectClass *object_class;
	GApplicationClass *application_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->get_property = instance_get_property;
	object_class->set_property = instance_set_property;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	application_class = G_APPLICATION_CLASS( klass );
	application_class->startup = application_startup;
	application_class->activate = application_activate;
	application_class->open = application_open;

	g_object_class_install_property( object_class, OFA_PROP_ARGC_ID,
			g_param_spec_int(
					OFA_PROP_ARGC,
					"Arguments count",
					"The count of command-line arguments",
					0, 65535, 0,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, OFA_PROP_ARGV_ID,
			g_param_spec_boxed(
					OFA_PROP_ARGV,
					"Arguments",
					"The array of command-line arguments",
					G_TYPE_STRV,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, OFA_PROP_OPTIONS_ID,
			g_param_spec_pointer(
					OFA_PROP_OPTIONS,
					"Option entries",
					"The array of command-line option definitions",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, OFA_PROP_APPLICATION_NAME_ID,
			g_param_spec_string(
					OFA_PROP_APPLICATION_NAME,
					"Application name",
					"The name of the application",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, OFA_PROP_DESCRIPTION_ID,
			g_param_spec_string(
					OFA_PROP_DESCRIPTION,
					"Description",
					"A short description to be displayed in the first line of --help output",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, OFA_PROP_ICON_NAME_ID,
			g_param_spec_string(
					OFA_PROP_ICON_NAME,
					"Icon name",
					"The name of the icon of the application",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, OFA_PROP_CODE_ID,
			g_param_spec_int(
					OFA_PROP_CODE,
					"Return code",
					"The return code of the application",
					-127, 127, 0,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	klass->private = g_new0( ofaApplicationClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *application, gpointer klass )
{
	static const gchar *thisfn = "ofa_application_instance_init";
	ofaApplication *self;

	g_return_if_fail( OFA_IS_APPLICATION( application ));

	g_debug( "%s: application=%p (%s), klass=%p",
			thisfn, ( void * ) application, G_OBJECT_TYPE_NAME( application ), ( void * ) klass );

	self = OFA_APPLICATION( application );

	self->private = g_new0( ofaApplicationPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaApplication *self;

	g_return_if_fail( OFA_IS_APPLICATION( object ));
	self = OFA_APPLICATION( object );

	if( !self->private->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_ARGC_ID:
				g_value_set_int( value, self->private->argc );
				break;

			case OFA_PROP_ARGV_ID:
				g_value_set_boxed( value, self->private->argv );
				break;

			case OFA_PROP_OPTIONS_ID:
				g_value_set_pointer( value, self->private->options );
				break;

			case OFA_PROP_APPLICATION_NAME_ID:
				g_value_set_string( value, self->private->application_name );
				break;

			case OFA_PROP_DESCRIPTION_ID:
				g_value_set_string( value, self->private->description );
				break;

			case OFA_PROP_ICON_NAME_ID:
				g_value_set_string( value, self->private->icon_name );
				break;

			case OFA_PROP_CODE_ID:
				g_value_set_int( value, self->private->code );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaApplication *self;

	g_return_if_fail( OFA_IS_APPLICATION( object ));
	self = OFA_APPLICATION( object );

	if( !self->private->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_ARGC_ID:
				self->private->argc = g_value_get_int( value );
				break;

			case OFA_PROP_ARGV_ID:
				if( self->private->argv ){
					g_boxed_free( G_TYPE_STRV, self->private->argv );
				}
				self->private->argv = g_value_dup_boxed( value );
				break;

			case OFA_PROP_OPTIONS_ID:
				self->private->options = g_value_get_pointer( value );
				break;

			case OFA_PROP_APPLICATION_NAME_ID:
				g_free( self->private->application_name );
				self->private->application_name = g_value_dup_string( value );
				break;

			case OFA_PROP_DESCRIPTION_ID:
				g_free( self->private->description );
				self->private->description = g_value_dup_string( value );
				break;

			case OFA_PROP_ICON_NAME_ID:
				g_free( self->private->icon_name );
				self->private->icon_name = g_value_dup_string( value );
				break;

			case OFA_PROP_CODE_ID:
				self->private->code = g_value_get_int( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_dispose( GObject *application )
{
	static const gchar *thisfn = "ofa_application_instance_dispose";
	ofaApplication *self;

	g_return_if_fail( OFA_IS_APPLICATION( application ));

	self = OFA_APPLICATION( application );

	if( !self->private->dispose_has_run ){

		g_debug( "%s: application=%p (%s)", thisfn, ( void * ) application, G_OBJECT_TYPE_NAME( application ));

		self->private->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( application );
		}
	}
}

static void
instance_finalize( GObject *application )
{
	static const gchar *thisfn = "ofa_application_instance_finalize";
	ofaApplication *self;

	g_return_if_fail( OFA_IS_APPLICATION( application ));

	g_debug( "%s: application=%p (%s)", thisfn, ( void * ) application, G_OBJECT_TYPE_NAME( application ));

	self = OFA_APPLICATION( application );

	g_strfreev( self->private->argv );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( application );
	}
}

/**
 * ofa_application_new:
 *
 * Returns: a newly allocated ofaApplication object.
 */
ofaApplication *
ofa_application_new( void )
{
	ofaApplication *application;

	application = g_object_new( OFA_TYPE_APPLICATION,
			"application-id", st_application_id,
			"flags",          st_application_flags,
			NULL );

	g_object_set( G_OBJECT( application ),
			OFA_PROP_OPTIONS,          st_option_entries,
			OFA_PROP_APPLICATION_NAME, gettext( st_application_name ),
			OFA_PROP_DESCRIPTION,      gettext( st_description ),
			OFA_PROP_ICON_NAME,        gettext( st_icon_name ),
			NULL );

	return( application );
}

/**
 * ofa_application_run_with_args:
 * @application: this #ofaApplication -derived instance.
 *
 * Starts and runs the application.
 * Takes care of creating, initializing, and running the main window.
 *
 * Returns: an %int code suitable as an exit code for the program.
 */
int
ofa_application_run_with_args( ofaApplication *application, int argc, GStrv argv )
{
	static const gchar *thisfn = "ofa_application_run_with_args";
	ofaApplicationPrivate *priv;

	g_return_val_if_fail( OFA_IS_APPLICATION( application ), OFA_EXIT_CODE_PROGRAM );

	priv = application->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: application=%p (%s), argc=%d",
				thisfn,
				( void * ) application, G_OBJECT_TYPE_NAME( application ),
				argc );

		priv->argc = argc;
		priv->argv = g_strdupv( argv );
		priv->code = OFA_EXIT_CODE_OK;

		init_i18n( application );
		g_set_application_name( priv->application_name );
		gtk_window_set_default_icon_name( priv->icon_name );

		if( init_gtk_args( application ) &&
			manage_options( application )){

			g_debug( "%s: entering g_application_run", thisfn );
			priv->code = g_application_run( G_APPLICATION( application ), 0, NULL );
		}
	}

	return( priv->code );
}

/*
 * https://wiki.gnome.org/HowDoI/GtkApplication
 *
 * Invoked on the primary instance immediately after registration.
 *
 * When your application starts, the startup signal will be fired. This
 * gives you a chance to perform initialisation tasks that are not
 * directly related to showing a new window. After this, depending on
 * how the application is started, either activate or open will be called
 * next.
 *
 * GtkApplication defaults to applications being single-instance. If the
 * user attempts to start a second instance of a single-instance
 * application then GtkApplication will signal the first instance and
 * you will receive additional activate or open signals. In this case,
 * the second instance will exit immediately, without calling startup
 * or shutdown.
 *
 * For this reason, you should do essentially no work at all from main().
 * All startup initialisation should be done in startup. This avoids
 * wasting work in the second-instance case where the program just exits
 * immediately.
 */
static void
application_startup( GApplication *application )
{
	static const gchar *thisfn = "ofa_application_startup";
	ofaApplication *appli;
	GtkBuilder *builder;
	GMenuModel *menu;
	GError *error = NULL;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );

	/* chain up to the parent class */
	if( G_APPLICATION_CLASS( st_parent_class )->startup ){
		G_APPLICATION_CLASS( st_parent_class )->startup( application );
	}

	/* define the application actions */
	g_action_map_add_action_entries(
			G_ACTION_MAP( appli ),
	        st_app_entries, G_N_ELEMENTS( st_app_entries ),
	        ( gpointer ) appli );

	/* define a traditional menubar */
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_appmenu_xml, &error )){
		menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_appmenu_id ));
		if( menu ){
			gtk_application_set_menubar( GTK_APPLICATION( application ), menu );
		} else {
			g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_appmenu_id, st_appmenu_xml );
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}
	g_object_unref( builder );
}

/*
 * https://wiki.gnome.org/Projects/GLib/GApplication/Introduction
 * https://wiki.gnome.org/HowDoI/GtkApplication
 *
 * activate is executed by GApplication when the application is "activated".
 * This corresponds to the program being run from the command line, or when
 * its icon is clicked on in an application launcher.
 * From a semantic standpoint, activate should usually do one of two things,
 * depending on the type of application.
 *
 * If your application is the type of application that deals with several
 * documents at a time, in separate windows (and/or tabs) then activate
 * should involve showing a window or creating a tab for a new document.
 *
 * If your application is more like the type of application with one primary
 * main window then activate should usually involve raising this window with
 * gtk_window_present(). It is the choice of the application in this case if
 * the window itself is constructed in startup or on the first execution of
 * activate.
 *
 * activate is potentially called many times in a process or maybe never.
 * If the process is started without files to open then activate will be run
 * after startup. It may also be run again if a second instance of the
 * process is started.
 */
static void
application_activate( GApplication *application )
{
	static const gchar *thisfn = "ofa_application_activate";
	ofaApplication *appli;
	ofaMainWindow *window;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );

	window = ofa_main_window_new( appli );
	g_debug( "%s: main window instanciated at %p", thisfn, window );
	gtk_window_present( GTK_WINDOW( window ));
}

/*
 * https://wiki.gnome.org/Projects/GLib/GApplication/Introduction
 *
 * open is similar to activate, but it is used when some files have been
 * passed to the application to open.
 * In fact, you could think of activate as a special case of open: the
 * one with zero files.
 * Similar to activate, open should create a window or tab. It should
 * open the file in this window. If multiple files are given, possibly
 * several windows should be opened.
 * open will only be invoked in the case that your application declares
 * that it supports opening files with the G_APPLICATION_HANDLES_OPE
 * GApplicationFlag.
 */
static void
application_open( GApplication *application, GFile **files, gint n_files, const gchar *hint )
{
	static const gchar *thisfn = "ofa_application_open";
	ofaApplication *appli;
	GList *windows;
	ofaMainWindow *window;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );

	windows = gtk_application_get_windows( GTK_APPLICATION( appli ));
	if( windows ){
		window = OFA_MAIN_WINDOW( windows->data );
	} else {
		window = ofa_main_window_new( appli );
	}

	/*for (i = 0; i < n_files; i++)
	    example_app_window_open (win, files[i]);*/

	gtk_window_present( GTK_WINDOW( window ));
}

/*
 * i18n initialization
 */
static void
init_i18n( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_application_init_i18n";

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

#ifdef ENABLE_NLS
	bindtextdomain( GETTEXT_PACKAGE, GNOMELOCALEDIR );
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset( GETTEXT_PACKAGE, "UTF-8" );
# endif
	textdomain( GETTEXT_PACKAGE );
#endif
}

/*
 * Pre-Gtk+ initialization
 *
 * Though GApplication has its own infrastructure to handle command-line
 * arguments, it appears that it does not deal with Gtk+-specific arguments.
 * We so have to explicitely call gtk_init_with_args() in order to let Gtk+
 * "eat" its own arguments, and only have to handle our owns...
 */
static gboolean
init_gtk_args( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_application_init_gtk_args";
	gboolean ret;
	char *parameter_string;
	GError *error;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	/* manage command-line arguments
	 */
	if( application->private->options ){
		parameter_string = g_strdup( g_get_application_name());
		error = NULL;
		ret = gtk_init_with_args(
				&application->private->argc,
				( char *** ) &application->private->argv,
				parameter_string,
				application->private->options,
				GETTEXT_PACKAGE,
				&error );
		if( error ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
			ret = FALSE;
			application->private->code = OFA_EXIT_CODE_ARGS;
		}
		g_free( parameter_string );

	} else {
		ret = gtk_init_check(
				&application->private->argc,
				( char *** ) &application->private->argv );
		if( !ret ){
			g_warning( "%s", _( "Unable to interpret command-line arguments" ));
			application->private->code = OFA_EXIT_CODE_ARGS;
		}
	}

	return( ret );
}

static gboolean
manage_options( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_application_manage_options";
	gboolean ret;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	ret = TRUE;

	/* display the program version ?
	 * if yes, then stops here
	 */
	if( st_version_opt ){
		on_version( application );
		ret = FALSE;
	}

	return( ret );
}

static void
on_new( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
#if 0
	static const gchar *thisfn = "ofa_application_on_new";
	ofaApplication *application;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	application = OFA_APPLICATION( user_data );
#endif
}

static void
on_open( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
#if 0
	static const gchar *thisfn = "ofa_application_on_new";
	ofaApplication *application;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	application = OFA_APPLICATION( user_data );
#endif
}

/*
 * quitting means quitting the current instance of the application
 * i.e. the most recent ofaMainWindows
 */
static void
on_quit( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_quit";
	ofaApplication *application;
	GList *windows;
	ofaMainWindow *window;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	application = OFA_APPLICATION( user_data );

	windows = gtk_application_get_windows( GTK_APPLICATION( application ));
	if( windows ){
		window = OFA_MAIN_WINDOW( windows->data );
		if( ofa_main_window_is_willing_to_quit( window )){
			gtk_widget_destroy( GTK_WIDGET( window ));
		}
	} else {
		g_application_quit( G_APPLICATION( application ));
	}
}

static void
on_about( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
#if 0
	static const gchar *thisfn = "ofa_application_on_about";
	ofaApplication *application;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	application = OFA_APPLICATION( user_data );
#endif
}

static void
on_version( ofaApplication *application )
{
#if 0
	static const gchar *thisfn = "ofa_application_on_version";

	g_debug( "%s: application=%p", application );
#endif
}
