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
#include <stdlib.h>

#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-utils.h"
#include "core/ofa-settings.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-main-window.h"

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

/* private instance data
 */
struct _ofaDossierNewPrivate {

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
	gint           p2_port_num;
	gchar         *p2_socket;
	gchar         *p2_account;
	gchar         *p2_password;

	/* p3: enter administrative account and password
	 */
	gboolean       p3_page_initialized;
	gchar         *p3_account;
	gchar         *p3_password;
	gchar         *p3_bis;
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-dossier-new.ui";
static const gchar *st_ui_id  = "DossierNewAssistant";

G_DEFINE_TYPE( ofaDossierNew, ofa_dossier_new, MY_TYPE_ASSISTANT )

static ofaOpenDossier *st_ood;

static void       on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaDossierNew *self );
static void       do_prepare_p0_intro( ofaDossierNew *self, GtkWidget *page );
static void       do_prepare_p1_dossier( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p1_dossier( ofaDossierNew *self, GtkWidget *page );
static void       on_p1_name_changed( GtkEntry *widget, ofaDossierNew *self );
static void       check_for_p1_complete( ofaDossierNew *self );
static void       do_prepare_p2_dbinfos( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p2_dbinfos( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p2_item( ofaDossierNew *self, GtkWidget *page, const gchar *field_name, gchar **var, GCallback fn, GList **focus );
static void       on_p2_dbname_changed( GtkEntry *widget, ofaDossierNew *self );
static void       on_p2_var_changed( GtkEntry *widget, gchar **var );
static void       on_p2_account_changed( GtkEntry *widget, ofaDossierNew *self );
static void       on_p2_password_changed( GtkEntry *widget, ofaDossierNew *self );
static void       check_for_p2_complete( ofaDossierNew *self );
static void       do_prepare_p3_account( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p3_account( ofaDossierNew *self, GtkWidget *page );
static void       on_p3_account_changed( GtkEntry *entry, ofaDossierNew *self );
static void       on_p3_password_changed( GtkEntry *entry, ofaDossierNew *self );
static void       on_p3_bis_changed( GtkEntry *entry, ofaDossierNew *self );
static void       check_for_p3_complete( ofaDossierNew *self );
static void       do_prepare_p4_confirm( ofaDossierNew *self, GtkWidget *page );
static void       do_init_p4_confirm( ofaDossierNew *self, GtkWidget *page );
static void       display_p4_param( GtkWidget *page, const gchar *field_name, const gchar *value, gboolean display );
static void       check_for_p4_complete( ofaDossierNew *self );
static void       on_apply( GtkAssistant *assistant, ofaDossierNew *self );
static gboolean   make_db_global( ofaDossierNew *self, GtkAssistant *assistant );
static gboolean   setup_new_dossier( ofaDossierNew *self );
static gboolean   create_db_model( ofaDossierNew *self, GtkAssistant *assistant );

static void
dossier_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_finalize";
	ofaDossierNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	priv = OFA_DOSSIER_NEW( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
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

	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->finalize( instance );
}

static void
dossier_new_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_init( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_init";

	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaDossierNewPrivate, 1 );

	/* be sure to initialize this static data each time we run the
	 * assistant */
	st_ood = NULL;
}

static void
ofa_dossier_new_class_init( ofaDossierNewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_finalize;
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 *
 * Rationale: the
 */
ofaOpenDossier *
ofa_dossier_new_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_new_new";
	ofaDossierNew *self;
	GtkAssistant *assistant;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_val_if_fail( assistant && GTK_IS_ASSISTANT( assistant ), NULL );

	g_signal_connect( G_OBJECT( assistant ), "prepare", G_CALLBACK( on_prepare ), self );
	g_signal_connect( G_OBJECT( assistant ), "apply", G_CALLBACK( on_apply ), self );

	my_assistant_run( MY_ASSISTANT( self ));

	return( st_ood );
}

/*
 * the provided 'page' is the toplevel widget of the asistant's page
 */
static void
on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_prepare";
	gint page_num;

	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));
	g_return_if_fail( self && OFA_IS_DOSSIER_NEW( self ));

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

		g_debug( "%s: assistant=%p, page=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) page, ( void * ) self );

		page_num = my_assistant_get_page_num( MY_ASSISTANT( self ), page );
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

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-name" );
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
	GtkAssistant *assistant;

	priv = self->private;

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));

	page = gtk_assistant_get_nth_page( assistant, ASSIST_PAGE_DOSSIER );
	gtk_assistant_set_page_complete( assistant, page,
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
	GList *focus;
	/*GtkWidget *grid;*/

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	g_return_if_fail( GTK_IS_BOX( page ));

	focus = NULL;
	do_init_p2_item( self, page, "p2-dbname",   NULL,                      G_CALLBACK( on_p2_dbname_changed ),   &focus );
	do_init_p2_item( self, page, "p2-host",     &self->private->p2_host,   G_CALLBACK( on_p2_var_changed ),      &focus );
	do_init_p2_item( self, page, "p2-port",     &self->private->p2_port,   G_CALLBACK( on_p2_var_changed ),      &focus );
	do_init_p2_item( self, page, "p2-socket",   &self->private->p2_socket, G_CALLBACK( on_p2_var_changed ),      &focus );
	do_init_p2_item( self, page, "p2-account",  NULL,                      G_CALLBACK( on_p2_account_changed ),  &focus );
	do_init_p2_item( self, page, "p2-password", NULL,                      G_CALLBACK( on_p2_password_changed ), &focus );

	/*grid = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-grid1" );
	gtk_container_set_focus_chain( GTK_CONTAINER( grid ), g_list_reverse( focus ));*/
}

static void
do_init_p2_item( ofaDossierNew *self,
		GtkWidget *page, const gchar *field_name, gchar **var, GCallback fn, GList **focus )
{
	GtkWidget *entry;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), field_name );
	*focus = g_list_prepend( *focus, entry );
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
	GtkAssistant *assistant;

	priv = self->private;

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));

	page = gtk_assistant_get_nth_page( assistant, ASSIST_PAGE_DBINFOS );
	gtk_assistant_set_page_complete( assistant, page,
			priv->p2_dbname && g_utf8_strlen( priv->p2_dbname, -1 ) &&
			priv->p2_account && g_utf8_strlen( priv->p2_account, -1 ) &&
			priv->p2_password && g_utf8_strlen( priv->p2_password, -1 ));
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
	GList *focus;

	g_debug( "%s: self=%p, page=%p",
			thisfn, ( void * ) self, ( void * ) page );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-account" );
	focus = g_list_prepend( NULL, entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p3_account_changed ), self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-password" );
	focus = g_list_prepend( focus, entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p3_password_changed ), self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-bis" );
	focus = g_list_prepend( focus, entry );
	g_signal_connect( entry, "changed", G_CALLBACK( on_p3_bis_changed ), self );

	gtk_container_set_focus_chain( GTK_CONTAINER( page ), g_list_reverse( focus ));
	g_list_free( focus );
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
	GtkAssistant *assistant;

	priv = self->private;

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));

	page = gtk_assistant_get_nth_page( assistant, ASSIST_PAGE_ACCOUNT );

	are_equals = (( !priv->p3_password && !priv->p3_bis ) ||
			( priv->p3_password && g_utf8_strlen( priv->p3_password, -1 ) &&
				priv->p3_bis && g_utf8_strlen( priv->p3_bis, -1 ) &&
				!g_utf8_collate( priv->p3_password, priv->p3_bis )));

	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" ));
	if( are_equals ){
		gtk_label_set_text( label, "" );
	} else {
		content = g_strdup_printf( "%s",
				_( "The two entered passwords are different each others" ));
		gtk_label_set_text( label, content );
		g_free( content );
		if( !gdk_rgba_parse( &color, "#FF0000" )){
			g_warning( "%s: unable to parse color", thisfn );
		}
		gtk_widget_override_color( GTK_WIDGET( label ), GTK_STATE_FLAG_NORMAL, &color );
	}

	gtk_assistant_set_page_complete( assistant, page,
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

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), field_name );
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
	GtkWidget *page;
	GtkAssistant *assistant;

	assistant = ( GtkAssistant * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( assistant && GTK_IS_ASSISTANT( assistant ));

	page = gtk_assistant_get_nth_page( assistant, ASSIST_PAGE_CONFIRM );
	gtk_assistant_set_page_complete( assistant, page, TRUE );
}

static void
on_apply( GtkAssistant *assistant, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_apply";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_DOSSIER_NEW( self ));

	if( !MY_WINDOW( self )->protected->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		self->private->p2_port_num = 0;
		if( self->private->p2_port && g_utf8_strlen( self->private->p2_port, 1 )){
			self->private->p2_port_num = atoi( self->private->p2_port );
		}

		if( make_db_global( self, assistant )){

			setup_new_dossier( self );

			create_db_model( self,assistant );

			st_ood = g_new0( ofaOpenDossier, 1 );
			st_ood->dossier = g_strdup( self->private->p1_name );
			st_ood->host = g_strdup( self->private->p2_host );
			st_ood->port = self->private->p2_port_num;
			st_ood->socket = g_strdup( self->private->p2_socket );
			st_ood->dbname = g_strdup( self->private->p2_dbname );
			st_ood->account = g_strdup( self->private->p3_account );
			st_ood->password = g_strdup( self->private->p3_password );

			g_signal_emit_by_name(
					MY_WINDOW( self )->protected->main_window,
					OFA_SIGNAL_OPEN_DOSSIER, &st_ood );
		}
	}
}

/*
 * Create the empty database with a global connection to dataserver
 * - create database
 * - create admin user
 * - grant admin user
 */
static gboolean
make_db_global( ofaDossierNew *self, GtkAssistant *assistant )
{
	static const gchar *thisfn = "ofa_dossier_new_make_db_global";
	ofoSgbd *sgbd;
	GString *stmt;
	gboolean db_created;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	db_created = FALSE;
	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofo_sgbd_connect( sgbd,
			self->private->p2_host,
			self->private->p2_port_num,
			self->private->p2_socket,
			"mysql",
			self->private->p2_account,
			self->private->p2_password )){

		gtk_assistant_previous_page( assistant );
		return( FALSE );
	}

	stmt = g_string_new( "" );

	g_string_printf( stmt,
			"CREATE DATABASE %s",
			self->private->p2_dbname );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE TABLE IF NOT EXISTS OFA_T_AUDIT ("
			"	AUD_ID    INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern identifier',"
			"	AUD_STAMP TIMESTAMP              NOT NULL        COMMENT 'Query actual timestamp',"
			"	AUD_QUERY VARCHAR(4096)          NOT NULL        COMMENT 'Query')" );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE USER '%s' IDENTIFIED BY '%s'",
			self->private->p3_account, self->private->p3_password );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"CREATE USER '%s'@'localhost' IDENTIFIED BY '%s'",
			self->private->p3_account, self->private->p3_password );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s' WITH GRANT OPTION",
			self->private->p2_dbname,
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT ALL ON %s.* TO '%s'@'localhost' WITH GRANT OPTION",
			self->private->p2_dbname,
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'",
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	g_string_printf( stmt,
			"GRANT CREATE USER, FILE ON *.* TO '%s'@'localhost'",
			self->private->p3_account );
	if( !ofo_sgbd_query( sgbd, stmt->str )){
		gtk_assistant_previous_page( assistant );
		goto free_stmt;
	}

	db_created = TRUE;

free_stmt:
	g_string_free( stmt, TRUE );
	g_object_unref( sgbd );

	return( db_created );
}

static gboolean
setup_new_dossier( ofaDossierNew *self )
{
	gint port = INT_MIN;

	if( self->private->p2_port_num > 0 ){
		port = self->private->p2_port_num;
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
create_db_model( ofaDossierNew *self, GtkAssistant *assistant )
{
	ofoSgbd *sgbd;
	gboolean model_created;

	model_created = FALSE;
	sgbd = ofo_sgbd_new( SGBD_PROVIDER_MYSQL );

	if( !ofo_sgbd_connect( sgbd,
			self->private->p2_host,
			self->private->p2_port_num,
			self->private->p2_socket,
			self->private->p2_dbname,
			self->private->p2_account,
			self->private->p2_password )){

		gtk_assistant_previous_page( assistant );
		goto free_sgbd;
	}

	if( !ofo_dossier_dbmodel_update( sgbd, self->private->p3_account )){
		gtk_assistant_previous_page( assistant );
		goto free_sgbd;
	}

	model_created = TRUE;

free_sgbd:
	g_object_unref( sgbd );

	return( model_created );
}
