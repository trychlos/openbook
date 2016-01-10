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

#include "api/my-date.h"
#include "api/my-editable-date.h"
#include "api/my-progress-bar.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-iconcil.h"
#include "core/ofa-main-window.h"

#include "ui/my-assistant.h"
#include "ui/ofa-balance-grid-bin.h"
#include "ui/ofa-check-balances-bin.h"
#include "ui/ofa-check-integrity-bin.h"
#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-exercice-close-assistant.h"

/* private instance data
 */
struct _ofaExerciceCloseAssistantPrivate {

	ofaMainWindow        *main_window;
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

	/* p6 - close the exercice
	 */
	GtkWidget            *p6_page;
	GList                *p6_forwards;			/* forward operations */
	GList                *p6_cleanup;
	GList                *p6_unreconciliated;
};

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

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-exercice-close-assistant.ui";
static const gchar *st_ui_id            = "ExerciceCloseAssistant";

G_DEFINE_TYPE( ofaExerciceCloseAssistant, ofa_exercice_close_assistant, MY_TYPE_ASSISTANT )

static void             p0_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p1_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p1_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p1_on_date_changed( GtkEditable *editable, ofaExerciceCloseAssistant *self );
static void             p1_on_closing_parms_changed( ofaClosingParmsBin *bin, ofaExerciceCloseAssistant *self );
static void             p1_check_for_complete( ofaExerciceCloseAssistant *self );
static void             p1_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p2_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p2_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p2_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaExerciceCloseAssistant *self );
static void             p2_check_for_complete( ofaExerciceCloseAssistant *self );
static void             p2_set_message( ofaExerciceCloseAssistant *self, const gchar *message );
static void             p2_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p3_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p3_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p3_on_checks_done( ofaCheckBalancesBin *bin, gboolean ok, ofaExerciceCloseAssistant *self );
static void             p4_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p4_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p4_on_checks_done( ofaCheckIntegrityBin *bin, gulong errors, ofaExerciceCloseAssistant *self );
static void             p6_do_close( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static gboolean         p6_validate_entries( ofaExerciceCloseAssistant *self );
static gboolean         p6_solde_accounts( ofaExerciceCloseAssistant *self );
static gint             p6_do_solde_accounts( ofaExerciceCloseAssistant *self, gboolean with_ui );
static void             p6_set_forward_settlement_number( GList *entries, const gchar *account, ofxCounter counter );
static gboolean         p6_close_ledgers( ofaExerciceCloseAssistant *self );
static gboolean         p6_archive_exercice( ofaExerciceCloseAssistant *self );
static gboolean         p6_do_archive_exercice( ofaExerciceCloseAssistant *self, gboolean with_ui );
static gboolean         p6_cleanup( ofaExerciceCloseAssistant *self );
static gboolean         p6_forward( ofaExerciceCloseAssistant *self );
static gboolean         p6_open( ofaExerciceCloseAssistant *self );
static gboolean         p6_future( ofaExerciceCloseAssistant *self );
static myProgressBar   *get_new_bar( ofaExerciceCloseAssistant *self, const gchar *w_name );

static const ofsAssistant st_pages_cb [] = {
		{ PAGE_INTRO,
				NULL,
				NULL,
				( myAssistantCb ) p0_do_forward },
		{ PAGE_PARMS,
				( myAssistantCb ) p1_do_init,
				( myAssistantCb ) p1_display,
				( myAssistantCb ) p1_do_forward },
		{ PAGE_DBMS,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_display,
				( myAssistantCb ) p2_do_forward },
		{ PAGE_CHECKS,
				( myAssistantCb ) p3_do_init,
				( myAssistantCb ) p3_checks,
				NULL },
		{ PAGE_CHECK_DBMS,
				( myAssistantCb ) p4_do_init,
				( myAssistantCb ) p4_checks,
				NULL },
		{ PAGE_CONFIRM,
				NULL,
				NULL,
				NULL },
		{ PAGE_CLOSE,
				NULL,
				( myAssistantCb ) p6_do_close,
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
	priv = OFA_EXERCICE_CLOSE_ASSISTANT( instance )->priv;

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

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_EXERCICE_CLOSE_ASSISTANT( instance )->priv;

		g_clear_object( &priv->connect );
		g_clear_object( &priv->meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_close_assistant_parent_class )->dispose( instance );
}

static void
ofa_exercice_close_assistant_init( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_CLOSE_ASSISTANT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistantPrivate );
}

static void
ofa_exercice_close_assistant_class_init( ofaExerciceCloseAssistantClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_close_assistant_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_close_assistant_finalize;

	g_type_class_add_private( klass, sizeof( ofaExerciceCloseAssistantPrivate ));
}

/**
 * ofa_exercice_close_assistant_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected ledgers
 */
void
ofa_exercice_close_assistant_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_run";
	ofaExerciceCloseAssistant *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_EXERCICE_CLOSE_ASSISTANT,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->main_window = main_window;

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );
	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * get some dossier data
 */
static void
p0_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p0_do_forward";
	ofaExerciceCloseAssistantPrivate *priv;
	GtkApplication *application;

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = self->priv;

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->connect = ofa_hub_get_connect( priv->hub );
	priv->meta = ofa_idbconnect_get_meta( priv->connect );
	priv->dos_name = ofa_idbmeta_get_dossier_name( priv->meta );

	priv->dossier = ofa_hub_get_dossier( priv->hub );
}

static void
p1_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *parent, *label;
	const GDate *begin_cur, *end_cur;
	GDate begin, end;
	gint exe_length;

	priv = self->priv;

	exe_length = ofo_dossier_get_exe_length( priv->dossier );

	/* closing exercice - beginning date */
	priv->p1_begin_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-begin-entry" );
	g_return_if_fail( priv->p1_begin_cur && GTK_IS_ENTRY( priv->p1_begin_cur ));
	my_editable_date_init( GTK_EDITABLE( priv->p1_begin_cur ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p1_begin_cur ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p1_begin_cur ), TRUE );
	begin_cur = ofo_dossier_get_exe_begin( priv->dossier );
	my_editable_date_set_date( GTK_EDITABLE( priv->p1_begin_cur ), begin_cur );
	g_signal_connect( G_OBJECT( priv->p1_begin_cur ), "changed", G_CALLBACK( p1_on_date_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->p1_begin_cur );

	/* closing exercice - ending date */
	priv->p1_end_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-end-entry" );
	g_return_if_fail( priv->p1_end_cur && GTK_IS_ENTRY( priv->p1_end_cur ));
	my_editable_date_init( GTK_EDITABLE( priv->p1_end_cur ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p1_end_cur ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p1_end_cur ), TRUE );
	end_cur = ofo_dossier_get_exe_end( priv->dossier );
	my_editable_date_set_date( GTK_EDITABLE( priv->p1_end_cur ), end_cur );
	g_signal_connect( G_OBJECT( priv->p1_end_cur ), "changed", G_CALLBACK( p1_on_date_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-closing-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->p1_end_cur );

	/* set a date if the other is valid */
	if( !my_date_is_valid( begin_cur ) && my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &begin, end_cur );
		g_date_subtract_months( &begin, exe_length );
		g_date_add_days( &begin, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p1_begin_cur ), &begin );
		my_date_set_from_date( &end, end_cur );

	} else if( my_date_is_valid( begin_cur ) && !my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &end, begin_cur );
		g_date_add_months( &end, exe_length );
		g_date_subtract_days( &end, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p1_end_cur ), &end );

	} else if( my_date_is_valid( end_cur )){
		my_date_set_from_date( &end, end_cur );
	}

	/* next exercice - beginning date */
	priv->p1_begin_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-begin-entry" );
	g_return_if_fail( priv->p1_begin_next && GTK_IS_ENTRY( priv->p1_begin_next ));
	my_editable_date_init( GTK_EDITABLE( priv->p1_begin_next ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p1_begin_next ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p1_begin_next ), TRUE );
	g_signal_connect( G_OBJECT( priv->p1_begin_next ), "changed", G_CALLBACK( p1_on_date_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->p1_begin_next );

	if( my_date_is_valid( &end )){
		my_date_set_from_date( &begin, &end );
		g_date_add_days( &begin, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p1_begin_next ), &begin );
	}

	/* next exercice - ending date */
	priv->p1_end_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-end-entry" );
	g_return_if_fail( priv->p1_end_next && GTK_IS_ENTRY( priv->p1_end_next ));
	my_editable_date_init( GTK_EDITABLE( priv->p1_end_next ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p1_end_next ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p1_end_next ), TRUE );
	g_signal_connect( G_OBJECT( priv->p1_end_next ), "changed", G_CALLBACK( p1_on_date_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-next-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->p1_end_next );

	if( my_date_is_valid( &end ) && exe_length > 0 ){
		g_date_add_months( &end, exe_length );
		my_editable_date_set_date( GTK_EDITABLE( priv->p1_end_next ), &end );
	}

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p1-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p1_closing_parms = ofa_closing_parms_bin_new( priv->main_window );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p1_closing_parms ));
	g_signal_connect(
			G_OBJECT( priv->p1_closing_parms ),
			"ofa-changed", G_CALLBACK( p1_on_closing_parms_changed ), self );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), FALSE );
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

	priv = self->priv;

	complete = FALSE;

	if( priv->p1_end_next ){
		begin_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p1_begin_cur ), NULL );
		end_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p1_end_cur ), NULL );
		begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );
		end_next = my_editable_date_get_date( GTK_EDITABLE( priv->p1_end_next ), NULL );

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

	my_assistant_set_page_complete( MY_ASSISTANT( self ), complete );
}

/*
 * as all parameters have been checked ok, save in dossier
 */
static void
p1_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	const GDate *begin_cur, *end_cur;

	priv = self->priv;

	begin_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p1_begin_cur ), NULL );
	end_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p1_end_cur ), NULL );

	ofo_dossier_set_exe_begin( priv->dossier, begin_cur );
	ofo_dossier_set_exe_end( priv->dossier, end_cur );
	g_signal_emit_by_name( priv->hub, SIGNAL_HUB_EXE_DATES_CHANGED, begin_cur, end_cur );
	ofa_main_window_update_title( priv->main_window );

	ofa_closing_parms_bin_apply( priv->p1_closing_parms );

	ofo_dossier_update( priv->dossier );
}

static void
p2_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p2_do_init";
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *parent, *label;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = self->priv;

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

	my_assistant_set_page_complete( MY_ASSISTANT( self ), FALSE );
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

	priv = self->priv;

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

	priv = self->priv;
	p2_set_message( self, "" );

	ok = ofa_dbms_root_bin_is_valid( priv->p2_dbms_credentials, &message );
	if( !ok ){
		p2_set_message( self, message );
		g_free( message );
	}

	my_assistant_set_page_complete( MY_ASSISTANT( self ), ok );
}

static void
p2_set_message( ofaExerciceCloseAssistant *self, const gchar *message )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

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

	priv = self->priv;

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->p3_done );

	if( !priv->p3_done ){
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_PROGRESS );
		ofa_check_balances_bin_set_hub( priv->p3_checks_bin, priv->hub );
	}
}

static void
p3_on_checks_done( ofaCheckBalancesBin *bin, gboolean ok, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;
	priv->p3_done = TRUE;

	if( ok ){
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_CONTENT );
	} else {
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
	}

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->p3_done );
}

/*
 * run the DBMS checks before exercice closing
 */
static void
p4_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;

	priv->p4_checks_bin = ofa_check_integrity_bin_new();
	gtk_container_add( GTK_CONTAINER( page_widget ), GTK_WIDGET( priv->p4_checks_bin ));

	g_signal_connect( priv->p4_checks_bin, "ofa-done", G_CALLBACK( p4_on_checks_done ), self );

	priv->p4_done = FALSE;
}

static void
p4_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->p4_done );

	if( !priv->p4_done ){
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_PROGRESS );
		ofa_check_integrity_bin_set_hub( priv->p4_checks_bin, priv->hub );
	}
}

static void
p4_on_checks_done( ofaCheckIntegrityBin *bin, gulong errors, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;
	priv->p4_done = TRUE;

	if( errors == 0 ){
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_CONTENT );
	} else {
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
	}

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->p4_done );
}

static void
p6_do_close( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_close";
	ofaExerciceCloseAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	my_assistant_set_page_complete( MY_ASSISTANT( self ), FALSE );

	priv = self->priv;
	priv->p6_page = page_widget;

	g_idle_add(( GSourceFunc ) p6_validate_entries, self );
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
	gint count, i;
	gdouble progress;
	gchar *text, *sstart, *send;
	gulong udelay;
	GTimeVal stamp_start, stamp_end;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	entries = ofo_entry_get_dataset_for_exercice_by_status( priv->hub, ENT_STATUS_ROUGH );
	count = g_list_length( entries );
	my_utils_stamp_set_now( &stamp_start );

	bar = get_new_bar( self, "p6-validating" );
	gtk_widget_show_all( priv->p6_page );

	for( i=1, it=entries ; it ; ++i, it=it->next ){
		ofo_entry_validate( OFO_ENTRY( it->data ));

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "ofa-double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "ofa-text", text );

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );

		g_free( text );
	}

	if( !entries ){
		g_signal_emit_by_name( bar, "ofa-text", "0/0" );
	}

	ofo_entry_free_dataset( entries );

	my_utils_stamp_set_now( &stamp_end );
	sstart = my_utils_stamp_to_str( &stamp_start, MY_STAMP_YYMDHMS );
	send = my_utils_stamp_to_str( &stamp_end, MY_STAMP_YYMDHMS );
	udelay = 1000000*(stamp_end.tv_sec-stamp_start.tv_sec)+stamp_end.tv_usec-stamp_start.tv_usec;

	g_debug( "%s: stamp_start=%s, stamp_end=%s, count=%u: average is %'.5lf s",
			thisfn, sstart, send, count, ( gdouble ) udelay / 1000000.0 / ( gdouble ) count );

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
	gint count, i;
	gdouble progress;
	gchar *text, *msg;
	ofoAccount *account;
	ofoOpeTemplate *sld_template, *for_template;
	const gchar *sld_ope, *for_ope, *acc_number;
	ofxAmount debit, credit;
	const GDate *end_cur, *begin_next;
	gboolean is_ran;
	ofsOpe *ope;
	ofsOpeDetail *detail;
	gint errors;
	gdouble precision;
	ofxCounter counter;
	ofoEntry *entry;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	bar = NULL;
	errors = 0;
	accounts = ofo_account_get_dataset_for_solde( priv->hub );
	count = g_list_length( accounts );
	precision = 1/PRECISION;

	if( with_ui ){
		bar = get_new_bar( self, "p6-balancing" );
		gtk_widget_show_all( priv->p6_page );
	}

	priv->p6_forwards = NULL;

	end_cur = ofo_dossier_get_exe_end( priv->dossier );
	begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );

	sld_ope = ofo_dossier_get_sld_ope( priv->dossier );
	sld_template = ofo_ope_template_get_by_mnemo( priv->hub, sld_ope );
	g_return_val_if_fail( sld_template && OFO_IS_OPE_TEMPLATE( sld_template ), 1 );

	for_ope = ofo_dossier_get_forward_ope( priv->dossier );
	for_template = ofo_ope_template_get_by_mnemo( priv->hub, for_ope );
	g_return_val_if_fail( for_template && OFO_IS_OPE_TEMPLATE( for_template ), 1 );

	for( i=1, it=accounts ; it ; ++i, it=it->next ){
		account = OFO_ACCOUNT( it->data );
		debit = ofo_account_get_val_debit( account );
		credit = ofo_account_get_val_credit( account );

		if( fabs( debit-credit ) > precision ){

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
			if( debit > credit ){
				detail->credit = debit-credit;
				detail->credit_user_set = TRUE;
			} else {
				detail->debit = credit-debit;
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
				if( debit > credit ){
					detail->debit = debit-credit;
					detail->debit_user_set = TRUE;
				} else {
					detail->credit = credit-debit;
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

		progress = ( gdouble ) i / ( gdouble ) count;
		text = g_strdup_printf( "%u/%u", i, count );

		if( with_ui ){
			g_signal_emit_by_name( bar, "ofa-double", progress );
			g_signal_emit_by_name( bar, "ofa-text", text );
		}

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );
		g_free( text );
	}

	ofo_account_free_dataset( accounts );

	if( errors ){
		msg = g_strdup_printf(
				_( "%d errors have been found while computing accounts soldes" ), errors );
		my_utils_dialog_warning( msg );
		g_free( msg );
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
		my_assistant_set_page_complete( MY_ASSISTANT( self ), TRUE );
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
	gint count, i;
	gdouble progress;
	gchar *text;
	const GDate *end_cur;
	ofoLedger *ledger;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	ledgers = ofo_ledger_get_dataset( priv->hub );
	count = g_list_length( ledgers );
	bar = get_new_bar( self, "p6-ledgers" );
	gtk_widget_show_all( priv->p6_page );

	end_cur = ofo_dossier_get_exe_end( priv->dossier );

	for( i=1, it=ledgers ; it ; ++i, it=it->next ){
		ledger = OFO_LEDGER( it->data );
		ofo_ledger_close( ledger, end_cur );

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "ofa-double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "ofa-text", text );

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );

		g_free( text );
	}

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

	priv = self->priv;
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
	ofaIDBConnect *cnx;
	ofaIDBPeriod *period;
	gboolean ok;
	const GDate *begin_old, *end_old;
	const GDate *begin_next, *end_next;
	gchar *str, *cur_account, *cur_password;
	ofaIDBProvider *provider;
	GtkApplication *application;
	ofaHub *hub;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;

	ofo_dossier_set_current( priv->dossier, FALSE );
	ofo_dossier_update( priv->dossier );
	ofa_main_window_update_title( priv->main_window );

	period = ofa_idbconnect_get_period( priv->connect );
	begin_old = ofo_dossier_get_exe_begin( priv->dossier );
	end_old = ofo_dossier_get_exe_end( priv->dossier );
	ofa_idbmeta_update_period( priv->meta, period, FALSE, begin_old, end_old );
	g_object_unref( period );

	begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p1_begin_next ), NULL );
	end_next = my_editable_date_get_date( GTK_EDITABLE( priv->p1_end_next ), NULL );
	ok = ofa_idbconnect_archive_and_new(
				priv->connect, priv->p2_account, priv->p2_password, begin_next, end_next );

	if( !ok ){
		my_utils_dialog_warning( _( "Unable to archive the dossier" ));
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
		my_assistant_set_page_complete( MY_ASSISTANT( self ), TRUE );

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
			str = g_strdup( _( "Unable to open a connection on the new exercice" ));
			my_utils_dialog_warning( str );
			g_free( str );
			my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
			my_assistant_set_page_complete( MY_ASSISTANT( self ), TRUE );

		} else {
			application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
			g_return_val_if_fail( application && OFA_IS_IHUBBER( application ), FALSE );
			hub = ofa_ihubber_new_hub( OFA_IHUBBER( application ), cnx );
			if( hub ){
				priv->dossier = ofa_hub_get_dossier( hub );
				priv->connect = ofa_hub_get_connect( hub );
				ofo_dossier_set_current( priv->dossier, TRUE );
				ofo_dossier_set_exe_begin( priv->dossier, begin_next );
				ofo_dossier_set_exe_end( priv->dossier, end_next );
				ofo_dossier_update( priv->dossier );

				/* re-emit the opened signal after changes */
				g_signal_emit_by_name( priv->main_window, OFA_SIGNAL_DOSSIER_OPENED, priv->dossier );
				ofa_main_window_update_title( priv->main_window );
			}
		}

		g_object_unref( cnx );
	}

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

	priv = self->priv;

	query = g_strdup( "TRUNCATE TABLE OFA_T_AUDIT" );
	ok = ofa_idbconnect_query( priv->connect, query, TRUE );
	g_free( query );

	/* archive deleted (non-reported) entries
	 * i.e. those which are tight to an unsettleable or an unreconcilable
	 * account, or are not settled, or are not reconciliated
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS OFA_T_KEEP_ENTRIES" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup_printf( "CREATE TABLE OFA_T_KEEP_ENTRIES "
					"SELECT ENT_NUMBER FROM OFA_T_ENTRIES,OFA_T_ACCOUNTS "
					"	WHERE ENT_ACCOUNT=ACC_NUMBER AND ("
					"		(ACC_SETTLEABLE='S' AND ENT_STLMT_NUMBER IS NULL) OR "
					"		(ACC_RECONCILIABLE='R' AND ENT_NUMBER NOT IN ("
					"			SELECT REC_IDS_OTHER FROM OFA_T_CONCIL_IDS WHERE REC_IDS_TYPE='E'))) AND "
					"		ENT_STATUS!=%d AND ENT_STATUS!=%d",
					ENT_STATUS_DELETED, ENT_STATUS_FUTURE );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS OFA_T_DELETED_ENTRIES" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE OFA_T_DELETED_ENTRIES "
					"SELECT * FROM OFA_T_ENTRIES WHERE "
					"	ENT_NUMBER NOT IN (SELECT ENT_NUMBER FROM OFA_T_KEEP_ENTRIES)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_ENTRIES "
					"WHERE ENT_NUMBER NOT IN (SELECT ENT_NUMBER FROM OFA_T_KEEP_ENTRIES)" );
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
		query = g_strdup( "DROP TABLE IF EXISTS OFA_T_KEEP_BATS" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE OFA_T_KEEP_BATS "
					"SELECT DISTINCT(BAT_ID) FROM OFA_T_BAT_LINES "
					"	WHERE BAT_LINE_ID NOT IN "
					"		(SELECT REC_IDS_OTHER FROM OFA_T_CONCIL_IDS "
					"			WHERE REC_IDS_TYPE='B')" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS OFA_T_DELETED_BATS" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE OFA_T_DELETED_BATS "
					"SELECT * FROM OFA_T_BAT "
					"	WHERE BAT_ID NOT IN (SELECT BAT_ID FROM OFA_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS OFA_T_DELETED_BAT_LINES" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE OFA_T_DELETED_BAT_LINES "
					"SELECT * FROM OFA_T_BAT_LINES "
					"	WHERE BAT_ID NOT IN (SELECT BAT_ID FROM OFA_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_BAT "
					"WHERE BAT_ID NOT IN (SELECT BAT_ID FROM OFA_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_BAT_LINES "
					"WHERE BAT_ID NOT IN (SELECT BAT_ID FROM OFA_T_KEEP_BATS)" );
		ok = ofa_idbconnect_query( priv->connect, query, TRUE );
		g_free( query );
	}

	/* reset to zero accounts and ledgers balances
	 */
	if( ok ){
		query = g_strdup( "UPDATE OFA_T_ACCOUNTS SET "
					"ACC_VAL_DEBIT=0, ACC_VAL_CREDIT=0, "
					"ACC_ROUGH_DEBIT=0, ACC_ROUGH_CREDIT=0, "
					"ACC_OPEN_DEBIT=0, ACC_OPEN_CREDIT=0" );
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
		my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
		my_assistant_set_page_complete( MY_ASSISTANT( self ), TRUE );
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
	gint count, i;
	GList *it;
	ofoEntry *entry;
	ofoAccount *account;
	ofxCounter counter;
	gdouble progress;
	gchar *text;
	const GDate *dbegin;

	priv = self->priv;

	dbegin = ofo_dossier_get_exe_begin( priv->dossier );

	bar = get_new_bar( self, "p6-forward" );
	gtk_widget_show_all( priv->p6_page );

	count = g_list_length( priv->p6_forwards );

	for( i=1, it=priv->p6_forwards ; it ; ++i, it=it->next ){
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
				SIGNAL_HUB_ENTRY_STATUS_CHANGE, entry, ENT_STATUS_ROUGH, ENT_STATUS_VALIDATED );

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "ofa-double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "ofa-text", text );

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );

		g_free( text );
	}

	ofo_entry_free_dataset( priv->p6_forwards );

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
	gint count, i;
	GList *accounts, *it;
	ofoAccount *account;
	gdouble progress;
	gchar *text;

	priv = self->priv;

	accounts = ofo_account_get_dataset( priv->hub );
	count = g_list_length( accounts );

	bar = get_new_bar( self, "p6-open" );
	gtk_widget_show_all( priv->p6_page );

	for( i=1, it=accounts ; it ; ++i, it=it->next ){
		account = OFO_ACCOUNT( it->data );

		ofo_account_archive_open_balances( account );

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "ofa-double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "ofa-text", text );

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );

		g_free( text );
	}

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
	gint count, i;
	GList *entries, *it;
	ofoEntry *entry;
	gdouble progress;
	gchar *text;
	GtkWidget *label;
	const GDate *dos_dend, *ent_deffect;

	priv = self->priv;

	dos_dend = ofo_dossier_get_exe_end( priv->dossier );

	entries = ofo_entry_get_dataset_for_exercice_by_status( priv->hub, ENT_STATUS_FUTURE );
	count = g_list_length( entries );

	bar = get_new_bar( self, "p6-future" );
	gtk_widget_show_all( priv->p6_page );

	for( i=1, it=entries ; it ; ++i, it=it->next ){
		entry = OFO_ENTRY( it->data );
		ent_deffect = ofo_entry_get_deffect( entry );

		if( my_date_compare( ent_deffect, dos_dend ) <= 0 ){
			g_signal_emit_by_name( priv->hub,
					SIGNAL_HUB_ENTRY_STATUS_CHANGE, entry, ENT_STATUS_FUTURE, ENT_STATUS_ROUGH );
		}

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "ofa-double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "ofa-text", text );

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );

		g_free( text );
	}
	if( !count ){
		g_signal_emit_by_name( bar, "ofa-text", "0/0" );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->p6_page ), "p6-summary" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );

	gtk_label_set_text( GTK_LABEL( label ),
			_( "The previous exercice has been successfully closed.\n"
				"The next exercice has been automatically defined and opened." ));

	my_assistant_set_page_type( MY_ASSISTANT( self ), GTK_ASSISTANT_PAGE_SUMMARY );
	my_assistant_set_page_complete( MY_ASSISTANT( self ), TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

static myProgressBar *
get_new_bar( ofaExerciceCloseAssistant *self, const gchar *w_name )
{
	GtkAssistant *toplevel;
	GtkWidget *parent;
	myProgressBar *bar;

	toplevel = my_assistant_get_assistant( MY_ASSISTANT( self ));
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );
	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( bar ));

	return( bar );
}
