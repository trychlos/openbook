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
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/my-progress-bar.h"
#include "core/ofa-admin-credentials-bin.h"
#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-file-dir.h"

#include "ui/ofa-application.h"
#include "ui/ofa-dossier-new-mini.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-restore-assistant.h"
#include "ui/ofa-main-window.h"

/* Export Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select a file
 *   2   Content  TARGET   Select dossier target
 *   3   Content  ROOT     Enter DBMS root account
 *   4   Content  ADMIN    Enter Dossier admin account
 *   5   Confirm  CONFIRM  Summary of the operations to be done
 *   6   Summary  DONE     After restore
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_TARGET,
	ASSIST_PAGE_ROOT,
	ASSIST_PAGE_ADMIN,
	ASSIST_PAGE_CONFIRM,
	ASSIST_PAGE_DONE
};

/* a structure to store the data needed to open the dossier
 * after the assistant has ran
 */
typedef struct {
	gboolean       open;
	ofaMainWindow *main_window;
	ofaIDBMeta    *meta;
	ofaIDBPeriod  *period;
	gchar         *account;
	gchar         *password;
}
	sOpenData;

/* private instance data
 */
struct _ofaRestoreAssistantPrivate {

	/* p1: select file to be imported
	 */
	GtkFileChooser         *p1_chooser;
	gchar                  *p1_folder;
	gchar                  *p1_furi;		/* the utf-8 to be restored file uri */
	gint                    p1_filter;

	/* p2: select the dossier target
	 */
	GtkWidget              *p2_furi;
	ofaDossierTreeview     *p2_dossier_treeview;
	GtkWidget              *p2_new_dossier_btn;
	ofaIDBMeta             *p2_meta;
	ofaIDBPeriod           *p2_period;
	gchar                  *p2_dbname;
	gboolean                p2_is_new_dossier;

	/* p3: DBMS root account
	 */
	GtkSizeGroup           *p3_hgroup;
	GtkWidget              *p3_furi;
	GtkWidget              *p3_dossier;
	ofaDBMSRootBin         *p3_dbms_credentials;
	gchar                  *p3_account;
	gchar                  *p3_password;
	GtkWidget              *p3_message;
	ofaIDBConnect          *p3_connect;

	/* p4: dossier administrative credentials
	 */
	GtkWidget              *p4_furi;
	GtkWidget              *p4_dossier;
	GtkWidget              *p4_database;
	ofaAdminCredentialsBin *p4_admin_credentials;
	gchar                  *p4_account;
	gchar                  *p4_password;
	GtkWidget              *p4_open_btn;
	gboolean                p4_open;
	GtkWidget              *p4_message;

	/* p5: display operations to be done and ask for confirmation
	 */
	GtkWidget              *p5_furi;
	GtkWidget              *p5_dossier;
	GtkWidget              *p5_database;
	GtkWidget              *p5_root_account;
	GtkWidget              *p5_root_password;
	GtkWidget              *p5_admin_account;
	GtkWidget              *p5_admin_password;
	GtkWidget              *p5_open;

	/* p6: restore the file, display the result
	 */
	GtkWidget              *p6_page;
	GtkWidget              *p6_label1;
	GtkWidget              *p6_label2;

	/* open the dossier after restore
	 */
	sOpenData              *sdata;
};

/* GtkFileChooser filters
 */
enum {
	FILE_CHOOSER_ALL = 1,
	FILE_CHOOSER_GZ
};

typedef struct {
	gint         type;
	const gchar *pattern;
	const gchar *name;
}
	sFilter;

static sFilter st_filters[] = {
		{ FILE_CHOOSER_ALL, "*",    N_( "All files (*)" )},
		{ FILE_CHOOSER_GZ,  "*.gz", N_( "Backup files (*.gz)" )},
		{ 0 }
};

#define CHOOSER_FILTER_TYPE             "file-chooser-filter-type"

/* the user preferences stored as a string list
 * folder
 */
static const gchar *st_prefs_import     = "RestoreAssistant-settings";

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-restore-assistant.ui";
static const gchar *st_ui_id            = "RestoreAssistant";

static void     p1_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p1_set_filters( ofaRestoreAssistant *self, GtkFileChooser *chooser );
static void     p1_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static void     p1_on_file_activated( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static gboolean p1_check_for_complete( ofaRestoreAssistant *self );
static void     p1_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void     p2_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p2_on_dossier_changed( ofaDossierTreeview *view, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaRestoreAssistant *assistant );
static void     p2_on_dossier_activated( ofaDossierTreeview *view, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaRestoreAssistant *assistant );
static void     p2_on_dossier_new( GtkButton *button, ofaRestoreAssistant *assistant );
static gboolean p2_check_for_complete( ofaRestoreAssistant *self );
static void     p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void     p3_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p3_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self );
static void     p3_check_for_complete( ofaRestoreAssistant *self );
static void     p3_set_message( ofaRestoreAssistant *self, const gchar *message );
static void     p3_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p4_on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self );
static void     p4_on_open_toggled( GtkToggleButton *button, ofaRestoreAssistant *self );
static void     p4_check_for_complete( ofaRestoreAssistant *self );
static void     p4_set_message( ofaRestoreAssistant *self, const gchar *message );
static void     p4_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static gboolean p6_restore_confirmed( const ofaRestoreAssistant *self );
static gboolean p6_do_restore( ofaRestoreAssistant *self );
static gboolean p6_do_open( ofaRestoreAssistant *self );
static void     get_settings( ofaRestoreAssistant *self );
static void     update_settings( ofaRestoreAssistant *self );

G_DEFINE_TYPE( ofaRestoreAssistant, ofa_restore_assistant, MY_TYPE_ASSISTANT );

static const ofsAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_SELECT,
				( myAssistantCb ) p1_do_init,
				( myAssistantCb ) p1_do_display,
				( myAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_TARGET,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_do_display,
				( myAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_ROOT,
				( myAssistantCb ) p3_do_init,
				( myAssistantCb ) p3_do_display,
				( myAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_ADMIN,
				( myAssistantCb ) p4_do_init,
				( myAssistantCb ) p4_do_display,
				( myAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myAssistantCb ) p5_do_init,
				( myAssistantCb ) p5_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				( myAssistantCb ) p6_do_init,
				( myAssistantCb ) p6_do_display,
				NULL },
		{ -1 }
};

static void
restore_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_restore_assistant_finalize";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RESTORE_ASSISTANT( instance ));

	/* free data members here */
	priv = OFA_RESTORE_ASSISTANT( instance )->priv;

	g_free( priv->p1_folder );
	g_free( priv->p1_furi );
	g_free( priv->p2_dbname );
	g_free( priv->p3_account );
	g_free( priv->p3_password );
	g_free( priv->p4_account );
	g_free( priv->p4_password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_restore_assistant_parent_class )->finalize( instance );
}

static void
restore_assistant_dispose( GObject *instance )
{
	ofaRestoreAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RESTORE_ASSISTANT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_RESTORE_ASSISTANT( instance )->priv;

		/* unref object members here */
		g_clear_object( &priv->p2_meta );
		g_clear_object( &priv->p2_period );
		g_clear_object( &priv->p3_hgroup );
		g_clear_object( &priv->p3_connect );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_restore_assistant_parent_class )->dispose( instance );
}

static void
ofa_restore_assistant_init( ofaRestoreAssistant *self )
{
	static const gchar *thisfn = "ofa_restore_assistant_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RESTORE_ASSISTANT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_RESTORE_ASSISTANT, ofaRestoreAssistantPrivate );
}

static void
ofa_restore_assistant_class_init( ofaRestoreAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_restore_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = restore_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = restore_assistant_finalize;

	g_type_class_add_private( klass, sizeof( ofaRestoreAssistantPrivate ));
}

/**
 * Run the assistant.
 *
 * @main: the main window of the application.
 */
void
ofa_restore_assistant_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_restore_assistant_run";
	ofaRestoreAssistant *self;
	sOpenData *data;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_RESTORE_ASSISTANT,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_WINDOW_XML,  st_ui_xml,
							MY_PROP_WINDOW_NAME, st_ui_id,
							NULL );

	get_settings( self );

#if 0
	GtkAssistant *assistant = my_assistant_get_assistant( MY_ASSISTANT( self ));
	gint n_pages = gtk_assistant_get_n_pages( assistant );
	gint i;
	for( i=0 ; i<n_pages ; ++i ){
		GtkWidget *page = gtk_assistant_get_nth_page( assistant, i );
		g_debug( "ofa_restore_assistant_run: page_num=%d, page_widget=%p", i, ( void * ) page );
	}
#endif

	data = g_new0( sOpenData, 1 );
	self->priv->sdata = data;

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );
	my_assistant_run( MY_ASSISTANT( self ));

	/* open the dossier after the assistant has quit */
	if( data->open ){
		ofa_dossier_open_run(
				data->main_window, data->meta, data->period, data->account, data->password );
		g_free( data->password );
		g_free( data->account );
		g_clear_object( &data->period );
		g_clear_object( &data->meta );
	}

	g_free( data );
}

/*
 * initialize the GtkFileChooser widget with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p1_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p1_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p1_chooser = GTK_FILE_CHOOSER( widget );

	p1_set_filters( self, priv->p1_chooser );

	g_signal_connect(
			G_OBJECT( widget ), "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	g_signal_connect(
			G_OBJECT( widget ), "file-activated", G_CALLBACK( p1_on_file_activated ), self );
}

static void
p1_set_filters( ofaRestoreAssistant *self, GtkFileChooser *chooser )
{
	ofaRestoreAssistantPrivate *priv;
	gint i;
	GtkFileFilter *filter, *selected;

	priv = self->priv;
	selected = NULL;
	for( i=0 ; st_filters[i].type ; ++i ){
		filter = gtk_file_filter_new();
		gtk_file_filter_set_name( filter, gettext( st_filters[i].name ));
		gtk_file_filter_add_pattern( filter, st_filters[i].pattern );
		gtk_file_chooser_add_filter( chooser, filter );
		g_object_set_data(
				G_OBJECT( filter ), CHOOSER_FILTER_TYPE, GINT_TO_POINTER( st_filters[i].type ));

		if( st_filters[i].type == priv->p1_filter ){
			selected = filter;
		}
	}

	if( selected ){
		gtk_file_chooser_set_filter( chooser, selected );
	}
}

static void
p1_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p1_do_display";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	if( priv->p1_folder ){
		gtk_file_chooser_set_current_folder_uri( priv->p1_chooser, priv->p1_folder );
	}

	p1_check_for_complete( self );
}

static void
p1_on_selection_changed( GtkFileChooser *chooser, ofaRestoreAssistant *self )
{
	p1_check_for_complete( self );
}

static void
p1_on_file_activated( GtkFileChooser *chooser, ofaRestoreAssistant *self )
{
	if( p1_check_for_complete( self )){
		gtk_assistant_next_page( my_assistant_get_assistant( MY_ASSISTANT( self )));
	}
}

static gboolean
p1_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	g_free( priv->p1_furi );
	priv->p1_furi = gtk_file_chooser_get_uri( priv->p1_chooser );
	g_debug( "p1_check_for_complete: furi=%s", priv->p1_furi );

	ok = my_strlen( priv->p1_furi ) > 0 &&
			my_utils_uri_is_readable_file( priv->p1_furi );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ok );

	return( ok );
}

static void
p1_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	ofaRestoreAssistantPrivate *priv;
	GtkFileFilter *filter;

	priv = self->priv;

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( priv->p1_chooser );

	filter = gtk_file_chooser_get_filter( priv->p1_chooser );
	priv->p1_filter = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( filter ), CHOOSER_FILTER_TYPE ));

	update_settings( self );
}

/*
 * p2: target dossier and database
 */
static void
p2_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p2_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *parent;
	static ofaDossierDispColumn st_columns[] = {
			DOSSIER_DISP_DOSNAME,
			0 };

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = self->priv;

	priv->p2_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_furi && GTK_IS_LABEL( priv->p2_furi ));
	my_utils_widget_set_style( priv->p2_furi, "labelinfo" );

	priv->p2_dossier_treeview = ofa_dossier_treeview_new();
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_dossier_treeview ));
	ofa_dossier_treeview_set_columns( priv->p2_dossier_treeview, st_columns );
	ofa_dossier_treeview_set_show( priv->p2_dossier_treeview, DOSSIER_SHOW_CURRENT );

	g_signal_connect( priv->p2_dossier_treeview, "changed", G_CALLBACK( p2_on_dossier_changed ), self );
	g_signal_connect( priv->p2_dossier_treeview, "activated", G_CALLBACK( p2_on_dossier_activated ), self );

	priv->p2_new_dossier_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-new-dossier" );

	g_signal_connect( priv->p2_new_dossier_btn, "clicked", G_CALLBACK( p2_on_dossier_new ), self );

	priv->p2_is_new_dossier = FALSE;
}

static void
p2_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p2_do_display";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = self->priv;

	gtk_label_set_text( GTK_LABEL( priv->p2_furi ), priv->p1_furi );

	p2_check_for_complete( self );

	gtk_widget_grab_focus( GTK_WIDGET( priv->p2_dossier_treeview ));
}

static void
p2_on_dossier_changed( ofaDossierTreeview *view, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaRestoreAssistant *assistant )
{
	ofaRestoreAssistantPrivate *priv;

	priv = assistant->priv;
	g_clear_object( &priv->p2_meta );
	priv->p2_meta = g_object_ref( meta );
	g_clear_object( &priv->p2_period );
	priv->p2_period = g_object_ref( period );

	p2_check_for_complete( assistant );
}

static void
p2_on_dossier_activated( ofaDossierTreeview *view, ofaIDBMeta *meta, ofaIDBPeriod *period, ofaRestoreAssistant *assistant )
{
	ofaRestoreAssistantPrivate *priv;

	priv = assistant->priv;
	g_clear_object( &priv->p2_meta );
	priv->p2_meta = g_object_ref( meta );
	g_clear_object( &priv->p2_period );
	priv->p2_period = g_object_ref( period );

	if( p2_check_for_complete( assistant )){
		gtk_assistant_next_page( my_assistant_get_assistant( MY_ASSISTANT( assistant )));
	}
}

static void
p2_on_dossier_new( GtkButton *button, ofaRestoreAssistant *assistant )
{
	ofaRestoreAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	gchar *dossier_name;
	GtkWindow *toplevel;
	ofaIDBMeta *meta;

	priv = assistant->priv;
	meta = NULL;

	main_window = my_window_get_main_window( MY_WINDOW( assistant ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	toplevel = my_window_get_toplevel( MY_WINDOW( assistant ));

	if( ofa_dossier_new_mini_run( OFA_MAIN_WINDOW( main_window ), toplevel, &meta )){

		g_clear_object( &priv->p2_meta );
		priv->p2_meta = meta;

		priv->p2_is_new_dossier = TRUE;

		dossier_name = ofa_idbmeta_get_dossier_name( priv->p2_meta );
		ofa_dossier_treeview_set_selected( priv->p2_dossier_treeview, dossier_name );
		g_free( dossier_name );

		gtk_widget_grab_focus( GTK_WIDGET( priv->p2_dossier_treeview ));
	}
}

static gboolean
p2_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = ( priv->p2_meta && OFA_IS_IDBMETA( priv->p2_meta ) &&
			priv->p2_period && OFA_IS_IDBPERIOD( priv->p2_period ));

	if( ok ){
		g_free( priv->p2_dbname );
		priv->p2_dbname = ofa_idbperiod_get_name( priv->p2_period );
	}

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ok );

	return( ok );
}

static void
p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	update_settings( self );
}

/*
 * p3: get DBMS root account and password
 */
static void
p3_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p3_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *label, *parent;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	priv->p3_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_furi && GTK_IS_LABEL( priv->p3_furi ));
	my_utils_widget_set_style( priv->p3_furi, "labelinfo" );

	priv->p3_dossier = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dossier" );
	g_return_if_fail( priv->p3_dossier && GTK_IS_LABEL( priv->p3_dossier ));
	my_utils_widget_set_style( priv->p3_dossier, "labelinfo" );

	priv->p3_hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label311" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p3_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label312" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p3_hgroup, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dra-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p3_dbms_credentials = ofa_dbms_root_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p3_dbms_credentials ));
	my_utils_size_group_add_size_group(
			priv->p3_hgroup, ofa_dbms_root_bin_get_size_group( priv->p3_dbms_credentials, 0 ));

	g_signal_connect(
			priv->p3_dbms_credentials, "ofa-changed", G_CALLBACK( p3_on_dbms_root_changed ), self );

	priv->p3_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" );
	g_return_if_fail( priv->p3_message && GTK_IS_LABEL( priv->p3_message ));
	my_utils_widget_set_style( priv->p3_message, "labelerror" );
}

static void
p3_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p3_do_display";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *parent, *child;
	GList *children;
	ofaIDBProvider *provider;
	ofaIDBEditor *editor;
	gchar *dossier_name;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	gtk_label_set_text( GTK_LABEL( priv->p3_furi ), priv->p1_furi );

	dossier_name = ofa_idbmeta_get_dossier_name( priv->p2_meta );
	gtk_label_set_text( GTK_LABEL( priv->p3_dossier ), dossier_name );
	g_free( dossier_name );

	/* as the dossier may have changed since the initialization,
	 * the ofaIDBEditor used to display connection informations
	 * must be set here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-connect-infos" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	children = gtk_container_get_children( GTK_CONTAINER( parent ));
	if( children ){
		child = ( GtkWidget * ) children->data;
		if( child && GTK_IS_WIDGET( child )){
			gtk_widget_destroy( child );
		}
		g_list_free( children );
	}

	provider = ofa_idbmeta_get_provider( priv->p2_meta );
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));
	editor = ofa_idbprovider_new_editor( provider, FALSE );
	g_object_unref( provider );

	ofa_idbeditor_set_meta( editor, priv->p2_meta, priv->p2_period );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( editor ));
	my_utils_size_group_add_size_group(
			priv->p3_hgroup, ofa_idbeditor_get_size_group( editor, 0 ));

	ofa_dbms_root_bin_set_meta( priv->p3_dbms_credentials, priv->p2_meta );

	/* setup the dbms root credentials */
	if( priv->p3_account ){
		ofa_dbms_root_bin_set_credentials(
				priv->p3_dbms_credentials, priv->p3_account, priv->p3_password );
	}

	gtk_widget_show_all( page );
	p3_check_for_complete( self );
}

static void
p3_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p3_account );
	priv->p3_account = g_strdup( account );

	g_free( priv->p3_password );
	priv->p3_password = g_strdup( password );

	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = self->priv;
	p3_set_message( self, "" );

	ok = ofa_dbms_root_bin_is_valid( priv->p3_dbms_credentials, &message );
	if( !ok ){
		p3_set_message( self, message );
		g_free( message );
	}

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ok );
}

static void
p3_set_message( ofaRestoreAssistant *self, const gchar *message )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	if( priv->p3_message ){
		gtk_label_set_text( GTK_LABEL( priv->p3_message ), message );
	}
}

/*
 * open a new superuser connection at DMBS server-level on page change
 * this could be done as soon as we get the root credentials, but this
 * transition seems more stable (less often run)
 */
static void
p3_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p3_do_forward";
	ofaRestoreAssistantPrivate *priv;
	ofaIDBProvider *provider;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	g_clear_object( &priv->p3_connect );
	provider = ofa_idbmeta_get_provider( priv->p2_meta );
	priv->p3_connect = ofa_idbprovider_new_connect( provider );
	if( !ofa_idbconnect_open_with_meta(
			priv->p3_connect, priv->p3_account, priv->p3_password, priv->p2_meta, NULL )){
		g_clear_object( &priv->p3_connect );
		g_warning( "%s: unable to open a new '%s' connection on DBMS", thisfn, priv->p3_account );
	}
}

/*
 * p4: get dossier administrative account and password
 */
static void
p4_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p4_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *parent, *label;
	GtkSizeGroup *hgroup;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	priv->p4_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( priv->p4_furi && GTK_IS_LABEL( priv->p4_furi ));
	my_utils_widget_set_style( priv->p4_furi, "labelinfo" );

	priv->p4_dossier = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-dossier" );
	g_return_if_fail( priv->p4_dossier && GTK_IS_LABEL( priv->p4_dossier ));
	my_utils_widget_set_style( priv->p4_dossier, "labelinfo" );

	priv->p4_database = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-database" );
	g_return_if_fail( priv->p4_database && GTK_IS_LABEL( priv->p4_database ));
	my_utils_widget_set_style( priv->p4_database, "labelinfo" );

	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label411" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label412" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label413" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-admin-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p4_admin_credentials = ofa_admin_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_admin_credentials ));
	my_utils_size_group_add_size_group(
			hgroup, ofa_admin_credentials_bin_get_size_group( priv->p4_admin_credentials, 0 ));

	g_signal_connect( priv->p4_admin_credentials,
			"ofa-changed", G_CALLBACK( p4_on_admin_credentials_changed ), self );

	priv->p4_open_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-open" );
	g_return_if_fail( priv->p4_open_btn && GTK_IS_CHECK_BUTTON( priv->p4_open_btn ));

	g_signal_connect( priv->p4_open_btn, "toggled", G_CALLBACK( p4_on_open_toggled ), self );

	priv->p4_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-message" );
	g_return_if_fail( priv->p4_message && GTK_IS_LABEL( priv->p4_message ));
	my_utils_widget_set_style( priv->p4_message, "labelerror" );

	g_object_unref( hgroup );
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p4_do_display";
	ofaRestoreAssistantPrivate *priv;
	gchar *dossier_name;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	gtk_label_set_text( GTK_LABEL( priv->p4_furi ), priv->p1_furi );

	dossier_name = ofa_idbmeta_get_dossier_name( priv->p2_meta );
	gtk_label_set_text( GTK_LABEL( priv->p4_dossier ), dossier_name );
	g_free( dossier_name );

	gtk_label_set_text( GTK_LABEL( priv->p4_database ), priv->p2_dbname );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p4_open_btn ), priv->p4_open );
	p4_on_open_toggled( GTK_TOGGLE_BUTTON( priv->p4_open_btn ), self );

	p4_check_for_complete( self );

	/*ofa_admin_credentials_bin_grab_focus( priv->p4_admin_credentials );*/
}

static void
p4_on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p4_account );
	priv->p4_account = g_strdup( account );

	g_free( priv->p4_password );
	priv->p4_password = g_strdup( password );

	p4_check_for_complete( self );
}

static void
p4_on_open_toggled( GtkToggleButton *button, ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	priv->p4_open = gtk_toggle_button_get_active( button );
}

static void
p4_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = self->priv;
	p4_set_message( self, "" );

	ok = ofa_admin_credentials_bin_is_valid( priv->p4_admin_credentials, &message );
	if( !ok ){
		p4_set_message( self, message );
		g_free( message );
	}

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ok );
}

static void
p4_set_message( ofaRestoreAssistant *self, const gchar *message )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	if( priv->p4_message ){
		gtk_label_set_text( GTK_LABEL( priv->p4_message ), message );
	}
}

static void
p4_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p4_do_forward";
	/*ofaRestoreAssistantPrivate *priv;*/

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	update_settings( self );
}

/*
 * confirmation page
 */
static void
p5_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p5_do_init";
	ofaRestoreAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	priv->p5_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-furi" );
	g_return_if_fail( priv->p5_furi && GTK_IS_LABEL( priv->p5_furi ));
	my_utils_widget_set_style( priv->p5_furi, "labelinfo" );

	priv->p5_dossier = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dossier" );
	g_return_if_fail( priv->p5_dossier && GTK_IS_LABEL( priv->p5_dossier ));
	my_utils_widget_set_style( priv->p5_dossier, "labelinfo" );

	priv->p5_database = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-database" );
	g_return_if_fail( priv->p5_database && GTK_IS_LABEL( priv->p5_database ));
	my_utils_widget_set_style( priv->p5_database, "labelinfo" );

	priv->p5_root_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbms-account" );
	g_return_if_fail( priv->p5_root_account && GTK_IS_LABEL( priv->p5_root_account ));
	my_utils_widget_set_style( priv->p5_root_account, "labelinfo" );

	priv->p5_root_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbms-password" );
	g_return_if_fail( priv->p5_root_password && GTK_IS_LABEL( priv->p5_root_password ));
	my_utils_widget_set_style( priv->p5_root_password, "labelinfo" );

	priv->p5_admin_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-adm-account" );
	g_return_if_fail( priv->p5_admin_account && GTK_IS_LABEL( priv->p5_admin_account ));
	my_utils_widget_set_style( priv->p5_admin_account, "labelinfo" );

	priv->p5_admin_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-adm-password" );
	g_return_if_fail( priv->p5_admin_password && GTK_IS_LABEL( priv->p5_admin_password ));
	my_utils_widget_set_style( priv->p5_admin_password, "labelinfo" );

	priv->p5_open = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-open" );
	g_return_if_fail( priv->p5_open && GTK_IS_LABEL( priv->p5_open ));
	my_utils_widget_set_style( priv->p5_open, "labelinfo" );
}

static void
p5_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p5_do_display";
	ofaRestoreAssistantPrivate *priv;
	gchar *dossier_name;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	dossier_name = ofa_idbmeta_get_dossier_name( priv->p2_meta );

	gtk_label_set_text( GTK_LABEL( priv->p5_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p5_dossier ), dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p5_database ), priv->p2_dbname );
	gtk_label_set_text( GTK_LABEL( priv->p5_root_account ), priv->p3_account );
	gtk_label_set_text( GTK_LABEL( priv->p5_root_password ), "******" );
	gtk_label_set_text( GTK_LABEL( priv->p5_admin_account ), priv->p4_account );
	gtk_label_set_text( GTK_LABEL( priv->p5_admin_password ), "******" );
	gtk_label_set_text( GTK_LABEL( priv->p5_open ), priv->p4_open ? _( "True" ): _( "False" ));

	g_free( dossier_name );
}

/*
 * execution
 * execution summary
 */
static void
p6_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p6_do_init";
	ofaRestoreAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->p6_page = page;

	priv->p6_label1 = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label61" );
	g_return_if_fail( priv->p6_label1 && GTK_IS_LABEL( priv->p6_label1 ));

	priv->p6_label2 = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label62" );
	g_return_if_fail( priv->p6_label1 && GTK_IS_LABEL( priv->p6_label1 ));
}

static void
p6_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p6_do_display";
	ofaRestoreAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	/* may be NULL */
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));

	priv = self->priv;

	if( !p6_restore_confirmed( self )){
		if( priv->p2_is_new_dossier ){
			ofa_idbmeta_remove_meta( priv->p2_meta );
		}
		gtk_label_set_text(
				GTK_LABEL( priv->p6_label1 ),
				_( "The restore operation has been cancelled by the user." ));

	} else {
		/* first close the currently opened dossier */
		if( dossier ){
			g_return_if_fail( OFO_IS_DOSSIER( dossier ));
			ofa_main_window_close_dossier( OFA_MAIN_WINDOW( main_window ));
		}

		g_idle_add(( GSourceFunc ) p6_do_restore, self );
	}
}

static gboolean
p6_restore_confirmed( const ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *name, *str;

	priv = self->priv;
	name = ofa_idbperiod_get_name( priv->p2_period );

	str = g_strdup_printf(
				_( "The restore operation will drop, fully reset and repopulate "
					"the '%s' database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to restore into this database ?" ), name );

	ok = my_utils_dialog_question( str, _( "_Restore" ));

	g_free( str );
	g_free( name );

	return( ok );
}

/*
 * restore the dossier
 * simultaneously installing administrative credentials
 */
static gboolean
p6_do_restore( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *dossier_name, *str;
	const gchar *style;

	priv = self->priv;
	dossier_name = ofa_idbmeta_get_dossier_name( priv->p2_meta );

	/* restore the backup */
	ok = ofa_idbconnect_restore(
				priv->p3_connect, NULL, priv->p1_furi, priv->p4_account, priv->p4_password );

	if( ok ){
		style = "labelnormal";
		str = g_strdup_printf(
				_( "The '%s' backup file has been successfully restored "
					"into the '%s' dossier." ), priv->p1_furi, dossier_name );
	} else {
		style = "labelerror";
		str = g_strdup_printf(
				_( "Unable to restore the '%s' backup file.\n"
					"Please fix the errors and retry." ), priv->p1_furi );
	}

	gtk_label_set_text( GTK_LABEL( priv->p6_label1 ), str );
	my_utils_widget_set_style( priv->p6_label1, style );

	g_free( dossier_name );
	g_free( str );

	if( ok ){
		g_idle_add(( GSourceFunc ) p6_do_open, self );
	}

	return( G_SOURCE_REMOVE );
}

/*
 * open the dossier if asked for
 * actually, keep the needed datas so that the dossier may be opened
 * after the assistant has quit
 */
static gboolean
p6_do_open( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	sOpenData *sdata;

	priv = self->priv;

	if( priv->p4_open ){
		sdata = priv->sdata;
		sdata->open = priv->p4_open;
		main_window = my_window_get_main_window( MY_WINDOW( self ));
		g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), G_SOURCE_REMOVE );
		sdata->main_window = OFA_MAIN_WINDOW( main_window );
		sdata->meta = g_object_ref( priv->p2_meta );
		sdata->period = g_object_ref( priv->p2_period );
		sdata->account = g_strdup( priv->p4_account );
		sdata->password = g_strdup( priv->p4_password );
	}

	return( G_SOURCE_REMOVE );
}

/*
 * settings is "folder;open;filter_type;"
 */
static void
get_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = self->priv;

	list = ofa_settings_user_get_string_list( st_prefs_import );

	it = list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->p1_folder );
		priv->p1_folder = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p4_open = my_utils_boolean_from_str( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p1_filter = atoi( cstr );
	}

	ofa_settings_free_string_list( list );
}

static void
update_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	GList *list;

	priv = self->priv;

	list = g_list_append( NULL, g_strdup( priv->p1_folder ));
	list = g_list_append( list, g_strdup_printf( "%s", priv->p4_open ? "True":"False" ));
	list = g_list_append( list, g_strdup_printf( "%d", priv->p1_filter ));

	ofa_settings_user_set_string_list( st_prefs_import, list );

	ofa_settings_free_string_list( list );
}
