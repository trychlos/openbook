/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-admin-credentials-bin.h"
#include "core/ofa-dbms-root-bin.h"

#include "ui/ofa-application.h"
#include "ui/ofa-dossier-new-mini.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-restore-assistant.h"

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

	/* p1: select file to be imported
	 */
	GtkFileChooser         *p1_chooser;
	gchar                  *p1_folder;
	gchar                  *p1_furi;		/* the utf-8 to be restored file uri */
	gint                    p1_filter;

	/* p2: select the dossier target
	 */
	GtkWidget              *p2_furi;
	ofaDossierTreeview     *p2_dossier_tview;
	ofaIDBDossierMeta      *p2_dossier_meta;
	ofaIDBExerciceMeta     *p2_exercice_meta;
	gchar                  *p2_dbname;
	gboolean                p2_is_new_dossier;
	GSimpleAction          *p2_new_action;

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
	gboolean                is_destroy_allowed;
}
	ofaRestoreAssistantPrivate;

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

#define CHOOSER_FILTER_TYPE                "file-chooser-filter-type"

static const gchar *st_resource_ui       = "/org/trychlos/openbook/ui/ofa-restore-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname );
static gboolean iwindow_is_destroy_allowed( const myIWindow *instance );
static void     set_settings( ofaRestoreAssistant *self );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p1_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p1_set_filters( ofaRestoreAssistant *self, GtkFileChooser *chooser );
static void     p1_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static void     p1_on_file_activated( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static gboolean p1_check_for_complete( ofaRestoreAssistant *self );
static void     p1_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void     p2_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static gboolean p2_on_dossier_changed( ofaDossierTreeview *view, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaRestoreAssistant *assistant );
static void     p2_on_dossier_activated( ofaDossierTreeview *view, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaRestoreAssistant *assistant );
static void     p2_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRestoreAssistant *assistant );
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
static gboolean p6_restore_confirmed( ofaRestoreAssistant *self );
static gboolean p6_do_restore( ofaRestoreAssistant *self );
static gboolean p6_do_open( ofaRestoreAssistant *self );
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
				( myIAssistantCb ) p3_do_forward },
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

	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->p2_dossier_meta );
		g_clear_object( &priv->p2_exercice_meta );
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
	iface->read_settings = iwindow_read_settings;
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

	my_iwindow_set_settings( instance, ofa_hub_get_user_settings( priv->hub ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );
}

/*
 * settings is "folder;open;filter_type;"
 */
static void
iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname )
{
	static const gchar *thisfn = "ofa_restore_assistant_iwindow_read_settings";
	ofaRestoreAssistantPrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	g_debug( "%s: instance=%p, settings=%p, keyname=%s",
			thisfn, ( void * ) instance, ( void * ) settings, keyname );

	priv = ofa_restore_assistant_get_instance_private( OFA_RESTORE_ASSISTANT( instance ));

	list = my_isettings_get_string_list( settings, SETTINGS_GROUP_GENERAL, keyname );

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

	my_isettings_free_string_list( settings, list );
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

static void
set_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	myISettings *settings;
	gchar *keyname, *str;

	priv = ofa_restore_assistant_get_instance_private( self );

	settings = my_iwindow_get_settings( MY_IWINDOW( self ));
	keyname = my_iwindow_get_keyname( MY_IWINDOW( self ));

	str = g_strdup_printf( "%s;%s;%d;",
				priv->p1_folder, priv->p4_open ? "True":"False", priv->p1_filter );

	my_isettings_set_string( settings, SETTINGS_GROUP_GENERAL, keyname, str );

	g_free( str );
	g_free( keyname );
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
	return( ofa_prefs_assistant_is_willing_to_quit( keyval ));
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

	g_free( priv->p1_furi );
	priv->p1_furi = gtk_file_chooser_get_uri( priv->p1_chooser );
	g_debug( "p1_check_for_complete: furi=%s", priv->p1_furi );

	ok = my_strlen( priv->p1_furi ) > 0 &&
			my_utils_uri_is_readable_file( priv->p1_furi );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
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

	set_settings( self );
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
	ofaButtonsBox *buttons_box;
	GMenu *menu;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p2_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-furi" );
	g_return_if_fail( priv->p2_furi && GTK_IS_LABEL( priv->p2_furi ));
	my_style_add( priv->p2_furi, "labelinfo" );

	priv->p2_dossier_tview = ofa_dossier_treeview_new( priv->hub );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_dossier_tview ));
	ofa_dossier_treeview_set_settings_key( priv->p2_dossier_tview, priv->settings_prefix );
	ofa_dossier_treeview_setup_columns( priv->p2_dossier_tview );
	ofa_dossier_treeview_set_show_all( priv->p2_dossier_tview, FALSE );

	g_signal_connect( priv->p2_dossier_tview, "ofa-doschanged", G_CALLBACK( p2_on_dossier_changed ), self );
	g_signal_connect( priv->p2_dossier_tview, "ofa-dosactivated", G_CALLBACK( p2_on_dossier_activated ), self );

	ofa_dossier_treeview_setup_store( priv->p2_dossier_tview );

	priv->p2_is_new_dossier = FALSE;

	buttons_box = ofa_buttons_box_new();
	my_utils_widget_set_margins( GTK_WIDGET( buttons_box ), 0, 0, 2, 2 );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-buttons-box" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( buttons_box ));

	/* new action */
	priv->p2_new_action = g_simple_action_new( "new", NULL );
	g_signal_connect( priv->p2_new_action, "activate", G_CALLBACK( p2_on_new_activated ), self );
	ofa_iactionable_set_menu_item(
			OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->p2_new_action ),
			OFA_IACTIONABLE_NEW_ITEM );
	ofa_buttons_box_append_button(
			buttons_box,
			ofa_iactionable_new_button(
					OFA_IACTIONABLE( self ), priv->settings_prefix, G_ACTION( priv->p2_new_action ),
					_( "Ne_w..." )));
	g_simple_action_set_enabled( priv->p2_new_action, TRUE );

	/* contextual menu */
	menu = ofa_iactionable_get_menu( OFA_IACTIONABLE( self ), priv->settings_prefix );
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->p2_dossier_tview ), OFA_IACTIONABLE( self ),
			menu );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->p2_dossier_tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->p2_dossier_tview ), OFA_IACTIONABLE( priv->p2_dossier_tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
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

	gtk_label_set_text( GTK_LABEL( priv->p2_furi ), priv->p1_furi );

	p2_check_for_complete( self );

	gtk_widget_grab_focus( GTK_WIDGET( priv->p2_dossier_tview ));
}

/*
 * Returns %TRUE if the page is validable after the change
 * (i.e. a target dossier+exercice are selected)
 */
static gboolean
p2_on_dossier_changed( ofaDossierTreeview *view, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaRestoreAssistant *assistant )
{
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( assistant );

	g_clear_object( &priv->p2_dossier_meta );
	g_clear_object( &priv->p2_exercice_meta );

	if( meta && period ){
		priv->p2_dossier_meta = g_object_ref( meta );
		priv->p2_exercice_meta = g_object_ref( period );
	}

	return( p2_check_for_complete( assistant ));
}

static void
p2_on_dossier_activated( ofaDossierTreeview *view, ofaIDBDossierMeta *meta, ofaIDBExerciceMeta *period, ofaRestoreAssistant *assistant )
{
	if( p2_on_dossier_changed( view, meta, period, assistant )){
		gtk_assistant_next_page( GTK_ASSISTANT( assistant ));
	}
}

static void
p2_on_new_activated( GSimpleAction *action, GVariant *empty, ofaRestoreAssistant *assistant )
{
	ofaRestoreAssistantPrivate *priv;
	const gchar *dossier_name;
	ofaIDBDossierMeta *meta;

	priv = ofa_restore_assistant_get_instance_private( assistant );

	meta = NULL;

	if( ofa_dossier_new_mini_run( priv->getter, GTK_WINDOW( assistant ), &meta )){

		g_clear_object( &priv->p2_dossier_meta );
		priv->p2_dossier_meta = meta;

		priv->p2_is_new_dossier = TRUE;

		dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p2_dossier_meta );
		ofa_dossier_treeview_set_selected( priv->p2_dossier_tview, dossier_name );

		gtk_widget_grab_focus( GTK_WIDGET( priv->p2_dossier_tview ));
	}
}

static gboolean
p2_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_restore_assistant_get_instance_private( self );

	ok = ( priv->p2_dossier_meta && OFA_IS_IDBDOSSIER_META( priv->p2_dossier_meta ) &&
			priv->p2_exercice_meta && OFA_IS_IDBEXERCICE_META( priv->p2_exercice_meta ));

	if( ok ){
		g_free( priv->p2_dbname );
		priv->p2_dbname = ofa_idbexercice_meta_get_name( priv->p2_exercice_meta );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	set_settings( self );
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

	priv->p3_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-furi" );
	g_return_if_fail( priv->p3_furi && GTK_IS_LABEL( priv->p3_furi ));
	my_style_add( priv->p3_furi, "labelinfo" );

	priv->p3_dossier = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dossier" );
	g_return_if_fail( priv->p3_dossier && GTK_IS_LABEL( priv->p3_dossier ));
	my_style_add( priv->p3_dossier, "labelinfo" );

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
	my_style_add( priv->p3_message, "labelerror" );
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
	const gchar *dossier_name;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_furi ), priv->p1_furi );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p2_dossier_meta );
	gtk_label_set_text( GTK_LABEL( priv->p3_dossier ), dossier_name );

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

	provider = ofa_idbdossier_meta_get_provider( priv->p2_dossier_meta );
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));
	editor = ofa_idbprovider_new_editor( provider, FALSE );
	g_object_unref( provider );

	ofa_idbeditor_set_meta( editor, priv->p2_dossier_meta, priv->p2_exercice_meta );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( editor ));
	my_utils_size_group_add_size_group(
			priv->p3_hgroup, ofa_idbeditor_get_size_group( editor, 0 ));

	ofa_dbms_root_bin_set_meta( priv->p3_dbms_credentials, priv->p2_dossier_meta );

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

	priv = ofa_restore_assistant_get_instance_private( self );

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

	priv = ofa_restore_assistant_get_instance_private( self );

	p3_set_message( self, "" );

	ok = ofa_dbms_root_bin_is_valid( priv->p3_dbms_credentials, &message );
	if( !ok ){
		p3_set_message( self, message );
		g_free( message );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p3_set_message( ofaRestoreAssistant *self, const gchar *message )
{
	ofaRestoreAssistantPrivate *priv;

	priv = ofa_restore_assistant_get_instance_private( self );

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

	priv = ofa_restore_assistant_get_instance_private( self );

	g_clear_object( &priv->p3_connect );
	provider = ofa_idbdossier_meta_get_provider( priv->p2_dossier_meta );
	priv->p3_connect = ofa_idbprovider_new_connect( provider );
	if( !ofa_idbconnect_open_with_meta(
			priv->p3_connect, priv->p3_account, priv->p3_password, priv->p2_dossier_meta, NULL )){
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

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p4_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-furi" );
	g_return_if_fail( priv->p4_furi && GTK_IS_LABEL( priv->p4_furi ));
	my_style_add( priv->p4_furi, "labelinfo" );

	priv->p4_dossier = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-dossier" );
	g_return_if_fail( priv->p4_dossier && GTK_IS_LABEL( priv->p4_dossier ));
	my_style_add( priv->p4_dossier, "labelinfo" );

	priv->p4_database = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-database" );
	g_return_if_fail( priv->p4_database && GTK_IS_LABEL( priv->p4_database ));
	my_style_add( priv->p4_database, "labelinfo" );

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
	my_style_add( priv->p4_message, "labelerror" );

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
	const gchar *dossier_name;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p4_furi ), priv->p1_furi );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p2_dossier_meta );
	gtk_label_set_text( GTK_LABEL( priv->p4_dossier ), dossier_name );

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

	priv = ofa_restore_assistant_get_instance_private( self );

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

	priv = ofa_restore_assistant_get_instance_private( self );

	priv->p4_open = gtk_toggle_button_get_active( button );
}

static void
p4_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_restore_assistant_get_instance_private( self );

	p4_set_message( self, "" );

	ok = ofa_admin_credentials_bin_is_valid( priv->p4_admin_credentials, &message );
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

	set_settings( self );
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

	priv->p5_furi = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-furi" );
	g_return_if_fail( priv->p5_furi && GTK_IS_LABEL( priv->p5_furi ));
	my_style_add( priv->p5_furi, "labelinfo" );

	priv->p5_dossier = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dossier" );
	g_return_if_fail( priv->p5_dossier && GTK_IS_LABEL( priv->p5_dossier ));
	my_style_add( priv->p5_dossier, "labelinfo" );

	priv->p5_database = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-database" );
	g_return_if_fail( priv->p5_database && GTK_IS_LABEL( priv->p5_database ));
	my_style_add( priv->p5_database, "labelinfo" );

	priv->p5_root_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbms-account" );
	g_return_if_fail( priv->p5_root_account && GTK_IS_LABEL( priv->p5_root_account ));
	my_style_add( priv->p5_root_account, "labelinfo" );

	priv->p5_root_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbms-password" );
	g_return_if_fail( priv->p5_root_password && GTK_IS_LABEL( priv->p5_root_password ));
	my_style_add( priv->p5_root_password, "labelinfo" );

	priv->p5_admin_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-adm-account" );
	g_return_if_fail( priv->p5_admin_account && GTK_IS_LABEL( priv->p5_admin_account ));
	my_style_add( priv->p5_admin_account, "labelinfo" );

	priv->p5_admin_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-adm-password" );
	g_return_if_fail( priv->p5_admin_password && GTK_IS_LABEL( priv->p5_admin_password ));
	my_style_add( priv->p5_admin_password, "labelinfo" );

	priv->p5_open = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-open" );
	g_return_if_fail( priv->p5_open && GTK_IS_LABEL( priv->p5_open ));
	my_style_add( priv->p5_open, "labelinfo" );
}

static void
p5_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p5_do_display";
	ofaRestoreAssistantPrivate *priv;
	const gchar *dossier_name;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p2_dossier_meta );

	gtk_label_set_text( GTK_LABEL( priv->p5_furi ), priv->p1_furi );
	gtk_label_set_text( GTK_LABEL( priv->p5_dossier ), dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p5_database ), priv->p2_dbname );
	gtk_label_set_text( GTK_LABEL( priv->p5_root_account ), priv->p3_account );
	gtk_label_set_text( GTK_LABEL( priv->p5_root_password ), "******" );
	gtk_label_set_text( GTK_LABEL( priv->p5_admin_account ), priv->p4_account );
	gtk_label_set_text( GTK_LABEL( priv->p5_admin_password ), "******" );
	gtk_label_set_text( GTK_LABEL( priv->p5_open ), priv->p4_open ? _( "True" ): _( "False" ));
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

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_restore_assistant_get_instance_private( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	if( !p6_restore_confirmed( self )){
		if( priv->p2_is_new_dossier ){
			ofa_idbdossier_meta_remove_meta( priv->p2_dossier_meta );
		}
		gtk_label_set_text(
				GTK_LABEL( priv->p6_label1 ),
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
	gchar *name, *str;

	priv = ofa_restore_assistant_get_instance_private( self );

	name = ofa_idbexercice_meta_get_name( priv->p2_exercice_meta );

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
	gchar *str;
	const gchar *dossier_name, *style;

	priv = ofa_restore_assistant_get_instance_private( self );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p2_dossier_meta );

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
	my_style_add( priv->p6_label1, style );

	g_free( str );

	if( ok ){
		g_idle_add(( GSourceFunc ) p6_do_open, self );
	} else {
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
	}

	return( G_SOURCE_REMOVE );
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

	if( priv->p4_open ){
		ofa_dossier_open_run(
				priv->getter, GTK_WINDOW( self ),
				priv->p2_dossier_meta, priv->p2_exercice_meta, priv->p4_account, priv->p4_password );

		g_debug( "%s: return from ofa_dossier_open_run", thisfn );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	return( G_SOURCE_REMOVE );
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
