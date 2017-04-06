/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
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
#include <stdlib.h>

#include "my/my-iassistant.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"

#include "ui/ofa-backup-assistant.h"

/* Backup Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  FILE     Select a file
 *   2   Content  COMMENT  Enter an optional comment
 *   3   Confirm  CONFIRM  Summary of the operations to be done
 *   4   Summary  DONE     After backup
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_FILE,
	ASSIST_PAGE_COMMENT,
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

	/* runtime
	 */
	gchar               *settings_prefix;
	ofaIDBDossierMeta   *dossier_meta;

	/* p1: select destination file
	 */
	GtkWidget           *p1_chooser;
	gchar               *p1_folder;
	gchar               *p1_uri;

	/* p2: enter an optional comment
	 */
	GtkWidget           *p2_uri;
	GtkWidget           *p2_textview;
	gchar               *p2_comment;

	/* p3: summary
	 */
	GtkWidget           *p3_uri;
	GtkWidget           *p3_comment;

	/* p4: backup estore the file, display the result
	 */
	GtkWidget           *p4_textview;
	GtkWidget           *p4_label;
}
	ofaBackupAssistantPrivate;

/* GtkFileChooser filters
 */
enum {
	FILE_CHOOSER_ALL = 1,
	FILE_CHOOSER_GZ,
	FILE_CHOOSER_ZIP,
};

typedef struct {
	gint         type;
	const gchar *pattern;
	const gchar *name;
	gboolean     def_selected;
}
	sFilter;

static sFilter st_filters[] = {
		{ FILE_CHOOSER_ALL, "*",     N_( "All files (*)" ),       FALSE },
		{ FILE_CHOOSER_GZ,  "*.gz",  N_( "Backup files (*.gz)" ), FALSE },
		{ FILE_CHOOSER_ZIP, "*.zip", N_( "ZIP files (*.zip)" ),   TRUE },
		{ 0 }
};

static const gchar *st_backup_folder    = "ofa-LastBackupFolder";
static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-backup-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p1_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static void     p1_set_filters( ofaBackupAssistant *self );
static gchar   *p1_get_default_name( ofaBackupAssistant *self );
static void     p1_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaBackupAssistant *self );
static void     p1_on_file_activated( GtkFileChooser *chooser, ofaBackupAssistant *self );
static gboolean p1_check_for_complete( ofaBackupAssistant *self );
static gboolean p1_confirm_overwrite( const ofaBackupAssistant *self, const gchar *fname );
static void     p1_do_forward( ofaBackupAssistant *self, GtkWidget *page );
static void     p2_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static gboolean p2_check_for_complete( ofaBackupAssistant *self );
static void     p2_do_forward( ofaBackupAssistant *self, GtkWidget *page );
static void     p3_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page );
static gboolean p4_do_backup( ofaBackupAssistant *self );
static void     p4_msg_cb( const gchar *buffer, ofaBackupAssistant *self );
static void     read_settings( ofaBackupAssistant *self );
static void     write_settings( ofaBackupAssistant *self );

G_DEFINE_TYPE_EXTENDED( ofaBackupAssistant, ofa_backup_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaBackupAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IASSISTANT, iassistant_iface_init ))

static const ofsIAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_FILE,
				( myIAssistantCb ) p1_do_init,
				( myIAssistantCb ) p1_do_display,
				( myIAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_COMMENT,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_do_display,
				NULL },
		{ -1 }
};

static void
backup_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_backup_assistant_finalize";
	ofaBackupAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BACKUP_ASSISTANT( instance ));

	/* free data members here */
	priv = ofa_backup_assistant_get_instance_private( OFA_BACKUP_ASSISTANT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->p1_folder );
	g_free( priv->p1_uri );
	g_free( priv->p2_comment );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_assistant_parent_class )->finalize( instance );
}

static void
backup_assistant_dispose( GObject *instance )
{
	ofaBackupAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BACKUP_ASSISTANT( instance ));

	priv = ofa_backup_assistant_get_instance_private( OFA_BACKUP_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_BACKUP_ASSISTANT( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_assistant_parent_class )->dispose( instance );
}

static void
ofa_backup_assistant_init( ofaBackupAssistant *self )
{
	static const gchar *thisfn = "ofa_backup_assistant_init";
	ofaBackupAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BACKUP_ASSISTANT( self ));

	priv = ofa_backup_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_backup_assistant_class_init( ofaBackupAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_backup_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = backup_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = backup_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_backup_assistant_run:
 * @getter: a #ofaIGetter instance.
 *
 * Run the assistant.
 */
void
ofa_backup_assistant_run( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_backup_assistant_run";
	ofaBackupAssistant *self;
	ofaBackupAssistantPrivate *priv;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_BACKUP_ASSISTANT, NULL );

	priv = ofa_backup_assistant_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_backup_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_backup_assistant_iwindow_init";
	ofaBackupAssistantPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_backup_assistant_get_instance_private( OFA_BACKUP_ASSISTANT( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	priv->dossier_meta = ofa_idbconnect_get_dossier_meta( connect );

	my_iwindow_set_parent( instance, GTK_WINDOW( ofa_igetter_get_main_window( priv->getter )));
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	read_settings( OFA_BACKUP_ASSISTANT( instance ));
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_backup_assistant_iassistant_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_willing_to_quit = iassistant_is_willing_to_quit;
}

static gboolean
iassistant_is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	ofaBackupAssistantPrivate *priv;

	priv = ofa_backup_assistant_get_instance_private( OFA_BACKUP_ASSISTANT( instance ));

	return( ofa_prefs_assistant_is_willing_to_quit( priv->getter, keyval ));
}

/*
 * initialize the GtkFileChooser widget with the last used folder
 * we are in save mode
 */
static void
p1_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p1_do_init";
	ofaBackupAssistantPrivate *priv;
	GtkWidget *widget;
	gchar *def_name;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p1_chooser = widget;

	p1_set_filters( self );

	def_name = p1_get_default_name( self );
	gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->p1_chooser ), def_name );
	g_free( def_name );

	g_signal_connect( widget, "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	g_signal_connect( widget, "file-activated", G_CALLBACK( p1_on_file_activated ), self );
}

static void
p1_set_filters( ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;
	gint i;
	GtkFileFilter *filter, *selected;

	priv = ofa_backup_assistant_get_instance_private( self );

	selected = NULL;

	for( i=0 ; st_filters[i].type ; ++i ){
		filter = gtk_file_filter_new();
		gtk_file_filter_set_name( filter, gettext( st_filters[i].name ));
		gtk_file_filter_add_pattern( filter, st_filters[i].pattern );
		gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( priv->p1_chooser ), filter );

		/* Starting with v0.65, the default selected filter is forced
		 * to .zip files */
		if( st_filters[i].def_selected ){
			selected = filter;
		}
	}

	if( selected ){
		gtk_file_chooser_set_filter( GTK_FILE_CHOOSER( priv->p1_chooser ), selected );
	}
}

static gchar *
p1_get_default_name( ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;
	ofaIDBExerciceMeta *exercice_meta;
	GRegex *regex;
	gchar *name, *fname, *sdate, *result;
	GDate date;

	priv = ofa_backup_assistant_get_instance_private( self );

	/* get name without spaces */
	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );
	exercice_meta = ofa_idbconnect_get_exercice_meta( connect );
	name = ofa_idbexercice_meta_get_name( exercice_meta );

	regex = g_regex_new( " ", 0, 0, NULL );
	fname = g_regex_replace_literal( regex, name, -1, 0, "", 0, NULL );

	g_free( name );

	my_date_set_now( &date );
	sdate = my_date_to_str( &date, MY_DATE_YYMD );
	result = g_strdup_printf( "%s-%s.zip", fname, sdate );
	g_free( sdate );

	g_free( fname );

	return( result );
}

static void
p1_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p1_do_display";
	ofaBackupAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	if( priv->p1_folder ){
		gtk_file_chooser_set_current_folder_uri( GTK_FILE_CHOOSER( priv->p1_chooser ), priv->p1_folder );
	}

	p1_check_for_complete( self );
}

static void
p1_on_selection_changed( GtkFileChooser *chooser, ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;

	priv = ofa_backup_assistant_get_instance_private( self );

	g_free( priv->p1_uri );
	priv->p1_uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->p1_chooser ));

	p1_check_for_complete( self );
}

static void
p1_on_file_activated( GtkFileChooser *chooser, ofaBackupAssistant *self )
{
	p1_on_selection_changed( chooser, self );

	if( p1_check_for_complete( self )){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p1_check_for_complete( ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_backup_assistant_get_instance_private( self );

	ok = my_strlen( priv->p1_uri ) > 0;

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
p1_confirm_overwrite( const ofaBackupAssistant *self, const gchar *fname )
{
	gboolean ok;
	gchar *str;
	GtkWindow *toplevel;

	str = g_strdup_printf(
				_( "The file '%s' already exists.\n"
					"Are you sure you want to overwrite it ?" ), fname );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "_Overwrite" ));

	g_free( str );

	return( ok );
}

static void
p1_do_forward( ofaBackupAssistant *self, GtkWidget *page )
{
	ofaBackupAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_backup_assistant_get_instance_private( self );

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( GTK_FILE_CHOOSER( priv->p1_chooser ));

	/* We cannot prevent this test to be made only here.
	 * If the user cancel, then the assistant will anyway go to the
	 * Confirmation page, without any dest uri
	 * This is because GtkAssistant does not let us stay on the same page
	 * when the user has clicked on the Next button */
	if( my_utils_uri_exists( priv->p1_uri )){
		ok = p1_confirm_overwrite( self, priv->p1_uri );
		g_debug( "p1_do_forward: ok=%s", ok ? "True":"False" );
		if( !ok ){
			g_free( priv->p1_uri );
			priv->p1_uri = NULL;
		}
	}
}

/*
 * p2: comment
 */
static void
p2_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p2_do_init";
	ofaBackupAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	priv->p2_uri = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-uri" );
	g_return_if_fail( priv->p2_uri && GTK_IS_LABEL( priv->p2_uri ));

	priv->p2_textview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-textview" );
	g_return_if_fail( priv->p2_textview && GTK_IS_TEXT_VIEW( priv->p2_textview ));
}

static void
p2_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p2_do_display";
	ofaBackupAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	if( p1_check_for_complete( self )){
		my_style_add( priv->p2_uri, "labelinfo" );
		my_style_remove( priv->p2_uri, "labelerror" );
		gtk_label_set_text( GTK_LABEL( priv->p2_uri ), priv->p1_uri );

	} else {
		my_style_remove( priv->p2_uri, "labelinfo" );
		my_style_add( priv->p2_uri, "labelerror" );
		gtk_label_set_text( GTK_LABEL( priv->p2_uri ),
				_( "Target is not set. Please hit 'Back' button to select a target."));
	}

	p2_check_for_complete( self );
}

static gboolean
p2_check_for_complete( ofaBackupAssistant *self )
{
	gboolean ok;

	ok = p1_check_for_complete( self );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p2_do_forward( ofaBackupAssistant *self, GtkWidget *page )
{
	ofaBackupAssistantPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	priv = ofa_backup_assistant_get_instance_private( self );

	buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p2_textview ));
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );

	g_free( priv->p2_comment );
	priv->p2_comment = gtk_text_buffer_get_text( buffer, &start, &end, FALSE );
}

/*
 * confirmation page
 */
static void
p3_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p3_do_init";
	ofaBackupAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_BACKUP_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	priv->p3_uri = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-uri" );
	g_return_if_fail( priv->p3_uri && GTK_IS_LABEL( priv->p3_uri ));
	my_style_add( priv->p3_uri, "labelinfo" );

	priv->p3_comment = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-comment" );
	g_return_if_fail( priv->p3_comment && GTK_IS_LABEL( priv->p3_comment ));
	my_style_add( priv->p3_comment, "labelinfo" );
}

static void
p3_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p3_do_display";
	ofaBackupAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_BACKUP_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_uri ), priv->p1_uri );
	gtk_label_set_text( GTK_LABEL( priv->p3_comment ), priv->p2_comment );
}

/*
 * execution
 * execution summary
 */
static void
p4_do_init( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p4_do_init";
	ofaBackupAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_BACKUP_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_backup_assistant_get_instance_private( self );

	priv->p4_textview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-textview" );
	g_return_if_fail( priv->p4_textview && GTK_IS_TEXT_VIEW( priv->p4_textview ));

	priv->p4_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label" );
	g_return_if_fail( priv->p4_label && GTK_IS_LABEL( priv->p4_label ));
}

static void
p4_do_display( ofaBackupAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_backup_assistant_p4_do_display";

	g_return_if_fail( OFA_IS_BACKUP_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	g_idle_add(( GSourceFunc ) p4_do_backup, self );
}

/*
 * restore the dossier
 * simultaneously installing administrative credentials
 */
static gboolean
p4_do_backup( ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;
	ofaHub *hub;
	ofaIDBConnect *connect;
	gboolean ok;
	gchar *msg;
	GtkWidget *dlg;
	const gchar *style, *dossier_name;

	priv = ofa_backup_assistant_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	connect = ofa_hub_get_connect( hub );

	ok = ofa_idbconnect_backup_db( connect, priv->p2_comment, priv->p1_uri, ( ofaMsgCb ) p4_msg_cb, self );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->dossier_meta );

	if( ok ){
		style = "labelinfo";
		msg = g_strdup_printf( _( "Dossier '%s' has been successfully archived into '%s' URI" ),
				dossier_name, priv->p1_uri );
	} else {
		style = "labelerror";
		msg = g_strdup_printf( _( "An error occured while archiving the '%s' dossier" ),
				dossier_name );
	}

	dlg = gtk_message_dialog_new(
				GTK_WINDOW( self ),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				"%s", msg );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );

	gtk_label_set_text( GTK_LABEL( priv->p4_label ), msg );
	my_style_add( priv->p4_label, style );

	g_free( msg );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	return( G_SOURCE_REMOVE );
}

static void
p4_msg_cb( const gchar *buffer, ofaBackupAssistant *self )
{
	static const gchar *thisfn = "ofa_backup_assistant_p4_msg_cb";
	ofaBackupAssistantPrivate *priv;
	GtkTextBuffer *textbuf;
	GtkTextIter enditer;
	const gchar *charset;
	gchar *utf8;

	if( 0 ){
		g_debug( "%s: buffer=%p, self=%p", thisfn, ( void * ) buffer, ( void * ) self );
	}

	priv = ofa_backup_assistant_get_instance_private( self );

	textbuf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p4_textview ));
	gtk_text_buffer_get_end_iter( textbuf, &enditer );

	/* Check if messages are in UTF-8. If not, assume
	 *  they are in current locale and try to convert.
	 *  We assume we're getting the stream in a 1-byte
	 *   encoding here, ie. that we do not have cut-off
	 *   characters at the end of our buffer (=BAD)
	 */
	if( g_utf8_validate( buffer, -1, NULL )){
		gtk_text_buffer_insert( textbuf, &enditer, buffer, -1 );

	} else {
		g_get_charset( &charset );
		utf8 = g_convert_with_fallback( buffer, -1, "UTF-8", charset, NULL, NULL, NULL, NULL );
		if( utf8 ){
			gtk_text_buffer_insert( textbuf, &enditer, utf8, -1 );
			g_free(utf8);

		} else {
			g_warning( "%s: message output is not in UTF-8 nor in locale charset", thisfn );
		}
	}

	/* A bit awkward, but better than nothing. Scroll text view to end */
	gtk_text_buffer_get_end_iter( textbuf, &enditer );
	gtk_text_buffer_move_mark_by_name( textbuf, "insert", &enditer );
	gtk_text_view_scroll_to_mark(
			GTK_TEXT_VIEW( priv->p4_textview),
			gtk_text_buffer_get_mark( textbuf, "insert" ),
			0.0, FALSE, 0.0, 0.0 );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

/*
 * dossier settings records last backup folder
 */
static void
read_settings( ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;
	myISettings *settings;
	const gchar *cgroup;

	priv = ofa_backup_assistant_get_instance_private( self );

	settings = ofa_igetter_get_dossier_settings( priv->getter );
	cgroup = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );
	g_free( priv->p1_folder );
	priv->p1_folder = my_isettings_get_string( settings, cgroup, st_backup_folder );
}

static void
write_settings( ofaBackupAssistant *self )
{
	ofaBackupAssistantPrivate *priv;
	myISettings *settings;
	const gchar *cgroup;

	priv = ofa_backup_assistant_get_instance_private( self );

	settings = ofa_igetter_get_dossier_settings( priv->getter );
	cgroup = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );

	my_isettings_set_string( settings, cgroup, st_backup_folder, priv->p1_folder );
}
