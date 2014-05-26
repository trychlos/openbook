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

#include "ui/ofa-main-page.h"
#include "ui/ofa-account-notebook.h"
#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-chart.h"
#include "ui/ofo-account.h"

/* private instance data
 */
struct _ofaAccountsChartPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* are set when run for the first time (page is new)
	 */

	/* UI
	 */
	ofaAccountNotebook *chart_child;
	GtkButton          *consult_btn;
};

static ofaMainPageClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaAccountsChartClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static GtkWidget *v_setup_view( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       v_init_view( ofaMainPage *page );
static void       on_row_activated( const gchar *number, ofaMainPage *page );
static void       on_account_selected( const gchar *number, ofaAccountsChart *self );
static void       v_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       v_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static gboolean   delete_confirmed( ofaAccountsChart *self, ofoAccount *account );
static void       on_view_entries( GtkButton *button, ofaAccountsChart *self );

GType
ofa_accounts_chart_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_accounts_chart_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaAccountsChartClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaAccountsChart ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaAccountsChart", &info, 0 );

	return( type );
}

static void
class_init( ofaAccountsChartClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = ( ofaMainPageClass * ) g_type_class_peek_parent( klass );
	g_return_if_fail( st_parent_class && OFA_IS_MAIN_PAGE_CLASS( st_parent_class ));

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	OFA_MAIN_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_MAIN_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_MAIN_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_MAIN_PAGE_CLASS( klass )->on_new_clicked = v_on_new_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_update_clicked = v_on_update_clicked;
	OFA_MAIN_PAGE_CLASS( klass )->on_delete_clicked = v_on_delete_clicked;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_init";
	ofaAccountsChart *self;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_ACCOUNTS_CHART( instance );

	self->private = g_new0( ofaAccountsChartPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_dispose";
	ofaAccountsChartPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	priv = ( OFA_ACCOUNTS_CHART( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( instance );
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_finalize";
	ofaAccountsChart *self;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_ACCOUNTS_CHART( instance );

	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( instance );
}

static GtkWidget *
v_setup_view( ofaMainPage *page )
{
	GtkNotebook *chart_book;
	ofaAccountNotebookParms parms;

	chart_book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( chart_book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( chart_book ), 4 );

	parms.book = chart_book;
	parms.dossier = ofa_main_page_get_dossier( page );
	parms.pfnSelect = ( ofaAccountNotebookCb ) on_account_selected;
	parms.user_data_select = page;
	parms.pfnDoubleClic = ( ofaAccountNotebookCb ) on_row_activated;
	parms.user_data_double_clic = page;

	OFA_ACCOUNTS_CHART( page )->private->chart_child = ofa_account_notebook_init_dialog( &parms );

	return( GTK_WIDGET( chart_book ));
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX( st_parent_class->setup_buttons( page ));

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 25 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "View _entries..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_view_entries ), page );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	OFA_ACCOUNTS_CHART( page )->private->consult_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
v_init_view( ofaMainPage *page )
{
	ofa_account_notebook_init_view( OFA_ACCOUNTS_CHART( page )->private->chart_child, NULL );
}

/*
 * ofaAccountNotebook callback:
 */
static void
on_row_activated( const gchar *number, ofaMainPage *page )
{
	v_on_update_clicked( NULL, page );
}

/*
 * ofaAccountNotebook callback:
 * first selection occurs during initialization of the chart notebook
 * thus at a moment where the buttons where not yet created
 */
static void
on_account_selected( const gchar *number, ofaAccountsChart *self )
{
	ofoAccount *account;

	account = NULL;

	if( self->private->chart_child ){
		account = ofa_account_notebook_get_selected( self->private->chart_child );
	}

	if( account ){
		gtk_widget_set_sensitive(
				ofa_main_page_get_update_btn( OFA_MAIN_PAGE( self )),
				account && OFO_IS_ACCOUNT( account ));
		gtk_widget_set_sensitive(
				ofa_main_page_get_delete_btn( OFA_MAIN_PAGE( self )),
				account && OFO_IS_ACCOUNT( account ) && ofo_account_is_deletable( account ));
		gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->consult_btn ),
				account && OFO_IS_ACCOUNT( account ));
	}
}

static void
v_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_accounts_chart_v_on_new_clicked";
	ofoAccount *account;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( page ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	account = ofo_account_new();

	if( ofa_account_properties_run(
			ofa_main_page_get_main_window( page ), account )){

		/* insert the account in its right place */
		ofa_account_notebook_insert(
				OFA_ACCOUNTS_CHART( page )->private->chart_child, account );

	} else {
		g_object_unref( account );
	}
}

static void
v_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_accounts_chart_v_on_update_clicked";
	ofaAccountsChart *self;
	ofoAccount *account;
	gchar *prev_number;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( page ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	self = OFA_ACCOUNTS_CHART( page );
	account = ofa_account_notebook_get_selected( self->private->chart_child );
	if( account ){
		prev_number = g_strdup( ofo_account_get_number( account ));

		if( ofa_account_properties_run(
				ofa_main_page_get_main_window( page ), account )){

			ofa_account_notebook_remove( self->private->chart_child, prev_number );
			ofa_account_notebook_insert( self->private->chart_child, account );
		}

		g_free( prev_number );
		g_object_unref( account );
	}

	ofa_account_notebook_grab_focus( self->private->chart_child );
}

/*
 * un compte peut être supprimé si ses soldes sont nuls, et après
 * confirmation de l'utilisateur
 */
static void
v_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_delete_account";
	ofaAccountsChart *self;
	ofoAccount *account;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( page ));

	g_debug( "%s: button=%p, page=%p", thisfn, ( void * ) button, ( void * ) page );

	self = OFA_ACCOUNTS_CHART( page );
	account = ofa_account_notebook_get_selected( self->private->chart_child );
	if( account ){
		g_object_unref( account );
		g_return_if_fail( ofo_account_is_deletable( account ));

		dossier = ofa_main_page_get_dossier( page );

		if( delete_confirmed( self, account ) &&
				ofo_account_delete( account, dossier )){

			/* remove the row from the model
			 * this will cause an automatic new selection */
			ofa_account_notebook_remove(
					self->private->chart_child, ofo_account_get_number( account ));
		}
	}

	ofa_account_notebook_grab_focus( self->private->chart_child );
}

static gboolean
delete_confirmed( ofaAccountsChart *self, ofoAccount *account )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' account ?" ),
			ofo_account_get_number( account ),
			ofo_account_get_label( account ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_view_entries";

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_warning( "%s: TO BE WRITTEN", thisfn );
}
