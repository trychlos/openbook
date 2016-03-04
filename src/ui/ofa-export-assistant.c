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
#include <stdlib.h>

#include "api/my-progress-bar.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofa-file-format-bin.h"
#include "core/ofa-main-window.h"

#include "ui/my-iassistant.h"
#include "ui/ofa-export-assistant.h"

/* private instance data
 *
 * There are 3 useful pages
 * + one introduction page (page '0')
 * + one confirmation page (page '4)
 * + one result page (page '5')
 */
struct _ofaExportAssistantPrivate {
	gboolean          dispose_has_run;

	ofaHub           *hub;

	/* p0: introduction
	 */

	/* p1: select data type to be exported
	 */
	GSList           *p1_group;
	GtkWidget        *p1_btn;
	gint              p1_code;
	gchar            *p1_datatype;
	gint              p1_idx;

	/* p2: select format
	 */
	GtkWidget        *p2_datatype;
	ofaFileFormat    *p2_export_settings;
	ofaFileFormatBin *p2_settings_prefs;
	GtkWidget        *p2_message;
	gchar            *p2_format;
	gchar            *p2_settings_key;

	/* p3: output file
	 */
	GtkWidget        *p3_datatype;
	GtkWidget        *p3_format;
	GtkFileChooser   *p3_chooser;
	gchar            *p3_furi;			/* the output file URI */
	gchar            *p3_last_folder;

	/* p5: apply
	 */
	myProgressBar    *p5_bar;
	ofaIExportable   *p5_base;
	GtkWidget        *p5_page;
};

/* ExportAssistant Assistant
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
	DATA_ACCOUNT = 1,
	DATA_CLASS,
	DATA_CURRENCY,
	DATA_ENTRY,
	DATA_LEDGER,
	DATA_MODEL,
	DATA_RATE,
	DATA_DOSSIER,
};

/* The informations needed when managing the exported data types:
 * @code: the above type of data
 * @widget_name: the name of the widget in the page (not localized)
 * @base_name: the (localized) basename of the default output file
 *
 * The index to this structure will be set against each radio button.
 */
typedef GType ( *fn_type )( void );
typedef struct {
	gint         code;
	const gchar *widget_name;
	const gchar *base_name;
	fn_type      get_type;
	const gchar *export_suffix;
}
	sTypes;

static sTypes st_types[] = {
		{ DATA_ACCOUNT,  "p1-account",  N_( "ofaAccounts.csv" ),     ofo_account_get_type,      "Account" },
		{ DATA_CLASS,    "p1-class",    N_( "ofaClasses.csv" ),      ofo_class_get_type,        "Class" },
		{ DATA_CURRENCY, "p1-currency", N_( "ofaCurrencies.csv" ),   ofo_currency_get_type,     "Currency" },
		{ DATA_ENTRY,    "p1-entries",  N_( "ofaEntries.csv" ),      ofo_entry_get_type,        "Entry" },
		{ DATA_LEDGER,   "p1-ledger",   N_( "ofaLedgers.csv" ),      ofo_ledger_get_type,       "Ledger" },
		{ DATA_MODEL,    "p1-model",    N_( "ofaOpeTemplates.csv" ), ofo_ope_template_get_type, "Model" },
		{ DATA_RATE,     "p1-rate",     N_( "ofaRates.csv" ),        ofo_rate_get_type,         "Rate" },
		{ DATA_DOSSIER,  "p1-dossier",  N_( "ofaDossier.csv" ),      ofo_dossier_get_type,      "Dossier" },
		{ 0 }
};

/* data set against each data type radio button */
#define DATA_TYPE_INDEX                 "ofa-data-type"

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-export-assistant.ui";
static const gchar *st_pref_settings    = "ofaExportAssistant-settings";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      iwindow_init( myIWindow *instance );
static void      iassistant_iface_init( myIAssistantInterface *iface );
static gboolean  iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void      p0_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page_widget );
static void      p1_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p1_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p1_on_type_toggled( GtkToggleButton *button, ofaExportAssistant *self );
static gboolean  p1_is_complete( ofaExportAssistant *self );
static void      p1_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p2_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p2_on_settings_changed( ofaFileFormatBin *bin, ofaExportAssistant *self );
static void      p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self );
static void      p2_check_for_complete( ofaExportAssistant *self );
static void      p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p3_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p3_on_selection_changed( GtkFileChooser *chooser, ofaExportAssistant *self );
static void      p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self );
static gboolean  p3_check_for_complete( ofaExportAssistant *self );
static void      p3_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static void      p4_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean  p3_confirm_overwrite( const ofaExportAssistant *self, const gchar *fname );
static void      p5_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page );
static gboolean  export_data( ofaExportAssistant *self );
static void      p5_on_progress( ofaIExportable *exportable, gdouble progress, const gchar *text, ofaExportAssistant *self );
static void      get_settings( ofaExportAssistant *self );
static void      set_settings( ofaExportAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaExportAssistant, ofa_export_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaExportAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IASSISTANT, iassistant_iface_init ))

static const ofsIAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				( myIAssistantCb ) p0_do_forward },
		{ ASSIST_PAGE_SELECT,
				( myIAssistantCb ) p1_do_init,
				( myIAssistantCb ) p1_do_display,
				( myIAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_FORMAT,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_OUTPUT,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				( myIAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				NULL,
				( myIAssistantCb ) p4_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myIAssistantCb ) p5_do_display,
				NULL },
		{ -1 }
};

static void
export_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_export_assistant_finalize";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	/* free data members here */
	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	g_free( priv->p1_datatype );
	g_free( priv->p2_format );
	g_free( priv->p2_settings_key );
	g_free( priv->p3_last_folder );
	g_free( priv->p3_furi );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->finalize( instance );
}

static void
export_dispose( GObject *instance )
{
	ofaExportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_ASSISTANT( instance ));

	priv = ofa_export_assistant_get_instance_private( OFA_EXPORT_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->p2_export_settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_assistant_parent_class )->dispose( instance );
}

static void
ofa_export_assistant_init( ofaExportAssistant *self )
{
	static const gchar *thisfn = "ofa_export_assistant_init";
	ofaExportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT_ASSISTANT( self ));

	priv = ofa_export_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->p1_code = -1;
	priv->p1_idx = -1;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_export_assistant_class_init( ofaExportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_export_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_export_assistant_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_export_assistant_run";
	ofaExportAssistant *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_EXPORT_ASSISTANT, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_export_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_export_assistant_iwindow_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );
	get_settings( OFA_EXPORT_ASSISTANT( instance ));
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_export_assistant_iassistant_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_willing_to_quit = iassistant_is_willing_to_quit;
}

static gboolean
iassistant_is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	return( ofa_prefs_assistant_is_willing_to_quit( keyval ));
}

/*
 * get some dossier data
 */
static void
p0_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_export_assistant_p0_do_forward";
	ofaExportAssistantPrivate *priv;
	GtkApplicationWindow *main_window;

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = ofa_export_assistant_get_instance_private( self );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));
}

/*
 * p1: type of the data to export
 *
 * Selection is radio-button based, so we are sure that at most
 * one exported data type is selected
 * and we make sure that at least one button is active
 *
 * so the page is always completed
 */
static void
p1_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *btn;
	gint i;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* attach its identifier to each widget */
	for( i=0 ; st_types[i].code ; ++i ){
		btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_types[i].widget_name );
		g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));

		g_object_set_data( G_OBJECT( btn ), DATA_TYPE_INDEX, GINT_TO_POINTER( st_types[i].code ));
		g_signal_connect( btn, "toggled", G_CALLBACK( p1_on_type_toggled ), self );
		if( 0 ){
			g_debug( "%s: btn=%p, code=%d", thisfn, ( void * ) btn, st_types[i].code );
		}

		if( !priv->p1_group ){
			priv->p1_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( btn ));
		}
	}
}

static void
p1_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_display";
	ofaExportAssistantPrivate *priv;
	gint i, code;
	GtkWidget *btn;
	gboolean found, is_complete;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	found = FALSE;

	for( i=0 ; !found && st_types[i].code ; ++i ){
		btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_types[i].widget_name );
		g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
		code = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
		if( code == priv->p1_code ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
			found = TRUE;
		}
	}

	if( !found ){
		g_return_if_fail( priv->p1_group && priv->p1_group->data );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p1_group->data ), TRUE );
	}

	is_complete = p1_is_complete( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), is_complete );
}

static void
p1_on_type_toggled( GtkToggleButton *button, ofaExportAssistant *self )
{
	gboolean is_complete;

	is_complete = p1_is_complete( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), is_complete );
}

static gboolean
p1_is_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GSList *it;

	priv = ofa_export_assistant_get_instance_private( self );

	g_return_val_if_fail( priv->p1_group, FALSE );

	priv->p1_btn = NULL;
	priv->p1_code = -1;
	g_free( priv->p1_datatype );
	priv->p1_datatype = NULL;

	/* which is the currently active button ? */
	for( it=priv->p1_group ; it ; it=it->next ){
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( it->data ))){
			priv->p1_code = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( it->data ), DATA_TYPE_INDEX ));
			priv->p1_btn = GTK_WIDGET( it->data );
			priv->p1_datatype = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( it->data )));
			break;
		}
	}

	return( priv->p1_code > 0 && priv->p1_btn && my_strlen( priv->p1_datatype ));
}

static void
p1_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p1_do_forward";
	ofaExportAssistantPrivate *priv;
	gint i;

	g_debug( "%s: self=%p, page=%p", thisfn, ( void * ) self, ( void * ) page );

	priv = ofa_export_assistant_get_instance_private( self );

	for( i=0 ; st_types[i].code ; ++i ){
		if( st_types[i].code == priv->p1_code ){
			priv->p1_idx = i;
			break;
		}
	}

	g_debug( "%s: idx=%d, code=%d", thisfn, priv->p1_idx, priv->p1_code );

	set_settings( self );
}

/*
 * p2: export format
 *
 * These are initialized with the export settings for this name, or
 * with default settings
 */
static void
p2_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_init";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label, *parent, *button;
	GtkSizeGroup *hgroup;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	priv->p2_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-datatype" );
	g_return_if_fail( priv->p2_datatype && GTK_IS_LABEL( priv->p2_datatype ));
	my_utils_widget_set_style( priv->p2_datatype, "labelinfo" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p2_settings_prefs = ofa_file_format_bin_new( NULL );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_settings_prefs ));

	g_signal_connect( priv->p2_settings_prefs, "ofa-changed", G_CALLBACK( p2_on_settings_changed ), self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-new-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( p2_on_new_profile_clicked ), self );

	priv->p2_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-message" );
	g_return_if_fail( priv->p2_message && GTK_IS_LABEL( priv->p2_message ));
	my_utils_widget_set_style( priv->p2_message, "labelerror" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-label221" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	my_utils_size_group_add_size_group(
			hgroup, ofa_file_format_bin_get_size_group( priv->p2_settings_prefs, 0 ));

	g_object_unref( hgroup );
}

static void
p2_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_display";
	ofaExportAssistantPrivate *priv;
	gchar *candidate_key;
	const gchar *found_key;
	myISettings *instance;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p2_datatype ), priv->p1_datatype );

	candidate_key =  g_strdup_printf( "%s-%s", SETTINGS_EXPORT_SETTINGS, st_types[priv->p1_idx].export_suffix );
	instance = ofa_settings_get_settings( SETTINGS_TARGET_USER );
	g_return_if_fail( instance && MY_IS_ISETTINGS( instance ));

	if( my_isettings_has_key( instance, SETTINGS_GROUP_GENERAL, candidate_key )){
		found_key = candidate_key;
	} else {
		g_debug( "%s: candidate_key=%s not found", thisfn, candidate_key );
		found_key = SETTINGS_EXPORT_SETTINGS;
	}
	priv->p2_settings_key = g_strdup( candidate_key );

	g_clear_object( &priv->p2_export_settings );
	priv->p2_export_settings = ofa_file_format_new( found_key );
	ofa_file_format_set_prefs_name( priv->p2_export_settings, priv->p2_settings_key );
	ofa_file_format_set_mode( priv->p2_export_settings, OFA_FFMODE_EXPORT );
	ofa_file_format_bin_set_format( priv->p2_settings_prefs, priv->p2_export_settings );

	g_free( candidate_key );

	p2_check_for_complete( self );
}

static void
p2_on_settings_changed( ofaFileFormatBin *bin, ofaExportAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_on_new_profile_clicked( GtkButton *button, ofaExportAssistant *self )
{
	g_warning( "ofa_export_assistant_p2_on_new_profile_clicked: TO BE MANAGED" );
}

static void
p2_check_for_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_export_assistant_get_instance_private( self );

	ok = ofa_file_format_bin_is_valid( priv->p2_settings_prefs, &message );

	gtk_label_set_text( GTK_LABEL( priv->p2_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p2_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p2_do_forward";
	ofaExportAssistantPrivate *priv;
	ofaFFtype fftype;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	ofa_file_format_bin_apply( priv->p2_settings_prefs );

	g_free( priv->p2_format );
	fftype = ofa_file_format_get_fftype( priv->p2_export_settings );
	priv->p2_format = g_strdup( ofa_file_format_get_fftype_str( fftype ));
}

/*
 * p3: choose output file
 */
static void
p3_do_init( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_init";
	ofaExportAssistantPrivate *priv;
	gchar *dirname;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	g_return_if_fail( priv->p1_idx >= 0 );

	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_utils_widget_set_style( priv->p3_datatype, "labelinfo" );

	priv->p3_format = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-format" );
	g_return_if_fail( priv->p3_format && GTK_IS_LABEL( priv->p3_format ));
	my_utils_widget_set_style( priv->p3_format, "labelinfo" );

	priv->p3_chooser =
			( GtkFileChooser * ) my_utils_container_get_child_by_name(
					GTK_CONTAINER( page ), "p3-filechooser" );
	g_return_if_fail( priv->p3_chooser && GTK_IS_FILE_CHOOSER( priv->p3_chooser ));

	g_signal_connect(
			priv->p3_chooser, "selection-changed", G_CALLBACK( p3_on_selection_changed ), self );
	g_signal_connect(
			priv->p3_chooser, "file-activated", G_CALLBACK( p3_on_file_activated ), self );

	/* build a default output filename from the last used folder
	 * plus the default basename for this data type class */
	if( my_strlen( priv->p3_furi )){
		dirname = g_path_get_dirname( priv->p3_furi );
	} else {
		dirname = g_strdup( ofa_settings_user_get_string( SETTINGS_EXPORT_FOLDER ));
	}
	g_free( priv->p3_furi );
	priv->p3_furi = g_build_filename( dirname, st_types[priv->p1_idx].base_name, NULL );

	g_free( dirname );
}

static void
p3_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_display";
	ofaExportAssistantPrivate *priv;
	gchar *dirname, *basename;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_datatype ), priv->p1_datatype );
	gtk_label_set_text( GTK_LABEL( priv->p3_format ), priv->p2_format );

	if( my_strlen( priv->p3_furi )){
		dirname = g_path_get_dirname( priv->p3_furi );
		gtk_file_chooser_set_current_folder_uri( priv->p3_chooser, dirname );
		g_free( dirname );
		basename = g_path_get_basename( priv->p3_furi );
		gtk_file_chooser_set_current_name( priv->p3_chooser, basename );
		g_free( basename );

	} else if( my_strlen( priv->p3_last_folder )){
		gtk_file_chooser_set_current_folder( priv->p3_chooser, priv->p3_last_folder );
	}

	p3_check_for_complete( self );
}

static void
p3_on_selection_changed( GtkFileChooser *chooser, ofaExportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_on_file_activated( GtkFileChooser *chooser, ofaExportAssistant *self )
{
	gboolean ok;

	ok = p3_check_for_complete( self );

	if( ok ){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p3_check_for_complete( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gboolean ok;
	gchar *name, *final, *folder;

	priv = ofa_export_assistant_get_instance_private( self );

	g_free( priv->p3_furi );

	name = gtk_file_chooser_get_current_name( priv->p3_chooser );
	if( my_strlen( name )){
		final = NULL;
		if( g_path_is_absolute( name )){
			final = g_strdup( name );
		} else {
			folder = gtk_file_chooser_get_current_folder( priv->p3_chooser );
			final = g_build_filename( folder, name, NULL );
			g_free( folder );
		}
		g_debug( "p3_check_for_complete: final=%s", final );
		priv->p3_furi = g_filename_to_uri( final, NULL, NULL );
		g_free( final );

	} else {
		priv->p3_furi = gtk_file_chooser_get_uri( priv->p3_chooser );
	}
	g_free( name );

	ok = my_strlen( priv->p3_furi ) > 0 && !my_utils_uri_is_dir( priv->p3_furi );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

/*
 * should be directly managed by the GtkFileChooser class, but doesn't
 * seem to work :(
 *
 * Returns: %TRUE in order to confirm overwrite.
 */
static gboolean
p3_confirm_overwrite( const ofaExportAssistant *self, const gchar *fname )
{
	gboolean ok;
	gchar *str;

	str = g_strdup_printf(
				_( "The file '%s' already exists.\n"
					"Are you sure you want to overwrite it ?" ), fname );

	ok = my_utils_dialog_question( str, _( "_Overwrite" ));

	g_free( str );

	return( ok );
}

static void
p3_do_forward( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p3_do_forward";
	ofaExportAssistantPrivate *priv;
	gboolean ok;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	/* keep the last used folder in case we go back to this page
	 * we choose to keep the same folder, letting the user choose
	 * another basename */
	g_free( priv->p3_last_folder );
	priv->p3_last_folder = g_path_get_dirname( priv->p3_furi );

	/* we cannot prevent this test to be made only here
	 * if the user cancel, then the assistant will anyway go to the
	 * Confirmation page, without any dest uri
	 * This is because GtkAssistant does not let us stay on the same page
	 * when the user has clicked on the Next button */
	if( my_utils_uri_exists( priv->p3_furi )){
		ok = p3_confirm_overwrite( self, priv->p3_furi );
		if( !ok ){
			g_free( priv->p3_furi );
			priv->p3_furi = NULL;
		}
	}

	set_settings( self );
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p5_do_prepare_confirm";
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str;
	gint format;
	gboolean complete;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_export_assistant_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-data" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_datatype );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-format" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_format );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-charmap" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p2_export_settings ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	format = ofa_file_format_get_date_format( priv->p2_export_settings );
	gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-decimal" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p2_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-field" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p2_export_settings ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-headers" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	str = g_strdup(
			ofa_file_format_has_headers( priv->p2_export_settings ) ? _( "True" ) : _( "False" ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_furi );

	complete = ( my_strlen( priv->p3_furi ) > 0 );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

/*
 * When executing this function, the display stays on the 'Confirmation'
 * page - The 'Result page is only displayed after this computing
 * returns.
 *
 * from a GType
 * - allocate a new object
 * - check that OFA_IS_IEXPORTABLE
 * - use OFA_IEXPORTABLE_GET_INTERFACE( instance )->method to check for an interface
 *
 * Text: Exporting <Accounts> to <filename>
 *
 * progress bar 50%
 *
 * result: 99 successfully exported records
 * provides a callback to display the progress fraction from 0.0 to 1.0
 */
static void
p5_do_display( ofaExportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_export_assistant_p5_do_display";
	ofaExportAssistantPrivate *priv;
	GtkWidget *parent;

	g_return_if_fail( OFA_IS_EXPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	priv = ofa_export_assistant_get_instance_private( self );

	priv->p5_page = page;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_bar ));

	priv->p5_base = ( ofaIExportable * ) g_object_new( st_types[priv->p1_idx].get_type(), NULL );
	if( !OFA_IS_IEXPORTABLE( priv->p5_base )){
		my_utils_dialog_warning( _( "The requested type does not implement the IExportable interface" ));
		return;
	}

	/* message provided by the ofaIExportable interface */
	g_signal_connect(
			G_OBJECT( priv->p5_base ), "ofa-progress", G_CALLBACK( p5_on_progress ), self );

	gtk_widget_show_all( page );

	g_idle_add(( GSourceFunc ) export_data, self );
}

static gboolean
export_data( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str, *text;
	gboolean ok;

	priv = ofa_export_assistant_get_instance_private( self );

	/* first, export */
	ok = ofa_iexportable_export_to_path(
			priv->p5_base, priv->p3_furi, priv->p2_export_settings, priv->hub, self );

	/* then display the result */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p5-label" );

	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p1_btn )));
	if( ok ){
		text = g_strdup_printf( _( "OK: « %s » has been successfully exported.\n\n"
				"%ld lines have been written in '%s' output file." ),
				str, ofa_iexportable_get_count( priv->p5_base ), priv->p3_furi );
	} else {
		text = g_strdup_printf( _( "Unfortunately, « %s » export has encountered errors.\n\n"
				"The '%s' file may be incomplete or inaccurate.\n\n"
				"Please fix these errors, and retry then." ), str, priv->p3_furi );
	}
	g_free( str );

	str = g_strdup_printf( "<b>%s</b>", text );
	g_free( text );

	gtk_label_set_markup( GTK_LABEL( label ), str );
	g_free( str );

	/* unref the reference object _after_ having built the label so
	 * we always have access to its internal datas */
	g_object_unref( priv->p5_base );

	gtk_assistant_set_page_complete( GTK_ASSISTANT( self ), priv->p5_page, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

static void
p5_on_progress( ofaIExportable *exportable, gdouble progress, const gchar *text, ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;

	priv = ofa_export_assistant_get_instance_private( self );

	g_signal_emit_by_name( priv->p5_bar, "ofa-double", progress );
	g_signal_emit_by_name( priv->p5_bar, "ofa-text", text );
}

/*
 * settings are:
 * content;uri;
 */
static void
get_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = ofa_export_assistant_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_pref_settings );

	it = slist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->p1_code = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		priv->p3_furi = g_strdup( cstr );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaExportAssistant *self )
{
	ofaExportAssistantPrivate *priv;
	gchar *str;

	priv = ofa_export_assistant_get_instance_private( self );

	str = g_strdup_printf( "%d;%s;",
			priv->p1_code,
			priv->p3_furi ? priv->p3_furi : "" );

	ofa_settings_user_set_string( st_pref_settings, str );

	g_free( str );
}
