/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "my/my-ibin.h"
#include "my/my-iident.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-dossier-collection.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-idbsuperuser.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irecover.h"
#include "api/ofa-prefs.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-dossier.h"

#include "core/ofa-stream-format-bin.h"

#include "ui/ofa-admin-credentials-bin.h"
#include "ui/ofa-dossier-actions-bin.h"
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-recovery-assistant.h"
#include "ui/ofa-target-chooser-bin.h"

/* Restore Assistant
 *
 * pos.  type     enum     title
 * ---   -------  -------  --------------------------------------------
 *   0   Intro    INTRO    Introduction
 *   1   Content  SELECT   Select source files
 *   2   Content  FORMAT   Configure the input format
 *   3   Content  RECOVER  Select the recoverer
 *   4   Content  TARGET   Select dossier and period targets
 *   5   Content  ROOT     Enter DBMS super-user credentials
 *   6   Content  ADMIN    Enter administrative account
 *   7   Confirm  CONFIRM  Summary of the operations to be done
 *   8   Summary  DONE     After recovery
 */

enum {
	ASSIST_PAGE_INTRO = 0,
	ASSIST_PAGE_SELECT,
	ASSIST_PAGE_FORMAT,
	ASSIST_PAGE_RECOVER,
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

	/* runtime
	 */
	gchar                  *settings_prefix;

	/* p1: select source files
	 */
	GtkFileChooser         *p1_entries_chooser;
	GtkFileChooser         *p1_accounts_chooser;
	gchar                  *p1_folder;
	gchar                  *p1_entries_uri;		/* the utf-8 to be restored file uri */
	gchar                  *p1_accounts_uri;

	/* p2: configure the input format
	 */
	GtkWidget              *p2_entries_label;
	GtkWidget              *p2_accounts_label;
	ofaStreamFormatBin     *p2_format_bin;
	GtkWidget              *p2_message;
	gchar                  *p2_format_name;
	ofaStreamFormat        *p2_format_st;

	/* p3: select the recoverer
	 */
	GtkWidget              *p3_entries_label;
	GtkWidget              *p3_accounts_label;
	GtkWidget              *p3_format_label;
	GtkWidget              *p3_tview;
	GtkListStore           *p3_store;
	GList                  *p3_recoverers;
	ofaIRecover            *p3_recoverer;
	gchar                  *p3_recover_name;

	/* p4: select the target
	 */
	GtkWidget              *p4_entries_label;
	GtkWidget              *p4_accounts_label;
	GtkWidget              *p4_format_label;
	GtkWidget              *p4_recover_label;
	ofaTargetChooserBin    *p4_chooser;
	ofaIDBDossierMeta      *p4_dossier_meta;
	gboolean                p4_new_dossier;
	ofaIDBExerciceMeta     *p4_exercice_meta;
	gboolean                p4_new_exercice;
	ofaIDBProvider         *p4_provider;
	ofaIDBConnect          *p4_connect;
	gchar                  *p4_dossier_name;
	gchar                  *p4_exercice_name;
	GtkWidget              *p4_message;

	/* p5: super-user credentials
	 */
	GtkSizeGroup           *p5_hgroup;
	GtkWidget              *p5_entries_label;
	GtkWidget              *p5_accounts_label;
	GtkWidget              *p5_format_label;
	GtkWidget              *p5_recover_label;
	GtkWidget              *p5_dossier_label;
	GtkWidget              *p5_name_label;
	GtkWidget              *p5_connect_parent;
	GtkWidget              *p5_dbsu_parent;
	ofaIDBSuperuser        *p5_dbsu_credentials;
	GtkWidget              *p5_message;
	gchar                  *p5_dossier_name;
	ofaIDBProvider         *p5_provider;

	/* p6: dossier administrative credentials + apply actions
	 */
	GtkSizeGroup           *p6_hgroup;
	GtkWidget              *p6_entries_label;
	GtkWidget              *p6_accounts_label;
	GtkWidget              *p6_format_label;
	GtkWidget              *p6_recover_label;
	GtkWidget              *p6_dossier_label;
	GtkWidget              *p6_name_label;
	GtkWidget              *p6_connect_parent;
	ofaAdminCredentialsBin *p6_admin_credentials;
	ofaDossierActionsBin   *p6_actions;
	gchar                  *p6_account;
	gchar                  *p6_password;
	gboolean                p6_apply_actions;
	GtkWidget              *p6_message;

	/* p7: display operations to be done and ask for confirmation
	 */
	GtkWidget              *p7_entries_label;
	GtkWidget              *p7_accounts_label;
	GtkWidget              *p7_format_label;
	GtkWidget              *p7_recover_label;
	GtkWidget              *p7_dossier_label;
	GtkWidget              *p7_name_label;
	GtkWidget              *p7_su_account;
	GtkWidget              *p7_su_password;
	GtkWidget              *p7_admin_account;
	GtkWidget              *p7_admin_password;
	GtkWidget              *p7_open_label;
	gboolean                p7_open;
	GtkWidget              *p7_apply_label;
	gboolean                p7_apply;

	/* p8: recover from the files, display the result
	 */
	GtkWidget              *p8_page;
	GtkWidget              *p8_textview;
	GtkWidget              *p8_label;
	ofaIDBDossierMeta      *p8_dossier_meta;
	ofaIDBExerciceMeta     *p8_exercice_meta;
}
	ofaRecoveryAssistantPrivate;

/**
 * The columns stored in the page 2 recoverers.
 */
enum {
	REC_COL_LABEL = 0,
	REC_COL_VERSION,
	REC_COL_OBJECT,
	REC_N_COLUMNS
};

static const gchar *st_resource_ui       = "/org/trychlos/openbook/ui/ofa-recovery-assistant.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     iassistant_iface_init( myIAssistantInterface *iface );
static gboolean iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void     p1_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p1_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p1_on_selection_changed( GtkFileChooser *chooser, ofaRecoveryAssistant *self );
static gboolean p1_check_for_complete( ofaRecoveryAssistant *self );
static void     p1_do_forward( ofaRecoveryAssistant *self, GtkWidget *page );
static void     p2_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p2_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p2_on_format_changed( myIBin *bin, ofaRecoveryAssistant *self );
static gboolean p2_check_for_complete( ofaRecoveryAssistant *self );
static void     p2_do_forward( ofaRecoveryAssistant *self, GtkWidget *page );
static void     p3_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p3_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p3_on_selection_changed( GtkTreeSelection *select, ofaRecoveryAssistant *self );
static void     p3_on_selection_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecoveryAssistant *self );
static gboolean p3_check_for_complete( ofaRecoveryAssistant *self );
static void     p3_do_forward( ofaRecoveryAssistant *self, GtkWidget *page );
static void     p4_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p4_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p4_on_target_chooser_changed( ofaTargetChooserBin *bin, ofaIDBDossierMeta *dossier_meta, ofaIDBExerciceMeta *exercice_meta, ofaRecoveryAssistant *self );
static gboolean p4_check_for_complete( ofaRecoveryAssistant *self );
static gboolean p4_check_for_recovery_rules( ofaRecoveryAssistant *self );
static void     p4_set_message( ofaRecoveryAssistant *self, const gchar *message );
static void     p4_do_forward( ofaRecoveryAssistant *self, GtkWidget *page );
static void     p5_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p5_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p5_on_dbsu_credentials_changed( myIBin *bin, ofaRecoveryAssistant *self );
static void     p5_check_for_complete( ofaRecoveryAssistant *self );
static void     p5_set_message( ofaRecoveryAssistant *self, const gchar *message );
static void     p6_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p6_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p6_on_admin_credentials_changed( myIBin *bin, ofaRecoveryAssistant *self );
static void     p6_on_actions_changed( myIBin *bin, ofaRecoveryAssistant *self );
static void     p6_check_for_complete( ofaRecoveryAssistant *self );
static void     p6_set_message( ofaRecoveryAssistant *self, const gchar *message );
static void     p6_do_forward( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p7_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p7_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p8_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static void     p8_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page );
static gboolean p8_recovery_confirmed( ofaRecoveryAssistant *self );
static gboolean p8_do_recover( ofaRecoveryAssistant *self );
//static gboolean p8_do_remediate_settings( ofaRecoveryAssistant *self );
//static gboolean p8_do_open( ofaRecoveryAssistant *self );
static void     p8_msg_cb( const gchar *buffer, ofaRecoveryAssistant *self );
static void     read_settings( ofaRecoveryAssistant *self );
static void     write_settings( ofaRecoveryAssistant *self );
static void     iactionable_iface_init( ofaIActionableInterface *iface );
static guint    iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaRecoveryAssistant, ofa_recovery_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaRecoveryAssistant )
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
		{ ASSIST_PAGE_FORMAT,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_do_display,
				( myIAssistantCb ) p2_do_forward },
		{ ASSIST_PAGE_RECOVER,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_do_display,
				( myIAssistantCb ) p3_do_forward },
		{ ASSIST_PAGE_TARGET,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_do_display,
				( myIAssistantCb ) p4_do_forward },
		{ ASSIST_PAGE_ROOT,
				( myIAssistantCb ) p5_do_init,
				( myIAssistantCb ) p5_do_display,
				NULL },
		{ ASSIST_PAGE_ADMIN,
				( myIAssistantCb ) p6_do_init,
				( myIAssistantCb ) p6_do_display,
				( myIAssistantCb ) p6_do_forward },
		{ ASSIST_PAGE_CONFIRM,
				( myIAssistantCb ) p7_do_init,
				( myIAssistantCb ) p7_do_display,
				NULL },
		{ ASSIST_PAGE_DONE,
				( myIAssistantCb ) p8_do_init,
				( myIAssistantCb ) p8_do_display,
				NULL },
		{ -1 }
};

static void
recovery_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recovery_assistant_finalize";
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECOVERY_ASSISTANT( instance ));

	/* free data members here */
	priv = ofa_recovery_assistant_get_instance_private( OFA_RECOVERY_ASSISTANT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->p1_folder );
	g_free( priv->p1_entries_uri );
	g_free( priv->p1_accounts_uri );
	g_free( priv->p2_format_name );
	g_free( priv->p3_recover_name );
	g_free( priv->p4_dossier_name );
	g_free( priv->p4_exercice_name );
	g_free( priv->p5_dossier_name );
	g_free( priv->p6_account );
	g_free( priv->p6_password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recovery_assistant_parent_class )->finalize( instance );
}

static void
recovery_assistant_dispose( GObject *instance )
{
	ofaRecoveryAssistantPrivate *priv;
	GtkApplicationWindow *main_window;

	g_return_if_fail( instance && OFA_IS_RECOVERY_ASSISTANT( instance ));

	priv = ofa_recovery_assistant_get_instance_private( OFA_RECOVERY_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;
		write_settings( OFA_RECOVERY_ASSISTANT( instance ));

		/* unref object members here */
		g_list_free( priv->p3_recoverers );
		g_clear_object( &priv->p4_dossier_meta );
		g_clear_object( &priv->p4_exercice_meta );
		g_clear_object( &priv->p5_hgroup );
		g_clear_object( &priv->p4_connect );

		if( priv->p6_apply_actions && priv->getter ){
			main_window = ofa_igetter_get_main_window( priv->getter );
			g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
			ofa_main_window_dossier_apply_actions( OFA_MAIN_WINDOW( main_window ));
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recovery_assistant_parent_class )->dispose( instance );
}

static void
ofa_recovery_assistant_init( ofaRecoveryAssistant *self )
{
	static const gchar *thisfn = "ofa_recovery_assistant_init";
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECOVERY_ASSISTANT( self ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->p4_dossier_meta = NULL;
	priv->p4_exercice_meta = NULL;
	priv->p4_provider = NULL;
	priv->p5_dossier_name = NULL;
	priv->p5_provider = NULL;
	priv->p6_apply_actions = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_recovery_assistant_class_init( ofaRecoveryAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_recovery_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recovery_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = recovery_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_recovery_assistant_run:
 * @getter: a #ofaIGetter instance.
 *
 * Run the assistant.
 */
void
ofa_recovery_assistant_run( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_recovery_assistant_run";
	ofaRecoveryAssistant *self;
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_RECOVERY_ASSISTANT, NULL );

	priv = ofa_recovery_assistant_get_instance_private( self );

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
	static const gchar *thisfn = "ofa_recovery_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_recovery_assistant_iwindow_init";
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recovery_assistant_get_instance_private( OFA_RECOVERY_ASSISTANT( instance ));

	my_iwindow_set_parent( instance, GTK_WINDOW( ofa_igetter_get_main_window( priv->getter )));
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );

	read_settings( OFA_RECOVERY_ASSISTANT( instance ));
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_recovery_assistant_iassistant_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_willing_to_quit = iassistant_is_willing_to_quit;
}

static gboolean
iassistant_is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( OFA_RECOVERY_ASSISTANT( instance ));

	return( ofa_prefs_assistant_is_willing_to_quit( priv->getter, keyval ));
}

/*
 * p1: select the source files
 *
 * Initialize the GtkFileChooser widgets with the last used folder
 * we allow only a single selection and no folder creation
 */
static void
p1_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p1_do_init";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *btn;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-entry-chooser" );
	g_return_if_fail( btn && GTK_IS_FILE_CHOOSER_BUTTON( btn ));
	g_signal_connect( btn, "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	priv->p1_entries_chooser = GTK_FILE_CHOOSER( btn );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p1-account-chooser" );
	g_return_if_fail( btn && GTK_IS_FILE_CHOOSER_BUTTON( btn ));
	g_signal_connect( btn, "selection-changed", G_CALLBACK( p1_on_selection_changed ), self );
	priv->p1_accounts_chooser = GTK_FILE_CHOOSER( btn );
}

static void
p1_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p1_do_display";
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	if( priv->p1_folder ){
		gtk_file_chooser_set_current_folder_uri( priv->p1_entries_chooser, priv->p1_folder );
		gtk_file_chooser_set_current_folder_uri( priv->p1_accounts_chooser, priv->p1_folder );
	}

	p1_check_for_complete( self );
}

static void
p1_on_selection_changed( GtkFileChooser *chooser, ofaRecoveryAssistant *self )
{
	p1_check_for_complete( self );
}

static gboolean
p1_check_for_complete( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_recovery_assistant_get_instance_private( self );

	g_free( priv->p1_entries_uri );
	priv->p1_entries_uri = gtk_file_chooser_get_uri( priv->p1_entries_chooser );

	ok = my_strlen( priv->p1_entries_uri ) > 0 &&
			my_utils_uri_is_readable( priv->p1_entries_uri );

	g_free( priv->p1_accounts_uri );
	priv->p1_accounts_uri = gtk_file_chooser_get_uri( priv->p1_accounts_chooser );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p1_do_forward( ofaRecoveryAssistant *self, GtkWidget *page )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* entries are the main, mandatory, file to be recovered
	 * only consider its folder */
	g_free( priv->p1_folder );
	priv->p1_folder = gtk_file_chooser_get_current_folder_uri( priv->p1_entries_chooser );
}

/*
 * p2: configure the input format
 */
static void
p2_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p2_do_init";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *parent, *label;
	GtkSizeGroup *hgroup, *group_bin;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* previously set */
	priv->p2_entries_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-entries" );
	g_return_if_fail( priv->p2_entries_label && GTK_IS_LABEL( priv->p2_entries_label ));
	my_style_add( priv->p2_entries_label, "labelinfo" );

	priv->p2_accounts_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-accounts" );
	g_return_if_fail( priv->p2_accounts_label && GTK_IS_LABEL( priv->p2_accounts_label ));
	my_style_add( priv->p2_accounts_label, "labelinfo" );

	/* horizontal size group */
	hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-label11" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-label12" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( hgroup, label );

	/* input format */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-format-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p2_format_st = ofa_stream_format_new( priv->getter, priv->p2_format_name, OFA_SFMODE_IMPORT );
	priv->p2_format_bin = ofa_stream_format_bin_new( priv->p2_format_st );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_format_bin ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->p2_format_bin ), 0 ))){
		my_utils_size_group_add_size_group( hgroup, group_bin );
	}
	g_signal_connect( priv->p2_format_bin, "my-ibin-changed", G_CALLBACK( p2_on_format_changed ), self );

	/* error message */
	priv->p2_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p2-message" );
	g_return_if_fail( priv->p2_message && GTK_IS_LABEL( priv->p2_message ));
	my_style_add( priv->p2_message, "labelerror" );
}

static void
p2_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p2_do_display";
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p2_entries_label ), priv->p1_entries_uri );
	gtk_label_set_text( GTK_LABEL( priv->p2_accounts_label ), priv->p1_accounts_uri );

	p2_check_for_complete( self );
}

static void
p2_on_format_changed( myIBin *bin, ofaRecoveryAssistant *self )
{
	p2_check_for_complete( self );
}

static gboolean
p2_check_for_complete( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gchar *message;
	gboolean ok;

	priv = ofa_recovery_assistant_get_instance_private( self );

	ok = my_ibin_is_valid( MY_IBIN( priv->p2_format_bin ), &message );

	gtk_label_set_text( GTK_LABEL( priv->p2_message ), my_strlen( message ) ? message : "" );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p2_do_forward( ofaRecoveryAssistant *self, GtkWidget *page )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->p2_format_bin ));

	g_free( priv->p2_format_name );
	priv->p2_format_name = g_strdup( ofa_stream_format_get_name( priv->p2_format_st ));
}

/*
 * p3: select the recoverer
 */
static void
p3_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p3_do_init";
	ofaRecoveryAssistantPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	ofaExtenderCollection *collection;
	GList *it;
	gchar *label, *version;
	GtkTreeIter iter;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* previously set */
	priv->p3_entries_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-entries" );
	g_return_if_fail( priv->p3_entries_label && GTK_IS_LABEL( priv->p3_entries_label ));
	my_style_add( priv->p3_entries_label, "labelinfo" );

	priv->p3_accounts_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-accounts" );
	g_return_if_fail( priv->p3_accounts_label && GTK_IS_LABEL( priv->p3_accounts_label ));
	my_style_add( priv->p3_accounts_label, "labelinfo" );

	priv->p3_format_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-format" );
	g_return_if_fail( priv->p3_format_label && GTK_IS_LABEL( priv->p3_format_label ));
	my_style_add( priv->p3_format_label, "labelinfo" );

	/* available recoverers */
	priv->p3_tview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p3-treeview" );
	g_return_if_fail( priv->p3_tview && GTK_IS_TREE_VIEW( priv->p3_tview ));

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					"Label", cell, "text", REC_COL_LABEL, NULL );
	gtk_tree_view_column_set_alignment( column, 0 );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->p3_tview ), column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					"Version", cell, "text", REC_COL_VERSION, NULL );
	gtk_tree_view_column_set_alignment( column, 0 );
	gtk_tree_view_append_column( GTK_TREE_VIEW( priv->p3_tview ), column );

	priv->p3_store = gtk_list_store_new( REC_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT );
	gtk_tree_view_set_model( GTK_TREE_VIEW( priv->p3_tview ), GTK_TREE_MODEL( priv->p3_store ));
	g_object_unref( priv->p3_store );

	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_tview ));
	g_signal_connect( selection, "changed", G_CALLBACK( p3_on_selection_changed ), self );
	g_signal_connect( priv->p3_tview, "row-activated", G_CALLBACK( p3_on_selection_activated ), self );

	collection = ofa_igetter_get_extender_collection( priv->getter );
	priv->p3_recoverers = ofa_extender_collection_get_for_type( collection, OFA_TYPE_IRECOVER );
	for( it=priv->p3_recoverers ; it ; it=it->next ){
		if( MY_IS_IIDENT( it->data )){
			label = my_iident_get_display_name( MY_IIDENT( it->data ), NULL );
			version = my_iident_get_version( MY_IIDENT( it->data ), NULL );
			if( my_strlen( label )){
				gtk_list_store_insert_with_values( priv->p3_store, &iter, -1,
						REC_COL_LABEL,   label,
						REC_COL_VERSION, version ? version : "",
						REC_COL_OBJECT,  it->data,
						-1 );
			}
			g_free( version );
			g_free( label );
		}
	}
}

static void
p3_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p3_do_display";
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p3_entries_label ), priv->p1_entries_uri );
	gtk_label_set_text( GTK_LABEL( priv->p3_accounts_label ), priv->p1_accounts_uri );
	gtk_label_set_text( GTK_LABEL( priv->p3_format_label ), priv->p2_format_name );

	p3_check_for_complete( self );
}

static void
p3_on_selection_changed( GtkTreeSelection *select, ofaRecoveryAssistant *self )
{
	p3_check_for_complete( self );
}

static void
p3_on_selection_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaRecoveryAssistant *self )
{
	if( p3_check_for_complete( self )){
		gtk_assistant_next_page( GTK_ASSISTANT( self ));
	}
}

static gboolean
p3_check_for_complete( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gboolean ok;
	GtkTreeSelection *selection;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	priv = ofa_recovery_assistant_get_instance_private( self );

	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->p3_tview ));
	ok = gtk_tree_selection_get_selected( selection, &tmodel, &iter );

	if( ok ){
		gtk_tree_model_get( tmodel, &iter, REC_COL_OBJECT, &priv->p3_recoverer, -1 );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

static void
p3_do_forward( ofaRecoveryAssistant *self, GtkWidget *page )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	my_ibin_apply( MY_IBIN( priv->p2_format_bin ));

	g_free( priv->p3_recover_name );
	priv->p3_recover_name = my_iident_get_display_name( MY_IIDENT( priv->p3_recoverer ), NULL );
}

/*
 * p4: target dossier and database
 */
static void
p4_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p4_do_init";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *parent;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* previously set */
	priv->p4_entries_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-entries" );
	g_return_if_fail( priv->p4_entries_label && GTK_IS_LABEL( priv->p4_entries_label ));
	my_style_add( priv->p4_entries_label, "labelinfo" );

	priv->p4_accounts_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-accounts" );
	g_return_if_fail( priv->p4_accounts_label && GTK_IS_LABEL( priv->p4_accounts_label ));
	my_style_add( priv->p4_accounts_label, "labelinfo" );

	priv->p4_format_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-format" );
	g_return_if_fail( priv->p4_format_label && GTK_IS_LABEL( priv->p4_format_label ));
	my_style_add( priv->p4_format_label, "labelinfo" );

	priv->p4_recover_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-recoverer" );
	g_return_if_fail( priv->p4_recover_label && GTK_IS_LABEL( priv->p4_recover_label ));
	my_style_add( priv->p4_recover_label, "labelinfo" );

	/* target dossier and period */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-chooser-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p4_chooser = ofa_target_chooser_bin_new( priv->getter, priv->settings_prefix, HUB_RULE_DOSSIER_RECOVERY );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p4_chooser ));
	g_signal_connect( priv->p4_chooser, "ofa-changed", G_CALLBACK( p4_on_target_chooser_changed ), self );

	/* message */
	priv->p4_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p4-message" );
	g_return_if_fail( priv->p4_message && GTK_IS_LABEL( priv->p4_message ));
	my_style_add( priv->p4_message, "labelerror" );
}

static void
p4_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p4_do_display";
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s), p4_dossier_meta=%p, p4_exercice_meta=%p",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ),
			( void * ) priv->p4_dossier_meta, ( void * ) priv->p4_exercice_meta );

	g_return_if_fail( page && GTK_IS_CONTAINER( page ));

	gtk_label_set_text( GTK_LABEL( priv->p4_entries_label ), priv->p1_entries_uri );
	gtk_label_set_text( GTK_LABEL( priv->p4_accounts_label ), priv->p1_accounts_uri );
	gtk_label_set_text( GTK_LABEL( priv->p4_format_label ), priv->p2_format_name );
	gtk_label_set_text( GTK_LABEL( priv->p4_recover_label ), priv->p3_recover_name );

	ofa_target_chooser_bin_set_selected(
			priv->p4_chooser, priv->p4_dossier_meta, priv->p4_exercice_meta );

	p4_check_for_complete( self );
}

static void
p4_on_target_chooser_changed( ofaTargetChooserBin *bin, ofaIDBDossierMeta *dossier_meta, ofaIDBExerciceMeta *exercice_meta, ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;

	g_debug( "p4_on_target_chooser_changed: dossier=%p, exercice=%p", dossier_meta, exercice_meta );

	priv = ofa_recovery_assistant_get_instance_private( self );

	g_clear_object( &priv->p4_dossier_meta );
	g_clear_object( &priv->p4_exercice_meta );

	if( dossier_meta ){
		priv->p4_dossier_meta = g_object_ref( dossier_meta );
		priv->p4_new_dossier = ofa_target_chooser_bin_is_new_dossier( bin, dossier_meta );
		if( exercice_meta ){
			priv->p4_exercice_meta = g_object_ref( exercice_meta );
			priv->p4_new_exercice = ofa_target_chooser_bin_is_new_exercice( bin, exercice_meta );
		}
	}

	p4_check_for_complete( self );
}

static gboolean
p4_check_for_complete( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gboolean ok;

	priv = ofa_recovery_assistant_get_instance_private( self );

	p4_set_message( self, "" );

	ok = ( priv->p4_dossier_meta && OFA_IS_IDBDOSSIER_META( priv->p4_dossier_meta ) &&
			priv->p4_exercice_meta && OFA_IS_IDBEXERCICE_META( priv->p4_exercice_meta ) &&
			p4_check_for_recovery_rules( self ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );

	return( ok );
}

/*
 * Check that reco uri is compatible with selected dossier/exercice.
 *
 * Note: what we mean by "new dossier/new exercice" actually means "empty".
 * But we do not have at this time the needed credentials to see if the
 * dossier/exercice are actually empty or does contain something.
 *
 * So we are tied to only check if they have been just created in *this*
 * #ofaTargetChooserBin instance.
 */
static gboolean
p4_check_for_recovery_rules( ofaRecoveryAssistant *self )
{
	return( TRUE );
}

static void
p4_set_message( ofaRecoveryAssistant *self, const gchar *message )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	if( priv->p4_message ){
		gtk_label_set_text( GTK_LABEL( priv->p4_message ), message ? message : "" );
	}
}

static void
p4_do_forward( ofaRecoveryAssistant *self, GtkWidget *page )
{
	ofaRecoveryAssistantPrivate *priv;
	const gchar *dossier_name;

	priv = ofa_recovery_assistant_get_instance_private( self );

	priv->p4_provider = ofa_idbdossier_meta_get_provider( priv->p4_dossier_meta );

	g_clear_object( &priv->p4_connect );
	priv->p4_connect = ofa_idbdossier_meta_new_connect( priv->p4_dossier_meta, NULL );

	dossier_name = ofa_idbdossier_meta_get_dossier_name( priv->p4_dossier_meta );
	priv->p4_dossier_name = g_strdup( dossier_name );

	priv->p4_exercice_name = ofa_idbexercice_meta_get_name( priv->p4_exercice_meta );
}

/*
 * p5: get DBMS root account and password
 */
static void
p5_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p5_do_init";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *label, *parent;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* previously set */
	priv->p5_entries_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-entries" );
	g_return_if_fail( priv->p5_entries_label && GTK_IS_LABEL( priv->p5_entries_label ));
	my_style_add( priv->p5_entries_label, "labelinfo" );

	priv->p5_accounts_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-accounts" );
	g_return_if_fail( priv->p5_accounts_label && GTK_IS_LABEL( priv->p5_accounts_label ));
	my_style_add( priv->p5_accounts_label, "labelinfo" );

	priv->p5_format_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-format" );
	g_return_if_fail( priv->p5_format_label && GTK_IS_LABEL( priv->p5_format_label ));
	my_style_add( priv->p5_format_label, "labelinfo" );

	priv->p5_recover_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-recoverer" );
	g_return_if_fail( priv->p5_recover_label && GTK_IS_LABEL( priv->p5_recover_label ));
	my_style_add( priv->p5_recover_label, "labelinfo" );

	priv->p5_dossier_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dossier" );
	g_return_if_fail( priv->p5_dossier_label && GTK_IS_LABEL( priv->p5_dossier_label ));
	my_style_add( priv->p5_dossier_label, "labelinfo" );

	priv->p5_name_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbname" );
	g_return_if_fail( priv->p5_name_label && GTK_IS_LABEL( priv->p5_name_label ));
	my_style_add( priv->p5_name_label, "labelinfo" );

	/* horizontal group size */
	priv->p5_hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-label11" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p5_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-label12" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p5_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-label13" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p5_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-label14" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p5_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-label15" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p5_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-label16" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p5_hgroup, label );

	/* connection informations
	 * the actual UI depends of the selected target => just get the
	 * parent here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-connect-infos" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_connect_parent = parent;

	/* super user interface
	 * the actual UI depends of the provider, which itself depends of
	 * selected dossier meta => just get the parent here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-dbsu-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p5_dbsu_parent = parent;
	priv->p5_dbsu_credentials = NULL;

	/* message */
	priv->p5_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p5-message" );
	g_return_if_fail( priv->p5_message && GTK_IS_LABEL( priv->p5_message ));
	my_style_add( priv->p5_message, "labelerror" );
}

/*
 * Store in p5_dossier_name the name of the dossier for which we have
 * the connection display; this may prevent us to destroy the display
 * without reason.
 *
 * Idem, store in p5_provider the provider for which we have created the
 * super-user credentials widget.
 */
static void
p5_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p5_do_display";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *label, *display;
	GtkSizeGroup *group;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p5_entries_label ), priv->p1_entries_uri );
	gtk_label_set_text( GTK_LABEL( priv->p5_accounts_label ), priv->p1_accounts_uri );
	gtk_label_set_text( GTK_LABEL( priv->p5_format_label ), priv->p2_format_name );
	gtk_label_set_text( GTK_LABEL( priv->p5_recover_label ), priv->p3_recover_name );
	gtk_label_set_text( GTK_LABEL( priv->p5_dossier_label ), priv->p4_dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p5_name_label ), priv->p4_exercice_name );

	/* as the dossier may have changed since the initialization,
	 * the display of connection informations is setup here */
	if( priv->p5_dossier_name && my_collate( priv->p5_dossier_name, priv->p4_dossier_name )){
		gtk_container_foreach( GTK_CONTAINER( priv->p5_connect_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
		g_free( priv->p5_dossier_name );
		priv->p5_dossier_name = NULL;
	}
	if( !priv->p5_dossier_name ){
		display = ofa_idbconnect_get_display( priv->p4_connect, "labelinfo" );
		if( display ){
			gtk_container_add( GTK_CONTAINER( priv->p5_connect_parent ), display );
			if(( group = my_ibin_get_size_group( MY_IBIN( display ), 0 ))){
				my_utils_size_group_add_size_group( priv->p5_hgroup, group );
			}
			priv->p5_dossier_name = g_strdup( priv->p4_dossier_name );
		}
	}

	/* setup superuser UI */
	g_debug( "%s: p4_provider=%p, p5_provider=%p", thisfn, priv->p4_provider, priv->p5_provider );
	if( priv->p5_provider && priv->p5_provider != priv->p4_provider ){
		gtk_container_foreach( GTK_CONTAINER( priv->p5_dbsu_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
		priv->p5_provider = NULL;
	}
	if( !priv->p5_provider ){
		priv->p5_dbsu_credentials = ofa_idbprovider_new_superuser_bin( priv->p4_provider, HUB_RULE_DOSSIER_RECOVERY );

		if( priv->p5_dbsu_credentials ){
			gtk_container_add( GTK_CONTAINER( priv->p5_dbsu_parent ), GTK_WIDGET( priv->p5_dbsu_credentials ));
			ofa_idbsuperuser_set_dossier_meta( priv->p5_dbsu_credentials, priv->p4_dossier_meta );
			group = ofa_idbsuperuser_get_size_group( priv->p5_dbsu_credentials, 0 );
			if( group ){
				my_utils_size_group_add_size_group( priv->p5_hgroup, group );
			}
			g_signal_connect( priv->p5_dbsu_credentials, "my-ibin-changed", G_CALLBACK( p5_on_dbsu_credentials_changed ), self );

			/* if SU account is already set */
			ofa_idbsuperuser_set_credentials_from_connect( priv->p5_dbsu_credentials, priv->p4_connect );

		} else {
			label = gtk_label_new( _(
					"The selected DBMS provider does not need super-user credentials for restore operations.\n"
					"Just press Next to continue." ));
			gtk_label_set_xalign( GTK_LABEL( label ), 0 );
			gtk_label_set_line_wrap( GTK_LABEL( label ), TRUE );
			gtk_label_set_line_wrap_mode( GTK_LABEL( label ), PANGO_WRAP_WORD );
			gtk_container_add( GTK_CONTAINER( priv->p5_dbsu_parent ), label );
		}

		priv->p5_provider = priv->p4_provider;
	}

	/* already triggered by #ofa_idbsuperuser_set_credentials_from_connect()
	 * via p5_on_dbsu_credentials_changed() */
	p5_check_for_complete( self );
}

static void
p5_on_dbsu_credentials_changed( myIBin *bin, ofaRecoveryAssistant *self )
{
	p5_check_for_complete( self );
}

static void
p5_check_for_complete( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_recovery_assistant_get_instance_private( self );
	g_debug( "p5_check_for_complete: p4_dossier_meta=%p", priv->p4_dossier_meta );

	ok = TRUE;
	message = NULL;

	if( ok && priv->p5_dbsu_credentials ){
		ok = ofa_idbsuperuser_is_valid( priv->p5_dbsu_credentials, &message );
	}

	if( ok && priv->p5_dbsu_credentials ){
		ok = ofa_idbconnect_open_with_superuser( priv->p4_connect, priv->p5_dbsu_credentials );
		if( !ok ){
			message = g_strdup_printf( _( "Unable to open a super-user connection on the DBMS" ));
		}
	}

	ofa_idbsuperuser_set_valid( priv->p5_dbsu_credentials, ok );

	p5_set_message( self, message );
	g_free( message );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p5_set_message( ofaRecoveryAssistant *self, const gchar *message )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	if( priv->p5_message ){
		gtk_label_set_text( GTK_LABEL( priv->p5_message ), message ? message : "" );
	}
}

/*
 * p6: get dossier administrative account and password
 */
static void
p6_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p6_do_init";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *parent, *label;
	GtkSizeGroup *group_bin;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* previously set */
	priv->p6_entries_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-entries" );
	g_return_if_fail( priv->p6_entries_label && GTK_IS_LABEL( priv->p6_entries_label ));
	my_style_add( priv->p6_entries_label, "labelinfo" );

	priv->p6_accounts_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-accounts" );
	g_return_if_fail( priv->p6_accounts_label && GTK_IS_LABEL( priv->p6_accounts_label ));
	my_style_add( priv->p6_accounts_label, "labelinfo" );

	priv->p6_format_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-format" );
	g_return_if_fail( priv->p6_format_label && GTK_IS_LABEL( priv->p6_format_label ));
	my_style_add( priv->p6_format_label, "labelinfo" );

	priv->p6_recover_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-recoverer" );
	g_return_if_fail( priv->p6_recover_label && GTK_IS_LABEL( priv->p6_recover_label ));
	my_style_add( priv->p6_recover_label, "labelinfo" );

	priv->p6_dossier_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-dossier" );
	g_return_if_fail( priv->p6_dossier_label && GTK_IS_LABEL( priv->p6_dossier_label ));
	my_style_add( priv->p6_dossier_label, "labelinfo" );

	priv->p6_name_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-dbname" );
	g_return_if_fail( priv->p6_name_label && GTK_IS_LABEL( priv->p6_name_label ));
	my_style_add( priv->p6_name_label, "labelinfo" );

	/* horizontal group size */
	priv->p6_hgroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label11" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p6_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label12" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p6_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label13" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p6_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label14" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p6_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label15" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p6_hgroup, label );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-label16" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_size_group_add_widget( priv->p6_hgroup, label );

	/* connection informations
	 * the actual UI depends of the selected target => just get the
	 * parent here */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-connect-infos" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p6_connect_parent = parent;

	/* admin credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-admin-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p6_admin_credentials = ofa_admin_credentials_bin_new( priv->getter, HUB_RULE_DOSSIER_RECOVERY );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p6_admin_credentials ));
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->p6_admin_credentials ), 0 ))){
		my_utils_size_group_add_size_group( priv->p6_hgroup, group_bin );
	}
	g_signal_connect( priv->p6_admin_credentials,
			"my-ibin-changed", G_CALLBACK( p6_on_admin_credentials_changed ), self );

	/* open, and action on open */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-actions" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p6_actions = ofa_dossier_actions_bin_new( priv->getter, priv->settings_prefix, HUB_RULE_DOSSIER_RECOVERY );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p6_actions ));
	g_signal_connect( priv->p6_actions, "my-ibin-changed", G_CALLBACK( p6_on_actions_changed ), self );

	priv->p6_message = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p6-message" );
	g_return_if_fail( priv->p6_message && GTK_IS_LABEL( priv->p6_message ));
	my_style_add( priv->p6_message, "labelerror" );
}

/*
 * ask the user to confirm the operation
 */
static void
p6_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p6_do_display";
	ofaRecoveryAssistantPrivate *priv;
	GtkWidget *display;
	GtkSizeGroup *group;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p6_entries_label ), priv->p1_entries_uri );
	gtk_label_set_text( GTK_LABEL( priv->p6_accounts_label ), priv->p1_accounts_uri );
	gtk_label_set_text( GTK_LABEL( priv->p6_format_label ), priv->p2_format_name );
	gtk_label_set_text( GTK_LABEL( priv->p6_recover_label ), priv->p3_recover_name );
	gtk_label_set_text( GTK_LABEL( priv->p6_dossier_label ), priv->p4_dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p6_name_label ), priv->p4_exercice_name );

	/* connection informations */
	gtk_container_foreach( GTK_CONTAINER( priv->p6_connect_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
	display = ofa_idbconnect_get_display( priv->p4_connect, "labelinfo" );
	if( display ){
		gtk_container_add( GTK_CONTAINER( priv->p6_connect_parent ), display );
		if(( group = my_ibin_get_size_group( MY_IBIN( display ), 0 ))){
			my_utils_size_group_add_size_group( priv->p6_hgroup, group );
		}
	}

	p6_check_for_complete( self );
}

static void
p6_on_admin_credentials_changed( myIBin *bin, ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	g_free( priv->p6_account );
	g_free( priv->p6_password );

	ofa_admin_credentials_bin_get_credentials(
			OFA_ADMIN_CREDENTIALS_BIN( bin ), &priv->p6_account, &priv->p6_password );

	p6_check_for_complete( self );
}

static void
p6_on_actions_changed( myIBin *bin, ofaRecoveryAssistant *self )
{
	/* nothing to do here */
}

static void
p6_check_for_complete( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_recovery_assistant_get_instance_private( self );

	p6_set_message( self, "" );

	ok = my_ibin_is_valid( MY_IBIN( priv->p6_admin_credentials ), &message ) &&
			my_ibin_is_valid( MY_IBIN( priv->p6_actions ), &message );

	if( !ok ){
		p6_set_message( self, message );
		g_free( message );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p6_set_message( ofaRecoveryAssistant *self, const gchar *message )
{
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	if( priv->p6_message ){
		gtk_label_set_text( GTK_LABEL( priv->p6_message ), message );
	}
}

static void
p6_do_forward( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p6_do_forward";
	/*ofaRecoveryAssistantPrivate *priv;*/

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
}

/*
 * confirmation page
 */
static void
p7_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p7_do_init";
	ofaRecoveryAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_RECOVERY_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	/* previously set */
	priv->p7_entries_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-entries" );
	g_return_if_fail( priv->p7_entries_label && GTK_IS_LABEL( priv->p7_entries_label ));
	my_style_add( priv->p7_entries_label, "labelinfo" );

	priv->p7_accounts_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-accounts" );
	g_return_if_fail( priv->p7_accounts_label && GTK_IS_LABEL( priv->p7_accounts_label ));
	my_style_add( priv->p7_accounts_label, "labelinfo" );

	priv->p7_format_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-format" );
	g_return_if_fail( priv->p7_format_label && GTK_IS_LABEL( priv->p7_format_label ));
	my_style_add( priv->p7_format_label, "labelinfo" );

	priv->p7_recover_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-recoverer" );
	g_return_if_fail( priv->p7_recover_label && GTK_IS_LABEL( priv->p7_recover_label ));
	my_style_add( priv->p7_recover_label, "labelinfo" );

	priv->p7_dossier_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-dossier" );
	g_return_if_fail( priv->p7_dossier_label && GTK_IS_LABEL( priv->p7_dossier_label ));
	my_style_add( priv->p7_dossier_label, "labelinfo" );

	priv->p7_name_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-dbname" );
	g_return_if_fail( priv->p7_name_label && GTK_IS_LABEL( priv->p7_name_label ));
	my_style_add( priv->p7_name_label, "labelinfo" );

	priv->p7_su_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-su-account" );
	g_return_if_fail( priv->p7_su_account && GTK_IS_LABEL( priv->p7_su_account ));
	my_style_add( priv->p7_su_account, "labelinfo" );

	priv->p7_su_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-su-password" );
	g_return_if_fail( priv->p7_su_password && GTK_IS_LABEL( priv->p7_su_password ));
	my_style_add( priv->p7_su_password, "labelinfo" );

	priv->p7_admin_account = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-adm-account" );
	g_return_if_fail( priv->p7_admin_account && GTK_IS_LABEL( priv->p7_admin_account ));
	my_style_add( priv->p7_admin_account, "labelinfo" );

	priv->p7_admin_password = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-adm-password" );
	g_return_if_fail( priv->p7_admin_password && GTK_IS_LABEL( priv->p7_admin_password ));
	my_style_add( priv->p7_admin_password, "labelinfo" );

	priv->p7_open_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-open-label" );
	g_return_if_fail( priv->p7_open_label && GTK_IS_LABEL( priv->p7_open_label ));
	my_style_add( priv->p7_open_label, "labelinfo" );

	priv->p7_apply_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p7-apply-label" );
	g_return_if_fail( priv->p7_apply_label && GTK_IS_LABEL( priv->p7_apply_label ));
	my_style_add( priv->p7_apply_label, "labelinfo" );
}

static void
p7_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p7_do_display";
	ofaRecoveryAssistantPrivate *priv;
	const gchar *cstr;

	g_return_if_fail( OFA_IS_RECOVERY_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->p7_entries_label ), priv->p1_entries_uri );
	gtk_label_set_text( GTK_LABEL( priv->p7_accounts_label ), priv->p1_accounts_uri );
	gtk_label_set_text( GTK_LABEL( priv->p7_format_label ), priv->p2_format_name );
	gtk_label_set_text( GTK_LABEL( priv->p7_recover_label ), priv->p3_recover_name );
	gtk_label_set_text( GTK_LABEL( priv->p7_dossier_label ), priv->p4_dossier_name );
	gtk_label_set_text( GTK_LABEL( priv->p7_name_label ), priv->p4_exercice_name );

	if( priv->p5_dbsu_credentials ){
		cstr = ofa_idbconnect_get_account( priv->p4_connect );
		gtk_label_set_text( GTK_LABEL( priv->p7_su_account ), cstr );
		cstr = ofa_idbconnect_get_password( priv->p4_connect );
		gtk_label_set_text( GTK_LABEL( priv->p7_su_password ), my_strlen( cstr ) ? "******" : "" );
	} else {
		gtk_label_set_text( GTK_LABEL( priv->p7_su_account ), "(unset)" );
		gtk_label_set_text( GTK_LABEL( priv->p7_su_password ), "" );
	}

	gtk_label_set_text( GTK_LABEL( priv->p7_admin_account ), priv->p6_account );
	gtk_label_set_text( GTK_LABEL( priv->p7_admin_password ), "******" );

	priv->p7_open = ofa_dossier_actions_bin_get_open( priv->p6_actions );
	gtk_label_set_text( GTK_LABEL( priv->p7_open_label ), priv->p7_open ? "True":"False" );
	priv->p7_apply = ofa_dossier_actions_bin_get_apply( priv->p6_actions );
	gtk_label_set_text( GTK_LABEL( priv->p7_apply_label ), priv->p7_apply ? "True":"False" );
}

/*
 * p8: execution and execution summary
 */
static void
p8_do_init( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p8_do_init";
	ofaRecoveryAssistantPrivate *priv;

	g_return_if_fail( OFA_IS_RECOVERY_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );

	priv->p8_page = page;

	priv->p8_textview = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p8-textview" );
	g_return_if_fail( priv->p8_textview && GTK_IS_TEXT_VIEW( priv->p8_textview ));

	priv->p8_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page ), "p8-label" );
	g_return_if_fail( priv->p8_label && GTK_IS_LABEL( priv->p8_label ));

	/* keep a ref on target dossier/exercice as the current selection
	 * will be reset during restore */
	priv->p8_dossier_meta = priv->p4_dossier_meta;
	priv->p8_exercice_meta = priv->p4_exercice_meta;

	/* it is time now for registering input format in user settings */
	my_ibin_apply( MY_IBIN( priv->p2_format_bin ));
}

static void
p8_do_display( ofaRecoveryAssistant *self, gint page_num, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p8_do_display";
	ofaRecoveryAssistantPrivate *priv;
	ofaDossierCollection *collection;

	g_return_if_fail( OFA_IS_RECOVERY_ASSISTANT( self ));

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page, G_OBJECT_TYPE_NAME( page ));

	priv = ofa_recovery_assistant_get_instance_private( self );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	if( !p8_recovery_confirmed( self )){

		if( priv->p4_new_dossier ){
			collection = ofa_igetter_get_dossier_collection( priv->getter );
			ofa_dossier_collection_delete_period( collection, priv->p4_connect, NULL, TRUE, NULL );

		} else if( priv->p4_new_exercice ){
			collection = ofa_igetter_get_dossier_collection( priv->getter );
			ofa_dossier_collection_delete_period( collection, priv->p4_connect, priv->p4_exercice_meta, TRUE, NULL );
		}

		gtk_label_set_text(
				GTK_LABEL( priv->p8_label ),
				_( "The restore operation has been cancelled by the user." ));

	} else {
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );
		g_idle_add(( GSourceFunc ) p8_do_recover, self );
	}
}

static gboolean
p8_recovery_confirmed( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	gboolean ok;
	gchar *str;
	GtkWindow *toplevel;

	priv = ofa_recovery_assistant_get_instance_private( self );

	str = g_strdup_printf(
				_( "The recovery operation will drop, fully reset and repopulate "
					"the '%s' database.\n"
					"This may not be what you actually want !\n"
					"Are you sure you want to recover into this database ?" ), priv->p4_exercice_name );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));
	ok = my_utils_dialog_question( toplevel, str, _( "_Recover" ));

	g_free( str );

	return( ok );
}

/*
 * Recover the source files into the target dossier
 * - create the target
 * - populate it
 * simultaneously installing administrative credentials
 */
static gboolean
p8_do_recover( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	ofaIDBConnect *connect;
	GList *uris;
	gboolean ok;
	gchar *msg;
	const gchar *style;
	GtkWidget *dlg;

	priv = ofa_recovery_assistant_get_instance_private( self );

	ok = TRUE;
	msg = NULL;

	ofa_target_chooser_bin_disconnect_handlers( priv->p4_chooser );

	connect = ofa_idbdossier_meta_new_connect( priv->p4_dossier_meta, NULL );
	if( !ofa_idbconnect_open_with_superuser( connect, priv->p5_dbsu_credentials )){
		ok = FALSE;
		style = "labelerror";
		msg = g_strdup( _( "Unable to open a super-user connection to the target." ));

	} else if( !ofa_idbconnect_new_period( connect, priv->p4_exercice_meta, priv->p6_account, priv->p6_password, &msg )){
		ok = FALSE;
		style = "labelerror";

	} else {
		uris = ofa_irecover_add_file( NULL, OFA_RECOVER_ENTRY, priv->p1_entries_uri );
		uris = ofa_irecover_add_file( uris, OFA_RECOVER_ACCOUNT, priv->p1_accounts_uri );

		ok = ofa_irecover_import_uris( priv->p3_recoverer,
					priv->getter, uris, priv->p2_format_st, connect, ( ofaMsgCb ) p8_msg_cb, self );
	}

	if( ok ){
		style = "labelinfo";
		msg = g_strdup_printf(
				_( "The specified URIs have been successfully recovered "
					"into the '%s' dossier." ), priv->p4_dossier_name );
	}

	dlg = gtk_message_dialog_new(
				GTK_WINDOW( self ),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				"%s", msg );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( dlg );

	gtk_label_set_text( GTK_LABEL( priv->p8_label ), msg );
	my_style_add( priv->p8_label, style );

	g_free( msg );

	if( ok ){
		//g_idle_add(( GSourceFunc ) p8_do_open, self );
	//} else {
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
	}

	return( G_SOURCE_REMOVE );
}

#if 0
/*
 * Remediate the database and the settings before officialy opening the
 * dossier.
 *
 * Returns: %TRUE if ok,
 * or %FALSE if the exercice happens to be not compatible with the restored uri.
 */
static gboolean
p8_do_remediate_settings( ofaRecoveryAssistant *self )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p8_do_remediate_settings";
	ofaRecoveryAssistantPrivate *priv;
	ofaIDBConnect *connect;
	ofoExemeta *exemeta;
	const GDate *dos_begin, *dos_end, *meta_begin, *meta_end;
	gboolean ok, dos_current, meta_current, settings_updated;
	ofaIDBExerciceMeta *period;
	gchar *label, *str, *sbegin, *send;

	priv = ofa_recovery_assistant_get_instance_private( self );

	exemeta = NULL;
	settings_updated = FALSE;
	connect = ofa_idbdossier_meta_new_connect( priv->p4_dossier_meta, priv->p4_exercice_meta );
	ok = ofa_idbconnect_open_with_superuser( connect, priv->p5_dbsu_credentials );

	if( ok ){
		exemeta = ofo_exemeta_new( connect );
		ok = ( exemeta && OFO_IS_EXEMETA( exemeta ));
	}
	if( ok ){
		dos_begin = ofo_exemeta_get_begin_date( exemeta );
		dos_end = ofo_exemeta_get_end_date( exemeta );
		dos_current = ofo_exemeta_get_current( exemeta );

		if( 1 ){
			sbegin = my_date_to_str( dos_begin, MY_DATE_SQL );
			send = my_date_to_str( dos_end, MY_DATE_SQL );
			g_debug( "%s: dossier begin=%s, end=%s, current=%s", thisfn, sbegin, send, dos_current ? "True":"False" );
			g_free( sbegin );
			g_free( send );
		}
	}

	/* check that the restored datas do not overlap with another exercice */
	if( ok && my_date_is_valid( dos_begin )){
		period = ofa_idbdossier_meta_get_period( priv->p4_dossier_meta, dos_begin, TRUE );
		if( period != priv->p4_exercice_meta ){
			label = ofa_idbexercice_meta_get_label( period );
			str = g_strdup_printf( _( "Error: the restored file overrides the '%s' exercice" ), label );
			p8_msg_cb( str, self );
			g_free( str );
			g_free( label );
			ok = FALSE;
		}
	}
	if( ok && my_date_is_valid( dos_end )){
		period = ofa_idbdossier_meta_get_period( priv->p4_dossier_meta, dos_end, TRUE );
		if( period != priv->p4_exercice_meta ){
			label = ofa_idbexercice_meta_get_label( period );
			str = g_strdup_printf( _( "Error: the restored file overrides the '%s' exercice" ), label );
			p8_msg_cb( str, self );
			g_free( str );
			g_free( label );
			ok = FALSE;
		}
	}

	/* these are the settings */
	meta_begin = ofa_idbexercice_meta_get_begin_date( priv->p4_exercice_meta );
	meta_end = ofa_idbexercice_meta_get_end_date( priv->p4_exercice_meta );
	meta_current = ofa_idbexercice_meta_get_current( priv->p4_exercice_meta );

	if( 1 ){
		sbegin = my_date_to_str( meta_begin, MY_DATE_SQL );
		send = my_date_to_str( meta_end, MY_DATE_SQL );
		g_debug( "%s: settings begin=%s, end=%s, current=%s", thisfn, sbegin, send, meta_current ? "True":"False" );
		g_free( sbegin );
		g_free( send );
	}

	/* set the settings date if not already done */
	if( ok && !my_date_is_valid( meta_begin ) && my_date_is_valid( dos_begin )){
		g_debug( "%s: remediating settings begin", thisfn );
		ofa_idbexercice_meta_set_begin_date( priv->p4_exercice_meta, dos_begin );
		meta_begin = ofa_idbexercice_meta_get_begin_date( priv->p4_exercice_meta );
		settings_updated = TRUE;
	}
	if( ok && !my_date_is_valid( meta_end ) && my_date_is_valid( dos_end )){
		g_debug( "%s: remediating settings end", thisfn );
		ofa_idbexercice_meta_set_end_date( priv->p4_exercice_meta, dos_end );
		meta_end = ofa_idbexercice_meta_get_end_date( priv->p4_exercice_meta );
		settings_updated = TRUE;
	}

	/* status is re-tagged to restored content as long as we keep only
	 * one current exercice */
	if( ok ){
		if( priv->p1_format == OFA_BACKUP_HEADER_GZ ){
			if( dos_current && !meta_current ){
				period = ofa_idbdossier_meta_get_current_period( priv->p4_dossier_meta );
				if( period != NULL ){
					label = ofa_idbexercice_meta_get_label( period );
					str = g_strdup_printf( _( "Error: the restored file overrides the %s exercice" ), label );
					p8_msg_cb( str, self );
					g_free( str );
					g_free( label );
					ok = FALSE;
				} else {
					g_debug( "%s: remediating settings current", thisfn );
					ofa_idbexercice_meta_set_current( priv->p4_exercice_meta, dos_current );
					meta_current = ofa_idbexercice_meta_get_current( priv->p4_exercice_meta );
					settings_updated = TRUE;
				}
			}
		}
	}
	if( ok ){
		if( priv->p1_format == OFA_BACKUP_HEADER_ZIP ){
			if( dos_current != meta_current ){
				g_debug( "%s: remediating settings current", thisfn );
				ofa_idbexercice_meta_set_current( priv->p4_exercice_meta, dos_current );
				meta_current = ofa_idbexercice_meta_get_current( priv->p4_exercice_meta );
				settings_updated = TRUE;
			}
		}
	}

	/* last remediate settings */
	if( ok && settings_updated ){
		ofa_idbexercice_meta_update_settings( priv->p4_exercice_meta );
	}

	g_clear_object( &exemeta );
	g_clear_object( &connect );

	return( ok );
}

/*
 * Open the dossier if asked for.
 *
 * Actually, because this assistant is non modal, the dossier is opened
 * before the assistant has quit.
 */
static gboolean
p8_do_open( ofaRecoveryAssistant *self )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p8_do_open";
	ofaRecoveryAssistantPrivate *priv;

	priv = ofa_recovery_assistant_get_instance_private( self );

	g_debug( "%s: self=%p, meta=%p, period=%p, account=%s",
			thisfn, ( void * ) self, ( void * ) priv->p4_dossier_meta, ( void * ) priv->p4_exercice_meta, priv->p6_account );

	if( priv->p7_open ){
		if( !ofa_dossier_open_run_modal(
				priv->getter,
				priv->p8_exercice_meta, priv->p6_account, priv->p6_password, FALSE )){

			priv->p6_apply_actions = FALSE;
		}
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	return( G_SOURCE_REMOVE );
}
#endif

static void
p8_msg_cb( const gchar *buffer, ofaRecoveryAssistant *self )
{
	static const gchar *thisfn = "ofa_recovery_assistant_p8_msg_cb";
	ofaRecoveryAssistantPrivate *priv;
	GtkTextBuffer *textbuf;
	GtkTextIter enditer;
	const gchar *charset;
	gchar *utf8;

	if( 0 ){
		g_debug( "%s: buffer=%p, self=%p", thisfn, ( void * ) buffer, ( void * ) self );
		g_debug( "%s: buffer=%s, self=%p", thisfn, buffer, ( void * ) self );
	}

	priv = ofa_recovery_assistant_get_instance_private( self );

	textbuf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->p8_textview ));
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
			GTK_TEXT_VIEW( priv->p8_textview),
			gtk_text_buffer_get_mark( textbuf, "insert" ),
			0.0, FALSE, 0.0, 0.0 );

	/* let Gtk update the display */
	while( gtk_events_pending()){
		gtk_main_iteration();
	}
}

/*
 * settings is "input_folder(s); format_name(s); "
 */
static void
read_settings( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_recovery_assistant_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
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
		g_free( priv->p2_format_name );
		priv->p2_format_name = g_strdup( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaRecoveryAssistant *self )
{
	ofaRecoveryAssistantPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_recovery_assistant_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = g_strdup_printf( "%s-settings", priv->settings_prefix );

	str = g_strdup_printf( "%s;%s;",
				priv->p1_folder, priv->p2_format_name );

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
	static const gchar *thisfn = "ofa_recovery_assistant_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}
