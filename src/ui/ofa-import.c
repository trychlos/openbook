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

#include "ui/my-utils.h"
#include "ui/ofa-import.h"
#include "ui/ofa-importer.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-plugin.h"

static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select a file
 *   2   Content  TYPE     Select a type of import
 *   3   Confirm  CONFIRM  Summary of the operations to be done
 *   4   Summary  DONE     After import
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_TYPE,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* type of imported datas
 */
enum {
	TYPE_BANK_ACCOUNT = 1
};

/* private instance data
 */
struct _ofaImportPrivate {
	gboolean         dispose_has_run;

	/* properties
	 */
	ofaMainWindow   *main_window;

	/* internals
	 */
	GtkAssistant    *assistant;
	gboolean         escape_key_pressed;

	/* p1: select file(s)
	 */
	gboolean         p1_page_initialized;
	GtkFileChooser  *p1_chooser;
	GSList          *p1_fnames;				/* selected uris */

	/* p2: select a type of data to be imported
	 */
	gboolean         p2_page_initialized;
	GtkToggleButton *p2_releve;
	gint             p2_type;
	GtkButton       *p2_type_btn;
};

/* class properties
 */
enum {
	OFA_PROP_TOPLEVEL_ID = 1,
};

#define PROP_TOPLEVEL              "dossier-new-prop-toplevel"

static const gchar    *st_ui_xml = PKGUIDIR "/ofa-import.ui";
static const gchar    *st_ui_id  = "ImportAssistant";

G_DEFINE_TYPE( ofaImport, ofa_import, G_TYPE_OBJECT )

static void       do_initialize_assistant( ofaImport *self );
static gboolean   on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaImport *self );
static void       on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaImport *self );
static void       do_prepare_p0_intro( ofaImport *self, GtkWidget *page );
static void       do_prepare_p1_select( ofaImport *self, GtkWidget *page );
static void       do_init_p1_select( ofaImport *self, GtkWidget *page );
static void       on_p1_selection_changed( GtkFileChooser *chooser, ofaImport *self );
static void       on_p1_file_activated( GtkFileChooser *chooser, ofaImport *self );
static void       check_for_p1_complete( ofaImport *self );
static void       do_prepare_p2_type( ofaImport *self, GtkWidget *page );
static void       do_init_p2_type( ofaImport *self, GtkWidget *page );
static void       get_active_type( ofaImport *self );
static void       check_for_p2_complete( ofaImport *self );
static void       do_prepare_p3_confirm( ofaImport *self, GtkWidget *page );
static void       do_init_p3_confirm( ofaImport *self, GtkWidget *page );
static void       check_for_p3_complete( ofaImport *self );
static void       on_apply( GtkAssistant *assistant, ofaImport *self );
static void       on_cancel( GtkAssistant *assistant, ofaImport *self );
static gboolean   is_willing_to_quit( ofaImport *self );
static void       on_close( GtkAssistant *assistant, ofaImport *self );
static void       do_close( ofaImport *self );
static gint       assistant_get_page_num( GtkAssistant *assistant, GtkWidget *page );

static void
import_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_import_finalize";
	ofaImportPrivate *priv;

	g_return_if_fail( OFA_IS_IMPORT( instance ));

	priv = OFA_IMPORT( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_slist_free_full( priv->p1_fnames, ( GDestroyNotify ) g_free );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_parent_class )->finalize( instance );
}

static void
import_dispose( GObject *instance )
{
	ofaImportPrivate *priv;

	g_return_if_fail( OFA_IS_IMPORT( instance ));

	priv = ( OFA_IMPORT( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		gtk_main_quit();
		gtk_widget_destroy( GTK_WIDGET( priv->assistant ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_parent_class )->dispose( instance );
}

static void
import_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_import_constructed";
	ofaImportPrivate *priv;
	GtkBuilder *builder;
	GError *error;

	g_return_if_fail( OFA_IS_IMPORT( instance ));

	priv = OFA_IMPORT( instance )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( ofa_import_parent_class )->constructed ){
			G_OBJECT_CLASS( ofa_import_parent_class )->constructed( instance );
		}

		/* create the GtkAssistant */
		error = NULL;
		builder = gtk_builder_new();
		if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
			priv->assistant = GTK_ASSISTANT( gtk_builder_get_object( builder, st_ui_id ));
			if( priv->assistant ){
				do_initialize_assistant( OFA_IMPORT( instance ));
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
import_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaImportPrivate *priv;

	g_return_if_fail( OFA_IS_IMPORT( instance ));

	priv = OFA_IMPORT( instance )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_TOPLEVEL_ID:
				g_value_set_pointer( value, priv->main_window );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
import_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaImportPrivate *priv;

	g_return_if_fail( OFA_IS_IMPORT( instance ));

	priv = OFA_IMPORT( instance )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_TOPLEVEL_ID:
				priv->main_window = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
ofa_import_init( ofaImport *self )
{
	static const gchar *thisfn = "ofa_import_init";

	g_return_if_fail( OFA_IS_IMPORT( self ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaImportPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_import_class_init( ofaImportClass *klass )
{
	static const gchar *thisfn = "ofa_import_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = import_get_property;
	G_OBJECT_CLASS( klass )->set_property = import_set_property;
	G_OBJECT_CLASS( klass )->constructed = import_constructed;
	G_OBJECT_CLASS( klass )->dispose = import_dispose;
	G_OBJECT_CLASS( klass )->finalize = import_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			OFA_PROP_TOPLEVEL_ID,
			g_param_spec_pointer(
					PROP_TOPLEVEL,
					"Main window",
					"A pointer (not a ref) to the toplevel parent main window",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_import_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_import_run";

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	g_object_new( OFA_TYPE_IMPORT,
			PROP_TOPLEVEL, main_window,
			NULL );

	gtk_main();
}

static void
do_initialize_assistant( ofaImport *self )
{
	static const gchar *thisfn = "ofa_import_do_initialize_assistant";
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
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaImport *self )
{
	gboolean stop = FALSE;

	g_return_val_if_fail( OFA_IS_IMPORT( self ), FALSE );

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
on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaImport *self )
{
	static const gchar *thisfn = "ofa_import_on_prepare";
	gint page_num;

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( GTK_IS_WIDGET( page ));
	g_return_if_fail( OFA_IS_IMPORT( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, page=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) page, ( void * ) self );

		page_num = assistant_get_page_num( assistant, page );
		switch( page_num ){
			case ASSIST_PAGE_INTRO:
				do_prepare_p0_intro( self, page );
				break;

			case ASSIST_PAGE_SELECT:
				do_prepare_p1_select( self, page );
				break;

			case ASSIST_PAGE_TYPE:
				do_prepare_p2_type( self, page );
				break;

			case ASSIST_PAGE_CONFIRM:
				do_prepare_p3_confirm( self, page );
				break;
		}
	}
}

static void
do_prepare_p0_intro( ofaImport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_do_prepare_p0_intro";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));
}

static void
do_prepare_p1_select( ofaImport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_do_prepare_p1_select";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p1_page_initialized ){
		do_init_p1_select( self, page );
		self->private->p1_page_initialized = TRUE;
	}

	check_for_p1_complete( self );
}

static void
do_init_p1_select( ofaImport *self, GtkWidget *page )
{
	ofaImportPrivate *priv;

	priv = self->private;
	priv->p1_chooser =
			GTK_FILE_CHOOSER( gtk_file_chooser_widget_new( GTK_FILE_CHOOSER_ACTION_OPEN ));
	gtk_widget_set_hexpand( GTK_WIDGET( priv->p1_chooser ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( priv->p1_chooser ), TRUE );
	gtk_file_chooser_set_select_multiple( priv->p1_chooser, TRUE );
	g_signal_connect( G_OBJECT( priv->p1_chooser ), "selection-changed", G_CALLBACK( on_p1_selection_changed ), self );
	g_signal_connect( G_OBJECT( priv->p1_chooser ), "file-activated", G_CALLBACK( on_p1_file_activated ), self );

	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( priv->p1_chooser ), 0, 1, 1, 1 );
	gtk_widget_show_all( page );
}

static void
on_p1_selection_changed( GtkFileChooser *chooser, ofaImport *self )
{
	check_for_p1_complete( self );
}

static void
on_p1_file_activated( GtkFileChooser *chooser, ofaImport *self )
{
	check_for_p1_complete( self );
}

static void
check_for_p1_complete( ofaImport *self )
{
	GtkWidget *page;
	ofaImportPrivate *priv;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_SELECT );
	g_slist_free_full( priv->p1_fnames, ( GDestroyNotify ) g_free );
	priv->p1_fnames = gtk_file_chooser_get_uris( priv->p1_chooser );
	gtk_assistant_set_page_complete( priv->assistant, page, g_slist_length( priv->p1_fnames ) > 0 );
}

/*
 * p2: nature of the data to import
 */
static void
do_prepare_p2_type( ofaImport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_do_prepare_p2_type";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p2_page_initialized ){
		do_init_p2_type( self, page );
		self->private->p2_page_initialized = TRUE;
	}

	check_for_p2_complete( self );
}

static void
do_init_p2_type( ofaImport *self, GtkWidget *page )
{
	ofaImportPrivate *priv;

	priv = self->private;
	priv->p2_releve = GTK_TOGGLE_BUTTON(
							my_utils_container_get_child_by_name(
									GTK_CONTAINER( page ), "p2-releve" ));
}

static void
check_for_p2_complete( ofaImport *self )
{
	GtkWidget *page;
	ofaImportPrivate *priv;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_TYPE );

	get_active_type( self );

	gtk_assistant_set_page_complete( priv->assistant, page, priv->p2_type > 0 );
}

static void
get_active_type( ofaImport *self )
{
	ofaImportPrivate *priv;

	priv = self->private;

	if( gtk_toggle_button_get_active( priv->p2_releve )){
		priv->p2_type_btn = GTK_BUTTON( priv->p2_releve );
		priv->p2_type = TYPE_BANK_ACCOUNT;
		return;
	}
}

/*
 * ask the user to confirm the operation
 */
static void
do_prepare_p3_confirm( ofaImport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_do_prepare_p3_confirm";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	do_init_p3_confirm( self, page );
	check_for_p3_complete( self );
}

static void
do_init_p3_confirm( ofaImport *self, GtkWidget *page )
{
	ofaImportPrivate *priv;
	GtkWidget *widget;
	GtkGrid *grid;
	GtkLabel *label;
	gchar *markup;
	gint row;
	GSList *i;

	priv = self->private;

	widget = gtk_grid_get_child_at( GTK_GRID( page ), 0, 0 );
	if( widget ){
		gtk_widget_destroy( widget );
	}

	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( grid ), 0, 0, 1, 1 );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b>%s</b> :", _( "Files to be imported :" ));
	gtk_label_set_markup( label, markup );
	g_free( markup );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	for( i=priv->p1_fnames, row=0 ; i ; i=i->next ){
		label = GTK_LABEL( gtk_label_new( i->data ));
		gtk_misc_set_alignment( GTK_MISC( label ), 0, 0 );
		gtk_label_set_line_wrap( label, TRUE );
		gtk_grid_attach( grid, GTK_WIDGET( label ), 1, row++, 1, 1 );
	}

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b>%s</b> :", _( "Type of data :" ));
	gtk_label_set_markup( label, markup );
	g_free( markup );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_widget_set_margin_top( GTK_WIDGET( label ), 6 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, row, 1, 1 );

	label = GTK_LABEL( gtk_label_new( gtk_button_get_label( priv->p2_type_btn )));
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_widget_set_margin_top( GTK_WIDGET( label ), 6 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 1, row, 1, 1 );

	gtk_widget_show_all( page );
}

static void
check_for_p3_complete( ofaImport *self )
{
	ofaImportPrivate *priv;
	GtkWidget *page;

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_CONFIRM );
	gtk_assistant_set_page_complete( priv->assistant, page, TRUE );
}

static void
on_apply( GtkAssistant *assistant, ofaImport *self )
{
	static const gchar *thisfn = "ofa_import_on_apply";
	guint count, i;

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_IMPORT( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		count = ofa_importer_import_from_uris(
						ofa_main_window_get_dossier( self->private->main_window ),
						self->private->p1_fnames );

		for( i=0 ; i<count ; ++i ){
			g_warning( "%s: TO BE WRITTEN", thisfn );
		}
	}
}

/*
 * the "cancel" message is sent when the user click on the "Cancel"
 * button, or if he hits the 'Escape' key and the 'Quit on escape'
 * preference is set
 */
static void
on_cancel( GtkAssistant *assistant, ofaImport *self )
{
	static const gchar *thisfn = "ofa_import_on_cancel";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_IMPORT( self ));

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
is_willing_to_quit( ofaImport *self )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( self->private->assistant ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			_( "Are you sure you want to quit this assistant ?" ));

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_QUIT, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static void
on_close( GtkAssistant *assistant, ofaImport *self )
{
	static const gchar *thisfn = "ofa_import_on_close";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_IMPORT( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		do_close( self );
	}
}

static void
do_close( ofaImport *self )
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
