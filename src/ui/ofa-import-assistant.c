/*
 * A double-entry accounting application for professional services.
 * Open Firm Accounting
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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
#include <string.h>

#include "my/my-iassistant.h"
#include "my/my-ibin.h"
#include "my/my-iprogress.h"
#include "my/my-isettings.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-import-duplicate.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofa-stream-format-bin.h"
#include "core/ofa-stream-format-disp.h"

#include "ui/ofa-import-assistant.h"

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select a file
 *   2   Content  TYPE     Select a datatype of import
 *   3   Content  IMPORTER Select a importer
 *   4   Content  BEHAVE   Configure the import behavior
 *   5   Content  SETTINGS Set the stream format
 *   6   Confirm  CONFIRM  Summary of the operations to be done
 *   7   Summary  DONE     After import
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_TYPE,
	ASSIST_PAGE_IMPORTER,
	ASSIST_PAGE_BEHAVE,
	ASSIST_PAGE_FORMAT,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;
	ofaIDBDossierMeta   *dossier_meta;

	/* runtime
	 */
	gchar               *settings_prefix;

	/* p0: introduction
	 */

	/* p1: select file to be imported
	 */
	GtkFileChooser      *p1_chooser;
	gchar               *p1_folder;
	gchar               *p1_furi;				/* the utf-8 imported filename */
	gchar               *p1_content;			/* guessed content */

	/* p2: select a type of data to be imported
	 */
	GtkWidget           *p2_furi;
	GtkWidget           *p2_content;
	GList               *p2_importables;
	GtkWidget           *p2_parent;
	gint                 p2_col;
	gint                 p2_row;
	GType                p2_selected_type;
	gchar               *p2_selected_label;
	GtkWidget           *p2_message;

	/* p3: select an importer
	 */
	GtkWidget           *p3_furi;
	GtkWidget           *p3_content;
	GtkWidget           *p3_datatype;
	GtkWidget           *p3_import_tview;
	GtkListStore        *p3_import_store;
	GList               *p3_importers;
	gchar               *p3_importer_label;
	ofaIImporter        *p3_importer_obj;
	GtkWidget           *p3_message;

	/* p4: configure the import behavior
	 */
	GtkWidget           *p4_furi;
	GtkWidget           *p4_content;
	GtkWidget           *p4_datatype;
	GtkWidget           *p4_importer;
	GtkWidget           *p4_empty_btn;
	GtkWidget           *p4_mode_combo;
	GtkWidget           *p4_stop_btn;
	GtkWidget           *p4_message;
	gboolean             p4_empty;
	ofeImportDuplicate   p4_import_mode;
	gboolean             p4_stop;

	/* p5: stream format
	 */
	GtkWidget           *p5_furi;
	GtkWidget           *p5_content;
	GtkWidget           *p5_datatype;
	GtkWidget           *p5_importer;
	GtkWidget           *p5_empty;
	GtkWidget           *p5_mode;
	GtkWidget           *p5_stop;
	ofaStreamFormat     *p5_import_settings;
	gboolean             p5_updatable;
	ofaStreamFormatBin  *p5_settings_prefs;
	GtkWidget           *p5_message;

	/* p6: confirm
	 */
	ofaStreamFormatDisp *p6_format;

	/* p7: import the file, display the result
	 */
	myProgressBar       *p7_import;
	myProgressBar       *p7_insert;
	GtkWidget           *p7_page;
	GtkWidget           *p7_text;
	guint                p7_phase;
	GtkWidget           *p7_bar;
	GtkTextBuffer       *p7_buffer;
}
	ofaImportAssistantPrivate;

/**
 * The columns stored in the page 3 importer store.
 */
enum {
	IMP_COL_LABEL = 0,
	IMP_COL_VERSION,
	IMP_COL_OBJECT,
	IMP_N_COLUMNS
};

/**
 * The columns stored in the page 4 import mode store
 */
enum {
	MODE_COL_MODE = 0,
	MODE_COL_LABEL,
	MODE_N_COLUMNS
};

/* data set against each of above radio buttons */
#define DATA_TYPE_INDEX                   "ofa-data-type"

static const gchar *st_import_folder    = "ofa-LastImportFolder";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-import-assistant.ui";

static const gchar *st_empty_true       = N_( "Empty the table before the first insertion" );
static const gchar *st_empty_false      = N_( "Do not empty the table before inserting imported datas" );
static const gchar *st_stop_true        = N_( "Stop the import operation on first error" );
static const gchar *st_stop_false       = N_( "Do not stop the import operation even if an error occurs" );

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p0_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
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
static void     p3_on_import_selection_changed( GtkTreeSelection *select, ofaImportAssistant *self );
static void     p3_on_import_selection_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaImportAssistant *self );
static gboolean p3_check_for_complete( ofaImportAssistant *self );
static void     p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_enum_cb( ofeImportDuplicate mode, const gchar *label, GtkListStore *store );
static void     p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p4_on_empty_toggled( GtkToggleButton *btn, ofaImportAssistant *self );
static void     p4_on_mode_changed( GtkComboBox *combo, ofaImportAssistant *self );
static void     p4_on_stop_toggled( GtkToggleButton *btn, ofaImportAssistant *self );
static gboolean p4_check_for_complete( ofaImportAssistant *self );
static void     p4_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p5_on_settings_changed( ofaStreamFormatBin *bin, ofaImportAssistant *self );
static void     p5_check_for_complete( ofaImportAssistant *self );
static void     p5_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static void     p7_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page );
static gboolean p7_confirm_empty_table( const ofaImportAssistant *self );
static gboolean p7_do_import( ofaImportAssistant *self );
static void     p7_do_user_cancelled( ofaImportAssistant *self );
static void     read_settings( ofaImportAssistant *self );
static void     write_settings( ofaImportAssistant *self );
static void     iprogress_iface_init( myIProgressInterface *iface );
static void     iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget );
static void     iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar );
static void     iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total );
static void     iprogress_set_text( myIProgress *instance, const void *worker, guint type, const gchar *text );

G_DEFINE_TYPE_EXTENDED( ofaImportAssistant, ofa_import_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaImportAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IPROGRESS, iprogress_iface_init )
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
		{ ASSIST_PAGE_TYPE,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_IMPORTER,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				( myIAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_BEHAVE,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_do_display,
				( myIAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_FORMAT,
				( myIAssistantCb ) p5_do_init,
				( myIAssistantCb ) p5_do_display,
				( myIAssistantCb ) p5_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myIAssistantCb ) p6_do_init,
				( myIAssistantCb ) p6_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myIAssistantCb ) p7_do_display,
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
	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->p1_folder );
	g_free( priv->p1_furi );
	g_free( priv->p1_content );
	g_free( priv->p2_selected_label );
	g_free( priv->p3_importer_label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->finalize( instance );
}

static void
import_assistant_dispose( GObject *instance )
{
	ofaImportAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORT_ASSISTANT( instance ));

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_IMPORT_ASSISTANT( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_list_free( priv->p2_importables );
		g_list_free_full( priv->p3_importers, ( GDestroyNotify ) g_object_unref );
		g_clear_object( &priv->p5_import_settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_import_assistant_parent_class )->dispose( instance );
}

static void
ofa_import_assistant_init( ofaImportAssistant *self )
{
	static const gchar *thisfn = "ofa_import_assistant_init";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORT_ASSISTANT( self ));

	priv = ofa_import_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->p3_importers = NULL;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_import_assistant_class_init( ofaImportAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_import_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = import_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = import_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_import_assistant_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run the assistant.
 */
void
ofa_import_assistant_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_import_assistant_run";
	ofaImportAssistant *self;
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_IMPORT_ASSISTANT, NULL );

	priv = ofa_import_assistant_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_import_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_import_assistant_iwindow_init";
	ofaImportAssistantPrivate *priv;
	ofaHub *hub;
	const ofaIDBConnect *connect;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	priv->dossier_meta = ofa_idbconnect_get_dossier_meta( connect );

	read_settings( OFA_IMPORT_ASSISTANT( instance ));
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_import_assistant_iassistant_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_willing_to_quit = iassistant_is_willing_to_quit;
}

static gboolean
iassistant_is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	return( ofa_prefs_assistant_is_willing_to_quit( priv->getter, keyval ));
}

static void
p0_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_import_assistant_p0_do_forward";

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));
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

	priv = ofa_import_assistant_get_instance_private( self );

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

	priv = ofa_import_assistant_get_instance_private( self );

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
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p1_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_import_assistant_get_instance_private( self );

	g_free( priv->p1_furi );
	priv->p1_furi = gtk_file_chooser_get_uri( priv->p1_chooser );

	ok = my_strlen( priv->p1_furi ) &&
			my_utils_uri_is_readable( priv->p1_furi );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p1_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p1_do_forward";
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( priv->p1_chooser );

	g_free( priv->p1_content );
	priv->p1_content = g_content_type_guess( priv->p1_furi, NULL, 0, NULL );

	g_debug( "%s: uri=%s, folder=%s", thisfn, priv->p1_furi, priv->p1_folder );
}

/*
 * p2: nature of the data to import
 */
static void
p2_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_init";
	ofaImportAssistantPrivate *priv;
	GList *it;
	gchar *label;
	GtkWidget *btn, *first;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	priv->p2_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_furi && GTK_IS_LABEL( priv->p2_furi ));
	my_style_add( priv->p2_furi, "labelinfo" );

	priv->p2_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-content" );
	g_return_if_fail( priv->p2_content && GTK_IS_LABEL( priv->p2_content ));
	my_style_add( priv->p2_content, "labelinfo" );

	/* expected data */
	priv->p2_importables = ofa_igetter_get_for_type( priv->getter, OFA_TYPE_IIMPORTABLE );
	g_debug( "%s: importables count=%d", thisfn, g_list_length( priv->p2_importables ));
	priv->p2_col = 1;
	priv->p2_row = 0;
	first = NULL;

	priv->p2_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-datatype-parent" );
	g_return_if_fail( priv->p2_parent && GTK_IS_GRID( priv->p2_parent ));

	for( it=priv->p2_importables ; it ; it=it->next ){
		label = ofa_iimportable_get_label( OFA_IIMPORTABLE( it->data ));
		if( my_strlen( label )){
			if( !first ){
				btn = gtk_radio_button_new_with_mnemonic( NULL, label );
				first = btn;
			} else {
				btn = gtk_radio_button_new_with_mnemonic_from_widget( GTK_RADIO_BUTTON( first ), label );
			}
			g_object_set_data( G_OBJECT( btn ), DATA_TYPE_INDEX, it->data );
			g_signal_connect( btn, "toggled", G_CALLBACK( p2_on_type_toggled ), self );
			gtk_grid_attach( GTK_GRID( priv->p2_parent ), btn, priv->p2_col, priv->p2_row++, 1, 1 );
		}
		g_free( label );
	}

	/* error message */
	priv->p2_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-message" );
	g_return_if_fail( priv->p2_message && GTK_IS_LABEL( priv->p2_message ));
	my_style_add( priv->p2_message, "labelerror" );
}

static void
p2_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p2_do_display";
	ofaImportAssistantPrivate *priv;
	gint i;
	GtkWidget *btn;
	ofaIImportable *object;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p2_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p2_content ), priv->p1_content );

	/* importables */
	if( priv->p2_selected_type > 0 ){
		for( i=0 ; i<priv->p2_row ; ++i ){
			btn = gtk_grid_get_child_at( GTK_GRID( priv->p2_parent ), priv->p2_col, i );
			g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
			object = OFA_IIMPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
			if( G_OBJECT_TYPE( object ) == priv->p2_selected_type ){
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
				break;
			}
		}
	}

	p2_check_for_complete( self );
}

static void
p2_on_type_toggled( GtkToggleButton *button, ofaImportAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean complete;
	gint i;
	GtkWidget *btn;
	ofaIImportable *object;
	const gchar *label;

	priv = ofa_import_assistant_get_instance_private( self );

	complete = TRUE;
	gtk_label_set_text( GTK_LABEL( priv->p2_message ), "" );

	/* do we have a currently active button ? */
	priv->p2_selected_type = 0;
	g_free( priv->p2_selected_label );
	priv->p2_selected_label = NULL;

	if( complete ){
		for( i=0 ; i<priv->p2_row ; ++i ){
			btn = gtk_grid_get_child_at( GTK_GRID( priv->p2_parent ), priv->p2_col, i );
			if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
				object = OFA_IIMPORTABLE( g_object_get_data( G_OBJECT( btn ), DATA_TYPE_INDEX ));
				priv->p2_selected_type = G_OBJECT_TYPE( object );
				label = gtk_button_get_label( GTK_BUTTON( btn ));
				priv->p2_selected_label = my_utils_str_remove_underlines( label );
				break;
			}
		}
		if( !priv->p2_selected_type ){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p2_message ), _( "No selected data type" ));
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

static void
p2_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	g_return_if_fail( priv->p2_selected_type > 0 );
	g_return_if_fail( my_strlen( priv->p2_selected_label ) > 0 );
}

/*
 * p3: select the importer
 */
static void
p3_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label_widget;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *select;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	priv->p3_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_furi && GTK_IS_LABEL( priv->p3_furi ));
	my_style_add( priv->p3_furi, "labelinfo" );

	priv->p3_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-content" );
	g_return_if_fail( priv->p3_content && GTK_IS_LABEL( priv->p3_content ));
	my_style_add( priv->p3_content, "labelinfo" );

	priv->p3_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-datatype" );
	g_return_if_fail( priv->p3_datatype && GTK_IS_LABEL( priv->p3_datatype ));
	my_style_add( priv->p3_datatype, "labelinfo" );

	/* available importers */
	priv->p3_import_tview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-treeview" );
	g_return_if_fail( priv->p3_import_tview && GTK_IS_TREE_VIEW( priv->p3_import_tview ));

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					"Label", cell, "text", IMP_COL_LABEL, NULL );
	gtk_tree_view_column_set_alignment( column, 0 );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->p3_import_tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					"Version", cell, "text", IMP_COL_VERSION, NULL );
	gtk_tree_view_column_set_alignment( column, 0 );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->p3_import_tview ), column );

	priv->p3_import_store = gtk_list_store_new( IMP_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT );
	gtk_tree_view_set_model( GTK_TREE_VIEW( priv->p3_import_tview ), GTK_TREE_MODEL( priv->p3_import_store ));
	g_object_unref( priv->p3_import_store );

	label_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "label32" );
	g_return_if_fail( label_widget && GTK_IS_LABEL( label_widget ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label_widget ), priv->p3_import_tview );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_import_tview ));
	g_signal_connect( select, "changed", G_CALLBACK( p3_on_import_selection_changed ), self );
	g_signal_connect( priv->p3_import_tview, "row-activated", G_CALLBACK( p3_on_import_selection_activated ), self );

	/* error message */
	priv->p3_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" );
	g_return_if_fail( priv->p3_message && GTK_IS_LABEL( priv->p3_message ));
	my_style_add( priv->p3_message, "labelerror" );
}

static void
p3_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p3_do_display";
	ofaImportAssistantPrivate *priv;
	GList *it;
	gchar *label, *version;
	GtkTreeIter iter;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p3_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p3_content ), priv->p1_content );
	gtk_label_set_text( GTK_LABEL( priv->p3_datatype ), priv->p2_selected_label );

	/* importers */
	if( priv->p3_importers ){
		g_list_free_full( priv->p3_importers, ( GDestroyNotify ) g_object_unref );
		priv->p3_importers = NULL;
		gtk_list_store_clear( priv->p3_import_store );
	}

	priv->p3_importers = ofa_iimporter_find_willing_to( priv->getter, priv->p1_furi, priv->p2_selected_type );
	for( it=priv->p3_importers ; it ; it=it->next ){
		label = ofa_iimporter_get_display_name( OFA_IIMPORTER( it->data ));
		version = ofa_iimporter_get_version( OFA_IIMPORTER( it->data ));
		if( my_strlen( label )){
			gtk_list_store_insert_with_values( priv->p3_import_store, &iter, -1,
					IMP_COL_LABEL,   label,
					IMP_COL_VERSION, version ? version : "",
					IMP_COL_OBJECT,  it->data,
					-1 );
		}
		g_free( version );
		g_free( label );
	}

	p3_check_for_complete( self );
}

static void
p3_on_import_selection_changed( GtkTreeSelection *select, ofaImportAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_on_import_selection_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaImportAssistant *self )
{
	if( p3_check_for_complete( self )){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p3_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean complete;
	GtkTreeSelection *select;

	priv = ofa_import_assistant_get_instance_private( self );

	complete = TRUE;
	gtk_label_set_text( GTK_LABEL( priv->p3_message ), "" );

	/* do we have a selected importer */
	if( complete ){
		if( !g_list_length( priv->p3_importers )){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p3_message ), _( "No available importer" ));
		}
	}
	if( complete ){
		select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_import_tview ));
		if( !gtk_tree_selection_get_selected( select, NULL, NULL )){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p3_message ), _( "No selected importer" ));
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );

	return( complete );
}

static void
p3_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	ofaImportAssistantPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = ofa_import_assistant_get_instance_private( self );

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_import_tview ));
	gtk_tree_selection_get_selected( select, &tmodel, &iter );
	gtk_tree_model_get( tmodel, &iter, IMP_COL_LABEL, &priv->p3_importer_label, IMP_COL_OBJECT, &priv->p3_importer_obj, -1 );
	g_return_if_fail( priv->p3_importer_obj && OFA_IS_IIMPORTER( priv->p3_importer_obj ));
	g_object_unref( priv->p3_importer_obj );
}

/*
 * p4: configure the import behavior
 */
static void
p4_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_init";
	ofaImportAssistantPrivate *priv;
	GtkCellRenderer *cell;
	GtkListStore *store;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	priv->p4_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( priv->p4_furi && GTK_IS_LABEL( priv->p4_furi ));
	my_style_add( priv->p4_furi, "labelinfo" );

	priv->p4_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-content" );
	g_return_if_fail( priv->p4_content && GTK_IS_LABEL( priv->p4_content ));
	my_style_add( priv->p4_content, "labelinfo" );

	priv->p4_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-datatype" );
	g_return_if_fail( priv->p4_datatype && GTK_IS_LABEL( priv->p4_datatype ));
	my_style_add( priv->p4_datatype, "labelinfo" );

	priv->p4_importer = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-importer" );
	g_return_if_fail( priv->p4_importer && GTK_IS_LABEL( priv->p4_importer ));
	my_style_add( priv->p4_importer, "labelinfo" );

	/* import behavior */
	priv->p4_empty_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-empty" );
	g_return_if_fail( priv->p4_empty_btn && GTK_IS_CHECK_BUTTON( priv->p4_empty_btn ));
	g_signal_connect( priv->p4_empty_btn, "toggled", G_CALLBACK( p4_on_empty_toggled ), self );

	priv->p4_mode_combo = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-mode" );
	g_return_if_fail( priv->p4_mode_combo && GTK_IS_COMBO_BOX( priv->p4_mode_combo ));

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->p4_mode_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->p4_mode_combo ), cell, "text", MODE_COL_LABEL );

	gtk_combo_box_set_id_column( GTK_COMBO_BOX( priv->p4_mode_combo ), MODE_COL_MODE );

	g_signal_connect( priv->p4_mode_combo, "changed", G_CALLBACK( p4_on_mode_changed ), self );

	store = gtk_list_store_new( MODE_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->p4_mode_combo ), GTK_TREE_MODEL( store ));
	g_object_unref( store );

	ofa_import_duplicate_enum(( ImportDuplicateEnumCb ) p4_enum_cb, store );

	priv->p4_stop_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-stop" );
	g_return_if_fail( priv->p4_stop_btn && GTK_IS_CHECK_BUTTON( priv->p4_stop_btn ));
	g_signal_connect( priv->p4_stop_btn, "toggled", G_CALLBACK( p4_on_stop_toggled ), self );

	/* error message */
	priv->p4_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-message" );
	g_return_if_fail( priv->p4_message && GTK_IS_LABEL( priv->p4_message ));
	my_style_add( priv->p4_message, "labelerror" );
}

static void
p4_enum_cb( ofeImportDuplicate mode, const gchar *label, GtkListStore *store )
{
	GtkTreeIter iter;
	gchar *str;

	str = g_strdup_printf( "%d", mode );

	gtk_list_store_insert_with_values( store, &iter, -1,
			MODE_COL_MODE,  str,
			MODE_COL_LABEL, label,
			-1 );

	g_free( str );
}

static void
p4_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p4_do_display";
	ofaImportAssistantPrivate *priv;
	gchar *str;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p4_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p4_content ), priv->p1_content );
	gtk_label_set_text( GTK_LABEL( priv->p4_datatype ), priv->p2_selected_label );
	gtk_label_set_text( GTK_LABEL( priv->p4_importer ), priv->p3_importer_label );

	/* import behavior */
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p4_empty_btn ), priv->p4_empty );

	str = g_strdup_printf( "%d", priv->p4_import_mode );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->p4_mode_combo ), str );
	g_free( str );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p4_stop_btn ), priv->p4_stop );

	p4_check_for_complete( self );
}

static void
p4_on_empty_toggled( GtkToggleButton *btn, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	priv->p4_empty = gtk_toggle_button_get_active( btn );
}

static void
p4_on_mode_changed( GtkComboBox *combo, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	const gchar *cstr;

	priv = ofa_import_assistant_get_instance_private( self );

	cstr = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->p4_mode_combo ));
	if( my_strlen( cstr )){
		priv->p4_import_mode = atoi( cstr );
	}

	p4_check_for_complete( self );
}

static void
p4_on_stop_toggled( GtkToggleButton *btn, ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( self );

	priv->p4_stop = gtk_toggle_button_get_active( btn );
}

static gboolean
p4_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean complete;
	const gchar *cstr;

	priv = ofa_import_assistant_get_instance_private( self );

	complete = TRUE;
	gtk_label_set_text( GTK_LABEL( priv->p4_message ), "" );

	/* do we have a selected duplicate behavior */
	if( complete ){
		cstr = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->p4_mode_combo ));
		if( !my_strlen( cstr )){
			complete = FALSE;
			gtk_label_set_text( GTK_LABEL( priv->p4_message ), _( "No selected behavior for duplicates" ));
		} else {
			priv->p4_import_mode = atoi( cstr );
			if( priv->p4_import_mode < 1 ){
				complete = FALSE;
				gtk_label_set_text( GTK_LABEL( priv->p4_message ), _( "No selected behavior for duplicates" ));
			}
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );

	return( complete );
}

static void
p4_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
}

/*
 * p5: stream format
 *
 * These are initialized with the import settings for this name, or
 * with default settings - unless the importer itself provides its
 * own format.
 */
static void
p5_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent, *label;
	GtkSizeGroup *hgroup, *group_bin;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	priv->p5_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-furi" );
	g_return_if_fail( priv->p5_furi && GTK_IS_LABEL( priv->p5_furi ));
	my_style_add( priv->p5_furi, "labelinfo" );

	priv->p5_content = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-content" );
	g_return_if_fail( priv->p5_content && GTK_IS_LABEL( priv->p5_content ));
	my_style_add( priv->p5_content, "labelinfo" );

	priv->p5_datatype = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-datatype" );
	g_return_if_fail( priv->p5_datatype && GTK_IS_LABEL( priv->p5_datatype ));
	my_style_add( priv->p5_datatype, "labelinfo" );

	priv->p5_importer = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-importer" );
	g_return_if_fail( priv->p5_importer && GTK_IS_LABEL( priv->p5_importer ));
	my_style_add( priv->p5_importer, "labelinfo" );

	priv->p5_empty = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-empty" );
	g_return_if_fail( priv->p5_empty && GTK_IS_LABEL( priv->p5_empty ));
	my_style_add( priv->p5_empty, "labelinfo" );

	priv->p5_mode = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-mode" );
	g_return_if_fail( priv->p5_mode && GTK_IS_LABEL( priv->p5_mode ));
	my_style_add( priv->p5_mode, "labelinfo" );

	priv->p5_stop = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-stop" );
	g_return_if_fail( priv->p5_stop && GTK_IS_LABEL( priv->p5_stop ));
	my_style_add( priv->p5_stop, "labelinfo" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-furi-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-content-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-data-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-importer-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-empty-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-mode-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-stop-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	/* stream format */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-settings-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p5_settings_prefs = ofa_stream_format_bin_new( NULL );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_settings_prefs ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->p5_settings_prefs ), 0 ))){
		my_utils_size_group_add_size_group( hgroup, group_bin );
	}

	g_signal_connect(
			priv->p5_settings_prefs, "ofa-changed", G_CALLBACK( p5_on_settings_changed ), self );

	/* error message */
	priv->p5_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-message" );
	g_return_if_fail( priv->p5_message && GTK_IS_LABEL( priv->p5_message ));
	my_style_add( priv->p5_message, "labelerror" );

	g_object_unref( hgroup );
}

static void
p5_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_init";
	ofaImportAssistantPrivate *priv;
	const gchar *found_key, *class_name;
	gchar *str;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	/* previously set */
	gtk_label_set_text( GTK_LABEL( priv->p5_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p5_content ), priv->p1_content );
	gtk_label_set_text( GTK_LABEL( priv->p5_datatype ), priv->p2_selected_label );
	gtk_label_set_text( GTK_LABEL( priv->p5_importer ), priv->p3_importer_label );
	gtk_label_set_text( GTK_LABEL( priv->p5_empty ), gettext( priv->p4_empty ? st_empty_true : st_empty_false ));
	str = ofa_import_duplicate_get_label( priv->p4_import_mode );
	gtk_label_set_text( GTK_LABEL( priv->p5_mode ), str );
	g_free( str );
	gtk_label_set_text( GTK_LABEL( priv->p5_stop ), gettext( priv->p4_stop ? st_stop_true : st_stop_false ));

	/* stream format */
	priv->p5_updatable = TRUE;
	g_clear_object( &priv->p5_import_settings );

	priv->p5_import_settings = ofa_iimporter_get_default_format( priv->p3_importer_obj, priv->getter, &priv->p5_updatable );

	if( !priv->p5_import_settings ){
		found_key = NULL;
		class_name = g_type_name( priv->p2_selected_type );
		if( ofa_stream_format_exists( priv->getter, class_name, OFA_SFMODE_IMPORT )){
			found_key = class_name;
		}
		priv->p5_import_settings = ofa_stream_format_new( priv->getter, found_key, OFA_SFMODE_IMPORT );
		if( !found_key ){
			ofa_stream_format_set_name( priv->p5_import_settings, class_name );
		}
	}

	ofa_stream_format_bin_set_format( priv->p5_settings_prefs, priv->p5_import_settings );
	ofa_stream_format_bin_set_mode_sensitive( priv->p5_settings_prefs, FALSE );
	ofa_stream_format_bin_set_updatable( priv->p5_settings_prefs, priv->p5_updatable );

	p5_check_for_complete( self );
}

static void
p5_on_settings_changed( ofaStreamFormatBin *bin, ofaImportAssistant *self )
{
	p5_check_for_complete( self );
}

static void
p5_check_for_complete( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_import_assistant_get_instance_private( self );

	ok = my_ibin_is_valid( MY_IBIN( priv->p5_settings_prefs ), &message );

	gtk_label_set_text( GTK_LABEL( priv->p5_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p5_do_forward( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p5_do_forward";
	ofaImportAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->p5_settings_prefs ));
}

/*
 * ask the user to confirm the operation
 */
static void
p6_do_init( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p6_do_init";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label, *parent;
	GtkSizeGroup *group, *group_bin;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-source-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-target-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-importer-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-behavior-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-stream-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( group, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-stream-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->p6_format = ofa_stream_format_disp_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p6_format ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->p6_format ), 0 ))){
		my_utils_size_group_add_size_group( group, group_bin );
	}

	g_object_unref( group );
}

static void
p6_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p6_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gboolean complete;
	gchar *str, *str2;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_import_assistant_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-furi" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_furi );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-content" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p1_content );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-type" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_selected_label );

	str = ofa_iimporter_get_version( priv->p3_importer_obj );
	str2 = g_strdup_printf( "%s %s", priv->p3_importer_label, str );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-importer" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ),  str2 );
	g_free( str2 );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-empty" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), gettext( priv->p4_empty ? st_empty_true : st_empty_false ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-import-mode" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	str = ofa_import_duplicate_get_label( priv->p4_import_mode );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-stop" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_style_add( label, "labelinfo" );
	gtk_label_set_text( GTK_LABEL( label ), gettext( priv->p4_stop ? st_stop_true : st_stop_false ));

	ofa_stream_format_disp_set_format( priv->p6_format, priv->p5_import_settings );

	complete = my_strlen( priv->p1_furi ) > 0;
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

/*
 * import
 * execution summary
 */
static void
p7_do_display( ofaImportAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_import_assistant_p7_do_display";
	ofaImportAssistantPrivate *priv;
	GtkWidget *parent;

	g_return_if_fail( OFA_IS_IMPORT_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	priv = ofa_import_assistant_get_instance_private( self );

	priv->p7_page = page;
	priv->p7_phase = 0;
	priv->p7_bar = NULL;

	if( !priv->p4_empty || p7_confirm_empty_table( self )){

		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-import-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->p7_import = my_progress_bar_new();
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p7_import ));

		parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-insert-parent" );
		g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
		priv->p7_insert = my_progress_bar_new();
		gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p7_insert ));

		priv->p7_text = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-textview" );
		g_return_if_fail( priv->p7_text && GTK_IS_TEXT_VIEW( priv->p7_text ));

		priv->p7_buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p7_text ));
		gtk_text_buffer_create_tag( priv->p7_buffer, "error", "foreground", "red", NULL );

		gtk_widget_show_all( page );

		g_idle_add(( GSourceFunc ) p7_do_import, self );

	} else {
		p7_do_user_cancelled( self );
	}
}

static gboolean
p7_confirm_empty_table( const ofaImportAssistant *self )
{
	gboolean ok;
	GtkWindow *toplevel;

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ok = my_utils_dialog_question(
				toplevel,
				_( "You have asked to fully drop the previously content of the target "
					"table before importing these new datas.\n"
					"Are you sure ?" ),
				_( "C_onfirm" ));

	return( ok );
}

static gboolean
p7_do_import( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GtkWidget *label;
	gchar *text;
	guint errors;
	const gchar *style;
	ofsImporterParms parms;

	priv = ofa_import_assistant_get_instance_private( self );

	memset( &parms, '\0', sizeof( parms ));
	parms.version = 1;
	parms.getter = priv->getter;
	parms.empty = priv->p4_empty;
	parms.mode = priv->p4_import_mode;
	parms.stop = priv->p4_stop;
	parms.uri = priv->p1_furi;
	parms.type = priv->p2_selected_type;
	parms.format = priv->p5_import_settings;
	parms.progress = MY_IPROGRESS( self );

	text = NULL;
	style = NULL;
	errors = ofa_iimporter_import( priv->p3_importer_obj, &parms );

	/* then display the result */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p7-label" );

	if( errors == 0 ){
		if( parms.inserted_count == 0 ){
			text = g_strdup_printf( _( "No line from '%s' has been imported." ),
					priv->p1_furi );
		} else if( parms.inserted_count == 1 ){
			text = g_strdup_printf( _( "OK: one line from '%s' has been successfully "
					"imported into « %s »." ), priv->p1_furi, priv->p2_selected_label );
		} else {
			text = g_strdup_printf( _( "OK: %d lines from '%s' have been successfully "
					"imported into « %s »." ),
					parms.inserted_count, priv->p1_furi, priv->p2_selected_label );
		}
		style = "labelinfo";

	} else if( parms.parse_errs > 0 ){
		text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors "
				"during analyse and parsing phase.\n"
				"The « %s » recordset has been left unchanged.\n"
				"Please fix these errors, and retry." ), priv->p1_furi, priv->p2_selected_label );
		style = "labelerror";

	} else if( parms.insert_errs > 0 ){
		text = g_strdup_printf( _( "Unfortunately, '%s' import has encountered errors "
				"during insertion phase.\n"
				"The « %s » recordset has been restored to its initial state.\n"
				"Please fix these errors, and retry." ), priv->p1_furi, priv->p2_selected_label );
		style = "labelerror";
	}

	if( text ){
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
		my_style_add( label, style );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	/* do not continue and remove from idle callbacks list */
	return( FALSE );
}

static void
p7_do_user_cancelled( ofaImportAssistant *self )
{
	GtkWidget *label;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p7-label" );
	gtk_label_set_text( GTK_LABEL( label ), _( "Import has been cancelled on user decision." ));
	my_style_add( label, "labelinfo" );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
}

/*
 * user settings are: class_name;empty;import_mode;stop;
 * dossier settings are: last_import_folder_uri
 */
static void
read_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr, *group;
	myISettings *settings;
	gchar *key;

	priv = ofa_import_assistant_get_instance_private( self );

	/* user settings
	 */
	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p2_selected_type = g_type_from_name( cstr );
	}
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p4_empty = my_utils_boolean_from_str( cstr );
	}
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p4_import_mode = atoi( cstr );
	}
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p4_stop = my_utils_boolean_from_str( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );

	/* dossier settings
	 */
	settings = ofa_igetter_get_dossier_settings( priv->getter );
	group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

	priv->p1_folder = my_isettings_get_string( settings, group, st_import_folder );
}

static void
write_settings( ofaImportAssistant *self )
{
	ofaImportAssistantPrivate *priv;
	myISettings *settings;
	gchar *key, *str;
	const gchar *group;

	priv = ofa_import_assistant_get_instance_private( self );

	/* user settings
	 */
	str = g_strdup_printf( "%s;%s;%d;%s;",
			priv->p2_selected_type ? g_type_name( priv->p2_selected_type ) : "",
			priv->p4_empty ? "True":"False",
			priv->p4_import_mode,
			priv->p4_stop ? "True":"False" );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );

	/* dossier settings
	 */
	settings = ofa_igetter_get_dossier_settings( priv->getter );
	group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

	if( my_strlen( priv->p1_folder )){
		my_isettings_set_string( settings, group, st_import_folder, priv->p1_folder );
	}
}

/*
 * myIProgress interface management
 */
static void
iprogress_iface_init( myIProgressInterface *iface )
{
	static const gchar *thisfn = "ofa_import_assistant_iprogress_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->start_work = iprogress_start_work;
	iface->start_progress = iprogress_start_progress;
	iface->pulse = iprogress_pulse;
	iface->set_text = iprogress_set_text;
}

/*
 * worker = ofaIImporter instance
 * widget = NULL
 *
 * Nothing to do here for now
 */
static void
iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget )
{
}

/*
 * worker = ofaIImporter instance
 * widget = NULL
 * with_bar = TRUE
 *
 * Called twice, first for analyse phase, second for insert phase.
 * Increment p7_phase and setup the target progress bar
 */
static void
iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar )
{
	ofaImportAssistantPrivate *priv;

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	priv->p7_phase += 1;

	if( priv->p7_phase == 1 ){
		priv->p7_bar = GTK_WIDGET( priv->p7_import );

	} else if( priv->p7_phase == 2 ){
		priv->p7_bar = GTK_WIDGET( priv->p7_insert );

	} else {
		priv->p7_bar = NULL;
	}
}

static void
iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total )
{
	ofaImportAssistantPrivate *priv;
	gdouble progress;
	gchar *str;

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	if( priv->p7_bar ){
		if( total ){
			progress = ( gdouble ) count / ( gdouble ) total;
			g_signal_emit_by_name( priv->p7_bar, "my-double", progress );
		}

		str = g_strdup_printf( "%lu/%lu", count, total );
		g_signal_emit_by_name( priv->p7_bar, "my-text", str );
		g_free( str );
	}
}

static void
iprogress_set_text( myIProgress *instance, const void *worker, guint type, const gchar *text )
{
	ofaImportAssistantPrivate *priv;
	GtkTextIter iter;
	gchar *str;
	GtkAdjustment* adjustment;

	priv = ofa_import_assistant_get_instance_private( OFA_IMPORT_ASSISTANT( instance ));

	str = g_strdup_printf( "%s\n", text );
	gtk_text_buffer_get_end_iter( priv->p7_buffer, &iter );
	if( type == MY_PROGRESS_ERROR ){
		gtk_text_buffer_insert_with_tags_by_name( priv->p7_buffer, &iter, str, -1, "error", NULL );
	} else {
		gtk_text_buffer_insert( priv->p7_buffer, &iter, str, -1 );
	}
	g_free( str );

	adjustment = gtk_scrollable_get_vadjustment( GTK_SCROLLABLE( priv->p7_text ));
	gtk_adjustment_set_value( adjustment, gtk_adjustment_get_upper( adjustment ));

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}
