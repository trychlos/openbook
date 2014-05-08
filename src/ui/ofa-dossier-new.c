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
#include <glib/gprintf.h>
#include <libgda/libgda.h>
#include <libgda-ui/libgda-ui.h>

#include "ui/ofa-dossier-new.h"

/* Export Assistant
 *
 * pos.  type     title
 * ---   -------  -----------------------------------------------
 *   0   Intro    Introduction
 *   1   Content  Define the new data source, select the provider
 *   2   Content  Provider some provider-specific informations
 *   3   Confirm  Summary of the operations to be done
 *   4   Summary  After creation: summary of the done operations
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_DSN_DEFINITION,
	ASSIST_PAGE_PROVIDER_INFOS,
	ASSIST_PAGE_ADMIN_ACCOUNT,
	ASSIST_PAGE_CONFIRM_BEFORE_CREATION,
	ASSIST_PAGE_DONE
};

/* private class data
 */
struct _ofaDossierNewClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaDossierNewPrivate {
	gboolean           dispose_has_run;

	/* properties
	 */
	ofaMainWindow     *main_window;

	/* internals
	 */
	GtkAssistant       *assistant;
	gboolean            escape_key_pressed;
	/* p1: enter dsn name and description, and select the provider
	 *     the provider name is also its string id
	 */
	gboolean            p1_page_initialized;
	gchar              *p1_ds_name;
	gchar              *p1_ds_description;
	gchar              *p1_provider_name;
	/* p2: enter connection and authentification parameters
	 *     as these depend of the selected provider, data are reset
	 *     when the provider has changed
	 */
	gchar              *p2_provider_prev;		/* previous provider name */
	GdaProviderInfo    *p2_gpi;
	GHashTable         *p2_dsn_params;
	GHashTable         *p2_auth_params;
	GtkWidget          *p2_first;				/* first widget of the page */
	/* p3: enter administrative account and password
	 */
	gboolean            p3_page_initialized;
	gchar              *p3_account;
	gchar              *p3_password;
	gchar              *p3_bis;
	/* when applying...
	 */
	GdaServerOperation *gso;
	GString            *cnc_string;
	GString            *auth_string;
	gboolean            error_when_applied;
};

/* the structure associated with the key in the hash tables
 */
typedef struct {
	gchar     *name;
	gboolean   mandatory;
	GType      type;
	gchar     *s_value;
	gboolean   b_value;
}
  sParamValue;

/* class properties
 */
enum {
	OFA_PROP_0,

	OFA_PROP_TOPLEVEL_ID,

	OFA_PROP_N_PROPERTIES
};

#define PROP_TOPLEVEL                   "dossier-new-prop-toplevel"

#define PARAM_VALUE                     "dossier-data-param-value"

/* pass parameters when building the p4 confirmation page
 */
typedef struct {
	ofaDossierNew *self;
	GtkGrid       *grid;
	gint           row;
}
  p4Params;

/* pass parameters when setting GdaServerOperation values
 */
typedef struct {
	ofaDossierNew      *self;
	GdaServerOperation *gso;
	gboolean            ok;
}
  sGSOValue;

static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;

static const gchar       *st_ui_xml   = PKGUIDIR "/ofa-dossier-new.ui";
static const gchar       *st_ui_id    = "DossierNewAssistant";

static GObjectClass *st_parent_class  = NULL;

static GType        register_type( void );
static void         class_init( ofaDossierNewClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec );
static void         instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec );
static void         instance_constructed( GObject *instance );
static void         instance_dispose( GObject *instance );
static void         instance_finalize( GObject *instance );
static void         do_initialize_assistant( ofaDossierNew *self );
static gboolean     on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaDossierNew *self );
static void         on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaDossierNew *self );
static void         do_prepare_p0_intro( ofaDossierNew *self, GtkWidget *page );
static void         do_prepare_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page );
static void         on_p1_ds_name_changed( GtkEntry *widget, ofaDossierNew *self );
static void         on_p1_provider_changed( GtkComboBox *widget, ofaDossierNew *self );
static void         on_p1_ds_description_changed( GtkEntry *widget, ofaDossierNew *self );
static void         check_for_p1_complete( ofaDossierNew *self );
static void         do_prepare_p2_provider_infos( ofaDossierNew *self, GtkWidget *page );
static void         provider_infos_free( ofaDossierNew *self );
static void         do_init_p2_provider_infos( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p2_provider_labels( GtkGrid *grid, gint row, const gchar *label, const gchar *value );
static void         do_init_p2_provider_params( ofaDossierNew *self, GtkGrid *grid, gint *row, const gchar *title, GdaSet *set, GHashTable **hash );
#if 0
static gboolean     hash_equal( const sParamValue *a, const sParamValue *b );
#endif
static void         hash_destroy_value( sParamValue *v );
static void         on_p2_parm_changed( GtkEntry *widget, ofaDossierNew *self );
static void         on_p2_parm_toggled( GtkSwitch *widget, void *foo, ofaDossierNew *self );
static void         check_for_p2_complete( ofaDossierNew *self );
static void         is_param_value_done( const gchar *key, sParamValue *spv, gint *count );
static void         do_prepare_p3_admin_account( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p3_admin_account( ofaDossierNew *self, GtkWidget *page );
static void         on_p3_account_changed( GtkEntry *entry, ofaDossierNew *self );
static void         on_p3_password_changed( GtkEntry *entry, ofaDossierNew *self );
static void         on_p3_bis_changed( GtkEntry *entry, ofaDossierNew *self );
static void         check_for_p3_complete( ofaDossierNew *self );
#if 0
static void       display_not_equal_passwords( ofaDossierNew *self );
static void       go_back_p3_admin_account( ofaDossierNew *self );
#endif
static void         do_prepare_p4_confirm( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p4_confirm( ofaDossierNew *self, GtkWidget *page );
static void         display_p4_params( const gchar *key, sParamValue *spv, p4Params *str );
static void         add_p4_row( ofaDossierNew *self, GtkGrid *grid, gint *row, const gchar *label, gboolean from_provider, const gchar *value );
static void         check_for_p4_complete( ofaDossierNew *self );
static void         on_apply( GtkAssistant *assistant, ofaDossierNew *self );
static void         build_cnc_string( ofaDossierNew *self );
static void         build_auth_string( ofaDossierNew *self );
static void         build_string_iter( const gchar *key, sParamValue *spv, GString *string );
static gboolean     create_database( ofaDossierNew *self, gchar **message );
static gboolean     set_server_operation_values( ofaDossierNew *self, GdaServerOperation *gso, GHashTable *hash );
static void         set_gso_hash_value( const gchar *key, sParamValue *spv, sGSOValue *sgv );
static gboolean     define_data_source( ofaDossierNew *self, gchar **message );
static void         display_error( ofaDossierNew *self, gchar *message );
static void         on_cancel( GtkAssistant *assistant, ofaDossierNew *self );
static gboolean     is_willing_to_quit( ofaDossierNew *self );
static void         on_close( GtkAssistant *assistant, ofaDossierNew *self );
static void         do_close( ofaDossierNew *self );
static gint         assistant_get_page_num( GtkAssistant *assistant, GtkWidget *page );
static GtkWidget   *container_get_child_by_name( GtkContainer *container, const gchar *name );
static GtkWidget   *container_get_child_by_type( GtkContainer *container, GType type );

GType
ofa_dossier_new_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_dossier_new_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaDossierNewClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaDossierNew ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaDossierNew", &info, 0 );

	return( type );
}

static void
class_init( ofaDossierNewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->get_property = instance_get_property;
	object_class->set_property = instance_set_property;
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	g_object_class_install_property( object_class, OFA_PROP_TOPLEVEL_ID,
			g_param_spec_pointer(
					PROP_TOPLEVEL,
					"Main window",
					"A pointer (not a ref) to the toplevel parent main window",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	klass->private = g_new0( ofaDossierNewClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_dossier_new_instance_init";
	ofaDossierNew *self;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_DOSSIER_NEW( instance );

	self->private = g_new0( ofaDossierNewPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaDossierNew *self;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( object ));
	self = OFA_DOSSIER_NEW( object );

	if( !self->private->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_TOPLEVEL_ID:
				g_value_set_pointer( value, self->private->main_window );
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
	ofaDossierNew *self;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( object ));
	self = OFA_DOSSIER_NEW( object );

	if( !self->private->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_TOPLEVEL_ID:
				self->private->main_window = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_constructed( GObject *window )
{
	static const gchar *thisfn = "ofa_dossier_new_instance_constructed";
	ofaDossierNewPrivate *priv;
	GtkBuilder *builder;
	GError *error;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( window ));

	priv = OFA_DOSSIER_NEW( window )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->constructed ){
			G_OBJECT_CLASS( st_parent_class )->constructed( window );
		}

		/* create the GtkAssistant */
		error = NULL;
		builder = gtk_builder_new();
		if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
			priv->assistant = GTK_ASSISTANT( gtk_builder_get_object( builder, st_ui_id ));
			if( priv->assistant ){
				do_initialize_assistant( OFA_DOSSIER_NEW( window ));
			} else {
				g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
			}
		} else {
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
		g_object_unref( builder );
	}
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_dossier_new_instance_dispose";
	ofaDossierNew *self;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( window ));

	self = OFA_DOSSIER_NEW( window );

	if( !self->private->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		self->private->dispose_has_run = TRUE;

		gtk_main_quit();
		gtk_widget_destroy( GTK_WIDGET( self->private->assistant ));

		g_free( self->private->p1_ds_name );
		g_free( self->private->p1_ds_description );
		g_free( self->private->p1_provider_name );
		provider_infos_free( self );
		g_free( self->private->p3_account );
		g_free( self->private->p3_password );
		g_free( self->private->p3_bis );
		if( self->private->gso ){
			g_object_unref( self->private->gso );
		}
		if( self->private->cnc_string ){
			g_string_free( self->private->cnc_string, TRUE );
		}
		if( self->private->auth_string ){
			g_string_free( self->private->auth_string, TRUE );
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_dossier_new_instance_finalize";
	ofaDossierNew *self;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_DOSSIER_NEW( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_dossier_new_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_new_run";

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	g_object_new( OFA_TYPE_DOSSIER_NEW,
			PROP_TOPLEVEL, main_window,
			NULL );

	gtk_main();
}

static void
do_initialize_assistant( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_do_initialize_assistant";
	GtkAssistant *assistant;

	g_debug( "%s: self=%p (%s)",
			thisfn,
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	assistant = self->private->assistant;

	/* deals with 'Esc' key */
	g_signal_connect( assistant,
			"key-press-event", G_CALLBACK( on_key_pressed_event ), self );

	g_signal_connect( assistant, "prepare", G_CALLBACK( on_prepare ), self );
	g_signal_connect( assistant, "apply",   G_CALLBACK( on_apply ),   self );
	g_signal_connect( assistant, "cancel",  G_CALLBACK( on_cancel ),  self );
	g_signal_connect( assistant, "close",   G_CALLBACK( on_close ),   self );

	gtk_widget_show_all( GTK_WIDGET( assistant ));
}

static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaDossierNew *self )
{
	gboolean stop = FALSE;

	g_return_val_if_fail( OFA_IS_DOSSIER_NEW( self ), FALSE );

	if( !self->private->dispose_has_run ){

		if( event->keyval == GDK_KEY_Escape && pref_quit_on_escape ){

				self->private->escape_key_pressed = TRUE;
				g_signal_emit_by_name( self->private->assistant, "cancel", self );
				stop = TRUE;
		}
	}

	return( stop );
}

/*
 * the provided 'page' is the toplevel widget of the asistant's page
 */
static void
on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_prepare";
	gint page_num;

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( GTK_IS_WIDGET( page ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, page=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) page, ( void * ) self );

		page_num = assistant_get_page_num( assistant, page );
		switch( page_num ){
			case ASSIST_PAGE_INTRO:
				do_prepare_p0_intro( self, page );
				break;

			case ASSIST_PAGE_DSN_DEFINITION:
				do_prepare_p1_dsn_definition( self, page );
				break;

			case ASSIST_PAGE_PROVIDER_INFOS:
				do_prepare_p2_provider_infos( self, page );
				break;

			case ASSIST_PAGE_ADMIN_ACCOUNT:
				do_prepare_p3_admin_account( self, page );
				break;

			case ASSIST_PAGE_CONFIRM_BEFORE_CREATION:
				do_prepare_p4_confirm( self, page );
				break;
		}
	}
}

static void
do_prepare_p0_intro( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p0_intro";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));
}

static void
do_prepare_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p1_dsn_definition";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p1_page_initialized ){
		do_init_p1_dsn_definition( self, page );
		self->private->p1_page_initialized = TRUE;
	}

	check_for_p1_complete( self );
}

static void
do_init_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p1_dsn_definition";
	GtkGrid *grid;
	GtkWidget *widget;
	GtkWidget *entry;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	grid = GTK_GRID( container_get_child_by_name( GTK_CONTAINER( page ), "p1-grid1" ));
	if( grid ){

		/* create a new widget for data source provider selection and
		 * attach it to the grid, taking into account the initial
		 *  selection which doesn't trigger the 'changed' signal
		 */
		widget = gdaui_provider_selector_new();
		gtk_grid_attach( grid, widget, 1, 0, 1, 1 );
		gtk_widget_show( widget );
		on_p1_provider_changed( GTK_COMBO_BOX( widget ), self );
		g_signal_connect( widget, "changed", G_CALLBACK( on_p1_provider_changed ), self );

		entry = container_get_child_by_name( GTK_CONTAINER( page ), "p1-name" );
		g_signal_connect( entry, "changed", G_CALLBACK( on_p1_ds_name_changed ), self );

		entry = container_get_child_by_name( GTK_CONTAINER( page ), "p1-description" );
		g_signal_connect( entry, "changed", G_CALLBACK( on_p1_ds_description_changed ), self );

	} else {
		g_warning( "%s: unable to find 'p1-grid1' widget", thisfn );
	}
}

static void
on_p1_provider_changed( GtkComboBox *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gdaui_provider_selector_get_provider( GDAUI_PROVIDER_SELECTOR( widget ));
	g_free( self->private->p1_provider_name );
	self->private->p1_provider_name = g_strdup( label );

	check_for_p1_complete( self );
}

static void
on_p1_ds_name_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gtk_entry_get_text( widget );
	g_free( self->private->p1_ds_name );
	self->private->p1_ds_name = g_strdup( label );

	check_for_p1_complete( self );
}

static void
on_p1_ds_description_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gtk_entry_get_text( widget );
	g_free( self->private->p1_ds_description );
	self->private->p1_ds_description = g_strdup( label );
}

static void
check_for_p1_complete( ofaDossierNew *self )
{
	GtkWidget *page;
	ofaDossierNewPrivate *priv;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_DSN_DEFINITION );
	gtk_assistant_set_page_complete( priv->assistant, page,
			priv->p1_ds_name && g_utf8_strlen( priv->p1_ds_name, -1 ) &&
			priv->p1_provider_name && g_utf8_strlen( priv->p1_provider_name, -1 ));
}

/*
 * the provider infos page must be rebuilt each time the user changes
 * its provider selection
 */
static void
do_prepare_p2_provider_infos( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p2_provider_infos";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( self->private->p1_provider_name );
	g_return_if_fail( g_utf8_strlen( self->private->p1_provider_name, -1 ));

	if( self->private->p2_provider_prev &&
		g_utf8_collate( self->private->p2_provider_prev, self->private->p1_provider_name )){
			provider_infos_free( self );
	}

	if( !self->private->p2_provider_prev ){
		do_init_p2_provider_infos( self, page );
	}

	if( self->private->p2_first ){
		g_return_if_fail( GTK_IS_WIDGET( self->private->p2_first ));
		gtk_widget_grab_focus( self->private->p2_first );
	}

	check_for_p2_complete( self );
}

static void
provider_infos_free( ofaDossierNew *self )
{
	g_free( self->private->p2_provider_prev );
	self->private->p2_provider_prev = NULL;

	if( self->private->p2_dsn_params ){
		g_hash_table_destroy( self->private->p2_dsn_params );
		self->private->p2_dsn_params = NULL;
	}

	if( self->private->p2_auth_params ){
		g_hash_table_destroy( self->private->p2_auth_params );
		self->private->p2_auth_params = NULL;
	}
}

static void
do_init_p2_provider_infos( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p2_provider_infos";
	GtkGrid *grid;
	GdaProviderInfo *gpi;
	gint row;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	g_return_if_fail( GTK_IS_BOX( page ));

	gpi = gda_config_get_provider_info( self->private->p1_provider_name );
	if( !gpi ){
		g_warning( "%s: unable to get '%s' provider informations",
				thisfn, self->private->p1_provider_name );
		return;
	}

	grid = GTK_GRID( container_get_child_by_type( GTK_CONTAINER( page ), GTK_TYPE_GRID ));
	if( grid ){
		gtk_widget_destroy( GTK_WIDGET( grid ));
	}
	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 3 );
	gtk_grid_set_row_spacing( grid, 5 );
	gtk_box_pack_start( GTK_BOX( page ), GTK_WIDGET( grid ), TRUE, TRUE, 0 );

	row = 0;
	do_init_p2_provider_labels( grid, row++, _( "Provider identifier :" ), gpi->id );
	do_init_p2_provider_labels( grid, row++, _( "Location :" ), gpi->location );
	do_init_p2_provider_labels( grid, row++, _( "Description :" ), gpi->description );

	do_init_p2_provider_params(
			self, grid, &row, _( "DSN Parameters" ), gpi->dsn_params,
			&self->private->p2_dsn_params );
	do_init_p2_provider_params(
			self, grid, &row, _( "Authentification Parameters" ), gpi->auth_params,
			&self->private->p2_auth_params );

	self->private->p2_provider_prev = g_strdup( self->private->p1_provider_name );
	self->private->p2_gpi = gpi;

	gtk_widget_show_all( page );
}

static void
do_init_p2_provider_labels( GtkGrid *grid, gint row, const gchar *label, const gchar *value )
{
	GtkLabel *widget;

	widget = GTK_LABEL( gtk_label_new( label ));
	gtk_widget_set_halign( GTK_WIDGET( widget ), GTK_ALIGN_END );
	gtk_grid_attach( grid, GTK_WIDGET( widget ), 0, row, 1, 1 );

	widget = GTK_LABEL( gtk_label_new( value ));
	gtk_widget_set_halign( GTK_WIDGET( widget ), GTK_ALIGN_START );
	gtk_grid_attach( grid, GTK_WIDGET( widget ), 1, row, 1, 1 );
}

static void
do_init_p2_provider_params( ofaDossierNew *self, GtkGrid *grid, gint *row, const gchar *title, GdaSet *set, GHashTable **hash )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p2_provider_params";
	GtkLabel *label;
	gchar *content;
	GSList *h_node;
	GdaHolder *holder;
	GType type;
	gboolean mandatory;
	const GValue *default_value;
	GtkEntry *entry;
	GtkSwitch *swit;
	sParamValue *spv;
	GtkWidget *widget;
	gchar *h_id, *h_name, *h_description;

	/* Do not display the title if there is no data to print after
	 * For example, SQLite doesn't have any auth parameter
	 */
	*hash = NULL;
	if( !set || !set->holders ){
		return;
	}

	label = GTK_LABEL( gtk_label_new( NULL ));
	content = g_strdup_printf( "<b>%s</b>", title );
	gtk_label_set_markup( label, content );
	g_free( content );
	gtk_widget_set_halign( GTK_WIDGET( label ), GTK_ALIGN_START );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, ( *row )++, 2, 1 );

	*hash = g_hash_table_new_full(
			( GHashFunc ) g_str_hash,
			( GEqualFunc ) g_str_equal,
			( GDestroyNotify ) g_free,
			( GDestroyNotify ) hash_destroy_value );

	/* With MySQL and SQLite, we have three different value types:
	 * - gchararray
	 * - gboolean
	 * - gint.
	 * It happens that none of them have any default value
	 */
	for( h_node=set->holders ; h_node ; h_node=g_slist_next( h_node )){
		holder = GDA_HOLDER( h_node->data );

		type = gda_holder_get_g_type( holder );
		mandatory = gda_holder_get_not_null( holder );
		default_value = gda_holder_get_default_value( holder );

		g_object_get( G_OBJECT( holder ),
				"id",          &h_id,
				"name",        &h_name,
				"description", &h_description,
				NULL );

		g_debug( "%s: holder id='%s', name='%s', description='%s'",
				thisfn, h_id, h_name, h_description );

		label = GTK_LABEL( gtk_label_new( NULL ));
		content = g_strdup_printf( "<i>%s%s</i> :", h_name, mandatory ? " (*)":"" );
		gtk_label_set_markup( label, content );
		g_free( content );
		gtk_widget_set_halign( GTK_WIDGET( label ), GTK_ALIGN_END );
		gtk_grid_attach( grid, GTK_WIDGET( label ), 0, *row, 1, 1 );

		spv = g_new0( sParamValue, 1 );
		g_hash_table_insert( *hash, g_strdup( h_id ), spv );
		spv->name = g_strdup( h_name );
		spv->mandatory = mandatory;
		spv->type = type;

		widget = NULL;
		content = g_strdup_printf( "%s", "" );
		switch( type ){
			case G_TYPE_STRING:
				if( default_value ){
					g_free( content );
					content = g_strdup_printf( "%s", g_value_get_string( default_value ));
				}
				break;

			case G_TYPE_INT:
				if( default_value ){
					g_free( content );
					content = g_strdup_printf( "%d", g_value_get_int( default_value ));
				}
				break;
		}

		switch( type ){
			case G_TYPE_STRING:
			case G_TYPE_INT:
				entry = GTK_ENTRY( gtk_entry_new());
				/* TODO #22 bad hack */
				if( !g_ascii_strcasecmp( h_id, "PASSWORD" )){
					gtk_entry_set_visibility( entry, FALSE );
				}
				gtk_entry_set_text( entry, content );
				spv->s_value = g_strdup( content );
				g_free( content );
				widget = GTK_WIDGET( entry );
				g_signal_connect( entry, "changed", G_CALLBACK( on_p2_parm_changed ), self );
				break;

			case G_TYPE_BOOLEAN:
				swit = GTK_SWITCH( gtk_switch_new());
				gtk_switch_set_active( swit, FALSE );
				spv->b_value = FALSE;
				if( default_value ){
					gtk_switch_set_active( swit, g_value_get_boolean( default_value ));
					spv->b_value = g_value_get_boolean( default_value );
				}
				gtk_widget_set_halign( GTK_WIDGET( swit ), GTK_ALIGN_START );
				widget = GTK_WIDGET( swit );
				g_signal_connect( swit, "notify::active", G_CALLBACK( on_p2_parm_toggled ), self );
				break;
		}
		if( widget ){
			gtk_grid_attach( grid, widget, 1, ( *row )++, 1, 1 );
			gtk_widget_set_tooltip_text( widget, h_description );
			g_object_set_data( G_OBJECT( widget ), PARAM_VALUE, spv );
			if( !self->private->p2_first ){
				self->private->p2_first = widget;
			}
		}

		g_free( h_id );
		g_free( h_name );
		g_free( h_description );
	}
}

/*
 * dsn_params and auth_params are stored as hash tables whose:
 * - key is the param id as a gchar *
 * - value is a sParamValue struc
 */
#if 0
static gboolean
hash_equal( const sParamValue *a, const sParamValue *b )
{
	if( a->s_value && !b->s_value )
		return( FALSE );

	if( !a->s_value && b->s_value )
		return( FALSE );

	if( !a->s_value && !b->s_value )
		return( a->b_value == b->b_value );

	return( g_utf8_collate( a->s_value, b->s_value ) == 0 );
}
#endif

static void
hash_destroy_value( sParamValue *v )
{
	g_free( v->name );
	g_free( v->s_value );
	g_free( v );
}

static void
on_p2_parm_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;
	sParamValue *spv;

	label = gtk_entry_get_text( widget );
	spv = g_object_get_data( G_OBJECT( widget ), PARAM_VALUE );
	g_free( spv->s_value );
	spv->s_value = g_strdup( label );

	check_for_p2_complete( self );
}

/*
 * foo is said to be a GParamBoolean at runtime
 * but this type is not recognized at compilation...
 */
static void
on_p2_parm_toggled( GtkSwitch *widget, void *foo, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_p2_parm_toggled";
	gboolean st;
	sParamValue *spv;

	g_debug( "%s: widget=%p (%s), status=%p (%s), self=%p (%s)",
			thisfn,
			( void * ) widget, G_OBJECT_TYPE_NAME( widget ),
			( void * ) foo, G_OBJECT_TYPE_NAME( foo ),
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	st = gtk_switch_get_active( GTK_SWITCH( widget ));
	spv = g_object_get_data( G_OBJECT( widget ), PARAM_VALUE );
	spv->b_value = st;

	check_for_p2_complete( self );
}

/*
 * The p2 page gathers informations required by the provider in order
 * to be able to create the datasource - count here the number of
 * mandatory items whose value is not set
 */
static void
check_for_p2_complete( ofaDossierNew *self )
{
	GtkWidget *page;
	ofaDossierNewPrivate *priv;
	gint count = 0;

	priv = self->private;
	if( priv->p2_dsn_params ){
		g_hash_table_foreach( priv->p2_dsn_params, ( GHFunc ) is_param_value_done, &count );
	}
	if( priv->p2_auth_params ){
		g_hash_table_foreach( priv->p2_auth_params, ( GHFunc ) is_param_value_done, &count );
	}
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_PROVIDER_INFOS );
	gtk_assistant_set_page_complete( priv->assistant, page, ( count == 0 ));

	/* also reinit the last applied status if any */
	self->private->error_when_applied = FALSE;
}

static void
is_param_value_done( const gchar *key, sParamValue *spv, gint *count )
{
	if( !spv->mandatory ){
		return;
	}
	if( spv->type == G_TYPE_BOOLEAN ){
		return;
	}
	if( !spv->s_value || !g_utf8_strlen( spv->s_value, -1 )){
		*count += 1;
	}
}

static void
do_prepare_p3_admin_account( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p3_admin_account";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p3_page_initialized ){
		do_init_p3_admin_account( self, page );
		self->private->p3_page_initialized = TRUE;
	}

	check_for_p3_complete( self );
}

static void
do_init_p3_admin_account( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p3_admin_account";
	GtkWidget *entry;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	entry = container_get_child_by_name( GTK_CONTAINER( page ), "p3-account" );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p3_account_changed ), self );

	entry = container_get_child_by_name( GTK_CONTAINER( page ), "p3-password" );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p3_password_changed ), self );

	entry = container_get_child_by_name( GTK_CONTAINER( page ), "p3-bis" );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p3_bis_changed ), self );
}

static void
on_p3_account_changed( GtkEntry *entry, ofaDossierNew *self )
{
	const gchar *content;

	content = gtk_entry_get_text( entry );
	g_free( self->private->p3_account );
	self->private->p3_account = g_strdup( content );

	check_for_p3_complete( self );
}

static void
on_p3_password_changed( GtkEntry *entry, ofaDossierNew *self )
{
	const gchar *content;

	content = gtk_entry_get_text( entry );
	g_free( self->private->p3_password );
	self->private->p3_password = g_strdup( content );

	check_for_p3_complete( self );
}

static void
on_p3_bis_changed( GtkEntry *entry, ofaDossierNew *self )
{
	const gchar *content;

	content = gtk_entry_get_text( entry );
	g_free( self->private->p3_bis );
	self->private->p3_bis = g_strdup( content );

	check_for_p3_complete( self );
}

/*
 * the page is supposed complete if the three fields are set
 * and the two entered passwords are equal
 */
static void
check_for_p3_complete( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_check_for_p3_complete";
	GtkWidget *page;
	ofaDossierNewPrivate *priv;
	gboolean are_equals;
	GtkLabel *label;
	gchar *content;
	GdkRGBA color;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_ADMIN_ACCOUNT );

	are_equals = (( !priv->p3_password && !priv->p3_bis ) ||
			( priv->p3_password && g_utf8_strlen( priv->p3_password, -1 ) &&
				priv->p3_bis && g_utf8_strlen( priv->p3_bis, -1 ) &&
				!g_utf8_collate( priv->p3_password, priv->p3_bis )));

	label = GTK_LABEL( container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" ));
	if( are_equals ){
		gtk_label_set_text( label, "" );
	} else {
		content = g_strdup_printf( "%s",
				_( "Error: the passwords you have entered are not equal" ));
		gtk_label_set_text( label, content );
		g_free( content );
		if( !gdk_rgba_parse( &color, "#FF0000" )){
			g_warning( "%s: unabel to parse color", thisfn );
		}
		gtk_widget_override_color( GTK_WIDGET( label ), GTK_STATE_FLAG_NORMAL, &color );
	}

	gtk_assistant_set_page_complete( priv->assistant, page,
			priv->p3_account && g_utf8_strlen( priv->p3_account, -1 ) &&
			priv->p3_password &&
			priv->p3_bis &&
			are_equals );
}

#if 0
static void
display_not_equal_passwords( ofaDossierNew *self )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( self->private->assistant ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK,
			_( "The passwords you have entered are not equal. Please re-enter them." ));

	gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );
}

static void
go_back_p3_admin_account( ofaDossierNew *self )
{
	/*GtkWidget *page;
	GtkWidget *passwd_entry;

	page = gtk_assistant_get_nth_page( self->private->assistant, ASSIST_PAGE_ADMIN_ACCOUNT );
	passwd_entry = container_get_child_by_name( GTK_CONTAINER( page ), "p3-password" );
	gtk_widget_grab_focus( passwd_entry );
	gtk_editable_select_region( GTK_EDITABLE( passwd_entry ), 0, -1 );*/

	gtk_assistant_set_current_page( self->private->assistant, ASSIST_PAGE_ADMIN_ACCOUNT );
}
#endif

/*
 * ask the user to confirm the creation of the new dossier
 *
 * when preparing this page, we first check that the two passwords
 * entered in the previous page are equal ; if this is not the case,
 * then we display an error message and force a go back to the previous
 * page
 */
static void
do_prepare_p4_confirm( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p4_confirm";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->private;

	g_return_if_fail( priv->p3_password && g_utf8_strlen( priv->p3_password, -1 ));
	g_return_if_fail( priv->p3_bis && g_utf8_strlen( priv->p3_bis, -1 ));

#if 0
	if( g_utf8_collate( priv->p3_password, priv->p3_bis )){
		display_not_equal_passwords( self );
		go_back_p3_admin_account( self );
		return;
	}
#endif

	do_init_p4_confirm( self, page );
	check_for_p4_complete( self );
}

static void
do_init_p4_confirm( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p4_confirm";
	GtkGrid *grid;
	gint row;
	GtkLabel *label;
	p4Params str;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	grid = GTK_GRID( container_get_child_by_type( GTK_CONTAINER( page ), GTK_TYPE_GRID ));
	if( grid ){
		gtk_widget_destroy( GTK_WIDGET( grid ));
	}
	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_set_column_spacing( grid, 3 );
	gtk_grid_set_row_spacing( grid, 5 );
	gtk_box_pack_start( GTK_BOX( page ), GTK_WIDGET( grid ), TRUE, TRUE, 0 );
	row = 0;

	label = GTK_LABEL( gtk_label_new(
			_( "You are about to create a new dossier, "
					"based on a new datasource with the following characteristics:" )));
	gtk_label_set_line_wrap( label, TRUE );
	gtk_widget_set_halign( GTK_WIDGET( label ), GTK_ALIGN_START );
	gtk_misc_set_padding( GTK_MISC( label ), 6, 6 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, row++, 2, 1 );

	add_p4_row( self, grid, &row, _( "Name :" ), FALSE, self->private->p1_ds_name );
	add_p4_row( self, grid, &row, _( "Description :" ), FALSE, self->private->p1_ds_description );
	add_p4_row( self, grid, &row, _( "Data source provider :" ), FALSE, self->private->p1_provider_name );

	str.self = self;
	str.grid = grid;
	str.row = row;
	g_hash_table_foreach( self->private->p2_dsn_params, ( GHFunc ) display_p4_params, &str );
	g_hash_table_foreach( self->private->p2_auth_params, ( GHFunc ) display_p4_params, &str );
	row = str.row;

	add_p4_row( self, grid, &row, _( "Administrative account :" ), FALSE, self->private->p3_account );
	/* i18n: the 'Set' here means that the password for the administrative account has been set */
	add_p4_row( self, grid, &row, _( "Password :" ), FALSE, _( "Set" ));

	label = GTK_LABEL( gtk_label_new(
			_( "Now, just click on 'Apply' button to create the new datasource "
					"and the corresponding new database." )));
	gtk_label_set_line_wrap( label, TRUE );
	gtk_widget_set_halign( GTK_WIDGET( label ), GTK_ALIGN_START );
	gtk_misc_set_padding( GTK_MISC( label ), 6, 6 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, row++, 2, 1 );

	gtk_widget_show_all( page );
}

static void
display_p4_params( const gchar *key, sParamValue *spv, p4Params *str )
{
	gchar *content;

	content = g_strdup_printf( "%s", spv->s_value );
	if( spv->type == G_TYPE_BOOLEAN ){
		g_free( content );
		/* i18n: Yes and No here are textual representations of boolean values
		 *       Off/On might also have been used */
		content = g_strdup_printf( "%s", spv->b_value ? _( "Yes" ):_( "No" ));
	}
	/* TODO #22 bad hack */
	if( !g_ascii_strcasecmp( key, "PASSWORD" )){
		g_free( content );
		/* i18n: the 'Set' here means that the password for the root account has been set */
		content = g_strdup( _( "Set" ));
	}
	add_p4_row( str->self, str->grid, &str->row, spv->name, TRUE, content );
	g_free( content );
}

static void
add_p4_row( ofaDossierNew *self, GtkGrid *grid, gint *row, const gchar *label, gboolean from_provider, const gchar *value )
{
	GtkLabel *widget;
	gchar *content;

	widget = GTK_LABEL( gtk_label_new( NULL ));
	if( from_provider ){
		content = g_strdup_printf( "<i>%s</i> :", label );
	} else {
		content = g_strdup( label );
	}
	gtk_label_set_markup( widget, content );
	g_free( content );
	gtk_widget_set_halign( GTK_WIDGET( widget ), GTK_ALIGN_END );
	gtk_grid_attach( grid, GTK_WIDGET( widget ), 0, *row, 1, 1 );

	widget = GTK_LABEL( gtk_label_new( value ));
	gtk_widget_set_halign( GTK_WIDGET( widget ), GTK_ALIGN_START );
	gtk_grid_attach( grid, GTK_WIDGET( widget ), 1, ( *row )++, 1, 1 );
}

static void
check_for_p4_complete( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	GtkWidget *page;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_CONFIRM_BEFORE_CREATION );

	gtk_assistant_set_page_complete( priv->assistant, page, !self->private->error_when_applied );
}

static void
on_apply( GtkAssistant *assistant, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_apply";
	gchar *message;

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		build_cnc_string( self );
		build_auth_string( self );

		if( !create_database( self, &message ) ||
			!define_data_source( self, &message )){

			display_error( self, message );
			g_free( message );

			self->private->error_when_applied = TRUE;
			gtk_assistant_previous_page( assistant );
		}
	}
}

static void
build_cnc_string( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_build_cnc_string";

	self->private->cnc_string = g_string_new( "" );

	g_hash_table_foreach(
			self->private->p2_dsn_params,
			( GHFunc ) build_string_iter,
			self->private->cnc_string );

	g_debug( "%s: cnc_string='%s'", thisfn, self->private->cnc_string->str );
}

static void
build_auth_string( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_build_auth_string";

	self->private->auth_string = g_string_new( "" );

	g_hash_table_foreach(
			self->private->p2_auth_params,
			( GHFunc ) build_string_iter,
			self->private->auth_string );

	g_debug( "%s: auth_string='%s'", thisfn, self->private->auth_string->str );
}

static void
build_string_iter( const gchar *key, sParamValue *spv, GString *string )
{
	gchar *str;

	if( spv->type == G_TYPE_BOOLEAN ){
		str = g_strdup_printf( "%s", spv->b_value ? _( "Yes" ):_( "No" ));
	} else {
		str = g_strdup( spv->s_value );
	}

	if( g_utf8_strlen( str, -1 )){
		if( string->len ){
			g_string_append( string, ";" );
		}

		g_string_append_printf( string, "%s=%s", key, str );
	}
	g_free( str );
}

static gboolean
create_database( ofaDossierNew *self, gchar **message )
{
	static const gchar *thisfn = "oda_dossier_new_create_data_base";
	GdaServerOperation *gso;
	GError *error;
	sParamValue *spv;

	spv = ( sParamValue * ) g_hash_table_lookup( self->private->p2_dsn_params, "DB_NAME" );
	if( !spv || !spv->s_value || !g_utf8_strlen( spv->s_value, -1 )){
		*message = g_strdup_printf( _( "Unable to find the database name (DB_NAME) among data source parameters" ));
		return( FALSE );
	}

	error = NULL;
	gso = gda_server_operation_prepare_create_database( self->private->p1_provider_name, spv->s_value, &error );
	if( !gso ){
		g_warning( "%s: gda_server_operation_prepare_create_database: %s", thisfn, error->message );
		*message = g_strdup( error->message );
		g_error_free( error );
		return( FALSE );
	}

#if 0
	/* oda_dossier_new_create_data_base: gda_server_operation_op_type_to_string='CREATE_DB' */
	g_debug( "%s: gda_server_operation_op_type_to_string='%s'",
			thisfn, gda_server_operation_op_type_to_string( gda_server_operation_get_op_type( gso )));
#endif

	if( self->private->p2_dsn_params ){
		if( !set_server_operation_values( self, gso, self->private->p2_dsn_params )){
			*message = g_strdup( _( "Unable to set DSN parameters" ));
			return( FALSE );
		}
	}
	if( self->private->p2_auth_params ){
		if( !set_server_operation_values( self, gso, self->private->p2_auth_params )){
			*message = g_strdup( _( "Unable to set authentification parameters" ));
			return( FALSE );
		}
	}

	if( !gda_server_operation_is_valid( gso, NULL, &error )){
		g_warning( "%s: gda_server_operation_is_valid: %s", thisfn, error->message );
		*message = g_strdup( error->message );
		g_error_free( error );
		return( FALSE );
	}

	if( !gda_server_operation_perform_create_database( gso, self->private->p1_provider_name, &error )){
		g_warning( "%s: gda_server_operation_perform_create_database: %s", thisfn, error->message );
		*message = g_strdup( error->message );
		g_error_free( error );
		return( FALSE );
	}

	self->private->gso = gso;
	return( TRUE );
}

static gboolean
set_server_operation_values( ofaDossierNew *self, GdaServerOperation *gso, GHashTable *hash )
{
	sGSOValue *sgv;
	gboolean ok;

	sgv = g_new0( sGSOValue, 1 );
	sgv->self = self;
	sgv->gso = gso;
	sgv->ok = TRUE;
	g_hash_table_foreach( hash, ( GHFunc ) set_gso_hash_value, sgv );
	ok = sgv->ok;
	g_free( sgv );

	return( ok );
}

static void
set_gso_hash_value( const gchar *key, sParamValue *spv, sGSOValue *sgv )
{
	static const gchar *thisfn = "ofa_dossier_new_set_gso_hash_value";
	gchar *content = NULL;
	GError *error = NULL;

	if( spv->type == G_TYPE_BOOLEAN ){
		content = g_strdup_printf( "%s", spv->b_value ? "Yes":"No" );
	} else if( spv->s_value && g_utf8_strlen( spv->s_value, -1 )){
		content = g_strdup( spv->s_value );
	}

	if( content ){
		if( !gda_server_operation_set_value_at( sgv->gso, content, &error, "/SERVER_CNX_P/%s", key )){
			sgv->ok = FALSE;
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
		g_debug( "%s: set value '%s' at path /SERVER_CNX_P/%s", thisfn, content, key );
		g_free( content );
	}
}

static gboolean
define_data_source( ofaDossierNew *self, gchar **message )
{
	static const gchar *thisfn = "ofa_dossier_new_define_data_source";
	GdaDsnInfo info;
	GError *error;
	gboolean ok;

	ok = TRUE;
	error = NULL;
	info.name = self->private->p1_ds_name;
	info.provider = self->private->p1_provider_name;
	info.description = self->private->p1_ds_description;
	info.cnc_string = self->private->cnc_string->str;
	info.auth_string = self->private->auth_string->str;
	info.is_system = FALSE;

	g_debug( "%s: cnc_string=%s", thisfn, self->private->cnc_string->str );
	g_debug( "%s: auth_string=%s", thisfn, self->private->auth_string->str );

	if( !gda_config_define_dsn( &info, &error )){
		g_warning( "%s: %s", thisfn, error->message );
		*message = g_strdup( error->message );
		g_error_free( error );
		ok = FALSE;
	}

	return( ok );
}

static void
display_error( ofaDossierNew *self, gchar *message )
{
	GtkMessageDialog *dlg;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( self->private->assistant ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", message ));
	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}

/*
 * the "cancel" message is sent when the user click on the "Cancel"
 * button, or if he hits the 'Escape' key and the 'Quit on escape'
 * preference is set
 */
static void
on_cancel( GtkAssistant *assistant, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_cancel";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		if(( self->private->escape_key_pressed && ( !pref_confirm_on_escape || is_willing_to_quit( self ))) ||
				!pref_confirm_on_cancel ||
				is_willing_to_quit( self )){

			do_close( self );
		}
	}
}

static gboolean
is_willing_to_quit( ofaDossierNew *self )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( self->private->assistant ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_( "Are you sure you want terminate this assistant ?" ));

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_QUIT, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
on_close( GtkAssistant *assistant, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_close";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		do_close( self );
	}
}

static void
do_close( ofaDossierNew *self )
{
	g_object_unref( self );
}

/*
 * Returns: the index of the given page, or -1 if not found
 */
static gint
assistant_get_page_num( GtkAssistant *assistant, GtkWidget *page )
{
	gint count, i;
	GtkWidget *page_n;

	count = gtk_assistant_get_n_pages( assistant );
	page_n = NULL;
	for( i=0 ; i<count ; ++i ){
		page_n = gtk_assistant_get_nth_page( assistant, i );
		if( page_n == page ){
			return( i );
		}
	}

	return( -1 );
}

static GtkWidget *
container_get_child_by_name( GtkContainer *container, const gchar *name )
{
	GList *children = gtk_container_get_children( container );
	GList *ic;
	GtkWidget *found = NULL;
	GtkWidget *child;
	const gchar *child_name;

	for( ic = children ; ic && !found ; ic = ic->next ){

		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			child_name = gtk_buildable_get_name( GTK_BUILDABLE( child ));
			if( child_name && strlen( child_name ) && !g_ascii_strcasecmp( name, child_name )){
				found = child;
				break;
			}
			if( GTK_IS_CONTAINER( child )){
				found = container_get_child_by_name( GTK_CONTAINER( child ), name );
			}
		}
	}

	g_list_free( children );
	return( found );
}

static GtkWidget *
container_get_child_by_type( GtkContainer *container, GType type )
{
	GList *children = gtk_container_get_children( container );
	GList *ic;
	GtkWidget *found = NULL;
	GtkWidget *child;

	for( ic = children ; ic && !found ; ic = ic->next ){

		if( GTK_IS_WIDGET( ic->data )){
			child = GTK_WIDGET( ic->data );
			if( G_OBJECT_TYPE( ic->data ) == type ){
				found = GTK_WIDGET( ic->data );
				break;
			}
			if( GTK_IS_CONTAINER( child )){
				found = container_get_child_by_type( GTK_CONTAINER( child ), type );
			}
		}
	}

	g_list_free( children );
	return( found );
}
