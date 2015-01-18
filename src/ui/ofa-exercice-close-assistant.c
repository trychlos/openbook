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
#include <glib/gstdio.h>
#include <math.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-dbms.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"
#include "api/ofs-ope.h"

#include "core/my-window-prot.h"
#include "core/ofa-dbms-root-bin.h"
#include "core/ofa-preferences.h"

#include "ui/my-assistant.h"
#include "ui/my-editable-date.h"
#include "ui/my-progress-bar.h"
#include "ui/ofa-balances-grid.h"
#include "ui/ofa-closing-parms-bin.h"
#include "ui/ofa-exercice-close-assistant.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-misc-chkbal.h"

/* private instance data
 */
struct _ofaExerciceCloseAssistantPrivate {

	GtkWidget          *current_page_widget;

	/* dossier
	 */
	const gchar        *dname;
	gchar              *provider;
	ofaIDbms           *dbms;
	const gchar        *cur_account;
	const gchar        *cur_password;

	/* p2 - closing parms
	 */
	GtkWidget          *p2_begin_cur;
	GtkWidget          *p2_end_cur;
	GtkWidget          *p2_begin_next;
	GtkWidget          *p2_end_next;
	ofaClosingParmsBin *p2_closing_parms;

	/* p3 - get DBMS root credentials
	 */
	ofaDBMSRootBin     *p3_dbms_credentials;
	gchar              *p3_account;
	gchar              *p3_password;

	/* p4 - checking that entries, accounts and ledgers are balanced
	 */
	gboolean            p4_entries_ok;
	GList              *p4_entries_list;		/* entry balances per currency */
	gboolean            p4_ledgers_ok;
	GList              *p4_ledgers_list;		/* ledger balances per currency */
	gboolean            p4_accounts_ok;
	GList              *p4_accounts_list;		/* account balances per currency */
	gboolean            p4_result;
	gboolean            p4_done;

	/* p5 - confirmation page
	 */

	/* p6 - close the exercice
	 */
	GList              *p6_forwards;			/* forward operations */
	GList              *p6_cleanup;
	GList              *p6_unreconciliated;
};

/* the pages of this assistant
 * note that pages are numbered from zero by GtkAssistant,
 * but from 1 by GtkBuilder !
 * So our page_names are numbered from 1 to stay sync with widgets
 */
enum {
	PAGE_INTRO = 0,						/* Intro */
	PAGE_PARMS,							/* p2 - Content */
	PAGE_DBMS,							/* p3 - Content */
	PAGE_CHECKS,						/* p4 - Progress */
	PAGE_CONFIRM,						/* p5 - Confirm */
	PAGE_CLOSE,							/* p6 - Progress then Summary */
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-exercice-close-assistant.ui";
static const gchar *st_ui_id            = "ExerciceCloseAssistant";

G_DEFINE_TYPE( ofaExerciceCloseAssistant, ofa_exercice_close_assistant, MY_TYPE_ASSISTANT )

static void             p1_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p2_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p2_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p2_on_date_changed( GtkEditable *editable, ofaExerciceCloseAssistant *self );
static void             p2_on_closing_parms_changed( ofaClosingParmsBin *bin, ofaExerciceCloseAssistant *self );
static void             p2_check_for_complete( ofaExerciceCloseAssistant *self );
static void             p2_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p3_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p3_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p3_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaExerciceCloseAssistant *self );
static void             p3_check_for_complete( ofaExerciceCloseAssistant *self );
static void             p3_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p4_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static void             p4_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static gboolean         p4_check_entries_balance( ofaExerciceCloseAssistant *self );
static gboolean         p4_check_ledgers_balance( ofaExerciceCloseAssistant *self );
static gboolean         p4_check_accounts_balance( ofaExerciceCloseAssistant *self );
static myProgressBar   *get_new_bar( ofaExerciceCloseAssistant *self, const gchar *w_name );
static ofaBalancesGrid *p4_get_new_balances( ofaExerciceCloseAssistant *self, const gchar *w_name );
static void             p4_check_status( ofaExerciceCloseAssistant *self, gboolean ok, const gchar *w_name );
static gboolean         p4_info_checks( ofaExerciceCloseAssistant *self );
static void             p6_do_close( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget );
static gboolean         p6_validate_entries( ofaExerciceCloseAssistant *self );
static gboolean         p6_solde_accounts( ofaExerciceCloseAssistant *self );
static gint             p6_do_solde_accounts( ofaExerciceCloseAssistant *self, gboolean with_ui );
static void             set_forward_settlement_number( GList *entries, const gchar *account, ofxCounter counter );
static gboolean         p6_close_ledgers( ofaExerciceCloseAssistant *self );
static gboolean         p6_archive_exercice( ofaExerciceCloseAssistant *self );
static gboolean         p6_do_archive_exercice( ofaExerciceCloseAssistant *self, gboolean with_ui );
static gboolean         p6_cleanup( ofaExerciceCloseAssistant *self );
static gboolean         p6_forward( ofaExerciceCloseAssistant *self );
static gboolean         p6_open( ofaExerciceCloseAssistant *self );
static gboolean         p6_future( ofaExerciceCloseAssistant *self );

static const ofsAssistant st_pages_cb [] = {
		{ PAGE_INTRO,
				NULL,
				NULL,
				( myAssistantCb ) p1_do_forward },
		{ PAGE_PARMS,
				( myAssistantCb ) p2_do_init,
				( myAssistantCb ) p2_display,
				( myAssistantCb ) p2_do_forward },
		{ PAGE_DBMS,
				( myAssistantCb ) p3_do_init,
				( myAssistantCb ) p3_display,
				( myAssistantCb ) p3_do_forward },
		{ PAGE_CHECKS,
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

	g_free( priv->provider );
	g_free( priv->p3_account );
	g_free( priv->p3_password );

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

		g_clear_object( &priv->dbms );
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
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	my_assistant_set_callbacks( MY_ASSISTANT( self ), st_pages_cb );

	my_assistant_run( MY_ASSISTANT( self ));
}

/*
 * get some dossier data
 */
static void
p1_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p1_do_forward";
	ofaExerciceCloseAssistantPrivate *priv;
	ofoDossier *dossier;

	g_debug( "%s: self=%p, page_num=%d, page_widget=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;

	priv->dname = ofo_dossier_get_name( dossier );
	priv->provider = ofa_settings_get_dossier_provider( priv->dname );
	if( !priv->provider ){
		g_warning( "%s: unable to get dossier provider", thisfn );

	} else {
		priv->dbms = ofa_idbms_get_provider_by_name( priv->provider );
		if( !priv->dbms ){
			g_warning( "%s: unable to access to '%s' provider", thisfn, priv->provider );

		} else {
			ofa_main_window_get_dossier_credentials(
					MY_WINDOW( self )->prot->main_window, &priv->cur_account, &priv->cur_password );
		}
	}
}

static void
p2_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *parent;
	ofoDossier *dossier;
	const GDate *begin_cur, *end_cur;
	GDate begin, end;
	gint exe_length;

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;
	exe_length = ofo_dossier_get_exe_length( dossier );

	priv->p2_begin_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-closing-begin" );
	g_return_if_fail( priv->p2_begin_cur && GTK_IS_ENTRY( priv->p2_begin_cur ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_begin_cur ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_begin_cur ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_begin_cur ), TRUE );
	begin_cur = ofo_dossier_get_exe_begin( dossier );
	my_editable_date_set_date( GTK_EDITABLE( priv->p2_begin_cur ), begin_cur );
	g_signal_connect( G_OBJECT( priv->p2_begin_cur ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	priv->p2_end_cur = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-closing-end" );
	g_return_if_fail( priv->p2_end_cur && GTK_IS_ENTRY( priv->p2_end_cur ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_end_cur ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_end_cur ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_end_cur ), TRUE );
	end_cur = ofo_dossier_get_exe_end( dossier );
	my_editable_date_set_date( GTK_EDITABLE( priv->p2_end_cur ), end_cur );
	g_signal_connect( G_OBJECT( priv->p2_end_cur ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	/* set a date if the other is valid */
	if( !my_date_is_valid( begin_cur ) && my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &begin, end_cur );
		g_date_subtract_months( &begin, exe_length );
		g_date_add_days( &begin, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_begin_cur ), &begin );
		my_date_set_from_date( &end, end_cur );

	} else if( my_date_is_valid( begin_cur ) && !my_date_is_valid( end_cur ) && exe_length > 0 ){
		my_date_set_from_date( &end, begin_cur );
		g_date_add_months( &end, exe_length );
		g_date_subtract_days( &end, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_end_cur ), &end );

	} else if( my_date_is_valid( end_cur )){
		my_date_set_from_date( &end, end_cur );
	}

	priv->p2_begin_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-next-begin" );
	g_return_if_fail( priv->p2_begin_next && GTK_IS_ENTRY( priv->p2_begin_next ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_begin_next ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_begin_next ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_begin_next ), TRUE );
	g_signal_connect( G_OBJECT( priv->p2_begin_next ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	if( my_date_is_valid( &end )){
		my_date_set_from_date( &begin, &end );
		g_date_add_days( &begin, 1 );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_begin_next ), &begin );
	}

	priv->p2_end_next = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-next-end" );
	g_return_if_fail( priv->p2_end_next && GTK_IS_ENTRY( priv->p2_end_next ));
	my_editable_date_init( GTK_EDITABLE( priv->p2_end_next ));
	my_editable_date_set_format( GTK_EDITABLE( priv->p2_end_next ), ofa_prefs_date_display());
	my_editable_date_set_mandatory( GTK_EDITABLE( priv->p2_end_next ), TRUE );
	g_signal_connect( G_OBJECT( priv->p2_end_next ), "changed", G_CALLBACK( p2_on_date_changed ), self );

	if( my_date_is_valid( &end ) && exe_length > 0 ){
		g_date_add_months( &end, exe_length );
		my_editable_date_set_date( GTK_EDITABLE( priv->p2_end_next ), &end );
	}

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p2-forward-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p2_closing_parms = ofa_closing_parms_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->p2_closing_parms ));
	ofa_closing_parms_bin_set_main_window(
			priv->p2_closing_parms, MY_WINDOW( self )->prot->main_window );
	g_signal_connect( G_OBJECT( priv->p2_closing_parms ), "changed", G_CALLBACK( p2_on_closing_parms_changed ), self );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page_widget, FALSE );
}

/*
 * check if the page is validable
 */
static void
p2_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	p2_check_for_complete( self );
}

/*
 * try to set some default values
 */
static void
p2_on_date_changed( GtkEditable *editable, ofaExerciceCloseAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_on_closing_parms_changed( ofaClosingParmsBin *bin, ofaExerciceCloseAssistant *self )
{
	p2_check_for_complete( self );
}

static void
p2_check_for_complete( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkAssistant *assistant;
	gint page_num;
	GtkWidget *page_widget;
	gboolean complete;
	const GDate *begin_cur, *end_cur, *begin_next, *end_next;
	GDate date;
	gchar *msg;

	priv = self->priv;

	assistant = my_assistant_get_assistant( MY_ASSISTANT( self ));
	page_num = gtk_assistant_get_current_page( assistant );
	page_widget = gtk_assistant_get_nth_page( assistant, page_num );

	complete = FALSE;

	if( priv->p2_end_next ){
		begin_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_cur ), NULL );
		end_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_cur ), NULL );
		begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_next ), NULL );
		end_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_next ), NULL );

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

	if( priv->p2_closing_parms ){
		complete &= ofa_closing_parms_bin_is_valid( priv->p2_closing_parms, &msg );
		g_free( msg );
	}

	gtk_assistant_set_page_complete( assistant, page_widget, complete );
}

/*
 * as all parameters have been checked ok, save in dossier
 */
static void
p2_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;
	const GDate *begin_cur, *end_cur;
	ofoDossier *dossier;

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;

	begin_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_cur ), NULL );
	end_cur = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_cur ), NULL );

	ofo_dossier_set_exe_begin( dossier, begin_cur );
	ofo_dossier_set_exe_end( dossier, end_cur );
	ofa_dossier_misc_set_current( priv->dname, begin_cur, end_cur );
	ofa_main_window_update_title( MY_WINDOW( self )->prot->main_window );

	ofa_closing_parms_bin_apply( priv->p2_closing_parms );

	ofo_dossier_update( dossier );
}

static void
p3_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p3_do_init";
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *parent;
	const gchar *dname;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	priv = self->priv;
	priv->current_page_widget = page_widget;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( page_widget ), "p3-dbms" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->p3_dbms_credentials = ofa_dbms_root_bin_new();
	ofa_dbms_root_bin_attach_to( priv->p3_dbms_credentials, GTK_CONTAINER( parent ), NULL );
	dname = ofo_dossier_get_name( MY_WINDOW( self )->prot->dossier );
	ofa_dbms_root_bin_set_dossier( priv->p3_dbms_credentials, dname );

	if( priv->p3_account && priv->p3_password ){
		ofa_dbms_root_bin_set_credentials(
				priv->p3_dbms_credentials, priv->p3_account, priv->p3_password );
	}

	g_signal_connect( priv->p3_dbms_credentials, "changed", G_CALLBACK( p3_on_dbms_root_changed ), self );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page_widget, FALSE );
}

static void
p3_display( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	p3_check_for_complete( self );
}

static void
p3_on_dbms_root_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;

	g_free( priv->p3_account );
	priv->p3_account = g_strdup( account );

	g_free( priv->p3_password );
	priv->p3_password = g_strdup( password );

	p3_check_for_complete( self );
}

static void
p3_check_for_complete( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	gboolean ok;

	priv = self->priv;

	ok = ofa_dbms_root_bin_is_valid( priv->p3_dbms_credentials );

	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_widget, ok );
}

static void
p3_do_forward( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
}

static void
p4_do_init( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;

	priv->p4_done = FALSE;
	priv->current_page_widget = page_widget;
}

/*
 * begins the checks before exercice closing
 */
static void
p4_checks( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	ofaExerciceCloseAssistantPrivate *priv;

	priv = self->priv;

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page_widget, priv->p4_done );

	if( !priv->p4_done ){

		priv->current_page_widget = page_widget;
		priv->p4_entries_ok = FALSE;
		priv->p4_ledgers_ok = FALSE;
		priv->p4_accounts_ok = FALSE;

		g_idle_add(( GSourceFunc ) p4_check_entries_balance, self );
	}
}

/*
 * 1/ check that entries are balanced per currency
 */
static gboolean
p4_check_entries_balance( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;

	priv = self->priv;

	bar = get_new_bar( self, "p4-entry-parent" );
	grid = p4_get_new_balances( self, "p4-entry-bals" );
	gtk_widget_show_all( priv->current_page_widget );

	priv->p4_entries_ok = ofa_misc_chkbalent_run(
			MY_WINDOW( self )->prot->dossier, &priv->p4_entries_list, bar, grid );

	p4_check_status( self, priv->p4_entries_ok, "p4-entry-ok" );

	/* next: check for ledgers balances */
	g_idle_add(( GSourceFunc ) p4_check_ledgers_balance, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * 2/ check that ledgers are balanced per currency
 */
static gboolean
p4_check_ledgers_balance( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;

	priv = self->priv;

	bar = get_new_bar( self, "p4-ledger-parent" );
	grid = p4_get_new_balances( self, "p4-ledger-bals" );
	gtk_widget_show_all( priv->current_page_widget );

	priv->p4_ledgers_ok = ofa_misc_chkballed_run(
			MY_WINDOW( self )->prot->dossier, &priv->p4_ledgers_list, bar, grid );

	p4_check_status( self, priv->p4_ledgers_ok, "p4-ledger-ok" );

	/* next: check for accounts balances */
	g_idle_add(( GSourceFunc ) p4_check_accounts_balance, self );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * 3/ check that accounts are balanced per currency
 */
static gboolean
p4_check_accounts_balance( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	ofaBalancesGrid *grid;
	gboolean complete;

	priv = self->priv;

	bar = get_new_bar( self, "p4-account-parent" );
	grid = p4_get_new_balances( self, "p4-account-bals" );
	gtk_widget_show_all( priv->current_page_widget );

	priv->p4_accounts_ok = ofa_misc_chkbalacc_run(
			MY_WINDOW( self )->prot->dossier, &priv->p4_accounts_list, bar, grid );

	p4_check_status( self, priv->p4_accounts_ok, "p4-account-ok" );

	/* next: if all checks complete and ok ?
	 * set priv->p4_done */
	complete = p4_info_checks( self );
	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_widget, complete );

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

static ofaBalancesGrid *
p4_get_new_balances( ofaExerciceCloseAssistant *self, const gchar *w_name )
{
	GtkWidget *parent;
	GtkAssistant *toplevel;
	ofaBalancesGrid *grid;

	toplevel = my_assistant_get_assistant( MY_ASSISTANT( self ));
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), w_name );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), FALSE );
	grid = ofa_balances_grid_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( grid ));

	return( grid );
}

/*
 * display OK/NOT OK for a single balance check
 */
static void
p4_check_status( ofaExerciceCloseAssistant *self, gboolean ok, const gchar *w_name )
{
	GtkAssistant *toplevel;
	GtkWidget *label;
	GdkRGBA color;

	toplevel = my_assistant_get_assistant( MY_ASSISTANT( self ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), w_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	gdk_rgba_parse( &color, ok ? "#000000" : "#ff0000" );

	if( ok ){
		gtk_label_set_text( GTK_LABEL( label ), _( "OK" ));

	} else {
		gtk_label_set_text( GTK_LABEL( label ), _( "NOT OK" ));
	}

	gtk_widget_override_color( label, GTK_STATE_FLAG_NORMAL, &color );
}

/*
 * after the end of individual checks (entries, ledgers, accounts)
 * check that the balances are the sames
 */
static gboolean
p4_info_checks( ofaExerciceCloseAssistant *self )
{
	ofaExerciceCloseAssistantPrivate *priv;
	GtkWidget *dialog, *label;

	priv = self->priv;

	priv->p4_result = priv->p4_entries_ok && priv->p4_ledgers_ok && priv->p4_accounts_ok;
	priv->p4_done = TRUE;

	if( !priv->p4_result ){
		dialog = gtk_message_dialog_new(
				my_window_get_toplevel( MY_WINDOW( self )),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_CLOSE,
				"%s", _( "We have detected losses of balance in your books.\n\n"
						"In this current state, we are unable to close this exercice\n"
						"until you fix your balances." ));

		gtk_dialog_run( GTK_DIALOG( dialog ));
		gtk_widget_destroy( dialog );

	} else {
		priv->p4_result = ofa_misc_chkbalsame_run(
				priv->p4_entries_list, priv->p4_ledgers_list, priv->p4_accounts_list );

		label = my_utils_container_get_child_by_name(
						GTK_CONTAINER( priv->current_page_widget ), "p4-label-end" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );

		if( priv->p4_result ){
			gtk_label_set_text( GTK_LABEL( label ),
					_( "Your books are rightly balanced. Good !" ));

		} else {
			gtk_label_set_text( GTK_LABEL( label ),
					_( "\nThough each book is individually balanced, it appears "
						"that some distorsion has happended among them.\n"
						"In this current state, we are unable to close this exercice "
						"until you fix your balances." ));
		}
	}

	ofa_misc_chkbal_free( priv->p4_entries_list );
	priv->p4_entries_list = NULL;

	ofa_misc_chkbal_free( priv->p4_ledgers_list );
	priv->p4_ledgers_list = NULL;

	ofa_misc_chkbal_free( priv->p4_accounts_list );
	priv->p4_accounts_list = NULL;

	return( priv->p4_result );
}

static void
p6_do_close( ofaExerciceCloseAssistant *self, gint page_num, GtkWidget *page_widget )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_close";
	ofaExerciceCloseAssistantPrivate *priv;

	g_debug( "%s: self=%p, page_num=%d, page=%p (%s)",
			thisfn, ( void * ) self, page_num, ( void * ) page_widget, G_OBJECT_TYPE_NAME( page_widget ));

	my_assistant_set_page_complete( MY_ASSISTANT( self ), page_widget, FALSE );

	priv = self->priv;
	priv->current_page_widget = page_widget;

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
	ofoDossier *dossier;
	GList *entries, *it;
	myProgressBar *bar;
	gint count, i;
	gdouble progress;
	gchar *text, *sstart, *send;
	gulong udelay;
	GTimeVal stamp_start, stamp_end;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;

	entries = ofo_entry_get_dataset_for_exercice_by_status( dossier, ENT_STATUS_ROUGH );
	count = g_list_length( entries );
	my_utils_stamp_set_now( &stamp_start );

	bar = get_new_bar( self, "p6-validating" );
	gtk_widget_show_all( priv->current_page_widget );

	for( i=1, it=entries ; it ; ++i, it=it->next ){
		ofo_entry_validate( OFO_ENTRY( it->data ), dossier );

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
	p6_do_solde_accounts( self, TRUE );

	g_idle_add(( GSourceFunc ) p6_close_ledgers, self );

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
 * as settled, being balanced with the corresponding solde entry.
 */
static gint
p6_do_solde_accounts( ofaExerciceCloseAssistant *self, gboolean with_ui )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_do_solde_accounts";
	ofaExerciceCloseAssistantPrivate *priv;
	ofoDossier *dossier;
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
	dossier = MY_WINDOW( self )->prot->dossier;
	accounts = ofo_account_get_dataset_for_solde( dossier );
	count = g_list_length( accounts );
	precision = 1/PRECISION;

	if( with_ui ){
		bar = get_new_bar( self, "p6-balancing" );
		gtk_widget_show_all( priv->current_page_widget );
	}

	priv->p6_forwards = NULL;

	end_cur = ofo_dossier_get_exe_end( dossier );
	begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_next ), NULL );

	sld_ope = ofo_dossier_get_sld_ope( dossier );
	sld_template = ofo_ope_template_get_by_mnemo( dossier, sld_ope );
	g_return_val_if_fail( sld_template && OFO_IS_OPE_TEMPLATE( sld_template ), 1 );

	for_ope = ofo_dossier_get_forward_ope( dossier );
	for_template = ofo_ope_template_get_by_mnemo( dossier, for_ope );
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

			ofs_ope_apply_template( ope, dossier );

			if( ofs_ope_is_valid( ope, dossier, &msg, &currencies )){
				sld_entries = ofs_ope_generate_entries( ope, dossier );

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
			is_ran = ofo_account_is_forward( account );
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

				ofs_ope_apply_template( ope, dossier );

				if( ofs_ope_is_valid( ope, dossier, NULL, NULL )){
					for_entries = ofs_ope_generate_entries( ope, dossier );
				}

				ofs_ope_free( ope );
			}

			/* all entries have been prepared
			 * -> set a settlement number on those which are to be
			 *    written on a settleable account - set the same
			 *    counter on the solde and the forward entries to have
			 *    an audit track
			 * -> set a reconciliation date on those which are to be
			 *    written on a reconciliable account */
			for( ite=sld_entries ; ite ; ite=ite->next ){
				entry = OFO_ENTRY( ite->data );
				ofo_entry_insert( entry, dossier );
				if( is_ran &&
						ofo_account_is_settleable( account ) &&
						!g_utf8_collate( ofo_entry_get_account( entry ), acc_number )){
					counter = ofo_dossier_get_next_settlement( dossier );
					ofo_entry_update_settlement( entry, dossier, counter );
					set_forward_settlement_number( for_entries, acc_number, counter );
				}
				if( ofo_account_is_reconciliable( account ) &&
						!g_utf8_collate( ofo_entry_get_account( entry ), acc_number )){
					ofo_entry_update_concil( entry, dossier, end_cur );
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
		my_utils_dialog_error( msg );
		g_free( msg );
		my_assistant_set_page_type( MY_ASSISTANT( self ), priv->current_page_widget, GTK_ASSISTANT_PAGE_SUMMARY );
		my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_widget, TRUE );
	}

	return( errors );
}

/*
 * set the specified settlement number on the entry for the specified
 * account - as there should only be one entry per account, we just stop
 * as soon as we have found it
 */
static void
set_forward_settlement_number( GList *entries, const gchar *account, ofxCounter counter )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_set_forward_settlement_number";
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
	ofoDossier *dossier;
	GList *ledgers, *it;
	myProgressBar *bar;
	gint count, i;
	gdouble progress;
	gchar *text;
	const GDate *end_cur;
	ofoLedger *ledger;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;
	ledgers = ofo_ledger_get_dataset( dossier );
	count = g_list_length( ledgers );
	bar = get_new_bar( self, "p6-ledgers" );
	gtk_widget_show_all( priv->current_page_widget );

	end_cur = ofo_dossier_get_exe_end( dossier );

	for( i=1, it=ledgers ; it ; ++i, it=it->next ){
		ledger = OFO_LEDGER( it->data );
		ofo_ledger_close( ledger, dossier, end_cur );

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

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->current_page_widget ), "p6-archived" );
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
	ofoDossier *dossier;
	gboolean ok;
	const GDate *begin_next, *end_next;
	ofsDossierOpen *sdo;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;
	ok = FALSE;
	dossier = MY_WINDOW( self )->prot->dossier;

	begin_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_begin_next ), NULL );
	end_next = my_editable_date_get_date( GTK_EDITABLE( priv->p2_end_next ), NULL );

	ofo_dossier_set_status( dossier, DOS_STATUS_CLOSED );
	ofo_dossier_update( dossier );
	ofa_main_window_update_title( MY_WINDOW( self )->prot->main_window );

	if( !ofa_idbms_archive(
				priv->dbms, priv->dname, priv->p3_account, priv->p3_password,
				priv->cur_account, begin_next, end_next )){
		my_utils_dialog_error( _( "Unable to archive the dossier" ));
		my_assistant_set_page_type( MY_ASSISTANT( self ), priv->current_page_widget, GTK_ASSISTANT_PAGE_SUMMARY );
		my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_widget, TRUE );

	} else {
		/* open the new exercice */
		sdo = g_new0( ofsDossierOpen, 1 );
		sdo->dname = g_strdup( priv->dname );
		sdo->account = g_strdup( priv->cur_account );
		sdo->password = g_strdup( priv->cur_password );
		g_signal_emit_by_name( MY_WINDOW( self )->prot->main_window, OFA_SIGNAL_DOSSIER_OPEN, sdo );

		dossier = ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window );
		if( dossier && OFO_IS_DOSSIER( dossier )){
			ofo_dossier_set_status( dossier, DOS_STATUS_OPENED );
			ofo_dossier_set_exe_begin( dossier, begin_next );
			ofo_dossier_set_exe_end( dossier, end_next );
			ofo_dossier_update( dossier );
			ofa_main_window_update_title( MY_WINDOW( self )->prot->main_window );

			ok = TRUE;
		}
	}

	return( ok );
}

/*
 * erase audit table
 * remove settled entries on settleable accounts
 * remove reconciliated entries on reconciliable accounts
 * remove all entries on unsettleable or unreconciliable accounts
 * update remaining entries status to PAST
 * reset all account and ledger balances to zero
 */
static gboolean
p6_cleanup( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_cleanup";
	ofaExerciceCloseAssistantPrivate *priv;
	ofoDossier *dossier;
	gchar *query;
	const ofaDbms *dbms;
	gboolean ok;
	GtkWidget *label;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	priv = self->priv;
	dossier = ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	dbms = ofo_dossier_get_dbms( dossier );
	g_return_val_if_fail( dbms && OFA_IS_DBMS( dbms ), FALSE );

	query = g_strdup( "TRUNCATE TABLE OFA_T_AUDIT" );
	ok = ofa_dbms_query( dbms, query, TRUE );
	g_free( query );

	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS OFA_T_DELETED_ENTRIES" );
		ok = ofa_dbms_query( dbms, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup_printf( "CREATE TABLE OFA_T_DELETED_ENTRIES "
					"SELECT * FROM OFA_T_ENTRIES,OFA_T_ACCOUNTS WHERE "
					"	ENT_ACCOUNT=ACC_NUMBER AND "
					"	(ACC_SETTLEABLE IS NULL OR ENT_STLMT_NUMBER IS NOT NULL) AND "
					"	(ACC_RECONCILIABLE IS NULL OR ENT_CONCIL_DVAL IS NOT NULL) AND"
					"	ENT_STATUS!=%d", ENT_STATUS_FUTURE );
		ok = ofa_dbms_query( dbms, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "DELETE FROM OFA_T_ENTRIES "
					"WHERE ENT_NUMBER IN (SELECT ENT_NUMBER FROM OFA_T_DELETED_ENTRIES)" );
		ok = ofa_dbms_query( dbms, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup_printf( "UPDATE OFA_T_ENTRIES SET "
					"ENT_STATUS=%d WHERE ENT_STATUS!=%d", ENT_STATUS_PAST, ENT_STATUS_FUTURE );
		ok = ofa_dbms_query( dbms, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "UPDATE OFA_T_ACCOUNTS SET "
					"ACC_VAL_DEBIT=0, ACC_VAL_CREDIT=0, "
					"ACC_ROUGH_DEBIT=0, ACC_ROUGH_CREDIT=0, "
					"ACC_OPEN_DEBIT=0, ACC_OPEN_CREDIT=0" );
		ok = ofa_dbms_query( dbms, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup( "UPDATE OFA_T_LEDGERS_CUR SET "
					"LED_CUR_VAL_DEBIT=0, LED_CUR_VAL_CREDIT=0, "
					"LED_CUR_ROUGH_DEBIT=0, LED_CUR_ROUGH_CREDIT=0" );
		ok = ofa_dbms_query( dbms, query, TRUE );
		g_free( query );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->current_page_widget ), "p6-cleanup" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );
	gtk_label_set_text( GTK_LABEL( label ), ok ? _( "Done" ) : _( "Error" ));

	if( ok ){
		g_idle_add(( GSourceFunc ) p6_forward, self );

	} else {
		my_assistant_set_page_type( MY_ASSISTANT( self ), priv->current_page_widget, GTK_ASSISTANT_PAGE_SUMMARY );
		my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_widget, TRUE );
	}

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * generate carried forward entries
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
	ofoDossier *dossier;
	ofxCounter counter;
	gdouble progress;
	gchar *text;

	priv = self->priv;
	dossier = ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window );

	bar = get_new_bar( self, "p6-forward" );
	gtk_widget_show_all( priv->current_page_widget );

	count = g_list_length( priv->p6_forwards );

	for( i=1, it=priv->p6_forwards ; it ; ++i, it=it->next ){
		entry = OFO_ENTRY( it->data );
		ofo_entry_insert( entry, dossier );
		counter = ofo_entry_get_settlement_number( entry );
		if( counter ){
			ofo_entry_update_settlement( entry, dossier, counter );
		}

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
 */
static gboolean
p6_open( ofaExerciceCloseAssistant *self )
{
	static const gchar *thisfn = "ofa_exercice_close_assistant_p6_open";
	ofaExerciceCloseAssistantPrivate *priv;
	myProgressBar *bar;
	gint count, i;
	GList *accounts, *it;
	ofoDossier *dossier;
	ofoAccount *account;
	gdouble progress;
	gchar *text;

	priv = self->priv;
	dossier = ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window );
	accounts = ofo_account_get_dataset( dossier );
	count = g_list_length( accounts );

	bar = get_new_bar( self, "p6-open" );
	gtk_widget_show_all( priv->current_page_widget );

	for( i=1, it=accounts ; it ; ++i, it=it->next ){
		account = OFO_ACCOUNT( it->data );

		ofo_account_archive_open_balance( account, dossier );

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
	ofoDossier *dossier;
	myProgressBar *bar;
	gint count, i;
	GList *entries, *it;
	ofoEntry *entry;
	gdouble progress;
	gchar *text;
	GtkWidget *label;

	priv = self->priv;
	dossier = ofa_main_window_get_dossier( MY_WINDOW( self )->prot->main_window );
	entries = ofo_entry_get_dataset_for_exercice_by_status( dossier, ENT_STATUS_FUTURE );
	count = g_list_length( entries );

	bar = get_new_bar( self, "p6-future" );
	gtk_widget_show_all( priv->current_page_widget );

	for( i=1, it=entries ; it ; ++i, it=it->next ){
		entry = OFO_ENTRY( it->data );
		ofo_entry_future_to_rough( entry, dossier );

		progress = ( gdouble ) i / ( gdouble ) count;
		g_signal_emit_by_name( bar, "ofa-double", progress );

		text = g_strdup_printf( "%u/%u", i, count );
		g_signal_emit_by_name( bar, "ofa-text", text );

		g_debug( "%s: progress=%.5lf, text=%s", thisfn, progress, text );

		g_free( text );
	}
	if( !count ){
		g_signal_emit_by_name( bar, "ofa-double", 1.0 );
		g_signal_emit_by_name( bar, "ofa-text", "0/0" );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->current_page_widget ), "p6-summary" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), FALSE );

	gtk_label_set_text( GTK_LABEL( label ),
			_( "The previous exercice has been successfully closed.\n"
				"The next exercice has been automatically defined and opened." ));

	my_assistant_set_page_type( MY_ASSISTANT( self ), priv->current_page_widget, GTK_ASSISTANT_PAGE_SUMMARY );
	my_assistant_set_page_complete( MY_ASSISTANT( self ), priv->current_page_widget, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}
