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
#include "my/my-isizegroup.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-backup-header.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-admin-credentials-bin.h"
#include "ui/ofa-dossier-actions-bin.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-restore-assistant.h"
#include "ui/ofa-target-chooser-bin.h"

/* Restore Assistant
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

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;
	GtkWindow              *parent;

	/* runtime
	 */
	gchar                  *settings_prefix;
	ofaHub                 *hub;

	/* p1: select file to be restored
	 */
	GtkFileChooser         *p1_chooser;
	gchar                  *p1_folder;
	gchar                  *p1_uri;		/* the utf-8 to be restored file uri */
	gint                    p1_filter;
	guint                   p1_format;

	/* p2: select the dossier target
	 */
	GtkWidget              *p2_uri_label;
	ofaTargetChooserBin    *p2_chooser;
	ofaIDBDossierMeta      *p2_dossier_meta;
	ofaIDBExerciceMeta     *p2_exercice_meta;
	ofaIDBProvider         *p2_provider;
	ofaIDBConnect          *p2_connect;
	gchar                  *p2_dossier_name;
	gchar                  *p2_exercice_name;

	/* p3: super-user credentials
	 */
	GtkSizeGroup           *p3_hgroup;
	GtkWidget              *p3_uri_label;
	GtkWidget              *p3_dossier_label;
	GtkWidget              *p3_name_label;
	GtkWidget              *p3_connect_parent;
	GtkWidget              *p3_dbsu_parent;
	ofaIDBSuperuser        *p3_dbsu_credentials;
	GtkWidget              *p3_message;

	/* p4: dossier administrative credentials
	 */
	GtkSizeGroup           *p4_hgroup;
	GtkWidget              *p4_uri_label;
	GtkWidget              *p4_dossier_label;
	GtkWidget              *p4_name_label;
	GtkWidget              *p4_connect_parent;
	ofaAdminCredentialsBin *p4_admin_credentials;
	ofaDossierActionsBin   *p4_actions;
	gchar                  *p4_account;
	gchar                  *p4_password;
	GtkWidget              *p4_message;

	/* p5: display operations to be done and ask for confirmation
	 */
	GtkWidget              *p5_uri_label;
	GtkWidget              *p5_dossier_label;
	GtkWidget              *p5_name_label;
	GtkWidget              *p5_su_account;
	GtkWidget              *p5_su_password;
	GtkWidget              *p5_admin_account;
	GtkWidget              *p5_admin_password;
	GtkWidget              *p5_open_label;
	gboolean                p5_open;
	GtkWidget              *p5_apply_label;
	gboolean                p5_apply;

	/* p6: restore the file, display the result
	 */
	GtkWidget              *p6_page;
	GtkWidget              *p6_textview;
	GtkWidget              *p6_label;
	gboolean                is_destroy_allowed;
}
	ofaRestoreAssistantPrivate;

/* GtkFileChooser filters
 */
enum {
	FILE_CHOOSER_ALL = 1,
	FILE_CHOOSER_GZ,
	FILE_CHOOSER_ZIP
};

typedef struct {
	gint         type;
	const gchar *pattern;
	const gchar *name;
}
	sFilter;

static sFilter st_filters[] = {
		{ FILE_CHOOSER_ALL, "*",     N_( "All files (*)" )},
		{ FILE_CHOOSER_GZ,  "*.gz",  N_( "First archive format (*.gz)" )},
		{ FILE_CHOOSER_ZIP, "*.zip", N_( "Most recent archive format (*.zip)" )},
		{ 0 }
};

#define CHOOSER_FILTER_TYPE                "file-chooser-filter-type"

static const gchar *st_resource_ui       = "/org/trychlos/openbook/ui/ofa-restore-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static gboolean iwindow_is_destroy_allowed( const myIWindow *instance );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p1_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p1_set_filters( ofaRestoreAssistant *self, GtkFileChooser *chooser );
static void     p1_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static void     p1_on_file_activated( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static gboolean p1_check_for_complete( ofaRestoreAssistant *self );
static guint    p1_get_archive_format( ofaRestoreAssistant *self );
static void     p1_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void     p2_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static gboolean p2_check_for_complete( ofaRestoreAssistant *self );
static void     p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void     p3_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p3_on_dbsu_credentials_changed( ofaIDBSuperuser *bin, ofaRestoreAssistant *self );
static void     p3_check_for_complete( ofaRestoreAssistant *self );
static void     p3_set_message( ofaRestoreAssistant *self, const gchar *message );
static void     p4_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p4_on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self );
static void     p4_on_actions_changed( ofaDossierActionsBin *bin, ofaRestoreAssistant *self );
static void     p4_check_for_complete( ofaRestoreAssistant *self );
static void     p4_set_message( ofaRestoreAssistant *self, const gchar *message );
static void     p4_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static gboolean p6_restore_confirmed( ofaRestoreAssistant *self );
static gboolean p6_do_restore( ofaRestoreAssistant *self );
static void     p6_msg_cb( const gchar *buffer, ofaRestoreAssistant *self );
static gboolean p6_do_open( ofaRestoreAssistant *self );
static void     read_settings( ofaRestoreAssistant *self );
static void     write_settings( ofaRestoreAssistant *self );
static void     iactionable_iface_init( ofaIActionableInterface *iface );
static guint    iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaRestoreAssistant, ofa_restore_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaRestoreAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IASSISTANT, iassistant_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ))

static const ofsIAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_SELECT,
				( myIAssistantCb ) p1_do_init,
				( myIAssistantCb ) p1_do_display,
				( myIAssistantCb ) p1_do_forward },
		{ ASSIST_PAGE_TARGET,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_ROOT,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				NULL },
		{ ASSIST_PAGE_ADMIN,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_do_display,
				( myIAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myIAssistantCb ) p5_do_init,
				( myIAssistantCb ) p5_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				( myIAssistantCb ) p6_do_init,
				( myIAssistantCb ) p6_do_display,
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
	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->p1_folder );
	g_free( priv->p1_uri );
	g_free( priv->p2_dossier_name );
	g_free( priv->p2_exercice_name );
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

	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;
		write_settings( OFA_RESTORE_ASSISTANT( instance ));

		/* unref object members here */
		g_clear_object( &priv->p2_dossier_meta );
		g_clear_object( &priv->p2_exercice_meta );
		g_clear_object( &priv->p3_hgroup );
		g_clear_object( &priv->p2_connect );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_restore_assistant_parent_class )->dispose( instance );
}

static void
ofa_restore_assistant_init( ofaRestoreAssistant *self )
{
	static const gchar *thisfn = "ofa_restore_assistant_init";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RESTORE_ASSISTANT( self ));

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->is_destroy_allowed = TRUE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_restore_assistant_class_init( ofaRestoreAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_restore_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = restore_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = restore_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_restore_assistant_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run the assistant.
 */
void
ofa_restore_assistant_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_restore_assistant_run";
	ofaRestoreAssistant *self;
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_RESTORE_ASSISTANT, NULL );

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->getter = ofa_igetter_get_permanent_getter( getter );
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
	static const gchar *thisfn = "ofa_restore_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
	iface->is_destroy_allowed = iwindow_is_destroy_allowed;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_restore_assistant_iwindow_init";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	my_iwindow_set_geometry_settings( instance, ofa_hub_get_user_settings( priv->hub ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	read_settings( OFA_RESTORE_ASSISTANT( instance ));
}

static gboolean
iwindow_is_destroy_allowed( const myIWindow *instance )
{
	static const gchar *thisfn = "ofa_restore_assistant_iwindow_is_destroy_allowed";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	return( priv->is_destroy_allowed );
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_restore_assistant_iassistant_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_willing_to_quit = iassistant_is_willing_to_quit;
}

static gboolean
iassistant_is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	return( ofa_prefs_assistant_is_willing_to_quit( priv->hub, keyval ));
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

	priv = ofa_restore_assistant_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p1_chooser = GTK_FILE_CHOOSER( widget );

	p1_set_filters( self, priv->p1_chooser );

	g_signal_connect( widget, "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	g_signal_connect( widget, "file-activated", G_CALLBACK( p1_on_file_activated ), self );
}

static void
p1_set_filters( ofaRestoreAssistant *self, GtkFileChooser *chooser )
{
	ofaRestoreAssistantPrivate *priv;
	gint i;
	GtkFileFilter *filter, *selected;

	priv = ofa_restore_assistant_get_instance_private( self );

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

	priv = ofa_restore_assistant_get_instance_private( self );

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
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p1_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_restore_assistant_get_instance_private( self );

	g_free( priv->p1_uri );
	priv->p1_uri = gtk_file_chooser_get_uri( priv->p1_chooser );
	g_debug( "p1_check_for_complete: uri=%s", priv->p1_uri );

	ok = my_strlen( priv->p1_uri ) > 0 &&
			my_utils_uri_is_readable( priv->p1_uri );

	if( ok ){
		priv->p1_format = p1_get_archive_format( self );
		ok = ( priv->p1_format > 0 );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

/*
 * Detect the format (.gz vs .zip) of the selected uri.
 * Relies on the file extension.
 */
static guint
p1_get_archive_format( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	guint format;
	gchar *extension;

	priv = ofa_restore_assistant_get_instance_private( self );

	format = 0;
	extension = my_utils_uri_get_extension( priv->p1_uri, TRUE );
	//g_debug( "p1_get_archive_format: extension=%s", extension );

	if( !my_collate( extension, ".gz" )){
		format = OFA_BACKUP_HEADER_GZ;
	} else if( !my_collate( extension, ".zip" )){
		format = OFA_BACKUP_HEADER_ZIP;
	}

	g_free( extension );

	return( format );
}

static void
p1_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	ofaRestoreAssistantPrivate *priv;
	GtkFileFilter *filter;

	priv = ofa_restore_assistant_get_instance_private( self );

	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( priv->p1_chooser );

	filter = gtk_file_chooser_get_filter( priv->p1_chooser );
	priv->p1_filter = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( filter ), CHOOSER_FILTER_TYPE ));
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

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p2_uri_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_uri_label && GTK_IS_LABEL( priv->p2_uri_label ));
	my_style_add( priv->p2_uri_label, "labelinfo" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-chooser-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p2_chooser = ofa_target_chooser_bin_new( priv->hub, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_chooser ));
}

static void
p2_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p2_do_display";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p2_uri_label ), priv->p1_uri );

	p2_check_for_complete( self );

	gtk_widget_show_all( page );
}

static gboolean
p2_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_restore_assistant_get_instance_private( self );

	ok = ( priv->p2_dossier_meta && OFA_IS_IDBDOSSIER_META( priv->p2_dossier_meta ) &&
			priv->p2_exercice_meta && OFA_IS_IDBEXERCICE_META( priv->p2_exercice_meta ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	ofaRestoreAssistantPrivate *priv;
	const gchar *dossier_name;

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p2_provider = ofa_idbdossier_meta_get_provider( priv->p2_dossier_meta );

	g_clear_object( &priv->p2_connect );
	priv->p2_connect = ofa_idbdossier_meta_new_connect( priv->p2_dossier_meta, NULL );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p2_dossier_meta );
	priv->p2_dossier_name = g_strdup( dossier_name );

	priv->p2_exercice_name = ofa_idbexercice_meta_get_name( priv->p2_exercice_meta );
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

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p3_uri_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_uri_label && GTK_IS_LABEL( priv->p3_uri_label ));
	my_style_add( priv->p3_uri_label, "labelinfo" );

	priv->p3_dossier_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dossier" );
	g_return_if_fail( priv->p3_dossier_label && GTK_IS_LABEL( priv->p3_dossier_label ));
	my_style_add( priv->p3_dossier_label, "labelinfo" );

	priv->p3_name_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dbname" );
	g_return_if_fail( priv->p3_name_label && GTK_IS_LABEL( priv->p3_name_label ));
	my_style_add( priv->p3_name_label, "labelinfo" );

	priv->p3_hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label311" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p3_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-label312" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p3_hgroup, label );

	/* connection informations
	 * the actual UI depends of the selected target => just get the
	 * parent here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-connect-infos" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p3_connect_parent = parent;

	/* super user interface
	 * the actual UI depends of the provider, which itself depends of
	 * selected dossier meta => just get the parent here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dbsu-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p3_dbsu_parent = parent;
	priv->p3_dbsu_credentials = NULL;

	/* message */
	priv->p3_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-message" );
	g_return_if_fail( priv->p3_message && GTK_IS_LABEL( priv->p3_message ));
	my_style_add( priv->p3_message, "labelerror" );
}

static void
p3_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p3_do_display";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *label, *display;
	GtkSizeGroup *group;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_uri_label ), priv->p1_uri );
	gtk_label_set_text( GTK_LABEL( priv->p3_dossier_label ), priv->p2_dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p3_name_label ), priv->p2_exercice_name );

	/* as the dossier may have changed since the initialization,
	 * the display of connection informations is setup here */
	gtk_container_foreach( GTK_CONTAINER( priv->p3_connect_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
	display = ofa_idbconnect_get_display( priv->p2_connect, "labelinfo" );
	if( display ){
		gtk_container_add( GTK_CONTAINER( priv->p3_connect_parent ), display );
		if(( group = my_isizegroup_get_size_group( MY_ISIZEGROUP( display ), 0 ))){
			my_utils_size_group_add_size_group( priv->p3_hgroup, group );
		}
	}

	/* setup superuser UI */
	gtk_container_foreach( GTK_CONTAINER( priv->p3_dbsu_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
	priv->p3_dbsu_credentials = ofa_idbprovider_new_superuser_bin( priv->p2_provider, HUB_RULE_DOSSIER_RESTORE );

	if( priv->p3_dbsu_credentials ){
		gtk_container_add( GTK_CONTAINER( priv->p3_dbsu_parent ), GTK_WIDGET( priv->p3_dbsu_credentials ));
		ofa_idbsuperuser_set_dossier_meta( priv->p3_dbsu_credentials, priv->p2_dossier_meta );
		group = ofa_idbsuperuser_get_size_group( priv->p3_dbsu_credentials, 0 );
		if( group ){
			my_utils_size_group_add_size_group( priv->p3_hgroup, group );
		}
		g_signal_connect( priv->p3_dbsu_credentials, "ofa-changed", G_CALLBACK( p3_on_dbsu_credentials_changed ), self );

		/* if SU account is already set */
		ofa_idbsuperuser_set_credentials_from_connect( priv->p3_dbsu_credentials, priv->p2_connect );

	} else {
		label = gtk_label_new( _(
				"The selected DBMS provider does not need super-user credentials for restore operations.\n"
				"Just press Next to continue." ));
		gtk_label_set_xalign( GTK_LABEL( label ), 0 );
		gtk_label_set_line_wrap( GTK_LABEL( label ), TRUE );
		gtk_label_set_line_wrap_mode( GTK_LABEL( label ), PANGO_WRAP_WORD );
		gtk_container_add( GTK_CONTAINER( priv->p3_dbsu_parent ), label );
	}

	/* already triggered by #ofa_idbsuperuser_set_credentials_from_connect()
	 * via p3_on_dbsu_credentials_changed() */
	p3_check_for_complete( self );
}

static void
p3_on_dbsu_credentials_changed( ofaIDBSuperuser *bin, ofaRestoreAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_restore_assistant_get_instance_private( self );
	g_debug( "p3_check_for_complete: p2_dossier_meta=%p", priv->p2_dossier_meta );

	ok = TRUE;
	message = NULL;

	if( ok && priv->p3_dbsu_credentials ){
		ok = ofa_idbsuperuser_is_valid( priv->p3_dbsu_credentials, &message );
	}

	if( ok && priv->p3_dbsu_credentials ){
		ok = ofa_idbconnect_open_with_superuser( priv->p2_connect, priv->p3_dbsu_credentials );
		if( !ok ){
			message = g_strdup_printf( _( "Unable to open a super-user connection on the DBMS" ));
		}
	}

	ofa_idbsuperuser_set_valid( priv->p3_dbsu_credentials, ok );

	p3_set_message( self, message );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p3_set_message( ofaRestoreAssistant *self, const gchar *message )
{
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( self );

	if( priv->p3_message ){
		gtk_label_set_text( GTK_LABEL( priv->p3_message ), message ? message : "" );
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
	GtkSizeGroup *group;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p4_uri_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( priv->p4_uri_label && GTK_IS_LABEL( priv->p4_uri_label ));
	my_style_add( priv->p4_uri_label, "labelinfo" );

	priv->p4_dossier_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-dossier" );
	g_return_if_fail( priv->p4_dossier_label && GTK_IS_LABEL( priv->p4_dossier_label ));
	my_style_add( priv->p4_dossier_label, "labelinfo" );

	priv->p4_name_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-dbname" );
	g_return_if_fail( priv->p4_name_label && GTK_IS_LABEL( priv->p4_name_label ));
	my_style_add( priv->p4_name_label, "labelinfo" );

	priv->p4_hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label411" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p4_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label412" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p4_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-label413" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p4_hgroup, label );

	/* connection informations
	 * the actual UI depends of the selected target => just get the
	 * parent here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-connect-infos" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p4_connect_parent = parent;

	/* admin credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-admin-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p4_admin_credentials = ofa_admin_credentials_bin_new( priv->hub, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_admin_credentials ));
	group = ofa_admin_credentials_bin_get_size_group( priv->p4_admin_credentials, 0 );
	if( group ){
		my_utils_size_group_add_size_group( priv->p4_hgroup, group );
	}

	g_signal_connect( priv->p4_admin_credentials,
			"ofa-changed", G_CALLBACK( p4_on_admin_credentials_changed ), self );

	/* open, and action on open */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-actions" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p4_actions = ofa_dossier_actions_bin_new( priv->hub, priv->settings_prefix, HUB_RULE_DOSSIER_RESTORE );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_actions ));
	g_signal_connect( priv->p4_actions, "ofa-changed", G_CALLBACK( p4_on_actions_changed ), self );

	priv->p4_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-message" );
	g_return_if_fail( priv->p4_message && GTK_IS_LABEL( priv->p4_message ));
	my_style_add( priv->p4_message, "labelerror" );
}

/*
 * ask the user to confirm the operation
 */
static void
p4_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p4_do_display";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *display;
	GtkSizeGroup *group;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p4_uri_label ), priv->p1_uri );
	gtk_label_set_text( GTK_LABEL( priv->p4_dossier_label ), priv->p2_dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p4_name_label ), priv->p2_exercice_name );

	/* connection informations */
	gtk_container_foreach( GTK_CONTAINER( priv->p4_connect_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
	display = ofa_idbconnect_get_display( priv->p2_connect, "labelinfo" );
	if( display ){
		gtk_container_add( GTK_CONTAINER( priv->p4_connect_parent ), display );
		if(( group = my_isizegroup_get_size_group( MY_ISIZEGROUP( display ), 0 ))){
			my_utils_size_group_add_size_group( priv->p4_hgroup, group );
		}
	}

	p4_check_for_complete( self );
}

static void
p4_on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( self );

	g_free( priv->p4_account );
	priv->p4_account = g_strdup( account );

	g_free( priv->p4_password );
	priv->p4_password = g_strdup( password );

	p4_check_for_complete( self );
}

static void
p4_on_actions_changed( ofaDossierActionsBin *bin, ofaRestoreAssistant *self )
{
	/* nothing to do here */
}

static void
p4_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_restore_assistant_get_instance_private( self );

	p4_set_message( self, "" );

	ok = ofa_admin_credentials_bin_is_valid( priv->p4_admin_credentials, &message ) &&
			ofa_dossier_actions_bin_is_valid( priv->p4_actions, &message );

	if( !ok ){
		p4_set_message( self, message );
		g_free( message );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p4_set_message( ofaRestoreAssistant *self, const gchar *message )
{
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( self );

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

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p5_uri_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-furi" );
	g_return_if_fail( priv->p5_uri_label && GTK_IS_LABEL( priv->p5_uri_label ));
	my_style_add( priv->p5_uri_label, "labelinfo" );

	priv->p5_dossier_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dossier" );
	g_return_if_fail( priv->p5_dossier_label && GTK_IS_LABEL( priv->p5_dossier_label ));
	my_style_add( priv->p5_dossier_label, "labelinfo" );

	priv->p5_name_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbname" );
	g_return_if_fail( priv->p5_name_label && GTK_IS_LABEL( priv->p5_name_label ));
	my_style_add( priv->p5_name_label, "labelinfo" );

	priv->p5_su_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-su-account" );
	g_return_if_fail( priv->p5_su_account && GTK_IS_LABEL( priv->p5_su_account ));
	my_style_add( priv->p5_su_account, "labelinfo" );

	priv->p5_su_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-su-password" );
	g_return_if_fail( priv->p5_su_password && GTK_IS_LABEL( priv->p5_su_password ));
	my_style_add( priv->p5_su_password, "labelinfo" );

	priv->p5_admin_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-adm-account" );
	g_return_if_fail( priv->p5_admin_account && GTK_IS_LABEL( priv->p5_admin_account ));
	my_style_add( priv->p5_admin_account, "labelinfo" );

	priv->p5_admin_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-adm-password" );
	g_return_if_fail( priv->p5_admin_password && GTK_IS_LABEL( priv->p5_admin_password ));
	my_style_add( priv->p5_admin_password, "labelinfo" );

	priv->p5_open_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-open-label" );
	g_return_if_fail( priv->p5_open_label && GTK_IS_LABEL( priv->p5_open_label ));
	my_style_add( priv->p5_open_label, "labelinfo" );

	priv->p5_apply_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-apply-label" );
	g_return_if_fail( priv->p5_apply_label && GTK_IS_LABEL( priv->p5_apply_label ));
	my_style_add( priv->p5_apply_label, "labelinfo" );
}

static void
p5_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p5_do_display";
	ofaRestoreAssistantPrivate *priv;
	const gchar *cstr;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p5_uri_label ), priv->p1_uri );
	gtk_label_set_text( GTK_LABEL( priv->p5_dossier_label ), priv->p2_dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p5_name_label ), priv->p2_exercice_name );

	if( priv->p3_dbsu_credentials ){
		cstr = ofa_idbconnect_get_account( priv->p2_connect );
		gtk_label_set_text( GTK_LABEL( priv->p5_su_account ), cstr );
		cstr = ofa_idbconnect_get_password( priv->p2_connect );
		gtk_label_set_text( GTK_LABEL( priv->p5_su_password ), my_strlen( cstr ) ? "******" : "" );
	} else {
		gtk_label_set_text( GTK_LABEL( priv->p5_su_account ), "(unset)" );
		gtk_label_set_text( GTK_LABEL( priv->p5_su_password ), "" );
	}

	gtk_label_set_text( GTK_LABEL( priv->p5_admin_account ), priv->p4_account );
	gtk_label_set_text( GTK_LABEL( priv->p5_admin_password ), "******" );

	priv->p5_open = ofa_dossier_actions_bin_get_open_on_create( priv->p4_actions );
	gtk_label_set_text( GTK_LABEL( priv->p5_open_label ), priv->p5_open ? "True":"False" );
	priv->p5_apply = ofa_dossier_actions_bin_get_apply_actions( priv->p4_actions );
	gtk_label_set_text( GTK_LABEL( priv->p5_apply_label ), priv->p5_apply ? "True":"False" );
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

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p6_page = page;

	priv->p6_textview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-textview" );
	g_return_if_fail( priv->p6_textview && GTK_IS_TEXT_VIEW( priv->p6_textview ));

	priv->p6_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label" );
	g_return_if_fail( priv->p6_label && GTK_IS_LABEL( priv->p6_label ));
}

static void
p6_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p6_do_display";
	ofaRestoreAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	if( !p6_restore_confirmed( self )){
#if 0
		ofaDossierCollection *collection;
		if( priv->p2_is_new_dossier ){
			collection = ofa_hub_get_dossier_collection( priv->hub );
			ofa_dossier_collection_delete_period( collection, priv->p2_connect, NULL, TRUE, NULL );
		}
#endif
		gtk_label_set_text(
				GTK_LABEL( priv->p6_label ),
				_( "The restore operation has been cancelled by the user." ));

	} else {
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

		/* prevent the window manager to close this assistant */
		priv->is_destroy_allowed = FALSE;
		ofa_hub_dossier_close( priv->hub );
		priv->is_destroy_allowed = TRUE;

		g_idle_add(( GSourceFunc ) p6_do_restore, self );
	}
}

static gboolean
p6_restore_confirmed( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *str;

	priv = ofa_restore_assistant_get_instance_private( self );

	str = g_strdup_printf(
				_( "The restore operation will drop, fully reset and repopulate "
					"the '%s' database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to restore into this database ?" ), priv->p2_exercice_name );

	ok = my_utils_dialog_question( str, _( "_Restore" ));

	g_free( str );

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
	gchar *msg;
	const gchar *style;
	GtkWidget *dlg;

	priv = ofa_restore_assistant_get_instance_private( self );

	/* restore the backup */
	ok = ofa_idbconnect_restore_db(
				priv->p2_connect, NULL, priv->p1_uri, priv->p1_format,
				priv->p4_account, priv->p4_password, ( ofaMsgCb ) p6_msg_cb, self );

	if( ok ){
		style = "labelinfo";
		msg = g_strdup_printf(
				_( "The '%s' archive URI has been successfully restored "
					"into the '%s' dossier." ), priv->p1_uri, priv->p2_dossier_name );
	} else {
		style = "labelerror";
		msg = g_strdup_printf(
				_( "Unable to restore the '%s' archive URI.\n"
					"Please fix the errors and retry." ), priv->p1_uri );
	}

	dlg = gtk_message_dialog_new(
				GTK_WINDOW( self ),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				"%s", msg );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );

	gtk_label_set_text( GTK_LABEL( priv->p6_label ), msg );
	my_style_add( priv->p6_label, style );

	g_free( msg );

	if( ok ){
		g_idle_add(( GSourceFunc ) p6_do_open, self );
	} else {
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
	}

	return( G_SOURCE_REMOVE );
}

static void
p6_msg_cb( const gchar *buffer, ofaRestoreAssistant *self )
{
	static const gchar *thisfn = "ofa_restore_assistant_p6_msg_cb";
	ofaRestoreAssistantPrivate *priv;
	GtkTextBuffer *textbuf;
	GtkTextIter enditer;
	const gchar *charset;
	gchar *utf8;

	if( 0 ){
		g_debug( "%s: buffer=%p, self=%p", thisfn, ( void * ) buffer, ( void * ) self );
		g_debug( "%s: buffer=%s, self=%p", thisfn, buffer, ( void * ) self );
	}

	priv = ofa_restore_assistant_get_instance_private( self );

	textbuf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p6_textview ));
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
		g_debug( "%s: message is not in UTF-8 charset, converting it", thisfn );
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
			GTK_TEXT_VIEW( priv->p6_textview),
			gtk_text_buffer_get_mark( textbuf, "insert" ),
			0.0, FALSE, 0.0, 0.0 );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

/*
 * Open the dossier if asked for.
 *
 * Actually, because this assistant is non modal, the dossier is opened
 * before the assistant has quit.
 */
static gboolean
p6_do_open( ofaRestoreAssistant *self )
{
	static const gchar *thisfn = "ofa_restore_assistant_p6_do_open";
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( self );

	g_debug( "%s: self=%p, meta=%p, period=%p, account=%s",
			thisfn, ( void * ) self, ( void * ) priv->p2_dossier_meta, ( void * ) priv->p2_exercice_meta, priv->p4_account );

	if( priv->p5_open ){
		ofa_dossier_open_run(
				priv->getter, GTK_WINDOW( self ),
				priv->p2_dossier_meta, priv->p2_exercice_meta, priv->p4_account, priv->p4_password );

		g_debug( "%s: return from ofa_dossier_open_run", thisfn );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	return( G_SOURCE_REMOVE );
}

/*
 * settings is "folder;filter_type;"
 */
static void
read_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_restore_assistant_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		g_free( priv->p1_folder );
		priv->p1_folder = g_strdup( cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		priv->p1_filter = atoi( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_restore_assistant_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );

	str = g_strdup_printf( "%s;%d;",
				priv->p1_folder, priv->p1_filter );

	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_restore_assistant_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
