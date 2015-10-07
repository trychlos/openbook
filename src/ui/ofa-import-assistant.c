/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-file-format.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/my-progress-bar.h"
#include "core/ofa-file-format-bin.h"

#include "ui/ofa-import-assistant.h"
#include "ui/ofa-main-window.h"

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select a file
 *   2   Content  TYPE     Select a type of import
 *   3   Content  SETTINGS Set the locale settings
 *   4   Confirm  CONFIRM  Summary of the operations to be done
 *   5   Summary  DONE     After import
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_TYPE,
	ASSIST_PAGE_SETTINGS,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* private instance data
 */
struct _ofaImportAssistantPrivate {

	/* p0: introduction
	 */

	/* p1: select file to be imported
	 */
	GtkFileChooser   *p1_chooser;
	gchar            *p1_folder;
	gchar            *p1_furi;			/* the utf-8 imported filename */

	/* p2: select a type of data to be imported
	 */
	GtkWidget        *p2_furi;
	GSList           *p2_group;
	gint              p2_type;
	gint              p2_idx;
	GtkButton        *p2_type_btn;
	gchar            *p2_datatype;

	/* p3: locale settings
	 */
	GtkWidget        *p3_furi;
	GtkWidget        *p3_datatype;
	ofaFileFormat    *p3_import_settings;
	ofaFileFormatBin *p3_settings_prefs;
	GtkWidget        *p3_message;

	/* p4: confirm
	 */

	/* p5: import the file, display the result
	 */
	myProgressBar    *p5_import;
	myProgressBar    *p5_insert;
	GtkWidget        *p5_page;
	GtkWidget        *p5_text;
	ofaIImportable   *p5_object;
	ofaIImportable   *p5_plugin;

	/* runtime data
	 */
	GtkWidget        *current_page_w;
};

/* management of the radio buttons group
 * types are defined in ofa-iimporter.h
 */
typedef GType ( *fn_type )( void );
typedef struct {
	guint        type_id;
	const gchar *w_name;				/* the name of the radio button widget */
	fn_type      get_type;
}
	sRadios;

enum {
	IMPORT_BAT = 1,
	IMPORT_CLASS,
	IMPORT_ACCOUNT,
	IMPORT_CURRENCY,
	IMPORT_LEDGER,
	IMPORT_MODEL,
	IMPORT_RATE,
	IMPORT_ENTRY,
};

static const sRadios st_radios[] = {
		{ IMPORT_BAT,      "p2-releve",   NULL },
		{ IMPORT_CLASS,    "p2-class",    ofo_class_get_type },
		{ IMPORT_ACCOUNT,  "p2-account",  ofo_account_get_type },
		{ IMPORT_CURRENCY, "p2-currency", ofo_currency_get_type },
		{ IMPORT_LEDGER,   "p2-journals", ofo_ledger_get_type },
		{ IMPORT_MODEL,    "p2-model",    ofo_ope_template_get_type },
		{ IMPORT_RATE,     "p2-rate",     ofo_rate_get_type },
		{ IMPORT_ENTRY,    "p2-entries",  ofo_entry_get_type },
		{ 0 }
};

/* data set against each of above radio buttons */
#define DATA_BUTTON_INDEX               "ofa-data-button-idx"

/* the user preferences stored as a string list
 * folder
 */
static const gchar *st_prefs_import     = "ImportAssistant-settings";

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-import-assistant.ui";
static const gchar *st_ui_id            = "ImportAssistant";

static void     iimporter_iface_init( ofaIImporterInterface *iface );
static guint    iimporter_get_interface_version( const ofaIImporter *instance );
static void     p1_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self );
static void     p1_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self );
static gboolean p1_check_for_complete( ofaImportAssistant *self );
static void     p1_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self );
static void     p2_check_for_complete( ofaImportAssistant *self );
static void     p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p3_on_settings_changed( ofaFileFormatBin *bin, ofaImportAssistant *self );
static void     p3_check_for_complete( ofaImportAssistant *self );
static void     p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_error_no_interface( const ofaImportAssistant *self );
static gboolean p5_do_import( ofaImportAssistant *self );
static guint    p5_do_import_csv( ofaImportAssistant *self, guint *errors );
static guint    p5_do_import_other( ofaImportAssistant *self, guint *errors );
static void     p5_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self );
static void     p5_on_pulse( ofaIImporter *importer, ofeImportablePhase phase, ofaImportAssistant *self );
static void     p5_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self );
static void     get_settings( ofaImportAssistant *self );
static void     update_settings( ofaImportAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaImportAssistant, ofa_import_assistant, MY_TYPE_ASSISTANT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ));

static const ofsAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_SELECT,
				( myAssistantCb ) p1_do_init,
				( myAssistantCb ) p1_do_display,
				( myAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_TYPE,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_do_display,
				( myAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_SETTINGS,
				( myAssistantCb ) p3_do_init,
				( myAssistantCb ) p3_do_display,
				( myAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				NULL,
				( myAssistantCb ) p4_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myAssistantCb ) p5_do_display,
				NULL },
		{ -1 }
};

static void
import_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_import_assistant_finalize";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	/* free data members here */
	priv = OFA_IMPORT_ASSISTANT( instance )->priv;

	g_free( priv->p1_folder );
	g_free( priv->p1_furi );
	g_free( priv->p2_datatype );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->finalize( instance );
}

static void
import_assistant_dispose( GObject *instance )
{
	ofaImportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_IMPORT_ASSISTANT( instance )->priv;

		/* unref object members here */
		g_clear_object( &priv->p3_import_settings );
		g_clear_object( &priv->p5_object );
		g_clear_object( &priv->p5_plugin );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->dispose( instance );
}

static void
ofa_import_assistant_init( ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_IMPORT_ASSISTANT, ofaImportAssistantPrivate );

	self->priv->p2_type = -1;
}

static void
ofa_import_assistant_class_init( ofaImportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_import_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = import_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = import_assistant_finalize;

	g_type_class_add_private( klass, sizeof( ofaImportAssistantPrivate ));
}

/*
 * ofaIImporter interface management
 */
static void
iimporter_iface_init( ofaIImporterInterface *iface )
{
	static const gchar *thisfn = "ofo_account_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimporter_get_interface_version;
}

static guint
iimporter_get_interface_version( const ofaIImporter *instance )
{
	return( 1 );
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_import_assistant_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_import_assistant_run";
	ofaImportAssistant *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_IMPORT_ASSISTANT,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	get_settings( self );

	/* messages provided by the ofaIImporter interface */
	g_signal_connect(
			G_OBJECT( self ), "pulse", G_CALLBACK( p5_on_pulse ), self );
	g_signal_connect(
			G_OBJECT( self ), "progress", G_CALLBACK( p5_on_progress ), self );
	g_signal_connect(
			G_OBJECT( self ), "message", G_CALLBACK( p5_on_message ), self );

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * initialize the GtkFileChooser widget with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p1_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p1_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p1_chooser = GTK_FILE_CHOOSER( widget );
	gtk_file_chooser_set_action( priv->p1_chooser, GTK_FILE_CHOOSER_ACTION_OPEN );

	g_signal_connect( widget, "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	g_signal_connect( widget, "file-activated", G_CALLBACK( p1_on_file_activated ), self );
}

/*
 * gtk_file_chooser_select_uri() is supposed to first set the ad-hoc
 * folder, and then select the appropriate file - but doesn't seem to
 * work alone :(
 *
 * gtk_file_chooser_set_current_folder_uri() rightly set the current
 * folder, but reset the selection (and have a bad location 'ebp_')
 * so, has to select after the right file
 *
 * gtk_file_chooser_set_uri() doesn't work (doesn't select the parent
 * folder, doesn't select the file)
 * gtk_file_chooser_set_filename() idem
 */
static void
p1_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	/*static const gchar *thisfn = "ofa_import_assistant_p1_do_display";*/
	ofaImportAssistantPrivate *priv;

	priv = self->priv;
	priv->current_page_w = page;

	if( priv->p1_furi ){
		gtk_file_chooser_set_uri( priv->p1_chooser, priv->p1_furi );

	} else if( priv->p1_folder ){
		gtk_file_chooser_set_current_folder_uri( priv->p1_chooser, priv->p1_folder );
	}
}

static void
p1_on_selection_changed( GtkFileChooser *chooser, ofaImportAssistant *self )
{
	p1_check_for_complete( self );
}

static void
p1_on_file_activated( GtkFileChooser *chooser, ofaImportAssistant *self )
{
	if( p1_check_for_complete( self )){
		gtk_assistant_next_page( my_assistant_get_assistant( MY_ASSISTANT( self )));
	}
}

static gboolean
p1_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	g_free( priv->p1_furi );
	priv->p1_furi = gtk_file_chooser_get_uri( priv->p1_chooser );

	ok = my_strlen( priv->p1_furi ) &&
			my_utils_uri_is_readable_file( priv->p1_furi );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );

	return( ok );
}

static void
p1_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p1_do_forward";
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( priv->p1_chooser );

	g_debug( "%s: uri=%s, folder=%s", thisfn, priv->p1_furi, priv->p1_folder );

	update_settings( self );
}

/*
 * p2: nature of the data to import
 */
static void
p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_init";
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *button;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	priv->p2_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_furi && GTK_IS_LABEL( priv->p2_furi ));
	my_utils_widget_set_style( priv->p2_furi, "labelinfo" );

	for( i=0 ; st_radios[i].type_id ; ++i ){
		button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_radios[i].w_name );
		g_object_set_data( G_OBJECT( button ), DATA_BUTTON_INDEX, GINT_TO_POINTER( i ));
		g_signal_connect( G_OBJECT( button ), "toggled", G_CALLBACK( p2_on_type_toggled ), self );
		if( !priv->p2_group ){
			priv->p2_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ));
		}
	}
}

static void
p2_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_display";
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *button;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	gtk_label_set_text( GTK_LABEL( priv->p2_furi ), priv->p1_furi );

	for( i=0 ; st_radios[i].type_id ; ++i ){
		button = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), st_radios[i].w_name );
		if( st_radios[i].type_id == priv->p2_type ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( button ), TRUE );
			p2_on_type_toggled( GTK_TOGGLE_BUTTON( button ), self );
			break;
		}
	}

	p2_check_for_complete( self );
}

static void
p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( gtk_toggle_button_get_active( button )){
		priv->p2_idx = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_BUTTON_INDEX ));
		priv->p2_type = st_radios[priv->p2_idx].type_id;
		priv->p2_type_btn = GTK_BUTTON( button );
		priv->p2_datatype = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( button )));
	} else {
		priv->p2_idx = -1;
		priv->p2_type = 0;
		priv->p2_type_btn = NULL;
		g_free( priv->p2_datatype );
		priv->p2_datatype = NULL;
	}

	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, priv->p2_type > 0 );
}

static void
p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	update_settings( self );
}

/*
 * p3: import format
 *
 * These are initialized with the import settings for this name, or
 * with default settings
 *
 * The page is always complete
 */
static void
p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent, *label;
	GtkSizeGroup *hgroup;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	priv->p3_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_furi && GTK_IS_LABEL( priv->p3_furi ));
	my_utils_widget_set_style( priv->p3_furi, "labelinfo" );

	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_utils_widget_set_style( priv->p3_datatype, "labelinfo" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label311" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label312" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p3_import_settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
	priv->p3_settings_prefs = ofa_file_format_bin_new( priv->p3_import_settings );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p3_settings_prefs ));
	my_utils_size_group_add_size_group(
			hgroup, ofa_file_format_bin_get_size_group( priv->p3_settings_prefs, 0 ));

	g_signal_connect(
			G_OBJECT( priv->p3_settings_prefs ),
			"ofa-changed", G_CALLBACK( p3_on_settings_changed ), self );

	priv->p3_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" );
	g_return_if_fail( priv->p3_message && GTK_IS_LABEL( priv->p3_message ));
	my_utils_widget_set_style( priv->p3_message, "labelerror" );

	g_object_unref( hgroup );
}

static void
p3_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	gtk_label_set_text( GTK_LABEL( priv->p3_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p3_datatype ), priv->p2_datatype );

	p3_check_for_complete( self );
}

static void
p3_on_settings_changed( ofaFileFormatBin *bin, ofaImportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = self->priv;
	ok = ofa_file_format_bin_is_valid( priv->p3_settings_prefs, &message );

	gtk_label_set_text( GTK_LABEL( priv->p3_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_forward";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	ofa_file_format_bin_apply( priv->p3_settings_prefs );
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gboolean complete;
	myDateFormat format;
	ofaFFtype ffmt;
	gchar *str;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_furi );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-type" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_datatype );

	ffmt = ofa_file_format_get_fftype( priv->p3_import_settings );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-ffmt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_fftype_str( ffmt ));

	if( ffmt != OFA_FFTYPE_OTHER ){

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-charmap" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		gtk_label_set_text( GTK_LABEL( label ), ofa_file_format_get_charmap( priv->p3_import_settings ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-date" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		format = ofa_file_format_get_date_format( priv->p3_import_settings );
		gtk_label_set_text( GTK_LABEL( label ), my_date_get_format_str( format ));

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-decimal" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%c", ofa_file_format_get_decimal_sep( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-field" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%c", ofa_file_format_get_field_sep( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-headers" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelinfo" );
		str = g_strdup_printf( "%d", ofa_file_format_get_headers_count( priv->p3_import_settings ));
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );
	}

	gtk_widget_show_all( page );

	complete = my_strlen( priv->p1_furi ) > 0;
	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, complete );
}

/*
 * import
 * execution summary
 */
static void
p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent;

	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page, FALSE );

	priv = self->priv;
	priv->current_page_w = page;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-bar-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_import = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_import ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-insert-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_insert = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_insert ));

	priv->p5_text = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-text-view" );
	g_return_if_fail( priv->p5_text && GTK_IS_TEXT_VIEW( priv->p5_text ));
	gtk_widget_set_can_focus( priv->p5_text, FALSE );

	priv->p5_page = page;
	gtk_widget_show_all( page );

	/* search from something which would be able to import the data */
	if( st_radios[priv->p2_idx].get_type ){
		priv->p5_object = ( ofaIImportable * ) g_object_new( st_radios[priv->p2_idx].get_type(), NULL );
	} else {
		priv->p5_plugin = ofa_iimportable_find_willing_to( priv->p1_furi, priv->p3_import_settings );
	}
	if( !priv->p5_object && !priv->p5_plugin ){
		p5_error_no_interface( self );
		return;
	}

	g_idle_add(( GSourceFunc ) p5_do_import, self );
}

static void
p5_error_no_interface( const ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gchar *str;
	GtkWidget *label;
	const gchar *cstr;

	priv = self->priv;

	/* display an error dialog */
	if( priv->p5_object ){
		str = g_strdup_printf(
					_( "The requested type (%s) does not implement the IImportable interface" ),
					G_OBJECT_TYPE_NAME( priv->p5_object ));
	} else {
		str = g_strdup_printf( _( "Unable to find a plugin to import the specified data" ));
	}

	my_utils_dialog_warning( str );

	g_free( str );

	/* prepare the assistant to terminate */
	label = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )) ), "p5-label" );

	cstr = _( "Unfortunately, we have not been able to do anything for "
			"import the specified file.\n"
			"You should most probably open a feature request against the "
			"Openbook maintainer." );

	gtk_label_set_text( GTK_LABEL( label ), cstr );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, TRUE );
}

static gboolean
p5_do_import( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *str, *text;
	gint ffmt;
	guint count, errors;
	gboolean has_worked;

	priv = self->priv;
	count = 0;
	errors = 0;
	has_worked = TRUE;

	/* first, import */
	ffmt = ofa_file_format_get_fftype( priv->p3_import_settings );
	switch( ffmt ){
		case OFA_FFTYPE_CSV:
			count = p5_do_import_csv( self, &errors );
			break;
		case OFA_FFTYPE_OTHER:
			g_return_val_if_fail( priv->p5_plugin && OFA_IS_IIMPORTABLE( priv->p5_plugin ), FALSE );
			count = p5_do_import_other( self, &errors );
			break;
		default:
			has_worked = FALSE;
			break;
	}

	/* then display the result */
	label = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )) ), "p5-label" );

	str = my_utils_str_remove_underlines( gtk_button_get_label( GTK_BUTTON( priv->p2_type_btn )));

	if( has_worked ){
		if( !errors ){
			text = g_strdup_printf( _( "OK: %u lines from '%s' have been successfully "
					"imported into « %s »." ),
					count, priv->p1_furi, str );
		} else if( errors > 0 ){
			text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors "
					"during analyse and import phase.\n"
					"The « %s » recordset has been left unchanged.\n"
					"Please fix these errors, and retry then." ), priv->p1_furi, str );
		} else if( errors < 0 ){
			text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors "
					"during insertion phase.\n"
					"The « %s » recordset only contains the successfully inserted records.\n"
					"Please fix these errors, and retry then." ), priv->p1_furi, str );
		}
	} else {
		text = g_strdup_printf( _( "Unfortunately, the required file format is not "
				"managed at this time.\n"
				"Please fix this, and retry, then." ));
	}

	g_free( str );

	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( FALSE );
}

static guint
p5_do_import_csv( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	guint count;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), 0 );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));

	count = ofa_dossier_misc_import_csv(
			dossier, priv->p5_object, priv->p1_furi, priv->p3_import_settings, self, errors );

	return( count );
}

static guint
p5_do_import_other( ofaImportAssistant *self, guint *errors )
{
	ofaImportAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	guint count;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), 0 );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));

	*errors = ofa_iimportable_import_uri( priv->p5_plugin, dossier, self, NULL );
	count = ofa_iimportable_get_count( priv->p5_plugin );

	return( count );
}

static void
p5_on_progress( ofaIImporter *importer, ofeImportablePhase phase, gdouble progress, const gchar *text, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p5_import, "ofa-double", progress );
		g_signal_emit_by_name( priv->p5_import, "ofa-text", text );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p5_insert, "ofa-double", progress );
		g_signal_emit_by_name( priv->p5_insert, "ofa-text", text );
	}
}

static void
p5_on_pulse( ofaIImporter *importer, ofeImportablePhase phase, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = self->priv;

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		g_signal_emit_by_name( priv->p5_import, "ofa-pulse" );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		g_signal_emit_by_name( priv->p5_insert, "ofa-pulse" );
	}
}

static void
p5_on_message( ofaIImporter *importer, guint line_number, ofeImportableMsg status, const gchar *msg, ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_on_error";
	ofaImportAssistantPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar *str;

	g_debug( "%s: importer=%p, line_number=%u, msg=%s, self=%p",
			thisfn, ( void * ) importer, line_number, msg, ( void * ) self );

	priv = self->priv;

	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p5_text ));
	gtk_text_buffer_get_end_iter( buffer, &iter );
	str = g_strdup_printf( "[%u] %s\n", line_number, msg );
	gtk_text_buffer_insert( buffer, &iter, str, -1 );
	g_free( str );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

/*
 * settings is "folder;last_chosen_type;"
 */
static void
get_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = self->priv;

	list = ofa_settings_get_string_list( st_prefs_import );

	it = list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p2_type = atoi( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->p1_folder );
		priv->p1_folder = g_strdup( cstr );
	}

	ofa_settings_free_string_list( list );
}

static void
update_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gchar *str;

	priv = self->priv;

	str = g_strdup_printf( "%d;%s;",
			priv->p2_type,
			priv->p1_folder ? priv->p1_folder : "" );

	ofa_settings_set_string( st_prefs_import, str );

	g_free( str );
}
