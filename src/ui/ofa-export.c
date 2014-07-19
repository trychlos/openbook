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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "core/my-utils.h"
#include "ui/ofa-export.h"
#include "ui/ofa-main-window.h"
#include "api/ofo-class.h"
#include "api/ofo-account.h"
#include "api/ofo-devise.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-journal.h"
#include "api/ofo-model.h"
#include "api/ofo-taux.h"

static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select the data
 *   2   Content  FORMAT   Select the export format
 *   3   Content  OUTPUT   Select the output file
 *   4   Confirm  CONFIRM  Summary of the operations to be done
 *   5   Summary  DONE     After export
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_FORMAT,
	ASSIST_PAGE_OUTPUT,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* type of exported datas
 */
enum {
	TYPE_ACCOUNT = 1,
	TYPE_CLASS,
	TYPE_DEVISE,
	TYPE_ENTRY,
	TYPE_JOURNAL,
	TYPE_MODEL,
	TYPE_RATE,
	TYPE_DOSSIER,
};

/* data set against each data type radio button */
#define DATA_TYPE                       "ofa-data-type"

/* data set against each export format radio button */
#define DATA_FORMAT                     "ofa-data-format"

/* export format
 */
enum {
	FORMAT_CSV = 1							/* field separator = semi-comma ';' */
};

/* manage radio buttons groups
 */
typedef GSList * (*ExportAsCsv )( const ofoDossier * );

typedef struct {
	const gchar *widget_name;
	gint         data;
}
	RadioGroup;

static RadioGroup st_type_group[] = {
		{ "p1-class",   TYPE_CLASS },
		{ "p1-account", TYPE_ACCOUNT },
		{ "p1-devise",  TYPE_DEVISE },
		{ "p1-journal", TYPE_JOURNAL },
		{ "p1-model",   TYPE_MODEL },
		{ "p1-rate",    TYPE_RATE },
		{ "p1-entries", TYPE_ENTRY },
		{ "p1-dossier", TYPE_DOSSIER },
		{ 0 }
};

static RadioGroup st_format_group[] = {
		{ "p2-csv",     FORMAT_CSV },
		{ 0 }
};

/* manage export files
 */
typedef struct {
	gint         type;
	const gchar *def_fname;
	ExportAsCsv  fn_csv;
}
	ExportDatas;

static const ExportDatas st_export_datas[] = {
		{ TYPE_CLASS,   "/tmp/class.csv",      ofo_class_get_csv },
		{ TYPE_ACCOUNT, "/tmp/accounts.csv",   ofo_account_get_csv },
		{ TYPE_DEVISE,  "/tmp/currencies.csv", ofo_devise_get_csv },
		{ TYPE_JOURNAL, "/tmp/journals.csv",   ofo_journal_get_csv },
		{ TYPE_MODEL,   "/tmp/models.csv",     ofo_model_get_csv },
		{ TYPE_RATE,    "/tmp/rates.csv",      ofo_taux_get_csv },
		{ TYPE_DOSSIER, "/tmp/dossier.csv",    ofo_dossier_get_csv },
		{ TYPE_ENTRY,   "/tmp/entries.csv",    ofo_entry_get_csv },
		{ 0 }
};

/* private instance data
 */
struct _ofaExportPrivate {
	gboolean         dispose_has_run;

	/* properties
	 */
	ofaMainWindow   *main_window;

	/* internals
	 */
	GtkAssistant    *assistant;
	gboolean         escape_key_pressed;

	/* p1: select data
	 */
	gboolean         p1_page_initialized;
	GSList          *p1_group;
	gint             p1_type;				/* the data to export */
	GtkToggleButton *p1_btn;

	/* p2: select format
	 */
	gboolean         p2_page_initialized;
	GSList          *p2_group;
	gint             p2_format;				/* the export format */
	GtkToggleButton *p2_btn;

	/* p3: output file
	 */
	gboolean         p3_page_initialized;
	GtkFileChooser  *p3_chooser;
	gchar           *p3_uri;				/* the output file */
};

/* class properties
 */
enum {
	OFA_PROP_TOPLEVEL_ID = 1,
};

#define PROP_TOPLEVEL              "dossier-new-prop-toplevel"

static const gchar    *st_ui_xml = PKGUIDIR "/ofa-export.ui";
static const gchar    *st_ui_id  = "ExportAssistant";

G_DEFINE_TYPE( ofaExport, ofa_export, G_TYPE_OBJECT )

static void       do_initialize_assistant( ofaExport *self );
static gboolean   on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaExport *self );
static void       on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaExport *self );
static void       do_prepare_p0_intro( ofaExport *self, GtkWidget *page );
static void       do_prepare_p1_type( ofaExport *self, GtkWidget *page );
static void       do_init_p1_type( ofaExport *self, GtkWidget *page );
static void       on_type_toggled( GtkToggleButton *button, ofaExport *self );
static void       get_active_type( ofaExport *self );
static void       check_for_p1_complete( ofaExport *self );
static void       do_prepare_p2_format( ofaExport *self, GtkWidget *page );
static void       do_init_p2_format( ofaExport *self, GtkWidget *page );
static void       on_format_toggled( GtkToggleButton *button, ofaExport *self );
static void       get_active_format( ofaExport *self );
static void       check_for_p2_complete( ofaExport *self );
static void       do_prepare_p3_output( ofaExport *self, GtkWidget *page );
static void       do_init_p3_output( ofaExport *self, GtkWidget *page );
static void       on_p3_selection_changed( GtkFileChooser *chooser, ofaExport *self );
static void       on_p3_file_activated( GtkFileChooser *chooser, ofaExport *self );
static void       check_for_p3_complete( ofaExport *self );
static void       do_prepare_p4_confirm( ofaExport *self, GtkWidget *page );
static void       do_init_p4_confirm( ofaExport *self, GtkWidget *page );
static void       on_apply( GtkAssistant *assistant, ofaExport *self );
static gboolean   apply_export_type( ofaExport *self );
static void       on_cancel( GtkAssistant *assistant, ofaExport *self );
static gboolean   is_willing_to_quit( ofaExport *self );
static void       on_close( GtkAssistant *assistant, ofaExport *self );
static void       do_close( ofaExport *self );
static gint       assistant_get_page_num( GtkAssistant *assistant, GtkWidget *page );

static void
export_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_export_finalize";
	ofaExportPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT( instance ));

	priv = OFA_EXPORT( instance )->private;

	/* free data members here */
	g_free( priv->p3_uri );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_parent_class )->finalize( instance );
}

static void
export_dispose( GObject *instance )
{
	ofaExportPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT( instance ));

	priv = ( OFA_EXPORT( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		gtk_main_quit();
		gtk_widget_destroy( GTK_WIDGET( priv->assistant ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_parent_class )->dispose( instance );
}

static void
export_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_export_constructed";
	ofaExportPrivate *priv;
	GtkBuilder *builder;
	GError *error;

	g_return_if_fail( instance && OFA_IS_EXPORT( instance ));

	priv = OFA_EXPORT( instance )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( ofa_export_parent_class )->constructed ){
			G_OBJECT_CLASS( ofa_export_parent_class )->constructed( instance );
		}

		/* create the GtkAssistant */
		error = NULL;
		builder = gtk_builder_new();
		if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
			priv->assistant = GTK_ASSISTANT( gtk_builder_get_object( builder, st_ui_id ));
			if( priv->assistant ){
				do_initialize_assistant( OFA_EXPORT( instance ));
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
export_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaExportPrivate *priv;

	g_return_if_fail( OFA_IS_EXPORT( instance ));

	priv = OFA_EXPORT( instance )->private;

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
export_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaExportPrivate *priv;

	g_return_if_fail( OFA_IS_EXPORT( instance ));

	priv = OFA_EXPORT( instance )->private;

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
ofa_export_init( ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT( self ));

	self->private = g_new0( ofaExportPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_export_class_init( ofaExportClass *klass )
{
	static const gchar *thisfn = "ofa_export_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = export_get_property;
	G_OBJECT_CLASS( klass )->set_property = export_set_property;
	G_OBJECT_CLASS( klass )->constructed = export_constructed;
	G_OBJECT_CLASS( klass )->dispose = export_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_finalize;

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
ofa_export_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_export_run";

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	g_object_new( OFA_TYPE_EXPORT,
			PROP_TOPLEVEL, main_window,
			NULL );

	gtk_main();
}

static void
do_initialize_assistant( ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_do_initialize_assistant";
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
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaExport *self )
{
	gboolean stop = FALSE;

	g_return_val_if_fail( OFA_IS_EXPORT( self ), FALSE );

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
on_prepare( GtkAssistant *assistant, GtkWidget *page, ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_on_prepare";
	gint page_num;

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( GTK_IS_WIDGET( page ));
	g_return_if_fail( OFA_IS_EXPORT( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, page=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) page, ( void * ) self );

		page_num = assistant_get_page_num( assistant, page );
		switch( page_num ){
			case ASSIST_PAGE_INTRO:
				do_prepare_p0_intro( self, page );
				break;

			case ASSIST_PAGE_SELECT:
				do_prepare_p1_type( self, page );
				break;

			case ASSIST_PAGE_FORMAT:
				do_prepare_p2_format( self, page );
				break;

			case ASSIST_PAGE_OUTPUT:
				do_prepare_p3_output( self, page );
				break;

			case ASSIST_PAGE_CONFIRM:
				do_prepare_p4_confirm( self, page );
				break;
		}
	}
}

static void
do_prepare_p0_intro( ofaExport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_do_prepare_p0_intro";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));
}

/*
 * p1: type of the data to export
 */
static void
do_prepare_p1_type( ofaExport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_do_prepare_p1_select";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p1_page_initialized ){
		do_init_p1_type( self, page );
		self->private->p1_page_initialized = TRUE;
	}

	check_for_p1_complete( self );
}

static void
do_init_p1_type( ofaExport *self, GtkWidget *page )
{
	ofaExportPrivate *priv;
	GtkWidget *btn;
	gint i;

	priv = self->private;

	for( i=0 ; st_type_group[i].widget_name ; ++ i ){

		btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_type_group[i].widget_name );
		g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
		g_object_set_data( G_OBJECT( btn ), DATA_TYPE, GINT_TO_POINTER( st_type_group[i].data ));
		g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_type_toggled ), self );
		if( !priv->p1_group ){
			priv->p1_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( btn ));
		}
	}

	priv->p1_type = 0;
}

static void
on_type_toggled( GtkToggleButton *button, ofaExport *self )
{
	ofaExportPrivate *priv;

	priv = self->private;

	if( gtk_toggle_button_get_active( button )){
		priv->p1_type = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_TYPE ));
		priv->p1_btn = button;
	} else {
		priv->p1_type = 0;
		priv->p1_btn = NULL;
	}
}

static void
get_active_type( ofaExport *self )
{
	ofaExportPrivate *priv;
	GSList *ig;

	priv = self->private;
	priv->p1_type = 0;
	priv->p1_btn = NULL;

	for( ig=priv->p1_group ; ig ; ig=ig->next ){
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ig->data ))){
			priv->p1_type = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( ig->data ), DATA_TYPE ));
			priv->p1_btn = GTK_TOGGLE_BUTTON( ig->data );
			break;
		}
	}
}

static void
check_for_p1_complete( ofaExport *self )
{
	GtkWidget *page;
	ofaExportPrivate *priv;

	get_active_type( self );

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_SELECT );
	gtk_assistant_set_page_complete( priv->assistant, page, priv->p1_type > 0 );
}

/*
 * p2: export format
 */
static void
do_prepare_p2_format( ofaExport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_do_prepare_p2_format";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p2_page_initialized ){
		do_init_p2_format( self, page );
		self->private->p2_page_initialized = TRUE;
	}

	check_for_p2_complete( self );
}

static void
do_init_p2_format( ofaExport *self, GtkWidget *page )
{
	ofaExportPrivate *priv;
	GtkWidget *btn;
	gint i;

	priv = self->private;

	for( i=0 ; st_format_group[i].widget_name ; ++ i ){

		btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_format_group[i].widget_name );
		g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
		g_object_set_data( G_OBJECT( btn ), DATA_FORMAT, GINT_TO_POINTER( st_format_group[i].data ));
		g_signal_connect( G_OBJECT( btn ), "toggled", G_CALLBACK( on_format_toggled ), self );
		if( !priv->p2_group ){
			priv->p2_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( btn ));
		}
	}
}

static void
on_format_toggled( GtkToggleButton *button, ofaExport *self )
{
	ofaExportPrivate *priv;

	priv = self->private;

	if( gtk_toggle_button_get_active( button )){
		priv->p2_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_FORMAT ));
		priv->p2_btn = button;
	} else {
		priv->p2_format = 0;
		priv->p2_btn = NULL;
	}
}

static void
get_active_format( ofaExport *self )
{
	ofaExportPrivate *priv;
	GSList *ig;

	priv = self->private;
	priv->p2_format = 0;
	priv->p2_btn = NULL;

	for( ig=priv->p2_group ; ig ; ig=ig->next ){
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ig->data ))){
			priv->p2_format = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( ig->data ), DATA_FORMAT ));
			priv->p2_btn = GTK_TOGGLE_BUTTON( ig->data );
			break;
		}
	}
}

static void
check_for_p2_complete( ofaExport *self )
{
	GtkWidget *page;
	ofaExportPrivate *priv;

	get_active_format( self );

	priv = self->private;
	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_FORMAT );
	gtk_assistant_set_page_complete( priv->assistant, page, priv->p2_format > 0 );
}

/*
 * p2: choose output file
 */
static void
do_prepare_p3_output( ofaExport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_do_prepare_p3_output";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	if( !self->private->p3_page_initialized ){
		do_init_p3_output( self, page );
		self->private->p3_page_initialized = TRUE;
	}

	check_for_p3_complete( self );
}

static gint
get_export_datas_for_type( gint type )
{
	gint i;

	for( i=0 ; st_export_datas[i].type ; ++i ){
		if( st_export_datas[i].type == type ){
			return( i );
		}
	}

	return( -1 );
}

static void
do_init_p3_output( ofaExport *self, GtkWidget *page )
{
	ofaExportPrivate *priv;
	gint idx;

	priv = self->private;

	priv->p3_chooser =
			GTK_FILE_CHOOSER( gtk_file_chooser_widget_new( GTK_FILE_CHOOSER_ACTION_SAVE ));
	gtk_widget_set_hexpand( GTK_WIDGET( priv->p3_chooser ), TRUE );
	gtk_widget_set_vexpand( GTK_WIDGET( priv->p3_chooser ), TRUE );
	gtk_file_chooser_set_do_overwrite_confirmation( priv->p3_chooser, TRUE );
	idx = get_export_datas_for_type( priv->p1_type );
	g_return_if_fail( idx >= 0 );
	gtk_file_chooser_set_uri( priv->p3_chooser, st_export_datas[idx].def_fname );
	g_signal_connect( G_OBJECT( priv->p3_chooser ), "selection-changed", G_CALLBACK( on_p3_selection_changed ), self );
	g_signal_connect( G_OBJECT( priv->p3_chooser ), "file-activated", G_CALLBACK( on_p3_file_activated ), self );
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( priv->p3_chooser ), 0, 1, 1, 1 );
	gtk_widget_show_all( page );
}

static void
on_p3_selection_changed( GtkFileChooser *chooser, ofaExport *self )
{
	check_for_p3_complete( self );
}

static void
on_p3_file_activated( GtkFileChooser *chooser, ofaExport *self )
{
	check_for_p3_complete( self );
}

static void
check_for_p3_complete( ofaExport *self )
{
	GtkWidget *page;
	ofaExportPrivate *priv;

	priv = self->private;

	g_free( priv->p3_uri );
	priv->p3_uri = gtk_file_chooser_get_uri( priv->p3_chooser );

	page = gtk_assistant_get_nth_page( priv->assistant, ASSIST_PAGE_OUTPUT );
	gtk_assistant_set_page_complete(
			priv->assistant,
			page,
			priv->p3_uri && g_utf8_strlen( priv->p3_uri, -1 ) > 0 );
}

/*
 * ask the user to confirm the operation
 */
static void
do_prepare_p4_confirm( ofaExport *self, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_do_prepare_p4_confirm";

	g_debug( "%s: self=%p, page=%p (%s)",
			thisfn,
			( void * ) self,
			( void * ) page, G_OBJECT_TYPE_NAME( page ));

	do_init_p4_confirm( self, page );
}

static void
do_init_p4_confirm( ofaExport *self, GtkWidget *page )
{
	ofaExportPrivate *priv;
	GtkWidget *widget;
	GtkGrid *grid;
	GtkLabel *label;
	gchar *markup;

	priv = self->private;

	widget = gtk_grid_get_child_at( GTK_GRID( page ), 0, 0 );
	if( widget ){
		gtk_widget_destroy( widget );
	}

	grid = GTK_GRID( gtk_grid_new());
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( grid ), 0, 0, 1, 1 );
	gtk_grid_set_row_spacing( grid, 6 );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b>%s</b> :", _( "Data to be exported :" ));
	gtk_label_set_markup( label, markup );
	g_free( markup );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 0, 1, 1 );

	label = GTK_LABEL( gtk_label_new( gtk_button_get_label( GTK_BUTTON( priv->p1_btn ))));
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 1, 0, 1, 1 );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b>%s</b> :", _( "Export format :" ));
	gtk_label_set_markup( label, markup );
	g_free( markup );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( gtk_button_get_label( GTK_BUTTON( priv->p2_btn ))));
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 1, 1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( NULL ));
	markup = g_markup_printf_escaped( "<b>%s</b> :", _( "Output file :" ));
	gtk_label_set_markup( label, markup );
	g_free( markup );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 0, 2, 1, 1 );

	label = GTK_LABEL( gtk_label_new( priv->p3_uri ));
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_grid_attach( grid, GTK_WIDGET( label ), 1, 2, 1, 1 );

	gtk_widget_show_all( page );
}

static void
on_apply( GtkAssistant *assistant, ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_on_apply";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_EXPORT( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		apply_export_type( self );
	}
}

static gboolean
apply_export_type( ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_apply_export_type";
	GSList *lines, *i;
	GFile *output;
	GOutputStream *stream;
	GError *error;
	gint idx, ret;
	gchar *str;
	gboolean ok;

	if( !my_utils_output_stream_new( self->private->p3_uri, &output, &stream )){
		return( FALSE );
	}
	g_return_val_if_fail( G_IS_FILE_OUTPUT_STREAM( stream ), FALSE );

	idx = get_export_datas_for_type( self->private->p1_type );
	g_return_val_if_fail( idx >= 0, FALSE );

	lines = ( *st_export_datas[idx].fn_csv )
						( ofa_main_window_get_dossier( self->private->main_window ));

	for( i=lines ; i ; i=i->next ){

		str = g_strdup_printf( "%s\n", ( const gchar * ) i->data );
		ret = g_output_stream_write( stream, str, strlen( str ), NULL, &error );
		g_free( str );

		if( ret == -1 ){
			g_warning( "%s: g_output_stream_write: %s", thisfn, error->message );
			g_error_free( error );
			ok = FALSE;
			goto terminate;
		}
	}
	ok = TRUE;

terminate:
	g_output_stream_close( stream, NULL, NULL );
	g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	g_object_unref( output );
	return( ok );
}

/*
 * the "cancel" message is sent when the user click on the "Cancel"
 * button, or if he hits the 'Escape' key and the 'Quit on escape'
 * preference is set
 */
static void
on_cancel( GtkAssistant *assistant, ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_on_cancel";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_EXPORT( self ));

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
is_willing_to_quit( ofaExport *self )
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
on_close( GtkAssistant *assistant, ofaExport *self )
{
	static const gchar *thisfn = "ofa_export_on_close";

	g_return_if_fail( GTK_IS_ASSISTANT( assistant ));
	g_return_if_fail( OFA_IS_EXPORT( self ));

	if( !self->private->dispose_has_run ){

		g_debug( "%s: assistant=%p, self=%p",
				thisfn, ( void * ) assistant, ( void * ) self );

		do_close( self );
	}
}

static void
do_close( ofaExport *self )
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
