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
	gboolean       dispose_has_run;

	/* properties
	 */
	ofaMainWindow *main_window;

	/* internals
	 */
	GtkAssistant  *assistant;
};

/* class properties
 */
enum {
	OFA_PROP_0,

	OFA_PROP_TOPLEVEL_ID,

	OFA_PROP_N_PROPERTIES
};

#define PROP_TOPLEVEL                   "dossier-new-prop-toplevel"

static const gchar       *st_ui_xml   = PKGUIDIR "/ofa-assistant-dossier-new.ui";
static const gchar       *st_ui_id    = "DossierNewAssistant";

static GObjectClass *st_parent_class  = NULL;

static GType      register_type( void );
static void       class_init( ofaDossierNewClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec );
static void       instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec );
static void       instance_constructed( GObject *instance );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       do_initialize_assistant( ofaDossierNew *self );
static void       on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaDossierNew *self );
static gint       assistant_get_page_num( GtkAssistant *assistant, GtkWidget *page );
static void       do_prepare_p0_intro( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p0_intro( ofaDossierNew *self, GtkWidget *page );
static void       do_prepare_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page );
static GtkWidget *container_get_child_by_name( GtkContainer *container, const gchar *name );
static void       on_apply( GtkAssistant *assistant, ofaDossierNew *self );
static void       on_cancel( GtkAssistant *assistant, ofaDossierNew *self );
static gboolean   is_willing_to_quit( ofaDossierNew *self );
static void       on_close( GtkAssistant *assistant, ofaDossierNew *self );
static void       do_close( ofaDossierNew *self );

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

	g_signal_connect( assistant, "prepare", G_CALLBACK( on_prepare ), self );
	g_signal_connect( assistant, "apply",   G_CALLBACK( on_apply ),   self );
	g_signal_connect( assistant, "cancel",  G_CALLBACK( on_cancel ),  self );
	g_signal_connect( assistant, "close",   G_CALLBACK( on_close ),   self );

	gtk_widget_show_all( GTK_WIDGET( assistant ));
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
		}
	}
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

static void
do_prepare_p0_intro( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p0_intro";
	static gboolean page_initialized = FALSE;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !page_initialized ){
		do_init_p0_intro( self, page );
		page_initialized = TRUE;
	}
}

static void
do_init_p0_intro( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p0_intro";

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	gtk_assistant_set_page_complete( self->private->assistant, page, TRUE );

	gtk_widget_show_all( page );
}

static void
do_prepare_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p1_dsn_definition";
	static gboolean page_initialized = FALSE;

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !page_initialized ){
		do_init_p1_dsn_definition( self, page );
		page_initialized = TRUE;
	}
}

static void
do_init_p1_dsn_definition( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p1_dsn_definition";
	GtkGrid *grid;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	grid = GTK_GRID( container_get_child_by_name( GTK_CONTAINER( page ), "p1-grid1" ));
	if( grid ){
		widget = gdaui_provider_selector_new();
		gtk_grid_attach( grid, widget, 1,1,1,1 );
	} else {
		g_warning( "%s: unable to find 'p1-grid1' widget", thisfn );
	}

	gtk_widget_show_all( page );
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

static void
on_apply( GtkAssistant *assistant, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_apply";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );
	}
}

static void
on_cancel( GtkAssistant *assistant, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_cancel";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		if( is_willing_to_quit( self )){
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
			_( "Etes-vous sûr de vouloir quitter cet assistant ?" ));

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
