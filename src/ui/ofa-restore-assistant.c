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

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"
#include "core/ofa-admin-credentials-bin.h"
#include "core/ofa-dbms-root-bin.h"

#include "ui/my-progress-bar.h"
#include "ui/ofa-dossier-new-mini.h"
#include "ui/ofa-dossier-treeview.h"
#include "ui/ofa-exercice-treeview.h"
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

/* private instance data
 */
struct _ofaRestoreAssistantPrivate {

	/* p1: select file to be imported
	 */
	GtkFileChooser         *p2_chooser;
	gchar                  *p2_folder;
	gchar                  *p2_fname;		/* the utf-8 to be restored filename */

	/* p2: select the dossier target
	 */
	ofaDossierTreeview     *p3_dossier_treeview;
	GtkWidget              *p3_new_dossier_btn;
	gchar                  *p3_dossier;
	gchar                  *p3_database;
	ofaIDbms               *p3_dbms;

	/* p3: DBMS root account
	 */
	ofaDBMSRootBin         *p4_dbms_credentials;
	gchar                  *p4_account;
	gchar                  *p4_password;

	/* p4: dossier administrative credentials
	 */
	ofaAdminCredentialsBin *p5_admin_credentials;
	gchar                  *p5_account;
	gchar                  *p5_password;
	gboolean                p5_open;

	/* p5: restore the file, display the result
	 */
	GtkWidget              *p7_page;

	/* runtime data
	 */
	GtkWidget              *current_page_w;
};

/* the user preferences stored as a string list
 * folder
 */
static const gchar *st_prefs_import     = "RestoreAssistant";

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-restore-assistant.ui";
static const gchar *st_ui_id            = "RestoreAssistant";

static void            p2_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p2_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p2_on_selection_changed( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static void            p2_on_file_activated( GtkFileChooser *chooser, ofaRestoreAssistant *self );
static void            p2_check_for_complete( ofaRestoreAssistant *self );
static void            p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void            p3_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p3_on_dossier_changed( ofaDossierTreeview *view, const gchar *dname, ofaRestoreAssistant *assistant );
static void            p3_on_dossier_activated( ofaDossierTreeview *view, const gchar *dname, ofaRestoreAssistant *assistant );
static void            p3_on_dossier_new( GtkButton *button, ofaRestoreAssistant *assistant );
static void            p3_check_for_complete( ofaRestoreAssistant *self );
static void            p3_do_forward( ofaRestoreAssistant *self, GtkWidget *page );
static void            p4_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p4_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self );
static void            p4_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p4_check_for_complete( ofaRestoreAssistant *self );
static void            p4_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p5_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p5_on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self );
static void            p5_on_open_toggled( GtkToggleButton *button, ofaRestoreAssistant *self );
static void            p5_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p5_check_for_complete( ofaRestoreAssistant *self );
static void            p5_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p6_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static void            p7_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page );
static gboolean        p7_restore_confirmed( const ofaRestoreAssistant *self );
static gboolean        p7_do_restore( ofaRestoreAssistant *self );
static gboolean        p7_do_credentials( ofaRestoreAssistant *self );
static gboolean        p7_do_open( ofaRestoreAssistant *self );
static void            get_settings( ofaRestoreAssistant *self );
static void            update_settings( ofaRestoreAssistant *self );

G_DEFINE_TYPE( ofaRestoreAssistant, ofa_restore_assistant, MY_TYPE_ASSISTANT );

static const ofsAssistant st_pages_cb [] = {
		{ ASSIST_PAGE_INTRO,
				NULL,
				NULL,
				NULL },
		{ ASSIST_PAGE_SELECT,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_display,
				( myAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_TARGET,
				( myAssistantCb ) p3_do_init,
				NULL,
				( myAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_ROOT,
				( myAssistantCb ) p4_do_init,
				( myAssistantCb ) p4_display,
				( myAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_ADMIN,
				( myAssistantCb ) p5_do_init,
				( myAssistantCb ) p5_display,
				( myAssistantCb ) p5_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				NULL,
				( myAssistantCb ) p6_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				NULL,
				( myAssistantCb ) p7_do_display,
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

	g_free( priv->p2_folder );
	g_free( priv->p2_fname );
	g_free( priv->p3_dossier );
	g_free( priv->p3_database );
	g_free( priv->p4_account );
	g_free( priv->p4_password );
	g_free( priv->p5_account );
	g_free( priv->p5_password );

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
		g_clear_object( &priv->p3_dbms );
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

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_RESTORE_ASSISTANT,
							MY_PROP_MAIN_WINDOW, main_window,
							MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
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

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * initialize the GtkFileChooser widget with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p2_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p2_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *widget;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-filechooser" );
	g_return_if_fail( widget && GTK_IS_FILE_CHOOSER_WIDGET( widget ));
	priv->p2_chooser = GTK_FILE_CHOOSER( widget );

	g_signal_connect(
			G_OBJECT( widget ), "selection-changed", G_CALLBACK( p2_on_selection_changed ), self );
	g_signal_connect(
			G_OBJECT( widget ), "file-activated", G_CALLBACK( p2_on_file_activated ), self );
}

static void
p2_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p2_display";
	ofaRestoreAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	if( priv->p2_folder ){
		gtk_file_chooser_set_current_folder( priv->p2_chooser, priv->p2_folder );
	}
}

static void
p2_on_selection_changed( GtkFileChooser *chooser, ofaRestoreAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_on_file_activated( GtkFileChooser *chooser, ofaRestoreAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	g_free( priv->p2_fname );
	priv->p2_fname = gtk_file_chooser_get_filename( priv->p2_chooser );
	g_debug( "p2_check_for_complete: fname=%s", priv->p2_fname );

	ok = priv->p2_fname &&
			g_utf8_strlen( priv->p2_fname, -1 ) > 0 &&
			my_utils_file_is_readable_file( priv->p2_fname );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p2_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p2_folder );
	priv->p2_folder = gtk_file_chooser_get_current_folder( priv->p2_chooser );

	update_settings( self );
}

/*
 * p2: target dossier and database
 */
static void
p3_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p3_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *parent, *label;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = self->priv;
	priv->current_page_w = page;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-fname" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_fname );

	priv->p3_dossier_treeview = ofa_dossier_treeview_new();
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	ofa_dossier_treeview_attach_to( priv->p3_dossier_treeview, GTK_CONTAINER( parent ));
	ofa_dossier_treeview_set_columns( priv->p3_dossier_treeview, DOSSIER_DISP_DNAME );

	g_signal_connect( priv->p3_dossier_treeview, "changed", G_CALLBACK( p3_on_dossier_changed ), self );
	g_signal_connect( priv->p3_dossier_treeview, "activated", G_CALLBACK( p3_on_dossier_activated ), self );

	priv->p3_new_dossier_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-new-dossier" );

	g_signal_connect( priv->p3_new_dossier_btn, "clicked", G_CALLBACK( p3_on_dossier_new ), self );

	p3_check_for_complete( self );
}

static void
p3_on_dossier_changed( ofaDossierTreeview *view, const gchar *dname, ofaRestoreAssistant *assistant )
{
	p3_check_for_complete( assistant );
}

static void
p3_on_dossier_activated( ofaDossierTreeview *view, const gchar *dname, ofaRestoreAssistant *assistant )
{
	p3_check_for_complete( assistant );
}

static void
p3_on_dossier_new( GtkButton *button, ofaRestoreAssistant *assistant )
{
	ofaRestoreAssistantPrivate *priv;
	gchar *dname, *account, *password;

	priv = assistant->priv;

	if( ofa_dossier_new_mini_run( MY_WINDOW( assistant )->prot->main_window, &dname, &account, &password )){

		g_free( priv->p3_dossier );
		priv->p3_dossier = dname;

		g_free( priv->p4_account );
		priv->p4_account = account;

		g_free( priv->p4_password );
		priv->p4_password = password;

		ofa_dossier_treeview_add_row( priv->p3_dossier_treeview, dname );
		ofa_dossier_treeview_set_selected( priv->p3_dossier_treeview, dname );
	}
}

static void
p3_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	g_free( priv->p3_dossier );
	priv->p3_dossier = ofa_dossier_treeview_get_selected( priv->p3_dossier_treeview );
	ok = priv->p3_dossier && g_utf8_strlen( priv->p3_dossier, -1 );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p3_do_forward( ofaRestoreAssistant *self, GtkWidget *page )
{
	ofaRestoreAssistantPrivate *priv;
	gchar *provider, *slist;
	gchar **array;

	priv = self->priv;
	g_clear_object( &priv->p3_dbms );

	provider = ofa_settings_get_dossier_provider( priv->p3_dossier );
	g_return_if_fail( provider && g_utf8_strlen( provider, -1 ));

	priv->p3_dbms = ofa_idbms_get_provider_by_name( provider );
	g_return_if_fail( priv->p3_dbms && OFA_IS_IDBMS( priv->p3_dbms ));

	slist = ofa_idbms_get_current( priv->p3_dbms, priv->p3_dossier );
	array = g_strsplit( slist, ";", -1 );

	priv->p3_database = g_strdup( *( array+1 ));

	g_strfreev( array );
	g_free( slist );
	g_free( provider );

	update_settings( self );
}

/*
 * p3: get DBMS root account and password
 */
static void
p4_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p4_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *label, *parent;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-dossier" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_dossier );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_database );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-connect-infos" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	ofa_idbms_connect_display_attach_to( priv->p3_dossier, GTK_CONTAINER( parent ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-dra-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p4_dbms_credentials = ofa_dbms_root_bin_new();
	ofa_dbms_root_bin_attach_to( priv->p4_dbms_credentials, GTK_CONTAINER( parent ), NULL );
	ofa_dbms_root_bin_set_dossier( priv->p4_dbms_credentials, priv->p3_dossier );

	if( priv->p4_account && priv->p4_password ){
		ofa_dbms_root_bin_set_credentials( priv->p4_dbms_credentials, priv->p4_account, priv->p4_password );
	}

	g_signal_connect( priv->p4_dbms_credentials, "changed", G_CALLBACK( p4_on_dbms_root_changed ), self );
}

static void
p4_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self )
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
p4_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	p4_check_for_complete( self );
}

static void
p4_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = ofa_dbms_root_bin_is_valid( priv->p4_dbms_credentials );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
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
 * p4: get dossier administrative account and password
 */
static void
p5_do_init( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p5_do_init";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *label, *parent, *toggle;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dossier" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_dossier );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_database );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-admin-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_admin_credentials = ofa_admin_credentials_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p5_admin_credentials ));

	g_signal_connect(
			priv->p5_admin_credentials, "changed", G_CALLBACK( p5_on_admin_credentials_changed ), self );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-open" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( toggle ), priv->p5_open );

	g_signal_connect( toggle, "toggled", G_CALLBACK( p5_on_open_toggled ), self );
}

static void
p5_on_admin_credentials_changed( ofaAdminCredentialsBin *bin, const gchar *account, const gchar *password, ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p5_account );
	priv->p5_account = g_strdup( account );

	g_free( priv->p5_password );
	priv->p5_password = g_strdup( password );

	p5_check_for_complete( self );
}

static void
p5_on_open_toggled( GtkToggleButton *button, ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;

	priv = self->priv;

	priv->p5_open = gtk_toggle_button_get_active( button );
}

/*
 * ask the user to confirm the operation
 */
static void
p5_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	p5_check_for_complete( self );
}

static void
p5_check_for_complete( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = ofa_admin_credentials_bin_is_valid( priv->p5_admin_credentials );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_w, ok );
}

static void
p5_do_forward( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p5_do_forward";
	/*ofaRestoreAssistantPrivate *priv;*/

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	update_settings( self );
}

/*
 * confirmation page
 */
static void
p6_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p6_display";
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *label;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-fname" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p2_fname );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-dossier" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_dossier );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p3_database );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-dbms-account" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p4_account );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-dbms-password" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), "******" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-adm-account" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p5_account );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-adm-password" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), "******" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-open" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), priv->p5_open ? _( "True" ): _( "False" ));
}

static void
p7_do_display( ofaRestoreAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_restore_assistant_p7_do_display";
	ofaRestoreAssistantPrivate *priv;
	ofoDossier *dossier;
	GtkWidget *label;

	g_return_if_fail( OFA_IS_RESTORE_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = self->priv;
	priv->current_page_w = page;

	if( !p7_restore_confirmed( self )){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-label1" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_label_set_text(
				GTK_LABEL( label ), _( "The restore operation has been cancelled by the user." ));

	} else {
		/* first close the currently opened dossier if we are going to
		 * restore to this same dossier */
		dossier = ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window );
		if( dossier ){
			g_return_if_fail( OFO_IS_DOSSIER( dossier ));
			if( !g_utf8_collate( priv->p3_dossier, ofo_dossier_get_name( dossier ))){
				ofa_main_window_close_dossier( MY_WINDOW( self )->prot->main_window );
			}
		}

		g_idle_add(( GSourceFunc ) p7_do_restore, self );
	}
}

static gboolean
p7_restore_confirmed( const ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	GtkWidget *dialog;
	gchar *str;
	gint response;

	priv = self->priv;

	str = g_strdup_printf(
				_( "The restore operation will drop, fully reset and repopulate "
					"the '%s' database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to restore into this database ?" ), priv->p3_database );

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", str );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_Cancel" ), GTK_RESPONSE_CANCEL,
			_( "_Restore" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	g_free( str );
	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

static gboolean
p7_do_restore( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *str;
	GtkWidget *label;

	priv = self->priv;

	/* restore the backup */
	ok = ofa_idbms_restore(
			priv->p3_dbms, priv->p3_dossier, priv->p2_fname, priv->p4_account, priv->p4_password );

	if( ok ){
		str = g_strdup_printf(
				_( "The '%s' backup file has been successfully restored "
					"into the '%s' dossier." ), priv->p2_fname, priv->p3_dossier );
	} else {
		str = g_strdup_printf(
				_( "Unable to restore the '%s' backup file.\n"
					"Please fix the errors and retry." ), priv->p2_fname );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->current_page_w ), "p7-label1" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	/* create administrative credentials */
	if( ok ){
		g_idle_add(( GSourceFunc ) p7_do_credentials, self );
	}

	return( G_SOURCE_REMOVE );
}

/*
 * create the dossier administrative credentials
 */
static gboolean
p7_do_credentials( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	gboolean ok;
	gchar *str;
	GtkWidget *label;

	priv = self->priv;

	ok = ofa_idbms_set_admin_credentials(
				priv->p3_dbms, priv->p3_dossier,
				priv->p4_account, priv->p4_password,
				priv->p5_account, priv->p5_password );

	if( ok ){
		str = g_strdup(
				_( "The dossier administrative credentials have been set." ));
	} else {
		str = g_strdup(
				_( "Unable to set the dossier administrative credentials." ));
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->current_page_w ), "p7-label2" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	/* open the dossier */
	if( ok ){
		g_idle_add(( GSourceFunc ) p7_do_open, self );
	}

	return( G_SOURCE_REMOVE );
}

/*
 * open the dossier if asked for
 */
static gboolean
p7_do_open( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	ofsDossierOpen *sdo;

	priv = self->priv;

	if( priv->p5_open ){
		sdo = g_new0( ofsDossierOpen, 1 );
		sdo->dname = g_strdup( priv->p3_dossier );
		sdo->dbname = g_strdup( priv->p3_database );
		sdo->account = g_strdup( priv->p5_account );
		sdo->password = g_strdup( priv->p5_password );
		g_signal_emit_by_name( MY_WINDOW( self )->prot->main_window, OFA_SIGNAL_DOSSIER_OPEN, sdo );
	}

	return( G_SOURCE_REMOVE );
}

/*
 * settings is "folder;open;"
 */
static void
get_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = self->priv;

	list = ofa_settings_get_string_list( st_prefs_import );

	it = list;
	cstr = ( it && it->data ? ( const gchar * ) it->data : NULL );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		g_free( priv->p2_folder );
		priv->p2_folder = g_strdup( cstr );
	}
	it = it ? it->next : NULL;
	cstr = ( it && it->data ? ( const gchar * ) it->data : NULL );
	if( cstr && g_utf8_strlen( cstr, -1 )){
		priv->p5_open = my_utils_boolean_from_str( cstr );
	}

	ofa_settings_free_string_list( list );
}

static void
update_settings( ofaRestoreAssistant *self )
{
	ofaRestoreAssistantPrivate *priv;
	GList *list;

	priv = self->priv;

	list = g_list_append( NULL, g_strdup( priv->p2_folder ));
	list = g_list_append( list, g_strdup_printf( "%s", priv->p5_open ? "True":"False" ));

	ofa_settings_set_string_list( st_prefs_import, list );

	ofa_settings_free_string_list( list );
}
