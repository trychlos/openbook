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

#include "api/ofo-sgbd.h"

#include "core/ofa-settings.h"

#include "ui/ofa-main-window.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-plugin.h"
#include "ui/ofa-preferences.h"

/* private instance data
 */
struct _ofaApplicationPrivate {
	gboolean        dispose_has_run;

	/* properties
	 */
	GOptionEntry   *options;
	gchar          *application_name;
	gchar          *description;
	gchar          *icon_name;

	/* command-line args
	 */
	ofaOpenDossier *sod;

	/* internals
	 */
	int             argc;
	GStrv           argv;
	int             code;
	ofaMainWindow  *main_window;
	GMenuModel     *menu;
};

/* class properties
 */
enum {
	OFA_PROP_OPTIONS_ID = 1,
	OFA_PROP_APPLICATION_NAME_ID,
	OFA_PROP_DESCRIPTION_ID,
	OFA_PROP_ICON_NAME_ID,
};

static gboolean pref_confirm_on_quit = FALSE;

static const gchar            *st_application_id     = "org.trychlos.openbook.ui";
static const GApplicationFlags st_application_flags  = G_APPLICATION_NON_UNIQUE;

static const gchar            *st_application_name   = N_( "Open Freelance Accounting" );
static const gchar            *st_description        = N_( "Une comptabilité en partie-double orientée vers les freelances" );
static const gchar            *st_icon_name          = N_( "openbook" );

static       gboolean          st_version_opt        = FALSE;
static       gchar            *st_dossier_name_opt   = NULL;
static       gchar            *st_dossier_user_opt   = NULL;
static       gchar            *st_dossier_passwd_opt = NULL;

static       GOptionEntry      st_option_entries[]  = {
	{ "dossier",  'd', 0, G_OPTION_ARG_STRING, &st_dossier_name_opt,
			N_( "open the specified dossier []" ), NULL },
	{ "user",     'u', 0, G_OPTION_ARG_STRING, &st_dossier_user_opt,
			N_( "username to be used on opening the dossier []" ), NULL },
	{ "password", 'p', 0, G_OPTION_ARG_STRING, &st_dossier_passwd_opt,
			N_( "password to be used on opening the dossier []" ), NULL },
	{ "version",  'v', 0, G_OPTION_ARG_NONE, &st_version_opt,
			N_( "display the version number [no]" ), NULL },
	{ NULL }
};

G_DEFINE_TYPE( ofaApplication, ofa_application, GTK_TYPE_APPLICATION )

static void     application_startup( GApplication *application );
static void     application_activate( GApplication *application );
static void     application_open( GApplication *application, GFile **files, gint n_files, const gchar *hint );

static void     init_i18n( ofaApplication *application );
static gboolean init_gtk_args( ofaApplication *application );
static gboolean manage_options( ofaApplication *application );
static void     do_prepare_for_open( ofaApplication *appli, const gchar *dossier, const gchar *user, const gchar *passwd );

static void     on_new( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_open( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_user_prefs( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_quit( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_about( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_version( ofaApplication *application );

static const GActionEntry st_app_entries[] = {
		{ "new",        on_new,        NULL, NULL, NULL },
		{ "open",       on_open,       NULL, NULL, NULL },
		{ "user_prefs", on_user_prefs, NULL, NULL, NULL },
		{ "quit",       on_quit,       NULL, NULL, NULL },
		{ "about",      on_about,      NULL, NULL, NULL },
};

static const gchar  *st_appmenu_xml = PKGUIDIR "/ofa-app-menubar.ui";
static const gchar  *st_appmenu_id  = "app-menu";

static void
application_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_application_finalize";
	ofaApplicationPrivate *priv;

	g_return_if_fail( OFA_IS_APPLICATION( instance ));

	priv = OFA_APPLICATION( instance )->private;

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_strfreev( priv->argv );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_application_parent_class )->finalize( instance );
}

static void
application_dispose( GObject *instance )
{
	ofaApplicationPrivate *priv;

	g_return_if_fail( OFA_IS_APPLICATION( instance ));

	priv = OFA_APPLICATION( instance )->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		if( priv->menu ){
			g_object_unref( priv->menu );
		}

		ofa_plugin_release_modules();
		ofa_settings_free();
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_application_parent_class )->dispose( instance );
}

static void
application_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaApplicationPrivate *priv;

	g_return_if_fail( OFA_IS_APPLICATION( object ));

	priv = OFA_APPLICATION( object )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_OPTIONS_ID:
				g_value_set_pointer( value, priv->options );
				break;

			case OFA_PROP_APPLICATION_NAME_ID:
				g_value_set_string( value, priv->application_name );
				break;

			case OFA_PROP_DESCRIPTION_ID:
				g_value_set_string( value, priv->description );
				break;

			case OFA_PROP_ICON_NAME_ID:
				g_value_set_string( value, priv->icon_name );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
application_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaApplicationPrivate *priv;

	g_return_if_fail( OFA_IS_APPLICATION( object ));

	priv = OFA_APPLICATION( object )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_OPTIONS_ID:
				priv->options = g_value_get_pointer( value );
				break;

			case OFA_PROP_APPLICATION_NAME_ID:
				g_free( priv->application_name );
				priv->application_name = g_value_dup_string( value );
				break;

			case OFA_PROP_DESCRIPTION_ID:
				g_free( priv->description );
				priv->description = g_value_dup_string( value );
				break;

			case OFA_PROP_ICON_NAME_ID:
				g_free( priv->icon_name );
				priv->icon_name = g_value_dup_string( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
ofa_application_init( ofaApplication *self )
{
	static const gchar *thisfn = "ofa_application_init";

	g_return_if_fail( OFA_IS_APPLICATION( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaApplicationPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->sod = NULL;
}

static void
ofa_application_class_init( ofaApplicationClass *klass )
{
	static const gchar *thisfn = "ofa_application_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = application_get_property;
	G_OBJECT_CLASS( klass )->set_property = application_set_property;
	G_OBJECT_CLASS( klass )->dispose = application_dispose;
	G_OBJECT_CLASS( klass )->finalize = application_finalize;

	G_APPLICATION_CLASS( klass )->startup = application_startup;
	G_APPLICATION_CLASS( klass )->activate = application_activate;
	G_APPLICATION_CLASS( klass )->open = application_open;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_OPTIONS_ID,
			g_param_spec_pointer(
					OFA_PROP_OPTIONS,
					"Option entries",
					"The array of command-line option definitions",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_APPLICATION_NAME_ID,
			g_param_spec_string(
					OFA_PROP_APPLICATION_NAME,
					"Application name",
					"The name of the application",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_DESCRIPTION_ID,
			g_param_spec_string(
					OFA_PROP_DESCRIPTION,
					"Description",
					"A short description to be displayed in the first line of --help output",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_ICON_NAME_ID,
			g_param_spec_string(
					OFA_PROP_ICON_NAME,
					"Icon name",
					"The name of the icon of the application",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
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

			/* GApplication properties */
			"application-id",          st_application_id,
			"flags",                   st_application_flags,

			/* ofaApplication properties */
			OFA_PROP_OPTIONS,          st_option_entries,
			OFA_PROP_APPLICATION_NAME, gettext( st_application_name ),
			OFA_PROP_DESCRIPTION,      gettext( st_description ),
			OFA_PROP_ICON_NAME,        gettext( st_icon_name ),
			NULL );

	ofa_plugin_load_modules();

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
	GError *error;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );

	/* chain up to the parent class */
	if( G_APPLICATION_CLASS( ofa_application_parent_class )->startup ){
		G_APPLICATION_CLASS( ofa_application_parent_class )->startup( application );
	}

	/* define the application actions */
	g_action_map_add_action_entries(
			G_ACTION_MAP( appli ),
	        st_app_entries, G_N_ELEMENTS( st_app_entries ),
	        ( gpointer ) appli );

	/* define a traditional menubar
	 * the program will abort if GtkBuilder is not able to be parsed
	 * from the given file
	 */
	error = NULL;
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_appmenu_xml, &error )){
		menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_appmenu_id ));
		if( menu ){
			appli->private->menu = menu;
			/*gtk_application_set_menubar( GTK_APPLICATION( application ), menu );*/
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

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );

	appli->private->main_window = ofa_main_window_new( appli );
	g_debug( "%s: main window instanciated at %p", thisfn, appli->private->main_window );

	gtk_window_present( GTK_WINDOW( appli->private->main_window ));

	if( appli->private->sod ){
		g_signal_emit_by_name(
				appli->private->main_window, OFA_SIGNAL_OPEN_DOSSIER, appli->private->sod );
	}
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

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );

	if( !appli->private->main_window ){
		appli->private->main_window = ofa_main_window_new( appli );
	}

	/*for (i = 0; i < n_files; i++)
	    example_app_window_open (win, files[i]);*/

	gtk_window_present( GTK_WINDOW( appli->private->main_window ));
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
			g_warning( "%s", _( "Erreur à l'interprétation des arguments en ligne de commande" ));
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

	} else if( st_dossier_name_opt || st_dossier_user_opt || st_dossier_passwd_opt ){

		if( !g_utf8_strlen( st_dossier_name_opt, -1 ) ||
				!g_utf8_strlen( st_dossier_user_opt, -1 ) ||
				!g_utf8_strlen( st_dossier_passwd_opt, -1 )){
			g_warning( "%s: unable to handle wrong arguments: dossier=%s, user=%s, password=%s",
					thisfn, st_dossier_name_opt, st_dossier_user_opt, st_dossier_passwd_opt );

		} else {
			do_prepare_for_open( application,
					st_dossier_name_opt, st_dossier_user_opt, st_dossier_passwd_opt );
		}
	}

	return( ret );
}

static void
do_prepare_for_open( ofaApplication *appli, const gchar *dossier, const gchar *user, const gchar *passwd )
{
	static const gchar *thisfn = "ofa_application_do_prepare_for_open";
	ofaOpenDossier *sod;
	ofoSgbd *sgbd;

	sod = g_new0( ofaOpenDossier, 1 );
	ofa_settings_get_dossier( dossier, &sod->host, &sod->port, &sod->socket, &sod->dbname );
	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofo_sgbd_connect(
			sgbd,
			sod->host,
			sod->port,
			sod->socket,
			sod->dbname,
			user,
			passwd )){

		g_free( sod->host );
		g_free( sod->socket );
		g_free( sod->dbname );
		g_free( sod );
		g_object_unref( sgbd );
		return;
	}

	g_debug( "%s: connection successfully opened", thisfn );

	sod->dossier = g_strdup( dossier );
	sod->account = g_strdup( user );
	sod->password = g_strdup( passwd );

	appli->private->sod = sod;
	g_object_unref( sgbd );
}

static void
on_new( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_new";
	ofaApplicationPrivate *priv;
	ofaOpenDossier *ood;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	priv = OFA_APPLICATION( user_data )->private;
	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ood = ofa_dossier_new_run( priv->main_window );
	if( ood ){
		/*g_signal_emit_by_name( priv->main_window, OFA_SIGNAL_OPEN_DOSSIER, ood );*/
	}
}

static void
on_open( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_open";
	ofaApplicationPrivate *priv;
	ofaOpenDossier *ood;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	priv = OFA_APPLICATION( user_data )->private;

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ood = ofa_dossier_open_run( priv->main_window );
	if( ood ){
		g_signal_emit_by_name( priv->main_window, OFA_SIGNAL_OPEN_DOSSIER, ood );
	}
}

static void
on_user_prefs( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_user_prefs";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));
	priv = OFA_APPLICATION( user_data )->private;
	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_preferences_run( priv->main_window );
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
		if( !pref_confirm_on_quit || ofa_main_window_is_willing_to_quit( window )){
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
	static const gchar *thisfn = "ofa_application_on_version";

	g_warning( "%s: application=%p: TO BE WRITTEN", thisfn, application );
}

/**
 * ofa_application_get_menu_model:
 */
GMenuModel *
ofa_application_get_menu_model( const ofaApplication *application )
{
	g_return_val_if_fail( OFA_IS_APPLICATION( application ), NULL );

	if( !application->private->dispose_has_run ){

		return( application->private->menu );
	}

	return( NULL );
}
