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

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <math.h>

#include "my/my-date-editable.h"
#include "my/my-iassistant.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-iexe-close.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-iconcil.h"

#include "ui/ofa-application.h"
#include "ui/ofa-balance-grid-bin.h"
#include "ui/ofa-check-balances-bin.h"
#include "ui/ofa-check-integrity-bin.h"
#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-exercice-close-assistant.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
typedef struct {
	gboolean              dispose_has_run;

	/* initialization
	 */
	ofaIGetter           *getter;
	ofaHub               *hub;

	/* dossier
	 */
	ofoDossier           *dossier;
	const ofaIDBConnect  *connect;
	ofaIDBMeta           *meta;
	gchar                *dos_name;

	/* p1 - closing parms
	 */
	GtkWidget            *p1_begin_cur;
	GtkWidget            *p1_end_cur;
	GtkWidget            *p1_begin_next;
	GtkWidget            *p1_end_next;
	ofaClosingParmsBin   *p1_closing_parms;

	/* p2 - get DBMS root credentials
	 */
	ofaDBMSRootBin       *p2_dbms_credentials;
	gchar                *p2_account;
	gchar                *p2_password;
	GtkWidget            *p2_message;

	/* p3 - checking that entries, accounts and ledgers are balanced
	 */
	ofaCheckBalancesBin  *p3_checks_bin;
	gboolean              p3_done;

	/* p4 - check for DBMS integrity
	 */
	ofaCheckIntegrityBin *p4_checks_bin;
	gboolean              p4_done;

	/* p5 - confirmation page
	 */
	GtkWidget            *p5_backup_btn;
	gboolean              p5_backuped;

	/* p6 - close the exercice
	 */
	GtkWidget            *p6_page;
	GList                *p6_forwards;			/* forward operations */
	GList                *p6_cleanup;
	GList                *p6_unreconciliated;
	gboolean              is_destroy_allowed;

	/* plugins for IExeClosexxx interfaces
	 */
	GList                *close_list;
}
	ofaExerciceCloseAssistantPrivate;

/* the pages of this assistant, counted froom zero
 */
enum {
	PAGE_INTRO = 0,						/* Intro */
	PAGE_PARMS,							/* p1 - Content: get closing parms */
	PAGE_DBMS,							/* p2 - Content: get dbms root credentials */
	PAGE_CHECKS,						/* p3 - Progress: check balances */
	PAGE_CHECK_DBMS,					/* p4 - Progress: check dbms integrity */
	PAGE_CONFIRM,						/* p5 - Confirm */
	PAGE_CLOSE,							/* p6 - Progress then Summary */
};

/* a structure attached to each ofaIExeClosexxx instance which has
 * showed its will to do some task
 */
typedef struct {
	GtkWidget *box;
}
	sClose;

#define EXECLOSE_CLOSING_DATA           "execlose-closing-data"
#define EXECLOSE_OPENING_DATA           "execlose-opening-data"

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-exercice-close-assistant.ui";
static const gchar *st_settings         = "ofaExerciceCloseAssistant";

static void           iwindow_iface_init( myIWindowInterface *iface );
static void           iwindow_init( myIWindow *instance );
static gboolean       iwindow_is_destroy_allowed( const myIWindow *instance );
static void           iassistant_iface_init( myIAssistantInterface *iface );
static gboolean       iassistant_is_willing_to_quit( myIAssistant*instance, guint keyval );
static void           p0_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p1_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p1_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p1_on_date_changed( GtkEditable *editable, ofaExerciceCloseAssistant *self );
static void           p1_on_closing_parms_changed( ofaClosingParmsBin *bin, ofaExerciceCloseAssistant *self );
static void           p1_check_for_complete( ofaExerciceCloseAssistant *self );
static void           p1_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p2_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p2_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p2_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaExerciceCloseAssistant *self );
static void           p2_check_for_complete( ofaExerciceCloseAssistant *self );
static void           p2_set_message( ofaExerciceCloseAssistant *self, const gchar *message );
static void           p2_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p3_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p3_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p3_on_checks_done( ofaCheckBalancesBin *bin, gboolean ok, ofaExerciceCloseAssistant *self );
static void           p4_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p4_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p4_on_checks_done( ofaCheckIntegrityBin *bin, gulong errors, ofaExerciceCloseAssistant *self );
static void           p5_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p5_do_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p5_on_backup_clicked( GtkButton *button, ofaExerciceCloseAssistant *self );
static void           p5_check_for_complete( ofaExerciceCloseAssistant *self );
static void           p6_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void           p6_init_plugin( ofaExerciceCloseAssistant *self, GtkWidget *grid, ofaIExeClose *instance, guint type, const gchar *data_name, GtkWidget *sibling, GWeakNotify fn );
static void           p6_do_close( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static gboolean       p6_closing_plugin( ofaExerciceCloseAssistant *self );
static gboolean       p6_validate_entries( ofaExerciceCloseAssistant *self );
static gboolean       p6_solde_accounts( ofaExerciceCloseAssistant *self );
static gint           p6_do_solde_accounts( ofaExerciceCloseAssistant *self, gboolean with_ui );
static void           p6_set_forward_settlement_number( GList *entries, const gchar *account, ofxCounter counter );
static gboolean       p6_close_ledgers( ofaExerciceCloseAssistant *self );
static gboolean       p6_archive_exercice( ofaExerciceCloseAssistant *self );
static gboolean       p6_do_archive_exercice( ofaExerciceCloseAssistant *self, gboolean with_ui );
static gboolean       p6_cleanup( ofaExerciceCloseAssistant *self );
static gboolean       p6_forward( ofaExerciceCloseAssistant *self );
static gboolean       p6_open( ofaExerciceCloseAssistant *self );
static gboolean       p6_future( ofaExerciceCloseAssistant *self );
static gboolean       p6_opening_plugin( ofaExerciceCloseAssistant *self );
static myProgressBar *get_new_bar( ofaExerciceCloseAssistant *self, const gchar *w_name );
static void           update_bar( myProgressBar *bar, guint *count, guint total, const gchar *emitter_name );
static void           on_closing_instance_finalized( sClose *close_data, GObject *finalized_instance );
static void           on_opening_instance_finalized( sClose *close_data, GObject *finalized_instance );

G_DEFINE_TYPE_EXTENDED( ofaExerciceCloseAssistant, ofa_exercice_close_assistant, GTK_TYPE_ASSISTANT, 0,
		G_ADD_PRIVATE( ofaExerciceCloseAssistant )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IASSISTANT, iassistant_iface_init ))

static const ofsIAssistant st_pages_cb [] = {
		{ PAGE_INTRO,
				NULL,
				NULL,
				( myIAssistantCb ) p0_do_forward },
		{ PAGE_PARMS,
				( myIAssistantCb ) p1_do_init,
				( myIAssistantCb ) p1_display,
				( myIAssistantCb ) p1_do_forward },
		{ PAGE_DBMS,
				( myIAssistantCb ) p2_do_init,
				( myIAssistantCb ) p2_display,
				( myIAssistantCb ) p2_do_forward },
		{ PAGE_CHECKS,
				( myIAssistantCb ) p3_do_init,
				( myIAssistantCb ) p3_checks,
				NULL },
		{ PAGE_CHECK_DBMS,
				( myIAssistantCb ) p4_do_init,
				( myIAssistantCb ) p4_checks,
				NULL },
		{ PAGE_CONFIRM,
				( myIAssistantCb ) p5_do_init,
				( myIAssistantCb ) p5_do_display,
				NULL },
		{ PAGE_CLOSE,
				( myIAssistantCb ) p6_do_init,
				( myIAssistantCb ) p6_do_close,
				NULL },
		{ -1 }
};

static void
exercice_close_assistant_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_finalize";
	ofaExerciceCloseAssistantPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_CLOSE_ASSISTANT( instance ));

	/* free data members here */
	priv = ofa_exercice_close_assistant_get_instance_private( OFA_EXERCICE_CLOSE_ASSISTANT( instance ));

	g_free( priv->dos_name );
	g_free( priv->p2_account );
	g_free( priv->p2_password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_close_assistant_parent_class )->finalize( instance );
}

static void
exercice_close_assistant_dispose( GObject *instance )
{
	ofaExerciceCloseAssistantPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_CLOSE_ASSISTANT( instance ));

	priv = ofa_exercice_close_assistant_get_instance_private( OFA_EXERCICE_CLOSE_ASSISTANT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->meta );
		ofa_extender_collection_free_types( priv->close_list );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_close_assistant_parent_class )->dispose( instance );
}

static void
ofa_exercice_close_assistant_init( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_init";
	ofaExerciceCloseAssistantPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_CLOSE_ASSISTANT( self ));

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	priv->is_destroy_allowed = TRUE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_exercice_close_assistant_class_init( ofaExerciceCloseAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_close_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_close_assistant_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_exercice_close_assistant_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run an intermediate closing on selected ledgers
 */
void
ofa_exercice_close_assistant_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_run";
	ofaExerciceCloseAssistant *self;
	ofaExerciceCloseAssistantPrivate *priv;;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_exercice_close_assistant_get_instance_private( self );

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
	static const gchar *thisfn = "ofa_exercice_close_assistant_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
	iface->is_destroy_allowed = iwindow_is_destroy_allowed;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_iwindow_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	my_iassistant_set_callbacks( MY_IASSISTANT( instance ), st_pages_cb );
}

static gboolean
iwindow_is_destroy_allowed( const myIWindow *instance )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_iwindow_is_destroy_allowed";
	ofaExerciceCloseAssistantPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_exercice_close_assistant_get_instance_private( OFA_EXERCICE_CLOSE_ASSISTANT( instance ));

	return( priv->is_destroy_allowed );
}

/*
 * myIAssistant interface management
 */
static void
iassistant_iface_init( myIAssistantInterface *iface )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_iassistant_iface_init";

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
p0_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p0_do_forward";
	ofaExerciceCloseAssistantPrivate *priv;
	ofaExtenderCollection *extenders;

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->connect = ofa_hub_get_connect( priv->hub );
	priv->meta = ofa_idbconnect_get_meta( priv->connect );
	priv->dos_name = ofa_idbmeta_get_dossier_name( priv->meta );

	priv->dossier = ofa_hub_get_dossier( priv->hub );

	extenders = ofa_hub_get_extender_collection( priv->hub );
	priv->close_list = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IEXECLOSE );
}

static void
p1_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *parent, *label, *prompt;
	const GDate *begin_cur, *end_cur;
	GDate begin, end;
	gint exe_length;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	exe_length = ofo_dossier_get_exe_length( priv->dossier );

	/* closing exercice - beginning date */
	priv->p1_begin_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-begin-entry" );
	g_return_if_fail( priv->p1_begin_cur && GTK_IS_ENTRY( priv->p1_begin_cur ));

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-begin-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), priv->p1_begin_cur );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-begin-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->p1_begin_cur ));
	my_date_editable_set_format( GTK_EDITABLE( priv->p1_begin_cur ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->p1_begin_cur ), label, ofa_prefs_date_check());
	my_date_editable_set_mandatory( GTK_EDITABLE( priv->p1_begin_cur ), TRUE );
	begin_cur = ofo_dossier_get_exe_begin( priv->dossier );
	my_date_editable_set_date( GTK_EDITABLE( priv->p1_begin_cur ), begin_cur );

	g_signal_connect( priv->p1_begin_cur, "changed", G_CALLBACK( p1_on_date_changed ), self );

	/* closing exercice - ending date */
	priv->p1_end_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-end-entry" );
	g_return_if_fail( priv->p1_end_cur && GTK_IS_ENTRY( priv->p1_end_cur ));

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-end-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), priv->p1_end_cur );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-end-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->p1_end_cur ));
	my_date_editable_set_format( GTK_EDITABLE( priv->p1_end_cur ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->p1_end_cur ), label, ofa_prefs_date_check());
	my_date_editable_set_mandatory( GTK_EDITABLE( priv->p1_end_cur ), TRUE );
	end_cur = ofo_dossier_get_exe_end( priv->dossier );
	my_date_editable_set_date( GTK_EDITABLE( priv->p1_end_cur ), end_cur );

	g_signal_connect( priv->p1_end_cur, "changed", G_CALLBACK( p1_on_date_changed ), self );

	/* set a date if the other is valid */
	if( !my_date_is_valid( begin_cur ) && my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &begin, end_cur );
		g_date_subtract_months( &begin, exe_length );
		g_date_add_days( &begin, 1 );
		my_date_editable_set_date( GTK_EDITABLE( priv->p1_begin_cur ), &begin );
		my_date_set_from_date( &end, end_cur );

	} else if( my_date_is_valid( begin_cur ) && !my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &end, begin_cur );
		g_date_add_months( &end, exe_length );
		g_date_subtract_days( &end, 1 );
		my_date_editable_set_date( GTK_EDITABLE( priv->p1_end_cur ), &end );

	} else if( my_date_is_valid( end_cur )){
		my_date_set_from_date( &end, end_cur );
	}

	/* next exercice - beginning date */
	priv->p1_begin_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-begin-entry" );
	g_return_if_fail( priv->p1_begin_next && GTK_IS_ENTRY( priv->p1_begin_next ));

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-begin-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), priv->p1_begin_next );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-begin-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->p1_begin_next ));
	my_date_editable_set_format( GTK_EDITABLE( priv->p1_begin_next ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->p1_begin_next ), label, ofa_prefs_date_check());
	my_date_editable_set_mandatory( GTK_EDITABLE( priv->p1_begin_next ), TRUE );

	g_signal_connect( priv->p1_begin_next, "changed", G_CALLBACK( p1_on_date_changed ), self );

	if( my_date_is_valid( &end )){
		my_date_set_from_date( &begin, &end );
		g_date_add_days( &begin, 1 );
		my_date_editable_set_date( GTK_EDITABLE( priv->p1_begin_next ), &begin );
	}

	/* next exercice - ending date */
	priv->p1_end_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-end-entry" );
	g_return_if_fail( priv->p1_end_next && GTK_IS_ENTRY( priv->p1_end_next ));

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-end-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), priv->p1_end_next );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-end-check" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->p1_end_next ));
	my_date_editable_set_format( GTK_EDITABLE( priv->p1_end_next ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->p1_end_next ), label, ofa_prefs_date_check());
	my_date_editable_set_mandatory( GTK_EDITABLE( priv->p1_end_next ), TRUE );

	g_signal_connect( G_OBJECT( priv->p1_end_next ), "changed", G_CALLBACK( p1_on_date_changed ), self );

	if( my_date_is_valid( &end ) && exe_length > 0 ){
		g_date_add_months( &end, exe_length );
		my_date_editable_set_date( GTK_EDITABLE( priv->p1_end_next ), &end );
	}

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p1_closing_parms = ofa_closing_parms_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p1_closing_parms ));
	g_signal_connect( priv->p1_closing_parms, "ofa-changed", G_CALLBACK( p1_on_closing_parms_changed ), self );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );
}

/*
 * check if the page is validable
 */
static void
p1_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	p1_check_for_complete( self );
}

/*
 * try to set some default values
 */
static void
p1_on_date_changed( GtkEditable *editable, ofaExerciceCloseAssistant *self )
{
	p1_check_for_complete( self );
}

static void
p1_on_closing_parms_changed( ofaClosingParmsBin *bin, ofaExerciceCloseAssistant *self )
{
	p1_check_for_complete( self );
}

static void
p1_check_for_complete( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	gboolean complete;
	const GDate *begin_cur, *end_cur, *begin_next, *end_next;
	GDate date;
	gchar *msg;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	complete = FALSE;

	if( priv->p1_end_next ){
		begin_cur = my_date_editable_get_date( GTK_EDITABLE( priv->p1_begin_cur ), NULL );
		end_cur = my_date_editable_get_date( GTK_EDITABLE( priv->p1_end_cur ), NULL );
		begin_next = my_date_editable_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );
		end_next = my_date_editable_get_date( GTK_EDITABLE( priv->p1_end_next ), NULL );

		/* check that all dates are valid
		 * and next exercice begins the next day after the end of the
		 * current */
		if( my_date_is_valid( begin_cur ) &&
				my_date_is_valid( end_cur ) &&
				my_date_is_valid( begin_next ) &&
				my_date_is_valid( end_next ) &&
				my_date_compare( begin_cur, end_cur ) < 0 &&
				my_date_compare( begin_next, end_next ) < 0 ){

			my_date_set_from_date( &date, end_cur );
			g_date_add_days( &date, 1 );
			if( my_date_compare( &date, begin_next ) == 0 ){
				complete = TRUE;
			}
		}
	}

	if( priv->p1_closing_parms ){
		complete &= ofa_closing_parms_bin_is_valid( priv->p1_closing_parms, &msg );
		g_free( msg );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), complete );
}

/*
 * as all parameters have been checked ok, save in dossier
 */
static void
p1_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	const GDate *begin_cur, *end_cur;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	begin_cur = my_date_editable_get_date( GTK_EDITABLE( priv->p1_begin_cur ), NULL );
	end_cur = my_date_editable_get_date( GTK_EDITABLE( priv->p1_end_cur ), NULL );

	ofo_dossier_set_exe_begin( priv->dossier, begin_cur );
	ofo_dossier_set_exe_end( priv->dossier, end_cur );
	g_signal_emit_by_name( priv->hub, SIGNAL_HUB_EXE_DATES_CHANGED, begin_cur, end_cur );

	ofa_closing_parms_bin_apply( priv->p1_closing_parms );

	ofo_dossier_update( priv->dossier );

	g_signal_emit_by_name( priv->hub, SIGNAL_HUB_DOSSIER_CHANGED );
}

static void
p2_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p2_do_init";
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *parent, *label;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-dbms" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p2_dbms_credentials = ofa_dbms_root_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_dbms_credentials ));
	ofa_dbms_root_bin_set_meta( priv->p2_dbms_credentials, priv->meta );

	g_signal_connect(
			priv->p2_dbms_credentials, "ofa-changed", G_CALLBACK( p2_on_dbms_root_changed ), self );

	if( priv->p2_account && priv->p2_password ){
		ofa_dbms_root_bin_set_credentials(
				priv->p2_dbms_credentials, priv->p2_account, priv->p2_password );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_utils_widget_set_style( label, "labelerror" );
	priv->p2_message = label;

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );
}

static void
p2_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	p2_check_for_complete( self );
}

static void
p2_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	g_free( priv->p2_account );
	priv->p2_account = g_strdup( account );

	g_free( priv->p2_password );
	priv->p2_password = g_strdup( password );

	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	gboolean ok;
	gchar *message;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	p2_set_message( self, "" );

	ok = ofa_dbms_root_bin_is_valid( priv->p2_dbms_credentials, &message );
	if( !ok ){
		p2_set_message( self, message );
		g_free( message );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), ok );
}

static void
p2_set_message( ofaExerciceCloseAssistant *self, const gchar *message )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	if( priv->p2_message ){
		gtk_label_set_text( GTK_LABEL( priv->p2_message ), message );
	}
}

static void
p2_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
}

static void
p3_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->p3_checks_bin = ofa_check_balances_bin_new();
	gtk_container_add( GTK_CONTAINER( page_widget ), GTK_WIDGET( priv->p3_checks_bin ));

	g_signal_connect( priv->p3_checks_bin, "ofa-done", G_CALLBACK( p3_on_checks_done ), self );

	priv->p3_done = FALSE;
}

/*
 * run the checks before exercice closing
 */
static void
p3_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), priv->p3_done );

	if( !priv->p3_done ){
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_PROGRESS );
		ofa_check_balances_bin_set_hub( priv->p3_checks_bin, priv->hub );
	}
}

static void
p3_on_checks_done( ofaCheckBalancesBin *bin, gboolean ok, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->p3_done = TRUE;

	if( ok ){
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_CONTENT );
	} else {
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), priv->p3_done );
}

/*
 * run the DBMS checks before exercice closing
 */
static void
p4_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->p4_checks_bin = ofa_check_integrity_bin_new( st_settings );
	gtk_container_add( GTK_CONTAINER( page_widget ), GTK_WIDGET( priv->p4_checks_bin ));

	g_signal_connect( priv->p4_checks_bin, "ofa-done", G_CALLBACK( p4_on_checks_done ), self );

	priv->p4_done = FALSE;
}

static void
p4_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), priv->p4_done );

	if( !priv->p4_done ){
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_PROGRESS );
		ofa_check_integrity_bin_set_hub( priv->p4_checks_bin, priv->hub );
	}
}

static void
p4_on_checks_done( ofaCheckIntegrityBin *bin, gulong errors, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->p4_done = TRUE;

	if( errors == 0 ){
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_CONTENT );
	} else {
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
	}

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), priv->p4_done );
}

static void
p5_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *btn;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p5-backup-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( p5_on_backup_clicked ), self );
	priv->p5_backup_btn = btn;

	p5_check_for_complete( self );
}

static void
p5_do_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	p5_check_for_complete( self );
}

static void
p5_on_backup_clicked( GtkButton *button, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkApplicationWindow *main_window;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	main_window = ofa_igetter_get_main_window( priv->getter );
	ofa_main_window_dossier_backup( OFA_MAIN_WINDOW( main_window ));
	priv->p5_backuped = TRUE;

	p5_check_for_complete( self );
}

static void
p5_check_for_complete( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	gtk_widget_set_sensitive( priv->p5_backup_btn, !priv->p5_backuped );
}

static void
p6_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_init";
	ofaExerciceCloseAssistantPrivate *priv;
	GList *it;
	GtkWidget *validating_label, *summary_label, *grid;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), FALSE );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p6-grid61" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	validating_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p6-validating-label" );
	g_return_if_fail( validating_label && GTK_IS_LABEL( validating_label ));

	for( it=priv->close_list ; it ; it=it->next ){
		p6_init_plugin(
				self, grid,
				OFA_IEXECLOSE( it->data ), EXECLOSE_CLOSING, EXECLOSE_CLOSING_DATA,
				validating_label, ( GWeakNotify ) on_closing_instance_finalized );
	}

	summary_label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p6-summary" );
	g_return_if_fail( summary_label && GTK_IS_LABEL( summary_label ));

	for( it=priv->close_list ; it ; it=it->next ){
		p6_init_plugin(
				self, grid,
				OFA_IEXECLOSE( it->data ), EXECLOSE_OPENING, EXECLOSE_OPENING_DATA,
				summary_label, ( GWeakNotify ) on_opening_instance_finalized );
	}
}

/*
 * Ask the plugin which implement ofaIExeClose interface if it
 * wants do something on closing/opening the exercice.
 * If a text label is provided, then create a box, and attach it to
 * the instance.
 */
static void
p6_init_plugin( ofaExerciceCloseAssistant *self, GtkWidget *grid, ofaIExeClose *instance,
					guint type, const gchar *data_name, GtkWidget *sibling, GWeakNotify fn )
{
	gchar *text;
	GtkWidget *text_label, *box;
	sClose *close_data;

	text = ofa_iexe_close_add_row( instance, type );
	if( my_strlen( text )){
		text_label = gtk_label_new( text );
		gtk_label_set_xalign( GTK_LABEL( text_label ), 1 );
		gtk_grid_insert_next_to(
				GTK_GRID( grid ), sibling, GTK_POS_TOP );
		gtk_grid_attach_next_to(
				GTK_GRID( grid ), text_label, sibling, GTK_POS_TOP, 1, 1 );
		box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
		gtk_grid_attach_next_to(
				GTK_GRID( grid ), box, text_label, GTK_POS_RIGHT, 1, 1 );
		close_data = g_new0( sClose, 1 );
		close_data->box = box;
		g_object_set_data( G_OBJECT( instance ), data_name, close_data );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) fn, close_data );
	}
}

static void
p6_do_close( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_close";
	ofaExerciceCloseAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	priv->p6_page = page_widget;

	g_idle_add(( GSourceFunc ) p6_closing_plugin, self );
}

/*
 * let the plugin do its tasks when closing the exercice
 */
static gboolean
p6_closing_plugin( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_closing_plugin";
	ofaExerciceCloseAssistantPrivate *priv;
	GList *it;
	sClose *close_data;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	for( it=priv->close_list ; it ; it=it->next ){
		close_data = ( sClose * ) g_object_get_data( G_OBJECT( it->data ), EXECLOSE_CLOSING_DATA );
		if( close_data ){
			ofa_iexe_close_do_task(
					OFA_IEXECLOSE( it->data ), EXECLOSE_CLOSING, close_data->box, priv->hub );
		}
	}

	/* weird code to make the test easier */
	if( 1 ){
		g_idle_add(( GSourceFunc ) p6_validate_entries, self );
	} else {
		g_idle_add(( GSourceFunc ) p6_open, self );
	}

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * validate rough entries remaining in the exercice
 */
static gboolean
p6_validate_entries( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_validate_entries";
	ofaExerciceCloseAssistantPrivate *priv;
	GList *entries, *it;
	myProgressBar *bar;
	guint count, i;
	gchar *sstart, *send;
	gulong udelay;
	GTimeVal stamp_start, stamp_end;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	entries = ofo_entry_get_dataset_for_exercice_by_status( priv->hub, ENT_STATUS_ROUGH );
	count = g_list_length( entries );
	i = 0;
	my_utils_stamp_set_now( &stamp_start );
	bar = get_new_bar( self, "p6-validating" );
	gtk_widget_show_all( priv->p6_page );

	for( it=entries ; it ; it=it->next ){
		ofo_entry_validate( OFO_ENTRY( it->data ));
		update_bar( bar, &i, count, thisfn );
	}
	if( count == 0 ){
		g_signal_emit_by_name( bar, "my-text", "0/0" );
	}

	ofo_entry_free_dataset( entries );

	my_utils_stamp_set_now( &stamp_end );
	sstart = my_utils_stamp_to_str( &stamp_start, MY_STAMP_YYMDHMS );
	send = my_utils_stamp_to_str( &stamp_end, MY_STAMP_YYMDHMS );
	udelay = 1000000*(stamp_end.tv_sec-stamp_start.tv_sec)+stamp_end.tv_usec-stamp_start.tv_usec;

	g_debug( "%s: stamp_start=%s, stamp_end=%s, count=%u: average is %'.5lf s",
			thisfn, sstart, send, count, ( gdouble ) udelay / 1000000.0 / ( gdouble ) count );

	gtk_widget_show_all( GTK_WIDGET( bar ));
	g_idle_add(( GSourceFunc ) p6_solde_accounts, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * balance the detail accounts - for validated soldes only
 *
 * It shouldn't remain any amount on daily soldes, but we do not take
 * care of that here.
 */
static gboolean
p6_solde_accounts( ofaExerciceCloseAssistant *self )
{
	if( !p6_do_solde_accounts( self, TRUE )){
		g_idle_add(( GSourceFunc ) p6_close_ledgers, self );
	}

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * balance the detail accounts - for validated soldes only
 *
 * It shouldn't remain any amount on daily soldes, but we do not take
 * care of that here.
 *
 * Note: forward entries on setlleable accounts are automatically set
 * as settled, being balanced with the corresponding solde entry
 */
static gint
p6_do_solde_accounts( ofaExerciceCloseAssistant *self, gboolean with_ui )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_solde_accounts";
	ofaExerciceCloseAssistantPrivate *priv;
	GList *accounts, *sld_entries, *for_entries, *it, *ite, *currencies;
	myProgressBar *bar;
	guint count, i;
	gchar *msg;
	ofoAccount *account;
	ofoOpeTemplate *sld_template, *for_template;
	const gchar *sld_ope, *for_ope, *acc_number, *acc_cur;
	const GDate *end_cur, *begin_next;
	gboolean is_ran;
	ofsOpe *ope;
	ofsOpeDetail *detail;
	gint errors;
	ofxCounter counter;
	ofoEntry *entry;
	ofoCurrency *cur_obj;
	ofsCurrency *scur;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	bar = NULL;
	errors = 0;
	accounts = ofo_account_get_dataset_for_solde( priv->hub );
	count = g_list_length( accounts );
	i = 0;

	if( with_ui ){
		bar = get_new_bar( self, "p6-balancing" );
		gtk_widget_show_all( priv->p6_page );
	}

	priv->p6_forwards = NULL;

	end_cur = ofo_dossier_get_exe_end( priv->dossier );
	begin_next = my_date_editable_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );

	sld_ope = ofo_dossier_get_sld_ope( priv->dossier );
	sld_template = ofo_ope_template_get_by_mnemo( priv->hub, sld_ope );
	g_return_val_if_fail( sld_template && OFO_IS_OPE_TEMPLATE( sld_template ), 1 );

	for_ope = ofo_dossier_get_forward_ope( priv->dossier );
	for_template = ofo_ope_template_get_by_mnemo( priv->hub, for_ope );
	g_return_val_if_fail( for_template && OFO_IS_OPE_TEMPLATE( for_template ), 1 );

	for( it=accounts ; it ; it=it->next ){
		account = OFO_ACCOUNT( it->data );

		/* setup ofsCurrency */
		acc_cur = ofo_account_get_currency( account );
		cur_obj = ofo_currency_get_by_code( priv->hub, acc_cur );
		scur = g_new0( ofsCurrency, 1 );
		scur->currency = cur_obj;
		scur->debit = ofo_account_get_val_debit( account );
		scur->credit = ofo_account_get_val_credit( account );

		if( !ofs_currency_is_balanced( scur )){

			acc_number = ofo_account_get_number( account );
			sld_entries = NULL;
			for_entries = NULL;
			counter = 0;

			/* create solde operation
			 * and generate corresponding solde entries */
			ope = ofs_ope_new( sld_template );
			my_date_set_from_date( &ope->deffect, end_cur );
			ope->deffect_user_set = TRUE;
			detail = ( ofsOpeDetail * ) ope->detail->data;
			detail->account = g_strdup( acc_number );
			detail->account_user_set = TRUE;
			if( scur->debit > scur->credit ){
				detail->credit = scur->debit - scur->credit;
				detail->credit_user_set = TRUE;
			} else {
				detail->debit = scur->credit - scur->debit;
				detail->debit_user_set = TRUE;
			}

			ofs_ope_apply_template( ope );

			if( ofs_ope_is_valid( ope, &msg, &currencies )){
				sld_entries = ofs_ope_generate_entries( ope );

			} else {
				g_warning( "%s: %s", thisfn, msg );
				g_free( msg );
				ofs_currency_list_dump( currencies );
				errors += 1;
			}
			ofs_currency_list_free( &currencies );

			ofs_ope_free( ope );

			/* create forward operation
			 * and generate corresponding entries */
			is_ran = ofo_account_is_forwardable( account );
			if( is_ran ){
				ope = ofs_ope_new( for_template );
				my_date_set_from_date( &ope->deffect, begin_next );
				ope->deffect_user_set = TRUE;
				detail = ( ofsOpeDetail * ) ope->detail->data;
				detail->account = g_strdup( acc_number );
				detail->account_user_set = TRUE;
				if( scur->debit > scur->credit ){
					detail->debit = scur->debit - scur->credit;
					detail->debit_user_set = TRUE;
				} else {
					detail->credit = scur->credit - scur->debit;
					detail->credit_user_set = TRUE;
				}

				ofs_ope_apply_template( ope );

				if( ofs_ope_is_valid( ope, NULL, NULL )){
					for_entries = ofs_ope_generate_entries( ope );
				}

				ofs_ope_free( ope );
			}

			/* all entries have been prepared
			 *
			 * -> set a settlement number on those which are to be
			 *    written on a settleable account
			 *    + take care of setting the same counter on the solde
			 *    and the forward entries to have an audit track
			 *
			 * -> set a reconciliation date on the solde entries which
			 *    are to be written on a reconciliable account, so that
			 *    they will not be reported on the next exercice
			 */
			for( ite=sld_entries ; ite ; ite=ite->next ){
				entry = OFO_ENTRY( ite->data );
				ofo_entry_insert( entry, priv->hub );
				if( is_ran &&
						ofo_account_is_settleable( account ) &&
						!g_utf8_collate( ofo_entry_get_account( entry ), acc_number )){
					counter = ofo_dossier_get_next_settlement( priv->dossier );
					ofo_entry_update_settlement( entry, counter );
					p6_set_forward_settlement_number( for_entries, acc_number, counter );
				}
				if( ofo_account_is_reconciliable( account ) &&
						!g_utf8_collate( ofo_entry_get_account( entry ), acc_number )){
					ofa_iconcil_new_concil( OFA_ICONCIL( entry ), end_cur );
				}
			}
			ofo_entry_free_dataset( sld_entries );

			for( ite=for_entries ; ite ; ite=ite->next ){
				entry = OFO_ENTRY( ite->data );
				priv->p6_forwards = g_list_prepend( priv->p6_forwards, entry );
			}
		}

		g_free( scur );

		if( with_ui ){
			update_bar( bar, &i, count, thisfn );
		}
	}

	ofo_account_free_dataset( accounts );

	if( with_ui ){
		gtk_widget_show_all( GTK_WIDGET( bar ));
	}

	if( errors ){
		msg = g_strdup_printf(
				_( "%d errors have been found while computing accounts soldes" ), errors );
		my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_WARNING, msg );
		g_free( msg );
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
	}

	return( errors );
}

/*
 * set the specified settlement number on the forward entry for the
 * specified account - as there should only be one entry per account,
 * we just stop as soon as we have found it
 */
static void
p6_set_forward_settlement_number( GList *entries, const gchar *account, ofxCounter counter )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_set_forward_settlement_number";
	GList *it;
	ofoEntry *entry;

	for( it=entries ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		if( !g_utf8_collate( ofo_entry_get_account( entry ), account )){
			ofo_entry_set_settlement_number( entry, counter );
			return;
		}
	}

	g_warning( "%s: no found entry for %s account", thisfn, account );
}

/*
 * close all the ledgers
 */
static gboolean
p6_close_ledgers( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_close_ledgers";
	ofaExerciceCloseAssistantPrivate *priv;
	GList *ledgers, *it;
	myProgressBar *bar;
	guint count, i;
	const GDate *end_cur;
	ofoLedger *ledger;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	ledgers = ofo_ledger_get_dataset( priv->hub );
	count = g_list_length( ledgers );
	i = 0;
	bar = get_new_bar( self, "p6-ledgers" );
	gtk_widget_show_all( priv->p6_page );

	end_cur = ofo_dossier_get_exe_end( priv->dossier );

	for( it=ledgers ; it ; it=it->next ){
		ledger = OFO_LEDGER( it->data );
		ofo_ledger_close( ledger, end_cur );
		update_bar( bar, &i, count, thisfn );
	}

	gtk_widget_show_all( GTK_WIDGET( bar ));
	g_idle_add(( GSourceFunc ) p6_archive_exercice, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * archive current exercice
 * opening the new one
 */
static gboolean
p6_archive_exercice( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	gboolean ok;
	GtkWidget *label;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	ok = p6_do_archive_exercice( self, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->p6_page ), "p6-archived" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
	gtk_label_set_text( GTK_LABEL( label ), ok ? _( "Done" ) : _( "Error" ));

	if( ok ){
		g_idle_add(( GSourceFunc ) p6_cleanup, self );
	}

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * archive current exercice
 * opening the new one
 */
static gboolean
p6_do_archive_exercice( ofaExerciceCloseAssistant *self, gboolean with_ui )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_archive_exercice";
	ofaExerciceCloseAssistantPrivate *priv;
	GtkApplicationWindow *main_window;
	ofaIDBConnect *cnx;
	ofaIDBPeriod *period;
	gboolean ok;
	const GDate *begin_old, *end_old;
	const GDate *begin_next, *end_next;
	gchar *cur_account, *cur_password;
	ofaIDBProvider *provider;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	main_window = ofa_igetter_get_main_window( priv->getter );

	ofo_dossier_set_current( priv->dossier, FALSE );
	ofo_dossier_update( priv->dossier );

	period = ofa_idbconnect_get_period( priv->connect );
	begin_old = ofo_dossier_get_exe_begin( priv->dossier );
	end_old = ofo_dossier_get_exe_end( priv->dossier );
	ofa_idbmeta_update_period( priv->meta, period, FALSE, begin_old, end_old );
	g_object_unref( period );

	begin_next = my_date_editable_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );
	end_next = my_date_editable_get_date( GTK_EDITABLE( priv->p1_end_next ), NULL );
	ok = ofa_idbconnect_archive_and_new(
				priv->connect, priv->p2_account, priv->p2_password, begin_next, end_next );

	if( !ok ){
		my_iwindow_msg_dialog(
				MY_IWINDOW( self ), GTK_MESSAGE_WARNING, _( "Unable to archive the dossier" ));
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	} else {
		/* open the new exercice */
		period = ofa_idbmeta_get_current_period( priv->meta );
		g_return_val_if_fail( period && OFA_IS_IDBPERIOD( period ), FALSE );
		ofa_idbperiod_dump( period );

		provider = ofa_idbmeta_get_provider( priv->meta );
		cur_account = ofa_idbconnect_get_account( priv->connect );
		cur_password = ofa_idbconnect_get_password( priv->connect );

		cnx = ofa_idbprovider_new_connect( provider );
		ok = ofa_idbconnect_open_with_meta( cnx, cur_account, cur_password, priv->meta, period );

		g_free( cur_password );
		g_free( cur_account );
		g_object_unref( provider );
		g_object_unref( period );

		if( !ok ){
			my_iwindow_msg_dialog(
					MY_IWINDOW( self ), GTK_MESSAGE_WARNING, _( "Unable to open a connection on the new exercice" ));
			my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
			my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

		} else {
			/* opening the new dossier means also closing the old one
			 * prevent the window manager to close this particular one
			 */
			priv->is_destroy_allowed = FALSE;
			ok = ofa_hub_dossier_open( priv->hub, cnx, GTK_WINDOW( main_window ));
			priv->is_destroy_allowed = TRUE;
			if( ok ){
				priv->dossier = ofa_hub_get_dossier( priv->hub );
				priv->connect = ofa_hub_get_connect( priv->hub );
				ofo_dossier_set_current( priv->dossier, TRUE );
				ofo_dossier_set_exe_begin( priv->dossier, begin_next );
				ofo_dossier_set_exe_end( priv->dossier, end_next );
				ofo_dossier_set_prevexe_end( priv->dossier, end_old );
				ofo_dossier_update( priv->dossier );
			}
		}

		g_object_unref( cnx );
	}

	/* re-emit the changed signal after changes */
	g_signal_emit_by_name( priv->hub, SIGNAL_HUB_DOSSIER_CHANGED );

	return( ok );
}

/*
 * erase audit table
 * remove settled entries on settleable accounts
 * remove reconciliated entries on reconciliable accounts
 * remove all entries on unsettleable or unreconciliable accounts
 * update remaining entries status to PAST
 * remove BAT files (and lines) which have been fully reconciliated
 * reset all account and ledger balances to zero
 */
static gboolean
p6_cleanup( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_cleanup";
	ofaExerciceCloseAssistantPrivate *priv;
	gchar *query;
	gboolean ok;
	GtkWidget *label;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	query = g_strdup( "TRUNCATE TABLE OFA_T_AUDIT" );
	ok = ofa_idbconnect_query( priv->connect, query, TRUE );
	g_free( query );

	/* cleanup archived accounts balances of the previous exercice
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_ACCOUNTS_ARC" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_ACCOUNTS_ARC "
					"SELECT * FROM OFA_T_ACCOUNTS_ARC" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_ACCOUNTS_ARC" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	/* archive deleted (non-reported) entries
	 * i.e. those which are tight to an unsettleable or an unreconcilable
	 * account, or are not settled, or are not reconciliated
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_KEEP_ENTRIES" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup_printf( "CREATE TABLE ARCHIVE_T_KEEP_ENTRIES "
					"SELECT ENT_NUMBER FROM OFA_T_ENTRIES,OFA_T_ACCOUNTS "
					"	WHERE ENT_ACCOUNT=ACC_NUMBER AND ("
					"		(ACC_SETTLEABLE='Y' AND ENT_STLMT_NUMBER IS NULL) OR "
					"		(ACC_RECONCILIABLE='Y' AND ENT_NUMBER NOT IN ("
					"			SELECT REC_IDS_OTHER FROM OFA_T_CONCIL_IDS WHERE REC_IDS_TYPE='E'))) AND "
					"		ENT_STATUS!=%d AND ENT_STATUS!=%d",
					ENT_STATUS_DELETED, ENT_STATUS_FUTURE );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_DELETED_ENTRIES" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_DELETED_ENTRIES "
					"SELECT * FROM OFA_T_ENTRIES WHERE "
					"	ENT_NUMBER NOT IN (SELECT ENT_NUMBER FROM ARCHIVE_T_KEEP_ENTRIES)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_ENTRIES "
					"WHERE ENT_NUMBER NOT IN (SELECT ENT_NUMBER FROM ARCHIVE_T_KEEP_ENTRIES)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	/* set previous exercice entries status to 'past' */
	if( ok ){
		query = g_strdup_printf( "UPDATE OFA_T_ENTRIES SET "
					"ENT_STATUS=%d WHERE ENT_STATUS!=%d", ENT_STATUS_PAST, ENT_STATUS_FUTURE );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	/* keep bat files which are not fully reconciliated
	 * and archived others
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_KEEP_BATS" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_KEEP_BATS "
					"SELECT DISTINCT(BAT_ID) FROM OFA_T_BAT_LINES "
					"	WHERE BAT_LINE_ID NOT IN "
					"		(SELECT REC_IDS_OTHER FROM OFA_T_CONCIL_IDS "
					"			WHERE REC_IDS_TYPE='B')" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_DELETED_BATS" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_DELETED_BATS "
					"SELECT * FROM OFA_T_BAT "
					"	WHERE BAT_ID NOT IN (SELECT BAT_ID FROM ARCHIVE_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_DELETED_BAT_LINES" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_DELETED_BAT_LINES "
					"SELECT * FROM OFA_T_BAT_LINES "
					"	WHERE BAT_ID NOT IN (SELECT BAT_ID FROM ARCHIVE_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_BAT "
					"WHERE BAT_ID NOT IN (SELECT BAT_ID FROM ARCHIVE_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_BAT_LINES "
					"WHERE BAT_ID NOT IN (SELECT BAT_ID FROM ARCHIVE_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	/* reset to zero accounts and ledgers balances
	 */
	if( ok ){
		query = g_strdup( "UPDATE OFA_T_ACCOUNTS SET "
					"ACC_VAL_DEBIT=0, ACC_VAL_CREDIT=0, "
					"ACC_ROUGH_DEBIT=0, ACC_ROUGH_CREDIT=0" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "UPDATE OFA_T_LEDGERS_CUR SET "
					"LED_CUR_VAL_DEBIT=0, LED_CUR_VAL_CREDIT=0, "
					"LED_CUR_ROUGH_DEBIT=0, LED_CUR_ROUGH_CREDIT=0" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->p6_page ), "p6-cleanup" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
	gtk_label_set_text( GTK_LABEL( label ), ok ? _( "Done" ) : _( "Error" ));

	if( ok ){
		g_idle_add(( GSourceFunc ) p6_forward, self );

	} else {
		my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
		my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );
	}

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * apply generated carried forward entries
 *
 * they are inserted with 'validated' status, and the settlement number
 * is set if it has already been previously set when generating the entry
 *
 * + entries on reconciliable accounts are set reconciliated
 *   on the first day of the exercice (which is also the operation date
 *   and the effect date)
 */
static gboolean
p6_forward( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_forward";
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	guint count, i;
	GList *it;
	ofoEntry *entry;
	ofoAccount *account;
	ofxCounter counter;
	const GDate *dbegin;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	dbegin = ofo_dossier_get_exe_begin( priv->dossier );

	bar = get_new_bar( self, "p6-forward" );
	gtk_widget_show_all( priv->p6_page );

	count = g_list_length( priv->p6_forwards );
	i = 0;

	for( it=priv->p6_forwards ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		ofo_entry_insert( entry, priv->hub );

		counter = ofo_entry_get_settlement_number( entry );
		if( counter ){
			ofo_entry_update_settlement( entry, counter );
		}

		/* set reconciliation on reconciliable account */
		account = ofo_account_get_by_number( priv->hub, ofo_entry_get_account( entry ));
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
		if( ofo_account_is_reconciliable( account )){
			ofa_iconcil_new_concil( OFA_ICONCIL( entry ), dbegin );
		}

		g_signal_emit_by_name( priv->hub,
				SIGNAL_HUB_STATUS_CHANGE, entry, ENT_STATUS_ROUGH, ENT_STATUS_VALIDATED );

		update_bar( bar, &i, count, thisfn );
	}

	ofo_entry_free_dataset( priv->p6_forwards );

	gtk_widget_show_all( GTK_WIDGET( bar ));
	g_idle_add(( GSourceFunc ) p6_open, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * archive begin of exercice accounts balance
 *
 * open=rough+validated, but at this time we only have:
 * - past entries (unreconciliated or unsettled from previous exercice)
 * - forward entries (which are in 'validated' status)
 */
static gboolean
p6_open( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_open";
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	guint count, i;
	GList *accounts, *it;
	ofoAccount *account;
	const GDate *begin_next;

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	accounts = ofo_account_get_dataset( priv->hub );
	count = g_list_length( accounts );
	i = 0;
	bar = get_new_bar( self, "p6-open" );
	gtk_widget_show_all( priv->p6_page );

	begin_next = my_date_editable_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );

	for( it=accounts ; it ; it=it->next ){
		account = OFO_ACCOUNT( it->data );
		if( !ofo_account_is_root( account )){
			ofo_account_archive_balances( account, begin_next );
		}
		update_bar( bar, &i, count, thisfn );
	}

	gtk_widget_show_all( GTK_WIDGET( bar ));
	g_idle_add(( GSourceFunc ) p6_future, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * take the ex-future entries, bringing them up in the new exercice
 * if appropriate
 */
static gboolean
p6_future( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_future";
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	guint count, i;
	GList *entries, *it;
	ofoEntry *entry;
	const GDate *dos_dend, *ent_deffect;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	dos_dend = ofo_dossier_get_exe_end( priv->dossier );

	entries = ofo_entry_get_dataset_for_exercice_by_status( priv->hub, ENT_STATUS_FUTURE );
	count = g_list_length( entries );
	i = 0;
	bar = get_new_bar( self, "p6-future" );
	gtk_widget_show_all( priv->p6_page );

	for( it=entries ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		ent_deffect = ofo_entry_get_deffect( entry );
		if( my_date_compare( ent_deffect, dos_dend ) <= 0 ){
			g_signal_emit_by_name( priv->hub,
					SIGNAL_HUB_STATUS_CHANGE, entry, ENT_STATUS_FUTURE, ENT_STATUS_ROUGH );
		}
		update_bar( bar, &i, count, thisfn );
	}
	if( count == 0 ){
		g_signal_emit_by_name( bar, "my-text", "0/0" );
	}

	gtk_widget_show_all( GTK_WIDGET( bar ));
	g_idle_add(( GSourceFunc ) p6_opening_plugin, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * let the plugins do their stuff
 */
static gboolean
p6_opening_plugin( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_opening_plugin";
	ofaExerciceCloseAssistantPrivate *priv;
	GList *it;
	sClose *close_data;
	GtkWidget *summary_label;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = ofa_exercice_close_assistant_get_instance_private( self );

	for( it=priv->close_list ; it ; it=it->next ){
		close_data = ( sClose * ) g_object_get_data( G_OBJECT( it->data ), EXECLOSE_OPENING_DATA );
		if( close_data ){
			ofa_iexe_close_do_task(
					OFA_IEXECLOSE( it->data ), EXECLOSE_OPENING, close_data->box, priv->hub );
		}
	}

	summary_label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->p6_page ), "p6-summary" );
	g_return_val_if_fail( summary_label && GTK_IS_LABEL( summary_label ), FALSE );

	gtk_label_set_text( GTK_LABEL( summary_label ),
			_( "The previous exercice has been successfully closed.\n"
				"The next exercice has been automatically defined and opened." ));

	my_iassistant_set_current_page_type( MY_IASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
	my_iassistant_set_current_page_complete( MY_IASSISTANT( self ), TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

static myProgressBar *
get_new_bar( ofaExerciceCloseAssistant *self, const gchar *w_name )
{
	GtkWidget *parent;
	myProgressBar *bar;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );
	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( bar ));

	return( bar );
}

static void
update_bar( myProgressBar *bar, guint *count, guint total, const gchar *emitter_name )
{
	gdouble progress;
	gchar *text;

	*count += 1;

	progress = ( gdouble ) *count / ( gdouble ) total;
	g_signal_emit_by_name( bar, "my-double", progress );

	text = g_strdup_printf( "%u/%u", *count, total );
	g_signal_emit_by_name( bar, "my-text", text );

	g_debug( "%s: progress=%.5lf, text=%s", emitter_name, progress, text );

	g_free( text );
}

/*
 * when the ofaIExeClosexxx plugin finalizes
 */
static void
on_closing_instance_finalized( sClose *close_data, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_on_closing_instance_finalized";

	g_debug( "%s: close_data=%p, finalized_instance=%p",
			thisfn, ( void * ) close_data, ( void * ) finalized_instance );

	g_free( close_data );
}

static void
on_opening_instance_finalized( sClose *close_data, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_on_opening_instance_finalized";

	g_debug( "%s: close_data=%p, finalized_instance=%p",
			thisfn, ( void * ) close_data, ( void * ) finalized_instance );

	g_free( close_data );
}
