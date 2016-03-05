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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-plugin.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"

#include "core/ofa-file-dir.h"
#include "core/ofa-main-window.h"

#include "ui/ofa-about.h"
#include "ui/ofa-application.h"
#include "ui/ofa-dossier-manager.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-store.h"
#include "ui/ofa-plugin-manager.h"
#include "ui/ofa-restore-assistant.h"

/* private instance data
 */
struct _ofaApplicationPrivate {
	gboolean         dispose_has_run;

	/* properties
	 */
	GOptionEntry    *options;
	gchar           *application_name;
	gchar           *description;
	gchar           *icon_name;
	ofaHub          *hub;

	/* internals
	 */
	int              argc;
	GStrv            argv;
	int              code;
	ofaMainWindow   *main_window;
	GMenuModel      *menu;
	ofaFileDir      *file_dir;
	ofaDossierStore *dos_store;

	/* menu items
	 */
	GSimpleAction   *action_open;
};

/* class properties
 */
enum {
	OFA_PROP_OPTIONS_ID = 1,
	OFA_PROP_APPLICATION_NAME_ID,
	OFA_PROP_DESCRIPTION_ID,
	OFA_PROP_ICON_NAME_ID,
	OFA_PROP_HUB_ID
};

/* signals defined here
 */
enum {
	MAIN_WINDOW_CREATED = 0,
	MENU_DEFINITION,
	N_SIGNALS
};

static guint             st_signals[ N_SIGNALS ] = { 0 };

static const gchar       *st_application_id     = "org.trychlos.openbook.ui";

static const gchar       *st_application_name   = N_( "Open Freelance Accounting" );
static const gchar       *st_description        = N_( "Une comptabilité en partie-double orientée vers les freelances" );
static const gchar       *st_icon_name          = N_( "openbook" );

static       gboolean     st_version_opt        = FALSE;

static       gchar       *st_dossier_name_opt   = NULL;
static       gchar       *st_dossier_begin_opt  = NULL;
static       gchar       *st_dossier_end_opt    = NULL;
static       gchar       *st_dossier_user_opt   = NULL;
static       gchar       *st_dossier_passwd_opt = NULL;

static       GOptionEntry st_option_entries[]   = {
	{ "dossier",  'd', 0, G_OPTION_ARG_STRING, &st_dossier_name_opt,
			N_( "open the specified dossier []" ), NULL },
	{ "begin",    'b', 0, G_OPTION_ARG_STRING, &st_dossier_begin_opt,
			N_( "beginning date (yyyymmdd) of the exercice to open []" ), NULL },
	{ "end",      'e', 0, G_OPTION_ARG_STRING, &st_dossier_end_opt,
			N_( "ending date (yyyymmdd) of the exercice to open []" ), NULL },
	{ "user",     'u', 0, G_OPTION_ARG_STRING, &st_dossier_user_opt,
			N_( "username to be used on opening the dossier []" ), NULL },
	{ "password", 'p', 0, G_OPTION_ARG_STRING, &st_dossier_passwd_opt,
			N_( "password to be used on opening the dossier []" ), NULL },
	{ "version",  'v', 0, G_OPTION_ARG_NONE, &st_version_opt,
			N_( "display the version number [no]" ), NULL },
	{ NULL }
};

static void     ihubber_iface_init( ofaIHubberInterface *iface );
static guint    ihubber_get_interface_version( const ofaIHubber *instance );
static ofaHub  *ihubber_new_hub( ofaIHubber *instance, const ofaIDBConnect *connect );
static ofaHub  *ihubber_get_hub( const ofaIHubber *instance );
static void     ihubber_clear_hub( ofaIHubber *instance );

static void     init_i18n( ofaApplication *application );
static gboolean init_gtk_args( ofaApplication *application );
static gboolean manage_options( ofaApplication *application );

static void     application_startup( GApplication *application );
static void     appli_store_ref( ofaApplication *application, GtkBuilder *builder, const gchar *placeholder );
static void     application_activate( GApplication *application );
static void     application_open( GApplication *application, GFile **files, gint n_files, const gchar *hint );
static void     maintainer_test_function( void );

static void     setup_actions_monitor( ofaApplication *application );
static void     on_file_dir_changed( ofaFileDir *dir, guint count, const gchar *filename, ofaApplication *application );
static void     enable_action_open( ofaApplication *application, gboolean enable );
static void     on_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_new( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_open( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_restore( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_user_prefs( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_quit( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_plugin_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_about( GSimpleAction *action, GVariant *parameter, gpointer user_data );
static void     on_version( ofaApplication *application );

G_DEFINE_TYPE_EXTENDED( ofaApplication, ofa_application, GTK_TYPE_APPLICATION, 0,
		G_ADD_PRIVATE( ofaApplication )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IHUBBER, ihubber_iface_init ))

static const GActionEntry st_app_entries[] = {
		{ "manage",        on_manage,        NULL, NULL, NULL },
		{ "new",           on_new,           NULL, NULL, NULL },
		{ "open",          on_open,          NULL, NULL, NULL },
		{ "restore",       on_restore,       NULL, NULL, NULL },
		{ "user_prefs",    on_user_prefs,    NULL, NULL, NULL },
		{ "quit",          on_quit,          NULL, NULL, NULL },
		{ "plugin_manage", on_plugin_manage, NULL, NULL, NULL },
		{ "about",         on_about,         NULL, NULL, NULL },
};

static const gchar *st_resource_appmenu = "/org/trychlos/openbook/ui/ofa-app-menubar.ui";
static const gchar *st_appmenu_id       = "app-menu";

static void
application_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_application_finalize";
	ofaApplicationPrivate *priv;

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_APPLICATION( instance ));

	/* free data members here */
	priv = ofa_application_get_instance_private( OFA_APPLICATION( instance ));

	g_free( priv->application_name );
	g_free( priv->description );
	g_free( priv->icon_name );
	g_strfreev( priv->argv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_application_parent_class )->finalize( instance );
}

static void
application_dispose( GObject *instance )
{
	ofaApplicationPrivate *priv;

	g_return_if_fail( instance && OFA_IS_APPLICATION( instance ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->hub );
		g_clear_object( &priv->file_dir );
		g_clear_object( &priv->dos_store );
		g_clear_object( &priv->menu );
		ofa_plugin_release_modules();
		ofa_settings_free();
		my_utils_css_provider_free();
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_application_parent_class )->dispose( instance );
}

static void
application_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaApplicationPrivate *priv;

	g_return_if_fail( object && OFA_IS_APPLICATION( object ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( object ));

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

			case OFA_PROP_HUB_ID:
				g_value_set_pointer( value, priv->hub );
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

	priv = ofa_application_get_instance_private( OFA_APPLICATION( object ));

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

			case OFA_PROP_HUB_ID:
				priv->hub = g_value_get_pointer( value );
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
	ofaApplicationPrivate *priv;

	g_return_if_fail( OFA_IS_APPLICATION( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_application_get_instance_private( self );

	priv->dispose_has_run = FALSE;
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

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_HUB_ID,
			g_param_spec_pointer(
					OFA_PROP_HUB,
					"Icon name",
					"The name of the icon of the application",
					G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	/**
	 * ofaApplication::main-window-created:
	 *
	 * This signal is sent on the application when the main window has
	 * been created.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaApplication  *application,
	 * 						ofaMainWindow *main_window,
	 * 						gpointer user_data );
	 */
	st_signals[ MAIN_WINDOW_CREATED ] = g_signal_new_class_handler(
				"main-window-created",
				OFA_TYPE_APPLICATION,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaApplication::menu-defined:
	 *
	 * This signal is sent on the application after having defined the
	 * actions map and loaded the menu definition. As our application
	 * defines two menus (with or without a dossier being opened), this
	 * signal is sent twice, once for each corresponding #GActionMap.
	 *
	 * The plugins may take advantage of this signal for updating the
	 * provided menus and actions maps.
	 *
	 * @main_window: %NULL when dealing with application menu.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaApplication  *application,
	 * 						GActionMap    *map,
	 * 						gpointer       user_data );
	 */
	st_signals[ MENU_DEFINITION ] = g_signal_new_class_handler(
				"menu-defined",
				OFA_TYPE_APPLICATION,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

/*
 * ofaIHubber interface management
 */
static void
ihubber_iface_init( ofaIHubberInterface *iface )
{
	static const gchar *thisfn = "ofa_application_ihubber_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ihubber_get_interface_version;
	iface->new_hub = ihubber_new_hub;
	iface->get_hub = ihubber_get_hub;
	iface->clear_hub = ihubber_clear_hub;
}

static guint
ihubber_get_interface_version( const ofaIHubber *instance )
{
	return( 1 );
}

static ofaHub *
ihubber_new_hub( ofaIHubber *instance, const ofaIDBConnect *connect )
{
	ofaApplicationPrivate *priv;

	priv = ofa_application_get_instance_private( OFA_APPLICATION( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	g_clear_object( &priv->hub );
	priv->hub = ofa_hub_new_with_connect( connect );

	return( priv->hub );
}

static ofaHub *
ihubber_get_hub( const ofaIHubber *instance )
{
	ofaApplicationPrivate *priv;

	priv = ofa_application_get_instance_private( OFA_APPLICATION( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->hub );
}

static void
ihubber_clear_hub( ofaIHubber *instance )
{
	ofaApplicationPrivate *priv;

	priv = ofa_application_get_instance_private( OFA_APPLICATION( instance ));

	g_return_if_fail( !priv->dispose_has_run );

	g_clear_object( &priv->hub );
}

/**
 * ofa_application_new:
 *
 * Returns: a newly allocated ofaApplication object.
 */
ofaApplication *
ofa_application_new( void )
{
	static const gchar *thisfn = "ofa_application_new";
	ofaApplication *application;

	application = g_object_new( OFA_TYPE_APPLICATION,

			/* GApplication properties */
			"application-id",          st_application_id,
			"flags",                   G_APPLICATION_NON_UNIQUE,

			/* ofaApplication properties */
			OFA_PROP_OPTIONS,          st_option_entries,
			OFA_PROP_APPLICATION_NAME, gettext( st_application_name ),
			OFA_PROP_DESCRIPTION,      gettext( st_description ),
			OFA_PROP_ICON_NAME,        gettext( st_icon_name ),
			NULL );

	ofa_box_register_types();

	if( ofa_plugin_load_modules( G_APPLICATION( application )) == -1 ){
		g_error( "%s: unable to initialize application, aborting", thisfn );
		g_clear_object( &application );
	}

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

	priv = ofa_application_get_instance_private( application );

	g_return_val_if_fail( !priv->dispose_has_run, OFA_EXIT_CODE_PROGRAM );

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

	return( priv->code );
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
	g_debug( "%s: binding to utf-8", thisfn );
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
	ofaApplicationPrivate *priv;
	gboolean ret;
	char *parameter_string;
	GError *error;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	priv = ofa_application_get_instance_private( application );

	/* manage command-line arguments
	 */
	if( priv->options ){
		parameter_string = g_strdup( g_get_application_name());
		error = NULL;
		ret = gtk_init_with_args(
				&priv->argc,
				( char *** ) &priv->argv,
				parameter_string,
				priv->options,
				GETTEXT_PACKAGE,
				&error );
		if( error ){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
			ret = FALSE;
			priv->code = OFA_EXIT_CODE_ARGS;
		}
		g_free( parameter_string );

	} else {
		ret = gtk_init_check(
				&priv->argc,
				( char *** ) &priv->argv );
		if( !ret ){
			g_warning( "%s", _( "Erreur à l'interprétation des arguments en ligne de commande" ));
			priv->code = OFA_EXIT_CODE_ARGS;
		}
	}

	return( ret );
}

static gboolean
manage_options( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_application_manage_options";
	gboolean ret;
	GDate date;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	ret = TRUE;

	/* display the program version ?
	 * if yes, then stops here
	 */
	if( st_version_opt ){
		on_version( application );
		ret = FALSE;

	/* for opening a dossier, minimal data is dossier name
	 * begin/end dates must be valid if specified */
	} else if( st_dossier_name_opt ){
		if( st_dossier_begin_opt ){
			my_date_set_from_str( &date, st_dossier_begin_opt, MY_DATE_YYMD );
			if( !my_date_is_valid( &date )){
				g_warning( "%s: invalid date: %s", thisfn, st_dossier_begin_opt );
				ret = FALSE;
			}
		}
		if( st_dossier_end_opt ){
			my_date_set_from_str( &date, st_dossier_end_opt, MY_DATE_YYMD );
			if( !my_date_is_valid( &date )){
				g_warning( "%s: invalid date: %s", thisfn, st_dossier_end_opt );
				ret = FALSE;
			}
		}
	} else {
		/* if dossier name is not specified */
		if( st_dossier_begin_opt ){
			g_warning( "%s: invalid begin date (%s) while dossier name is not specified",
					thisfn, st_dossier_begin_opt );
			ret = FALSE;
		}
		if( st_dossier_end_opt ){
			g_warning( "%s: invalid end date (%s) while dossier name is not specified",
					thisfn, st_dossier_end_opt );
			ret = FALSE;
		}
		if( st_dossier_user_opt ){
			g_warning( "%s: invalid account (%s) while dossier name is not specified",
					thisfn, st_dossier_user_opt );
			ret = FALSE;
		}
		if( st_dossier_passwd_opt ){
			g_warning( "%s: invalid password (%s) while dossier name is not specified",
					thisfn, st_dossier_begin_opt );
			ret = FALSE;
		}
	}

	return( ret );
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
	ofaApplicationPrivate *priv;
	GtkBuilder *builder;
	GMenuModel *menu;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));
	appli = OFA_APPLICATION( application );
	priv = ofa_application_get_instance_private( appli );

	/* chain up to the parent class */
	if( G_APPLICATION_CLASS( ofa_application_parent_class )->startup ){
		G_APPLICATION_CLASS( ofa_application_parent_class )->startup( application );
	}

	/* initialize the application settings */
	ofa_settings_new();

	/* define the application actions */
	g_action_map_add_action_entries(
			G_ACTION_MAP( appli ),
	        st_app_entries, G_N_ELEMENTS( st_app_entries ),
	        ( gpointer ) appli );

	/* define a traditional menubar
	 * the program will abort if GtkBuilder is not able to be parsed
	 * from the given file
	 */
	builder = gtk_builder_new_from_resource( st_resource_appmenu );
	menu = G_MENU_MODEL( gtk_builder_get_object( builder, st_appmenu_id ));
	if( menu ){
		priv->menu = g_object_ref( menu );
		g_debug( "%s: menu successfully loaded from %s at %p: items=%d",
				thisfn, st_resource_appmenu, ( void * ) menu, g_menu_model_get_n_items( menu ));

		/* store the references to the plugins placeholders */
		appli_store_ref( appli, builder, "plugins_app_dossier" );
		appli_store_ref( appli, builder, "plugins_app_misc" );

	} else {
		g_warning( "%s: unable to find '%s' object in '%s' resource", thisfn, st_appmenu_id, st_resource_appmenu );
	}
	g_object_unref( builder );

	/* let the plugins update these menu map/model */
	g_signal_emit_by_name( application, "menu-defined", application );

	/* setup the monitoring of action items */
	setup_actions_monitor( OFA_APPLICATION( application ));

	/* takes the ownership on the dossier store so that we are sure
	 * it will be available during the run */
	priv->dos_store = ofa_dossier_store_new( priv->file_dir );
}

/*
 * stores against the @application GObject the data needed later by the
 * plugins to be able to update the menus
 */
static void
appli_store_ref( ofaApplication *application, GtkBuilder *builder, const gchar *placeholder )
{
	static const gchar *thisfn = "ofa_application_appli_store_ref";
	GObject *menu;

	menu = gtk_builder_get_object( builder, placeholder );
	if( !menu ){
		g_warning( "%s: unable to find '%s' placeholder", thisfn, placeholder );
	} else {
		/* gtk+/examples/plugman.c uses g_object_set_data_full(), but
		 *  menu appears to be unreffed with its parent GMenuModel */
		g_object_set_data( G_OBJECT( application ), placeholder, menu );
	}
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
	ofaApplicationPrivate *priv;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;
	GDate dbegin, dend;
	gchar *str;

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	g_return_if_fail( OFA_IS_APPLICATION( application ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( application ));

	priv->main_window = ofa_main_window_new( OFA_APPLICATION( application ));
	g_debug( "%s: main window instanciated at %p", thisfn, priv->main_window );

	gtk_window_present( GTK_WINDOW( priv->main_window ));

	if( 0 ){
		maintainer_test_function();
	}

	/* if a dossier is to be opened due to options specified in the
	 * command-line */
	if( st_dossier_name_opt ){
		meta = ofa_file_dir_get_meta( priv->file_dir, st_dossier_name_opt );
		period = NULL;
		if( meta ){
			if( !st_dossier_begin_opt && !st_dossier_end_opt ){
				period = ofa_idbmeta_get_current_period( meta );
			} else {
				my_date_set_from_str( &dbegin, st_dossier_begin_opt, MY_DATE_YYMD );
				my_date_set_from_str( &dend, st_dossier_end_opt, MY_DATE_YYMD );
				period = ofa_idbmeta_get_period( meta, &dbegin, &dend );
				if( !period ){
					str = g_strdup_printf(
							_( "Unable to find a financial period with specified dates (begin=%s, end=%s)" ),
							st_dossier_begin_opt, st_dossier_end_opt );
					my_utils_dialog_warning( str );
					g_free( str );
				}
			}
		} else {
			str = g_strdup_printf( _( "Unable to find the '%s' dossier"), st_dossier_name_opt );
			my_utils_dialog_warning( str );
			g_free( str );
		}
		if( meta && period ){
			ofa_dossier_open_run(
					priv->main_window, meta, period, st_dossier_user_opt, st_dossier_passwd_opt );
		}
		g_clear_object( &period );
		g_clear_object( &meta );
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
 * that it supports opening files with the G_APPLICATION_HANDLES_OPEN
 * GApplicationFlag.
 *
 * Openbook: as the G_APPLICATION_HANDLES_OPEN flag is not set, then
 * this function should never be called.
 */
static void
application_open( GApplication *application, GFile **files, gint n_files, const gchar *hint )
{
	static const gchar *thisfn = "ofa_application_open";

	g_warning( "%s: application=%p, n_files=%d, hint=%s: unexpected run here",
			thisfn, ( void * ) application, n_files, hint );
}

static void
maintainer_test_function( void )
{
#if 0
	static const gchar *thisfn = "ofa_application_maintainer_test_function";

	gchar *cstr = "GRANT ALL PRIVILEGES ON `ofat`.* TO 'ofat'@'localhost' WITH GRANT OPTION";
	gchar *prev_dbname = "ofat";
	gchar *dbname = "ofat_3";
	GRegex *regex;
	gchar *str = g_strdup_printf( " `%s`\\.\\* ", prev_dbname );
	g_debug( "%s: str='%s'", thisfn, str );
	regex = g_regex_new( str, 0, 0, NULL );
	g_free( str );
	/*str = g_strdup_printf( "\\1%s", dbname );*/
	str = g_strdup_printf( " `%s`.* ", dbname );
	g_debug( "%s: str=%s", thisfn, str );
	if( g_regex_match( regex, cstr, 0, NULL )){
		gchar *query = g_regex_replace( regex, cstr, -1, 0, str, 0, NULL );
		g_debug( "%s: cstr=%s", thisfn, cstr );
		g_debug( "%s: query=%s", thisfn, query );
	}
#endif
}
/*                                                                   */
/*                                                                   */
/******************* END OF APPLICATION STARTUP **********************/

/***** Starting here with functions which handle the menu options ****/
/*                                                                   */
/*                                                                   */
static void
setup_actions_monitor( ofaApplication *application )
{
	ofaApplicationPrivate *priv;

	priv = ofa_application_get_instance_private( application );

	/* dossiers directory monitoring
	 */
	priv->file_dir = ofa_file_dir_new();
	g_signal_connect(
			priv->file_dir, "changed", G_CALLBACK( on_file_dir_changed ), application );
	on_file_dir_changed(
			priv->file_dir, ofa_file_dir_get_dossiers_count( priv->file_dir ), NULL, application );
}

static void
on_file_dir_changed( ofaFileDir *dir, guint count, const gchar *filename, ofaApplication *application )
{
	static const gchar *thisfn = "ofa_application_on_filed_dir_changed";

	g_debug( "%s: dir=%p, count=%u, filename=%s, application=%p",
			thisfn, ( void * ) dir, count, filename, ( void * ) application );

	enable_action_open( application, count > 0 );
}

static void
enable_action_open( ofaApplication *application, gboolean enable )
{
	ofaApplicationPrivate *priv;

	priv = ofa_application_get_instance_private( application );

	my_utils_action_enable( G_ACTION_MAP( application ), &priv->action_open, "open", enable );
}

static void
on_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_manage";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_dossier_manager_run( priv->main_window );
}

static void
on_new( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_new";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_dossier_new_run( priv->main_window );
}

static void
on_open( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_open";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_dossier_open_run( priv->main_window, NULL, NULL, NULL, NULL );
}

static void
on_restore( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_restore";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_restore_assistant_run( priv->main_window );
}

static void
on_user_prefs( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_user_prefs";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_preferences_run( GTK_APPLICATION_WINDOW( priv->main_window ), NULL );
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
		if( !ofa_prefs_appli_confirm_on_quit() || ofa_main_window_is_willing_to_quit( window )){
			gtk_widget_destroy( GTK_WIDGET( window ));
		}
	} else {
		g_application_quit( G_APPLICATION( application ));
	}
}

static void
on_plugin_manage( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_plugin_manage";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_plugin_manager_run( priv->main_window );
}

static void
on_about( GSimpleAction *action, GVariant *parameter, gpointer user_data )
{
	static const gchar *thisfn = "ofa_application_on_about";
	ofaApplicationPrivate *priv;

	g_debug( "%s: action=%p, parameter=%p, user_data=%p",
			thisfn, action, parameter, ( void * ) user_data );

	g_return_if_fail( user_data && OFA_IS_APPLICATION( user_data ));

	priv = ofa_application_get_instance_private( OFA_APPLICATION( user_data ));

	g_return_if_fail( priv->main_window && OFA_IS_MAIN_WINDOW( priv->main_window ));

	ofa_about_run( priv->main_window );
}

static void
on_version( ofaApplication *application )
{
	g_print( "%s v %s\n", PACKAGE_NAME, PACKAGE_VERSION );
	g_print( "%s.\n", ofa_application_get_copyright( application ));

	g_print( "%s is free software, and is provided without any warranty.\n", PACKAGE_NAME );
	g_print( "You may redistribute copies of %s under the terms of the\n", PACKAGE_NAME );
	g_print( "GNU General Public License (see COPYING).\n" );

	g_debug( "Program has been compiled against Glib %d.%d.%d, Gtk+ %d.%d.%d",
			GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
			GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION );
}

/**
 * ofa_application_get_menu_model:
 */
GMenuModel *
ofa_application_get_menu_model( const ofaApplication *application )
{
	ofaApplicationPrivate *priv;

	g_return_val_if_fail( application && OFA_IS_APPLICATION( application ), NULL );

	priv = ofa_application_get_instance_private( application );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->menu );
}

/**
 * ofa_application_get_copyright:
 * @application:
 */
const gchar *
ofa_application_get_copyright( const ofaApplication *application )
{
	ofaApplicationPrivate *priv;

	g_return_val_if_fail( application && OFA_IS_APPLICATION( application ), NULL );

	priv = ofa_application_get_instance_private( application );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( _( "Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)" ));
}

/**
 * ofa_application_get_file_dir:
 * @application:
 *
 * Returns: the #ofaFileDir object.
 *
 * The returned reference is owned by the @application object, and
 * should not be released by the caller.
 */
ofaFileDir *
ofa_application_get_file_dir( const ofaApplication *application )
{
	ofaApplicationPrivate *priv;

	g_return_val_if_fail( application && OFA_IS_APPLICATION( application ), NULL );

	priv = ofa_application_get_instance_private( application );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->file_dir );
}
