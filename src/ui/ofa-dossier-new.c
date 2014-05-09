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
#include <mysql/mysql.h>
#include <stdlib.h>

#include "ui/ofa-dossier-new.h"
#include "ui/ofa-settings.h"
#include "ui/ofo-dossier.h"

/* Export Assistant
 *
 * pos.  type     title
 * ---   -------  -----------------------------------------------
 *   0   Intro    Introduction
 *   1   Content  Define the dossier
 *   2   Content  Provide connection informations
 *   3   Content  Define first administrative account
 *   4   Confirm  Summary of the operations to be done
 *   5   Summary  After creation: OK
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_DOSSIER,
	ASSIST_PAGE_DBINFOS,
	ASSIST_PAGE_ACCOUNT,
	ASSIST_PAGE_CONFIRM,
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
	gboolean       escape_key_pressed;

	/* p1: enter dossier name and description
	 */
	gboolean       p1_page_initialized;
	gchar         *p1_name;

	/* p2: enter connection and authentification parameters
	 */
	gboolean       p2_page_initialized;
	gchar         *p2_dbname;
	gchar         *p2_host;
	gchar         *p2_port;
	gchar         *p2_socket;
	gchar         *p2_account;
	gchar         *p2_password;

	/* p3: enter administrative account and password
	 */
	gboolean       p3_page_initialized;
	gchar         *p3_account;
	gchar         *p3_password;
	gchar         *p3_bis;

	/* when applying...
	 */
	gboolean       error_when_applied;
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
static void         do_prepare_p1_dossier( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p1_dossier( ofaDossierNew *self, GtkWidget *page );
static void         on_p1_name_changed( GtkEntry *widget, ofaDossierNew *self );
static void         check_for_p1_complete( ofaDossierNew *self );
static void         do_prepare_p2_dbinfos( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p2_dbinfos( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p2_item( ofaDossierNew *self, GtkWidget *page, const gchar *field_name, gchar **var, GCallback fn );
static void         on_p2_dbname_changed( GtkEntry *widget, ofaDossierNew *self );
static void         on_p2_var_changed( GtkEntry *widget, gchar **var );
static void         on_p2_account_changed( GtkEntry *widget, ofaDossierNew *self );
static void         on_p2_password_changed( GtkEntry *widget, ofaDossierNew *self );
static void         check_for_p2_complete( ofaDossierNew *self );
static void         do_prepare_p3_account( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p3_account( ofaDossierNew *self, GtkWidget *page );
static void         on_p3_account_changed( GtkEntry *entry, ofaDossierNew *self );
static void         on_p3_password_changed( GtkEntry *entry, ofaDossierNew *self );
static void         on_p3_bis_changed( GtkEntry *entry, ofaDossierNew *self );
static void         check_for_p3_complete( ofaDossierNew *self );
static void         do_prepare_p4_confirm( ofaDossierNew *self, GtkWidget *page );
static void         do_init_p4_confirm( ofaDossierNew *self, GtkWidget *page );
static void         display_p4_param( GtkWidget *page, const gchar *field_name, const gchar *value, gboolean display );
static void         check_for_p4_complete( ofaDossierNew *self );
static void         on_apply( GtkAssistant *assistant, ofaDossierNew *self );
static gboolean     make_db_global( ofaDossierNew *self );
static gboolean     exec_mysql_query( ofaDossierNew *self, MYSQL *mysql, const gchar *query );
static void         error_on_apply( ofaDossierNew *self, const gchar *stmt, const gchar *error );
static gboolean     setup_new_dossier( ofaDossierNew *self );
static gboolean     create_db_model( ofaDossierNew *self );
static void         on_cancel( GtkAssistant *assistant, ofaDossierNew *self );
static gboolean     is_willing_to_quit( ofaDossierNew *self );
static void         on_close( GtkAssistant *assistant, ofaDossierNew *self );
static void         do_close( ofaDossierNew *self );
static gint         assistant_get_page_num( GtkAssistant *assistant, GtkWidget *page );
static GtkWidget   *container_get_child_by_name( GtkContainer *container, const gchar *name );
#if 0
static GtkWidget   *container_get_child_by_type( GtkContainer *container, GType type );
#endif

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
	ofaDossierNewPrivate *priv;

	g_return_if_fail( OFA_IS_DOSSIER_NEW( window ));

	priv = ( OFA_DOSSIER_NEW( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		gtk_main_quit();
		gtk_widget_destroy( GTK_WIDGET( priv->assistant ));

		g_free( priv->p1_name );

		g_free( priv->p2_dbname );
		g_free( priv->p2_host );
		g_free( priv->p2_port );
		g_free( priv->p2_socket );
		g_free( priv->p2_account );
		g_free( priv->p2_password );

		g_free( priv->p3_account );
		g_free( priv->p3_password );
		g_free( priv->p3_bis );

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

			case ASSIST_PAGE_DOSSIER:
				do_prepare_p1_dossier( self, page );
				break;

			case ASSIST_PAGE_DBINFOS:
				do_prepare_p2_dbinfos( self, page );
				break;

			case ASSIST_PAGE_ACCOUNT:
				do_prepare_p3_account( self, page );
				break;

			case ASSIST_PAGE_CONFIRM:
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
do_prepare_p1_dossier( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p1_dossier";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p1_page_initialized ){
		do_init_p1_dossier( self, page );
		self->private->p1_page_initialized = TRUE;
	}

	check_for_p1_complete( self );
}

static void
do_init_p1_dossier( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p1_dossier";
	GtkWidget *entry;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	entry = container_get_child_by_name( GTK_CONTAINER( page ), "p1-name" );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p1_name_changed ), self );
}

static void
on_p1_name_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gtk_entry_get_text( widget );
	g_free( self->private->p1_name );
	self->private->p1_name = g_strdup( label );

	check_for_p1_complete( self );
}

static void
check_for_p1_complete( ofaDossierNew *self )
{
	GtkWidget *page;
	ofaDossierNewPrivate *priv;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_DOSSIER );
	gtk_assistant_set_page_complete( priv->assistant, page,
			priv->p1_name && g_utf8_strlen( priv->p1_name, -1 ));
}

/*
 * p2: informations required for the connection to the sgbd
 */
static void
do_prepare_p2_dbinfos( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p2_dbinfos";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p2_page_initialized ){
		do_init_p2_dbinfos( self, page );
		self->private->p2_page_initialized = TRUE;
	}

	check_for_p2_complete( self );
}

static void
do_init_p2_dbinfos( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p2_dbinfos";

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	g_return_if_fail( GTK_IS_BOX( page ));

	do_init_p2_item( self, page, "p2-dbname",   NULL,                      G_CALLBACK( on_p2_dbname_changed ));
	do_init_p2_item( self, page, "p2-host",     &self->private->p2_host,   G_CALLBACK( on_p2_var_changed ));
	do_init_p2_item( self, page, "p2-port",     &self->private->p2_port,   G_CALLBACK( on_p2_var_changed ));
	do_init_p2_item( self, page, "p2-socket",   &self->private->p2_socket, G_CALLBACK( on_p2_var_changed ));
	do_init_p2_item( self, page, "p2-account",  NULL,                      G_CALLBACK( on_p2_account_changed ));
	do_init_p2_item( self, page, "p2-password", NULL,                      G_CALLBACK( on_p2_password_changed ));
}

static void
do_init_p2_item( ofaDossierNew *self, GtkWidget *page, const gchar *field_name, gchar **var, GCallback fn )
{
	GtkWidget *entry;

	entry = container_get_child_by_name( GTK_CONTAINER( page ), field_name );
	g_signal_connect( entry, "changed", fn, ( var ? ( gpointer ) var : ( gpointer ) self ));
}

static void
on_p2_dbname_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gtk_entry_get_text( widget );
	g_free( self->private->p2_dbname );
	self->private->p2_dbname = g_strdup( label );

	check_for_p2_complete( self );
}

static void
on_p2_var_changed( GtkEntry *widget, gchar **var )
{
	g_free( *var );
	*var = g_strdup( gtk_entry_get_text( widget ));
}

static void
on_p2_account_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gtk_entry_get_text( widget );
	g_free( self->private->p2_account );
	self->private->p2_account = g_strdup( label );

	check_for_p2_complete( self );
}

static void
on_p2_password_changed( GtkEntry *widget, ofaDossierNew *self )
{
	const gchar *label;

	label = gtk_entry_get_text( widget );
	g_free( self->private->p2_password );
	self->private->p2_password = g_strdup( label );

	check_for_p2_complete( self );
}

/*
 * The p2 page gathers informations required to connect to the server
 * check that mandatory items are not empty
 */
static void
check_for_p2_complete( ofaDossierNew *self )
{
	GtkWidget *page;
	ofaDossierNewPrivate *priv;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_DBINFOS );
	gtk_assistant_set_page_complete( priv->assistant, page,
			priv->p2_dbname && g_utf8_strlen( priv->p2_dbname, -1 ) &&
			priv->p2_account && g_utf8_strlen( priv->p2_account, -1 ) &&
			priv->p2_password && g_utf8_strlen( priv->p2_password, -1 ));

	/* also reinit the last applied status if any */
	self->private->error_when_applied = FALSE;
}

static void
do_prepare_p3_account( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_prepare_p3_account";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p3_page_initialized ){
		do_init_p3_account( self, page );
		self->private->p3_page_initialized = TRUE;
	}

	check_for_p3_complete( self );
}

static void
do_init_p3_account( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p3_account";
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
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_ACCOUNT );

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

	do_init_p4_confirm( self, page );
	check_for_p4_complete( self );
}

static void
do_init_p4_confirm( ofaDossierNew *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_dossier_new_do_init_p4_confirm";

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	display_p4_param( page, "p4-dos-name",        self->private->p1_name,        TRUE );
	display_p4_param( page, "p4-db-name",         self->private->p2_dbname,      TRUE );
	display_p4_param( page, "p4-db-host",         self->private->p2_host,        TRUE );
	display_p4_param( page, "p4-db-port",         self->private->p2_port,        TRUE );
	display_p4_param( page, "p4-db-socket",       self->private->p2_socket,      TRUE );
	display_p4_param( page, "p4-db-account",      self->private->p2_account,     TRUE );
	display_p4_param( page, "p4-db-password",     self->private->p2_password,    FALSE );
	display_p4_param( page, "p4-adm-account",     self->private->p3_account,     TRUE );
	display_p4_param( page, "p4-adm-password",    self->private->p3_password,    FALSE );
}

static void
display_p4_param( GtkWidget *page, const gchar *field_name, const gchar *value, gboolean display )
{
	static const gchar *thisfn = "ofa_dossier_new_display_p4_param";
	GtkWidget *label;

	label = container_get_child_by_name( GTK_CONTAINER( page ), field_name );
	if( label ){
		if( display ){
			gtk_label_set_text( GTK_LABEL( label ), value );
		} else {
			/* i18n: 'Set' here means the password has been set */
			gtk_label_set_text( GTK_LABEL( label ), _( "Set" ));
		}
	} else {
		g_warning( "%s: unable to find '%s' field", thisfn, field_name );
	}
}

static void
check_for_p4_complete( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	GtkWidget *page;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_CONFIRM );

	gtk_assistant_set_page_complete( priv->assistant, page, !self->private->error_when_applied );
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

		if( make_db_global( self )){
			setup_new_dossier( self );
			create_db_model( self );
		}
	}
}

/*
 * Create the empty database with a global connection to dataserver
 * - create database
 * - create admin user
 * - grant to admin user
 */
static gboolean
make_db_global( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_make_db_global";
	MYSQL mysql;
	gint port;
	GString *stmt;
	gboolean db_created;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	/* initialize the MariaDB connection */
	mysql_init( &mysql );
	db_created = FALSE;

	/* connect to the 'mysql' database */
	port = 0;
	if( self->private->p2_port && g_utf8_strlen( self->private->p2_port, 1 )){
		port = atoi( self->private->p2_port );
	}

	if( !mysql_real_connect( &mysql,
			self->private->p2_host,
			self->private->p2_account,
			self->private->p2_password,
			"mysql",
			port,
			self->private->p2_socket,
			CLIENT_MULTI_RESULTS )){

		error_on_apply( self, NULL, mysql_error( &mysql ));
		return( FALSE );
	}

	stmt = g_string_new( "" );

	g_string_printf( stmt,
			"CREATE DATABASE %s",
			self->private->p2_dbname );
	if( !exec_mysql_query( self, &mysql, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE USER '%s' IDENTIFIED BY '%s'",
			self->private->p3_account, self->private->p3_password );
	if( !exec_mysql_query( self, &mysql, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s'@'localhost' WITH GRANT OPTION",
			self->private->p2_dbname,
			self->private->p3_account );
	if( !exec_mysql_query( self, &mysql, stmt->str )){
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'@'localhost'",
			self->private->p3_account );
	if( !exec_mysql_query( self, &mysql, stmt->str )){
		goto free_stmt;
	}

	db_created = TRUE;

free_stmt:
	g_string_free( stmt, TRUE );
	mysql_close( &mysql );

	return( db_created );
}

static gboolean
exec_mysql_query( ofaDossierNew *self, MYSQL *mysql, const gchar *query )
{
	gboolean success;

	success = TRUE;

	if( mysql_query( mysql, query )){
		error_on_apply( self, query, mysql_error( mysql ));
		success = FALSE;
	}

	return( success );
}

static void
error_on_apply( ofaDossierNew *self, const gchar *stmt, const gchar *error )
{
	GtkMessageDialog *dlg;
	gchar *msg;

	if( stmt ){
		msg = g_strdup_printf( "%s\n\n%s", stmt, error );
	} else {
		msg = g_strdup( error );
	}
	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( self->private->assistant ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));
	g_free( msg );
	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));

	self->private->error_when_applied = TRUE;
	gtk_assistant_previous_page( self->private->assistant );
}

static gboolean
setup_new_dossier( ofaDossierNew *self )
{
	gint port = INT_MIN;

	if( self->private->p2_port ){
		port = atoi( self->private->p2_port );
	}

	return(
		ofa_settings_set_dossier(
			self->private->p1_name,
			"Provider",    SETTINGS_TYPE_STRING, "MySQL",
			"Host",        SETTINGS_TYPE_STRING, self->private->p2_host,
			"Port",        SETTINGS_TYPE_INT,    port,
			"Socket",      SETTINGS_TYPE_STRING, self->private->p2_socket,
			"Database",    SETTINGS_TYPE_STRING, self->private->p2_dbname,
			NULL ));
}

static gboolean
create_db_model( ofaDossierNew *self )
{
	MYSQL mysql;
	gint port;

	/* initialize the MariaDB connection */
	mysql_init( &mysql );

	/* connect to the 'mysql' database with the newly created admin
	 * account
	 */
	port = 0;
	if( self->private->p2_port && g_utf8_strlen( self->private->p2_port, 1 )){
		port = atoi( self->private->p2_port );
	}
	if( !mysql_real_connect( &mysql,
			self->private->p2_host,
			self->private->p3_account,
			self->private->p3_password,
			self->private->p2_dbname,
			port,
			self->private->p2_socket,
			CLIENT_MULTI_RESULTS )){

		error_on_apply( self, NULL, mysql_error( &mysql ));
		return( FALSE );
	}

	ofo_dossier_dbmodel_update( &mysql );

	mysql_close( &mysql );
	return( TRUE );
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

#if 0
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
#endif
